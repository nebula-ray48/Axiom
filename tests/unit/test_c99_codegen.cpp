/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <tree_sitter/api.h>

#include "axiom/base/compiler.h"
#include "axiom/codegen/c99_codegen.h"
#include "axiom/ir/ir.h"
#include "axiom/ir/ir_builder.h"

extern "C" const TSLanguage* tree_sitter_axiom();

namespace {

using namespace axiom;

std::string CompileToC99(std::string_view source, StringInterner& interner) {
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_axiom());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source.data(), static_cast<uint32_t>(source.size()));
    TSNode  root = ts_tree_root_node(tree);

    ir::IrBuilder builder(source, interner);
    ir::IrModule  module = builder.build(root);

    C99CodeGen  codegen(interner);
    std::string c_code = codegen.generate(module);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return c_code;
}

TEST(C99CodeGenTest, SimpleAdditionFunction) {
    StringInterner             interner;
    constexpr std::string_view code = R"(
        fun add(a: int32, b: int32) -> int32 {
            return a + b;
        }
    )";

    std::string c_code = CompileToC99(code, interner);

    const std::string expected =
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n\n"
        "int32_t add(int32_t a, int32_t b) {\n"
        "    int32_t _t2 = a + b;\n"
        "    return _t2;\n"
        "}\n\n";

    EXPECT_EQ(c_code, expected);
}

TEST(C99CodeGenTest, VoidFunctionNoParams) {
    StringInterner             interner;
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
    StringInterner             interner;
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

TEST(C99CodeGenTest, ComparisonOperators) {
    StringInterner             interner;
    constexpr std::string_view code = R"(
        fun is_alive(hp: int32) -> bool {
            val alive: bool = hp > 0;
            return alive;
        }
    )";

    std::string c_code = CompileToC99(code, interner);

    const std::string expected =
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n\n"
        "bool is_alive(int32_t hp) {\n"
        "    bool alive;\n"
        "    bool _t2 = hp > 0;\n"
        "    alive = _t2;\n"
        "    return alive;\n"
        "}\n\n";

    EXPECT_EQ(c_code, expected);
}

TEST(C99CodeGenTest, IfElseStatement) {
    StringInterner             interner;
    constexpr std::string_view code = R"(
        fun max_val(a: int32, b: int32) -> int32 {
            var res: int32 = a;
            if b > a {
                res = b;
            } else {
                res = a;
            }
            return res;
        }
    )";

    std::string c_code = CompileToC99(code, interner);

    const std::string expected =
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n\n"
        "int32_t max_val(int32_t a, int32_t b) {\n"
        "    int32_t res;\n"
        "    res = a;\n"
        "    bool _t3 = b > a;\n"
        "    if (!_t3) goto L0;\n"
        "    res = b;\n"
        "    goto L1;\n"
        "L0:;\n"
        "    res = a;\n"
        "L1:;\n"
        "    return res;\n"
        "}\n\n";

    EXPECT_EQ(c_code, expected);
}

TEST(C99CodeGenTest, WhileLoopAndBreakContinue) {
    StringInterner             interner;
    constexpr std::string_view code = R"(
        fun count_down(start: int32) -> int32 {
            var n: int32 = start;
            while n > 0 {
                if n == 5 {
                    break;
                }
                n = n - 1;
            }
            return n;
        }
    )";

    std::string c_code = CompileToC99(code, interner);

    const std::string expected =
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n\n"
        "int32_t count_down(int32_t start) {\n"
        "    int32_t n;\n"
        "    n = start;\n"
        "L0:;\n"
        "    bool _t2 = n > 0;\n"
        "    if (!_t2) goto L1;\n"
        "    bool _t3 = n == 5;\n"
        "    if (!_t3) goto L2;\n"
        "    goto L1;\n"
        "L2:;\n"
        "    int32_t _t4 = n - 1;\n"
        "    n = _t4;\n"
        "    goto L0;\n"
        "L1:;\n"
        "    return n;\n"
        "}\n\n";

    EXPECT_EQ(c_code, expected);
}

}  // namespace
