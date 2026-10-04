#include "axiom/syntax/analyzer.h"
#include "axiom/base/registry.h"
#include "axiom/sema/symbol_table.h"
#include "axiom/sema/type_system.h"
#include "axiom/sema/type_checker.h"

#include <gtest/gtest.h>

using namespace axiom;

void ParseAndAnalyze(std::string_view code, StringInterner& interner, VariableRegistry_DOD& var_reg, ComponentRegistry_DOD& comp_reg) {
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(
        parser,
        nullptr,
        code.data(),
        static_cast<uint32_t>(code.length())
    );

    TSNode root_node = ts_tree_root_node(tree);

    TreeSitterSymbols symbols;
    symbols.Initialize(tree_sitter_nexa());

    AnalyzeAST(root_node, code, interner, var_reg, comp_reg, symbols);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 1. 変数解析のテスト
TEST(AnalyzerTest, ParseVariable) {
    StringInterner interner;
    VariableRegistry_DOD var_reg;
    ComponentRegistry_DOD comp_reg;

    ParseAndAnalyze("val hp: int32 = 100;", interner, var_reg, comp_reg);

    // 抽出された変数が1つだけであることを保証
    ASSERT_EQ(var_reg.names.size(), 1);

    // 名前、型、初期値が正しくDODメモリに格納されていることを検証
    EXPECT_EQ(interner.GetString(var_reg.names[0]), "hp");
    EXPECT_EQ(interner.GetString(var_reg.types[0]), "int32");
    EXPECT_EQ(interner.GetString(var_reg.initial_values[0]), "100");
}

// 2. コンポーネント解析のテスト
TEST(AnalyzerTest, ParseComponent) {
    StringInterner interner;
    VariableRegistry_DOD var_reg;
    ComponentRegistry_DOD comp_reg;

    ParseAndAnalyze("component Position { x: float32, y: float32 }", interner, var_reg, comp_reg);

    // 抽出されたコンポーネントが1つであることを保証
    ASSERT_EQ(comp_reg.names.size(), 1);
    EXPECT_EQ(interner.GetString(comp_reg.names[0]), "Position");

    // フィールドが2つ抽出されていることを保証
    ASSERT_EQ(comp_reg.field_counts[0], 2);

    // フラットなSoA配列からフィールド情報を検証
    uint32_t start = comp_reg.field_starts[0];

    EXPECT_EQ(interner.GetString(comp_reg.field_names[start + 0]), "x");
    EXPECT_EQ(interner.GetString(comp_reg.field_types[start + 0]), "float32");

    EXPECT_EQ(interner.GetString(comp_reg.field_names[start + 1]), "y");
    EXPECT_EQ(interner.GetString(comp_reg.field_types[start + 1]), "float32");
}

TEST(AnalyzerTest, ParseFunctionAndForEach) {
    // テスト用のNexaコード
    // 関数宣言と、その中にある forEach 文を定義
    const char* source = R"(
        pub fun update_monsters(delta_time: float32) {
            forEach Monster where is_active {
                Position.x = 1.0;
            }
        }
    )";

    // パーサーのセットアップ
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());
    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);


    // 1. ルート直下の最初のノードが `function_declaration` であることを確認
    TSNode func_node = ts_node_named_child(root_node, 0);
    ASSERT_FALSE(ts_node_is_null(func_node));
    EXPECT_STREQ(ts_node_type(func_node), "function_declaration");

    // 2. 関数の名前 (`name` フィールド) が "update_monsters" であるか確認
    TSNode func_name_node = ts_node_child_by_field_name(func_node, "name", 4);
    ASSERT_FALSE(ts_node_is_null(func_name_node));
    uint32_t name_start = ts_node_start_byte(func_name_node);
    uint32_t name_end = ts_node_end_byte(func_name_node);
    std::string func_name(source + name_start, name_end - name_start);
    EXPECT_EQ(func_name, "update_monsters");

    // 3. 関数のボディ (`block`) を取得
    TSNode body_node = ts_node_child_by_field_name(func_node, "body", 4);
    ASSERT_FALSE(ts_node_is_null(body_node));

    // 4. ボディの中の最初の文が `forEach_statement` であることを確認
    TSNode foreach_node = ts_node_named_child(body_node, 0);
    ASSERT_FALSE(ts_node_is_null(foreach_node));
    EXPECT_STREQ(ts_node_type(foreach_node), "forEach_statement");

    // 5. forEach のターゲット (`target` フィールド) が "Monster" であるか確認
    TSNode target_node = ts_node_child_by_field_name(foreach_node, "target", 6);
    ASSERT_FALSE(ts_node_is_null(target_node));
    uint32_t target_start = ts_node_start_byte(target_node);
    uint32_t target_end = ts_node_end_byte(target_node);
    std::string target_name(source + target_start, target_end - target_start);
    EXPECT_EQ(target_name, "Monster");

    // 6. forEach の条件 (`condition` フィールド) が "is_active" であるか確認
    TSNode condition_node = ts_node_child_by_field_name(foreach_node, "condition", 9);
    ASSERT_FALSE(ts_node_is_null(condition_node));
    uint32_t cond_start = ts_node_start_byte(condition_node);
    uint32_t cond_end = ts_node_end_byte(condition_node);
    std::string cond_name(source + cond_start, cond_end - cond_start);
    EXPECT_EQ(cond_name, "is_active");

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

TEST(AnalyzerTest, ExtractFunctionAndForEach) {
    // テスト用のNexaコード
    const char* source = R"(
        pub fun update_monsters(delta_time: float32) {
            forEach Monster where is_active {
                Position.x = 1.0;
            }
        }
    )";

    // 1. パーサーと Interner の準備
    axiom::StringInterner interner;
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    uint32_t source_length = static_cast<uint32_t>(strlen(source));
    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, source_length);
    TSNode root_node = ts_tree_root_node(tree);

    char* tree_str = ts_node_string(root_node);
    std::cout << "\n==== AST DUMP ====\n" << tree_str << "\n==================\n" << std::endl;
    free(tree_str);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    const auto& functions = analyzer.get_functions();

    // 関数が1つだけ見つかっているか？
    ASSERT_EQ(functions.size(), 1);
    const auto& func = functions[0];

    // 関数の名前が "update_monsters" になっているか？
    // IDを Interner に渡して文字列に戻して確認する
    EXPECT_EQ(interner.GetString(func.name_id), "update_monsters");

    ASSERT_EQ(func.parameters.size(), 1);
    const auto& param = func.parameters[0];
    EXPECT_EQ(interner.GetString(param.name_id), "delta_time");
    EXPECT_EQ(interner.GetString(param.type_id), "float32");

    // forEach ループが1つ見つかっているか？
    ASSERT_EQ(func.for_each_loops.size(), 1);
    const auto& loop = func.for_each_loops[0];

    // ループの対象（ターゲット）が "Monster" か？
    EXPECT_EQ(interner.GetString(loop.target_entity_id), "Monster");

    // 条件（where）がちゃんと存在しているか？
    EXPECT_TRUE(loop.has_condition());

    // 条件の文字が "is_active" か？
    EXPECT_EQ(interner.GetString(loop.condition_id), "is_active");

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

TEST(AnalyzerTest, ExtractIfStatement) {
    const char* source = R"(
        fun check_status(is_alive: bool) {
            if is_alive {
                forEach Monster where is_active {
                    Position.x = 1.0;
                }
            }
        }
    )";

    axiom::StringInterner interner;
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    const auto& functions = analyzer.get_functions();
    ASSERT_EQ(functions.size(), 1);
    const auto& func = functions[0];

    // if文が抽出できているか
    ASSERT_EQ(func.if_statements.size(), 1);
    EXPECT_EQ(interner.GetString(func.if_statements[0].condition_id), "is_alive");

    // ifブロック内のforEachも再帰的に拾えているか
    ASSERT_EQ(func.for_each_loops.size(), 1);
    EXPECT_EQ(interner.GetString(func.for_each_loops[0].target_entity_id), "Monster");

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

TEST(AnalyzerTest, ExtractWhileStatement) {
    const char* source = R"(
        pub fun wait_for_ready() {
            while is_waiting {
                if ready_flag {
                    is_waiting = 0;
                }
            }
        }
    )";

    axiom::StringInterner interner;
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    const auto& functions = analyzer.get_functions();
    ASSERT_EQ(functions.size(), 1);
    const auto& func = functions[0];

    // whileループが抽出できているか
    ASSERT_EQ(func.while_loops.size(), 1);
    EXPECT_EQ(interner.GetString(func.while_loops[0].condition_id), "is_waiting");

    // whileの中のif文も再帰的に拾えているか
    ASSERT_EQ(func.if_statements.size(), 1);
    EXPECT_EQ(interner.GetString(func.if_statements[0].condition_id), "ready_flag");

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

TEST(AnalyzerTest, ExtractVariableDeclaration) {
    const char* source = R"(
        pub fun setup_player() {
            val max_health: float32 = 100.0;
            var current_state = 1;
        }
    )";

    axiom::StringInterner interner;
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    const auto& functions = analyzer.get_functions();
    ASSERT_EQ(functions.size(), 1);
    const auto& func = functions[0];

    // 変数が2つ抽出できているか
    ASSERT_EQ(func.variables.size(), 2);

    // 1つ目: val max_health: float32 = 100.0;
    const auto& var1 = func.variables[0];
    EXPECT_EQ(interner.GetString(var1.name_id), "max_health");
    EXPECT_TRUE(var1.has_explicit_type());
    EXPECT_EQ(interner.GetString(var1.type_id), "float32");
    EXPECT_FALSE(var1.is_mutable);
    EXPECT_FALSE(ts_node_is_null(var1.value_node));

    // 2つ目: var current_state = 1;
    const auto& var2 = func.variables[1];
    EXPECT_EQ(interner.GetString(var2.name_id), "current_state");
    EXPECT_FALSE(var2.has_explicit_type()); // 型指定がない
    EXPECT_TRUE(var2.is_mutable);
    EXPECT_FALSE(ts_node_is_null(var2.value_node));

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

TEST(TypeSystemTest, BuiltinTypeRegistration) {
    axiom::StringInterner interner;
    axiom::TypeRegistry type_registry(interner);

    // float32 や bool という文字列をIDに変換してみる
    axiom::StringID float32_id = interner.Intern("float32");
    axiom::StringID bool_id = interner.Intern("bool");
    axiom::StringID unknown_id = interner.Intern("Monster"); // 組み込み型ではない適当な名前

    // TypeRegistryが正しくIDを保持し、組み込み型として判定できるか確認
    EXPECT_TRUE(type_registry.is_builtin(float32_id));
    EXPECT_TRUE(type_registry.is_builtin(bool_id));

    // ユーザー定義の型は組み込み型ではないと判定されるか確認
    EXPECT_FALSE(type_registry.is_builtin(unknown_id));

    // ゲッター経由で取得したIDが一致するか確認
    EXPECT_EQ(type_registry.get_float32(), float32_id);
    EXPECT_EQ(type_registry.get_bool(), bool_id);
}

TEST(SymbolTableTest, BasicDeclarationAndLookup) {
    axiom::StringInterner interner;
    axiom::SymbolTable table;

    axiom::StringID var_x = interner.Intern("x");
    axiom::StringID type_i32 = interner.Intern("int32");

    // 未定義の変数は検索失敗する
    EXPECT_EQ(table.lookup(var_x), axiom::kInvalidStringID);

    // 変数登録と検索
    EXPECT_TRUE(table.declare(var_x, type_i32, /*is_mutable=*/false));
    EXPECT_EQ(table.lookup(var_x), type_i32);

    // 同一スコープでの同名定義は弾かれる
    EXPECT_FALSE(table.declare(var_x, type_i32, /*is_mutable=*/true));
}

TEST(SymbolTableTest, ScopeNestingAndShadowing) {
    axiom::StringInterner interner;
    axiom::SymbolTable table;

    axiom::StringID var_x = interner.Intern("x");
    axiom::StringID var_y = interner.Intern("y");
    axiom::StringID type_i32 = interner.Intern("int32");
    axiom::StringID type_f32 = interner.Intern("float32");

    // グローバル / 最外周スコープ
    EXPECT_TRUE(table.declare(var_x, type_i32, false));

    // 内側スコープへ突入
    table.enter_scope();
    {
        EXPECT_TRUE(table.declare(var_y, type_f32, true));
        // 外側の変数は内側からも見える
        EXPECT_EQ(table.lookup(var_x), type_i32);
        EXPECT_EQ(table.lookup(var_y), type_f32);

        // 別スコープであれば同名変数を定義可能（シャドウイング）
        EXPECT_TRUE(table.declare(var_x, type_f32, false));
        // 内側では新しい型（float32）で解決される
        EXPECT_EQ(table.lookup(var_x), type_f32);
    }
    // 内側スコープを脱出
    table.exit_scope();

    // 脱出後は外側の型（int32）に復帰している
    EXPECT_EQ(table.lookup(var_x), type_i32);
    // 内側で定義された y は消滅している
    EXPECT_EQ(table.lookup(var_y), axiom::kInvalidStringID);
}

TEST(SymbolTableTest, RedundantExitScopeSafety) {
    axiom::SymbolTable table;

    // スコープが空の状態で exit_scope を呼んでもクラッシュしない
    EXPECT_NO_THROW(table.exit_scope());
}

#include "axiom/sema/type_checker.h"

// 1. 正常な関数のテスト（正しい戻り値と引数）
TEST(TypeCheckerTest, ValidFunctionSignature) {
    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);

    // 手動で正しい関数のデータを作る
    axiom::FunctionInfo func;
    func.name_id = interner.Intern("valid_func");
    func.return_type_id = registry.get_int32(); // 組み込み型 int32

    axiom::ParameterInfo p1;
    p1.name_id = interner.Intern("x");
    p1.type_id = registry.get_float32(); // 組み込み型 float32
    func.parameters.push_back(p1);

    std::vector<axiom::FunctionInfo> funcs = { func };
    axiom::TypeChecker checker(funcs, registry, interner);

    // エラーがなく、true が返るはず
    EXPECT_TRUE(checker.check_all());
    EXPECT_TRUE(checker.get_errors().empty());
}

// 2. 未定義の戻り値のテスト
TEST(TypeCheckerTest, InvalidReturnType) {
    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);

    axiom::FunctionInfo func;
    func.name_id = interner.Intern("bad_return");
    func.return_type_id = interner.Intern("UnknownType"); // 存在しない型

    std::vector<axiom::FunctionInfo> funcs = { func };
    axiom::TypeChecker checker(funcs, registry, interner);

    // エラーが発生し、false が返るはず
    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 1);

    // メッセージに "Unknown return type" が含まれているか確認
    EXPECT_NE(checker.get_errors()[0].message.find("Unknown return type"), std::string::npos);
    // 先ほど追加した interner_ のおかげで関数名が含まれているかも確認
    EXPECT_NE(checker.get_errors()[0].message.find("bad_return"), std::string::npos);
}

// 3. 未定義の引数 ＆ 引数の名前被りテスト
TEST(TypeCheckerTest, InvalidAndDuplicateParameters) {
    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);

    axiom::FunctionInfo func;
    func.name_id = interner.Intern("bad_params");
    func.return_type_id = registry.get_int32();

    // 1つ目の引数: 型が存在しない
    axiom::ParameterInfo p1;
    p1.name_id = interner.Intern("x");
    p1.type_id = interner.Intern("UnknownParamType");

    // 2つ目の引数: 型は正しいが、名前 'x' が1つ目と被っている
    axiom::ParameterInfo p2;
    p2.name_id = interner.Intern("x");
    p2.type_id = registry.get_int32();

    func.parameters.push_back(p1);
    func.parameters.push_back(p2);

    std::vector<axiom::FunctionInfo> funcs = { func };
    axiom::TypeChecker checker(funcs, registry, interner);

    EXPECT_FALSE(checker.check_all());
    // エラーが2つ（型不明 ＋ 名前被り）出ているはず
    ASSERT_EQ(checker.get_errors().size(), 2);
    EXPECT_NE(checker.get_errors()[0].message.find("Unknown parameter type"), std::string::npos);
    EXPECT_NE(checker.get_errors()[1].message.find("Duplicate parameter name"), std::string::npos);
}

// 4. 関数内のローカル変数宣言のテスト
TEST(TypeCheckerTest, VariableDeclarations) {
    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);

    axiom::FunctionInfo func;
    func.name_id = interner.Intern("test_vars");
    func.return_type_id = registry.get_int32();

    // 変数1: 正しい型 (float32)
    axiom::VariableInfo v1;
    v1.name_id = interner.Intern("health");
    v1.type_id = registry.get_float32();
    v1.is_mutable = true;

    // 変数2: 存在しない型
    axiom::VariableInfo v2;
    v2.name_id = interner.Intern("magic");
    v2.type_id = interner.Intern("UnknownType");
    v2.is_mutable = false;

    // 変数3: 名前が変数1と被っている (health)
    axiom::VariableInfo v3;
    v3.name_id = interner.Intern("health");
    v3.type_id = registry.get_int32();
    v3.is_mutable = false;

    func.variables.push_back(v1);
    func.variables.push_back(v2);
    func.variables.push_back(v3);

    std::vector<axiom::FunctionInfo> funcs = { func };
    axiom::TypeChecker checker(funcs, registry, interner);

    EXPECT_FALSE(checker.check_all());

    // エラーは2つ（v2の型不明、v3の名前被り）出るはず
    ASSERT_EQ(checker.get_errors().size(), 2);
    EXPECT_NE(checker.get_errors()[0].message.find("Unknown variable type"), std::string::npos);
    EXPECT_NE(checker.get_errors()[1].message.find("Duplicate variable name"), std::string::npos);
}

// 5. If文とWhile文の条件式テスト
TEST(TypeCheckerTest, IfAndWhileConditions) {
    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);

    axiom::FunctionInfo func;
    func.name_id = interner.Intern("test_conds");
    func.return_type_id = registry.get_int32();

    // スコープに bool 型の変数 "is_active" と int32 型の "count" を登録（今回は引数として）
    axiom::ParameterInfo p1{ interner.Intern("is_active"), registry.get_bool() };
    axiom::ParameterInfo p2{ interner.Intern("count"), registry.get_int32() };
    func.parameters.push_back(p1);
    func.parameters.push_back(p2);

    // 1. If文: 条件が int32 型 (count) -> エラーになるはず
    axiom::IfInfo if_bad{ interner.Intern("count") };
    func.if_statements.push_back(if_bad);

    // 2. While文: 条件が bool 型 (is_active) -> 正常
    axiom::WhileInfo while_ok{ interner.Intern("is_active") };
    func.while_loops.push_back(while_ok);

    // 3. While文: 条件が存在しない変数 (unknown) -> エラーになるはず
    axiom::WhileInfo while_bad{ interner.Intern("unknown") };
    func.while_loops.push_back(while_bad);

    std::vector<axiom::FunctionInfo> funcs = { func };
    axiom::TypeChecker checker(funcs, registry, interner);

    EXPECT_FALSE(checker.check_all());

    // エラーは2つ（Ifの型違い、Whileの未定義変数）出るはず
    ASSERT_EQ(checker.get_errors().size(), 2);
    EXPECT_NE(checker.get_errors()[0].message.find("If condition must be bool"), std::string::npos);
    EXPECT_NE(checker.get_errors()[1].message.find("Undefined variable in while condition"), std::string::npos);
}

// 6. ForEachループのテスト
TEST(TypeCheckerTest, ForEachLoops) {
    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);

    axiom::StringID valid_entity_id = interner.Intern("string");

    axiom::FunctionInfo func;
    func.name_id = interner.Intern("test_foreach");
    func.return_type_id = registry.get_int32();

    // スコープに bool 型の "is_alive" と int32 型の "hp" を登録
    func.parameters.push_back({ interner.Intern("is_alive"), registry.get_bool() });
    func.parameters.push_back({ interner.Intern("hp"), registry.get_int32() });

    // 1. 正常なForEach: 対象が存在し(string)、条件が bool (is_alive)
    axiom::ForEachInfo loop_ok;
    loop_ok.target_entity_id = valid_entity_id;
    loop_ok.condition_id = interner.Intern("is_alive");
    func.for_each_loops.push_back(loop_ok);

    // 2. エラー: 存在しない対象エンティティ ("Ghost")
    axiom::ForEachInfo loop_bad_target;
    loop_bad_target.target_entity_id = interner.Intern("Ghost");
    loop_bad_target.condition_id = axiom::kInvalidStringID; // 条件なし
    func.for_each_loops.push_back(loop_bad_target);

    // 3. エラー: 条件が bool ではない ("hp")
    axiom::ForEachInfo loop_bad_cond;
    loop_bad_cond.target_entity_id = valid_entity_id;
    loop_bad_cond.condition_id = interner.Intern("hp");
    func.for_each_loops.push_back(loop_bad_cond);

    std::vector<axiom::FunctionInfo> funcs = { func };
    axiom::TypeChecker checker(funcs, registry, interner);

    EXPECT_FALSE(checker.check_all());

    // エラーは2つ（存在しないエンティティ、条件の型違い）出るはず
    ASSERT_EQ(checker.get_errors().size(), 2);
    EXPECT_NE(checker.get_errors()[0].message.find("Unknown target entity in forEach"), std::string::npos);
    EXPECT_NE(checker.get_errors()[1].message.find("forEach condition must be bool"), std::string::npos);
}

TEST(TypeCheckerTest, ExpressionEvaluationValid) {
    const char* source = R"(
        fun calc_test() -> void {
            val a = 10;
            val b = 20;
            val c = a + b;
            val flag = a < b;
            val is_ok = not flag;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    // エラーがなく、成功することを確認
    EXPECT_TRUE(checker.check_all());
    EXPECT_TRUE(checker.get_errors().empty());

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 8. 二項演算での型不一致（暗黙キャスト禁止）のテスト
TEST(TypeCheckerTest, ExpressionEvaluationTypeMismatch) {
    const char* source = R"(
        fun bad_calc() -> void {
            val a: int32 = 10;
            val b: float32 = 1.5;
            val c = a + b;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 1);
    EXPECT_NE(checker.get_errors()[0].message.find("Type mismatch in arithmetic operation"), std::string::npos);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 9. 明示された型と初期値の型不一致テスト
TEST(TypeCheckerTest, ExpressionEvaluationAnnotationMismatch) {
    const char* source = R"(
        fun bad_annotation() -> void {
            val x: int32 = 3.14;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 1);
    EXPECT_NE(checker.get_errors()[0].message.find("Variable type annotation does not match initial value type"), std::string::npos);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 10. 論理演算および単項演算の型不一致テスト
TEST(TypeCheckerTest, ExpressionEvaluationLogicalErrors) {
    const char* source = R"(
        fun bad_logic() -> void {
            val num = 10;
            val bad_not = not num;
            val bad_and = true and 20;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 2);
    EXPECT_NE(checker.get_errors()[0].message.find("Operand of 'not' must be bool"), std::string::npos);
    EXPECT_NE(checker.get_errors()[1].message.find("Operands of 'and' / 'or' must be bool"), std::string::npos);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 11. 代入文の正常系テスト（var 変数への代入）
TEST(TypeCheckerTest, AssignmentValid) {
    const char* source = R"(
        fun test_valid_assign() -> void {
            var count = 0;
            count = 10;
            var flag = false;
            flag = true;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_TRUE(checker.check_all());
    EXPECT_TRUE(checker.get_errors().empty());

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 12. 代入文の不変性違反テスト（val 変数への再代入エラー）
TEST(TypeCheckerTest, AssignmentImmutableError) {
    const char* source = R"(
        fun test_immutable_assign() -> void {
            val speed = 100;
            speed = 200;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 1);
    EXPECT_NE(checker.get_errors()[0].message.find("Cannot assign to immutable variable"), std::string::npos);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 13. 代入文の型不一致テスト（異なる型の代入エラー）
TEST(TypeCheckerTest, AssignmentTypeMismatchError) {
    const char* source = R"(
        fun test_type_mismatch() -> void {
            var hp: int32 = 100;
            hp = 3.14;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 1);
    EXPECT_NE(checker.get_errors()[0].message.find("Type mismatch in assignment"), std::string::npos);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 14. 代入文の未定義変数テスト（宣言されていない変数への代入エラー）
TEST(TypeCheckerTest, AssignmentUndefinedVariableError) {
    const char* source = R"(
        fun test_undefined_assign() -> void {
            unknown_var = 10;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 1);
    EXPECT_NE(checker.get_errors()[0].message.find("Undefined variable"), std::string::npos);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 15. return 文の正常系テスト（正しい型を返す & void関数での空return）
TEST(TypeCheckerTest, ReturnValid) {
    const char* source = R"(
        fun add(a: int32, b: int32) -> int32 {
            return a + b;
        }

        fun do_nothing() -> void {
            return;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_TRUE(checker.check_all());
    EXPECT_TRUE(checker.get_errors().empty());

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 16. return 文の型不一致テスト（戻り値型と異なる式を返す）
TEST(TypeCheckerTest, ReturnTypeMismatchError) {
    const char* source = R"(
        fun bad_calc() -> int32 {
            return 3.14;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 1);
    EXPECT_NE(checker.get_errors()[0].message.find("Return type mismatch"), std::string::npos);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 17. return 文の空returnエラーテスト（非void関数で値を返さない）
TEST(TypeCheckerTest, ReturnEmptyInNonVoidError) {
    const char* source = R"(
        fun need_value() -> int32 {
            return;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 1);
    EXPECT_NE(checker.get_errors()[0].message.find("Non-void function must return a value"), std::string::npos);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}

// 18. return 文のvoid関数での値返却エラーテスト
TEST(TypeCheckerTest, ReturnValueInVoidError) {
    const char* source = R"(
        fun void_func() -> void {
            return 100;
        }
    )";

    axiom::StringInterner interner;
    axiom::TypeRegistry registry(interner);
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_nexa());

    TSTree* tree = ts_parser_parse_string(parser, nullptr, source, static_cast<uint32_t>(strlen(source)));
    TSNode root_node = ts_tree_root_node(tree);

    axiom::Analyzer analyzer(source, interner);
    analyzer.analyze_root(root_node);

    axiom::TypeChecker checker(source, analyzer.get_functions(), registry, interner);

    EXPECT_FALSE(checker.check_all());
    ASSERT_EQ(checker.get_errors().size(), 1);
    EXPECT_NE(checker.get_errors()[0].message.find("Cannot return a value from void function"), std::string::npos);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
}