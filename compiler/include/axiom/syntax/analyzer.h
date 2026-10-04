// core/include/axiom/analyzer.h
//
// 【役割】
//   Tree-sitter が生成した AST（抽象構文木）を走査し、
//   型検査フェーズに必要な中間データ（FunctionInfo リスト）を抽出するクラス。
//
// 【処理フロー】
//   Analyzer analyzer(source_code, interner);
//   analyzer.analyze_root(root_node);         // AST の根から走査を開始
//   auto& functions = analyzer.get_functions(); // 抽出結果を TypeChecker に渡す
//
// 【責務の分離】
//   Analyzer は「何が書いてあるか」を読み取るだけで、
//   「正しいかどうか」の判断は TypeChecker に任せる。

#pragma once

#include <tree_sitter/api.h>

#include "axiom/base/registry.h"

namespace axiom {

/// AST を走査して FunctionInfo リストを構築するクラス。
///
/// コンストラクタでソースコード文字列と StringInterner の参照を受け取り、
/// analyze_root() で AST のルートから解析を開始する。
///
/// 解析結果は get_functions() で取得できる。
class Analyzer {
public:
    /// @param source_code  Nexa ソースコード全体の文字列ビュー（ライフタイムに注意。Analyzer より長く生存させること）
    /// @param interner     文字列 → StringID の変換に使う共有インターナー
    Analyzer(std::string_view source_code, StringInterner& interner);

    /// ASTのルートノードを走査し、トップレベルの関数宣言をすべて解析する。
    /// 内部で analyze_function() を呼ぶ。
    void analyze_root(TSNode root_node);

    /// 解析結果の関数情報リストへの const 参照を返す。
    /// analyze_root() を呼んだ後に使うこと。
    [[nodiscard]] const std::vector<FunctionInfo>& get_functions() const { return functions_; }

private:
    std::string_view source_;         // ソースコード全体。バイトオフセット計算に使う
    StringInterner&  interner_;       // StringID への変換用
    std::vector<FunctionInfo> functions_;  // 解析結果の蓄積先

    /// TSNode が対応するソーステキストを StringID に変換するヘルパー。
    /// ノードが null なら kInvalidStringID を返す。
    StringID get_node_string_id(TSNode node);

    /// function_declaration ノードを解析して FunctionInfo を生成し functions_ に追加する。
    void analyze_function(TSNode func_node);

    /// ブロック（波括弧内の文の列）を走査し、forEach / if / while / 変数宣言を current_func に追加する。
    /// ネストしたブロック（if の consequence など）は再帰呼び出しで処理する。
    void analyze_block(TSNode block_node, FunctionInfo& current_func);

    /// parameter_list ノードを走査して ParameterInfo を current_func.parameters に追加する。
    void analyze_parameters(TSNode params_node, FunctionInfo& current_func);

    /// if_statement ノードを解析して IfInfo を追加し、consequence ブロックを再帰解析する。
    void analyze_if(TSNode if_node, FunctionInfo& current_func);

    /// while_statement ノードを解析して WhileInfo を追加し、body ブロックを再帰解析する。
    void analyze_while(TSNode while_node, FunctionInfo& current_func);

    /// variable_declaration ノードを解析して VariableInfo を current_func.variables に追加する。
    void analyze_variable(TSNode var_node, FunctionInfo& current_func);
};

}  // namespace axiom