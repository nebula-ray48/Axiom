#include <gtest/gtest.h>
#include <tree_sitter/api.h>

#include "axiom/base/compiler.h"
#include "axiom/codegen/c99_codegen.h"
#include "axiom/ir/ir_builder.h"

using namespace axiom;
using namespace axiom::ir;

namespace {

std::string CompileToC99(std::string_view code, StringInterner& interner) {
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

    C99CodeGen codegen(interner);
    return codegen.generate(module);
}

} // namespace

TEST(C99CodeGenTest, SimpleAdditionFunction) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun add(a: int32, b: int32) -> int32 {
            val res: int32 = a + b;
            return res;
        }
    )";

    std::string c_code = CompileToC99(code, interner);

    const std::string expected =
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n\n"
        "int32_t add(int32_t a, int32_t b) {\n"
        "    int32_t res;\n"
        "    int32_t _t3 = a + b;\n"
        "    res = _t3;\n"
        "    return res;\n"
        "}\n\n";

    EXPECT_EQ(c_code, expected);
}

TEST(C99CodeGenTest, VoidFunctionNoParams) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun do_nothing() -> void {
            return;
        }
    )";

    std::string c_code = CompileToC99(code, interner);

    const std::string expected =
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n\n"
        "void do_nothing(void) {\n"
        "    return;\n"
        "}\n\n";

    EXPECT_EQ(c_code, expected);
}

TEST(C99CodeGenTest, CompoundAssignmentAndArithmetic) {
    StringInterner interner;
    constexpr std::string_view code = R"(
        fun calc(x: int32) -> int32 {
            var total: int32 = x;
            total += 10;
            return total;
        }
    )";

    std::string c_code = CompileToC99(code, interner);

    const std::string expected =
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n\n"
        "int32_t calc(int32_t x) {\n"
        "    int32_t total;\n"
        "    total = x;\n"
        "    int32_t _t2 = total + 10;\n"
        "    total = _t2;\n"
        "    return total;\n"
        "}\n\n";

    EXPECT_EQ(c_code, expected);
}
