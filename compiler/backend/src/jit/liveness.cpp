/*
* Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include "axiom/jit/liveness.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace axiom::jit {

LivenessResult compute_liveness(const ir::IrFunction& func) {
    size_t                    num_regs = func.reg_types.size();
    std::vector<LiveInterval> intervals(num_regs);
    for (uint32_t r = 0; r < num_regs; ++r) {
        intervals[r].reg   = r;
        intervals[r].start = UINT32_MAX;
        intervals[r].end   = 0;
    }

    for (const auto& param : func.parameters) {
        intervals[param.reg].start = 0;
        intervals[param.reg].end   = 0;
    }

    auto record_use = [&](const ir::IrOperand& op, uint32_t inst_idx) {
        if (op.kind == ir::IrOperandKind::Variable || op.kind == ir::IrOperandKind::Temporary) {
            intervals[op.reg].start = std::min(intervals[op.reg].start, inst_idx);
            intervals[op.reg].end   = std::max(intervals[op.reg].end, inst_idx);
        }
    };

    for (uint32_t i = 0; i < func.instructions.size(); ++i) {
        const auto& inst = func.instructions[i];
        if (inst.op == ir::IrOpcode::Nop) {
            continue;
        }
        record_use(inst.dest, i);
        record_use(inst.src1, i);
        record_use(inst.src2, i);

        if (inst.op == ir::IrOpcode::Call) {
            uint32_t start_index = inst.arg_start;
            uint32_t count       = inst.arg_count;

            for (uint32_t idx = start_index; idx < start_index + count; ++idx) {
                record_use(func.call_args[idx], i);
            }
        }
    }

    std::vector<uint32_t> label_to_inst(func.label_names.size(), UINT32_MAX);
    for (uint32_t i = 0; i < func.instructions.size(); ++i) {
        if (func.instructions[i].op == ir::IrOpcode::Label) {
            if (func.instructions[i].label < label_to_inst.size()) {
                label_to_inst[func.instructions[i].label] = i;
            }
        }
    }

    for (uint32_t i = 0; i < func.instructions.size(); ++i) {
        const auto& inst = func.instructions[i];
        if (inst.op == ir::IrOpcode::Jump || inst.op == ir::IrOpcode::BranchIfFalse) {
            if (inst.label < label_to_inst.size()) {
                uint32_t target_idx = label_to_inst[inst.label];
                if (target_idx < i) {
                    for (auto& iv : intervals) {
                        if (iv.is_live() && iv.start <= i && iv.end >= target_idx) {
                            iv.end = std::max(iv.end, i);
                        }
                    }
                }
            }
        }
    }

    LivenessResult result;
    for (const auto& iv : intervals) {
        if (iv.is_live()) {
            result.sorted_intervals.push_back(iv);
        }
    }
    std::ranges::sort(result.sorted_intervals, {}, &LiveInterval::start);

    result.intervals_by_reg = std::move(intervals);
    return result;
}

}  // namespace axiom::jit
