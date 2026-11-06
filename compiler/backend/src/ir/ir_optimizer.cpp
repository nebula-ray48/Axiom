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

/// オペランドがレジスタ（Variable / Temporary）であれば、known_values に記録されている値で置き換える
[[nodiscard]] bool try_replace_operand(axiom::ir::IrOperand&                    op,
                                       const std::vector<axiom::ir::IrOperand>& known_values) noexcept {
    using namespace axiom::ir;
    if ((op.kind == IrOperandKind::Variable || op.kind == IrOperandKind::Temporary) && op.reg < known_values.size()) {
        const IrOperand& replacement = known_values[op.reg];
        if (replacement.kind != IrOperandKind::None) {
            op = replacement;
            return true;
        }
    }
    return false;
}

/// 指定したレジスタが書き換えられた際、そのレジスタ自身およびそのレジスタを参照していた追跡情報を無効化する
void invalidate_reg(axiom::ir::RegId target_reg, std::vector<axiom::ir::IrOperand>& known_values) noexcept {
    using namespace axiom::ir;
    if (target_reg < known_values.size()) {
        known_values[target_reg] = IrOperand::MakeNone();
    }
    for (size_t i = 0; i < known_values.size(); ++i) {
        if ((known_values[i].kind == IrOperandKind::Variable || known_values[i].kind == IrOperandKind::Temporary) &&
            known_values[i].reg == target_reg) {
            known_values[i] = IrOperand::MakeNone();
        }
    }
}

/// ラベル等の合流地点で、全レジスタの追跡情報をリセットする
void clear_all_values(std::vector<axiom::ir::IrOperand>& known_values) noexcept {
    for (size_t i = 0; i < known_values.size(); ++i) {
        known_values[i] = axiom::ir::IrOperand::MakeNone();
    }
}

/// オペランドがレジスタ（Variable / Temporary）であれば、使用済みフラグ（is_used[reg] = 1）を立てる
void mark_used(const axiom::ir::IrOperand& op, std::vector<uint8_t>& is_used) noexcept {
    using namespace axiom::ir;
    if ((op.kind == IrOperandKind::Variable || op.kind == IrOperandKind::Temporary) && op.reg < is_used.size()) {
        is_used[op.reg] = 1;
    }
}

/// 結果（dest）が誰にも読まれていない場合に、安全に削除（Nop化）してよい命令かを判定する
/// （副作用のある Call や分岐命令、ゼロ除算トラップの可能性がある Div は消さない）
[[nodiscard]] bool can_eliminate_if_unused(const axiom::ir::IrInstruction& inst) noexcept {
    using namespace axiom::ir;
    switch (inst.op) {
        case IrOpcode::Alloca:
        case IrOpcode::Assign:
        case IrOpcode::Add:
        case IrOpcode::Sub:
        case IrOpcode::Mul:
        case IrOpcode::CmpEq:
        case IrOpcode::CmpNe:
        case IrOpcode::CmpLt:
        case IrOpcode::CmpLe:
        case IrOpcode::CmpGt:
        case IrOpcode::CmpGe: return true;
        case IrOpcode::Div:
            // 除数が非ゼロ定数かつ INT32_MIN / -1 でない場合のみトラップしないので削除可能
            if (inst.src2.kind == IrOperandKind::Constant && inst.src2.constant_int != 0) {
                if (inst.src1.kind == IrOperandKind::Constant &&
                    static_cast<int32_t>(inst.src1.constant_int) == std::numeric_limits<int32_t>::min() &&
                    static_cast<int32_t>(inst.src2.constant_int) == -1) {
                    return false;
                }
                if (static_cast<int32_t>(inst.src2.constant_int) != -1) {
                    return true;
                }
            }
            return false;
        default: return false;
    }
}

/// 命令を Nop に書き換えて無効化する
void mark_as_nop(axiom::ir::IrInstruction& inst) noexcept {
    inst = axiom::ir::IrInstruction{.op = axiom::ir::IrOpcode::Nop};
}

}  // namespace

namespace axiom::ir {

void IrOptimizer::eliminate_nops(IrFunction& func) {
    // インプレースで Nop を詰める線形走査
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

        if (options.copy_propagation) {
            changed_in_iter |= run_copy_propagation(func);
        }
        if (options.constant_folding) {
            changed_in_iter |= run_constant_folding(func);
        }
        if (options.dead_code_elimination) {
            changed_in_iter |= run_dead_code_elimination(func);
        }

        if (!changed_in_iter) {
            break;
        }
        any_changed = true;
    }

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

    // instructions を走査し、定数同士の演算（Add, Sub, Mul, Div, Cmp*）や
    // 代数簡約（x + 0, x * 1 等）を検出して畳み込む。
    for (size_t i = 0; i < func.instructions.size(); ++i) {
        IrInstruction& inst = func.instructions[i];
        if (inst.op == IrOpcode::Nop) {
            continue;
        }

        if (inst.op == IrOpcode::BranchIfFalse && inst.src1.kind == IrOperandKind::Constant) {
            if (inst.src1.constant_int == 0) {
                inst.op   = IrOpcode::Jump;
                inst.src1 = IrOperand::MakeNone();
                changed   = true;
                continue;
            }
            if (inst.src1.constant_int != 0) {
                inst.op    = IrOpcode::Nop;
                inst.src1  = IrOperand::MakeNone();
                inst.label = kInvalidLabelId;
                changed    = true;
                continue;
            }
        }

        int64_t c1       = inst.src1.constant_int;
        int64_t c2       = inst.src2.constant_int;
        bool    foldable = false;
        int64_t result   = 0;

        if (inst.src1.kind == IrOperandKind::Constant && inst.src2.kind == IrOperandKind::Constant) {
            switch (inst.op) {
                case IrOpcode::Add:
                    result   = static_cast<int32_t>(static_cast<uint32_t>(c1) + static_cast<uint32_t>(c2));
                    foldable = true;
                    break;
                case IrOpcode::Sub:
                    result   = static_cast<int32_t>(static_cast<uint32_t>(c1) - static_cast<uint32_t>(c2));
                    foldable = true;
                    break;
                case IrOpcode::Mul:
                    result   = static_cast<int32_t>(static_cast<uint32_t>(c1) * static_cast<uint32_t>(c2));
                    foldable = true;
                    break;
                case IrOpcode::Div:
                    // ゼロ除算および INT32_MIN / -1 は実行時トラップ対象なので畳み込まずに残す
                    if (c2 != 0 && !(static_cast<int32_t>(c1) == std::numeric_limits<int32_t>::min() &&
                                     static_cast<int32_t>(c2) == -1)) {
                        result   = static_cast<int32_t>(c1) / static_cast<int32_t>(c2);
                        foldable = true;
                    }
                    break;
                case IrOpcode::CmpEq:
                    result   = (c1 == c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpNe:
                    result   = (c1 != c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpLt:
                    result   = (c1 < c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpLe:
                    result   = (c1 <= c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpGt:
                    result   = (c1 > c2) ? 1 : 0;
                    foldable = true;
                    break;
                case IrOpcode::CmpGe:
                    result   = (c1 >= c2) ? 1 : 0;
                    foldable = true;
                    break;
                default: break;
            }
        }
        if (foldable) {
            inst.op   = IrOpcode::Assign;
            inst.src1 = IrOperand::MakeConst(result, inst.dest.type);
            inst.src2 = IrOperand::MakeNone();
            changed   = true;
        }

        switch (inst.op) {
            case IrOpcode::Add:
                if (is_const(inst.src1, 0)) {
                    // 0 + x  ==>  dest = assign x
                    inst.op   = IrOpcode::Assign;
                    inst.src1 = inst.src2;
                    inst.src2 = IrOperand::MakeNone();
                    changed   = true;
                } else if (is_const(inst.src2, 0)) {
                    // x + 0  ==>  dest = assign x （src1 は x のまま）
                    inst.op   = IrOpcode::Assign;
                    inst.src2 = IrOperand::MakeNone();
                    changed   = true;
                }
                break;

            case IrOpcode::Sub:
                if (is_const(inst.src2, 0)) {
                    inst.op   = IrOpcode::Assign;
                    inst.src2 = IrOperand::MakeNone();
                    changed   = true;
                }
                break;

            case IrOpcode::Mul:
                if (is_const(inst.src1, 0) || is_const(inst.src2, 0)) {
                    inst.src1 = IrOperand::MakeConst(0, inst.dest.type);
                    inst.src2 = IrOperand::MakeNone();
                    inst.op   = IrOpcode::Assign;
                    changed   = true;
                } else if (is_const(inst.src1, 1)) {
                    inst.src1 = inst.src2;
                    inst.src2 = IrOperand::MakeNone();
                    inst.op   = IrOpcode::Assign;
                    changed   = true;
                } else if (is_const(inst.src2, 1)) {
                    inst.src2 = IrOperand::MakeNone();
                    inst.op   = IrOpcode::Assign;
                    changed   = true;
                }
                break;

            case IrOpcode::Div:
                if (is_const(inst.src2, 1)) {
                    inst.src2 = IrOperand::MakeNone();
                    inst.op   = IrOpcode::Assign;
                    changed   = true;
                }
                break;

            default: break;
        }
    }

    return changed;
}

bool IrOptimizer::run_copy_propagation(IrFunction& func) {
    bool changed = false;

    // 各レジスタ（RegId）が現在保持している既知のオペランド（Constant または別レジスタ）を追跡する配列
    std::vector<IrOperand> known_values(func.reg_types.size(), IrOperand::MakeNone());

    for (size_t i = 0; i < func.instructions.size(); ++i) {
        IrInstruction& inst = func.instructions[i];
        if (inst.op == IrOpcode::Nop) {
            continue;
        }

        // 1. Label（合流点）に到達したら全追跡情報をリセット
        if (inst.op == IrOpcode::Label) {
            clear_all_values(known_values);
            continue;
        }

        // 2. 右辺のオペランド（src1, src2, Call引数）を既知の値に置換
        if (try_replace_operand(inst.src1, known_values)) {
            changed = true;
        }
        if (try_replace_operand(inst.src2, known_values)) {
            changed = true;
        }
        if (inst.op == IrOpcode::Call) {
            for (uint32_t a = 0; a < inst.arg_count; ++a) {
                if (try_replace_operand(func.call_args[inst.arg_start + a], known_values)) {
                    changed = true;
                }
            }
        }

        // 3. 左辺（dest）の古い追跡情報を無効化し、Assign 命令なら新しい値を記録
        if (inst.dest.kind == IrOperandKind::Variable || inst.dest.kind == IrOperandKind::Temporary) {
            invalidate_reg(inst.dest.reg, known_values);
            if (inst.op == IrOpcode::Assign) {
                if (!(inst.src1.kind == inst.dest.kind && inst.src1.reg == inst.dest.reg)) {
                    known_values[inst.dest.reg] = inst.src1;
                }
            }
        }
    }

    return changed;
}

bool IrOptimizer::run_dead_code_elimination(IrFunction& func) {
    bool                 changed = false;
    std::vector<uint8_t> is_used(func.reg_types.size(), 0);

    for (size_t i = 0; i < func.instructions.size(); ++i) {
        const IrInstruction& inst = func.instructions[i];
        if (inst.op == IrOpcode::Nop) {
            continue;  // Nop ならスキップ
        }

        mark_used(inst.src1, is_used);
        mark_used(inst.src2, is_used);

        if (inst.op == IrOpcode::Call) {
            for (uint32_t a = 0; a < inst.arg_count; ++a) {
                mark_used(func.call_args[inst.arg_start + a], is_used);
            }
        }
    }

    for (size_t i = 0; i < func.instructions.size(); ++i) {
        IrInstruction& inst = func.instructions[i];
        if (inst.op == IrOpcode::Nop) {
            continue;  // Nop ならスキップ
        }

        if (can_eliminate_if_unused(inst)) {
            if (inst.dest.kind == IrOperandKind::Variable || inst.dest.kind == IrOperandKind::Temporary) {
                if (is_used[inst.dest.reg] == 0) {
                    mark_as_nop(inst);
                    changed = true;
                }
            }
        }
    }

    return changed;
}

}  // namespace axiom::ir
