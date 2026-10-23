#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <tree_sitter/api.h>

#include "axiom/base/compiler.h"
#include "axiom/ir/ir.h"

namespace axiom::ir {
class IrBuilder {
public:
    IrBuilder(std::string_view source_code, StringInterner& interner);

    [[nodiscard]] IrModule build(TSNode root_node);

private:
    struct LoopContext {
        LabelId continue_label;
        LabelId break_label;
    };

    std::vector<LoopContext> loop_stack_;

    std::string_view                    source_;
    StringInterner&                     interner_;
    std::unordered_map<StringID, RegId> local_vars_;

    IrFunction build_function(TSNode func_node);

    void build_block(TSNode block_node, IrFunction& current_func);

    void build_statement(TSNode stmt_node, IrFunction& current_func);

    IrOperand build_expression(TSNode expr_node, IrFunction& current_func);

    static IrOperand allocate_temp(IrFunction& func, IrType type);

    [[nodiscard]] std::string_view get_node_text(TSNode node) const;

    [[nodiscard]] TSNode child_by_field(TSNode node, std::string_view name) const;

    StringID field_id(TSNode node, std::string_view field_name, std::string_view fallback = "");

    [[nodiscard]] IrType field_type(TSNode node, std::string_view field_name, IrType fallback = IrType::Unknown) const;

    void emit(IrFunction& func,
              IrOpcode    op,
              IrOperand   dest  = IrOperand::MakeNone(),
              IrOperand   src1  = IrOperand::MakeNone(),
              IrOperand   src2  = IrOperand::MakeNone(),
              LabelId     label = kInvalidLabelId);

    void emit_label(IrFunction& func, LabelId label);

    void emit_jump(IrFunction& func, LabelId label);

    void emit_branch_false(IrFunction& func, IrOperand cond, LabelId label);

    static IrOpcode parse_arith_op(std::string_view op_text);
};
}  // namespace axiom::ir
