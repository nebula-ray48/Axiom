#include "axiom/codegen/c99_codegen.h"

#include <string>
#include <string_view>

namespace axiom {

C99CodeGen::C99CodeGen(const StringInterner& interner)
    : interner_(interner) {}

std::string C99CodeGen::map_type(ir::IrType type) {
    switch (type) {
        case ir::IrType::I8:   return "int8_t";
        case ir::IrType::I16:  return "int16_t";
        case ir::IrType::I32:  return "int32_t";
        case ir::IrType::I64:  return "int64_t";
        case ir::IrType::U8:   return "uint8_t";
        case ir::IrType::U16:  return "uint16_t";
        case ir::IrType::U32:  return "uint32_t";
        case ir::IrType::U64:  return "uint64_t";
        case ir::IrType::F32:  return "float";
        case ir::IrType::F64:  return "double";
        case ir::IrType::Bool: return "bool";
        case ir::IrType::Void:
        case ir::IrType::Unknown:
        default:               return "void";
    }
}

std::string C99CodeGen::format_operand(const ir::IrFunction& func, const ir::IrOperand& op) const {
    if (op.kind == ir::IrOperandKind::Constant) {
        return std::to_string(op.constant_int);
    }
    if (op.kind == ir::IrOperandKind::Variable) {
        return std::string(interner_.GetString(func.reg_names[op.reg]));
    }
    if (op.kind == ir::IrOperandKind::Temporary) {
        return "_t" + std::to_string(op.reg);
    }
    return "";
}

void C99CodeGen::generate_instruction(const ir::IrFunction& func, const ir::IrInstruction& inst, std::string& out) {
    if (inst.op == ir::IrOpcode::Alloca) {
        out += "    " + map_type(inst.dest.type) + " " + format_operand(func, inst.dest) + ";\n";
    } else if (inst.op == ir::IrOpcode::Assign) {
        out += "    " + format_operand(func, inst.dest) + " = " + format_operand(func, inst.src1) + ";\n";
    } else if (inst.op == ir::IrOpcode::Add || inst.op == ir::IrOpcode::Sub ||
               inst.op == ir::IrOpcode::Mul || inst.op == ir::IrOpcode::Div ||
               inst.op == ir::IrOpcode::CmpEq || inst.op == ir::IrOpcode::CmpNe ||
               inst.op == ir::IrOpcode::CmpLt || inst.op == ir::IrOpcode::CmpLe ||
               inst.op == ir::IrOpcode::CmpGt || inst.op == ir::IrOpcode::CmpGe) {
        std::string_view op_sym = "+";
        if (inst.op == ir::IrOpcode::Add)        op_sym = "+";
        else if (inst.op == ir::IrOpcode::Sub)   op_sym = "-";
        else if (inst.op == ir::IrOpcode::Mul)   op_sym = "*";
        else if (inst.op == ir::IrOpcode::Div)   op_sym = "/";
        else if (inst.op == ir::IrOpcode::CmpEq) op_sym = "==";
        else if (inst.op == ir::IrOpcode::CmpNe) op_sym = "!=";
        else if (inst.op == ir::IrOpcode::CmpLt) op_sym = "<";
        else if (inst.op == ir::IrOpcode::CmpLe) op_sym = "<=";
        else if (inst.op == ir::IrOpcode::CmpGt) op_sym = ">";
        else if (inst.op == ir::IrOpcode::CmpGe) op_sym = ">=";

        out += "    " + map_type(inst.dest.type) + " " + format_operand(func, inst.dest) + " = "
            + format_operand(func, inst.src1) + " " + std::string(op_sym) + " " + format_operand(func, inst.src2) + ";\n";
    } else if (inst.op == ir::IrOpcode::Label) {
        out += "L" + std::to_string(inst.label) + ":;\n";
    } else if (inst.op == ir::IrOpcode::Jump) {
        out += "    goto L" + std::to_string(inst.label) + ";\n";
    } else if (inst.op == ir::IrOpcode::BranchIfFalse) {
        out += "    if (!" + format_operand(func, inst.src1) + ") goto L" + std::to_string(inst.label) + ";\n";
    } else if (inst.op == ir::IrOpcode::Return) {
        if (inst.src1.kind != ir::IrOperandKind::None) {
            out += "    return " + format_operand(func, inst.src1) + ";\n";
        } else {
            out += "    return;\n";
        }
    }
}

void C99CodeGen::generate_function(const ir::IrFunction& func, std::string& out) {
    out += map_type(func.return_type) + " " + std::string(interner_.GetString(func.name_id)) + "(";

    if (func.parameters.empty()) {
        out += "void";
    } else {
        for (size_t i = 0; i < func.parameters.size(); ++i) {
            if (i > 0) {
                out += ", ";
            }
            out += map_type(func.parameters[i].type) + " "
                + std::string(interner_.GetString(func.parameters[i].name_id));
        }
    }

    out += ") {\n";

    for (const auto& inst : func.instructions) {
        generate_instruction(func, inst, out);
    }

    out += "}\n\n";
}

std::string C99CodeGen::generate(const ir::IrModule& module) {
    std::string out;
    out += "#include <stdint.h>\n";
    out += "#include <stdbool.h>\n\n";

    for (const auto& func : module.functions) {
        generate_function(func, out);
    }

    return out;
}

}  // namespace axiom