#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build"
PORT="${1:-8080}"

if lsof -ti :"${PORT}" >/dev/null 2>&1; then
  echo "Stopping existing gui_server on port ${PORT}..."
  lsof -ti :"${PORT}" | xargs kill 2>/dev/null || true
  sleep 0.5
fi

cmake -S "${ROOT}" -B "${BUILD}"
cmake --build "${BUILD}" --target gui_server
exec "${BUILD}/gui_server" "${PORT}"
