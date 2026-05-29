#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build"
PORT="${1:-8080}"

if [[ "${BASH_VERSINFO[0]}" -lt 4 ]]; then
  exec bash "$0" "$@"
fi

cmake -S "${ROOT}" -B "${BUILD}"
cmake --build "${BUILD}" --target gui_server
exec "${BUILD}/gui_server" "${PORT}"
