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
    if [ "${VERBOSE:-0}" = "1" ]; then
        ctest --preset default
    else
        log_file="$(mktemp)"
        trap 'rm -f "${log_file}"' EXIT
        if ctest --preset default > "${log_file}" 2>&1; then
            passed_line="$(grep "tests passed" "${log_file}" | sed 's/^[[:space:]]*//')"
            time_val="$(grep "Total Test time" "${log_file}" | awk -F'= ' '{print $2}' | sed 's/^[[:space:]]*//')"
            echo "✅ ${passed_line} (${time_val})"
        else
            echo "❌ テスト失敗（詳細ログのみ表示。全体を見る場合は VERBOSE=1 ./scripts/shell/test.sh）"
            grep -v -E '^[[:space:]]*Start [0-9]+:|^[[:space:]]*[0-9]+/[0-9]+ Test[[:space:]]+#[0-9]+:.*Passed[[:space:]]+[0-9.]+ sec$' "${log_file}" || cat "${log_file}"
            exit 1
        fi
    fi
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
