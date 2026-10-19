#pragma once

#include <cstdint>
#include <string_view>

#include "axiom/base/compiler.h"
#include "axiom/ir/ir.h"

#include <tree_sitter/api.h>

namespace axiom::ir {
class IrBuilder {
public:
    IrBuilder(std::string_view source_code, StringInterner& interner);

    [[nodiscard]] IrModule build(TSNode root_node);

private:
    std::string_view source_;
    StringInterner& interner_;
    uint32_t temp_counter_ { 0 };

    IrFunction build_function(TSNode func_node);

    void build_block(TSNode block_node, IrFunction& current_func);

    void build_statement(TSNode stmt_node, IrFunction& current_func);

    IrOperand build_expression(TSNode expr_node, IrFunction& current_func);

    IrOperand allocate_temp(StringID type_id);

    [[nodiscard]] std::string_view get_node_text(TSNode node) const;

    [[nodiscard]] TSNode child_by_field(TSNode node, std::string_view name) const;

    StringID field_id(TSNode node, std::string_view field_name, std::string_view fallback = "");

    void emit(IrFunction& func, IrOpcode op,
              IrOperand dest = IrOperand::MakeNone(),
              IrOperand src1 = IrOperand::MakeNone(),
              IrOperand src2 = IrOperand::MakeNone());

    static IrOpcode parse_arith_op(std::string_view op_text);
};
}  // namespace axiom::ir