/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include "axiom/target/arm64.h"

#include <cassert>

namespace axiom::arm64 {

namespace {

// 3オペランド レジスタ算術命令の共通ヘルパー (add, sub, mul, sdiv)
uint32_t encode_dp_3reg(uint32_t sf, uint32_t base_op, uint8_t rd, uint8_t rn, uint8_t rm) {
    assert(rd < 32 && rn < 32 && rm < 32);
    return (sf << 31) | base_op | (static_cast<uint32_t>(rm & 0x1F) << 16) | (static_cast<uint32_t>(rn & 0x1F) << 5) |
           static_cast<uint32_t>(rd & 0x1F);
}

// 12-bit 即値算術命令の共通ヘルパー (add_imm, sub_imm)
uint32_t encode_dp_imm12(uint32_t sf, uint32_t base_op, uint8_t rd, uint8_t rn, uint16_t imm12) {
    assert(rd < 32 && rn < 32);
    assert(imm12 < 4096);
    return (sf << 31) | base_op | (static_cast<uint32_t>(imm12 & 0xFFF) << 10) |
           (static_cast<uint32_t>(rn & 0x1F) << 5) | static_cast<uint32_t>(rd & 0x1F);
}

// 16-bit 即値移動命令の共通ヘルパー (movz, movk, movn)
uint32_t encode_mov_wide(uint32_t sf, uint32_t base_op, uint8_t rd, uint16_t imm16, uint8_t shift) {
    assert(rd < 32);
    uint32_t hw = shift / 16;
    return (sf << 31) | base_op | (hw << 21) | (static_cast<uint32_t>(imm16) << 5) | static_cast<uint32_t>(rd & 0x1F);
}

// 19-bit 符号付きオフセットの範囲検査 (-2^18 <= offset < 2^18)
constexpr bool is_int19(int32_t offset) {
    return offset >= -(1 << 18) && offset < (1 << 18);
}

// 26-bit 符号付きオフセットの範囲検査 (-2^25 <= offset < 2^25)
constexpr bool is_int26(int32_t offset) {
    return offset >= -(1 << 25) && offset < (1 << 25);
}

}  // namespace

// ============================================================================
// 1. 算術命令（レジスタ間: rd = rn op rm）
// ============================================================================
uint32_t encode_add(WReg rd, WReg rn, WReg rm) {
    return encode_dp_3reg(0, 0x0B000000U, rd.id, rn.id, rm.id);
}
uint32_t encode_add(XReg rd, XReg rn, XReg rm) {
    return encode_dp_3reg(1, 0x0B000000U, rd.id, rn.id, rm.id);
}

uint32_t encode_sub(WReg rd, WReg rn, WReg rm) {
    return encode_dp_3reg(0, 0x4B000000U, rd.id, rn.id, rm.id);
}
uint32_t encode_sub(XReg rd, XReg rn, XReg rm) {
    return encode_dp_3reg(1, 0x4B000000U, rd.id, rn.id, rm.id);
}

uint32_t encode_mul(WReg rd, WReg rn, WReg rm) {
    return encode_dp_3reg(0, 0x1B007C00U, rd.id, rn.id, rm.id);
}
uint32_t encode_mul(XReg rd, XReg rn, XReg rm) {
    return encode_dp_3reg(1, 0x1B007C00U, rd.id, rn.id, rm.id);
}

uint32_t encode_sdiv(WReg rd, WReg rn, WReg rm) {
    return encode_dp_3reg(0, 0x1AC00C00U, rd.id, rn.id, rm.id);
}
uint32_t encode_sdiv(XReg rd, XReg rn, XReg rm) {
    return encode_dp_3reg(1, 0x1AC00C00U, rd.id, rn.id, rm.id);
}

// ============================================================================
// 2. 算術命令（12-bit 即値: rd = rn op imm12）
// ============================================================================
uint32_t encode_add_imm(WReg rd, WReg rn, uint16_t imm12) {
    return encode_dp_imm12(0, 0x11000000U, rd.id, rn.id, imm12);
}
uint32_t encode_add_imm(XReg rd, XReg rn, uint16_t imm12) {
    return encode_dp_imm12(1, 0x11000000U, rd.id, rn.id, imm12);
}

uint32_t encode_sub_imm(WReg rd, WReg rn, uint16_t imm12) {
    return encode_dp_imm12(0, 0x51000000U, rd.id, rn.id, imm12);
}
uint32_t encode_sub_imm(XReg rd, XReg rn, uint16_t imm12) {
    return encode_dp_imm12(1, 0x51000000U, rd.id, rn.id, imm12);
}

// ============================================================================
// 3. 定数ロード & レジスタ間コピー
// ============================================================================
uint32_t encode_movz(WReg rd, uint16_t imm16, uint8_t shift) {
    assert(shift == 0 || shift == 16);
    return encode_mov_wide(0, 0x52800000U, rd.id, imm16, shift);
}
uint32_t encode_movz(XReg rd, uint16_t imm16, uint8_t shift) {
    assert(shift == 0 || shift == 16 || shift == 32 || shift == 48);
    return encode_mov_wide(1, 0x52800000U, rd.id, imm16, shift);
}

uint32_t encode_movk(WReg rd, uint16_t imm16, uint8_t shift) {
    assert(shift == 0 || shift == 16);
    return encode_mov_wide(0, 0x72800000U, rd.id, imm16, shift);
}
uint32_t encode_movk(XReg rd, uint16_t imm16, uint8_t shift) {
    assert(shift == 0 || shift == 16 || shift == 32 || shift == 48);
    return encode_mov_wide(1, 0x72800000U, rd.id, imm16, shift);
}

uint32_t encode_movn(WReg rd, uint16_t imm16, uint8_t shift) {
    assert(shift == 0 || shift == 16);
    return encode_mov_wide(0, 0x12800000U, rd.id, imm16, shift);
}
uint32_t encode_movn(XReg rd, uint16_t imm16, uint8_t shift) {
    assert(shift == 0 || shift == 16 || shift == 32 || shift == 48);
    return encode_mov_wide(1, 0x12800000U, rd.id, imm16, shift);
}

uint32_t encode_mov(WReg rd, WReg rm) {
    assert(rd.id < 32 && rm.id < 32);
    return 0x2A0003E0U | (static_cast<uint32_t>(rm.id & 0x1F) << 16) | static_cast<uint32_t>(rd.id & 0x1F);
}
uint32_t encode_mov(XReg rd, XReg rm) {
    assert(rd.id < 32 && rm.id < 32);
    return 0xAA0003E0U | (static_cast<uint32_t>(rm.id & 0x1F) << 16) | static_cast<uint32_t>(rd.id & 0x1F);
}

// ============================================================================
// 4. メモリロード / ストア（スタックアクセス & 関数プロローグ・エピローグ）
// ============================================================================
uint32_t encode_ldr(WReg rt, XReg rn, uint32_t byte_offset) {
    assert(rt.id < 32 && rn.id < 32);
    assert(byte_offset % 4 == 0 && (byte_offset / 4) < 4096);
    uint32_t imm12 = (byte_offset / 4) & 0xFFFU;
    return 0xB9400000U | (imm12 << 10) | (static_cast<uint32_t>(rn.id & 0x1F) << 5) |
           static_cast<uint32_t>(rt.id & 0x1F);
}
uint32_t encode_ldr(XReg rt, XReg rn, uint32_t byte_offset) {
    assert(rt.id < 32 && rn.id < 32);
    assert(byte_offset % 8 == 0 && (byte_offset / 8) < 4096);
    uint32_t imm12 = (byte_offset / 8) & 0xFFFU;
    return 0xF9400000U | (imm12 << 10) | (static_cast<uint32_t>(rn.id & 0x1F) << 5) |
           static_cast<uint32_t>(rt.id & 0x1F);
}

uint32_t encode_str(WReg rt, XReg rn, uint32_t byte_offset) {
    assert(rt.id < 32 && rn.id < 32);
    assert(byte_offset % 4 == 0 && (byte_offset / 4) < 4096);
    uint32_t imm12 = (byte_offset / 4) & 0xFFFU;
    return 0xB9000000U | (imm12 << 10) | (static_cast<uint32_t>(rn.id & 0x1F) << 5) |
           static_cast<uint32_t>(rt.id & 0x1F);
}
uint32_t encode_str(XReg rt, XReg rn, uint32_t byte_offset) {
    assert(rt.id < 32 && rn.id < 32);
    assert(byte_offset % 8 == 0 && (byte_offset / 8) < 4096);
    uint32_t imm12 = (byte_offset / 8) & 0xFFFU;
    return 0xF9000000U | (imm12 << 10) | (static_cast<uint32_t>(rn.id & 0x1F) << 5) |
           static_cast<uint32_t>(rt.id & 0x1F);
}

uint32_t encode_stp_pre(XReg rt1, XReg rt2, XReg rn, int16_t byte_offset) {
    assert(rt1.id < 32 && rt2.id < 32 && rn.id < 32);
    assert(byte_offset % 8 == 0 && byte_offset >= -512 && byte_offset <= 504);
    uint32_t imm7 = static_cast<uint32_t>(byte_offset / 8) & 0x7FU;
    return 0xA9800000U | (imm7 << 15) | (static_cast<uint32_t>(rt2.id & 0x1F) << 10) |
           (static_cast<uint32_t>(rn.id & 0x1F) << 5) | static_cast<uint32_t>(rt1.id & 0x1F);
}

uint32_t encode_ldp_post(XReg rt1, XReg rt2, XReg rn, int16_t byte_offset) {
    assert(rt1.id < 32 && rt2.id < 32 && rn.id < 32);
    assert(byte_offset % 8 == 0 && byte_offset >= -512 && byte_offset <= 504);
    uint32_t imm7 = static_cast<uint32_t>(byte_offset / 8) & 0x7FU;
    return 0xA8C00000U | (imm7 << 15) | (static_cast<uint32_t>(rt2.id & 0x1F) << 10) |
           (static_cast<uint32_t>(rn.id & 0x1F) << 5) | static_cast<uint32_t>(rt1.id & 0x1F);
}

// ============================================================================
// 5. 比較 & 条件フラグ抽出（cmp / cset）
// ============================================================================
uint32_t encode_cmp(WReg rn, WReg rm) {
    return encode_dp_3reg(0, 0x6B000000U, reg::wzr.id, rn.id, rm.id);
}
uint32_t encode_cmp(XReg rn, XReg rm) {
    return encode_dp_3reg(1, 0x6B000000U, reg::xzr.id, rn.id, rm.id);
}

uint32_t encode_cmp_imm(WReg rn, uint16_t imm12) {
    return encode_dp_imm12(0, 0x71000000U, reg::wzr.id, rn.id, imm12);
}
uint32_t encode_cmp_imm(XReg rn, uint16_t imm12) {
    return encode_dp_imm12(1, 0x71000000U, reg::xzr.id, rn.id, imm12);
}

uint32_t encode_cset(WReg rd, Cond cond) {
    assert(rd.id < 32);
    uint32_t inv_cond = (static_cast<uint32_t>(cond) ^ 1U) & 0xFU;
    return 0x1A9F07E0U | (inv_cond << 12) | static_cast<uint32_t>(rd.id & 0x1F);
}
uint32_t encode_cset(XReg rd, Cond cond) {
    assert(rd.id < 32);
    uint32_t inv_cond = (static_cast<uint32_t>(cond) ^ 1U) & 0xFU;
    return 0x9A9F07E0U | (inv_cond << 12) | static_cast<uint32_t>(rd.id & 0x1F);
}

// ============================================================================
// 6. 分岐・関数呼び出し・リターン
// ============================================================================
uint32_t encode_b(int32_t inst_offset) {
    assert(is_int26(inst_offset));
    return 0x14000000U | (static_cast<uint32_t>(inst_offset) & 0x03FFFFFFU);
}

uint32_t encode_bl(int32_t inst_offset) {
    assert(is_int26(inst_offset));
    return 0x94000000U | (static_cast<uint32_t>(inst_offset) & 0x03FFFFFFU);
}

uint32_t encode_b_cond(Cond cond, int32_t inst_offset) {
    assert(is_int19(inst_offset));
    uint32_t imm19 = static_cast<uint32_t>(inst_offset) & 0x7FFFFU;
    return 0x54000000U | (imm19 << 5) | (static_cast<uint32_t>(cond) & 0xFU);
}

uint32_t encode_cbz(WReg rt, int32_t inst_offset) {
    assert(rt.id < 32 && is_int19(inst_offset));
    uint32_t imm19 = static_cast<uint32_t>(inst_offset) & 0x7FFFFU;
    return 0x34000000U | (imm19 << 5) | static_cast<uint32_t>(rt.id & 0x1F);
}
uint32_t encode_cbz(XReg rt, int32_t inst_offset) {
    assert(rt.id < 32 && is_int19(inst_offset));
    uint32_t imm19 = static_cast<uint32_t>(inst_offset) & 0x7FFFFU;
    return 0xB4000000U | (imm19 << 5) | static_cast<uint32_t>(rt.id & 0x1F);
}

uint32_t encode_cbnz(WReg rt, int32_t inst_offset) {
    assert(rt.id < 32 && is_int19(inst_offset));
    uint32_t imm19 = static_cast<uint32_t>(inst_offset) & 0x7FFFFU;
    return 0x35000000U | (imm19 << 5) | static_cast<uint32_t>(rt.id & 0x1F);
}
uint32_t encode_cbnz(XReg rt, int32_t inst_offset) {
    assert(rt.id < 32 && is_int19(inst_offset));
    uint32_t imm19 = static_cast<uint32_t>(inst_offset) & 0x7FFFFU;
    return 0xB5000000U | (imm19 << 5) | static_cast<uint32_t>(rt.id & 0x1F);
}

uint32_t encode_ret(XReg rn) {
    assert(rn.id < 32);
    return 0xD65F0000U | (static_cast<uint32_t>(rn.id & 0x1F) << 5);
}

uint32_t encode_nop() {
    return 0xD503201FU;
}

}  // namespace axiom::arm64
