/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>
#include <limits>
#include "axiom/ir/ir.h"
#include "axiom/ir/ir_optimizer.h"
#include "axiom/ir/ir_vm.h"

using namespace axiom::ir;

TEST(IrOptimizerTest, EliminateNops) {
    IrFunction func;
    auto r0 = func.allocate_reg(IrType::I32);
    auto r1 = func.allocate_reg(IrType::I32);

    // [Inst0: Alloca, Inst1: Nop, Inst2: Assign, Inst3: Nop, Inst4: Return]
    func.instructions.push_back({ .op = IrOpcode::Alloca, .dest = IrOperand::MakeVar(r0, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::Nop });
    func.instructions.push_back({ .op = IrOpcode::Assign, .dest = IrOperand::MakeVar(r1, IrType::I32), .src1 = IrOperand::MakeConst(42, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::Nop });
    func.instructions.push_back({ .op = IrOpcode::Return, .src1 = IrOperand::MakeVar(r1, IrType::I32) });

    ASSERT_EQ(func.instructions.size(), 5u);

    IrOptimizer::eliminate_nops(func);

    ASSERT_EQ(func.instructions.size(), 3u);
    EXPECT_EQ(func.instructions[0].op, IrOpcode::Alloca);
    EXPECT_EQ(func.instructions[1].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[2].op, IrOpcode::Return);
}

TEST(IrOptimizerTest, ConstantFoldingAdd) {
    IrFunction func;
    auto r0 = func.allocate_reg(IrType::I32);

    // t0 = 10 + 20
    func.instructions.push_back({
        .op = IrOpcode::Add,
        .dest = IrOperand::MakeTemp(r0, IrType::I32),
        .src1 = IrOperand::MakeConst(10, IrType::I32),
        .src2 = IrOperand::MakeConst(20, IrType::I32)
    });
    func.instructions.push_back({
        .op = IrOpcode::Return,
        .src1 = IrOperand::MakeTemp(r0, IrType::I32)
    });

    bool changed = IrOptimizer::run_constant_folding(func);
    EXPECT_TRUE(changed);
    EXPECT_EQ(func.instructions[0].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[0].src1.kind, IrOperandKind::Constant);
    EXPECT_EQ(func.instructions[0].src1.constant_int, 30);

    IrModule mod;
    mod.functions.push_back(func);
    IrVm vm;
    auto res = vm.execute(func, {}, &mod);
    EXPECT_EQ(res.value, 30);
}

TEST(IrOptimizerTest, ConstantFoldingSubMulDiv) {
    IrFunction func;
    auto r0 = func.allocate_reg(IrType::I32);
    auto r1 = func.allocate_reg(IrType::I32);
    auto r2 = func.allocate_reg(IrType::I32);

    // r0 = 50 - 18 => 32
    func.instructions.push_back({
        .op = IrOpcode::Sub,
        .dest = IrOperand::MakeTemp(r0, IrType::I32),
        .src1 = IrOperand::MakeConst(50, IrType::I32),
        .src2 = IrOperand::MakeConst(18, IrType::I32)
    });
    // r1 = 6 * 7 => 42
    func.instructions.push_back({
        .op = IrOpcode::Mul,
        .dest = IrOperand::MakeTemp(r1, IrType::I32),
        .src1 = IrOperand::MakeConst(6, IrType::I32),
        .src2 = IrOperand::MakeConst(7, IrType::I32)
    });
    // r2 = 84 / 2 => 42
    func.instructions.push_back({
        .op = IrOpcode::Div,
        .dest = IrOperand::MakeTemp(r2, IrType::I32),
        .src1 = IrOperand::MakeConst(84, IrType::I32),
        .src2 = IrOperand::MakeConst(2, IrType::I32)
    });
    func.instructions.push_back({
        .op = IrOpcode::Return,
        .src1 = IrOperand::MakeTemp(r2, IrType::I32)
    });

    bool changed = IrOptimizer::run_constant_folding(func);
    EXPECT_TRUE(changed);

    EXPECT_EQ(func.instructions[0].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[0].src1.constant_int, 32);

    EXPECT_EQ(func.instructions[1].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[1].src1.constant_int, 42);

    EXPECT_EQ(func.instructions[2].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[2].src1.constant_int, 42);
}

TEST(IrOptimizerTest, ConstantFoldingDivTrapPreserved) {
    IrFunction func;
    auto r0 = func.allocate_reg(IrType::I32);
    auto r1 = func.allocate_reg(IrType::I32);

    // ゼロ除算: 10 / 0 は畳み込まない
    func.instructions.push_back({
        .op = IrOpcode::Div,
        .dest = IrOperand::MakeTemp(r0, IrType::I32),
        .src1 = IrOperand::MakeConst(10, IrType::I32),
        .src2 = IrOperand::MakeConst(0, IrType::I32)
    });
    // INT32_MIN / -1 は畳み込まない
    func.instructions.push_back({
        .op = IrOpcode::Div,
        .dest = IrOperand::MakeTemp(r1, IrType::I32),
        .src1 = IrOperand::MakeConst(std::numeric_limits<int32_t>::min(), IrType::I32),
        .src2 = IrOperand::MakeConst(-1, IrType::I32)
    });
    func.instructions.push_back({
        .op = IrOpcode::Return,
        .src1 = IrOperand::MakeTemp(r0, IrType::I32)
    });

    bool changed = IrOptimizer::run_constant_folding(func);
    EXPECT_FALSE(changed);
    EXPECT_EQ(func.instructions[0].op, IrOpcode::Div);
    EXPECT_EQ(func.instructions[1].op, IrOpcode::Div);

    // VM で実行するとゼロ除算トラップが発生することを確認
    IrVm vm;
    auto res = vm.execute(func);
    EXPECT_EQ(res.trap, IrVm::TrapCode::IntegerDivideByZero);
}

TEST(IrOptimizerTest, ConstantFoldingComparisons) {
    IrFunction func;
    auto r0 = func.allocate_reg(IrType::Bool);
    auto r1 = func.allocate_reg(IrType::Bool);
    auto r2 = func.allocate_reg(IrType::Bool);
    auto r3 = func.allocate_reg(IrType::Bool);
    auto r4 = func.allocate_reg(IrType::Bool);
    auto r5 = func.allocate_reg(IrType::Bool);

    func.instructions.push_back({ .op = IrOpcode::CmpEq, .dest = IrOperand::MakeTemp(r0, IrType::Bool), .src1 = IrOperand::MakeConst(5, IrType::I32), .src2 = IrOperand::MakeConst(5, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpNe, .dest = IrOperand::MakeTemp(r1, IrType::Bool), .src1 = IrOperand::MakeConst(5, IrType::I32), .src2 = IrOperand::MakeConst(5, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpLt, .dest = IrOperand::MakeTemp(r2, IrType::Bool), .src1 = IrOperand::MakeConst(3, IrType::I32), .src2 = IrOperand::MakeConst(7, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpLe, .dest = IrOperand::MakeTemp(r3, IrType::Bool), .src1 = IrOperand::MakeConst(7, IrType::I32), .src2 = IrOperand::MakeConst(7, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpGt, .dest = IrOperand::MakeTemp(r4, IrType::Bool), .src1 = IrOperand::MakeConst(2, IrType::I32), .src2 = IrOperand::MakeConst(9, IrType::I32) });
    func.instructions.push_back({ .op = IrOpcode::CmpGe, .dest = IrOperand::MakeTemp(r5, IrType::Bool), .src1 = IrOperand::MakeConst(9, IrType::I32), .src2 = IrOperand::MakeConst(2, IrType::I32) });

    bool changed = IrOptimizer::run_constant_folding(func);
    EXPECT_TRUE(changed);

    EXPECT_EQ(func.instructions[0].src1.constant_int, 1); // 5 == 5 -> 1
    EXPECT_EQ(func.instructions[1].src1.constant_int, 0); // 5 != 5 -> 0
    EXPECT_EQ(func.instructions[2].src1.constant_int, 1); // 3 < 7  -> 1
    EXPECT_EQ(func.instructions[3].src1.constant_int, 1); // 7 <= 7 -> 1
    EXPECT_EQ(func.instructions[4].src1.constant_int, 0); // 2 > 9  -> 0
    EXPECT_EQ(func.instructions[5].src1.constant_int, 1); // 9 >= 2 -> 1
}

TEST(IrOptimizerTest, ConstantFoldingBranchIfFalse) {
    IrFunction func;
    auto l0 = func.allocate_label();
    auto l1 = func.allocate_label();

    // 1. 条件が false (0): 必ずジャンプするので Jump l0 に畳み込まれる
    func.instructions.push_back({
        .op = IrOpcode::BranchIfFalse,
        .label = l0,
        .src1 = IrOperand::MakeConst(0, IrType::Bool)
    });
    // 2. 条件が true (1): 絶対に飛ばないので Nop に畳み込まれる
    func.instructions.push_back({
        .op = IrOpcode::BranchIfFalse,
        .label = l1,
        .src1 = IrOperand::MakeConst(1, IrType::Bool)
    });

    bool changed = IrOptimizer::run_constant_folding(func);
    EXPECT_TRUE(changed);

    EXPECT_EQ(func.instructions[0].op, IrOpcode::Jump);
    EXPECT_EQ(func.instructions[0].label, l0);
    EXPECT_EQ(func.instructions[0].src1.kind, IrOperandKind::None);

    EXPECT_EQ(func.instructions[1].op, IrOpcode::Nop);
    EXPECT_EQ(func.instructions[1].label, kInvalidLabelId);
    EXPECT_EQ(func.instructions[1].src1.kind, IrOperandKind::None);
}

TEST(IrOptimizerTest, AlgebraicSimplification) {
    IrFunction func;
    auto x  = func.allocate_reg(IrType::I32);
    auto r0 = func.allocate_reg(IrType::I32);
    auto r1 = func.allocate_reg(IrType::I32);
    auto r2 = func.allocate_reg(IrType::I32);
    auto r3 = func.allocate_reg(IrType::I32);
    auto r4 = func.allocate_reg(IrType::I32);
    auto r5 = func.allocate_reg(IrType::I32);
    auto r6 = func.allocate_reg(IrType::I32);
    auto r7 = func.allocate_reg(IrType::I32);

    // x = 42
    func.instructions.push_back({ .op = IrOpcode::Assign, .dest = IrOperand::MakeVar(x, IrType::I32), .src1 = IrOperand::MakeConst(42, IrType::I32) });

    // r0 = x + 0 => assign x
    func.instructions.push_back({ .op = IrOpcode::Add, .dest = IrOperand::MakeTemp(r0, IrType::I32), .src1 = IrOperand::MakeVar(x, IrType::I32), .src2 = IrOperand::MakeConst(0, IrType::I32) });
    // r1 = 0 + x => assign x
    func.instructions.push_back({ .op = IrOpcode::Add, .dest = IrOperand::MakeTemp(r1, IrType::I32), .src1 = IrOperand::MakeConst(0, IrType::I32), .src2 = IrOperand::MakeVar(x, IrType::I32) });
    // r2 = x - 0 => assign x
    func.instructions.push_back({ .op = IrOpcode::Sub, .dest = IrOperand::MakeTemp(r2, IrType::I32), .src1 = IrOperand::MakeVar(x, IrType::I32), .src2 = IrOperand::MakeConst(0, IrType::I32) });
    // r3 = x * 1 => assign x
    func.instructions.push_back({ .op = IrOpcode::Mul, .dest = IrOperand::MakeTemp(r3, IrType::I32), .src1 = IrOperand::MakeVar(x, IrType::I32), .src2 = IrOperand::MakeConst(1, IrType::I32) });
    // r4 = 1 * x => assign x
    func.instructions.push_back({ .op = IrOpcode::Mul, .dest = IrOperand::MakeTemp(r4, IrType::I32), .src1 = IrOperand::MakeConst(1, IrType::I32), .src2 = IrOperand::MakeVar(x, IrType::I32) });
    // r5 = x * 0 => assign 0
    func.instructions.push_back({ .op = IrOpcode::Mul, .dest = IrOperand::MakeTemp(r5, IrType::I32), .src1 = IrOperand::MakeVar(x, IrType::I32), .src2 = IrOperand::MakeConst(0, IrType::I32) });
    // r6 = 0 * x => assign 0
    func.instructions.push_back({ .op = IrOpcode::Mul, .dest = IrOperand::MakeTemp(r6, IrType::I32), .src1 = IrOperand::MakeConst(0, IrType::I32), .src2 = IrOperand::MakeVar(x, IrType::I32) });
    // r7 = x / 1 => assign x
    func.instructions.push_back({ .op = IrOpcode::Div, .dest = IrOperand::MakeTemp(r7, IrType::I32), .src1 = IrOperand::MakeVar(x, IrType::I32), .src2 = IrOperand::MakeConst(1, IrType::I32) });

    func.instructions.push_back({ .op = IrOpcode::Return, .src1 = IrOperand::MakeTemp(r7, IrType::I32) });

    bool changed = IrOptimizer::run_constant_folding(func);
    EXPECT_TRUE(changed);

    // 全ての演算が Assign に簡約されていることを検証
    for (size_t i = 1; i <= 8; ++i) {
        EXPECT_EQ(func.instructions[i].op, IrOpcode::Assign);
        EXPECT_EQ(func.instructions[i].src2.kind, IrOperandKind::None);
    }

    // r0 ~ r4, r7 は src1 が x
    EXPECT_EQ(func.instructions[1].src1.reg, x);
    EXPECT_EQ(func.instructions[2].src1.reg, x);
    EXPECT_EQ(func.instructions[3].src1.reg, x);
    EXPECT_EQ(func.instructions[4].src1.reg, x);
    EXPECT_EQ(func.instructions[5].src1.reg, x);
    EXPECT_EQ(func.instructions[8].src1.reg, x);

    // r5, r6 は src1 が Const(0)
    EXPECT_EQ(func.instructions[6].src1.kind, IrOperandKind::Constant);
    EXPECT_EQ(func.instructions[6].src1.constant_int, 0);
    EXPECT_EQ(func.instructions[7].src1.kind, IrOperandKind::Constant);
    EXPECT_EQ(func.instructions[7].src1.constant_int, 0);

    // VM 実行で 42 が返ることを検証
    IrModule mod;
    mod.functions.push_back(func);
    IrVm vm;
    auto res = vm.execute(func, {}, &mod);
    EXPECT_EQ(res.value, 42);
}

TEST(IrOptimizerTest, CopyAndConstantPropagationBasic) {
    IrFunction func;
    auto a = func.allocate_reg(IrType::I32);
    auto b = func.allocate_reg(IrType::I32);
    auto c = func.allocate_reg(IrType::I32);

    // a = 10
    func.instructions.push_back({ .op = IrOpcode::Assign, .dest = IrOperand::MakeVar(a, IrType::I32), .src1 = IrOperand::MakeConst(10, IrType::I32) });
    // b = a  (定数伝播で b = 10 になる)
    func.instructions.push_back({ .op = IrOpcode::Assign, .dest = IrOperand::MakeVar(b, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32) });
    // c = b + 20 (伝播で 10 + 20 になり、定数畳み込みで c = 30 になる)
    func.instructions.push_back({ .op = IrOpcode::Add, .dest = IrOperand::MakeTemp(c, IrType::I32), .src1 = IrOperand::MakeVar(b, IrType::I32), .src2 = IrOperand::MakeConst(20, IrType::I32) });
    // return c (伝播で return 30 になる)
    func.instructions.push_back({ .op = IrOpcode::Return, .src1 = IrOperand::MakeTemp(c, IrType::I32) });

    bool prop_changed = IrOptimizer::run_copy_propagation(func);
    if (prop_changed) {
        // 1回目の伝播で b = 10, c = 10 + 20 になっているはず
        EXPECT_EQ(func.instructions[1].src1.kind, IrOperandKind::Constant);
        EXPECT_EQ(func.instructions[1].src1.constant_int, 10);
        EXPECT_EQ(func.instructions[2].src1.kind, IrOperandKind::Constant);
        EXPECT_EQ(func.instructions[2].src1.constant_int, 10);

        // パイプライン全体（optimize_function）を回すと c = 30, return 30 まで畳み込まれる
        IrOptimizer::optimize_function(func);
        EXPECT_EQ(func.instructions[2].op, IrOpcode::Assign);
        EXPECT_EQ(func.instructions[2].src1.constant_int, 30);
        EXPECT_EQ(func.instructions[3].src1.kind, IrOperandKind::Constant);
        EXPECT_EQ(func.instructions[3].src1.constant_int, 30);

        IrVm vm;
        auto res = vm.execute(func);
        EXPECT_EQ(res.value, 30);
    }
}

TEST(IrOptimizerTest, CopyPropagationInvalidationOnOverwriteAndLabel) {
    IrFunction func;
    auto a  = func.allocate_reg(IrType::I32);
    auto r0 = func.allocate_reg(IrType::I32);
    auto r1 = func.allocate_reg(IrType::I32);
    auto r2 = func.allocate_reg(IrType::I32);
    auto l0 = func.allocate_label();

    // [0] a = 10
    func.instructions.push_back({ .op = IrOpcode::Assign, .dest = IrOperand::MakeVar(a, IrType::I32), .src1 = IrOperand::MakeConst(10, IrType::I32) });
    // [1] r0 = a + 1  -> 10 + 1 に伝播される
    func.instructions.push_back({ .op = IrOpcode::Add, .dest = IrOperand::MakeTemp(r0, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeConst(1, IrType::I32) });
    // [2] a = 50      -> a が上書きされたので古い値(10)は破棄され、50になる
    func.instructions.push_back({ .op = IrOpcode::Assign, .dest = IrOperand::MakeVar(a, IrType::I32), .src1 = IrOperand::MakeConst(50, IrType::I32) });
    // [3] r1 = a + 1  -> 50 + 1 に伝播される
    func.instructions.push_back({ .op = IrOpcode::Add, .dest = IrOperand::MakeTemp(r1, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeConst(1, IrType::I32) });
    // [4] label L0    -> 合流点なので追跡テーブルが全リセットされる
    func.instructions.push_back({ .op = IrOpcode::Label, .label = l0 });
    // [5] r2 = a + 1  -> ラベル越えなので a は定数に置換されないまま残る
    func.instructions.push_back({ .op = IrOpcode::Add, .dest = IrOperand::MakeTemp(r2, IrType::I32), .src1 = IrOperand::MakeVar(a, IrType::I32), .src2 = IrOperand::MakeConst(1, IrType::I32) });

    bool changed = IrOptimizer::run_copy_propagation(func);
    if (changed) {
        EXPECT_EQ(func.instructions[1].src1.kind, IrOperandKind::Constant);
        EXPECT_EQ(func.instructions[1].src1.constant_int, 10);

        EXPECT_EQ(func.instructions[3].src1.kind, IrOperandKind::Constant);
        EXPECT_EQ(func.instructions[3].src1.constant_int, 50);

        // ラベル直後の命令では a は Variable のまま維持されていること
        EXPECT_EQ(func.instructions[5].src1.kind, IrOperandKind::Variable);
        EXPECT_EQ(func.instructions[5].src1.reg, a);
    }
}



