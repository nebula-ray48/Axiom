// core/src/compiler.cpp
//
// 【役割】
//   コンパイラの「旧実装」と「テスト用エントリポイント」をまとめたファイル。
//
//   - TreeSitterSymbols::Initialize() : Tree-sitter のシンボル/フィールドIDを一括解決する初期化処理
//   - AnalyzeAST()                    : DODレジストリに変数・コンポーネントを格納する旧AST解析処理
//   - parse_test_code()               : テスト用のハードコードされたサンプルコードをパースする関数
//
// 【現状の位置付け】
//   AnalyzeAST() は Analyzer クラスへの移行が進行中で、旧実装として残っている状態。
//   将来的には Analyzer + TypeChecker の組み合わせに統一する予定。
//
// 【Tree-sitter のシンボルとフィールドIDについて】
//   TSSymbol  : ASTノードの種類を表す整数ID（例: "variable_declaration" → 整数）
//   TSFieldId : ASTのフィールド名を表す整数ID（例: "name" → 整数）
//   これらを毎回文字列で解決するのは遅いため、Initialize() で起動時に一度だけ解決してキャッシュする。

#include "axiom/compiler.h"

namespace axiom {

/// Tree-sitter の TSLanguage* からシンボルIDとフィールドIDを解決してメンバに格納する。
///
/// ts_language_symbol_for_name(lang, name, len, named):
///   name  : ノード種別の文字列名（grammar.js の rule名）
///   len   : 文字列の長さ
///   named : true = 名前付きノード（通常これを使う）
///
/// ts_language_field_id_for_name(lang, name, len):
///   name : フィールド名（grammar.js で field() で定義したもの）
///   len  : 文字列の長さ
void TreeSitterSymbols::Initialize(const TSLanguage* lang) {
    // ノード種別の整数IDを解決してキャッシュ
    variable_declaration  = ts_language_symbol_for_name(lang, "variable_declaration",  20, true);
    component_declaration = ts_language_symbol_for_name(lang, "component_declaration", 21, true);
    field_definition      = ts_language_symbol_for_name(lang, "field_definition",      16, true);

    // フィールド名の整数IDを解決してキャッシュ
    field_name  = ts_language_field_id_for_name(lang, "name",  4);
    field_type  = ts_language_field_id_for_name(lang, "type",  4);
    field_value = ts_language_field_id_for_name(lang, "value", 5);
}

/// ASTルートノードを走査して VariableRegistry_DOD と ComponentRegistry_DOD に格納する。
/// （旧実装。現在は Analyzer クラスへ移行中）
///
/// 処理対象:
///   - variable_declaration  → VariableRegistry_DOD (names, types, initial_values) に追加
///   - component_declaration → ComponentRegistry_DOD にコンポーネント情報を追加
///
/// 【DOD（Data-Oriented Design）のコンポーネントフィールドの格納方法】
///   コンポーネント "Position { x: float32, y: float32 }" を格納すると:
///     comp_registry.names         = ["Position"]
///     comp_registry.field_starts  = [0]          ← field_names 配列の開始インデックス
///     comp_registry.field_counts  = [2]           ← フィールドの個数
///     comp_registry.field_names   = ["x", "y"]
///     comp_registry.field_types   = ["float32", "float32"]
///
///   複数コンポーネントがある場合、field_names/field_types は連続して追記される。
void AnalyzeAST(TSNode root_node, std::string_view source_text, StringInterner& interner, VariableRegistry_DOD& registry, ComponentRegistry_DOD& comp_registry, const TreeSitterSymbols& symbols) {
    uint32_t child_count = ts_node_child_count(root_node);  // 名前なし含む全子ノード数

    for (uint32_t i = 0; i < child_count; ++i) {
        TSNode child = ts_node_child(root_node, i);

        if (ts_node_symbol(child) == symbols.variable_declaration) {
            // --- 変数宣言の処理 ---
            TSNode name_node  = ts_node_child_by_field_id(child, symbols.field_name);
            TSNode type_node  = ts_node_child_by_field_id(child, symbols.field_type);
            TSNode value_node = ts_node_child_by_field_id(child, symbols.field_value);

            // 各ノードのバイト範囲をソース文字列から切り出す
            uint32_t n_start = ts_node_start_byte(name_node);
            uint32_t n_end   = ts_node_end_byte(name_node);
            std::string_view name_str = source_text.substr(n_start, n_end - n_start);

            uint32_t t_start = ts_node_start_byte(type_node);
            uint32_t t_end   = ts_node_end_byte(type_node);
            std::string_view type_str = source_text.substr(t_start, t_end - t_start);

            uint32_t v_start = ts_node_start_byte(value_node);
            uint32_t v_end   = ts_node_end_byte(value_node);
            std::string_view value_str = source_text.substr(v_start, v_end - v_start);

            // 文字列を StringID に変換して DOD レジストリへ追加
            StringID name_id  = interner.Intern(name_str);
            StringID type_id  = interner.Intern(type_str);
            StringID value_id = interner.Intern(value_str);

            registry.names.push_back(name_id);
            registry.types.push_back(type_id);
            registry.initial_values.push_back(value_id);

        } else if (ts_node_symbol(child) == symbols.component_declaration) {
            // --- コンポーネント宣言の処理 ---
            TSNode name_node = ts_node_child_by_field_id(child, symbols.field_name);
            uint32_t n_start = ts_node_start_byte(name_node);
            uint32_t n_end   = ts_node_end_byte(name_node);
            std::string_view name_str = source_text.substr(n_start, n_end - n_start);
            StringID name_id = interner.Intern(name_str);

            // このコンポーネントのフィールドが field_names 配列の何番目から始まるかを記録
            uint32_t start_idx  = static_cast<uint32_t>(comp_registry.field_names.size());
            uint32_t field_count = 0;

            // コンポーネントの子ノードを走査して field_definition を収集
            uint32_t comp_child_count = ts_node_child_count(child);
            for (uint32_t j = 0; j < comp_child_count; ++j) {
                TSNode comp_child = ts_node_child(child, j);

                if (ts_node_symbol(comp_child) == symbols.field_definition) {
                    TSNode f_name_node = ts_node_child_by_field_id(comp_child, symbols.field_name);
                    TSNode f_type_node = ts_node_child_by_field_id(comp_child, symbols.field_type);

                    uint32_t fn_start = ts_node_start_byte(f_name_node);
                    uint32_t fn_end   = ts_node_end_byte(f_name_node);
                    std::string_view f_name_str = source_text.substr(fn_start, fn_end - fn_start);

                    uint32_t ft_start = ts_node_start_byte(f_type_node);
                    uint32_t ft_end   = ts_node_end_byte(f_type_node);
                    std::string_view f_type_str = source_text.substr(ft_start, ft_end - ft_start);

                    // フィールド名・型を連続配列に追加
                    comp_registry.field_names.push_back(interner.Intern(f_name_str));
                    comp_registry.field_types.push_back(interner.Intern(f_type_str));
                    field_count++;
                }
            }

            // コンポーネント自体の情報を追加（DODの「ヘッダー」部分）
            comp_registry.names.push_back(name_id);
            comp_registry.field_starts.push_back(start_idx);
            comp_registry.field_counts.push_back(field_count);
        }
    }
}

/// コンパイラの動作確認用テスト関数。
///
/// ハードコードされたサンプルコードをパースして DOD レジストリに格納し、
/// 処理が正常に完了するか確認する目的で使う。
/// （現時点では結果の出力はしていない。デバッガで registry を確認すること）
///
/// サンプルコード: "val hp: int32 = 100; component Position { x: float32, y: float32 }"
void parse_test_code() {
    // --- Tree-sitter パーサーの初期化 ---
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());  // Nexa 言語グラマーをセット

    std::string source_code = "val hp: int32 = 100; component Position { x: float32, y: float32 }";

    // ソースコードを解析して構文木を生成
    // 第2引数 (nullptr) は「前回の木」で、インクリメンタル更新には使わないので null
    TSTree* tree = ts_parser_parse_string(
        parser,
        nullptr,
        source_code.c_str(),
        static_cast<uint32_t>(source_code.length())
    );

    TSNode root_node = ts_tree_root_node(tree);

    // --- コンパイラ部品の初期化 ---
    StringInterner interner;
    TreeSitterSymbols symbols;
    symbols.Initialize(tree_sitter_nexa());  // シンボルIDを一度だけ解決

    VariableRegistry_DOD registry;
    ComponentRegistry_DOD comp_registry;

    // AST を走査して DOD レジストリに格納
    AnalyzeAST(root_node, source_code, interner, registry, comp_registry, symbols);

    // --- 後片付け（Tree-sitter のリソースを解放する） ---
    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

} // namespace axiom