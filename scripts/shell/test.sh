#!/usr/bin/env bash

set -euo pipefail

# どこから実行されてもプロジェクトルートに移動する
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${PROJECT_ROOT}"

# 初回（または build ディレクトリ未構成時）は自動で Configure を実行
if [ ! -f "build/CMakeCache.txt" ]; then
    echo "⚙️  CMake Configure (preset: default) を実行しています..."
    cmake --preset default
fi

# 増分ビルド
echo "🔨 ビルド中..."
cmake --build --preset default

# テストまたは CLI の実行
if [ $# -eq 0 ]; then
    echo "🧪 全テストを実行中..."
    ctest --preset default
elif [ "$1" = "run" ]; then
    shift
    echo "🚀 axc を実行中..."
    if [ -f "./build/compiler/driver/axc" ]; then
        ./build/compiler/driver/axc "$@"
    else
        ./build/compiler/axc "$@"
    fi
else
    echo "🔍 フィルタ付きテストを実行中: *$1*"
    ./build/tests/axiom-test --gtest_color=yes --gtest_filter="*$1*"
fi
