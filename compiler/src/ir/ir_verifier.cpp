#include "axiom/ir/ir_verifier.h"

#include <format>

namespace axiom::ir {

namespace {

/// オペランド単体の妥当性（レジスタ境界・型一致）を検証する内部ヘルパー
bool verify_operand(
    const IrFunction& func,
    const IrOperand& op,
    std::string_view role_name,
    std::vector<Diagnostic>& errors,
    const StringInterner* interner
) {
    if (op.kind == IrOperandKind::None) {
        report_error(
            errors, func.name_id,
            std::format("Missing {} operand", role_name),
            interner
        );
        return false;
    }

    if (op.type == IrType::Unknown || op.type == IrType::Void) {
        report_error(
            errors, func.name_id,
            std::format("{} operand has invalid or unresolved type", role_name),
            interner
        );
        return false;
    }

    if (op.kind == IrOperandKind::Variable || op.kind == IrOperandKind::Temporary) {
        if (op.reg >= func.reg_types.size()) {
            report_error(
                errors, func.name_id,
                std::format("{} register %{} is out of bounds (total registers: {})",
                            role_name, op.reg, func.reg_types.size()),
                interner
            );
            return false;
        }
        if (op.type != func.reg_types[op.reg]) {
            report_error(
                errors, func.name_id,
                std::format("{} register %{} type mismatch with register table", role_name, op.reg),
                interner
            );
            return false;
        }
    }

    return true;
}

} // namespace

std::vector<Diagnostic> verify_function(const IrFunction& func, const StringInterner* interner) {
    std::vector<Diagnostic> errors;

    if (func.reg_types.size() != func.reg_names.size()) {
        report_error(
            errors, func.name_id,
            "Register table size mismatch between reg_types and reg_names",
            interner
        );
        return errors;
    }

    std::vector<uint8_t> label_defined(func.label_names.size(), 0);
    for (const auto& inst : func.instructions) {
        if (inst.op == IrOpcode::Label && inst.label < func.label_names.size()) {
            if (label_defined[inst.label] != 0) {
                report_error(
                    errors, func.name_id,
                    std::format("Duplicate definition of label ${}", inst.label),
                    interner
                );
            }
            label_defined[inst.label] = 1;
        }
    }

    for (size_t i = 0; i < func.instructions.size(); ++i) {
        const auto& inst = func.instructions[i];

        if (inst.op == IrOpcode::Alloca) {
            if (inst.dest.kind != IrOperandKind::Variable) {
                report_error(
                    errors, func.name_id,
                    "Alloca instruction requires a variable operand as destination",
                    interner
                );
            } else if (inst.dest.reg >= func.reg_types.size()) {
                report_error(
                    errors, func.name_id,
                    std::format("Destination register %{} is out of bounds (total registers: {})",
                                inst.dest.reg, func.reg_types.size()),
                    interner
                );
            } else if (inst.dest.type == IrType::Unknown || inst.dest.type != func.reg_types[inst.dest.reg]) {
                report_error(
                    errors, func.name_id,
                    std::format("Destination register %{} has an unresolved or mismatched type", inst.dest.reg),
                    interner
                );
            }
        } else if (inst.op == IrOpcode::Assign) {
            if (inst.dest.kind != IrOperandKind::Variable && inst.dest.kind != IrOperandKind::Temporary) {
                report_error(
                    errors, func.name_id,
                    "Assign instruction requires a register operand as destination",
                    interner
                );
            } else {
                bool dest_ok = verify_operand(func, inst.dest, "Destination", errors, interner);
                bool src1_ok = verify_operand(func, inst.src1, "Source", errors, interner);
                if (dest_ok && src1_ok && inst.dest.type != inst.src1.type) {
                    report_error(
                        errors, func.name_id,
                        "Assign operand type mismatch between destination and source",
                        interner
                    );
                }
            }
        } else if (inst.op == IrOpcode::Add || inst.op == IrOpcode::Sub ||
                   inst.op == IrOpcode::Mul || inst.op == IrOpcode::Div) {
            if (inst.dest.kind != IrOperandKind::Variable && inst.dest.kind != IrOperandKind::Temporary) {
                report_error(
                    errors, func.name_id,
                    "Binary arithmetic instruction requires a register operand as destination",
                    interner
                );
            } else {
                bool dest_ok = verify_operand(func, inst.dest, "Destination", errors, interner);
                bool src1_ok = verify_operand(func, inst.src1, "LHS", errors, interner);
                bool src2_ok = verify_operand(func, inst.src2, "RHS", errors, interner);
                if (dest_ok && src1_ok && src2_ok &&
                    (inst.dest.type != inst.src1.type || inst.dest.type != inst.src2.type)) {
                    report_error(
                        errors, func.name_id,
                        "Binary arithmetic operand type mismatch",
                        interner
                    );
                }
            }
        } else if (inst.op == IrOpcode::CmpEq || inst.op == IrOpcode::CmpNe ||
                   inst.op == IrOpcode::CmpLt || inst.op == IrOpcode::CmpLe ||
                   inst.op == IrOpcode::CmpGt || inst.op == IrOpcode::CmpGe) {
            if (inst.dest.kind != IrOperandKind::Variable && inst.dest.kind != IrOperandKind::Temporary) {
                report_error(
                    errors, func.name_id,
                    "Comparison instruction requires a register operand as destination",
                    interner
                );
            } else {
                bool dest_ok = verify_operand(func, inst.dest, "Destination", errors, interner);
                bool src1_ok = verify_operand(func, inst.src1, "LHS", errors, interner);
                bool src2_ok = verify_operand(func, inst.src2, "RHS", errors, interner);
                if (dest_ok && inst.dest.type != IrType::Bool) {
                    report_error(
                        errors, func.name_id,
                        "Comparison instruction destination must have bool type",
                        interner
                    );
                }
                if (src1_ok && src2_ok && inst.src1.type != inst.src2.type) {
                    report_error(
                        errors, func.name_id,
                        "Comparison instruction LHS and RHS types must match",
                        interner
                    );
                }
            }
        } else if (inst.op == IrOpcode::Return) {
            if (func.return_type == IrType::Void) {
                if (inst.src1.kind != IrOperandKind::None) {
                    report_error(
                        errors, func.name_id,
                        "Void function cannot return a value",
                        interner
                    );
                }
            } else {
                if (inst.src1.kind == IrOperandKind::None) {
                    report_error(
                        errors, func.name_id,
                        "Non-void function must return a value",
                        interner
                    );
                } else if (verify_operand(func, inst.src1, "Return", errors, interner) &&
                           inst.src1.type != func.return_type) {
                    report_error(
                        errors, func.name_id,
                        "Return value type does not match function return type",
                        interner
                    );
                }
            }
        } else if (inst.op == IrOpcode::Label) {
            if (inst.label >= func.label_names.size()) {
                report_error(
                    errors, func.name_id,
                    std::format("Label ID ${} is out of bounds (total labels: {})",
                                inst.label, func.label_names.size()),
                    interner
                );
            }
        } else if (inst.op == IrOpcode::Jump) {
            if (inst.label >= func.label_names.size()) {
                report_error(
                    errors, func.name_id,
                    std::format("Jump target label ${} is out of bounds (total labels: {})",
                                inst.label, func.label_names.size()),
                    interner
                );
            } else if (label_defined[inst.label] == 0) {
                report_error(
                    errors, func.name_id,
                    std::format("Jump target label ${} is never defined", inst.label),
                    interner
                );
            }
        } else if (inst.op == IrOpcode::BranchIfFalse) {
            if (inst.label >= func.label_names.size()) {
                report_error(
                    errors, func.name_id,
                    std::format("Branch target label ${} is out of bounds (total labels: {})",
                                inst.label, func.label_names.size()),
                    interner
                );
            } else if (label_defined[inst.label] == 0) {
                report_error(
                    errors, func.name_id,
                    std::format("Branch target label ${} is never defined", inst.label),
                    interner
                );
            }
            if (verify_operand(func, inst.src1, "Branch condition", errors, interner) &&
                inst.src1.type != IrType::Bool) {
                report_error(
                    errors, func.name_id,
                    "Branch condition operand must have bool type",
                    interner
                );
            }
        }
    }

    return errors;
}

std::vector<Diagnostic> verify_module(const IrModule& module, const StringInterner* interner) {
    std::vector<Diagnostic> all_errors;
    for (const auto& func : module.functions) {
        auto func_errors = verify_function(func, interner);
        all_errors.insert(all_errors.end(), func_errors.begin(), func_errors.end());
    }
    return all_errors;
}

} // namespace axiom::ir
