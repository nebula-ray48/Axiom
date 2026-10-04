// core/include/axiom/type_checker.h
//
// 【役割】
//   Analyzer が抽出した FunctionInfo リストを走査し、
//   型の整合性を検証する TypeChecker クラスを定義するヘッダー。
//
// 【処理フロー】
//   1. check_all() を呼ぶ → 全関数をループ
//   2. check_function() → 戻り値型・引数・ブロック内の各要素を検査
//   3. check_variables() / check_for_each_loops() / check_if_statements() / check_while_loops()
//      がそれぞれ専門の検査を担当
//   4. エラーがあれば report_error() で errors_ に追記
//   5. check_all() が errors_.empty() を返す（エラーなし=true）
//
// 【依存関係】
//   TypeRegistry  : 「この StringID は組み込み型か？」の問い合わせ先
//   SymbolTable   : 変数の宣言・スコープ管理・ルックアップ
//   StringInterner: StringID → 文字列への逆変換（エラーメッセージ生成に使用）

#pragma once

#include <vector>
#include <string>
#include <string_view>
#include "axiom/base/compiler.h"
#include "axiom/base/registry.h"
#include "axiom/sema/type_system.h"
#include "axiom/sema/symbol_table.h"

namespace axiom {

/// 型エラーの情報を保持するフラットな構造体。
/// エラー発生時に errors_ ベクタに push_back される。
struct TypeError {
    StringID    function_name_id;  // どの関数でエラーが起きたかの StringID
    std::string message;           // 人間可読のエラーメッセージ（関数名付き）
};

/// 意味解析フェーズの型検査クラス。
///
/// コンストラクタで Analyzer の結果（FunctionInfo のリスト）と
/// TypeRegistry・StringInterner を受け取り、check_all() で検査を実行する。
///
/// 【使い方】
///   TypeChecker checker(functions, type_registry, interner);
///   if (!checker.check_all()) {
///       for (const auto& err : checker.get_errors()) { ... }
///   }
class TypeChecker {
public:
    /// @param functions     Analyzer が抽出した関数情報リストへの const 参照
    /// @param type_registry 組み込み型の問い合わせ先
    /// @param interner      StringID → 文字列の変換に使う（エラーメッセージ用）
    TypeChecker(std::string_view                 source_code,
                const std::vector<FunctionInfo>& functions,
                TypeRegistry&                    type_registry,
                StringInterner&                  interner);

    TypeChecker(const std::vector<FunctionInfo>& functions,
                TypeRegistry& type_registry,
                StringInterner& interner)
        : TypeChecker("", functions, type_registry, interner) {}

    /// 全関数の型検査を実行する。
    /// @return エラーが1つもなければ true、あれば false
    [[nodiscard]] bool check_all();

    /// 蓄積されたエラーの一覧を取得する（check_all() 後に呼ぶ）。
    [[nodiscard]] const std::vector<TypeError>& get_errors() const noexcept { return errors_; }

private:
    // --- 外部から受け取るデータ（参照のみ、所有権はなし）---
    std::string_view source_;
    const std::vector<FunctionInfo>& functions_;
    TypeRegistry&   type_registry_;
    StringInterner& interner_;

    // --- 検査中に状態が変化するデータ ---
    SymbolTable          symbol_table_;  // スコープ管理付きの変数テーブル
    std::vector<TypeError> errors_;      // 検出したエラーの蓄積先

    // --- 内部の検証ロジック ---

    /// 1つの関数全体を検査する。enter_scope/exit_scope でスコープを管理する。
    void check_function(const FunctionInfo& func);

    /// TypeError を生成して errors_ に追加するユーティリティ。
    /// メッセージには関数名を自動的にプレフィックスとして付ける。
    void report_error(StringID func_id, std::string_view message);

    /// ブロック（文の集まり）を処理する。
    /// 現在は ParameterInfo のベクタを受け取るが、将来的に StatementInfo 等に変わる可能性がある。
    void check_block(const std::vector<ParameterInfo>& statements);

    /// 変数宣言の配列を一括チェックし、シンボルテーブルにも登録する。
    void check_variables(StringID func_name, const std::vector<VariableInfo>& variables);

    /// forEach ループの配列を一括チェック（エンティティ型・条件式の型を確認）。
    void check_for_each_loops(StringID func_name, const std::vector<ForEachInfo>& loops);

    /// if 文の配列を一括チェック（条件式が bool 型かを確認）。
    void check_if_statements(StringID func_name, const std::vector<IfInfo>& if_stmts);

    /// while ループの配列を一括チェック（条件式が bool 型かを確認）。
    void check_while_loops(StringID func_name, const std::vector<WhileInfo>& while_loops);

    StringID evaluate_number(TSNode node);

    StringID evaluate_identifier(TSNode node, StringID func_name);

    StringID evaluate_binary(TSNode node, StringID func_name);

    std::string_view get_node_text(TSNode node) const;

    [[nodiscard]] StringID get_node_string_id(TSNode node);

    StringID evaluate_unary(TSNode node, StringID func_name);

    /// 式の型評価
    StringID evaluate_expression(TSNode expr_node, StringID func_name);

    void check_assignments(StringID func_name, const std::vector<AssignmentInfo>& assignments);

    void check_return_statements(StringID func_name, StringID expected_return_type, const std::vector<ReturnInfo>& return_stmts);

};

} // namespace axiom