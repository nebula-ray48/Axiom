#include "axiom/ir/ir_printer.h"

#include <string>

namespace axiom::ir {

namespace {

std::string format_reg_identifier(
    const IrFunction& func,
    RegId reg,
    const StringInterner* interner,
    bool use_reg_names
) {
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

std::string format_label(
    const IrFunction& func,
    LabelId label,
    const StringInterner* interner
) {
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

std::string format_operand(
    const IrFunction& func,
    const IrOperand& op,
    const StringInterner* interner,
    bool use_reg_names
) {
    if (op.kind == IrOperandKind::None) {
        return "";
    }
    if (op.kind == IrOperandKind::Constant) {
        return std::to_string(op.constant_int) + ": " + std::string(format_ir_type(op.type));
    }
    return format_reg_identifier(func, op.reg, interner, use_reg_names)
        + ": " + std::string(format_ir_type(op.type));
}

} // namespace

std::string dump_ir(const IrFunction& func, const StringInterner* interner, bool use_reg_names) {
    std::string out = "fun ";
    if (interner != nullptr && func.name_id != kInvalidStringID) {
        out += interner->GetString(func.name_id);
    } else {
        out += "fn_" + std::to_string(func.name_id);
    }

    out += "(";
    for (size_t i = 0; i < func.parameters.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }
        const auto& param = func.parameters[i];
        out += format_reg_identifier(func, param.reg, interner, use_reg_names)
            + ": " + std::string(format_ir_type(param.type));
    }
    out += ") -> " + std::string(format_ir_type(func.return_type)) + " {\n";

    for (const auto& inst : func.instructions) {
        out += "  ";
        switch (inst.op) {
            case IrOpcode::Alloca:
                out += "alloca " + format_operand(func, inst.dest, interner, use_reg_names);
                break;
            case IrOpcode::Assign:
                out += format_operand(func, inst.dest, interner, use_reg_names)
                    + " = assign " + format_operand(func, inst.src1, interner, use_reg_names);
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
                out += format_operand(func, inst.dest, interner, use_reg_names)
                    + " = " + std::string(format_ir_opcode(inst.op)) + " "
                    + format_operand(func, inst.src1, interner, use_reg_names) + ", "
                    + format_operand(func, inst.src2, interner, use_reg_names);
                break;
            case IrOpcode::Label:
                out += "label " + format_label(func, inst.label, interner);
                break;
            case IrOpcode::Jump:
                out += "jmp " + format_label(func, inst.label, interner);
                break;
            case IrOpcode::BranchIfFalse:
                out += "br_false " + format_operand(func, inst.src1, interner, use_reg_names)
                    + ", " + format_label(func, inst.label, interner);
                break;
            case IrOpcode::Return:
                out += "ret";
                if (inst.src1.kind != IrOperandKind::None) {
                    out += " " + format_operand(func, inst.src1, interner, use_reg_names);
                }
                break;
        }
        out += "\n";
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

} // namespace axiom::ir

