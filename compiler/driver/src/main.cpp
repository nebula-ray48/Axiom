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

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "tree_sitter/api.h"

#include "axiom/base/compiler.h"
#include "axiom/ir/ir_builder.h"
#include "axiom/ir/ir_printer.h"
#include "axiom/ir/ir_vm.h"
#include "axiom/jit/arm64_naive_jit.h"
#include "axiom/jit/jit_arena.h"

// parse_test_code() の前方宣言。
// 本体は core/src/compiler.cpp に実装されている。
// CLI は core ライブラリにリンクされているので、ここで宣言するだけで使える。
namespace axiom {

enum class Command {
    DumpIr,   // dump-ir
    DumpDot,  // dump-dot
    Vm,       // vm
    Run,      // run (JIT)
    Help,     // --help または引数不正
};

struct CliOptions {
    Command command = Command::Help;
    std::string file_path;
};

CliOptions parse_args(int argc, char* argv[]) {
    if (argc < 2) {
        return CliOptions{ .command = Command::Help };
    }

    std::string_view cmd = argv[1];
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        return CliOptions{ .command = Command::Help };
    }

    if (argc < 3) {
        std::cerr << "error: no input .ax file specified\n";
        return CliOptions{ .command = Command::Help };
    }

    CliOptions opts;
    opts.file_path = argv[2];

    if (cmd == "dump-ir") {
        opts.command = Command::DumpIr;
    } else if (cmd == "dump-dot") {
        opts.command = Command::DumpDot;
    } else if (cmd == "vm") {
        opts.command = Command::Vm;
    } else if (cmd == "run") {
        opts.command = Command::Run;
    } else {
        std::cerr << "error: unknown command '" << cmd << "'\n";
        opts.command = Command::Help;
    }

    return opts;
}

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " <command> <file.ax>\n\n"
              << "Commands:\n"
              << "  dump-ir   Dump intermediate representation (IR) in text format\n"
              << "  dump-dot  Dump control flow graph (CFG) in Graphviz DOT format\n"
              << "  vm        Execute using reference IR VM\n"
              << "  run       Execute natively using ARM64 JIT\n"
              << "  help      Show this help message\n";
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
    axiom::ir::IrBuilder builder(source, interner);
    axiom::ir::IrModule module = builder.build(root);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    switch (opts.command) {
        case axiom::Command::DumpIr:
            std::cout << dump_ir(module, &interner, true);
            break;

        case axiom::Command::DumpDot:
            std::cout << dump_cfg_dot(module, &interner, true);
            break;

        case axiom::Command::Vm: {
            // "main" 関数を探して実行する
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

        default:
            break;
    }

    return 0;
}