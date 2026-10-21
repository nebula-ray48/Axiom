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

