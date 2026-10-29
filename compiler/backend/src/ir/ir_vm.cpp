/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include "axiom/ir/ir_vm.h"

#include <cassert>
#include <limits>

namespace axiom::ir {

IrVm::VmResult IrVm::execute(const IrFunction& func, std::span<const int64_t> args) {
    // 1. レジスタスロットの準備
    std::vector<int64_t> registers(func.reg_types.size(), 0);

    // 引数のロード
    for (size_t i = 0; i < func.parameters.size() && i < args.size(); ++i) {
        registers[func.parameters[i].reg] = args[i];
    }

    // 2. ラベル位置の事前解決 (O(N) で 1 回だけ走査)
    std::vector<size_t> label_to_pc(func.label_names.size(), 0);
    for (size_t i = 0; i < func.instructions.size(); ++i) {
        if (func.instructions[i].op == IrOpcode::Label) {
            if (func.instructions[i].label < label_to_pc.size()) {
                label_to_pc[func.instructions[i].label] = i;
            }
        }
    }

    // 3. 【道具】オペランドから数値を取り出す
    auto get_value = [&](const IrOperand& op) -> int64_t {
        if (op.kind == IrOperandKind::Constant) {
            return op.constant_int;
        }
        if (op.reg < registers.size()) {
            return registers[op.reg];
        }
        return 0;
    };

    // 4. 【ディスパッチループ】
    size_t pc = 0;
    while (pc < func.instructions.size()) {
        const auto& inst = func.instructions[pc];

        switch (inst.op) {
            case IrOpcode::Alloca:
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = 0;
                }
                break;

            case IrOpcode::Assign:
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = get_value(inst.src1);
                }
                break;

            case IrOpcode::Add: {
                int64_t a   = get_value(inst.src1);
                int64_t b   = get_value(inst.src2);
                auto    sum = static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = sum;
                }
                break;
            }

            case IrOpcode::Sub: {
                int64_t a    = get_value(inst.src1);
                int64_t b    = get_value(inst.src2);
                auto    diff = static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = diff;
                }
                break;
            }

            case IrOpcode::Mul: {
                int64_t a    = get_value(inst.src1);
                int64_t b    = get_value(inst.src2);
                auto    prod = static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = prod;
                }
                break;
            }

            case IrOpcode::Div: {
                int64_t a = get_value(inst.src1);
                int64_t b = get_value(inst.src2);
                if (b == 0) {
                    return VmResult{.value = 0, .trap = TrapCode::IntegerDivideByZero};
                }
                if (static_cast<int32_t>(a) == std::numeric_limits<int32_t>::min() && static_cast<int32_t>(b) == -1) {
                    return VmResult{.value = 0, .trap = TrapCode::IntegerOverflow};
                }
                auto quot = static_cast<int32_t>(a) / static_cast<int32_t>(b);
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = quot;
                }
                break;
            }

            case IrOpcode::CmpEq:
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = (get_value(inst.src1) == get_value(inst.src2)) ? 1 : 0;
                }
                break;

            case IrOpcode::CmpNe:
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = (get_value(inst.src1) != get_value(inst.src2)) ? 1 : 0;
                }
                break;

            case IrOpcode::CmpLt:
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = (get_value(inst.src1) < get_value(inst.src2)) ? 1 : 0;
                }
                break;

            case IrOpcode::CmpLe:
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = (get_value(inst.src1) <= get_value(inst.src2)) ? 1 : 0;
                }
                break;

            case IrOpcode::CmpGt:
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = (get_value(inst.src1) > get_value(inst.src2)) ? 1 : 0;
                }
                break;

            case IrOpcode::CmpGe:
                if (inst.dest.reg < registers.size()) {
                    registers[inst.dest.reg] = (get_value(inst.src1) >= get_value(inst.src2)) ? 1 : 0;
                }
                break;

            case IrOpcode::Label:
                // ラベル自体は何もしない
                break;

            case IrOpcode::Jump:
                if (inst.label < label_to_pc.size()) {
                    pc = label_to_pc[inst.label];
                    continue;
                }
                return VmResult{.value = 0, .trap = TrapCode::InvalidInstruction};

            case IrOpcode::BranchIfFalse: {
                int64_t cond = get_value(inst.src1);
                if (cond == 0) {
                    if (inst.label < label_to_pc.size()) {
                        pc = label_to_pc[inst.label];
                        continue;
                    }
                    return VmResult{.value = 0, .trap = TrapCode::InvalidInstruction};
                }
                break;
            }

            case IrOpcode::Return: {
                int64_t ret_val = (inst.src1.kind != IrOperandKind::None) ? get_value(inst.src1) : 0;
                return VmResult{.value = ret_val, .trap = TrapCode::None};
            }

            default: return VmResult{.value = 0, .trap = TrapCode::InvalidInstruction};
        }

        pc++;
    }

    return VmResult{.value = 0, .trap = TrapCode::None};
}

}  // namespace axiom::ir
