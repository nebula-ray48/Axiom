#include "axiom/codegen/c99_codegen.h"

#include <string>
#include <string_view>

namespace axiom {

C99CodeGen::C99CodeGen(const StringInterner& interner)
    : interner_(interner) {}

std::string C99CodeGen::map_type(StringID type_id) const {
    if (type_id == kInvalidStringID) {
        return "void";
    }

    std::string_view name = interner_.GetString(type_id);

    if (name == "int8")    return "int8_t";
    if (name == "int16")   return "int16_t";
    if (name == "int32")   return "int32_t";
    if (name == "int64")   return "int64_t";
    if (name == "uint8")   return "uint8_t";
    if (name == "uint16")  return "uint16_t";
    if (name == "uint32")  return "uint32_t";
    if (name == "uint64")  return "uint64_t";
    if (name == "float32") return "float";
    if (name == "float64") return "double";
    if (name == "bool")    return "bool";
    if (name == "void")    return "void";

    return std::string(name);
}

std::string C99CodeGen::format_operand(const ir::IrOperand& op) const {
    if (op.kind == ir::IrOperandKind::Constant) {
        return std::to_string(op.constant_int);
    }
    if (op.kind == ir::IrOperandKind::Variable) {
        return std::string(interner_.GetString(op.id));
    }
    if (op.kind == ir::IrOperandKind::Temporary) {
        std::string_view name = interner_.GetString(op.id);
        // C言語の識別子として有効にするため、先頭の '%' を '_t' に置換（%0 -> _t0）
        if (name.starts_with('%')) {
            return "_t" + std::string(name.substr(1));
        }
        return std::string(name);
    }
    return "";
}

void C99CodeGen::generate_instruction(const ir::IrInstruction& inst, std::string& out) {
    if (inst.op == ir::IrOpcode::Alloca) {
        out += "    " + map_type(inst.dest.type_id) + " " + format_operand(inst.dest) + ";\n";
    } else if (inst.op == ir::IrOpcode::Assign) {
        out += "    " + format_operand(inst.dest) + " = " + format_operand(inst.src1) + ";\n";
    } else if (inst.op == ir::IrOpcode::Add || inst.op == ir::IrOpcode::Sub ||
               inst.op == ir::IrOpcode::Mul || inst.op == ir::IrOpcode::Div) {
        std::string_view op_sym = "+";
        if (inst.op == ir::IrOpcode::Add) op_sym = "+";
        else if (inst.op == ir::IrOpcode::Sub) op_sym = "-";
        else if (inst.op == ir::IrOpcode::Mul) op_sym = "*";
        else if (inst.op == ir::IrOpcode::Div) op_sym = "/";

        out += "    " + map_type(inst.dest.type_id) + " " + format_operand(inst.dest) + " = "
            + format_operand(inst.src1) + " " + std::string(op_sym) + " " + format_operand(inst.src2) + ";\n";
    } else if (inst.op == ir::IrOpcode::Return) {
        if (inst.src1.kind != ir::IrOperandKind::None) {
            out += "    return " + format_operand(inst.src1) + ";\n";
        } else {
            out += "    return;\n";
        }
    }
}

void C99CodeGen::generate_function(const ir::IrFunction& func, std::string& out) {
    out += map_type(func.return_type_id) + " " + std::string(interner_.GetString(func.name_id)) + "(";

    if (func.parameters.empty()) {
        out += "void";
    } else {
        for (size_t i = 0; i < func.parameters.size(); ++i) {
            if (i > 0) {
                out += ", ";
            }
            out += map_type(func.parameters[i].type_id) + " "
                + std::string(interner_.GetString(func.parameters[i].name_id));
        }
    }

    out += ") {\n";

    for (const auto& inst : func.instructions) {
        generate_instruction(inst, out);
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