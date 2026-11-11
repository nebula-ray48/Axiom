/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

// cli/src/main.cpp
//
// 【役割】
//   Axiom コンパイラ CLI (axc) のエントリポイント。
//   現時点では parse_test_code() を呼ぶだけの最小構成。
//
// 【将来の展望】
//   - コマンドライン引数（.ax ファイルパス）を受け取ってコンパイルを実行
//   - エラーが出た場合は stderr に出力して非ゼロで終了
//   - オプション: --check（型検査のみ）、--emit-ir 等

#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#define IS_TTY(fd) _isatty(_fileno(fd))
#else
#include <unistd.h>
#define IS_TTY(fd) isatty(fileno(fd))
#endif

#include "tree_sitter/api.h"

#include "axiom/base/compiler.h"
#include "axiom/base/diagnostic.h"
#include "axiom/codegen/arm64_asm_emitter.h"
#include "axiom/ir/ir_builder.h"
#include "axiom/ir/ir_optimizer.h"
#include "axiom/ir/ir_printer.h"
#include "axiom/ir/ir_vm.h"
#include "axiom/jit/arm64_naive_jit.h"
#include "axiom/jit/jit_arena.h"
#include "axiom/sema/type_checker.h"
#include "axiom/sema/type_system.h"
#include "axiom/syntax/analyzer.h"

#if defined(AXIOM_HAS_PRIVATE_CORE)
#include "axiom/priv/private_core.h"
#endif

// parse_test_code() の前方宣言。
// 本体は core/src/compiler.cpp に実装されている。
// CLI は core ライブラリにリンクされているので、ここで宣言するだけで使える。
namespace axiom {

enum class Command {
    Check,    // check (構文解析・型検査のみ)
    DumpIr,   // dump-ir
    DumpDot,  // dump-dot
    Vm,       // vm
    Run,      // run (JIT)
    EmitAsm,  // emit-asm (.s アセンブリ出力)
    Build,    // build (clang アセンブル＆リンクで実行可能バイナリ生成)
    Help,     // --help または引数不正
};

struct CliOptions {
    Command command = Command::Help;
    std::string file_path;
    std::string output_path;
    bool optimize = false;
};

CliOptions parse_args(int argc, char* argv[]) {
    if (argc < 2) {
        return CliOptions{ .command = Command::Help };
    }

    CliOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "help" || arg == "--help" || arg == "-h") {
            return CliOptions{ .command = Command::Help };
        } else if (arg == "check") {
            opts.command = Command::Check;
        } else if (arg == "dump-ir") {
            opts.command = Command::DumpIr;
        } else if (arg == "dump-dot") {
            opts.command = Command::DumpDot;
        } else if (arg == "vm") {
            opts.command = Command::Vm;
        } else if (arg == "run") {
            opts.command = Command::Run;
        } else if (arg == "emit-asm") {
            opts.command = Command::EmitAsm;
        } else if (arg == "build") {
            opts.command = Command::Build;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                opts.output_path = argv[++i];
            } else {
                std::cerr << "error: option '" << arg << "' requires an output path\n";
                return CliOptions{ .command = Command::Help };
            }
        } else if (arg == "-O" || arg == "-O1" || arg == "--opt" || arg == "--optimize") {
            opts.optimize = true;
        } else if (!arg.starts_with('-')) {
            opts.file_path = arg;
        } else {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return CliOptions{ .command = Command::Help };
        }
    }

    if (opts.command == Command::Help) {
        return opts;
    }

    if (opts.file_path.empty()) {
        std::cerr << "error: no input .ax file specified\n";
        return CliOptions{ .command = Command::Help };
    }

    return opts;
}

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " <command> [options] <file.ax>\n\n"
              << "Commands:\n"
              << "  check     Run semantic analysis and type checking without code generation\n"
              << "  dump-ir   Dump intermediate representation (IR) in text format\n"
              << "  dump-dot  Dump control flow graph (CFG) in Graphviz DOT format\n"
              << "  vm        Execute using reference IR VM\n"
              << "  run       Execute natively using ARM64 JIT\n"
              << "  emit-asm  Emit ARM64 assembly (.s)\n"
              << "  build     Build standalone native executable via system clang\n"
              << "  help      Show this help message\n\n"
              << "Options:\n"
              << "  -o <path> Specify output file path (.s or binary)\n"
              << "  -O, --opt Enable IR optimizations (folding, propagation, DCE)\n";
}

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "error: cannot open file '" << path << "'\n";
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool write_file(const std::string& path, std::string_view content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "error: cannot open output file '" << path << "'\n";
        return false;
    }
    file << content;
    return true;
}

bool assemble_with_clang(std::string_view asm_code, std::string_view output_bin) {
    std::string cmd = std::format("clang -x assembler - -o {}", output_bin);
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "wb");
#else
    FILE* pipe = popen(cmd.c_str(), "w");
#endif
    if (!pipe) {
        std::cerr << "error: failed to launch clang\n";
        return false;
    }
    fwrite(asm_code.data(), 1, asm_code.size(), pipe);
#if defined(_WIN32)
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
#endif
    if (status != 0) {
        std::cerr << "error: clang assembler/linker failed with status " << status << "\n";
        return false;
    }
    return true;
}

void parse_test_code();
}  // namespace axiom

int main(int argc, char* argv[]) {
    axiom::CliOptions opts = axiom::parse_args(argc, argv);
    if (opts.command == axiom::Command::Help) {
        axiom::print_usage(argv[0]);
        return 1;
    }

    // 1. ファイルを読む
    auto source_opt = axiom::read_file(opts.file_path);
    if (!source_opt.has_value()) {
        return 1;
    }
    const std::string& source = *source_opt;

    // 2. Tree-sitter でパースする
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_axiom());
    TSTree* tree = ts_parser_parse_string(parser, nullptr, source.data(), static_cast<uint32_t>(source.size()));
    TSNode root = ts_tree_root_node(tree);

    // 3. IR を構築する
    axiom::StringInterner interner;
    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root);

    axiom::TypeRegistry type_registry(interner);
    axiom::TypeChecker checker(source, analyzer.get_functions(), type_registry, interner);

    if (!checker.check_all()) {
        bool use_color = IS_TTY(stderr);
        for (const auto& err : checker.get_errors()) {
            axiom::render_diagnostic(std::cerr, err, source, opts.file_path, use_color);
        }
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        return 1;
    }

    if (opts.command == axiom::Command::Check) {
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        return 0;
    }

    axiom::ir::IrBuilder builder(source, interner);
    axiom::ir::IrModule module = builder.build(root);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    if (opts.optimize) {
        axiom::ir::IrOptimizer::optimize_module(module);
#if defined(AXIOM_HAS_PRIVATE_CORE)
        axiom::priv::run_private_ir_passes(module);
#endif
    }

    switch (opts.command) {
        case axiom::Command::DumpIr:
            std::cout << dump_ir(module, &interner, true);
            break;

        case axiom::Command::DumpDot:
            std::cout << dump_cfg_dot(module, &interner, true);
            break;

        case axiom::Command::Vm: {
            axiom::StringID main_id = interner.Intern("main");
            axiom::ir::IrVm vm;
            auto result = vm.execute(module, main_id);
            if (!result.ok()) {
                std::cerr << "error: VM execution trapped\n";
                return 1;
            }
            std::cout << result.value << std::endl;
            break;
        }

        case axiom::Command::Run: {
            // JIT コンパイルして main をネイティブ実行する
            auto compiled = axiom::jit::Arm64NaiveJit::compile_module(module);
            axiom::StringID main_id = interner.Intern("main");
            if (!compiled.func_inst_offsets.contains(main_id)) {
                std::cerr << "error: 'main' function not found\n";
                return 1;
            }
            axiom::jit::JitCodeArena arena;
            size_t base = arena.emit_function(compiled.code);
            size_t offset = base + compiled.func_inst_offsets.at(main_id) * sizeof(uint32_t);
            auto fn = arena.get_function<int32_t()>(offset);
            std::cout << fn() << std::endl;
            break;
        }
        
        case axiom::Command::EmitAsm: {
            axiom::codegen::Arm64AsmEmitter emitter(interner);
            std::string asm_code = emitter.emit_module(module);
            if (opts.output_path.empty()) {
                std::cout << asm_code;
            } else if (!axiom::write_file(opts.output_path, asm_code)) {
                return 1;
            }
            break;
        }

        case axiom::Command::Build: {
            axiom::codegen::Arm64AsmEmitter emitter(interner);
            std::string asm_code = emitter.emit_module(module);
            std::string out_bin = opts.output_path.empty()
            ? std::filesystem::path(opts.file_path).stem().string()
            : opts.output_path;
            if (!axiom::assemble_with_clang(asm_code, out_bin)) {
                return 1;
            }
            break;
        }

        default:
            break;
    }

    return 0;
}