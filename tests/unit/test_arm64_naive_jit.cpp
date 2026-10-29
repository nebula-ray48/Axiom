/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>
#include <tree_sitter/api.h>

#include <cstdint>
#include <string_view>
#include <vector>

#include "axiom/base/compiler.h"
#include "axiom/ir/ir.h"
#include "axiom/ir/ir_builder.h"
#include "axiom/ir/ir_vm.h"
#include "axiom/jit/arm64_naive_jit.h"
#include "axiom/jit/jit_arena.h"

using namespace axiom;
using namespace axiom::ir;
using namespace axiom::jit;

#if defined(__aarch64__) || defined(_M_ARM64)

namespace {

IrModule BuildIrFromSource(std::string_view code, StringInterner& interner) {
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_axiom());

    TSTree* tree = ts_parser_parse_string(
        parser,
        nullptr,
        code.data(),
        static_cast<uint32_t>(code.length())
    );

    TSNode root_node = ts_tree_root_node(tree);
    IrBuilder builder(code, interner);
    IrModule module = builder.build(root_node);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return module;
}

}  // namespace

TEST(Arm64NaiveJitTest, ReturnConstant) {
    IrFunction func;
    func.return_type = IrType::I32;

    IrInstruction ret_inst;
    ret_inst.op   = IrOpcode::Return;
    ret_inst.src1 = IrOperand::MakeConst(42, IrType::I32);
    func.instructions.push_back(ret_inst);

    std::vector<uint32_t> code = Arm64NaiveJit::compile(func);
    ASSERT_FALSE(code.empty());

    JitCodeArena arena;
    size_t offset = arena.emit_function(code);
    auto fn = arena.get_function<int32_t()>(offset);
    ASSERT_NE(fn, nullptr);

    EXPECT_EQ(fn(), 42);
}

TEST(Arm64NaiveJitTest, AddTwoArgumentsManualIr) {
    IrFunction func;
    func.return_type = IrType::I32;

    RegId reg_a = func.allocate_reg(IrType::I32);
    RegId reg_b = func.allocate_reg(IrType::I32);
    RegId reg_c = func.allocate_reg(IrType::I32);

    func.parameters.push_back(IrParam{
        .name_id = kInvalidStringID,
        .type    = IrType::I32,
        .reg     = reg_a,
    });
    func.parameters.push_back(IrParam{
        .name_id = kInvalidStringID,
        .type    = IrType::I32,
        .reg     = reg_b,
    });

    // c = a + b
    IrInstruction add_inst;
    add_inst.op   = IrOpcode::Add;
    add_inst.dest = IrOperand::MakeTemp(reg_c, IrType::I32);
    add_inst.src1 = IrOperand::MakeVar(reg_a, IrType::I32);
    add_inst.src2 = IrOperand::MakeVar(reg_b, IrType::I32);
    func.instructions.push_back(add_inst);

    // return c
    IrInstruction ret_inst;
    ret_inst.op   = IrOpcode::Return;
    ret_inst.src1 = IrOperand::MakeTemp(reg_c, IrType::I32);
    func.instructions.push_back(ret_inst);

    std::vector<uint32_t> code = Arm64NaiveJit::compile(func);
    ASSERT_FALSE(code.empty());

    JitCodeArena arena;
    size_t offset = arena.emit_function(code);
    auto add_fn = arena.get_function<int32_t(int32_t, int32_t)>(offset);
    ASSERT_NE(add_fn, nullptr);

    EXPECT_EQ(add_fn(12, 30), 42);
    EXPECT_EQ(add_fn(-5, 15), 10);
    EXPECT_EQ(add_fn(1000, 2345), 3345);
    EXPECT_EQ(add_fn(2147483647, 1), -2147483648);  // 32-bit two's complement wrap
}

TEST(Arm64NaiveJitTest, AxiomSourceAddAndDifferentialTestWithIrVm) {
    StringInterner interner;
    constexpr std::string_view source = R"(
        fun calculate(a: int32, b: int32) -> int32 {
            val sum: int32 = a + b;
            return sum;
        }
    )";

    IrModule module = BuildIrFromSource(source, interner);
    ASSERT_EQ(module.functions.size(), 1U);
    const auto& func = module.functions[0];

    // JIT コンパイル
    std::vector<uint32_t> code = Arm64NaiveJit::compile(func);
    ASSERT_FALSE(code.empty());

    JitCodeArena arena;
    size_t offset = arena.emit_function(code);
    auto jit_fn = arena.get_function<int32_t(int32_t, int32_t)>(offset);
    ASSERT_NE(jit_fn, nullptr);

    IrVm vm;

    const std::vector<std::pair<int32_t, int32_t>> test_cases = {
        {10, 20},
        {0, 0},
        {-50, 100},
        {12345, 67890},
        {2147483647, 1},
        {-2147483648, -1},
    };

    for (const auto& [a, b] : test_cases) {
        // 1. VM での実行結果
        std::vector<int64_t> vm_args = {a, b};
        auto vm_res = vm.execute(func, vm_args);
        ASSERT_TRUE(vm_res.ok());

        // 2. JIT (ネイティブ ARM64 CPU) での実行結果
        int32_t jit_res = jit_fn(a, b);

        // 3. Differential Check: 完全に一致することを検証！
        EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
            << "Mismatch for inputs a=" << a << ", b=" << b;
    }
}

TEST(Arm64NaiveJitTest, SubMulDivAndCompoundAssignmentDifferentialTest) {
    StringInterner interner;
    constexpr std::string_view source = R"(
        fun compute(a: int32, b: int32, c: int32) -> int32 {
            var res: int32 = (a + b) * c - (a / b);
            res += 10;
            res -= 3;
            res *= 2;
            res /= 3;
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(source, interner);
    ASSERT_EQ(module.functions.size(), 1U);
    const auto& func = module.functions[0];

    std::vector<uint32_t> code = Arm64NaiveJit::compile(func);
    ASSERT_FALSE(code.empty());

    JitCodeArena arena;
    size_t offset = arena.emit_function(code);
    auto jit_fn = arena.get_function<int32_t(int32_t, int32_t, int32_t)>(offset);
    ASSERT_NE(jit_fn, nullptr);

    IrVm vm;

    struct Input {
        int32_t a;
        int32_t b;
        int32_t c;
    };

    const std::vector<Input> test_cases = {
        {20, 5, 3},
        {100, 10, 4},
        {-40, 8, 5},
        {50, -5, 2},
        {-30, -6, -3},
        {0, 7, 100},
        {10000, 25, 12},
    };

    for (const auto& tc : test_cases) {
        std::vector<int64_t> vm_args = {tc.a, tc.b, tc.c};
        auto vm_res = vm.execute(func, vm_args);
        ASSERT_TRUE(vm_res.ok());

        int32_t jit_res = jit_fn(tc.a, tc.b, tc.c);
        EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
            << "Mismatch for a=" << tc.a << ", b=" << tc.b << ", c=" << tc.c;
    }
}

TEST(Arm64NaiveJitTest, AllComparisonOperatorsDifferentialTest) {
    StringInterner interner;
    constexpr std::string_view source = R"(
        fun test_eq(a: int32, b: int32) -> bool { return a == b; }
        fun test_ne(a: int32, b: int32) -> bool { return a != b; }
        fun test_lt(a: int32, b: int32) -> bool { return a < b; }
        fun test_le(a: int32, b: int32) -> bool { return a <= b; }
        fun test_gt(a: int32, b: int32) -> bool { return a > b; }
        fun test_ge(a: int32, b: int32) -> bool { return a >= b; }
    )";

    IrModule module = BuildIrFromSource(source, interner);
    ASSERT_EQ(module.functions.size(), 6U);

    const std::vector<std::pair<int32_t, int32_t>> test_pairs = {
        {10, 20},
        {20, 10},
        {15, 15},
        {-50, 50},
        {50, -50},
        {-30, -30},
        {0, 0},
        {0, 1},
        {-1, 0},
        {2147483647, -2147483648},
        {-2147483648, 2147483647},
        {2147483647, 2147483647},
        {-2147483648, -2147483648},
    };

    IrVm vm;
    JitCodeArena arena;

    for (const auto& func : module.functions) {
        std::vector<uint32_t> code = Arm64NaiveJit::compile(func);
        ASSERT_FALSE(code.empty());

        size_t offset = arena.emit_function(code);
        auto jit_fn = arena.get_function<int32_t(int32_t, int32_t)>(offset);
        ASSERT_NE(jit_fn, nullptr);

        for (const auto& [a, b] : test_pairs) {
            std::vector<int64_t> vm_args = {a, b};
            auto vm_res = vm.execute(func, vm_args);
            ASSERT_TRUE(vm_res.ok());

            int32_t jit_res = jit_fn(a, b);
            EXPECT_TRUE(jit_res == 0 || jit_res == 1)
                << "Comparison result must be boolean 0 or 1, got " << jit_res;

            EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
                << "Mismatch for function " << interner.GetString(func.name_id)
                << " with inputs a=" << a << ", b=" << b;
        }
    }
}

#endif
