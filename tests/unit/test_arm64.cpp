/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>

#include "axiom/target/arm64.h"

using namespace axiom::arm64;
using namespace axiom::arm64::reg;

// ============================================================================
// 1. 算術命令（レジスタ版: add, sub, mul, sdiv）
// ============================================================================
TEST(Arm64EncoderTest, EncodeAddRegisters) {
    // add w0, w1, w2 -> 0x0b020020
    EXPECT_EQ(encode_add(w0, w1, w2), 0x0B020020U);
    // add x0, x1, x2 -> 0x8b020020
    EXPECT_EQ(encode_add(x0, x1, x2), 0x8B020020U);
}

TEST(Arm64EncoderTest, EncodeSubMulSdivRegisters) {
    // sub w0, w1, w2 -> 0x4b020020
    EXPECT_EQ(encode_sub(w0, w1, w2), 0x4B020020U);
    // sub x0, x1, x2 -> 0xcb020020
    EXPECT_EQ(encode_sub(x0, x1, x2), 0xCB020020U);

    // mul w3, w4, w5 -> 0x1b057c83
    EXPECT_EQ(encode_mul(w3, w4, w5), 0x1B057C83U);
    // mul x3, x4, x5 -> 0x9b057c83
    EXPECT_EQ(encode_mul(x3, x4, x5), 0x9B057C83U);

    // sdiv w6, w7, w8 -> 0x1ac80ce6
    EXPECT_EQ(encode_sdiv(w6, w7, w8), 0x1AC80CE6U);
    // sdiv x6, x7, x8 -> 0x9ac80ce6
    EXPECT_EQ(encode_sdiv(x6, x7, x8), 0x9AC80CE6U);
}

// ============================================================================
// 2. 算術命令（12-bit 即値版: add_imm, sub_imm）
// ============================================================================
TEST(Arm64EncoderTest, EncodeAddSubImmediate) {
    // add w0, w1, #42 -> 0x1100a820
    EXPECT_EQ(encode_add_imm(w0, w1, 42), 0x1100A820U);
    // add x29, sp, #16 -> 0x910043fd
    EXPECT_EQ(encode_add_imm(fp, sp, 16), 0x910043FDU);

    // sub sp, sp, #32 -> 0xd10083ff
    EXPECT_EQ(encode_sub_imm(sp, sp, 32), 0xD10083FFU);
    // sub w2, w3, #100 -> 0x51019062
    EXPECT_EQ(encode_sub_imm(w2, w3, 100), 0x51019062U);
}

// ============================================================================
// 3. 定数ロード & レジスタ間コピー（movz, movk, movn, mov）
// ============================================================================
TEST(Arm64EncoderTest, EncodeMovzImmediate) {
    // movz w0, #42 (shift 0) -> 0x52800540
    EXPECT_EQ(encode_movz(w0, 42), 0x52800540U);
    // movz w1, #0x1234, lsl #16 -> 0x52a24681
    EXPECT_EQ(encode_movz(w1, 0x1234, 16), 0x52A24681U);

    // movz x0, #42 (shift 0) -> 0xd2800540
    EXPECT_EQ(encode_movz(x0, 42), 0xD2800540U);
    // movz x2, #0x5678, lsl #32 -> 0xd2cacf02
    EXPECT_EQ(encode_movz(x2, 0x5678, 32), 0xD2CACF02U);
}

TEST(Arm64EncoderTest, EncodeMovkMovnAndMovReg) {
    // movk w0, #0x1234, lsl #16 -> 0x72a24680
    EXPECT_EQ(encode_movk(w0, 0x1234, 16), 0x72A24680U);
    // movk x0, #0xabcd, lsl #48 -> 0xf2f579a0
    EXPECT_EQ(encode_movk(x0, 0xABCD, 48), 0xF2F579A0U);

    // movn w0, #0 -> 0x12800000 (w0 = -1)
    EXPECT_EQ(encode_movn(w0, 0), 0x12800000U);
    // movn x1, #15 -> 0x928001e1 (x1 = -16)
    EXPECT_EQ(encode_movn(x1, 15), 0x928001E1U);

    // mov w0, w1 -> 0x2a0103e0
    EXPECT_EQ(encode_mov(w0, w1), 0x2A0103E0U);
    // mov x0, x1 -> 0xaa0103e0
    EXPECT_EQ(encode_mov(x0, x1), 0xAA0103E0U);
}

// ============================================================================
// 4. メモリロード / ストア（ldr, str, stp_pre, ldp_post）
// ============================================================================
TEST(Arm64EncoderTest, EncodeLoadStoreAndPair) {
    // ldr w0, [sp, #16] -> 0xb94013e0
    EXPECT_EQ(encode_ldr(w0, sp, 16), 0xB94013E0U);
    // ldr x0, [sp, #24] -> 0xf9400fe0
    EXPECT_EQ(encode_ldr(x0, sp, 24), 0xF9400FE0U);

    // str w1, [x29, #8] -> 0xb9000ba1
    EXPECT_EQ(encode_str(w1, fp, 8), 0xB9000BA1U);
    // str x2, [sp, #0] -> 0xf90003e2
    EXPECT_EQ(encode_str(x2, sp, 0), 0xF90003E2U);

    // stp x29, x30, [sp, #-16]! -> 0xa9bf7bfd
    EXPECT_EQ(encode_stp_pre(fp, lr, sp, -16), 0xA9BF7BFDU);
    // ldp x29, x30, [sp], #16 -> 0xa8c17bfd
    EXPECT_EQ(encode_ldp_post(fp, lr, sp, 16), 0xA8C17BFDU);
}

// ============================================================================
// 5. 比較 & 条件フラグ抽出（cmp, cmp_imm, cset）
// ============================================================================
TEST(Arm64EncoderTest, EncodeCompareAndCset) {
    // cmp w0, w1 -> 0x6b01001f
    EXPECT_EQ(encode_cmp(w0, w1), 0x6B01001FU);
    // cmp x0, x1 -> 0xeb01001f
    EXPECT_EQ(encode_cmp(x0, x1), 0xEB01001FU);

    // cmp w2, #10 -> 0x7100285f
    EXPECT_EQ(encode_cmp_imm(w2, 10), 0x7100285FU);
    // cmp x3, #255 -> 0xf103fc7f
    EXPECT_EQ(encode_cmp_imm(x3, 255), 0xF103FC7FU);

    // cset w0, eq -> 0x1a9f17e0
    EXPECT_EQ(encode_cset(w0, Cond::EQ), 0x1A9F17E0U);
    // cset w1, ne -> 0x1a9f07e1
    EXPECT_EQ(encode_cset(w1, Cond::NE), 0x1A9F07E1U);
    // cset w2, lt -> 0x1a9fa7e2
    EXPECT_EQ(encode_cset(w2, Cond::LT), 0x1A9FA7E2U);
    // cset w3, ge -> 0x1a9fb7e3
    EXPECT_EQ(encode_cset(w3, Cond::GE), 0x1A9FB7E3U);
    // cset w4, gt -> 0x1a9fd7e4
    EXPECT_EQ(encode_cset(w4, Cond::GT), 0x1A9FD7E4U);
    // cset w5, le -> 0x1a9fc7e5
    EXPECT_EQ(encode_cset(w5, Cond::LE), 0x1A9FC7E5U);
    // cset x0, eq -> 0x9a9f17e0
    EXPECT_EQ(encode_cset(x0, Cond::EQ), 0x9A9F17E0U);
}

// ============================================================================
// 6. 分岐・関数呼び出し・リターン（b, bl, b.cond, cbz, cbnz, ret, nop）
// ============================================================================
TEST(Arm64EncoderTest, EncodeBranchesAndCalls) {
    // b #16 (inst_offset = +4) -> 0x14000004
    EXPECT_EQ(encode_b(4), 0x14000004U);
    // b #-8 (inst_offset = -2) -> 0x17fffffe
    EXPECT_EQ(encode_b(-2), 0x17FFFFFEU);

    // bl #32 (inst_offset = +8) -> 0x94000008
    EXPECT_EQ(encode_bl(8), 0x94000008U);

    // b.eq #12 (inst_offset = +3) -> 0x54000060
    EXPECT_EQ(encode_b_cond(Cond::EQ, 3), 0x54000060U);
    // b.lt #-16 (inst_offset = -4) -> 0x54ffff8b
    EXPECT_EQ(encode_b_cond(Cond::LT, -4), 0x54FFFF8BU);

    // cbz w0, #20 (inst_offset = +5) -> 0x340000a0
    EXPECT_EQ(encode_cbz(w0, 5), 0x340000A0U);
    // cbz x1, #-12 (inst_offset = -3) -> 0xb4ffffa1
    EXPECT_EQ(encode_cbz(x1, -3), 0xB4FFFFA1U);

    // cbnz w2, #8 (inst_offset = +2) -> 0x35000042
    EXPECT_EQ(encode_cbnz(w2, 2), 0x35000042U);
    // cbnz x3, #16 (inst_offset = +4) -> 0xb5000083
    EXPECT_EQ(encode_cbnz(x3, 4), 0xB5000083U);
}

TEST(Arm64EncoderTest, EncodeRetAndNop) {
    // ret (デフォルト引数: lr = x30) -> 0xd65f03c0
    EXPECT_EQ(encode_ret(), 0xD65F03C0U);
    // ret x0 -> 0xd65f0000
    EXPECT_EQ(encode_ret(x0), 0xD65F0000U);
    // ret x1 -> 0xd65f0020
    EXPECT_EQ(encode_ret(x1), 0xD65F0020U);
    // ret x29 (fp) -> 0xd65f03a0
    EXPECT_EQ(encode_ret(fp), 0xD65F03A0U);

    // nop -> 0xd503201f
    EXPECT_EQ(encode_nop(), 0xD503201FU);
}
