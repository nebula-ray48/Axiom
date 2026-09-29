// core/include/axiom/registry.h
//
// 【役割】
//   Analyzer が Tree-sitter の AST から抽出した構文情報を保持する
//   「中間データ構造」を定義するヘッダー。
//
//   AST ノードを直接持ち回すのではなく、必要な情報だけを
//   StringID（整数）として抽出することで、後段の TypeChecker などが
//   Tree-sitter API に依存せず処理できるようになる。
//
// 【データフローでの位置付け】
//   Nexa ソース
//     → Tree-sitter パース
//     → Analyzer (analyzer.cpp)       ← ASTを走査してここで定義した構造体に変換
//     → TypeChecker (type_checker.cpp) ← FunctionInfo を使って型検査
//     → (将来) コード生成

#pragma once

#include <cstdint>
#include <vector>
#include <string_view>
#include <tree_sitter/api.h>

#include "compiler.h"

namespace axiom {

/// 関数の仮引数1つ分の情報。
/// 例: `(hp: int32)` → name_id="hp", type_id="int32"
struct ParameterInfo {
    StringID name_id;  // 引数名のStringID
    StringID type_id;  // 型名のStringID
};

/// 変数宣言1つ分の情報。
///
/// 例:
///   `val speed: float32 = 1.5` → name_id="speed", type_id="float32", is_mutable=false
///   `var count = 0`            → name_id="count",  type_id=kInvalidStringID, is_mutable=true
///
/// type_id が kInvalidStringID の場合は型が省略されており、型推論が必要。
/// has_explicit_type() でその確認ができる。
///
/// value_node は右辺の AST ノード。型推論（TODO）で使う予定。
struct VariableInfo {
    StringID name_id;   // 変数名
    StringID type_id;   // 型名（省略可能。省略時は kInvalidStringID）
    bool is_mutable;    // var なら true, val なら false
    TSNode value_node {};  // 右辺の AST ノード（型推論のために保持）

    /// 型が明示されているか？ kInvalidStringID でなければ true。
    [[nodiscard]] bool has_explicit_type() const noexcept { return is_valid(type_id); }
};

/// if 文1つ分の情報。
/// 現時点では条件式の変数名(StringID)のみ保持する簡易実装。
struct IfInfo {
    StringID condition_id;  // 条件式に使われている変数名のID
};

/// forEach ループ1つ分の情報。
///
/// 例: `forEach Monster where is_active { ... }`
///   target_entity_id = "Monster"のID
///   condition_id     = "is_active"のID（省略時は kInvalidStringID）
///
/// has_condition() で条件式があるかを確認できる。
struct ForEachInfo {
    StringID target_entity_id; // 対象エンティティ（例: "Monster" のID）
    StringID condition_id;     // 条件式フラグ（例: "is_active" のID、無ければ kInvalidStringID）
    TSNode condition_node;     // 条件式の AST ノード（詳細解析用。将来の型推論に使う）

    /// 条件式が指定されているかどうかを判定する。
    [[nodiscard]] bool has_condition() const noexcept {
        return is_valid(condition_id);
    }
};

/// while ループ1つ分の情報。
struct WhileInfo {
    StringID condition_id;  // 条件式に使われている変数名のID
};

/// 関数宣言1つ分の情報。Analyzer が抽出し TypeChecker に渡す主要な中間データ。
///
/// 例:
///   fn update(dt: float32) -> void {
///       val speed: float32 = 1.0
///       forEach Monster where is_active { ... }
///   }
///
/// 各フィールドは Analyzer::analyze_function() / analyze_block() で収集される。
struct FunctionInfo {
    StringID name_id;         // 関数名のID（例: "update"）
    StringID return_type_id;  // 戻り値型のID（例: "void"）

    std::vector<ParameterInfo> parameters;      // 仮引数リスト
    std::vector<ForEachInfo>   for_each_loops;  // 関数内の forEach 一覧
    std::vector<IfInfo>        if_statements;   // 関数内の if 文一覧
    std::vector<WhileInfo>     while_loops;     // 関数内の while 一覧
    std::vector<VariableInfo>  variables;       // 関数内の変数宣言一覧
};

} // namespace axiom