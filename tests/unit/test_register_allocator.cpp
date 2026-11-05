/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>

#include "axiom/ir/ir.h"
#include "axiom/jit/liveness.h"
#include "axiom/jit/register_allocator.h"

using namespace axiom;
using namespace axiom::ir;
using namespace axiom::jit;

TEST(RegisterAllocatorTest, OverlappingIntervalsGetDifferentRegisters) {
    // 2つの変数が同時に生きている場合、異なる物理レジスタが割り当てられる
    IrFunction func;
    RegId reg_a = func.allocate_reg(IrType::I32); // 0
    RegId reg_b = func.allocate_reg(IrType::I32); // 1

    LivenessResult liveness;
    // reg_a: [0, 5], reg_b: [2, 6] (重なっている)
    liveness.sorted_intervals = {
        LiveInterval{ .reg = reg_a, .start = 0, .end = 5 },
        LiveInterval{ .reg = reg_b, .start = 2, .end = 6 },
    };
    liveness.intervals_by_reg.resize(2);
    liveness.intervals_by_reg[reg_a] = liveness.sorted_intervals[0];
    liveness.intervals_by_reg[reg_b] = liveness.sorted_intervals[1];

    RegAllocResult result = allocate_registers(func, liveness);

    ASSERT_EQ(result.locations.size(), 2U);
    EXPECT_EQ(result.locations[reg_a].kind, StorageKind::Register);
    EXPECT_EQ(result.locations[reg_b].kind, StorageKind::Register);

    // 割り当てられた物理レジスタは Callee-saved (19〜28) の範囲内
    EXPECT_GE(result.locations[reg_a].index, 19U);
    EXPECT_LE(result.locations[reg_a].index, 28U);
    EXPECT_GE(result.locations[reg_b].index, 19U);
    EXPECT_LE(result.locations[reg_b].index, 28U);

    // 重なっているため、異なるレジスタでなければならない
    EXPECT_NE(result.locations[reg_a].index, result.locations[reg_b].index);
    EXPECT_EQ(result.num_spill_slots, 0U);
}

TEST(RegisterAllocatorTest, NonOverlappingIntervalsReuseRegister) {
    // 寿命が被っていない2つの変数は、同じ物理レジスタを再利用（再割り当て）できる
    IrFunction func;
    RegId reg_a = func.allocate_reg(IrType::I32); // 0
    RegId reg_b = func.allocate_reg(IrType::I32); // 1

    LivenessResult liveness;
    // reg_a: [0, 2], reg_b: [4, 6] (被っていない)
    liveness.sorted_intervals = {
        LiveInterval{ .reg = reg_a, .start = 0, .end = 2 },
        LiveInterval{ .reg = reg_b, .start = 4, .end = 6 },
    };
    liveness.intervals_by_reg.resize(2);
    liveness.intervals_by_reg[reg_a] = liveness.sorted_intervals[0];
    liveness.intervals_by_reg[reg_b] = liveness.sorted_intervals[1];

    RegAllocResult result = allocate_registers(func, liveness);

    ASSERT_EQ(result.locations.size(), 2U);
    EXPECT_EQ(result.locations[reg_a].kind, StorageKind::Register);
    EXPECT_EQ(result.locations[reg_b].kind, StorageKind::Register);

    // reg_a が終わった後に reg_b が始まるため、同じレジスタを再利用できる
    EXPECT_EQ(result.locations[reg_a].index, result.locations[reg_b].index);
    EXPECT_EQ(result.num_spill_slots, 0U);
}

TEST(RegisterAllocatorTest, SpillWhenRegistersExhausted) {
    // 10本の物理レジスタに対して、11個の変数が同時に生きている場合
    // 10個がレジスタに、1個がスタックスロットにスピルされる
    IrFunction func;
    std::vector<RegId> regs;
    for (int i = 0; i < 11; ++i) {
        regs.push_back(func.allocate_reg(IrType::I32));
    }

    LivenessResult liveness;
    // 11個すべてが start = 0 で同時に始まる
    for (size_t i = 0; i < 11; ++i) {
        uint32_t end_time = (i == 10) ? 20 : 10;
        liveness.sorted_intervals.push_back(LiveInterval{
            .reg   = regs[i],
            .start = 0,
            .end   = end_time,
        });
    }
    liveness.intervals_by_reg = liveness.sorted_intervals;

    RegAllocResult result = allocate_registers(func, liveness);

    ASSERT_EQ(result.locations.size(), 11U);
    EXPECT_EQ(result.num_spill_slots, 1U);

    uint32_t reg_count   = 0;
    uint32_t spill_count = 0;
    for (const auto& loc : result.locations) {
        if (loc.kind == StorageKind::Register) {
            ++reg_count;
        } else if (loc.kind == StorageKind::StackSlot) {
            ++spill_count;
        }
    }
    EXPECT_EQ(reg_count, 10U);
    EXPECT_EQ(spill_count, 1U);
}


