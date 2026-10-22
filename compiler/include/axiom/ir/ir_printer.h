#pragma once

#include <string>
#include <string_view>

#include "axiom/base/compiler.h"
#include "axiom/ir/ir.h"

namespace axiom::ir {

/// IrType を短縮名 ("i32", "f32", "void" 等) に変換する
[[nodiscard]] constexpr std::string_view format_ir_type(IrType type) noexcept {
    switch (type) {
        case IrType::Void:    return "void";
        case IrType::Bool:    return "bool";
        case IrType::I8:      return "i8";
        case IrType::I16:     return "i16";
        case IrType::I32:     return "i32";
        case IrType::I64:     return "i64";
        case IrType::U8:      return "u8";
        case IrType::U16:     return "u16";
        case IrType::U32:     return "u32";
        case IrType::U64:     return "u64";
        case IrType::F32:     return "f32";
        case IrType::F64:     return "f64";
        case IrType::Unknown:
        default:              return "unknown";
    }
}

/// IrOpcode をニーモニック文字列 ("alloca", "add" 等) に変換する
[[nodiscard]] constexpr std::string_view format_ir_opcode(IrOpcode op) noexcept {
    switch (op) {
        case IrOpcode::Alloca: return "alloca";
        case IrOpcode::Assign: return "assign";
        case IrOpcode::Add:    return "add";
        case IrOpcode::Sub:    return "sub";
        case IrOpcode::Mul:    return "mul";
        case IrOpcode::Div:    return "div";
        case IrOpcode::Return: return "ret";
        case IrOpcode::CmpEq:  return "cmp_eq";
        case IrOpcode::CmpNe:  return "cmp_ne";
        case IrOpcode::CmpLt:  return "cmp_lt";
        case IrOpcode::CmpLe:  return "cmp_le";
        case IrOpcode::CmpGt:  return "cmp_gt";
        case IrOpcode::CmpGe:  return "cmp_ge";
        case IrOpcode::Label:  return "label";
        case IrOpcode::Jump:   return "jmp";
        case IrOpcode::BranchIfFalse: return "br_false";
        default:               return "unknown";
    }
}

/// 1つの IrFunction を人間が読めるテキスト形式にダンプする。
/// - use_reg_names = true かつ interner != nullptr の場合: 名前付き変数は `%a: i32`、一時値は `%3: i32`
/// - use_reg_names = false または interner == nullptr の場合: すべて番号で `%0: i32`, `%3: i32`
[[nodiscard]] std::string dump_ir(
    const IrFunction& func,
    const StringInterner* interner = nullptr,
    bool use_reg_names = true
);

/// IrModule 全体をテキスト形式にダンプする。
[[nodiscard]] std::string dump_ir(
    const IrModule& module,
    const StringInterner* interner = nullptr,
    bool use_reg_names = true
);

} // namespace axiom::ir

