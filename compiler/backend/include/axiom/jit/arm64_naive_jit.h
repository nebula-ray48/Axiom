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
#include "axiom/target/arm64.h"

namespace axiom::jit {

class Arm64NaiveJit {
public:
    /// IrFunction を ARM64 機械語列へコンパイルする
    [[nodiscard]] static std::vector<uint32_t> compile(const ir::IrFunction& func);

private:
    /// RegId から sp 基準のバイトオフセット (reg_id * 4) を計算
    [[nodiscard]] static constexpr uint32_t stack_offset(ir::RegId reg) noexcept { return reg * 4; }

    /// 1命令追加するヘルパー
    void emit(uint32_t inst) { code_.push_back(inst); }

    /// 関数プロローグ & 仮引数のスタック退避
    void emit_prologue(const ir::IrFunction& func);

    /// 関数エピローグ (sp 復元 + fp/lr ポップ + ret)
    void emit_epilogue();

    /// IrOperand (Constant / Variable / Temporary) を指定レジスタ dst へロード
    void emit_load_operand(arm64::WReg dst, const ir::IrOperand& op);

    /// 指定レジスタ src の値を RegId のスタックスロットへストア
    void emit_store_reg(arm64::WReg src, ir::RegId dst_reg);

    std::vector<uint32_t> code_;
    uint16_t              frame_size_ = 0;
};

}  // namespace axiom::jit
