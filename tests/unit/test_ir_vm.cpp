/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>
#include <tree_sitter/api.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

#include "axiom/base/compiler.h"
#include "axiom/ir/ir.h"
#include "axiom/ir/ir_builder.h"
#include "axiom/ir/ir_vm.h"

using namespace axiom;
using namespace axiom::ir;

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

} // namespace

TEST(IrVmTest, BasicAdditionAndAssignment) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun add(a: int32, b: int32) -> int32 {
            val sum: int32 = a + b;
            return sum;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1);

    IrVm vm;
    std::array<int64_t, 2> args = { 10, 25 };
    IrVm::VmResult result = vm.execute(module.functions[0], args);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.trap, IrVm::TrapCode::None);
    EXPECT_EQ(result.value, 35);
}

TEST(IrVmTest, ArithmeticOperatorsAndPrecedence) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun calc(a: int32, b: int32, c: int32) -> int32 {
            val res: int32 = a * b - c / 2;
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1);

    IrVm vm;
    // 7 * 6 - 8 / 2 = 42 - 4 = 38
    std::array<int64_t, 3> args = { 7, 6, 8 };
    IrVm::VmResult result = vm.execute(module.functions[0], args);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value, 38);
}

TEST(IrVmTest, IntegerOverflowWrapAround) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun overflow_add(a: int32, b: int32) -> int32 {
            val res: int32 = a + b;
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1);

    IrVm vm;
    // INT32_MAX + 1 -> INT32_MIN (2の補数 wrap)
    std::array<int64_t, 2> args = { std::numeric_limits<int32_t>::max(), 1 };
    IrVm::VmResult result = vm.execute(module.functions[0], args);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value, std::numeric_limits<int32_t>::min());
}

TEST(IrVmTest, DivideByZeroTrap) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun divide(a: int32, b: int32) -> int32 {
            val res: int32 = a / b;
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1);

    IrVm vm;
    std::array<int64_t, 2> args = { 42, 0 };
    IrVm::VmResult result = vm.execute(module.functions[0], args);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.trap, IrVm::TrapCode::IntegerDivideByZero);
}

TEST(IrVmTest, IntMinDivideByNegativeOneTrap) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun divide(a: int32, b: int32) -> int32 {
            val res: int32 = a / b;
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1);

    IrVm vm;
    std::array<int64_t, 2> args = { std::numeric_limits<int32_t>::min(), -1 };
    IrVm::VmResult result = vm.execute(module.functions[0], args);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.trap, IrVm::TrapCode::IntegerOverflow);
}

TEST(IrVmTest, ComparisonOperators) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun test_comparisons(a: int32, b: int32) -> int32 {
            var score: int32 = 0;
            if a == b { score += 1; }
            if a != b { score += 2; }
            if a < b  { score += 4; }
            if a <= b { score += 8; }
            if a > b  { score += 16; }
            if a >= b { score += 32; }
            return score;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1);

    IrVm vm;
    // a = 10, b = 20 -> != (2), < (4), <= (8) = 14
    std::array<int64_t, 2> args1 = { 10, 20 };
    EXPECT_EQ(vm.execute(module.functions[0], args1).value, 14);

    // a = 20, b = 20 -> == (1), <= (8), >= (32) = 41
    std::array<int64_t, 2> args2 = { 20, 20 };
    EXPECT_EQ(vm.execute(module.functions[0], args2).value, 41);

    // a = 30, b = 20 -> != (2), > (16), >= (32) = 50
    std::array<int64_t, 2> args3 = { 30, 20 };
    EXPECT_EQ(vm.execute(module.functions[0], args3).value, 50);
}

TEST(IrVmTest, IfElseIfElseChains) {
    StringInterner interner;
    constexpr std::string_view code = R"(
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

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1);

    IrVm vm;
    EXPECT_EQ(vm.execute(module.functions[0], std::array<int64_t, 1>{ 95 }).value, 1);
    EXPECT_EQ(vm.execute(module.functions[0], std::array<int64_t, 1>{ 85 }).value, 2);
    EXPECT_EQ(vm.execute(module.functions[0], std::array<int64_t, 1>{ 55 }).value, 3);
    EXPECT_EQ(vm.execute(module.functions[0], std::array<int64_t, 1>{ 30 }).value, 4);
}

TEST(IrVmTest, WhileLoopAccumulator) {
    StringInterner interner;
    constexpr std::string_view code = R"(
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

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1);

    IrVm vm;
    // 1 + 2 + ... + 10 = 55
    std::array<int64_t, 1> args = { 10 };
    IrVm::VmResult result = vm.execute(module.functions[0], args);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value, 55);
}

TEST(IrVmTest, WhileLoopBreakAndContinue) {
    StringInterner interner;
    constexpr std::string_view code = R"(
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

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1);

    IrVm vm;
    // i = 1: sum = 1
    // i = 2: sum = 3
    // i = 3: continue (skipped)
    // i = 4: sum = 7
    // i = 5: sum = 12 (>= 10 -> break)
    std::array<int64_t, 1> args = { 10 };
    IrVm::VmResult result = vm.execute(module.functions[0], args);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value, 12);
}

TEST(IrVmTest, FunctionCallsAndNestedArguments) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun mul(a: int32, b: int32) -> int32 {
            return a * b;
        }
        fun add(a: int32, b: int32) -> int32 {
            return a + b;
        }
        fun compute(x: int32, y: int32) -> int32 {
            val res: int32 = add(mul(x, 3), mul(y, 4));
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 3U);

    IrVm vm;
    StringID compute_id = interner.Intern("compute");
    // compute(5, 6) = 5*3 + 6*4 = 15 + 24 = 39
    std::array<int64_t, 2> args = { 5, 6 };
    IrVm::VmResult result = vm.execute(module, compute_id, args);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value, 39);
}

TEST(IrVmTest, RecursiveFactorialAndFibonacci) {
    StringInterner interner;
    constexpr std::string_view code = R"(
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

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 2U);

    IrVm vm;
    StringID fact_id = interner.Intern("fact");
    StringID fib_id  = interner.Intern("fib");

    // 5! = 120
    IrVm::VmResult fact_res = vm.execute(module, fact_id, std::array<int64_t, 1>{ 5 });
    EXPECT_TRUE(fact_res.ok());
    EXPECT_EQ(fact_res.value, 120);

    // fib(10) = 55
    IrVm::VmResult fib_res = vm.execute(module, fib_id, std::array<int64_t, 1>{ 10 });
    EXPECT_TRUE(fib_res.ok());
    EXPECT_EQ(fib_res.value, 55);
}


