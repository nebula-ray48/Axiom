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

TEST(Arm64NaiveJitTest, WhileLoopAccumulatorDifferentialTest) {
    StringInterner interner;
    constexpr std::string_view source = R"(
        fun sum_to(n: int32) -> int32 {
            var sum: int32 = 0;
            var i: int32 = 1;
            while i <= n {
                sum += i;
                i += 1;
            }
            return sum;
        }
    )";

    IrModule module = BuildIrFromSource(source, interner);
    ASSERT_EQ(module.functions.size(), 1U);
    const auto& func = module.functions[0];

    std::vector<uint32_t> code = Arm64NaiveJit::compile(func);
    ASSERT_FALSE(code.empty());

    JitCodeArena arena;
    size_t offset = arena.emit_function(code);
    auto jit_fn = arena.get_function<int32_t(int32_t)>(offset);
    ASSERT_NE(jit_fn, nullptr);

    IrVm vm;
    const std::vector<int32_t> test_cases = {0, 1, 5, 10, 50, 100, 1000};

    for (int32_t n : test_cases) {
        auto vm_res = vm.execute(func, std::vector<int64_t>{n});
        ASSERT_TRUE(vm_res.ok());

        int32_t jit_res = jit_fn(n);
        int32_t expected = (n <= 0) ? 0 : (n * (n + 1)) / 2;

        EXPECT_EQ(jit_res, expected) << "Direct formula mismatch for n=" << n;
        EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
            << "Differential VM mismatch for n=" << n;
    }
}

TEST(Arm64NaiveJitTest, WhileLoopBreakAndContinueDifferentialTest) {
    StringInterner interner;
    constexpr std::string_view source = R"(
        fun loop_control(limit: int32) -> int32 {
            var sum: int32 = 0;
            var i: int32 = 0;
            while i < 100 {
                i += 1;
                if i == 3 {
                    continue;
                }
                sum += i;
                if sum >= limit {
                    break;
                }
            }
            return sum;
        }
    )";

    IrModule module = BuildIrFromSource(source, interner);
    ASSERT_EQ(module.functions.size(), 1U);
    const auto& func = module.functions[0];

    std::vector<uint32_t> code = Arm64NaiveJit::compile(func);
    ASSERT_FALSE(code.empty());

    JitCodeArena arena;
    size_t offset = arena.emit_function(code);
    auto jit_fn = arena.get_function<int32_t(int32_t)>(offset);
    ASSERT_NE(jit_fn, nullptr);

    IrVm vm;
    const std::vector<int32_t> limits = {0, 1, 2, 5, 10, 50, 100, 500, 2000, 10000};

    for (int32_t limit : limits) {
        auto vm_res = vm.execute(func, std::vector<int64_t>{limit});
        ASSERT_TRUE(vm_res.ok());

        int32_t jit_res = jit_fn(limit);
        EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
            << "Loop control mismatch for limit=" << limit;
    }
}

TEST(Arm64NaiveJitTest, IfElseIfElseChainsDifferentialTest) {
    StringInterner interner;
    constexpr std::string_view source = R"(
        fun classify(score: int32) -> int32 {
            if score >= 90 {
                return 1;
            } else if score >= 70 {
                return 2;
            } else if score >= 50 {
                return 3;
            } else {
                return 4;
            }
        }
    )";

    IrModule module = BuildIrFromSource(source, interner);
    ASSERT_EQ(module.functions.size(), 1U);
    const auto& func = module.functions[0];

    std::vector<uint32_t> code = Arm64NaiveJit::compile(func);
    ASSERT_FALSE(code.empty());

    JitCodeArena arena;
    size_t offset = arena.emit_function(code);
    auto jit_fn = arena.get_function<int32_t(int32_t)>(offset);
    ASSERT_NE(jit_fn, nullptr);

    IrVm vm;
    const std::vector<int32_t> scores = {
        100, 95, 90, 89, 80, 70, 69, 60, 50, 49, 10, 0, -10, -100
    };

    for (int32_t score : scores) {
        auto vm_res = vm.execute(func, std::vector<int64_t>{score});
        ASSERT_TRUE(vm_res.ok());

        int32_t jit_res = jit_fn(score);
        EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
            << "Classification mismatch for score=" << score;
    }
}

TEST(Arm64NaiveJitTest, NestedLoopsDifferentialTest) {
    StringInterner interner;
    constexpr std::string_view source = R"(
        fun nested_grid(rows: int32, cols: int32) -> int32 {
            var sum: int32 = 0;
            var r: int32 = 0;
            while r < rows {
                var c: int32 = 0;
                while c < cols {
                    sum += 1;
                    c += 1;
                }
                r += 1;
            }
            return sum;
        }
    )";

    IrModule module = BuildIrFromSource(source, interner);
    ASSERT_EQ(module.functions.size(), 1U);
    const auto& func = module.functions[0];

    std::vector<uint32_t> code = Arm64NaiveJit::compile(func);
    ASSERT_FALSE(code.empty());

    JitCodeArena arena;
    size_t offset = arena.emit_function(code);
    auto jit_fn = arena.get_function<int32_t(int32_t, int32_t)>(offset);
    ASSERT_NE(jit_fn, nullptr);

    IrVm vm;
    const std::vector<std::pair<int32_t, int32_t>> test_pairs = {
        {0, 0},
        {0, 5},
        {5, 0},
        {1, 1},
        {3, 4},
        {5, 5},
        {10, 20},
        {25, 40},
    };

    for (const auto& [rows, cols] : test_pairs) {
        auto vm_res = vm.execute(func, std::vector<int64_t>{rows, cols});
        ASSERT_TRUE(vm_res.ok());

        int32_t jit_res = jit_fn(rows, cols);
        EXPECT_EQ(jit_res, rows * cols) << "Expected rows*cols for (" << rows << ", " << cols << ")";
        EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
            << "Differential VM mismatch for rows=" << rows << ", cols=" << cols;
    }
}

TEST(Arm64NaiveJitTest, MultiFunctionCallsDifferentialTest) {
    StringInterner interner;
    constexpr std::string_view source = R"(
        fun mul(a: int32, b: int32) -> int32 {
            return a * b;
        }
        fun add(a: int32, b: int32) -> int32 {
            return a + b;
        }
        fun square_diff(a: int32, b: int32) -> int32 {
            return mul(a, a) - mul(b, b);
        }
        fun compute(x: int32, y: int32) -> int32 {
            val part1: int32 = add(mul(x, 3), mul(y, 4));
            val part2: int32 = square_diff(x, y);
            return add(part1, part2);
        }
    )";

    IrModule module = BuildIrFromSource(source, interner);
    ASSERT_EQ(module.functions.size(), 4U);

    CompiledModule compiled = Arm64NaiveJit::compile_module(module);
    ASSERT_FALSE(compiled.code.empty());

    StringID compute_id = interner.Intern("compute");
    ASSERT_TRUE(compiled.func_inst_offsets.contains(compute_id));

    JitCodeArena arena;
    size_t base_offset = arena.emit_function(compiled.code);
    size_t compute_byte_offset = base_offset + compiled.func_inst_offsets.at(compute_id) * sizeof(uint32_t);
    auto jit_compute = arena.get_function<int32_t(int32_t, int32_t)>(compute_byte_offset);
    ASSERT_NE(jit_compute, nullptr);

    IrVm vm;
    const std::vector<std::pair<int32_t, int32_t>> test_cases = {
        {0, 0},
        {5, 6},
        {10, 3},
        {-4, 7},
        {12, -8},
        {100, 50},
    };

    for (const auto& [x, y] : test_cases) {
        auto vm_res = vm.execute(module, compute_id, std::vector<int64_t>{x, y});
        ASSERT_TRUE(vm_res.ok());

        int32_t jit_res = jit_compute(x, y);
        EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
            << "Multi-function call mismatch for x=" << x << ", y=" << y;
    }
}

TEST(Arm64NaiveJitTest, RecursiveFactorialAndFibonacciDifferentialTest) {
    StringInterner interner;
    constexpr std::string_view source = R"(
        fun fact(n: int32) -> int32 {
            if n <= 1 {
                return 1;
            }
            return n * fact(n - 1);
        }
        fun fib(n: int32) -> int32 {
            if n <= 1 {
                return n;
            }
            return fib(n - 1) + fib(n - 2);
        }
    )";

    IrModule module = BuildIrFromSource(source, interner);
    ASSERT_EQ(module.functions.size(), 2U);

    CompiledModule compiled = Arm64NaiveJit::compile_module(module);
    ASSERT_FALSE(compiled.code.empty());

    StringID fact_id = interner.Intern("fact");
    StringID fib_id  = interner.Intern("fib");

    JitCodeArena arena;
    size_t base_offset = arena.emit_function(compiled.code);

    auto jit_fact = arena.get_function<int32_t(int32_t)>(
        base_offset + compiled.func_inst_offsets.at(fact_id) * sizeof(uint32_t));
    auto jit_fib = arena.get_function<int32_t(int32_t)>(
        base_offset + compiled.func_inst_offsets.at(fib_id) * sizeof(uint32_t));
    ASSERT_NE(jit_fact, nullptr);
    ASSERT_NE(jit_fib, nullptr);

    IrVm vm;

    // 1. 再帰階乗 fact(0) ~ fact(12)
    for (int32_t n = 0; n <= 12; ++n) {
        auto vm_res = vm.execute(module, fact_id, std::vector<int64_t>{n});
        ASSERT_TRUE(vm_res.ok());

        int32_t jit_res = jit_fact(n);
        EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
            << "Recursive factorial mismatch for n=" << n;
    }

    // 2. 二重再帰フィボナッチ fib(0) ~ fib(15)
    for (int32_t n = 0; n <= 15; ++n) {
        auto vm_res = vm.execute(module, fib_id, std::vector<int64_t>{n});
        ASSERT_TRUE(vm_res.ok());

        int32_t jit_res = jit_fib(n);
        EXPECT_EQ(jit_res, static_cast<int32_t>(vm_res.value))
            << "Recursive fibonacci mismatch for n=" << n;
    }
}

#endif
