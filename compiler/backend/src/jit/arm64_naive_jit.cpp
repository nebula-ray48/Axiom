/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include "axiom/jit/arm64_naive_jit.h"

#include <cassert>

namespace axiom::jit {

using namespace axiom::arm64;

void Arm64NaiveJit::emit_prologue(const ir::IrFunction& func) {
    size_t raw_bytes = func.reg_types.size() * 4;
    frame_size_      = static_cast<uint16_t>((raw_bytes + 15) & ~size_t(15));

    emit(encode_stp_pre(reg::fp, reg::lr, reg::sp, -16));
    emit(encode_add_imm(reg::fp, reg::sp, 0));

    if (frame_size_ > 0) {
        emit(encode_sub_imm(reg::sp, reg::sp, frame_size_));
    }

    for (size_t i = 0; i < func.parameters.size() && i < 8; ++i) {
        WReg arg_reg{static_cast<uint8_t>(i)};
        emit_store_reg(arg_reg, func.parameters[i].reg);
    }
}

void Arm64NaiveJit::emit_epilogue() {
    if (frame_size_ > 0) {
        emit(encode_add_imm(reg::sp, reg::sp, frame_size_));
    }

    emit(encode_ldp_post(reg::fp, reg::lr, reg::sp, 16));

    emit(encode_ret());
}

void Arm64NaiveJit::emit_load_operand(WReg dst, const ir::IrOperand& op) {
    if (op.kind == ir::IrOperandKind::Constant) {
        emit(encode_movz(dst, static_cast<uint16_t>(op.constant_int), 0));
    } else {
        emit(encode_ldr(dst, reg::sp, stack_offset(op.reg)));
    }
}

void Arm64NaiveJit::emit_store_reg(WReg src, ir::RegId dst_reg) {
    emit(encode_str(src, reg::sp, stack_offset(dst_reg)));
}

std::vector<uint32_t> Arm64NaiveJit::compile(const ir::IrFunction& func) {
    Arm64NaiveJit jit;
    jit.emit_prologue(func);
    jit.label_offsets_.assign(func.label_names.size(), 0);

    for (const auto& inst : func.instructions) {
        switch (inst.op) {
            case ir::IrOpcode::Alloca: jit.emit_store_reg(reg::wzr, inst.dest.reg); break;

            case ir::IrOpcode::Assign:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_store_reg(reg::w9, inst.dest.reg);
                break;

            case ir::IrOpcode::Add:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_add(reg::w8, reg::w9, reg::w10));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::Sub:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_sub(reg::w8, reg::w9, reg::w10));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::Mul:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_mul(reg::w8, reg::w9, reg::w10));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::Div:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_sdiv(reg::w8, reg::w9, reg::w10));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::CmpEq:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_cmp(reg::w9, reg::w10));
                jit.emit(encode_cset(reg::w8, Cond::EQ));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::CmpNe:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_cmp(reg::w9, reg::w10));
                jit.emit(encode_cset(reg::w8, Cond::NE));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::CmpLt:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_cmp(reg::w9, reg::w10));
                jit.emit(encode_cset(reg::w8, Cond::LT));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::CmpLe:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_cmp(reg::w9, reg::w10));
                jit.emit(encode_cset(reg::w8, Cond::LE));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::CmpGt:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_cmp(reg::w9, reg::w10));
                jit.emit(encode_cset(reg::w8, Cond::GT));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::CmpGe:
                jit.emit_load_operand(reg::w9, inst.src1);
                jit.emit_load_operand(reg::w10, inst.src2);
                jit.emit(encode_cmp(reg::w9, reg::w10));
                jit.emit(encode_cset(reg::w8, Cond::GE));
                jit.emit_store_reg(reg::w8, inst.dest.reg);
                break;

            case ir::IrOpcode::Return:
                if (inst.src1.kind != ir::IrOperandKind::None) {
                    jit.emit_load_operand(reg::w0, inst.src1);
                }
                jit.emit_epilogue();
                break;

            case ir::IrOpcode::Label:
                jit.label_offsets_[inst.label] = jit.code_.size();
                break;

            case ir::IrOpcode::Jump: {
                size_t branch_idx = jit.code_.size();
                jit.pending_fixups_.push_back({
                    .inst_index     = branch_idx,
                    .target_label   = inst.label,
                    .is_conditional = false,
                    .cond_reg       = reg::wzr,
                });
                jit.emit(encode_b(0));
                break;
            }

            case ir::IrOpcode::BranchIfFalse: {
                jit.emit_load_operand(reg::w9, inst.src1);
                size_t branch_idx = jit.code_.size();
                jit.pending_fixups_.push_back({
                    .inst_index     = branch_idx,
                    .target_label   = inst.label,
                    .is_conditional = true,
                    .cond_reg       = reg::w9,
                });
                jit.emit(encode_cbz(reg::w9, 0));
                break;
            }

            default:
                break;
        }
    }

    for (const auto& fixup : jit.pending_fixups_) {
        size_t target_idx = jit.label_offsets_[fixup.target_label];
        int32_t offset = static_cast<int32_t>(target_idx) - static_cast<int32_t>(fixup.inst_index);
        if (fixup.is_conditional) {
            jit.code_[fixup.inst_index] = encode_cbz(fixup.cond_reg, offset);
        } else {
            jit.code_[fixup.inst_index] = encode_b(offset);
        }
    }

    if (jit.code_.empty() || jit.code_.back() != encode_ret()) {
        jit.emit_epilogue();
    }

    return jit.code_;
}

}  // namespace axiom::jit
