/*
* Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>
#include <vector>

#include "axiom/ir/ir.h"
#include "axiom/jit/liveness.h"

namespace axiom::jit {

enum class StorageKind : uint8_t {
    Register,  // 物理レジスタ（w19〜w28）
    StackSlot, // スタックフレーム上のスロット（スピルされた）
};

struct StorageLocation {
    StorageKind kind { StorageKind::Register };
    uint32_t    index { 0 }; // 物理レジスタ番号 (19..28) または スロット番号 (0, 1, ...)
};

struct RegAllocResult {
    /// 1. RegId で O(1) に引ける格納先配列（添字 = RegId）
    std::vector<StorageLocation> locations;
    /// 2. スピルによってスタックに何枠必要になったか
    uint32_t num_spill_slots { 0 };
    /// 3. 使った callee-saved レジスタのビットマスク（プロローグで STP/LDP するため）
    uint32_t used_registers_mask { 0 };
};

[[nodiscard]] RegAllocResult allocate_registers(
    const ir::IrFunction& func,
    const LivenessResult& liveness
);

}  // namespace axiom::jit