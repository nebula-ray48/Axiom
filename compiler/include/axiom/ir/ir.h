#pragma once

#include <cstdint>
#include <vector>

#include "axiom/base/compiler.h"

namespace axiom::ir {

using RegId = uint32_t;
using LabelId = uint32_t;

/// 命令の種類（1バイトの識別子）
enum class IrOpcode : uint8_t {
    Alloca, // 変数の枠を確保
    Assign, // 代入: dest = src1
    Add,    // 加算: dest = src1 + src2
    Sub,    // 減算: dest = src1 - src2
    Mul,    // 乗算: dest = src1 * src2
    Div,    // 除算: dest = src1 / src2
    Return, // 戻り値返却: return src1
};

/// 値の種類
enum class IrOperandKind : uint8_t {
    None,       // 値なし
    Variable,   // 変数名（StringID）
    Temporary,  // 一時計算レジスタ（%0, %1 など）
    Constant,   // 整数即値
};

/// 命令が読み書きする値（オペランド）
struct IrOperand {
    IrOperandKind kind { IrOperandKind::None };
    StringID      id { kInvalidStringID };      // Variable または Temporary の StringID
    int64_t       constant_int { 0 };           // Constant の場合の整数値
    StringID      type_id { kInvalidStringID }; // 型 (int32 など)

    // 便利な静的ヘルパー（ファクトリ関数）
    static IrOperand MakeVar(StringID var_id, StringID type) noexcept {
        return { IrOperandKind::Variable, var_id, 0, type };
    }
    static IrOperand MakeTemp(StringID temp_id, StringID type) noexcept {
        return { IrOperandKind::Temporary, temp_id, 0, type };
    }
    static IrOperand MakeConst(int64_t val, StringID type) noexcept {
        return { IrOperandKind::Constant, kInvalidStringID, val, type };
    }
    static IrOperand MakeNone() noexcept {
        return {};
    }
};

/// 3番地コード形式の命令（dest = src1 op src2）
struct IrInstruction {
    IrOpcode  op { IrOpcode::Alloca };
    IrOperand dest {}; // 格納先
    IrOperand src1 {}; // 入力1
    IrOperand src2 {}; // 入力2（単項やReturnならNone）
};

/// 関数の仮引数
struct IrParam {
    StringID name_id { kInvalidStringID };
    StringID type_id { kInvalidStringID };
};

/// 1つの関数（時系列順に並んだ命令の配列を持つ）
struct IrFunction {
    StringID name_id { kInvalidStringID };
    StringID return_type_id { kInvalidStringID };

    std::vector<IrParam>       parameters;
    std::vector<IrInstruction> instructions; // ★ 時系列順のフラットな命令列！
};

/// モジュール全体（ファイル内の全関数）
struct IrModule {
    std::vector<IrFunction> functions;
};

} // namespace axiom::ir
