#include "axiom/jit/register_allocator.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace axiom::jit {

RegAllocResult allocate_registers(const ir::IrFunction& func, const LivenessResult& liveness) {
    RegAllocResult result;
    result.locations.resize(func.reg_types.size());

    std::vector<uint32_t> available_regs;
    for (uint32_t r = 28; r >= 19; --r) {
        available_regs.push_back(r);
    }

    struct ActiveEntry {
        LiveInterval interval;
        uint32_t     phys_reg;
    };
    std::vector<ActiveEntry> active;

    for (const auto& current : liveness.sorted_intervals) {

        std::vector<ActiveEntry> next_active;

        for (const auto& entry : active) {
            if (entry.interval.end < current.start) {
                available_regs.push_back(entry.phys_reg);
            } else {
                next_active.push_back(entry);
            }
        }

        active = std::move(next_active);

        std::ranges::sort(active, [](const ActiveEntry& a, const ActiveEntry& b) {
            return a.interval.end < b.interval.end;
        });

        if (!available_regs.empty()) {
            uint32_t phys_reg = available_regs.back();
            available_regs.pop_back();

            result.locations[current.reg].kind  = StorageKind::Register;
            result.locations[current.reg].index = phys_reg;
            result.used_registers_mask |= (1U << phys_reg);

            ActiveEntry new_entry;
            new_entry.interval = current;
            new_entry.phys_reg = phys_reg;
            active.push_back(new_entry);
        } else {
            auto& spill_candidate = active.back();

            if (spill_candidate.interval.end > current.end) {
                uint32_t slot = result.num_spill_slots++;
                result.locations[spill_candidate.interval.reg].kind  = StorageKind::StackSlot;
                result.locations[spill_candidate.interval.reg].index = slot;

                uint32_t phys_reg = spill_candidate.phys_reg;
                result.locations[current.reg].kind  = StorageKind::Register;
                result.locations[current.reg].index = phys_reg;
                result.used_registers_mask |= (1U << phys_reg);

                spill_candidate.interval = current;
                std::ranges::sort(active, [](const ActiveEntry& a, const ActiveEntry& b) {
                    return a.interval.end < b.interval.end;
                });
            } else {
                uint32_t slot = result.num_spill_slots++;
                result.locations[current.reg].kind  = StorageKind::StackSlot;
                result.locations[current.reg].index = slot;
            }
        }
    }
    return result;
}

}  // namespace axiom::jit
