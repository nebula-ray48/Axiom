/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>
#include <tree_sitter/api.h>

#include "axiom/base/compiler.h"
#include "axiom/ir/ir.h"
#include "axiom/ir/ir_builder.h"
#include "axiom/ir/ir_printer.h"
#include "axiom/ir/ir_verifier.h"

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

TEST(IrBuilderTest, SimpleAdditionAndReturn) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun add(a: int32, b: int32) -> int32 {
            val res: int32 = a + b;
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);

    ASSERT_EQ(module.functions.size(), 1);
    const IrFunction& func = module.functions[0];

    EXPECT_EQ(interner.GetString(func.name_id), "add");
    EXPECT_EQ(func.return_type, IrType::I32);

    ASSERT_EQ(func.parameters.size(), 2);
    EXPECT_EQ(interner.GetString(func.parameters[0].name_id), "a");
    EXPECT_EQ(func.parameters[0].type, IrType::I32);
    EXPECT_EQ(func.parameters[0].reg, 0U);
    EXPECT_EQ(interner.GetString(func.parameters[1].name_id), "b");
    EXPECT_EQ(func.parameters[1].type, IrType::I32);
    EXPECT_EQ(func.parameters[1].reg, 1U);

    // レジスタテーブル (a=0, b=1, res=2, temp=3)
    ASSERT_EQ(func.reg_types.size(), 4);
    ASSERT_EQ(func.reg_names.size(), 4);

    // 命令列の検証:
    // 1. Alloca : res (reg 2)
    // 2. Add    : %3 = a(reg 0) + b(reg 1)
    // 3. Assign : res(reg 2) = %3
    // 4. Return : res(reg 2)
    ASSERT_EQ(func.instructions.size(), 4);

    // 1. Alloca res
    EXPECT_EQ(func.instructions[0].op, IrOpcode::Alloca);
    EXPECT_EQ(func.instructions[0].dest.kind, IrOperandKind::Variable);
    EXPECT_EQ(func.instructions[0].dest.reg, 2U);
    EXPECT_EQ(interner.GetString(func.reg_names[func.instructions[0].dest.reg]), "res");

    // 2. Add %3 = a + b
    EXPECT_EQ(func.instructions[1].op, IrOpcode::Add);
    EXPECT_EQ(func.instructions[1].dest.kind, IrOperandKind::Temporary);
    EXPECT_EQ(func.instructions[1].dest.reg, 3U);
    EXPECT_EQ(func.instructions[1].src1.kind, IrOperandKind::Variable);
    EXPECT_EQ(func.instructions[1].src1.reg, 0U);
    EXPECT_EQ(func.instructions[1].src2.kind, IrOperandKind::Variable);
    EXPECT_EQ(func.instructions[1].src2.reg, 1U);

    // 3. Assign res = %3
    EXPECT_EQ(func.instructions[2].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[2].dest.kind, IrOperandKind::Variable);
    EXPECT_EQ(func.instructions[2].dest.reg, 2U);
    EXPECT_EQ(func.instructions[2].src1.kind, IrOperandKind::Temporary);
    EXPECT_EQ(func.instructions[2].src1.reg, 3U);

    // 4. Return res
    EXPECT_EQ(func.instructions[3].op, IrOpcode::Return);
    EXPECT_EQ(func.instructions[3].src1.kind, IrOperandKind::Variable);
    EXPECT_EQ(func.instructions[3].src1.reg, 2U);
}

TEST(IrBuilderTest, CompoundArithmeticExpression) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun calc() -> int32 {
            val x = (10 + 20) * 2;
            return x;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);

    ASSERT_EQ(module.functions.size(), 1);
    const IrFunction& func = module.functions[0];

    // Registers: x = 0, temp1 = 1, temp2 = 2
    // Instructions:
    // 1. Alloca x (reg 0)
    // 2. Add    reg 1 = 10 + 20
    // 3. Mul    reg 2 = reg 1 * 2
    // 4. Assign x (reg 0) = reg 2
    // 5. Return x (reg 0)
    ASSERT_EQ(func.instructions.size(), 5);

    EXPECT_EQ(func.instructions[0].op, IrOpcode::Alloca);
    EXPECT_EQ(func.instructions[0].dest.reg, 0U);

    EXPECT_EQ(func.instructions[1].op, IrOpcode::Add);
    EXPECT_EQ(func.instructions[1].dest.reg, 1U);
    EXPECT_EQ(func.instructions[1].src1.constant_int, 10);
    EXPECT_EQ(func.instructions[1].src2.constant_int, 20);

    EXPECT_EQ(func.instructions[2].op, IrOpcode::Mul);
    EXPECT_EQ(func.instructions[2].dest.reg, 2U);
    EXPECT_EQ(func.instructions[2].src1.reg, 1U);
    EXPECT_EQ(func.instructions[2].src2.constant_int, 2);

    EXPECT_EQ(func.instructions[3].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[3].dest.reg, 0U);
    EXPECT_EQ(func.instructions[3].src1.reg, 2U);

    EXPECT_EQ(func.instructions[4].op, IrOpcode::Return);
    EXPECT_EQ(func.instructions[4].src1.reg, 0U);
}

TEST(IrBuilderTest, CompoundAssignment) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun update() -> int32 {
            var hp = 100;
            hp += 20;
            return hp;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);

    ASSERT_EQ(module.functions.size(), 1);
    const IrFunction& func = module.functions[0];

    // Registers: hp = 0, temp = 1
    // Instructions:
    // 1. Alloca hp (reg 0)
    // 2. Assign hp (reg 0) = 100
    // 3. Add    reg 1 = hp (reg 0) + 20
    // 4. Assign hp (reg 0) = reg 1
    // 5. Return hp (reg 0)
    ASSERT_EQ(func.instructions.size(), 5);

    EXPECT_EQ(func.instructions[0].op, IrOpcode::Alloca);
    EXPECT_EQ(func.instructions[0].dest.reg, 0U);
    EXPECT_EQ(interner.GetString(func.reg_names[0]), "hp");

    EXPECT_EQ(func.instructions[1].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[1].dest.reg, 0U);
    EXPECT_EQ(func.instructions[1].src1.constant_int, 100);

    EXPECT_EQ(func.instructions[2].op, IrOpcode::Add);
    EXPECT_EQ(func.instructions[2].dest.reg, 1U);
    EXPECT_EQ(func.instructions[2].src1.reg, 0U);
    EXPECT_EQ(func.instructions[2].src2.constant_int, 20);

    EXPECT_EQ(func.instructions[3].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[3].dest.reg, 0U);
    EXPECT_EQ(func.instructions[3].src1.reg, 1U);

    EXPECT_EQ(func.instructions[4].op, IrOpcode::Return);
    EXPECT_EQ(func.instructions[4].src1.reg, 0U);
}

TEST(IrBuilderTest, DumpIrNamedAndNumberedModes) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun add(a: int32, b: int32) -> int32 {
            val res: int32 = a + b;
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);

    // 1. 名前モード（%a, %b, %res, 一時値は %3）
    const std::string expected_named =
        "fun add(%a: i32, %b: i32) -> i32 {\n"
        "  alloca %res: i32\n"
        "  %3: i32 = add %a: i32, %b: i32\n"
        "  %res: i32 = assign %3: i32\n"
        "  ret %res: i32\n"
        "}\n";
    EXPECT_EQ(dump_ir(module, &interner, true), expected_named);

    // 2. 番号モード（すべて %0, %1, %2, %3）
    const std::string expected_numbered =
        "fun add(%0: i32, %1: i32) -> i32 {\n"
        "  alloca %2: i32\n"
        "  %3: i32 = add %0: i32, %1: i32\n"
        "  %2: i32 = assign %3: i32\n"
        "  ret %2: i32\n"
        "}\n";
    EXPECT_EQ(dump_ir(module, &interner, false), expected_numbered);
}

TEST(IrBuilderTest, VerifierPassesValidIrAndDetectsCorruptedIr) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun add(a: int32, b: int32) -> int32 {
            val res: int32 = a + b;
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);

    // 1. 正常な IR はエラーゼロで通過する
    EXPECT_TRUE(verify_module(module, &interner).empty());

    // 2. 境界外の RegId を持つ不正な Alloca 命令を追加すると検出される
    IrFunction bad_reg_func = module.functions[0];
    bad_reg_func.instructions.push_back(IrInstruction{
        .op = IrOpcode::Alloca,
        .dest = IrOperand::MakeVar(99U, IrType::I32)
    });
    auto reg_errors = verify_function(bad_reg_func, &interner);
    ASSERT_EQ(reg_errors.size(), 1U);
    EXPECT_NE(reg_errors[0].message.find("out of bounds"), std::string::npos);

    // 3. reg_types と reg_names の要素数が不一致の場合に即座に検出される
    IrFunction bad_soa_func = module.functions[0];
    bad_soa_func.reg_names.pop_back();
    auto soa_errors = verify_function(bad_soa_func, &interner);
    ASSERT_EQ(soa_errors.size(), 1U);
    EXPECT_NE(soa_errors[0].message.find("mismatch"), std::string::npos);

    // 4. 非 void 関数で空の Return を行った場合に検出される
    IrFunction bad_ret_func = module.functions[0];
    bad_ret_func.instructions.back().src1 = IrOperand::MakeNone();
    auto ret_errors = verify_function(bad_ret_func, &interner);
    ASSERT_EQ(ret_errors.size(), 1U);
    EXPECT_NE(ret_errors[0].message.find("must return a value"), std::string::npos);
}

TEST(IrBuilderTest, ComparisonOperatorsEmitBoolAndPassVerifier) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun check_all_cmp(a: int32, b: int32) -> bool {
            val eq: bool = a == b;
            val ne: bool = a != b;
            val lt: bool = a < b;
            val le: bool = a <= b;
            val gt: bool = a > b;
            val ge: bool = a >= b;
            return ge;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    EXPECT_TRUE(verify_module(module, &interner).empty());

    const std::string expected_named =
        "fun check_all_cmp(%a: i32, %b: i32) -> bool {\n"
        "  alloca %eq: bool\n"
        "  %3: bool = cmp_eq %a: i32, %b: i32\n"
        "  %eq: bool = assign %3: bool\n"
        "  alloca %ne: bool\n"
        "  %5: bool = cmp_ne %a: i32, %b: i32\n"
        "  %ne: bool = assign %5: bool\n"
        "  alloca %lt: bool\n"
        "  %7: bool = cmp_lt %a: i32, %b: i32\n"
        "  %lt: bool = assign %7: bool\n"
        "  alloca %le: bool\n"
        "  %9: bool = cmp_le %a: i32, %b: i32\n"
        "  %le: bool = assign %9: bool\n"
        "  alloca %gt: bool\n"
        "  %11: bool = cmp_gt %a: i32, %b: i32\n"
        "  %gt: bool = assign %11: bool\n"
        "  alloca %ge: bool\n"
        "  %13: bool = cmp_ge %a: i32, %b: i32\n"
        "  %ge: bool = assign %13: bool\n"
        "  ret %ge: bool\n"
        "}\n";
    EXPECT_EQ(dump_ir(module, &interner, true), expected_named);
}

TEST(IrBuilderTest, IfStatementWithoutElse) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun clamp_positive(x: int32) -> int32 {
            var res: int32 = x;
            if res < 0 {
                res = 0;
            }
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    EXPECT_TRUE(verify_module(module, &interner).empty());

    const std::string expected_named =
        "fun clamp_positive(%x: i32) -> i32 {\n"
        "  alloca %res: i32\n"
        "  %res: i32 = assign %x: i32\n"
        "  %2: bool = cmp_lt %res: i32, 0: i32\n"
        "  br_false %2: bool, $L0\n"
        "  %res: i32 = assign 0: i32\n"
        "  label $L0\n"
        "  ret %res: i32\n"
        "}\n";
    EXPECT_EQ(dump_ir(module, &interner, true), expected_named);
}

TEST(IrBuilderTest, IfElseAndElseIfChain) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun classify(x: int32) -> int32 {
            var sign: int32 = 0;
            if x > 0 {
                sign = 1;
            } else if x < 0 {
                sign = 2;
            } else {
                sign = 0;
            }
            return sign;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    EXPECT_TRUE(verify_module(module, &interner).empty());

    const std::string expected_named =
        "fun classify(%x: i32) -> i32 {\n"
        "  alloca %sign: i32\n"
        "  %sign: i32 = assign 0: i32\n"
        "  %2: bool = cmp_gt %x: i32, 0: i32\n"
        "  br_false %2: bool, $L0\n"
        "  %sign: i32 = assign 1: i32\n"
        "  jmp $L1\n"
        "  label $L0\n"
        "  %3: bool = cmp_lt %x: i32, 0: i32\n"
        "  br_false %3: bool, $L2\n"
        "  %sign: i32 = assign 2: i32\n"
        "  jmp $L3\n"
        "  label $L2\n"
        "  %sign: i32 = assign 0: i32\n"
        "  label $L3\n"
        "  label $L1\n"
        "  ret %sign: i32\n"
        "}\n";
    EXPECT_EQ(dump_ir(module, &interner, true), expected_named);
}

TEST(IrBuilderTest, VerifierDetectsInvalidBranchAndLabels) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun check_if(x: int32) -> int32 {
            if x > 0 {
                return 1;
            }
            return 0;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1U);
    EXPECT_TRUE(verify_module(module, &interner).empty());

    // 1. 非 bool 型を BranchIfFalse の条件に渡した場合
    IrFunction bad_cond_func = module.functions[0];
    bad_cond_func.instructions[1].src1 = IrOperand::MakeVar(0, IrType::I32);
    auto cond_errors = verify_function(bad_cond_func, &interner);
    ASSERT_EQ(cond_errors.size(), 1U);
    EXPECT_NE(cond_errors[0].message.find("must have bool type"), std::string::npos);

    // 2. 範囲外のラベル ID にジャンプした場合
    IrFunction bad_label_func = module.functions[0];
    bad_label_func.instructions[1].label = 99;
    auto label_errors = verify_function(bad_label_func, &interner);
    ASSERT_EQ(label_errors.size(), 1U);
    EXPECT_NE(label_errors[0].message.find("out of bounds"), std::string::npos);

    // 3. 確保されたが未配置（Label 命令がない）ラベルへジャンプした場合
    IrFunction undef_label_func = module.functions[0];
    LabelId ghost_label = undef_label_func.allocate_label();
    undef_label_func.instructions[1].label = ghost_label;
    auto undef_errors = verify_function(undef_label_func, &interner);
    ASSERT_EQ(undef_errors.size(), 1U);
    EXPECT_NE(undef_errors[0].message.find("never defined"), std::string::npos);
}

TEST(IrBuilderTest, WhileLoopBasic) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun sum_to(n: int32) -> int32 {
            var i: int32 = 0;
            var total: int32 = 0;
            while i < n {
                i = i + 1;
                total = total + i;
            }
            return total;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    EXPECT_TRUE(verify_module(module, &interner).empty());

    const std::string expected_named =
        "fun sum_to(%n: i32) -> i32 {\n"
        "  alloca %i: i32\n"
        "  %i: i32 = assign 0: i32\n"
        "  alloca %total: i32\n"
        "  %total: i32 = assign 0: i32\n"
        "  label $L0\n"
        "  %3: bool = cmp_lt %i: i32, %n: i32\n"
        "  br_false %3: bool, $L1\n"
        "  %4: i32 = add %i: i32, 1: i32\n"
        "  %i: i32 = assign %4: i32\n"
        "  %5: i32 = add %total: i32, %i: i32\n"
        "  %total: i32 = assign %5: i32\n"
        "  jmp $L0\n"
        "  label $L1\n"
        "  ret %total: i32\n"
        "}\n";
    EXPECT_EQ(dump_ir(module, &interner, true), expected_named);
}

TEST(IrBuilderTest, WhileLoopWithBreakAndContinue) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun search_loop(target: int32) -> int32 {
            var x: int32 = 0;
            while x < 100 {
                x = x + 1;
                if x == target {
                    break;
                }
                if x == 50 {
                    continue;
                }
            }
            return x;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    EXPECT_TRUE(verify_module(module, &interner).empty());

    const std::string expected_named =
        "fun search_loop(%target: i32) -> i32 {\n"
        "  alloca %x: i32\n"
        "  %x: i32 = assign 0: i32\n"
        "  label $L0\n"
        "  %2: bool = cmp_lt %x: i32, 100: i32\n"
        "  br_false %2: bool, $L1\n"
        "  %3: i32 = add %x: i32, 1: i32\n"
        "  %x: i32 = assign %3: i32\n"
        "  %4: bool = cmp_eq %x: i32, %target: i32\n"
        "  br_false %4: bool, $L2\n"
        "  jmp $L1\n"
        "  label $L2\n"
        "  %5: bool = cmp_eq %x: i32, 50: i32\n"
        "  br_false %5: bool, $L3\n"
        "  jmp $L0\n"
        "  label $L3\n"
        "  jmp $L0\n"
        "  label $L1\n"
        "  ret %x: i32\n"
        "}\n";
    EXPECT_EQ(dump_ir(module, &interner, true), expected_named);
}

TEST(IrBuilderTest, DumpCfgDotWhileLoopAndIfElse) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun sum_to(n: int32) -> int32 {
            var i: int32 = 0;
            var total: int32 = 0;
            while i < n {
                i = i + 1;
                total = total + i;
            }
            return total;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 1U);

    const std::string expected_dot =
        "digraph sum_to {\n"
        "  node [shape=record, fontname=\"Courier\"];\n"
        "  edge [fontname=\"Courier\"];\n"
        "  BB0 [label=\"{BB0|alloca %i: i32\\l%i: i32 = assign 0: i32\\lalloca %total: i32\\l%total: i32 = assign 0: i32\\l}\"];\n"
        "  BB1 [label=\"{BB1|label $L0\\l%3: bool = cmp_lt %i: i32, %n: i32\\lbr_false %3: bool, $L1\\l}\"];\n"
        "  BB2 [label=\"{BB2|%4: i32 = add %i: i32, 1: i32\\l%i: i32 = assign %4: i32\\l%5: i32 = add %total: i32, %i: i32\\l%total: i32 = assign %5: i32\\ljmp $L0\\l}\"];\n"
        "  BB3 [label=\"{BB3|label $L1\\lret %total: i32\\l}\"];\n"
        "  BB0 -> BB1 [color=\"#64748b\"];\n"
        "  BB1 -> BB2 [label=\"true\", color=\"#16a34a\"];\n"
        "  BB1 -> BB3 [label=\"false\", color=\"#dc2626\", style=\"dashed\"];\n"
        "  BB2 -> BB1 [label=\"jmp\", color=\"#2563eb\"];\n"
        "}\n";

    EXPECT_EQ(dump_cfg_dot(module.functions[0], &interner, true), expected_dot);

    const std::string module_dot = dump_cfg_dot(module, &interner, true);
    EXPECT_NE(module_dot.find("digraph Module"), std::string::npos);
    EXPECT_NE(module_dot.find("subgraph cluster_sum_to"), std::string::npos);
    EXPECT_NE(module_dot.find("fn0_BB1 -> fn0_BB2 [label=\"true\""), std::string::npos);
    EXPECT_NE(module_dot.find("fn0_BB2 -> fn0_BB1 [label=\"jmp\""), std::string::npos);
}

TEST(IrBuilderTest, DumpIrAndCfgDotWithCallInstruction) {
    StringInterner interner;
    StringID main_id = interner.Intern("main");
    StringID add_id  = interner.Intern("add");
    StringID x_id    = interner.Intern("x");

    IrFunction func;
    func.name_id     = main_id;
    func.return_type = IrType::I32;

    RegId reg_x   = func.allocate_reg(IrType::I32, x_id);
    RegId reg_tmp = func.allocate_reg(IrType::I32);

    // call_args にフラットに引数を格納: (10, %x)
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
        .call_func_name = add_id,
        .arg_start      = arg_start,
        .arg_count      = 2,
    });
    func.instructions.push_back(IrInstruction{
        .op   = IrOpcode::Return,
        .src1 = IrOperand::MakeTemp(reg_tmp, IrType::I32),
    });

    const std::string expected_ir =
        "fun main() -> i32 {\n"
        "  alloca %x: i32\n"
        "  %x: i32 = assign 32: i32\n"
        "  %1: i32 = call @add(10: i32, %x: i32)\n"
        "  ret %1: i32\n"
        "}\n";
    EXPECT_EQ(dump_ir(func, &interner, true), expected_ir);

    const std::string dot = dump_cfg_dot(func, &interner, true);
    EXPECT_NE(dot.find("%1: i32 = call @add(10: i32, %x: i32)"), std::string::npos);
}

TEST(IrBuilderTest, CallExpressionAndNestedCallsFromSource) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun mul(a: int32, b: int32) -> int32 {
            return a * b;
        }
        fun add(a: int32, b: int32) -> int32 {
            return a + b;
        }
        fun compute(x: int32) -> int32 {
            val res: int32 = add(mul(x, 2), 10);
            return res;
        }
    )";

    IrModule module = BuildIrFromSource(code, interner);
    ASSERT_EQ(module.functions.size(), 3U);
    EXPECT_TRUE(verify_module(module, &interner).empty());

    const IrFunction& compute_fn = module.functions[2];
    // mul(x, 2) の2引数 + add(%2, 10) の2引数 = 合計4要素がフラットに格納される
    ASSERT_EQ(compute_fn.call_args.size(), 4U);

    const std::string expected_compute_ir =
        "fun compute(%x: i32) -> i32 {\n"
        "  alloca %res: i32\n"
        "  %2: i32 = call @mul(%x: i32, 2: i32)\n"
        "  %3: i32 = call @add(%2: i32, 10: i32)\n"
        "  %res: i32 = assign %3: i32\n"
        "  ret %res: i32\n"
        "}\n";
    EXPECT_EQ(dump_ir(compute_fn, &interner, true), expected_compute_ir);

    // 不正な Call 命令の検証 (arg_start + arg_count が範囲外)
    IrFunction bad_slice_fn = compute_fn;
    bad_slice_fn.instructions[1].arg_count = 99;
    auto slice_errors = verify_function(bad_slice_fn, &interner);
    ASSERT_EQ(slice_errors.size(), 1U);
    EXPECT_NE(slice_errors[0].message.find("out of bounds"), std::string::npos);

    // 不正な Call 命令の検証 (call_func_name が kInvalidStringID)
    IrFunction bad_callee_fn = compute_fn;
    bad_callee_fn.instructions[1].call_func_name = kInvalidStringID;
    auto callee_errors = verify_function(bad_callee_fn, &interner);
    ASSERT_EQ(callee_errors.size(), 1U);
    EXPECT_NE(callee_errors[0].message.find("invalid target function name"), std::string::npos);
}






