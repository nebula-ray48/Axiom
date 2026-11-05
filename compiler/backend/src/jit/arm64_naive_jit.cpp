/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#include "axiom/jit/arm64_naive_jit.h"

#include <cassert>

#include "axiom/jit/liveness.h"

namespace axiom::jit {

using namespace axiom::arm64;

void Arm64NaiveJit::emit_prologue(const ir::IrFunction& func) {
    size_t raw_bytes = static_cast<size_t>(alloc_.num_spill_slots) * 4;
    frame_size_      = static_cast<uint16_t>((raw_bytes + 15) & ~size_t(15));

    emit(encode_stp_pre(reg::fp, reg::lr, reg::sp, -16));
    emit(encode_add_imm(reg::fp, reg::sp, 0));

    for (uint8_t r = 19; r <= 27; r += 2) {
        if ((alloc_.used_registers_mask & (1U << r)) != 0 || (alloc_.used_registers_mask & (1U << (r + 1))) != 0) {
            emit(encode_stp_pre(XReg{r}, XReg{static_cast<uint8_t>(r + 1)}, reg::sp, -16));
        }
    }

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

    for (uint8_t r = 27; r >= 19; r -= 2) {
        if ((alloc_.used_registers_mask & (1U << r)) != 0 || (alloc_.used_registers_mask & (1U << (r + 1))) != 0) {
            emit(encode_ldp_post(XReg{r}, XReg{static_cast<uint8_t>(r + 1)}, reg::sp, 16));
        }
    }

    emit(encode_ldp_post(reg::fp, reg::lr, reg::sp, 16));

    emit(encode_ret());
}

void Arm64NaiveJit::emit_load_operand(WReg dst, const ir::IrOperand& op) {
    if (op.kind == ir::IrOperandKind::Constant) {
        emit(encode_movz(dst, static_cast<uint16_t>(op.constant_int), 0));
    } else {
        const auto& loc = alloc_.locations[op.reg];
        if (loc.kind == StorageKind::Register) {
            if (loc.index >= 19) {
                emit(encode_mov(dst, WReg{static_cast<uint8_t>(loc.index)}));
            }
        } else {
            emit(encode_ldr(dst, reg::sp, stack_offset(loc.index)));
        }
    }
}

void Arm64NaiveJit::emit_store_reg(WReg src, ir::RegId dst_reg) {
    const auto& loc = alloc_.locations[dst_reg];
    if (loc.kind == StorageKind::Register) {
        if (loc.index >= 19 && loc.index != src.id) {
            emit(encode_mov(WReg{static_cast<uint8_t>(loc.index)}, src));
        }
    } else {
        emit(encode_str(src, reg::sp, stack_offset(loc.index)));
    }
}

WReg Arm64NaiveJit::resolve_src(const ir::IrOperand& op, WReg scratch) {
    if (op.kind != ir::IrOperandKind::Constant) {
        const auto& loc = alloc_.locations[op.reg];
        if (loc.kind == StorageKind::Register && loc.index >= 19) {
            return WReg{static_cast<uint8_t>(loc.index)};
        }
    }
    emit_load_operand(scratch, op);
    return scratch;
}

WReg Arm64NaiveJit::resolve_dst(ir::RegId dst_reg, WReg scratch) const noexcept {
    const auto& loc = alloc_.locations[dst_reg];
    if (loc.kind == StorageKind::Register && loc.index >= 19) {
        return WReg{static_cast<uint8_t>(loc.index)};
    }
    return scratch;
}

void Arm64NaiveJit::compile_function(const ir::IrFunction& func) {
    LivenessResult liveness = compute_liveness(func);
    alloc_                  = allocate_registers(func, liveness);

    emit_prologue(func);
    label_offsets_.assign(func.label_names.size(), 0);
    pending_fixups_.clear();

    for (const auto& inst : func.instructions) {
        switch (inst.op) {
            case ir::IrOpcode::Alloca: emit_store_reg(reg::wzr, inst.dest.reg); break;

            case ir::IrOpcode::Assign: {
                WReg src = resolve_src(inst.src1, reg::w9);
                emit_store_reg(src, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::Add: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_add(dst, s1, s2));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::Sub: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_sub(dst, s1, s2));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::Mul: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_mul(dst, s1, s2));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::Div: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_sdiv(dst, s1, s2));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::CmpEq: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_cmp(s1, s2));
                emit(encode_cset(dst, Cond::EQ));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::CmpNe: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_cmp(s1, s2));
                emit(encode_cset(dst, Cond::NE));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::CmpLt: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_cmp(s1, s2));
                emit(encode_cset(dst, Cond::LT));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::CmpLe: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_cmp(s1, s2));
                emit(encode_cset(dst, Cond::LE));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::CmpGt: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_cmp(s1, s2));
                emit(encode_cset(dst, Cond::GT));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::CmpGe: {
                WReg s1  = resolve_src(inst.src1, reg::w9);
                WReg s2  = resolve_src(inst.src2, reg::w10);
                WReg dst = resolve_dst(inst.dest.reg, reg::w8);
                emit(encode_cmp(s1, s2));
                emit(encode_cset(dst, Cond::GE));
                emit_store_reg(dst, inst.dest.reg);
                break;
            }

            case ir::IrOpcode::Return:
                if (inst.src1.kind != ir::IrOperandKind::None) {
                    WReg ret_val = resolve_src(inst.src1, reg::w0);
                    if (ret_val.id != reg::w0.id) {
                        emit(encode_mov(reg::w0, ret_val));
                    }
                }
                emit_epilogue();
                break;

            case ir::IrOpcode::Label:
                label_offsets_[inst.label] = code_.size();
                break;

            case ir::IrOpcode::Jump: {
                size_t branch_idx = code_.size();
                pending_fixups_.push_back({
                    .inst_index     = branch_idx,
                    .target_label   = inst.label,
                    .is_conditional = false,
                    .cond_reg       = reg::wzr,
                });
                emit(encode_b(0));
                break;
            }

            case ir::IrOpcode::BranchIfFalse: {
                WReg cond = resolve_src(inst.src1, reg::w9);
                size_t branch_idx = code_.size();
                pending_fixups_.push_back({
                    .inst_index     = branch_idx,
                    .target_label   = inst.label,
                    .is_conditional = true,
                    .cond_reg       = cond,
                });
                emit(encode_cbz(cond, 0));
                break;
            }

            case ir::IrOpcode::Call: {
                // 1. 実引数を w0 ~ w7 レジスタへロード (AAPCS64)
                uint32_t count = (inst.arg_count <= 8) ? inst.arg_count : 8;
                for (uint32_t i = 0; i < count; ++i) {
                    WReg arg_reg{static_cast<uint8_t>(i)};
                    WReg src = resolve_src(func.call_args[inst.arg_start + i], arg_reg);
                    if (src.id != arg_reg.id) {
                        emit(encode_mov(arg_reg, src));
                    }
                }

                // 2. bl 命令のバックパッチ予約と仮命令 emit
                size_t call_idx = code_.size();
                pending_calls_.push_back({
                    .inst_index       = call_idx,
                    .target_func_name = inst.call_func_name,
                });
                emit(encode_bl(0));

                // 3. 戻り値 (w0) を dest スロットへ退避
                if (inst.dest.kind != ir::IrOperandKind::None) {
                    emit_store_reg(reg::w0, inst.dest.reg);
                }
                break;
            }

            default:
                break;
        }
    }

    for (const auto& fixup : pending_fixups_) {
        size_t target_idx = label_offsets_[fixup.target_label];
        int32_t offset = static_cast<int32_t>(target_idx) - static_cast<int32_t>(fixup.inst_index);
        if (fixup.is_conditional) {
            code_[fixup.inst_index] = encode_cbz(fixup.cond_reg, offset);
        } else {
            code_[fixup.inst_index] = encode_b(offset);
        }
    }

    if (code_.empty() || code_.back() != encode_ret()) {
        emit_epilogue();
    }
}

std::vector<uint32_t> Arm64NaiveJit::compile(const ir::IrFunction& func) {
    Arm64NaiveJit jit;
    jit.compile_function(func);

    // 単一関数コンパイル時の自己再帰呼び出し (target_func_name == func.name_id) を解決
    for (const auto& call_fixup : jit.pending_calls_) {
        if (call_fixup.target_func_name == func.name_id && func.name_id != kInvalidStringID) {
            int32_t offset = -static_cast<int32_t>(call_fixup.inst_index);
            jit.code_[call_fixup.inst_index] = encode_bl(offset);
        }
    }

    return jit.code_;
}

CompiledModule Arm64NaiveJit::compile_module(const ir::IrModule& module) {
    Arm64NaiveJit  jit;
    CompiledModule result;

    for (const auto& func : module.functions) {
        // 16バイト (4命令) アライメントに揃える
        while (jit.code_.size() % 4 != 0) {
            jit.emit(encode_nop());
        }

        size_t func_start_idx = jit.code_.size();
        result.func_inst_offsets[func.name_id] = func_start_idx;
        jit.compile_function(func);
    }
    
    for (const auto& call_fixup : jit.pending_calls_) {
        auto it = result.func_inst_offsets.find(call_fixup.target_func_name);
        if (it != result.func_inst_offsets.end()) {
            int32_t offset = static_cast<int32_t>(it->second) - static_cast<int32_t>(call_fixup.inst_index);
            jit.code_[call_fixup.inst_index] = encode_bl(offset);
        }
    }

    result.code = std::move(jit.code_);
    return result;
}

}  // namespace axiom::jit
