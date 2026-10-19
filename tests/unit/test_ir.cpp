#include <gtest/gtest.h>
#include <tree_sitter/api.h>

#include "axiom/base/compiler.h"
#include "axiom/ir/ir.h"
#include "axiom/ir/ir_builder.h"

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
    EXPECT_EQ(interner.GetString(func.return_type_id), "int32");

    ASSERT_EQ(func.parameters.size(), 2);
    EXPECT_EQ(interner.GetString(func.parameters[0].name_id), "a");
    EXPECT_EQ(interner.GetString(func.parameters[0].type_id), "int32");
    EXPECT_EQ(interner.GetString(func.parameters[1].name_id), "b");
    EXPECT_EQ(interner.GetString(func.parameters[1].type_id), "int32");

    // 命令列の検証:
    // 1. Alloca : res
    // 2. Add    : %0 = a + b
    // 3. Assign : res = %0
    // 4. Return : res
    ASSERT_EQ(func.instructions.size(), 4);

    // 1. Alloca res
    EXPECT_EQ(func.instructions[0].op, IrOpcode::Alloca);
    EXPECT_EQ(func.instructions[0].dest.kind, IrOperandKind::Variable);
    EXPECT_EQ(interner.GetString(func.instructions[0].dest.id), "res");

    // 2. Add %0 = a + b
    EXPECT_EQ(func.instructions[1].op, IrOpcode::Add);
    EXPECT_EQ(func.instructions[1].dest.kind, IrOperandKind::Temporary);
    EXPECT_EQ(interner.GetString(func.instructions[1].dest.id), "%0");
    EXPECT_EQ(func.instructions[1].src1.kind, IrOperandKind::Variable);
    EXPECT_EQ(interner.GetString(func.instructions[1].src1.id), "a");
    EXPECT_EQ(func.instructions[1].src2.kind, IrOperandKind::Variable);
    EXPECT_EQ(interner.GetString(func.instructions[1].src2.id), "b");

    // 3. Assign res = %0
    EXPECT_EQ(func.instructions[2].op, IrOpcode::Assign);
    EXPECT_EQ(func.instructions[2].dest.kind, IrOperandKind::Variable);
    EXPECT_EQ(interner.GetString(func.instructions[2].dest.id), "res");
    EXPECT_EQ(func.instructions[2].src1.kind, IrOperandKind::Temporary);
    EXPECT_EQ(interner.GetString(func.instructions[2].src1.id), "%0");

    // 4. Return res
    EXPECT_EQ(func.instructions[3].op, IrOpcode::Return);
    EXPECT_EQ(func.instructions[3].src1.kind, IrOperandKind::Variable);
    EXPECT_EQ(interner.GetString(func.instructions[3].src1.id), "res");
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

    // Instructions:
    // 1. Alloca x
    // 2. Add    %0 = 10 + 20
    // 3. Mul    %1 = %0 * 2
    // 4. Assign x = %1
    // 5. Return x
    ASSERT_EQ(func.instructions.size(), 5);

    EXPECT_EQ(func.instructions[0].op, IrOpcode::Alloca);

    EXPECT_EQ(func.instructions[1].op, IrOpcode::Add);
    EXPECT_EQ(interner.GetString(func.instructions[1].dest.id), "%0");
    EXPECT_EQ(func.instructions[1].src1.constant_int, 10);
    EXPECT_EQ(func.instructions[1].src2.constant_int, 20);

    EXPECT_EQ(func.instructions[2].op, IrOpcode::Mul);
    EXPECT_EQ(interner.GetString(func.instructions[2].dest.id), "%1");
    EXPECT_EQ(interner.GetString(func.instructions[2].src1.id), "%0");
    EXPECT_EQ(func.instructions[2].src2.constant_int, 2);

    EXPECT_EQ(func.instructions[3].op, IrOpcode::Assign);
    EXPECT_EQ(interner.GetString(func.instructions[3].dest.id), "x");
    EXPECT_EQ(interner.GetString(func.instructions[3].src1.id), "%1");

    EXPECT_EQ(func.instructions[4].op, IrOpcode::Return);
    EXPECT_EQ(interner.GetString(func.instructions[4].src1.id), "x");
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

    // Instructions:
    // 1. Alloca hp
    // 2. Assign hp = 100
    // 3. Add    %0 = hp + 20
    // 4. Assign hp = %0
    // 5. Return hp
    ASSERT_EQ(func.instructions.size(), 5);

    EXPECT_EQ(func.instructions[0].op, IrOpcode::Alloca);
    EXPECT_EQ(interner.GetString(func.instructions[0].dest.id), "hp");

    EXPECT_EQ(func.instructions[1].op, IrOpcode::Assign);
    EXPECT_EQ(interner.GetString(func.instructions[1].dest.id), "hp");
    EXPECT_EQ(func.instructions[1].src1.constant_int, 100);

    EXPECT_EQ(func.instructions[2].op, IrOpcode::Add);
    EXPECT_EQ(interner.GetString(func.instructions[2].dest.id), "%0");
    EXPECT_EQ(interner.GetString(func.instructions[2].src1.id), "hp");
    EXPECT_EQ(func.instructions[2].src2.constant_int, 20);

    EXPECT_EQ(func.instructions[3].op, IrOpcode::Assign);
    EXPECT_EQ(interner.GetString(func.instructions[3].dest.id), "hp");
    EXPECT_EQ(interner.GetString(func.instructions[3].src1.id), "%0");

    EXPECT_EQ(func.instructions[4].op, IrOpcode::Return);
    EXPECT_EQ(interner.GetString(func.instructions[4].src1.id), "hp");
}

