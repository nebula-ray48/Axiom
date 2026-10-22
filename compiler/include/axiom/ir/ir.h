#pragma once

#include <cstdint>
#include <vector>
#include <limits>

#include "axiom/base/compiler.h"

namespace axiom::ir {

using RegId = uint32_t;
using LabelId = uint32_t;

constexpr RegId kInvalidRegId = std::numeric_limits<RegId>::max();

constexpr LabelId kInvalidLabelId = std::numeric_limits<LabelId>::max();

/// 命令の種類（1バイトの識別子）
enum class IrOpcode : uint8_t {
    Alloca, // 変数の枠を確保
    Assign, // 代入: dest = src1
    Add,    // 加算: dest = src1 + src2
    Sub,    // 減算: dest = src1 - src2
    Mul,    // 乗算: dest = src1 * src2
    Div,    // 除算: dest = src1 / src2
    Return, // 戻り値返却: return src1
    CmpEq, CmpNe, CmpLt, CmpLe, CmpGt, CmpGe, // 比較演算 { ==, !=, <, <=, >, >= }
    Label,  // ジャンプ先の目印（ターゲット）
    Jump,   // 無条件ジャンプ
    BranchIfFalse, // 条件が false (0) のときジャンプ
};

/// 値の種類
enum class IrOperandKind : uint8_t {
    None,       // 値なし
    Variable,   // 変数名（StringID）
    Temporary,  // 一時計算レジスタ（%0, %1 など）
    Constant,   // 整数即値
};

enum class IrType : uint8_t {
    Unknown,
    Void,
    Bool,
    I32, I64,
    F32, F64,
    I8, I16,
    U8, U16, U32, U64
};

[[nodiscard]] constexpr IrType parse_ir_type(std::string_view name) noexcept {
    if (name == "int8")    return IrType::I8;
    if (name == "int16")   return IrType::I16;
    if (name == "int32")   return IrType::I32;
    if (name == "int64")   return IrType::I64;
    if (name == "uint8")   return IrType::U8;
    if (name == "uint16")  return IrType::U16;
    if (name == "uint32")  return IrType::U32;
    if (name == "uint64")  return IrType::U64;
    if (name == "float32") return IrType::F32;
    if (name == "float64") return IrType::F64;
    if (name == "bool")    return IrType::Bool;
    if (name == "void")    return IrType::Void;
    return IrType::Unknown;
}

/// 命令が読み書きする値（オペランド）
struct IrOperand {
    IrOperandKind kind { IrOperandKind::None };
    IrType        type { IrType::Unknown }; // 型 (int32 など)
    RegId reg { kInvalidRegId }; // Variable または Temporary の StringID
    int64_t       constant_int { 0 };      // Constant の場合の整数値

    // 便利な静的ヘルパー（ファクトリ関数）
    static IrOperand MakeVar(RegId var_id, IrType type) noexcept {
        return { IrOperandKind::Variable, type, var_id, 0 };
    }
    static IrOperand MakeTemp(RegId temp_id, IrType type) noexcept {
        return { IrOperandKind::Temporary, type, temp_id, 0 };
    }
    static IrOperand MakeConst(int64_t val, IrType type) noexcept {
        return { IrOperandKind::Constant, type, kInvalidRegId, val };
    }
    static IrOperand MakeNone() noexcept {
        return {};
    }
};

/// 3番地コード形式の命令（dest = src1 op src2）
struct IrInstruction {
    IrOpcode  op { IrOpcode::Alloca };
    LabelId   label { kInvalidLabelId };
    IrOperand dest {}; // 格納先
    IrOperand src1 {}; // 入力1
    IrOperand src2 {}; // 入力2（単項やReturnならNone）
};

/// 関数の仮引数
struct IrParam {
    StringID name_id { kInvalidStringID };
    IrType type { IrType::Unknown };
    RegId reg { kInvalidRegId };
};

/// 1つの関数（時系列順に並んだ命令の配列を持つ）
struct IrFunction {
    StringID name_id { kInvalidStringID };
    IrType return_type { IrType::Void };

    std::vector<IrParam>       parameters;
    std::vector<IrInstruction> instructions;
    std::vector<IrType>        reg_types;
    std::vector<StringID>      reg_names;
    std::vector<StringID>      label_names;

    RegId allocate_reg(IrType type, StringID reg_name_id = kInvalidStringID) {
        RegId new_reg = static_cast<RegId>(reg_types.size());
        reg_types.push_back(type);
        reg_names.push_back(reg_name_id);

        return new_reg;
    }

    LabelId allocate_label(StringID label_name_id = kInvalidStringID) {
        LabelId new_label = static_cast<LabelId>(label_names.size());
        label_names.push_back(label_name_id);

        return new_label;
    }
};

/// モジュール全体（ファイル内の全関数）
struct IrModule {
    std::vector<IrFunction> functions;
};

} // namespace axiom::ir
