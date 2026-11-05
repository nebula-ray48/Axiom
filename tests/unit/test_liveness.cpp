/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>

#include "axiom/base/compiler.h"
#include "axiom/ir/ir.h"
#include "axiom/jit/liveness.h"

using namespace axiom;
using namespace axiom::ir;
using namespace axiom::jit;

TEST(LivenessTest, ManualLinearInstructionTrace) {
    // 手計算トレースと同じ命令列を手動構築して検証
    // 仮引数: %a (0), %b (1)
    // 0: %2 = add %a, %b
    // 1: %3 = mul %2, 10
    // 2: ret %3
    IrFunction func;
    func.return_type = IrType::I32;

    RegId reg_a = func.allocate_reg(IrType::I32); // 0
    RegId reg_b = func.allocate_reg(IrType::I32); // 1
    RegId reg_2 = func.allocate_reg(IrType::I32); // 2
    RegId reg_3 = func.allocate_reg(IrType::I32); // 3

    func.parameters.push_back({ .name_id = kInvalidStringID, .type = IrType::I32, .reg = reg_a });
    func.parameters.push_back({ .name_id = kInvalidStringID, .type = IrType::I32, .reg = reg_b });

    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Add,
        .dest = IrOperand::MakeTemp(reg_2, IrType::I32),
        .src1 = IrOperand::MakeVar(reg_a, IrType::I32),
        .src2 = IrOperand::MakeVar(reg_b, IrType::I32),
    });

    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Mul,
        .dest = IrOperand::MakeTemp(reg_3, IrType::I32),
        .src1 = IrOperand::MakeTemp(reg_2, IrType::I32),
        .src2 = IrOperand::MakeConst(10, IrType::I32),
    });

    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Return,
        .src1 = IrOperand::MakeTemp(reg_3, IrType::I32),
    });

    LivenessResult result = compute_liveness(func);

    // 1. 各レジスタの生存区間 [start, end] を検証
    ASSERT_EQ(result.intervals_by_reg.size(), 4U);

    // %a (reg 0): 関数先頭で生まれ、index 0 で使用
    EXPECT_EQ(result.intervals_by_reg[0].start, 0U);
    EXPECT_EQ(result.intervals_by_reg[0].end, 0U);

    // %b (reg 1): 関数先頭で生まれ、index 0 で使用
    EXPECT_EQ(result.intervals_by_reg[1].start, 0U);
    EXPECT_EQ(result.intervals_by_reg[1].end, 0U);

    // %2 (reg 2): index 0 で定義され、index 1 で使用
    EXPECT_EQ(result.intervals_by_reg[2].start, 0U);
    EXPECT_EQ(result.intervals_by_reg[2].end, 1U);

    // %3 (reg 3): index 1 で定義され、index 2 で使用
    EXPECT_EQ(result.intervals_by_reg[3].start, 1U);
    EXPECT_EQ(result.intervals_by_reg[3].end, 2U);

    // 2. start 昇順でソートされていることを検証
    ASSERT_EQ(result.sorted_intervals.size(), 4U);
    for (size_t i = 1; i < result.sorted_intervals.size(); ++i) {
        EXPECT_LE(result.sorted_intervals[i - 1].start, result.sorted_intervals[i].start);
    }
}

TEST(LivenessTest, UnusedRegistersAreFilteredFromSortedList) {
    IrFunction func;
    func.return_type = IrType::I32;

    RegId used_reg   = func.allocate_reg(IrType::I32); // 0
    RegId unused_reg = func.allocate_reg(IrType::I32); // 1

    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Return,
        .src1 = IrOperand::MakeConst(42, IrType::I32),
    });

    LivenessResult result = compute_liveness(func);

    ASSERT_EQ(result.intervals_by_reg.size(), 2U);
    EXPECT_FALSE(result.intervals_by_reg[unused_reg].is_live());
    EXPECT_FALSE(result.intervals_by_reg[used_reg].is_live());

    // 未使用のレジスタは sorted_intervals に入らない
    EXPECT_TRUE(result.sorted_intervals.empty());
}

TEST(LivenessTest, FunctionCallArgumentsRecorded) {
    // 0: alloca %x
    // 1: %x = assign 32
    // 2: %tmp = call @foo(10, %x)
    // 3: ret %tmp
    IrFunction func;
    func.return_type = IrType::I32;

    RegId reg_x   = func.allocate_reg(IrType::I32); // 0
    RegId reg_tmp = func.allocate_reg(IrType::I32); // 1

    uint32_t arg_start = static_cast<uint32_t>(func.call_args.size());
    func.call_args.push_back(IrOperand::MakeConst(10, IrType::I32));
    func.call_args.push_back(IrOperand::MakeVar(reg_x, IrType::I32));

    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Alloca,
        .dest = IrOperand::MakeVar(reg_x, IrType::I32),
    });
    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Assign,
        .dest = IrOperand::MakeVar(reg_x, IrType::I32),
        .src1 = IrOperand::MakeConst(32, IrType::I32),
    });
    func.instructions.push_back(IrInstruction{
        .op             = IrOpcode::Call,
        .dest           = IrOperand::MakeTemp(reg_tmp, IrType::I32),
        .call_func_name = kInvalidStringID,
        .arg_start      = arg_start,
        .arg_count      = 2,
    });
    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Return,
        .src1 = IrOperand::MakeTemp(reg_tmp, IrType::I32),
    });

    LivenessResult result = compute_liveness(func);

    ASSERT_EQ(result.intervals_by_reg.size(), 2U);

    // %x (reg 0): inst 0 (Alloca) で定義され、inst 2 (Call) で実引数として使われる
    EXPECT_EQ(result.intervals_by_reg[reg_x].start, 0U);
    EXPECT_EQ(result.intervals_by_reg[reg_x].end, 2U);

    // %tmp (reg 1): inst 2 (Call) で戻り値を受け取り、inst 3 (Return) で使われる
    EXPECT_EQ(result.intervals_by_reg[reg_tmp].start, 2U);
    EXPECT_EQ(result.intervals_by_reg[reg_tmp].end, 3U);
}

TEST(LivenessTest, WhileLoopExtendsLiveIntervalsToBackEdge) {
    // 0: %i = assign 0
    // 1: Label loop_start ($L0)
    // 2: %cond = %i < 10
    // 3: BranchIfFalse %cond, $L1
    // 4: %i = %i + 1
    // 5: Jump $L0
    // 6: Label loop_end ($L1)
    // 7: ret 0
    IrFunction func;
    func.return_type = IrType::I32;

    RegId reg_i    = func.allocate_reg(IrType::I32);  // 0
    RegId reg_cond = func.allocate_reg(IrType::Bool); // 1

    LabelId l_start = func.allocate_label(); // 0
    LabelId l_end   = func.allocate_label(); // 1

    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Assign,
        .dest = IrOperand::MakeVar(reg_i, IrType::I32),
        .src1 = IrOperand::MakeConst(0, IrType::I32),
    });
    func.instructions.push_back(IrInstruction{
        .op    = IrOpcode::Label,
        .label = l_start,
    });
    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::CmpLt,
        .dest = IrOperand::MakeTemp(reg_cond, IrType::Bool),
        .src1 = IrOperand::MakeVar(reg_i, IrType::I32),
        .src2 = IrOperand::MakeConst(10, IrType::I32),
    });
    func.instructions.push_back(IrInstruction{
        .op    = IrOpcode::BranchIfFalse,
        .label = l_end,
        .src1  = IrOperand::MakeTemp(reg_cond, IrType::Bool),
    });
    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Add,
        .dest = IrOperand::MakeVar(reg_i, IrType::I32),
        .src1 = IrOperand::MakeVar(reg_i, IrType::I32),
        .src2 = IrOperand::MakeConst(1, IrType::I32),
    });
    func.instructions.push_back(IrInstruction{
        .op    = IrOpcode::Jump,
        .label = l_start,
    });
    func.instructions.push_back(IrInstruction{
        .op    = IrOpcode::Label,
        .label = l_end,
    });
    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Return,
        .src1 = IrOperand::MakeConst(0, IrType::I32),
    });

    LivenessResult result = compute_liveness(func);

    // %i はループ内で更新され、末尾の Jump $L0 (命令5) を越えて先頭へ戻るため、
    // end は最低でも命令5（Jump）まで引き延ばされる必要がある。
    EXPECT_EQ(result.intervals_by_reg[reg_i].start, 0U);
    EXPECT_EQ(result.intervals_by_reg[reg_i].end, 5U);
}



