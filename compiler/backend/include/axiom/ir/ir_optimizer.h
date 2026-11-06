/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>

#include "axiom/ir/ir.h"

namespace axiom::ir {

struct OptimizerOptions {
    bool     constant_folding{true};
    bool     copy_propagation{true};       // M3: 定数・コピー伝播
    bool     dead_code_elimination{true};  // M4: デッドコード削除
    uint32_t max_iterations{10};           // 固定点ループの最大周回数
};

class IrOptimizer {
public:
    /// 1つの関数に対して最適化パイプラインを実行する
    /// 何らかの命令変更が行われた場合は true を返す
    static bool optimize_function(IrFunction& func, const OptimizerOptions& options = {});

    /// モジュール全体に対して最適化を実行する
    static bool optimize_module(IrModule& module, const OptimizerOptions& options = {});

    /// 【Milestone 2】定数畳み込み & 代数簡約パス
    /// 命令が1つでも変更されたら true を返す
    static bool run_constant_folding(IrFunction& func);

    /// 【Milestone 3】定数伝播 & コピー伝播パス
    /// 命令のオペランドが1つでも置換されたら true を返す
    static bool run_copy_propagation(IrFunction& func);

    /// 【Milestone 4】デッドコード削除パス（未使用レジスタ計算・到達不能コードの消去）
    /// 命令が1つでも Nop 化されたら true を返す
    static bool run_dead_code_elimination(IrFunction& func);

    /// 【Milestone 1】Nop 命令を消去して命令列を詰める（線形走査）
    static void eliminate_nops(IrFunction& func);
};

}  // namespace axiom::ir
