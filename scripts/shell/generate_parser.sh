#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

echo "Tree-sitterのパーサーを再生成しています..."

cd "${PROJECT_ROOT}/grammar/tree-sitter-axiom"

npx tree-sitter-cli generate

echo "生成が完了しました"