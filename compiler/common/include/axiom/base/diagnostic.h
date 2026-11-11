/*
 * Copyright (c) 2026 Nebula-Ray42
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <algorithm>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tree_sitter/api.h"

#include "axiom/base/compiler.h"

namespace axiom {

/// ソースコード内の位置情報（行・列は 1-based）
struct SourceLocation {
    uint32_t line   { 0 }; // 1-based (0 は位置情報なし)
    uint32_t column { 0 }; // 1-based
    uint32_t length { 1 }; // ハイライト (^^^^) の文字幅

    [[nodiscard]] bool is_valid() const noexcept { return line > 0; }
};

/// TSNode から SourceLocation を構築する軽量ヘルパー
inline SourceLocation get_node_location(TSNode node) {
    if (ts_node_is_null(node)) {
        return SourceLocation{};
    }
    TSPoint start = ts_node_start_point(node);
    TSPoint end   = ts_node_end_point(node);

    uint32_t len = 1;
    if (end.row == start.row && end.column > start.column) {
        len = end.column - start.column;
    }

    return SourceLocation{
        .line   = start.row + 1,
        .column = start.column + 1,
        .length = len,
    };
}

/// コンパイラ各フェーズ（型検査・IR検証など）で共通利用するエラー診断構造体
struct Diagnostic {
    StringID       function_name_id { kInvalidStringID }; // エラーが発生した関数の StringID
    std::string    message;                               // 人間可読のエラーメッセージ
    SourceLocation location {};                           // ソースコード内の位置情報
};

/// 既存の TypeChecker テストとの互換性のための型エイリアス
using TypeError = Diagnostic;

/// Diagnostic を生成して errors ベクタに追加する共通ユーティリティ。
/// interner が渡された場合は "<func_name>: <message>" 形式に整形する。
inline void report_error(
    std::vector<Diagnostic>& errors,
    StringID func_id,
    std::string_view message,
    const StringInterner* interner = nullptr,
    SourceLocation location = {}
) {
    std::string full_message;
    if (interner != nullptr && func_id != kInvalidStringID) {
        full_message = std::string(interner->GetString(func_id)) + ": " + std::string(message);
    } else {
        full_message = std::string(message);
    }

    errors.push_back(Diagnostic{
        .function_name_id = func_id,
        .message = std::move(full_message),
        .location = location,
    });
}

/// TSNode を直接受け取って SourceLocation を自動抽出するオーバーロード
inline void report_error(
    std::vector<Diagnostic>& errors,
    StringID func_id,
    std::string_view message,
    const StringInterner* interner,
    TSNode node
) {
    report_error(errors, func_id, message, interner, get_node_location(node));
}

/// ソースコード全文から指定行（1-based）の文字列スライスを取得する。
/// 改行文字（\r\n または \n）は除外される。
inline std::string_view get_source_line(std::string_view source, uint32_t line_num) {
    if (line_num == 0 || source.empty()) {
        return "";
    }
    uint32_t current_line = 1;
    size_t start = 0;
    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n') {
            if (current_line == line_num) {
                size_t end = i;
                if (end > start && source[end - 1] == '\r') {
                    end--;
                }
                return source.substr(start, end - start);
            }
            current_line++;
            start = i + 1;
        }
    }
    if (current_line == line_num && start <= source.size()) {
        size_t end = source.size();
        if (end > start && source[end - 1] == '\r') {
            end--;
        }
        return source.substr(start, end - start);
    }
    return "";
}

/// 診断情報を Option A スタイル（3行凝縮型）でフォーマット出力する。
///
/// 例:
///   examples/fact.ax:2:8: error: If condition must be bool
///     2 |     if n + 1 {
///       |        ^^^^^
inline void render_diagnostic(
    std::ostream&          os,
    const Diagnostic&      diag,
    std::string_view       source,
    std::string_view       file_path = "",
    bool                   use_color = false
) {
    // ANSI カラーコード
    const char* c_reset     = use_color ? "\033[0m" : "";
    const char* c_bold      = use_color ? "\033[1m" : "";
    const char* c_bold_red  = use_color ? "\033[1;31m" : "";
    const char* c_bold_cyan = use_color ? "\033[1;36m" : "";

    // 位置情報がない場合はシンプルな 1 行フォールバック
    if (!diag.location.is_valid()) {
        if (!file_path.empty()) {
            os << c_bold << file_path << ": " << c_reset;
        }
        os << c_bold_red << "error: " << c_reset << diag.message << "\n";
        return;
    }

    // 1行目: file:line:col: error: message
    if (!file_path.empty()) {
        os << c_bold << file_path << ":" << diag.location.line << ":" << diag.location.column << ": " << c_reset;
    } else {
        os << c_bold << diag.location.line << ":" << diag.location.column << ": " << c_reset;
    }
    os << c_bold_red << "error: " << c_reset << diag.message << "\n";

    // 2行目: ガター (  2 | ) と該当行ソースコード
    std::string_view line_content = get_source_line(source, diag.location.line);
    std::string line_str = std::to_string(diag.location.line);
    size_t gutter_num_width = std::max<size_t>(2, line_str.size());
    std::string gutter_padding(gutter_num_width - line_str.size(), ' ');

    os << c_bold_cyan << " " << gutter_padding << line_str << " | " << c_reset
       << line_content << "\n";

    // 3行目: 下線キャレット (    |    ^^^^^)
    std::string empty_gutter_padding(gutter_num_width, ' ');
    os << c_bold_cyan << " " << empty_gutter_padding << " | " << c_reset;

    // column - 1 個のスペースを配置
    uint32_t col_offset = diag.location.column > 1 ? (diag.location.column - 1) : 0;
    os << std::string(col_offset, ' ');

    // length 個の ^ を描画（最低 1 個）
    uint32_t len = diag.location.length > 0 ? diag.location.length : 1;
    os << c_bold_red << std::string(len, '^') << c_reset << "\n";
}

} // namespace axiom

