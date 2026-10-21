## 🇯🇵 日本語

### Nexa とは？

Nexa（ネクサ）は、**データ指向設計（DOD: Data-Oriented Design）** とハイパフォーマンス・コンピューティング（ゲームエンジンやグラフィックス処理など）のためにゼロから設計された、新しいシステムプログラミング言語です。

関数型やオブジェクト指向といった従来のアプローチではなく、CPUキャッシュの最適化とメモリレイアウトを最優先し、フラットなデータ構造（SoA）を言語レベルで自動構築します。現在はC++23とTree-sitterを用いたブートストラップ（足場作り）の段階にあり、将来的なセルフホスティングと、完全なC/C++互換を目指しています。

### 主な特徴

* **デフォルトでデータ指向**: 人間にとって読み書きしやすい構文でありながら、コンパイル時には自動的にキャッシュ効率の圧倒的に高い SoA（Structure of Arrays）レイアウトに変換されます。
* **C/C++ とのシームレスな連携**: 既存のC/C++の強大なエコシステム（LLVMやVulkanなど）と摩擦なく連携できるように設計されています。
* **フラットなメモリモデル**: ポインタの依存を排除し、IDベースの文字列管理と連続したメモリブロックを採用することで、キャッシュミスを劇的に削減します。

### コード例

Nexaでは、コンポーネントを直感的に定義するだけで、裏側では最速の並列配列としてメモリに配置されます。

```nexa
// 宣言はシンプルですが、コンパイラが自動的に最適化されたSoA配列を生成します
component Position {
    x: f32,
    y: f32
}

component Velocity {
    x: f32,
    y: f32
}

val hp: i32 = 100;
```

### ビルド方法

現在、Nexaコンパイラは C++23 と CMake を使用して開発されています。

**必須環境:**

* CMake (バージョン 3.24 以上)
* C++23 対応コンパイラ (Clang / GCC / MSVC)

**ソースからのビルド:**

```bash
git clone https://github.com/yourusername/nexa.git
cd nexa

# CMake Presets を使用して構成・ビルド
cmake --preset default
cmake --build --preset default
```

### テストの実行

堅牢なコンパイラを構築するため、Google Test および CTest を用いたテスト駆動開発（TDD）を導入しています。

```bash
# ビルド＆テスト一括実行スクリプト（初回は自動で Configure も実行）
./scripts/shell/test.sh

# 特定のテストのみフィルタ実行
./scripts/shell/test.sh IrBuilder
./scripts/shell/test.sh C99CodeGen

# CTest またはテストバイナリを直接実行する場合
ctest --preset default
./build/tests/axiom-test
```

---

### ライセンス (License)

This project is licensed under the [BSD-2-Clause-Patent](LICENSE).