#include "axiom/ir/ir_builder.h"

#include <charconv>
#include <format>
#include <string>

#include "tree_sitter/api.h"

#include "axiom/ir/ir.h"

namespace axiom::ir {

IrBuilder::IrBuilder(std::string_view source_code, StringInterner& interner)
    : source_(source_code), interner_(interner) {}

std::string_view IrBuilder::get_node_text(TSNode node) const {
    if (ts_node_is_null(node)) {
        return std::string_view{};
    }

    uint32_t start = ts_node_start_byte(node);
    uint32_t end   = ts_node_end_byte(node);
    return source_.substr(start, end - start);
}

TSNode IrBuilder::child_by_field(TSNode node, std::string_view name) const {
    return ts_node_child_by_field_name(node, name.data(), static_cast<uint32_t>(name.size()));
}

StringID IrBuilder::field_id(TSNode node, std::string_view field_name, std::string_view fallback) {
    TSNode child = child_by_field(node, field_name);
    if (ts_node_is_null(child)) {
        return fallback.empty() ? kInvalidStringID : interner_.Intern(fallback);
    }
    return interner_.Intern(get_node_text(child));
}

IrType IrBuilder::field_type(TSNode node, std::string_view field_name, IrType fallback) const {
    TSNode child = child_by_field(node, field_name);
    if (ts_node_is_null(child)) {
        return fallback;
    }

    IrType parsed = parse_ir_type(get_node_text(child));
    return (parsed != IrType::Unknown) ? parsed : fallback;
}

void IrBuilder::emit(IrFunction& func, IrOpcode op, IrOperand dest, IrOperand src1, IrOperand src2, LabelId label) {
    func.instructions.push_back(IrInstruction{.op = op, .label = label, .dest = dest, .src1 = src1, .src2 = src2});
}

void IrBuilder::emit_label(IrFunction& func, LabelId label) {
    emit(func, IrOpcode::Label, {}, {}, {}, label);
}

void IrBuilder::emit_jump(IrFunction& func, LabelId label) {
    emit(func, IrOpcode::Jump, {}, {}, {}, label);
}

void IrBuilder::emit_branch_false(IrFunction& func, IrOperand cond, LabelId label) {
    emit(func, IrOpcode::BranchIfFalse, {}, cond, {}, label);
}

IrOpcode IrBuilder::parse_arith_op(std::string_view op_text) {
    if (op_text.starts_with('+')) return IrOpcode::Add;
    if (op_text.starts_with('-')) return IrOpcode::Sub;
    if (op_text.starts_with('*')) return IrOpcode::Mul;
    if (op_text.starts_with('/')) return IrOpcode::Div;
    return IrOpcode::Add;
}

IrOperand IrBuilder::allocate_temp(IrFunction& func, IrType type) {
    RegId temp_reg = func.allocate_reg(type);
    return IrOperand::MakeTemp(temp_reg, type);
}

IrModule IrBuilder::build(TSNode root_node) {
    IrModule module;
    uint32_t count = ts_node_named_child_count(root_node);

    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_named_child(root_node, i);
        if (std::string_view(ts_node_type(child)) == "function_declaration") {
            module.functions.push_back(build_function(child));
        }
    }

    return module;
}

IrFunction IrBuilder::build_function(TSNode func_node) {
    local_vars_.clear();
    loop_stack_.clear();
    IrFunction func;

    // 1. 関数名 & 戻り値型（ヘルパーで1行ずつ！）
    func.name_id     = field_id(func_node, "name");
    func.return_type = field_type(func_node, "return_type", IrType::Void);

    // 2. 引数リスト
    uint32_t count = ts_node_named_child_count(func_node);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_named_child(func_node, i);
        if (std::string_view(ts_node_type(child)) == "parameter_list") {
            uint32_t param_count = ts_node_named_child_count(child);
            for (uint32_t j = 0; j < param_count; ++j) {
                TSNode param_node = ts_node_named_child(child, j);
                if (std::string_view(ts_node_type(param_node)) == "parameter") {
                    StringID name_id     = field_id(param_node, "name");
                    IrType   type        = field_type(param_node, "type");
                    RegId    reg         = func.allocate_reg(type, name_id);
                    local_vars_[name_id] = reg;
                    func.parameters.push_back({name_id, type, reg});
                }
            }
        }
    }

    // 3. 関数本体ブロック
    if (TSNode body_node = child_by_field(func_node, "body"); !ts_node_is_null(body_node)) {
        build_block(body_node, func);
    }

    return func;
}

void IrBuilder::build_block(TSNode block_node, IrFunction& current_func) {
    uint32_t count = ts_node_named_child_count(block_node);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode stmt = ts_node_named_child(block_node, i);
        build_statement(stmt, current_func);
    }
}

void IrBuilder::build_statement(TSNode stmt_node, IrFunction& current_func) {
    std::string_view type = ts_node_type(stmt_node);

    if (type == "variable_declaration") {
        StringID name_id     = field_id(stmt_node, "name");
        IrType   var_type    = field_type(stmt_node, "type", IrType::I32);
        RegId    reg         = current_func.allocate_reg(var_type, name_id);
        local_vars_[name_id] = reg;
        IrOperand var_op     = IrOperand::MakeVar(reg, var_type);

        emit(current_func, IrOpcode::Alloca, var_op);

        if (TSNode val_node = child_by_field(stmt_node, "value"); !ts_node_is_null(val_node)) {
            emit(current_func, IrOpcode::Assign, var_op, build_expression(val_node, current_func));
        }
    } else if (type == "return_statement") {
        IrOperand ret_val = (ts_node_named_child_count(stmt_node) != 0)
                                ? build_expression(ts_node_named_child(stmt_node, 0), current_func)
                                : IrOperand::MakeNone();
        emit(current_func, IrOpcode::Return, IrOperand::MakeNone(), ret_val);
    } else if (type == "expression_statement" && ts_node_named_child_count(stmt_node) != 0) {
        build_expression(ts_node_named_child(stmt_node, 0), current_func);
    } else if (type == "if_statement") {
        TSNode    cond_node   = child_by_field(stmt_node, "condition");
        IrOperand cond_op     = build_expression(cond_node, current_func);
        TSNode    consequence = child_by_field(stmt_node, "consequence");
        TSNode    alternative = child_by_field(stmt_node, "alternative");

        if (ts_node_is_null(alternative)) {
            LabelId end_label = current_func.allocate_label();
            emit_branch_false(current_func, cond_op, end_label);

            if (!ts_node_is_null(consequence)) {
                build_block(consequence, current_func);
            }

            emit_label(current_func, end_label);
        } else {
            LabelId else_label = current_func.allocate_label();
            LabelId end_label  = current_func.allocate_label();
            emit_branch_false(current_func, cond_op, else_label);

            if (!ts_node_is_null(consequence)) {
                build_block(consequence, current_func);
            }
            emit_jump(current_func, end_label);

            emit_label(current_func, else_label);
            if (std::string_view(ts_node_type(alternative)) == "block") {
                build_block(alternative, current_func);
            } else {
                build_statement(alternative, current_func);
            }

            emit_label(current_func, end_label);
        }
    } else if (type == "while_statement") {
        TSNode cond_node = child_by_field(stmt_node, "condition");
        TSNode body_node = child_by_field(stmt_node, "body");

        LabelId cond_label = current_func.allocate_label();
        LabelId end_label  = current_func.allocate_label();

        emit_label(current_func, cond_label);

        IrOperand cond_op = build_expression(cond_node, current_func);
        emit_branch_false(current_func, cond_op, end_label);

        loop_stack_.push_back({.continue_label = cond_label, .break_label = end_label});

        if (!ts_node_is_null(body_node)) {
            build_block(body_node, current_func);
        }
        loop_stack_.pop_back();

        emit_jump(current_func, cond_label);
        emit_label(current_func, end_label);
    } else if (type == "break_statement") {
        if (!loop_stack_.empty()) {
            emit_jump(current_func, loop_stack_.back().break_label);
        }
    } else if (type == "continue_statement") {
        if (!loop_stack_.empty()) {
            emit_jump(current_func, loop_stack_.back().continue_label);
        }
    }
}

IrOperand IrBuilder::build_expression(TSNode expr_node, IrFunction& current_func) {
    if (ts_node_is_null(expr_node)) {
        return IrOperand::MakeNone();
    }

    std::string_view type = ts_node_type(expr_node);

    // 1. 整数リテラル
    if (type == "number") {
        std::string_view text = get_node_text(expr_node);
        int64_t          val  = 0;
        std::from_chars(text.data(), text.data() + text.size(), val);
        return IrOperand::MakeConst(val, IrType::I32);
    }

    // 2. 変数参照（識別子）
    if (type == "identifier") {
        StringID name_id = interner_.Intern(get_node_text(expr_node));
        auto     it      = local_vars_.find(name_id);
        if (it == local_vars_.end()) {
            return IrOperand::MakeNone();
        }
        RegId reg = it->second;
        return IrOperand::MakeVar(reg, current_func.reg_types[reg]);
    }

    // 3. 括弧式 (a + b)
    if (type == "parenthesized_expression" && ts_node_named_child_count(expr_node) != 0) {
        return build_expression(ts_node_named_child(expr_node, 0), current_func);
    }

    // 4. 代入式 (x = 10, x += 5)
    if (type == "assignment_expression") {
        TSNode left_node  = child_by_field(expr_node, "left");
        TSNode op_node    = child_by_field(expr_node, "operator");
        TSNode right_node = child_by_field(expr_node, "right");

        IrOperand rhs    = build_expression(right_node, current_func);
        StringID  var_id = interner_.Intern(get_node_text(left_node));
        auto      it     = local_vars_.find(var_id);
        if (it == local_vars_.end()) {
            return IrOperand::MakeNone();
        }
        RegId     reg  = it->second;
        IrOperand dest = IrOperand::MakeVar(reg, current_func.reg_types[reg]);

        std::string_view op_text = get_node_text(op_node);
        if (op_text == "=") {
            emit(current_func, IrOpcode::Assign, dest, rhs);
        } else {
            IrOpcode  bin_op = parse_arith_op(op_text);
            IrOperand temp   = allocate_temp(current_func, dest.type);
            emit(current_func, bin_op, temp, dest, rhs);
            emit(current_func, IrOpcode::Assign, dest, temp);
        }
        return dest;
    }

    // 5. 二項演算式 (lhs + rhs)
    if (type == "binary_expression") {
        IrOperand        lhs     = build_expression(ts_node_child(expr_node, 0), current_func);
        IrOperand        rhs     = build_expression(ts_node_child(expr_node, 2), current_func);
        std::string_view op_text = get_node_text(ts_node_child(expr_node, 1));

        IrOpcode op;
        IrType   res_type;

        if (op_text == "==") {
            op       = IrOpcode::CmpEq;
            res_type = IrType::Bool;
        } else if (op_text == "!=") {
            op       = IrOpcode::CmpNe;
            res_type = IrType::Bool;
        } else if (op_text == "<") {
            op       = IrOpcode::CmpLt;
            res_type = IrType::Bool;
        } else if (op_text == "<=") {
            op       = IrOpcode::CmpLe;
            res_type = IrType::Bool;
        } else if (op_text == ">") {
            op       = IrOpcode::CmpGt;
            res_type = IrType::Bool;
        } else if (op_text == ">=") {
            op       = IrOpcode::CmpGe;
            res_type = IrType::Bool;
        } else {
            op       = parse_arith_op(op_text);
            res_type = (lhs.type != IrType::Unknown) ? lhs.type : IrType::I32;
        }
        IrOperand temp = allocate_temp(current_func, res_type);
        emit(current_func, op, temp, lhs, rhs);
        return temp;
    }

    return IrOperand::MakeNone();
}

}  // namespace axiom::ir
