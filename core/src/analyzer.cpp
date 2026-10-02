// core/src/analyzer.cpp
//
// 【役割】
//   Analyzer クラスの実装。Tree-sitter の AST を再帰的に走査し、
//   各文法要素（関数・変数・制御構文）を FunctionInfo 等の中間データに変換する。
//
// 【Tree-sitter API 早見表（このファイルで使っているもの）】
//   ts_node_named_child_count(node)          : 名前付き子ノードの数を返す
//   ts_node_named_child(node, i)             : i番目の名前付き子ノードを返す
//   ts_node_child(node, i)                   : i番目の子ノード（名前なし含む）を返す
//   ts_node_type(node)                       : ノードの種類名（文字列）を返す（例: "function_declaration"）
//   ts_node_child_by_field_name(node, name, len) : フィールド名でノードを取得する
//   ts_node_is_null(node)                    : ノードが null かを確認する
//   ts_node_start_byte(node)                 : ノードの開始バイト位置
//   ts_node_end_byte(node)                   : ノードの終了バイト位置
//
// 【フィールド名について】
//   tree-sitter-nexa の grammar.js で定義されたフィールド名（"name", "body" 等）を使う。
//   第3引数はフィールド名の文字列長。

#include "axiom/registry.h"
#include "axiom/analyzer.h"

namespace axiom {

Analyzer::Analyzer(std::string_view source_code, StringInterner& interner)
    : source_(source_code), interner_(interner) {}

/// ASTのルートノードを走査し、トップレベルの function_declaration を解析する。
/// Nexa ではトップレベルに関数宣言しか置けない設計なので、それ以外は無視する。
void Analyzer::analyze_root(TSNode root_node) {
    uint32_t count = ts_node_named_child_count(root_node);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_named_child(root_node, i);
        std::string_view type = ts_node_type(child);
        if (type == "function_declaration") {
            analyze_function(child);
        }
    }
}

/// TSNode が対応するソーステキストを取り出し、StringInterner に登録して StringID を返す。
/// ノードが null の場合は kInvalidStringID を返す。
///
/// 【仕組み】
///   Tree-sitter は各ノードに「ソース文字列中の開始/終了バイト位置」を持っている。
///   source_.substr(start, end - start) でノードのテキストを切り出し、
///   interner_.Intern() で StringID に変換する。
StringID Analyzer::get_node_string_id(TSNode node) {
    if (ts_node_is_null(node)) {
        return kInvalidStringID;
    }
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    std::string_view text = source_.substr(start, end - start);
    return interner_.Intern(text);
}

/// function_declaration ノードから FunctionInfo を構築して functions_ に追加する。
///
/// 処理内容:
///   1. フィールド "name" で関数名を取得
///   2. 子ノードを走査して parameter_list を見つけ、analyze_parameters() に委譲
///   3. フィールド "body" でブロックを取得し、analyze_block() に委譲
void Analyzer::analyze_function(TSNode func_node) {
    FunctionInfo info;

    // "name" フィールドから関数名の StringID を取得（4は "name" の文字列長）
    TSNode name_node = ts_node_child_by_field_name(func_node, "name", 4);
    info.name_id = get_node_string_id(name_node);

    // 子ノードの中から parameter_list を探す（フィールド名ではなくノード型で特定）
    uint32_t child_count = ts_node_named_child_count(func_node);
    for (uint32_t i = 0; i < child_count; ++i) {
        TSNode child = ts_node_named_child(func_node, i);
        if (ts_node_type(child) == std::string_view("parameter_list")) {
            analyze_parameters(child, info);
            break;  // parameter_list は1つだけなので見つかったら終了
        }
    }

    // 関数本体（ブロック）を解析
    TSNode body_node = ts_node_child_by_field_name(func_node, "body", 4);
    if (!ts_node_is_null(body_node)) {
        analyze_block(body_node, info);
    }

    // 構築した FunctionInfo を蓄積する
    functions_.push_back(std::move(info));
}

/// ブロック（波括弧内の文の列）を走査し、各種文を current_func に追記する。
///
/// 対応する文の種類:
///   - forEach_statement  → ForEachInfo を生成
///   - if_statement       → analyze_if() に委譲（consequence ブロックも再帰解析）
///   - while_statement    → analyze_while() に委譲（body も再帰解析）
///   - variable_declaration → analyze_variable() に委譲
///
/// 【注意】
///   TSNode は Tree-sitter が管理するポインタ的なものなので、
///   TSNode{} でゼロ初期化すれば「無効なノード」として安全に扱える。
void Analyzer::analyze_block(TSNode block_node, FunctionInfo& current_func) {
    uint32_t count = ts_node_named_child_count(block_node);

    for (uint32_t i = 0; i < count; ++i) {
        TSNode statement = ts_node_named_child(block_node, i);
        std::string_view type = ts_node_type(statement);

        if (type == "forEach_statement") {
            ForEachInfo loop_info;

            // "target" フィールドから対象エンティティ名を取得（例: "Monster"）
            TSNode target_node = ts_node_child_by_field_name(statement, "target", 6);
            loop_info.target_entity_id = get_node_string_id(target_node);

            // "condition" フィールドが存在すれば条件式を取得（where 節）
            TSNode condition_node = ts_node_child_by_field_name(statement, "condition", 9);
            if (!ts_node_is_null(condition_node)) {
                loop_info.condition_id = get_node_string_id(condition_node);
                loop_info.condition_node = condition_node;
            } else {
                // 条件なし forEach（全エンティティを対象にするケース）
                loop_info.condition_id = kInvalidStringID;
                loop_info.condition_node = TSNode{}; // 空の波括弧で安全にゼロ初期化
            }
            current_func.for_each_loops.push_back(std::move(loop_info));

        } else if (type == "if_statement") {
            analyze_if(statement, current_func);

        } else if (type == "while_statement") {
            analyze_while(statement, current_func);

        } else if (type == "variable_declaration") {
            analyze_variable(statement, current_func);
        }
        // それ以外のノード（コメント等）は無視する
        else if (type == "expression_statement") {
            TSNode expr = ts_node_named_child(statement, 0);
            if (std::string_view(ts_node_type(expr)) == "assignment_expression") {
                TSNode left_node = ts_node_child_by_field_name(expr, "left", 4);
                TSNode right_node = ts_node_child_by_field_name(expr, "right", 5);

                AssignmentInfo assign_info;
                assign_info.left_node = left_node;
                assign_info.right_node = right_node;

                current_func.assignments.push_back(assign_info);
            }
        }
    }
}

/// variable_declaration ノードから VariableInfo を構築して current_func.variables に追加する。
///
/// 処理内容:
///   1. 最初の子ノード（"var" or "val" キーワード）で可変性を判定
///   2. "name" フィールドで変数名を取得
///   3. "type" フィールドで型名を取得（省略可能。なければ kInvalidStringID）
///   4. "value" フィールドで右辺の AST ノードを保存（型推論で後で使う）
void Analyzer::analyze_variable(TSNode var_node, FunctionInfo& current_func) {
    VariableInfo info;

    // 0番目の子ノードはキーワード "var" か "val"。
    // ts_node_child()（名前なしも含む）を使ってキーワードノードを取得する。
    TSNode kind_node = ts_node_child(var_node, 0);
    info.is_mutable = (ts_node_type(kind_node) == std::string_view("var"));

    // 変数名（"name" フィールド、4は文字数）
    TSNode name_node = ts_node_child_by_field_name(var_node, "name", 4);
    info.name_id = get_node_string_id(name_node);

    // 型注釈（"type" フィールド）。省略されている場合は null ノードが返る。
    TSNode type_node = ts_node_child_by_field_name(var_node, "type", 4);
    if (!ts_node_is_null(type_node)) {
        info.type_id = get_node_string_id(type_node);
    } else {
        info.type_id = kInvalidStringID;  // 型省略 → 型推論が必要
    }

    // 右辺の値ノードを保存（"value" フィールド、5は文字数）
    // TypeChecker での型推論実装時に使う予定（現時点では TODO）
    info.value_node = ts_node_child_by_field_name(var_node, "value", 5);

    current_func.variables.push_back(std::move(info));
}

/// if_statement ノードを解析して IfInfo を追加し、consequence ブロックを再帰解析する。
///
/// 【注意】
///   condition は変数名/識別子のみを想定した簡易実装。
///   複雑な式（x > 0 等）の解析は将来の課題。
void Analyzer::analyze_if(TSNode if_node, FunctionInfo& current_func) {
    IfInfo if_info;

    // "condition" フィールドで条件式を取得（9は "condition" の文字数）
    TSNode condition_node = ts_node_child_by_field_name(if_node, "condition", 9);
    if (!ts_node_is_null(condition_node)) {
        if_info.condition_id = get_node_string_id(condition_node);
    } else {
        if_info.condition_id = kInvalidStringID;
    }

    current_func.if_statements.push_back(std::move(if_info));

    // then ブロック（"consequence" フィールド、11は文字数）を再帰解析
    // ネストした変数宣言・ループなども current_func に追加される
    TSNode consequence_node = ts_node_child_by_field_name(if_node, "consequence", 11);
    if (!ts_node_is_null(consequence_node)) {
        analyze_block(consequence_node, current_func);
    }
}

/// while_statement ノードを解析して WhileInfo を追加し、body ブロックを再帰解析する。
void Analyzer::analyze_while(TSNode while_node, FunctionInfo& current_func) {
    WhileInfo info;

    // "condition" フィールドで条件式を取得
    TSNode cond_node = ts_node_child_by_field_name(while_node, "condition", 9);
    if (!ts_node_is_null(cond_node)) {
        info.condition_id = get_node_string_id(cond_node);
    } else {
        info.condition_id = kInvalidStringID;
    }

    current_func.while_loops.push_back(std::move(info));

    // ループ本体のブロックを再帰解析
    TSNode body_node = ts_node_child_by_field_name(while_node, "body", 4);
    if (!ts_node_is_null(body_node)) {
        analyze_block(body_node, current_func);
    }
}

/// parameter_list ノードを走査し、各 parameter ノードから ParameterInfo を生成する。
///
/// 各 parameter は "name" フィールドと "type" フィールドを持つ。
/// 型のない引数は現時点では想定していない。
void Analyzer::analyze_parameters(TSNode params_node, FunctionInfo& current_func) {
    uint32_t count = ts_node_named_child_count(params_node);

    for (uint32_t i = 0; i < count; ++i) {
        TSNode param_node = ts_node_named_child(params_node, i);

        // "parameter" 種別のノードだけを処理（区切り文字等は無視）
        if (ts_node_type(param_node) == std::string_view("parameter")) {
            ParameterInfo param_info;

            TSNode name_node = ts_node_child_by_field_name(param_node, "name", 4);
            param_info.name_id = get_node_string_id(name_node);

            TSNode type_node = ts_node_child_by_field_name(param_node, "type", 4);
            param_info.type_id = get_node_string_id(type_node);

            current_func.parameters.push_back(param_info);
        }
    }
}

} // namespace axiom