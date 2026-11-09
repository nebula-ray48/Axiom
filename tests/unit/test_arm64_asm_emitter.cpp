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

TEST(Arm64AsmEmitterTest, ArithmeticAndAssignInstructions) {
    StringInterner interner;
    IrModule mod;
    IrFunction func;
    func.name_id     = interner.Intern("calc");
    func.return_type = IrType::I32;

    auto a  = func.allocate_reg(IrType::I32);
    auto b  = func.allocate_reg(IrType::I32);
    auto t0 = func.allocate_reg(IrType::I32);
    auto t1 = func.allocate_reg(IrType::I32);
    auto t2 = func.allocate_reg(IrType::I32);
    auto t3 = func.allocate_reg(IrType::I32);

    // Alloca a; a = 20; b = 5
    func.instructions.push_back({ .op = IrOpcode::Alloca, .dest = IrOperand::MakeVar(a, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::Assign, .dest = IrOperand::MakeVar(a, IrType::I32), .src1 = IrOperand::MakeConst(20, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::Assign, .dest = IrOperand::MakeVar(b, IrType::I32), .src1 = IrOperand::MakeConst(5, IrType::I32) });

    // t0 = a + b
    func.instructions.push_back({ .op = IrOpcode::Add, .dest = IrOperand::MakeTemp(t0, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    // t1 = t0 - b
    func.instructions.push_back({ .op = IrOpcode::Sub, .dest = IrOperand::MakeTemp(t1, IrType::I32), .src1 = IrOperand::MakeTemp(t0, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    // t2 = t1 * b
    func.instructions.push_back({ .op = IrOpcode::Mul, .dest = IrOperand::MakeTemp(t2, IrType::I32), .src1 = IrOperand::MakeTemp(t1, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    // t3 = t2 / b
    func.instructions.push_back({ .op = IrOpcode::Div, .dest = IrOperand::MakeTemp(t3, IrType::I32), .src1 = IrOperand::MakeTemp(t2, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    // return t3
    func.instructions.push_back({ .op = IrOpcode::Return, .src1 = IrOperand::MakeTemp(t3, IrType::I32) });

    mod.functions.push_back(func);

    Arm64AsmEmitter emitter(interner);
    std::string asm_code = emitter.emit_module(mod);

    EXPECT_NE(asm_code.find("add w"), std::string::npos);
    EXPECT_NE(asm_code.find("sub w"), std::string::npos);
    EXPECT_NE(asm_code.find("mul w"), std::string::npos);
    EXPECT_NE(asm_code.find("sdiv w"), std::string::npos);
    EXPECT_TRUE(ValidateAsmWithClang(asm_code));
}

TEST(Arm64AsmEmitterTest, ComparisonInstructions) {
    StringInterner interner;
    IrModule mod;
    IrFunction func;
    func.name_id     = interner.Intern("compare_all");
    func.return_type = IrType::I32;

    auto a  = func.allocate_reg(IrType::I32);
    auto b  = func.allocate_reg(IrType::I32);
    auto c0 = func.allocate_reg(IrType::I32);
    auto c1 = func.allocate_reg(IrType::I32);
    auto c2 = func.allocate_reg(IrType::I32);
    auto c3 = func.allocate_reg(IrType::I32);
    auto c4 = func.allocate_reg(IrType::I32);
    auto c5 = func.allocate_reg(IrType::I32);

    func.parameters.push_back({ .name_id = interner.Intern("a"), .type = IrType::I32, .reg = a });
    func.parameters.push_back({ .name_id = interner.Intern("b"), .type = IrType::I32, .reg = b });

    func.instructions.push_back({ .op = IrOpcode::CmpEq, .dest = IrOperand::MakeTemp(c0, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpNe, .dest = IrOperand::MakeTemp(c1, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpLt, .dest = IrOperand::MakeTemp(c2, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpLe, .dest = IrOperand::MakeTemp(c3, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpGt, .dest = IrOperand::MakeTemp(c4, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpGe, .dest = IrOperand::MakeTemp(c5, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeVar(b, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::Return, .src1 = IrOperand::MakeTemp(c5, IrType::I32) });

    mod.functions.push_back(func);

    Arm64AsmEmitter emitter(interner);
    std::string asm_code = emitter.emit_module(mod);

    EXPECT_NE(asm_code.find("cmp w"), std::string::npos);
    EXPECT_NE(asm_code.find("eq"), std::string::npos);
    EXPECT_NE(asm_code.find("ne"), std::string::npos);
    EXPECT_NE(asm_code.find("lt"), std::string::npos);
    EXPECT_NE(asm_code.find("le"), std::string::npos);
    EXPECT_NE(asm_code.find("gt"), std::string::npos);
    EXPECT_NE(asm_code.find("ge"), std::string::npos);
    EXPECT_TRUE(ValidateAsmWithClang(asm_code));
}

