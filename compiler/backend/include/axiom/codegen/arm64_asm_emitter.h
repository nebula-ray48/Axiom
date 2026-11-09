/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "axiom/base/compiler.h"
#include "axiom/ir/ir.h"
#include "axiom/jit/register_allocator.h"
#include "axiom/target/arm64.h"

namespace axiom::codegen {

enum class TargetOs : uint8_t {
    MacOS,  // Mach-O: symbol prefix '_'
    Linux,  // ELF: no symbol prefix, .type func, @function
};

#if defined(__APPLE__)
inline constexpr TargetOs kDefaultTargetOs = TargetOs::MacOS;
#else
inline constexpr TargetOs kDefaultTargetOs = TargetOs::Linux;
#endif

struct AsmEmitterOptions {
    TargetOs target_os     = kDefaultTargetOs;
    bool     emit_comments = true;
};

class Arm64AsmEmitter {
public:
    explicit Arm64AsmEmitter(const StringInterner& interner, AsmEmitterOptions options = {});

    /// IrModule 全体を ARM64 アセンブリ文字列 (.s) に変換する
    [[nodiscard]] std::string emit_module(const ir::IrModule& module);

    /// 単一の IrFunction をアセンブリ文字列に変換する
    [[nodiscard]] std::string emit_function(const ir::IrFunction& func);

private:
    const StringInterner& interner_;
    AsmEmitterOptions     options_;

    // 関数ごとの変換コンテキスト
    jit::RegAllocResult alloc_;
    uint16_t            frame_size_ = 0;
    std::string_view    current_func_name_;

    /// シンボル名（OS差異: macOSは "_" 付加）を整形
    [[nodiscard]] std::string format_symbol(std::string_view raw_name) const;

    /// 関数内ローカルラベル名を整形 (例: ".L_main_0")
    [[nodiscard]] std::string format_local_label(ir::LabelId label) const;

    /// 仮想レジスタのスタックオフセット (reg_id * 4)
    [[nodiscard]] static constexpr uint32_t stack_offset(ir::RegId reg) noexcept { return reg * 4; }

    /// 行を出力バッファに追記するヘルパー（インデント付き）
    static void emit_line(std::string& out, std::string_view line);

    /// ラベルを出力バッファに追記するヘルパー（インデントなし）
    static void emit_label(std::string& out, std::string_view label);

    /// 関数プロローグを出力
    void emit_prologue(const ir::IrFunction& func, std::string& out);

    /// 関数エピローグを出力
    void emit_epilogue(std::string& out);

    /// 単一命令のディスパッチ (M2 & M3 で実装)
    void emit_instruction(const ir::IrFunction& func, const ir::IrInstruction& inst, size_t inst_idx, std::string& out);

    /// オペランド解決ヘルパー
    arm64::WReg resolve_src(const ir::IrOperand& op, arm64::WReg scratch, std::string& out);
    [[nodiscard]] arm64::WReg resolve_dst(ir::RegId dst_reg, arm64::WReg scratch) const noexcept;
    void emit_store_reg(arm64::WReg src, ir::RegId dst_reg, std::string& out);
};

}  // namespace axiom::codegen

