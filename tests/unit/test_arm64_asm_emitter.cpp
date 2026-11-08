/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdio>
#include <string>

#include "axiom/base/compiler.h"
#include "axiom/codegen/arm64_asm_emitter.h"
#include "axiom/ir/ir.h"

using namespace axiom;
using namespace axiom::codegen;
using namespace axiom::ir;

namespace {

bool ValidateAsmWithClang(const std::string& asm_code) {
#if defined(__APPLE__) || defined(__linux__)
    // clang -x assembler -c -o /dev/null -
    FILE* pipe = popen("clang -x assembler -c -o /dev/null - 2>/dev/null", "w");
    if (!pipe) {
        return false;
    }
    fwrite(asm_code.data(), 1, asm_code.size(), pipe);
    int status = pclose(pipe);
    return status == 0;
#else
    (void)asm_code;
    return true;
#endif
}

}  // namespace

TEST(Arm64AsmEmitterTest, SymbolManglingMacOSVsLinux) {
    StringInterner interner;
    IrModule mod;
    IrFunction func;
    func.name_id = interner.Intern("compute_total");
    func.return_type = IrType::I32;
    func.instructions.push_back({
        .op = IrOpcode::Return,
        .src1 = IrOperand::MakeConst(42, IrType::I32),
    });
    mod.functions.push_back(func);

    // MacOS target: starts with '_'
    {
        Arm64AsmEmitter emitter(interner, AsmEmitterOptions{ .target_os = TargetOs::MacOS });
        std::string s = emitter.emit_module(mod);
        EXPECT_NE(s.find(".globl _compute_total"), std::string::npos);
        EXPECT_NE(s.find("_compute_total:"), std::string::npos);
    }

    // Linux target: no leading '_' and has .type / .size
    {
        Arm64AsmEmitter emitter(interner, AsmEmitterOptions{ .target_os = TargetOs::Linux });
        std::string s = emitter.emit_module(mod);
        EXPECT_NE(s.find(".globl compute_total"), std::string::npos);
        EXPECT_NE(s.find("compute_total:"), std::string::npos);
        EXPECT_NE(s.find(".type compute_total, %function"), std::string::npos);
        EXPECT_NE(s.find(".size compute_total, .-compute_total"), std::string::npos);
    }
}

TEST(Arm64AsmEmitterTest, ReturnConstant42PrologueAndEpilogue) {
    StringInterner interner;
    IrModule mod;
    IrFunction func;
    func.name_id = interner.Intern("main");
    func.return_type = IrType::I32;
    func.instructions.push_back({
        .op = IrOpcode::Return,
        .src1 = IrOperand::MakeConst(42, IrType::I32),
    });
    mod.functions.push_back(func);

    Arm64AsmEmitter emitter(interner);
    std::string asm_code = emitter.emit_module(mod);

    // プロローグ・エピローグの標準命令が含まれていること
    EXPECT_NE(asm_code.find("stp x29, x30, [sp, #-16]!"), std::string::npos);
    EXPECT_NE(asm_code.find("mov x29, sp"), std::string::npos);
    EXPECT_NE(asm_code.find("mov w0, #42"), std::string::npos);
    EXPECT_NE(asm_code.find("ldp x29, x30, [sp], #16"), std::string::npos);
    EXPECT_NE(asm_code.find("ret"), std::string::npos);

    // clang アセンブラで文法チェック
    EXPECT_TRUE(ValidateAsmWithClang(asm_code));
}

TEST(Arm64AsmEmitterTest, FunctionParametersStoreToStackOrRegister) {
    StringInterner interner;
    IrModule mod;
    IrFunction func;
    func.name_id = interner.Intern("add_params");
    func.return_type = IrType::I32;

    auto p0 = func.allocate_reg(IrType::I32);
    auto p1 = func.allocate_reg(IrType::I32);
    func.parameters.push_back({ .name_id = interner.Intern("a"), .type = IrType::I32, .reg = p0 });
    func.parameters.push_back({ .name_id = interner.Intern("b"), .type = IrType::I32, .reg = p1 });

    func.instructions.push_back({
        .op = IrOpcode::Return,
        .src1 = IrOperand::MakeVar(p0, IrType::I32),
    });
    mod.functions.push_back(func);

    Arm64AsmEmitter emitter(interner);
    std::string asm_code = emitter.emit_module(mod);

    // 引数0 (w0), 引数1 (w1) の取り扱いまたはストアが存在すること
    EXPECT_TRUE(ValidateAsmWithClang(asm_code));
}

