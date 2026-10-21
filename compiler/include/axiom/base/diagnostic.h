#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "axiom/base/compiler.h"

namespace axiom {

/// コンパイラ各フェーズ（型検査・IR検証など）で共通利用するエラー診断構造体
struct Diagnostic {
    StringID    function_name_id { kInvalidStringID }; // エラーが発生した関数の StringID
    std::string message;                               // 人間可読のエラーメッセージ
};

/// 既存の TypeChecker テストとの互換性のための型エイリアス
using TypeError = Diagnostic;

/// Diagnostic を生成して errors ベクタに追加する共通ユーティリティ。
/// interner が渡された場合は "<func_name>: <message>" 形式に整形する。
inline void report_error(
    std::vector<Diagnostic>& errors,
    StringID func_id,
    std::string_view message,
    const StringInterner* interner = nullptr
) {
    std::string full_message;
    if (interner != nullptr && func_id != kInvalidStringID) {
        full_message = std::string(interner->GetString(func_id)) + ": " + std::string(message);
    } else {
        full_message = std::string(message);
    }

    errors.push_back(Diagnostic{
        .function_name_id = func_id,
        .message = std::move(full_message)
    });
}

} // namespace axiom

