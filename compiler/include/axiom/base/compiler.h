// core/include/axiom/compiler.h
//
// 【役割】
//   Nexaコンパイラのコア部品（文字列インターナー・DOD用レジストリ・
//   Tree-sitterシンボルキャッシュ）を定義するヘッダー。
//   コンパイラ全体で最初にインクルードされる "基盤" ファイル。
//
// 【用語メモ】
//   StringID    : 文字列を整数IDに変換したもの。文字列比較をO(1)にするため。
//   DOD         : Data-Oriented Design。メモリ局所性のため、
//                 「構造体の配列(AoS)」ではなく「配列の構造体(SoA)」で管理する設計。
//   Tree-sitter : Nexaのパーサーとして使っているインクリメンタルパーサーライブラリ。
//                 C APIを持ち、TSNode・TSSymbol・TSFieldId などの型を提供する。

#pragma once

#include <deque>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <tree_sitter/api.h>

// tree-sitter-nexa グラマーのエントリポイント（C言語で実装されている）。
// tree_sitter_axiom() を呼ぶことで Axiom 言語の TSLanguage* が得られる。
extern "C" {
const TSLanguage* tree_sitter_axiom();
}

namespace axiom {

// ----------------------------------------------------------------
// StringID / StringInterner
// ----------------------------------------------------------------

/// 文字列をインターン（重複を排除して一元管理）するための整数型の別名。
/// 実態は uint32_t。0xFFFFFFFF は「無効値」として使う。
using StringID = uint32_t;

/// 「IDが未設定である」ことを示す番兵値。
/// 比較には is_valid() を使うとわかりやすい。
constexpr StringID kInvalidStringID = std::numeric_limits<StringID>::max(); // 0xFFFFFFFFU

/// IDが有効かどうかを判定するヘルパー。
/// `id != kInvalidStringID` の読みやすい別名として使う。
constexpr bool is_valid(StringID id) noexcept {
    return id != kInvalidStringID;
}


/// 文字列インターナー。
///
/// 同じ内容の文字列には同じ uint32_t (StringID) を割り当て、
/// 文字列比較を整数比較に変換することでパフォーマンスを改善する。
///
/// 【内部構造】
///   id_to_string : IDから元の文字列へのマッピング（std::deque でポインタ安定性を保つ）
///   string_to_id : 文字列→IDの逆引きマップ（string_view で参照するためdequeが必要）
///
/// 【注意】
///   string_to_id の string_view は id_to_string の要素を参照しているので、
///   id_to_string の要素が再アロケートされると dangling になる。
///   そのため vector ではなく deque を使って push_back 時のポインタ安定性を保っている。
class StringInterner {
private:
    std::deque<std::string> id_to_string;
    std::unordered_map<std::string_view, StringID> string_to_id;

public:
    /// 文字列を登録し、対応する StringID を返す。
    /// すでに登録済みの文字列なら新たに追加せず既存IDを返す（べき等）。
    StringID Intern(std::string_view str) {
        auto it = string_to_id.find(str);
        if (it != string_to_id.end()) return it->second;

        // 新規文字列をdequeの末尾に追加し、そのstring_viewをキーにする
        id_to_string.push_back(std::string(str));
        StringID new_id = static_cast<StringID>(id_to_string.size() - 1);
        string_to_id[id_to_string.back()] = new_id;
        return new_id;
    }

    /// IDに対応する文字列を返す。IDが範囲外なら "<unknown>" を返す。
    [[nodiscard]] std::string_view GetString(StringID id) const {
        if (id < id_to_string.size()) {
            return id_to_string[id];
        }
        return "<unknown>";
    }
};

// ----------------------------------------------------------------
// DOD レジストリ（旧AST解析の中間データ。現在は Analyzer クラスに移行中）
// ----------------------------------------------------------------

/// グローバルスコープの変数宣言をDOD形式で保持するレジストリ。
/// names[i], types[i], initial_values[i] が同じ変数 i のデータを表す。
/// （例: `val hp: int32 = 100` → name="hp", type="int32", value="100"）
struct VariableRegistry_DOD {
    std::vector<StringID> names;
    std::vector<StringID> types;
    std::vector<StringID> initial_values;
};

/// コンポーネント宣言をDOD形式で保持するレジストリ。
///
/// コンポーネント i のフィールド一覧は以下で取得できる:
///   field_names[field_starts[i] .. field_starts[i] + field_counts[i]]
///
/// 例: `component Position { x: float32, y: float32 }`
///   names[0]       = "Position"
///   field_starts[0]= 0, field_counts[0]= 2
///   field_names    = ["x", "y"]
///   field_types    = ["float32", "float32"]
struct ComponentRegistry_DOD {
    std::vector<StringID> names;
    std::vector<uint32_t> field_starts;  // フィールド配列の開始インデックス
    std::vector<uint32_t> field_counts;  // このコンポーネントのフィールド数
    std::vector<StringID> field_names;   // 全コンポーネントのフィールド名を連続して格納
    std::vector<StringID> field_types;   // 全コンポーネントのフィールド型を連続して格納
};

// ----------------------------------------------------------------
// TreeSitterSymbols
// ----------------------------------------------------------------

/// Tree-sitter のノード種別(TSSymbol)とフィールドID(TSFieldId)をキャッシュする構造体。
///
/// Tree-sitter のシンボル/フィールドIDは ts_language_symbol_for_name() 等で
/// 毎回文字列検索すると遅いため、起動時に一度だけ Initialize() で解決しておく。
///
/// 【使い方】
///   TreeSitterSymbols symbols;
///   symbols.Initialize(tree_sitter_axiom());
///   // 以降は symbols.variable_declaration などを直接比較に使う
struct TreeSitterSymbols {
    TSSymbol variable_declaration;   // "variable_declaration" ノードのシンボルID
    TSSymbol component_declaration;  // "component_declaration" ノードのシンボルID
    TSSymbol field_definition;       // "field_definition" ノードのシンボルID

    TSFieldId field_name;   // フィールド名 "name" のID
    TSFieldId field_type;   // フィールド名 "type" のID
    TSFieldId field_value;  // フィールド名 "value" のID

    /// TSLanguage* から各シンボル/フィールドIDを解決してメンバに格納する。
    /// プログラム起動時に1回だけ呼ぶ。
    void Initialize(const TSLanguage* lang);
};

// ----------------------------------------------------------------
// 関数宣言
// ----------------------------------------------------------------

/// Tree-sitter で解析した ASTルートノードを走査し、
/// VariableRegistry_DOD と ComponentRegistry_DOD にデータを格納する。
/// （旧実装。現在は Analyzer クラスへ移行中）
void AnalyzeAST(TSNode root_node, std::string_view source_text, StringInterner& interner, VariableRegistry_DOD& registry, ComponentRegistry_DOD& comp_registry, const TreeSitterSymbols& symbols);

/// コンパイラの動作確認用テスト関数。
/// ハードコードされたサンプルコードをパースして DOD レジストリに登録する。
void parse_test_code();

} // namespace axiom