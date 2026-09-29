// core/include/axiom/symbol_table.h
//
// 【役割】
//   型検査フェーズで使用するシンボルテーブルを定義するヘッダー。
//   スコープ管理（ネストしたブロックへの入退場）と
//   変数の「宣言」「参照解決（ルックアップ）」を担当する。
//
// 【スコープ管理の仕組み】
//   enter_scope() / exit_scope() がスタック的に動作する:
//
//   symbols_       : [a, b,  | c, d ]   ← 配列に全シンボルを詰める
//   scope_markers_ : [0,       2    ]   ← スコープ開始位置を記録
//
//   exit_scope() を呼ぶと scope_markers_.back() の位置まで symbols_ を縮める。
//   → スコープ内の変数が一括で消える（デストラクタなし、O(1)のリサイズ）。
//
// 【ルックアップの方向】
//   内側のスコープが優先されるよう、symbols_ を末尾から先頭に向かって線形探索する。
//   同名変数のシャドウイングに自然に対応できる。

#pragma once

#include <vector>

#include "compiler.h"

namespace axiom {

/// シンボルテーブルに登録される1エントリ。
/// 変数1つ分の名前・型・可変性を保持する。
struct Symbol {
    StringID name_id;   // 変数名のStringID
    StringID type_id;   // 型名のStringID（kInvalidStringID なら型不明）
    bool is_mutable;    // val(false) か var(true) か
};

/// スコープ対応のシンボルテーブル。
///
/// TypeChecker から使われる。関数の入退場や if/while ブロックの
/// 入退場ごとに enter_scope() / exit_scope() を呼ぶ。
class SymbolTable {
public:
    SymbolTable() = default;

    /// 新しいスコープに入る。現在の symbols_ サイズをマーカーとして記録する。
    void enter_scope();

    /// 現在のスコープから出る。スコープ内で宣言した変数を一括削除する。
    /// スコープが空の場合は何もしない（noexcept保証）。
    void exit_scope() noexcept;

    /// 現在のスコープに変数を宣言する。
    /// @param name_id   変数名の StringID
    /// @param type_id   型の StringID
    /// @param is_mutable var なら true, val なら false
    /// @return 成功すれば true、同スコープ内に同名変数がすでにあれば false（重複エラー）
    [[nodiscard]] bool declare(StringID name_id, StringID type_id, bool is_mutable);

    /// 変数名から型IDを逆引きする（内側スコープ優先）。
    /// @return 見つかれば型の StringID、見つからなければ kInvalidStringID
    [[nodiscard]] StringID lookup(StringID name_id) const noexcept;

private:
    std::vector<Symbol> symbols_;           // 全スコープのシンボルをフラットに格納
    std::vector<size_t> scope_markers_;     // 各スコープの開始インデックスを記録するスタック
};

}  // namespace axiom