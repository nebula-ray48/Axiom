/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "axiom/jit/jit_arena.h"
#include "axiom/target/arm64.h"

using namespace axiom::jit;
using namespace axiom::arm64;
using namespace axiom::arm64::reg;

#if defined(__aarch64__) || defined(_M_ARM64)

TEST(JitArenaTest, ReturnConstant42) {
    JitCodeArena arena;

    // mov w0, #42
    // ret
    const std::array<uint32_t, 2> code = {
        encode_movz(w0, 42),
        encode_ret(),
    };

    size_t offset = arena.emit_function(code);
    EXPECT_EQ(offset, 0U);
    EXPECT_EQ(arena.used_bytes(), 8U);

    auto fn = arena.get_function<int32_t()>(offset);
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn(), 42);
}

TEST(JitArenaTest, AddTwoArguments) {
    JitCodeArena arena;
    // AAPCS64: 第1引数 = w0, 第2引数 = w1, 戻り値 = w0
    // add w0, w0, w1
    // ret
    const std::array<uint32_t, 2> code = {
        encode_add(w0, w0, w1),
        encode_ret(),
    };

    size_t offset = arena.emit_function(code);
    auto   add_fn = arena.get_function<int32_t(int32_t, int32_t)>(offset);

    EXPECT_EQ(add_fn(15, 27), 42);
    EXPECT_EQ(add_fn(-100, 250), 150);
    EXPECT_EQ(add_fn(2147483647, 1), -2147483648);  // 32-bit wrap-around on real hardware!
}

TEST(JitArenaTest, ComplexArithmetic5Args) {
    JitCodeArena arena;

    // ((w0 + w1) * w2 / w3) - w4
    const std::array<uint32_t, 5> code = {
        encode_add(w0, w0, w1),
        encode_mul(w0, w0, w2),
        encode_sdiv(w0, w0, w3),
        encode_sub(w0, w0, w4),
        encode_ret(),
    };

    size_t offset  = arena.emit_function(code);
    auto   calc_fn = arena.get_function<int32_t(int32_t, int32_t, int32_t, int32_t, int32_t)>(offset);

    // ((10 + 20) * 4 / 3) - 5 = (30 * 4 / 3) - 5 = 40 - 5 = 35
    EXPECT_EQ(calc_fn(10, 20, 4, 3, 5), 35);
}

TEST(JitArenaTest, MultipleFunctionsAnd16ByteAlignment) {
    JitCodeArena arena;

    // Function 1 (8 bytes): w0 = w0 + 10
    const std::array<uint32_t, 2> code1 = {
        encode_add_imm(w0, w0, 10),
        encode_ret(),
    };
    // Function 2 (8 bytes): w0 = w0 * w1
    const std::array<uint32_t, 2> code2 = {
        encode_mul(w0, w0, w1),
        encode_ret(),
    };
    // Function 3 (12 bytes): 32-bit constant 0x12345678 via movz + movk
    const std::array<uint32_t, 3> code3 = {
        encode_movz(w0, 0x5678, 0),
        encode_movk(w0, 0x1234, 16),
        encode_ret(),
    };

    size_t off1 = arena.emit_function(code1);
    size_t off2 = arena.emit_function(code2);
    size_t off3 = arena.emit_function(code3);

    // 16バイト境界に正しく整列されていることを検証
    EXPECT_EQ(off1, 0U);
    EXPECT_EQ(off2, 16U);
    EXPECT_EQ(off3, 32U);

    auto fn1 = arena.get_function<int32_t(int32_t)>(off1);
    auto fn2 = arena.get_function<int32_t(int32_t, int32_t)>(off2);
    auto fn3 = arena.get_function<int32_t()>(off3);

    EXPECT_EQ(fn1(32), 42);
    EXPECT_EQ(fn2(6, 7), 42);
    EXPECT_EQ(fn3(), 0x12345678);
}

TEST(JitArenaTest, NativeLoopAccumulator) {
    JitCodeArena arena;

    // int32_t sum_1_to_n(int32_t n [w0]):
    //   0: movz w1, #0          (acc = 0)
    //   1: cbz  w0, +4          (if n == 0 -> jump to inst 5)
    //   2: add  w1, w1, w0      (acc += n)
    //   3: sub  w0, w0, #1      (n -= 1)
    //   4: b    -3              (jump back to inst 1)
    //   5: mov  w0, w1          (return acc)
    //   6: ret
    const std::array<uint32_t, 7> code = {
        encode_movz(w1, 0),
        encode_cbz(w0, 4),
        encode_add(w1, w1, w0),
        encode_sub_imm(w0, w0, 1),
        encode_b(-3),
        encode_mov(w0, w1),
        encode_ret(),
    };

    size_t offset = arena.emit_function(code);
    auto   sum_fn = arena.get_function<int32_t(int32_t)>(offset);

    EXPECT_EQ(sum_fn(0), 0);
    EXPECT_EQ(sum_fn(10), 55);
    EXPECT_EQ(sum_fn(100), 5050);
}

TEST(JitArenaTest, ArenaResetZeroCostReuse) {
    JitCodeArena arena;

    const std::array<uint32_t, 2> code_a = {
        encode_movz(w0, 100),
        encode_ret(),
    };
    size_t off_a = arena.emit_function(code_a);
    EXPECT_EQ(arena.get_function<int32_t()>(off_a)(), 100);

    // DOD Reset: OS のシステムコール (mmap/munmap) を呼ばずにオフセットを 0 に戻して再利用
    arena.reset();
    EXPECT_EQ(arena.used_bytes(), 0U);

    const std::array<uint32_t, 2> code_b = {
        encode_movz(w0, 777),
        encode_ret(),
    };
    size_t off_b = arena.emit_function(code_b);
    EXPECT_EQ(off_b, 0U);
    EXPECT_EQ(arena.get_function<int32_t()>(off_b)(), 777);
}

#endif
