// core/src/type_checker.cpp
//
// 【役割】
//   TypeChecker クラスの実装。
//   Analyzer が抽出した FunctionInfo リストを走査し、
//   型の整合性を検証する（型検査フェーズ）。
//
// 【全体フロー】
//   check_all()
//     └─ check_function(func)          ← 関数ごとに呼ばれる
//           ├─ 戻り値型が組み込み型か確認
//           ├─ enter_scope()           ← 関数スコープ開始
//           ├─ 引数型をチェック → SymbolTable に登録
//           ├─ check_variables()       ← 変数宣言をチェック
//           ├─ check_for_each_loops()  ← forEach をチェック
//           ├─ check_if_statements()   ← if をチェック
//           ├─ check_while_loops()     ← while をチェック
//           └─ exit_scope()            ← 関数スコープ終了
//
// 【エラー報告の方針】
//   エラーが出ても即中断せず、全関数を最後まで検査する。
//   errors_ にすべてのエラーを蓄積し、check_all() の戻り値で成否を伝える。

#include "axiom/sema/type_checker.h"

namespace axiom {

TypeChecker::TypeChecker(std::string_view source_code,
                         const std::vector<FunctionInfo>& functions,
                         TypeRegistry& type_registry,
                         StringInterner& interner)
    : source_(source_code), functions_(functions), type_registry_(type_registry), interner_(interner) {}


/// エラーを errors_ に追記する内部ユーティリティ。
/// func_id から関数名を逆引きしてメッセージのプレフィックスにする。
/// 例: func_id="update", message="Unknown return type"
///     → "update: Unknown return type" として格納される
void TypeChecker::report_error(StringID func_id, std::string_view message) {

    std::string func_name = std::string(interner_.GetString(func_id));
    std::string full_message = func_name + ": " + std::string(message);

    errors_.push_back(TypeError{ .function_name_id=func_id, .message=full_message });
}

void TypeChecker::register_function_signature(const FunctionInfo& func) {

    if (function_signatures_.contains(func.name_id)) {
        report_error(func.name_id, "Duplicate function declaration");
        return;
    }

    std::vector<StringID> param_types;
    param_types.reserve(func.parameters.size());
    for (const auto& param : func.parameters) {
        param_types.push_back(param.type_id);
    }
    // 3. マップに保存する
    function_signatures_[func.name_id] = TypeRegistry::FunctionSignature{
        .return_type_id = func.return_type_id,
        .parameter_types = std::move(param_types)
    };

}

/// 全関数を型検査する。エラーが1つもなければ true を返す。
bool TypeChecker::check_all() {

    for (const auto& func : functions_) {
        register_function_signature(func);
    }

    for (const auto& func : functions_) {
        check_function(func);
    }

    return errors_.empty();
}

StringID TypeChecker::get_node_string_id(TSNode node) {
    if (ts_node_is_null(node)) return kInvalidStringID;
    return interner_.Intern(get_node_text(node));
}

/// 1つの関数を型検査する。
///
/// 手順:
///   1. 戻り値型が組み込み型として認識できるか確認
///   2. enter_scope() で関数スコープを開始
///   3. 引数の型を確認し、SymbolTable に登録（重複引数名もここで検出）
///   4. 変数・ループ・条件分岐を各サブ関数でチェック
///   5. exit_scope() でスコープを閉じる（関数内の変数をすべて消去）
void TypeChecker::check_function(const FunctionInfo& func) {
    // 戻り値型が未知の型ではないかチェック（void は現時点では組み込み型に含まれていない点に注意）
    if (!type_registry_.is_builtin(func.return_type_id)) {
        report_error(func.name_id, "Unknown return type");
    }

    symbol_table_.enter_scope(); // 関数のスコープに入る

    // 1. 引数の登録
    for (const auto& param : func.parameters) {
        // 引数の型が組み込み型でなければエラー
        if (!type_registry_.is_builtin(param.type_id)) {
            report_error(func.name_id, "Unknown parameter type");
        }
        // SymbolTable に登録。引数は val 扱い（is_mutable=false）
        bool success = symbol_table_.declare(param.name_id, param.type_id, false);
        if (!success) {
            report_error(func.name_id, "Duplicate parameter name");
        }
    }

    // 2. 変数宣言の一括検査
    check_variables(func.name_id, func.variables);

    // 3. ループや条件分岐の一括検査（今回は空の枠組みだけ作ります）
    check_for_each_loops(func.name_id, func.for_each_loops);
    check_if_statements(func.name_id, func.if_statements);
    check_while_loops(func.name_id, func.while_loops);
    check_assignments(func.name_id, func.assignments);
    check_return_statements(func.name_id, func.return_type_id, func.return_statements);

    symbol_table_.exit_scope(); // 関数のスコープから出る
}

/// 変数宣言リストを型検査し、SymbolTable に登録する。
///
/// 各変数について:
///   - 型が明示されている場合: TypeRegistry で既知の型かチェック
///   - 型が省略されている場合: 型推論が必要（現在は TODO）
///   - SymbolTable に declare して、同スコープ内の重複名もチェック
void TypeChecker::check_variables(StringID func_name, const std::vector<VariableInfo>& variables) {
    for (const auto& var : variables) {
        StringID expr_type = kInvalidStringID;

        // 右辺ノードが存在する場合のみ式を評価
        if (var.value_node.id != nullptr && !ts_node_is_null(var.value_node)) {
            expr_type = evaluate_expression(var.value_node, func_name);
        }

        StringID final_type = var.type_id;

        if (var.has_explicit_type()) {
            if (!type_registry_.is_builtin(var.type_id)) {
                report_error(func_name, "Unknown variable type");
            } else if (is_valid(expr_type) && var.type_id != expr_type) {
                report_error(func_name, "Variable type annotation does not match initial value type");
            }
        } else {
            // 型省略時は右辺から推論
            final_type = expr_type;
        }

        bool success = symbol_table_.declare(var.name_id, final_type, var.is_mutable);
        if (!success) {
            report_error(func_name, "Duplicate variable name");
        }
    }
}

/// forEach ループリストを型検査する。
///
/// チェック内容:
///   1. target_entity_id（例: "Monster"）が TypeRegistry に存在するか
///      （現在は組み込み型のみ対応。将来はユーザー定義エンティティ型も対応予定）
///   2. 条件式が指定されている場合、その変数が SymbolTable に存在し bool 型かを確認
void TypeChecker::check_for_each_loops(StringID func_name, const std::vector<ForEachInfo>& loops) {
    for (const auto& loop : loops) {

        // 対象のエンティティ型が辞書に存在するかチェック
        // 注意: 現在は is_builtin() しか持っていないので、ユーザー定義エンティティは必ずエラーになる
        //       将来は EntityRegistry 等を参照するよう拡張が必要
        if (!type_registry_.is_builtin(loop.target_entity_id)) {
            report_error(func_name, "Unknown target entity in forEach");
        }

        // 条件フラグが指定されている場合のみチェック（has_condition() で has_condition = id != kInvalidStringID）
        if (loop.has_condition()) {
            StringID cond_type = symbol_table_.lookup(loop.condition_id);

            if (cond_type == kInvalidStringID) {
                // 条件式の変数がスコープ内に見つからない
                report_error(func_name, "Undefined variable in forEach condition");
            } else if (cond_type != type_registry_.get_bool()) {
                // 変数は存在するが bool 型ではない
                report_error(func_name, "forEach condition must be bool");
            }
        }
    }
}

/// if 文リストを型検査する。
///
/// チェック内容:
///   - 条件式として使われている変数が SymbolTable に存在するか
///   - その変数が bool 型かどうか
void TypeChecker::check_if_statements(StringID func_name, const std::vector<IfInfo>& if_stmts) {
    for (const auto& stmt : if_stmts) {
        // SymbolTable で条件式変数の型を逆引き
        StringID type_id = symbol_table_.lookup(stmt.condition_id);

        if (type_id == kInvalidStringID) {
            report_error(func_name, "Undefined variable in if condition");
        } else if (type_id != type_registry_.get_bool()) {
            report_error(func_name, "If condition must be bool");
        }
    }
}

/// while ループリストを型検査する。
///
/// チェック内容は check_if_statements() と同様:
///   - 条件式として使われている変数が SymbolTable に存在するか
///   - その変数が bool 型かどうか
void TypeChecker::check_while_loops(StringID func_name, const std::vector<WhileInfo>& while_loops) {
    for (const auto& loop : while_loops) {
        StringID type_id = symbol_table_.lookup(loop.condition_id);

        if (type_id == kInvalidStringID) {
            report_error(func_name, "Undefined variable in while condition");
        } else if (type_id != type_registry_.get_bool()) {
            report_error(func_name, "While condition must be bool");
        }
    }
}

std::string_view TypeChecker::get_node_text(TSNode node) const {
    if (ts_node_is_null(node)) return "";
    uint32_t start = ts_node_start_byte(node);
    uint32_t end   = ts_node_end_byte(node);
    return source_.substr(start, end - start);
}


StringID TypeChecker::evaluate_number(TSNode node) {
    std::string_view text = get_node_text(node); // ノードの文字列を取得
    if (text.find('.') != std::string_view::npos) {
        return interner_.Intern("float32");
    }
    return interner_.Intern("int32");
}

StringID TypeChecker::evaluate_identifier(TSNode node, StringID func_name) {
    std::string_view name = get_node_text(node);

    // true / false リテラルのサポート
    if (name == "true" || name == "false") {
        return interner_.Intern("bool");
    }

    StringID var_name_id = get_node_string_id(node);
    StringID var_type_id = symbol_table_.lookup(var_name_id);

    if (!is_valid(var_type_id)) {
        report_error(func_name, "Undefined variable: " + std::string(interner_.GetString(var_name_id)));
        return kInvalidStringID;
    }
    return var_type_id;
}

StringID TypeChecker::evaluate_binary(TSNode node, StringID func_name) {
    // Tree-sitter の binary_expression は通常 [左辺, 演算子, 右辺] の3つの子を持つ
    TSNode left_node  = ts_node_child(node, 0);
    TSNode op_node    = ts_node_child(node, 1);
    TSNode right_node = ts_node_child(node, 2);

    StringID left_type  = evaluate_expression(left_node, func_name);
    StringID right_type = evaluate_expression(right_node, func_name);

    // エラーがすでに起きていれば早期リターン
    if (!is_valid(left_type) || !is_valid(right_type)) {
        return kInvalidStringID;
    }

    std::string_view op = get_node_text(op_node);

    // 算術演算 (+, -, *, /)
    if (op == "+" || op == "-" || op == "*" || op == "/") {
        if (left_type != right_type) {
            report_error(func_name, "Type mismatch in arithmetic operation");
            return kInvalidStringID;
        }
        return left_type; // 例: int32 + int32 -> int32
    }
    // 比較演算 (==, !=, <, >, <=, >=)
    else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
        if (left_type != right_type) {
            report_error(func_name, "Type mismatch in comparison");
            return kInvalidStringID;
        }
        return interner_.Intern("bool");
    }
    // 論理演算 (and, or)
    else if (op == "and" || op == "or") {
        StringID bool_id = interner_.Intern("bool");
        if (left_type != bool_id || right_type != bool_id) {
            report_error(func_name, "Operands of 'and' / 'or' must be bool");
            return kInvalidStringID;
        }
        return bool_id;
    }

    return kInvalidStringID;
}

void TypeChecker::check_assignments(StringID func_name, const std::vector<AssignmentInfo>& assignments) {
    for (const auto assign : assignments) {
        StringID var_name_id = get_node_string_id(assign.left_node);

        const Symbol* sym = symbol_table_.lookup_symbol(var_name_id);
        if (!sym) {
            report_error(func_name, "Undefined variable: " + std::string(interner_.GetString(var_name_id)));
            continue;
        }

        if (!sym->is_mutable) {
            report_error(func_name, "Cannot assign to immutable variable: " + std::string(interner_.GetString(var_name_id)));
        }

        StringID right_type = evaluate_expression(assign.right_node, func_name);
        if (is_valid(right_type) && sym->type_id != right_type) {
            report_error(func_name, "Type mismatch in assignment");
        }
    }
}

void TypeChecker::check_return_statements(StringID                       func_name,
                                          StringID                       expected_return_type,
                                          const std::vector<ReturnInfo>& return_stmts) {

    bool is_void = (expected_return_type == interner_.Intern("void"))  || !is_valid(expected_return_type);

    for (const auto& ret : return_stmts) {

        if (is_void) {
            if (ret.has_value()) {
                report_error(func_name, "Cannot return a value from void function");
            }

        } else {
            if (!ret.has_value()) {
                report_error(func_name, "Non-void function must return a value");
            } else {
                // 2. 値があるなら型を計算してチェック
                StringID actual_type = evaluate_expression(ret.value_node, func_name);
                if (is_valid(actual_type) && actual_type != expected_return_type) {
                    report_error(func_name, "Return type mismatch");
                }
            }
        }
    }

}

StringID TypeChecker::evaluate_expression(TSNode expr_node, StringID func_name) {
    // 1. nullノードのガード
    if (ts_node_is_null(expr_node)) {
        return kInvalidStringID;
    }

    // 2. ノードの種類を取得
    std::string_view node_type = ts_node_type(expr_node);

    // 3. 種類ごとに処理を振り分け（ディスパッチ）
    if (node_type == "number") {
        // 数値リテラル (例: 10, 3.14)
        return evaluate_number(expr_node);
    }
    else if (node_type == "identifier") {
        // 変数参照 (例: speed, hp)
        return evaluate_identifier(expr_node, func_name);
    }
    else if (node_type == "binary_expression") {
        // 二項演算 (例: a + b, x > 0)
        return evaluate_binary(expr_node, func_name);
    }
    else if (node_type == "unary_expression") {
        // 単項演算 (例: not is_active)
        return evaluate_unary(expr_node, func_name);
    } else if (node_type == "call_expression") {
        return evaluate_call(expr_node, func_name);
    }

    // 未対応の式ノードの場合
    report_error(func_name, "Unsupported expression type");
    return kInvalidStringID;
}

StringID TypeChecker::evaluate_unary(TSNode node, StringID func_name) {
    // unary_expression は [ "not", オペランド ]
    TSNode operand_node = ts_node_child(node, 1);
    StringID operand_type = evaluate_expression(operand_node, func_name);
    if (!is_valid(operand_type)) {
        return kInvalidStringID;
    }
    StringID bool_id = interner_.Intern("bool");
    if (operand_type != bool_id) {
        report_error(func_name, "Operand of 'not' must be bool");
        return kInvalidStringID;
    }
    return bool_id;
}

StringID TypeChecker::evaluate_call(TSNode node, StringID func_name) {
    // 1. 呼び出し先関数名を取得して辞書を検索
    TSNode fn_node = ts_node_child_by_field_name(node, "function", 8);
    StringID target_func_id = get_node_string_id(fn_node);

    auto it = function_signatures_.find(target_func_id);
    if (it == function_signatures_.end()) {
        report_error(func_name, "Undefined function: " + std::string(interner_.GetString(target_func_id)));
        return kInvalidStringID;
    }

    const auto& sig = it->second;
    // 2. 引数リストと個数のチェック
    TSNode args_node = ts_node_child_by_field_name(node, "arguments", 9);
    uint32_t actual_arg_count = ts_node_named_child_count(args_node);
    uint32_t expected_arg_count = static_cast<uint32_t>(sig.parameter_types.size());
    if (actual_arg_count != expected_arg_count) {
        report_error(func_name, "Function argument count mismatch");
        return kInvalidStringID;
    }
    // 3. 各引数の型チェック
    bool has_error = false;
    for (uint32_t i = 0; i < actual_arg_count; ++i) {
        TSNode arg_node = ts_node_named_child(args_node, i);
        StringID actual_type = evaluate_expression(arg_node, func_name);
        StringID expected_type = sig.parameter_types[i];
        if (!is_valid(actual_type) || actual_type != expected_type) {
            report_error(func_name, "Function argument type mismatch");
            has_error = true;
        }
    }
    if (has_error) {
        return kInvalidStringID;
    }
    // 4. 成功！関数の戻り値型を返す
    return sig.return_type_id;
};

} // namespace axiom