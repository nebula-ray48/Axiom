#pragma once
#include <cstdint>

namespace axiom::arm64 {

struct WReg {
    uint8_t id;
};
struct XReg {
    uint8_t id;
};

namespace reg {
/// 32-bit general purpose registers (w0 - w30) and zero/stack register (31)
inline constexpr WReg w0{0};
inline constexpr WReg w1{1};
inline constexpr WReg w2{2};
inline constexpr WReg w3{3};
inline constexpr WReg w4{4};
inline constexpr WReg w5{5};
inline constexpr WReg w6{6};
inline constexpr WReg w7{7};
inline constexpr WReg w8{8};
inline constexpr WReg w9{9};
inline constexpr WReg w10{10};
inline constexpr WReg w11{11};
inline constexpr WReg w12{12};
inline constexpr WReg w13{13};
inline constexpr WReg w14{14};
inline constexpr WReg w15{15};
inline constexpr WReg w16{16};
inline constexpr WReg w17{17};
inline constexpr WReg w18{18};
inline constexpr WReg w19{19};
inline constexpr WReg w20{20};
inline constexpr WReg w21{21};
inline constexpr WReg w22{22};
inline constexpr WReg w23{23};
inline constexpr WReg w24{24};
inline constexpr WReg w25{25};
inline constexpr WReg w26{26};
inline constexpr WReg w27{27};
inline constexpr WReg w28{28};
inline constexpr WReg w29{29};
inline constexpr WReg w30{30};
inline constexpr WReg wzr{31};  // Zero Register (32-bit)
inline constexpr WReg wsp{31};  // Stack Pointer (32-bit)

/// 64-bit general purpose registers (x0 - x30) and zero/stack register (31)
inline constexpr XReg x0{0};
inline constexpr XReg x1{1};
inline constexpr XReg x2{2};
inline constexpr XReg x3{3};
inline constexpr XReg x4{4};
inline constexpr XReg x5{5};
inline constexpr XReg x6{6};
inline constexpr XReg x7{7};
inline constexpr XReg x8{8};
inline constexpr XReg x9{9};
inline constexpr XReg x10{10};
inline constexpr XReg x11{11};
inline constexpr XReg x12{12};
inline constexpr XReg x13{13};
inline constexpr XReg x14{14};
inline constexpr XReg x15{15};
inline constexpr XReg x16{16};
inline constexpr XReg x17{17};
inline constexpr XReg x18{18};
inline constexpr XReg x19{19};
inline constexpr XReg x20{20};
inline constexpr XReg x21{21};
inline constexpr XReg x22{22};
inline constexpr XReg x23{23};
inline constexpr XReg x24{24};
inline constexpr XReg x25{25};
inline constexpr XReg x26{26};
inline constexpr XReg x27{27};
inline constexpr XReg x28{28};
inline constexpr XReg x29{29};
inline constexpr XReg fp{29};  // Frame Pointer (x29 alias)
inline constexpr XReg x30{30};
inline constexpr XReg lr{30};   // Link Register (x30 alias)
inline constexpr XReg xzr{31};  // Zero Register (64-bit)
inline constexpr XReg sp{31};   // Stack Pointer (64-bit)
}  // namespace reg

enum class Cond : uint8_t {
    EQ = 0x0,  // Equal (Z == 1)
    NE = 0x1,  // Not equal (Z == 0)
    GE = 0xA,  // Signed Greater than or equal (N == V)
    LT = 0xB,  // Signed Less than (N != V)
    GT = 0xC,  // Signed Greater than (Z == 0 && N == V)
    LE = 0xD,  // Signed Less than or equal (Z == 1 || N != V)
    AL = 0xE,  // Always (無条件・常に成立)
};

// ============================================================================
// 1. 算術命令（レジスタ間: rd = rn op rm）
// ============================================================================
uint32_t encode_add(WReg rd, WReg rn, WReg rm);
uint32_t encode_add(XReg rd, XReg rn, XReg rm);

uint32_t encode_sub(WReg rd, WReg rn, WReg rm);
uint32_t encode_sub(XReg rd, XReg rn, XReg rm);

uint32_t encode_mul(WReg rd, WReg rn, WReg rm);
uint32_t encode_mul(XReg rd, XReg rn, XReg rm);

uint32_t encode_sdiv(WReg rd, WReg rn, WReg rm);
uint32_t encode_sdiv(XReg rd, XReg rn, XReg rm);

// ============================================================================
// 2. 算術命令（12-bit 即値: rd = rn op imm12）
// ============================================================================
uint32_t encode_add_imm(WReg rd, WReg rn, uint16_t imm12);
uint32_t encode_add_imm(XReg rd, XReg rn, uint16_t imm12);

uint32_t encode_sub_imm(WReg rd, WReg rn, uint16_t imm12);
uint32_t encode_sub_imm(XReg rd, XReg rn, uint16_t imm12);

// ============================================================================
// 3. 定数ロード & レジスタ間コピー
// ============================================================================
/// 16bit即値ロード（他ビットはゼロクリア）
uint32_t encode_movz(WReg rd, uint16_t imm16, uint8_t shift = 0);
uint32_t encode_movz(XReg rd, uint16_t imm16, uint8_t shift = 0);

/// 16bit即値ロード（他ビットは保持・上書き）
uint32_t encode_movk(WReg rd, uint16_t imm16, uint8_t shift = 0);
uint32_t encode_movk(XReg rd, uint16_t imm16, uint8_t shift = 0);

/// 16bitビット反転即値ロード（負数用: ~((imm16) << shift)）
uint32_t encode_movn(WReg rd, uint16_t imm16, uint8_t shift = 0);
uint32_t encode_movn(XReg rd, uint16_t imm16, uint8_t shift = 0);

/// レジスタ間コピー（mov rd, rm）
uint32_t encode_mov(WReg rd, WReg rm);
uint32_t encode_mov(XReg rd, XReg rm);

// ============================================================================
// 4. メモリロード / ストア（スタックアクセス & 関数プロローグ・エピローグ）
// ============================================================================
/// 符号なしバイトオフセット付きロード / ストア（ldr / str rt, [rn, #byte_offset]）
uint32_t encode_ldr(WReg rt, XReg rn, uint32_t byte_offset = 0);
uint32_t encode_ldr(XReg rt, XReg rn, uint32_t byte_offset = 0);

uint32_t encode_str(WReg rt, XReg rn, uint32_t byte_offset = 0);
uint32_t encode_str(XReg rt, XReg rn, uint32_t byte_offset = 0);

/// ペアストア（Pre-index: stp rt1, rt2, [rn, #byte_offset]!）
uint32_t encode_stp_pre(XReg rt1, XReg rt2, XReg rn, int16_t byte_offset);

/// ペアロード（Post-index: ldp rt1, rt2, [rn], #byte_offset）
uint32_t encode_ldp_post(XReg rt1, XReg rt2, XReg rn, int16_t byte_offset);

// ============================================================================
// 5. 比較 & 条件フラグ抽出（cmp / cset）
// ============================================================================
uint32_t encode_cmp(WReg rn, WReg rm);
uint32_t encode_cmp(XReg rn, XReg rm);

uint32_t encode_cmp_imm(WReg rn, uint16_t imm12);
uint32_t encode_cmp_imm(XReg rn, uint16_t imm12);

/// 条件コードが成立していれば 1、不成立なら 0 を rd にセット（cset rd, cond）
uint32_t encode_cset(WReg rd, Cond cond);
uint32_t encode_cset(XReg rd, Cond cond);

// ============================================================================
// 6. 分岐・関数呼び出し・リターン（inst_offset は命令単位 = byte_offset / 4）
// ============================================================================
/// 無条件ジャンプ（b #offset）: 26-bit 符号付き命令オフセット
uint32_t encode_b(int32_t inst_offset);

/// 関数呼び出し（bl #offset）: 26-bit 符号付き命令オフセット
uint32_t encode_bl(int32_t inst_offset);

/// 条件付きジャンプ（b.cond #offset）: 19-bit 符号付き命令オフセット
uint32_t encode_b_cond(Cond cond, int32_t inst_offset);

/// ゼロ判定ジャンプ（cbz rt, #offset）: rt == 0 ならジャンプ（19-bit 符号付き命令オフセット）
uint32_t encode_cbz(WReg rt, int32_t inst_offset);
uint32_t encode_cbz(XReg rt, int32_t inst_offset);

/// 非ゼロ判定ジャンプ（cbnz rt, #offset）: rt != 0 ならジャンプ（19-bit 符号付き命令オフセット）
uint32_t encode_cbnz(WReg rt, int32_t inst_offset);
uint32_t encode_cbnz(XReg rt, int32_t inst_offset);

/// リターン命令（ret rn）
uint32_t encode_ret(XReg rn = reg::lr);

/// No Operation（nop）
uint32_t encode_nop();

}  // namespace axiom::arm64
