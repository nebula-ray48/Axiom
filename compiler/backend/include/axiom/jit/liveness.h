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

namespace axiom::jit {

struct LiveInterval {
    ir::RegId reg   { ir::kInvalidRegId };
    uint32_t  start { 0 }; // 最初に値が定義（書かれた）された命令番号
    uint32_t  end   { 0 }; // 最後に値が参照（読まれた）された命令番号

    [[nodiscard]] bool is_live() const noexcept {
        return start <= end && start != UINT32_MAX;
    }
};

// 生存区間解析の結果
struct LivenessResult {
    /// 1. RegId で一発で引ける配列（添字 = RegId）
    /// デバッグや「この変数はどこで生きてる？」を O(1) で調べるときに使う
    std::vector<LiveInterval> intervals_by_reg;
    /// 2. リニアスキャン用に start 昇順でソートされたリスト
    /// レジスタ割り付け器はこの順にループを回すだけでよい
    std::vector<LiveInterval> sorted_intervals;
};

/// 1つの関数を解析して生存区間を計算する
[[nodiscard]] LivenessResult compute_liveness(const ir::IrFunction& func);

} // namespace axiom::jit