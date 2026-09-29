// cli/src/main.cpp
//
// 【役割】
//   Axiom コンパイラ CLI (axc) のエントリポイント。
//   現時点では parse_test_code() を呼ぶだけの最小構成。
//
// 【将来の展望】
//   - コマンドライン引数（.ax ファイルパス）を受け取ってコンパイルを実行
//   - エラーが出た場合は stderr に出力して非ゼロで終了
//   - オプション: --check（型検査のみ）、--emit-ir 等

#include <iostream>

// parse_test_code() の前方宣言。
// 本体は core/src/compiler.cpp に実装されている。
// CLI は core ライブラリにリンクされているので、ここで宣言するだけで使える。
namespace axiom {
void parse_test_code();
}

int main() {
    std::cout << "--- Axiom Compiler (axc) ---" << std::endl;

    // パース処理の実行
    // 現時点はハードコードされたテストコードを実行するだけ
    axiom::parse_test_code();

    return 0;
}