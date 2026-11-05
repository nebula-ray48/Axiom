/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include "axiom/ir/ir_optimizer.h"

namespace {
[[nodiscard]] bool is_const(const axiom::ir::IrOperand& op, int64_t val) noexcept {
    return op.kind == axiom::ir::IrOperandKind::Constant && op.constant_int == val;
}
}

namespace axiom::ir {

void IrOptimizer::eliminate_nops(IrFunction& func) {
    // インプレースで Nop を詰める線形走査（アロケーションなし）
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < func.instructions.size(); ++read_idx) {
        if (func.instructions[read_idx].op != IrOpcode::Nop) {
            if (write_idx != read_idx) {
                func.instructions[write_idx] = func.instructions[read_idx];
            }
            ++write_idx;
        }
    }
    func.instructions.resize(write_idx);
}

bool IrOptimizer::optimize_function(IrFunction& func, const OptimizerOptions& options) {
    bool any_changed = false;

    for (uint32_t iter = 0; iter < options.max_iterations; ++iter) {
        bool changed_in_iter = false;

        if (options.constant_folding) {
            changed_in_iter |= run_constant_folding(func);
        }

        if (!changed_in_iter) {
            break;
        }
        any_changed = true;
    }

    // パス完了後に不要になった Nop を詰める
    eliminate_nops(func);

    return any_changed;
}

bool IrOptimizer::optimize_module(IrModule& module, const OptimizerOptions& options) {
    bool any_changed = false;
    for (auto& func : module.functions) {
        any_changed |= optimize_function(func, options);
    }
    return any_changed;
}

bool IrOptimizer::run_constant_folding(IrFunction& func) {
    bool changed = false;

    // 【開発者が自力実装する箇所】
    // instructions を走査し、定数同士の演算（Add, Sub, Mul, Div, Cmp*）や
    // 代数簡約（x + 0, x * 1 等）を検出して畳み込む。
    for (size_t i = 0; i < func.instructions.size(); ++i) {
        IrInstruction& inst = func.instructions[i];
        if (inst.op == IrOpcode::Nop) {
            continue;
        }

        if (inst.op == IrOpcode::BranchIfFalse && inst.src1.kind == IrOperandKind::Constant) {
            if (inst.src1.constant_int == 0) {
                inst.op = IrOpcode::Jump;
                inst.src1 = IrOperand::MakeNone();
                changed = true;
                continue;
            }
            if (inst.src1.constant_int != 0) {
                inst.op = IrOpcode::Nop;
                inst.src1 = IrOperand::MakeNone();
                inst.label = kInvalidLabelId;
                changed = true;
                continue;
            }
        }

        int64_t c1 = inst.src1.constant_int;
        int64_t c2 = inst.src2.constant_int;
        bool foldable = false;
        int64_t result = 0;

        if (inst.src1.kind == IrOperandKind::Constant && inst.src2.kind == IrOperandKind::Constant) {
            switch (inst.op) {
                case IrOpcode::Add:
                    result = static_cast<int32_t>(static_cast<uint32_t>(c1) + static_cast<uint32_t>(c2));
                    foldable = true;
                    break;
                case IrOpcode::Sub:
                    result = static_cast<int32_t>(static_cast<uint32_t>(c1) - static_cast<uint32_t>(c2));
                    foldable = true;
                    break;
                case IrOpcode::Mul:
                    result = static_cast<int32_t>(static_cast<uint32_t>(c1) * static_cast<uint32_t>(c2));
                    foldable = true;
                    break;
                case IrOpcode::Div:
                    // ゼロ除算および INT32_MIN / -1 は実行時トラップ対象なので畳み込まずに残す
                    if (c2 != 0 && !(static_cast<int32_t>(c1) == std::numeric_limits<int32_t>::min() && static_cast<int32_t>(c2) == -1)) {
                        result = static_cast<int32_t>(c1) / static_cast<int32_t>(c2);
                        foldable = true;
                    }
                    break;
                case IrOpcode::CmpEq:
                    result = (c1 == c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpNe:
                    result = (c1 != c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpLt:
                    result = (c1 < c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpLe:
                    result = (c1 <= c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpGt:
                    result = (c1 > c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpGe:
                    result = (c1 >= c2) ? 1 : 0;
                    foldable = true;
                    break;
                default:
                    break;
            }
        }
        if (foldable) {
            inst.op = IrOpcode::Assign;
            inst.src1 = IrOperand::MakeConst(result, inst.dest.type);
            inst.src2 = IrOperand::MakeNone();
            changed = true;
        }

        switch (inst.op) {
            case IrOpcode::Add:
                if (is_const(inst.src1, 0)) {
                    // 0 + x  ==>  dest = assign x
                    inst.op = IrOpcode::Assign;
                    inst.src1 = inst.src2;
                    inst.src2 = IrOperand::MakeNone();
                    changed = true;
                } else if (is_const(inst.src2, 0)) {
                    // x + 0  ==>  dest = assign x （src1 は x のまま）
                    inst.op = IrOpcode::Assign;
                    inst.src2 = IrOperand::MakeNone();
                    changed = true;
                }
                break;

            case IrOpcode::Sub:
                if (is_const(inst.src2, 0)) {
                    inst.op = IrOpcode::Assign;
                    inst.src2 = IrOperand::MakeNone();
                    changed = true;
                }
                break;

            case IrOpcode::Mul:
                if (is_const(inst.src1, 0) || is_const(inst.src2, 0)) {
                    inst.src1 = IrOperand::MakeConst(0, inst.dest.type);
                    inst.src2 = IrOperand::MakeNone();
                    inst.op = IrOpcode::Assign;
                    changed = true;
                } else if (is_const(inst.src1, 1)) {
                    inst.src1 = inst.src2;
                    inst.src2 = IrOperand::MakeNone();
                    inst.op = IrOpcode::Assign;
                    changed = true;
                } else if (is_const(inst.src2, 1)) {
                    inst.src2 = IrOperand::MakeNone();
                    inst.op = IrOpcode::Assign;
                    changed = true;
                }
                break;

            case IrOpcode::Div:
                if (is_const(inst.src2, 1)) {
                    inst.src2 = IrOperand::MakeNone();
                    inst.op = IrOpcode::Assign;
                    changed = true;
                }
                break;

            default:
                break;
        }
    }

    return changed;
}

} // namespace axiom::ir

