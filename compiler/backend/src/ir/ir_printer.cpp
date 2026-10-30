/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include "axiom/ir/ir_printer.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace axiom::ir {

namespace {

std::string format_reg_identifier(const IrFunction&     func,
                                  RegId                 reg,
                                  const StringInterner* interner,
                                  bool                  use_reg_names) {
    if (reg == kInvalidRegId) {
        return "%invalid";
    }
    if (use_reg_names && interner != nullptr && reg < func.reg_names.size()) {
        StringID name_id = func.reg_names[reg];
        if (name_id != kInvalidStringID) {
            return "%" + std::string(interner->GetString(name_id));
        }
    }
    return "%" + std::to_string(reg);
}

std::string format_label(const IrFunction& func, LabelId label, const StringInterner* interner) {
    if (label == kInvalidLabelId) {
        return "$invalid";
    }
    if (interner != nullptr && label < func.label_names.size()) {
        StringID name_id = func.label_names[label];
        if (name_id != kInvalidStringID) {
            return "$" + std::string(interner->GetString(name_id));
        }
    }
    return "$L" + std::to_string(label);
}

std::string format_operand(const IrFunction&     func,
                           const IrOperand&      op,
                           const StringInterner* interner,
                           bool                  use_reg_names) {
    if (op.kind == IrOperandKind::None) {
        return "";
    }
    if (op.kind == IrOperandKind::Constant) {
        return std::to_string(op.constant_int) + ": " + std::string(format_ir_type(op.type));
    }
    return format_reg_identifier(func, op.reg, interner, use_reg_names) + ": " + std::string(format_ir_type(op.type));
}

std::string format_func_name(const IrFunction& func, const StringInterner* interner) {
    if (interner != nullptr && func.name_id != kInvalidStringID) {
        return std::string(interner->GetString(func.name_id));
    }
    return "fn_" + std::to_string(func.name_id);
}

std::string format_instruction(const IrFunction&     func,
                               const IrInstruction&  inst,
                               const StringInterner* interner,
                               bool                  use_reg_names) {
    std::string out;
    switch (inst.op) {
        case IrOpcode::Alloca:
            out += "alloca " + format_operand(func, inst.dest, interner, use_reg_names);
            break;
        case IrOpcode::Assign:
            out += format_operand(func, inst.dest, interner, use_reg_names) + " = assign " +
                   format_operand(func, inst.src1, interner, use_reg_names);
            break;
        case IrOpcode::Add:
        case IrOpcode::Sub:
        case IrOpcode::Mul:
        case IrOpcode::Div:
        case IrOpcode::CmpEq:
        case IrOpcode::CmpNe:
        case IrOpcode::CmpLt:
        case IrOpcode::CmpLe:
        case IrOpcode::CmpGt:
        case IrOpcode::CmpGe:
            out += format_operand(func, inst.dest, interner, use_reg_names) + " = " +
                   std::string(format_ir_opcode(inst.op)) + " " +
                   format_operand(func, inst.src1, interner, use_reg_names) + ", " +
                   format_operand(func, inst.src2, interner, use_reg_names);
            break;
        case IrOpcode::Label:
            out += "label " + format_label(func, inst.label, interner);
            break;
        case IrOpcode::Jump:
            out += "jmp " + format_label(func, inst.label, interner);
            break;
        case IrOpcode::BranchIfFalse:
            out += "br_false " + format_operand(func, inst.src1, interner, use_reg_names) + ", " +
                   format_label(func, inst.label, interner);
            break;
        case IrOpcode::Call: {
            if (inst.dest.kind != IrOperandKind::None) {
                out += format_operand(func, inst.dest, interner, use_reg_names) + " = ";
            }
            out += "call @";
            if (interner != nullptr && inst.call_func_name != kInvalidStringID) {
                out += interner->GetString(inst.call_func_name);
            } else {
                out += "fn_" + std::to_string(inst.call_func_name);
            }
            out += "(";
            for (uint32_t i = 0; i < inst.arg_count; ++i) {
                if (i > 0) {
                    out += ", ";
                }
                uint32_t idx = inst.arg_start + i;
                if (idx < func.call_args.size()) {
                    out += format_operand(func, func.call_args[idx], interner, use_reg_names);
                } else {
                    out += "<invalid_arg>";
                }
            }
            out += ")";
            break;
        }
        case IrOpcode::Return:
            out += "ret";
            if (inst.src1.kind != IrOperandKind::None) {
                out += " " + format_operand(func, inst.src1, interner, use_reg_names);
            }
            break;
    }
    return out;
}

bool is_terminator(IrOpcode op) noexcept {
    return op == IrOpcode::Jump || op == IrOpcode::BranchIfFalse || op == IrOpcode::Return;
}

std::string escape_dot_record_label(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (char c : text) {
        if (c == '<' || c == '>' || c == '{' || c == '}' || c == '|' || c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

struct CfgBlock {
    size_t start_idx { 0 };
    size_t end_idx { 0 };
    bool   has_non_label { false };
};

void append_function_cfg_nodes_and_edges(std::string&          out,
                                         const IrFunction&     func,
                                         const StringInterner* interner,
                                         bool                  use_reg_names,
                                         std::string_view      node_prefix,
                                         std::string_view      indent) {
    std::vector<CfgBlock> blocks;
    std::unordered_map<LabelId, size_t> label_to_block;

    for (size_t i = 0; i < func.instructions.size(); ++i) {
        const auto& inst = func.instructions[i];
        bool start_new = false;
        if (i == 0) {
            start_new = true;
        } else if (is_terminator(func.instructions[i - 1].op)) {
            start_new = true;
        } else if (inst.op == IrOpcode::Label && blocks.back().has_non_label) {
            start_new = true;
        }

        if (start_new) {
            blocks.push_back(CfgBlock{ .start_idx = i, .end_idx = i + 1, .has_non_label = false });
        } else {
            blocks.back().end_idx = i + 1;
        }

        if (inst.op == IrOpcode::Label) {
            label_to_block[inst.label] = blocks.size() - 1;
        } else {
            blocks.back().has_non_label = true;
        }
    }

    for (size_t b_idx = 0; b_idx < blocks.size(); ++b_idx) {
        const auto& block = blocks[b_idx];
        std::string node_name = std::string(node_prefix) + "BB" + std::to_string(b_idx);

        out += std::string(indent) + node_name + " [label=\"{BB" + std::to_string(b_idx) + "|";
        for (size_t i = block.start_idx; i < block.end_idx; ++i) {
            std::string inst_str = format_instruction(func, func.instructions[i], interner, use_reg_names);
            out += escape_dot_record_label(inst_str) + "\\l";
        }
        out += "}\"];\n";
    }

    for (size_t b_idx = 0; b_idx < blocks.size(); ++b_idx) {
        const auto& block = blocks[b_idx];
        if (block.start_idx >= block.end_idx) {
            continue;
        }
        std::string src_node = std::string(node_prefix) + "BB" + std::to_string(b_idx);
        const auto& last_inst = func.instructions[block.end_idx - 1];

        if (last_inst.op == IrOpcode::Jump) {
            auto it = label_to_block.find(last_inst.label);
            if (it != label_to_block.end()) {
                std::string dst_node = std::string(node_prefix) + "BB" + std::to_string(it->second);
                out += std::string(indent) + src_node + " -> " + dst_node +
                       " [label=\"jmp\", color=\"#2563eb\"];\n";
            }
        } else if (last_inst.op == IrOpcode::BranchIfFalse) {
            if (b_idx + 1 < blocks.size()) {
                std::string true_node = std::string(node_prefix) + "BB" + std::to_string(b_idx + 1);
                out += std::string(indent) + src_node + " -> " + true_node +
                       " [label=\"true\", color=\"#16a34a\"];\n";
            }
            auto it = label_to_block.find(last_inst.label);
            if (it != label_to_block.end()) {
                std::string false_node = std::string(node_prefix) + "BB" + std::to_string(it->second);
                out += std::string(indent) + src_node + " -> " + false_node +
                       " [label=\"false\", color=\"#dc2626\", style=\"dashed\"];\n";
            }
        } else if (last_inst.op == IrOpcode::Return) {
            // 関数終端のため後続ブロックへのエッジなし
        } else {
            if (b_idx + 1 < blocks.size()) {
                std::string next_node = std::string(node_prefix) + "BB" + std::to_string(b_idx + 1);
                out += std::string(indent) + src_node + " -> " + next_node + " [color=\"#64748b\"];\n";
            }
        }
    }
}

}  // namespace

std::string dump_ir(const IrFunction& func, const StringInterner* interner, bool use_reg_names) {
    std::string out = "fun " + format_func_name(func, interner) + "(";

    for (size_t i = 0; i < func.parameters.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }
        const auto& param = func.parameters[i];
        out += format_reg_identifier(func, param.reg, interner, use_reg_names) + ": " +
               std::string(format_ir_type(param.type));
    }
    out += ") -> " + std::string(format_ir_type(func.return_type)) + " {\n";

    for (const auto& inst : func.instructions) {
        out += "  " + format_instruction(func, inst, interner, use_reg_names) + "\n";
    }

    out += "}\n";
    return out;
}

std::string dump_ir(const IrModule& module, const StringInterner* interner, bool use_reg_names) {
    std::string out;
    for (size_t i = 0; i < module.functions.size(); ++i) {
        if (i > 0) {
            out += "\n";
        }
        out += dump_ir(module.functions[i], interner, use_reg_names);
    }
    return out;
}

std::string dump_cfg_dot(const IrFunction& func, const StringInterner* interner, bool use_reg_names) {
    std::string func_name = format_func_name(func, interner);
    std::string out = "digraph " + func_name + " {\n";
    out += "  node [shape=record, fontname=\"Courier\"];\n";
    out += "  edge [fontname=\"Courier\"];\n";

    append_function_cfg_nodes_and_edges(out, func, interner, use_reg_names, "", "  ");

    out += "}\n";
    return out;
}

std::string dump_cfg_dot(const IrModule& module, const StringInterner* interner, bool use_reg_names) {
    std::string out = "digraph Module {\n";
    out += "  node [shape=record, fontname=\"Courier\"];\n";
    out += "  edge [fontname=\"Courier\"];\n";

    for (size_t i = 0; i < module.functions.size(); ++i) {
        const auto& func = module.functions[i];
        std::string func_name = format_func_name(func, interner);
        std::string prefix = "fn" + std::to_string(i) + "_";

        out += "  subgraph cluster_" + func_name + " {\n";
        out += "    label=\"fun " + func_name + "\";\n";
        out += "    fontname=\"Courier\";\n";
        append_function_cfg_nodes_and_edges(out, func, interner, use_reg_names, prefix, "    ");
        out += "  }\n";
    }

    out += "}\n";
    return out;
}

}  // namespace axiom::ir
