# Axiom Language Project Handoff Context (引き継ぎ用コンテキスト)

このドキュメントは、Axiom 言語開発セッションの文脈、アーキテクチャ設計、実装進捗、および次の課題を新しい AI エージェント（VSCode 拡張機能等）に 1 秒で引き継ぐためのマスターコンテキストです。

---

## 1. プロジェクト概要 & 設計哲学

- **プロジェクト名**: Nexa / Axiom Language (`axc`)
- **ターゲット**: DOD（Data-Oriented Design）/ ECS 指向の高スループット言語（ゲームエンジン・リアルタイムシミュレーション向け）
- **実装言語**: Modern C++23 (CMake 3.24+, MSVC/Clang/GCC クロスプラットフォーム対応)
- **コア哲学**:
  - **「脱LLVM」**: 巨大なブラックボックス依存を排除し、自前で完全にコントロールできる軽量・超高速コンパイル基盤を構築する。
  - **DOD ガイドライン厳守**: 深いポインタ階層やポリモーフィックな仮想関数を避け、フラットなメモリ配列（SoA/値セマンティクス）で管理。ホットパスでの動的ヒープアロケーションをゼロにする。

---

## 2. これまでの達成状況 & 実装パイプライン

現在、以下のパイプラインが完全に開通し、**全 73 件のテストが 0.52 秒（マイクロ秒オーダー）で 100% PASS** しています。

```
[Axiom ソースコード]
       │
       ▼ (Tree-sitter)
[構文木 AST]
       │
       ▼ (Analyzer & TypeChecker: セマンティクス解析・型検査完了)
[FunctionInfo / AST ノード]
       │
       ▼ (IrBuilder: 3番地コードへの平坦化・時系列化)
[Axiom IR (IrModule)]
       │
       ▼ (C99CodeGen: C言語ソースコードへのマッピング)
[C99 ソースコード文字列]
```

### 完了したフェーズの詳細

1. **構文解析 & 型検査 (67テスト)**:
   - 変数宣言（`val` / `var`）、四則演算、比較、論理演算、丸括弧式
   - 複合代入演算子（`+=`, `-=`, `*=`, `/=`）
   - 条件分岐（`if`, `else if`, `else`）
   - ループ制御（`while` 内の `break`, `continue`、`forEach` 内は禁止）
   - 型推論および厳格な型チェッカー

2. **フェーズ 1: Axiom IR の設計と構築 (`compiler/include/axiom/ir/`, `compiler/src/ir/`)**:
   - 3番地コード形式（`dest = src1 op src2`）のフラットな命令列。
   - `IrOpcode`: `Alloca`, `Assign`, `Add`, `Sub`, `Mul`, `Div`, `Return`
   - `IrOperand`: `Constant`（即値）, `Variable`（変数名ID）, `Temporary`（仮想レジスタ `%0`, `%1`...）
   - `IrBuilder`: AST の木構造を上から下へ巡回し、時系列の `std::vector<IrInstruction>` に平坦化。
   - 開発効率ヘルパー:
     - `emit(func, op, dest, src1, src2)`: 命令発行を1行に短縮
     - `child_by_field(node, "name")`: Tree-sitter の文字数カウントを自動化
     - `field_id(node, "name", fallback)`: フィールド抽出から `StringInterner` 登録まで1行で完結
     - `parse_arith_op(op_text)`: `+`, `+=` などの演算子を共通判定

3. **フェーズ 2: C99 コード生成 (`compiler/include/axiom/codegen/`, `compiler/src/codegen/`)**:
   - `C99CodeGen`: `IrModule` を受け取り、有効な C99 ソースコード文字列を出力。
   - 型マッピング: `int32` ➔ `int32_t`, `float32` ➔ `float`, `bool` ➔ `bool` 等
   - オペランドサニタイズ: C言語で識別子に使えない `%0` を `_t0` に自動置換。
   - 複合代入（`hp += 20`）も `Add` + `Assign` として正しく展開・出力されることを確認済み。

---

## 3. 主要ファイル構成

```
nexa/
├── CMakeLists.txt                       # トップレベル (enable_testing, CMAKE_EXPORT_COMPILE_COMMANDS)
├── CMakePresets.json                    # default, windows-msvc (build/test プリセット完備)
├── grammar/tree-sitter-axiom/           # Tree-sitter 文法定義 (grammar.js)
├── compiler/
│   ├── CMakeLists.txt                   # axiom-core, axiom-tree-sitter (サードパーティ警告抑制済み)
│   ├── include/axiom/
│   │   ├── base/compiler.h              # StringID, StringInterner (文字列インターン機構)
│   │   ├── ir/ir.h                      # Axiom IR データ構造 (IrInstruction, IrFunction, IrModule)
│   │   ├── ir/ir_builder.h              # IrBuilder クラス定義
│   │   └── codegen/c99_codegen.h        # C99CodeGen クラス定義
│   └── src/
│       ├── ir/ir_builder.cpp            # IR 構築実装 (AST -> IR)
│       └── codegen/c99_codegen.cpp      # C99 生成実装 (IR -> C99)
└── tests/
    ├── CMakeLists.txt
    └── unit/
        ├── test_compiler.cpp            # 構文解析・型チェッカーテスト (67 tests)
        ├── test_ir.cpp                  # IR 生成テスト (3 tests)
        └── test_c99_codegen.cpp         # C99 コード生成テスト (3 tests)
```

---

## 4. ビルド & テスト実行コマンド

```bash
# ビルド
cmake --build --preset default

# 全テスト実行 (70+ tests)
ctest --preset default

# 個別テスト実行
./build/tests/axiom-test --gtest_filter="C99CodeGenTest.*"
./build/tests/axiom-test --gtest_filter="IrBuilderTest.*"
```

---

## 5. 次に取り組むべき課題 & 議論中の論点

直前のディスカッションで **「どのバックエンド / コンパイラを採用すべきか」** を比較検討していました：

### 論点 1: バックエンド実行エンジンの選定
1. **案 A（推奨・王道）: C99 ➔ システムの Clang / MSVC / GCC 呼び出し**
   - 生成した C99 コードを `clang -O3` でコンパイルして実行バイナリを出力。
   - **利点**: 自動ベクトル化（SIMD/NEON/AVX）の恩恵を最大に受けられ、ゲーム/DODの実行速度が最強。macOS (Apple Silicon M1/M2/M3) と Windows で 100% 安定動作。
2. **案 B: libtcc (TinyCC) のインプロセス JIT 統合**
   - 外部コンパイラ不要でメモリ上で即座に機械語に変換して実行。
   - **注意点**: macOS の ARM64（Apple Silicon）におけるコード署名・W^X メモリ保護の制限により、JIT 実行でクラッシュするハードルがある。
3. **案 C: ハイブリッド**
   - 開発時は `clang -O0`（または簡易インタプリタ）で瞬時に実行、リリース時は `clang -O3` で最適化バイナリ出力。

### 論点 2: IR の機能拡張
- 条件分岐（`if` / `else if` / `else`）および `while` ループの IR 命令化：
  - `IrOpcode::Label`（ジャンプ先ラベル）
  - `IrOpcode::Jump`（無条件ジャンプ）
  - `IrOpcode::BranchIf`（条件付きジャンプ）

### 論点 3: CLI ドライバー (`axc`) のコマンド実装
- `compiler/driver/src/main.cpp` を実装し、CLI から直接 `.ax` ファイルを C99 出力または実行できるようにする：
  - `axc emit-c sample.ax`
  - `axc build sample.ax -o sample`

---

## 6. ペアプログラミング & メンターモードのルール

- **AI の役割**: 実装の丸投げ代行ではなく、アーキテクチャ設計・データフロー解説・命名提案・ステップバイステップのガイドを行う「専任メンター」。
- **境界線ルール**:
  - **ユーザーが握る領域**: コアの設計思想、データ構造（struct/enum）の定義、言語仕様の決定。
  - **AI に任せてよい領域**: ボイラープレートな構文記述、外部 API の介護コード、定型テーブル変換、テストコードの量産。
  - ユーザーが「実装して」と明示的に指示した場合は、正確でクリーンな実装を提供する。

---

## 7. VSCode 側での再開用プロンプト例

VSCode の Antigravity チャットを開いたら、以下のプロンプトをそのまま送信してください：

```
@HANDOFF.md を読み込みました。
Axiom 言語開発のコンテキストを引き継ぎました。
現在「Axiom IR ➔ C99 CodeGen（73テスト通過）」まで完了しており、
バックエンド実行エンジン（Clang サブプロセス呼び出し vs libtcc）の選定について議論していました。
続きを進めましょう。
```
