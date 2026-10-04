// core/src/symbol_table.cpp
//
// 【役割】
//   SymbolTable クラスの実装。
//   スコープ管理（ネストしたブロックの入退場）と
//   変数の「宣言・重複チェック」「型の逆引き（ルックアップ）」を提供する。
//
// 【内部表現のイメージ】
//
//   symbols_       : [x(int32), y(float32) | count(int32), flag(bool)]
//                                           ↑ ここが scope_markers_.back()
//   scope_markers_ : [0, 2]
//
//   → exit_scope() を呼ぶと symbols_.resize(2) で [x, y] だけが残る。
//
// 【ルックアップが内側スコープ優先な理由】
//   symbols_ を末尾から先頭に向かって探すので、
//   内側スコープ（末尾側）で宣言した変数が先にヒットする。
//   これにより変数シャドウイングが自然に機能する。

#include "axiom/sema/symbol_table.h"

#include <__ranges/reverse_view.h>

#include <ranges>

namespace axiom {

/// 新しいスコープに入る。
/// 現在の symbols_ サイズをマーカーとして push するだけ。
/// exit_scope() でこの位置まで巻き戻す。
void SymbolTable::enter_scope() {
    scope_markers_.push_back(symbols_.size());
}

/// 現在のスコープから出る。
/// スコープ内で宣言した変数をまとめて削除する（resize で末尾を切り捨て）。
/// スコープが1つもない状態で呼ばれても安全（何もしない）。
void SymbolTable::exit_scope() noexcept {

    if (scope_markers_.empty()) {
        // スコープが存在しない（enter_scope なしで exit を呼んだ場合など）
        return;
    }

    // マーカーが指す位置まで symbols_ を縮め、そのスコープの変数を削除
    size_t previous_size = scope_markers_.back();
    symbols_.resize(previous_size);
    scope_markers_.pop_back();

}

/// 現在のスコープに変数を宣言する。
///
/// 同一スコープ内に同名変数が既にある場合は false を返す（重複エラー）。
/// 外側スコープに同名変数がある場合はシャドウイングとして許可する（true を返す）。
///
/// 【探索範囲】
///   scope_markers_.empty() なら グローバルスコープ（先頭から）を探す。
///   そうでなければ scope_markers_.back() から末尾までが現在のスコープ。
bool SymbolTable::declare(StringID name_id, StringID type_id, bool is_mutable) {

    // 現在のスコープの開始インデックスを決定
    const size_t current_scope_start = scope_markers_.empty() ? 0 : scope_markers_.back();

    // 現在のスコープ内でのみ重複チェック（外側スコープは見ない）
    for (size_t i = current_scope_start; i < symbols_.size(); ++i) {
        if (symbols_[i].name_id == name_id) {
            return false;  // 同スコープ内で重複
        }
    }

    // 新しいシンボルを末尾に追加（指定初期化子構文でフィールドを明示）
    symbols_.push_back(Symbol{.name_id=name_id, .type_id=type_id, .is_mutable=is_mutable});
    return true;
}

const Symbol* SymbolTable::lookup_symbol(StringID name_id) const noexcept {
    // 末尾（内側スコープ）から先頭に向かって探索
    for (const auto& symbol : symbols_ | std::ranges::views::reverse) {
        if (symbol.name_id == name_id) {
            return &symbol;
        }
    }
    return nullptr;
}

/// 変数名の StringID から型の StringID を逆引きする。
/// 内側スコープ（末尾）から外側スコープ（先頭）の順に探す。
///
/// @return 変数が見つかればその型の StringID、見つからなければ kInvalidStringID
StringID SymbolTable::lookup(StringID name_id) const noexcept {
    const Symbol* sym = lookup_symbol(name_id);
    return sym ? sym->type_id : kInvalidStringID;
}

} // namespace axiom