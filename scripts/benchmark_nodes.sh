#!/usr/bin/env bash
# Requires bash ([[ ]], arrays).
if [[ -z "${BASH_VERSION:-}" ]]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

# Benchmark nodes searched within a time budget for each engine version.
#
# For comparing pruning across versions, prefer fixed-depth search instead:
#   ./scripts/benchmark_depth.sh
#
# Uses search_main with iterative deepening until movetime_ms elapses.
# Each version runs in a fresh process (clean transposition table).
#
# Usage:
#   ./scripts/benchmark_nodes.sh [movetime_ms] [max_depth] [fen] [min_version] [max_version]
#
# Defaults:
#   movetime_ms  = 1000
#   max_depth    = 64
#   fen          = midgame tactical FEN (out of opening book; v25/v26 search normally)
#   min_version  = 1
#   max_version  = 26
#
# Single version:
#   ./scripts/benchmark_nodes.sh 500 64 startpos 19 19
#
# Notes:
#   - v25/v26 on startpos return nodes=0 (opening book). Use a non-book FEN for them.
#   - nps uses the configured movetime budget, not wall clock (search checks time periodically).

MOVETIME_MS="${1:-1000}"
MAX_DEPTH="${2:-64}"
FEN="${3:-6k1/1pp2rp1/p2pNq2/3P4/8/3b4/PP1Q2PP/4R1K1 w - - 2 1}"
MIN_VERSION="${4:-1}"
MAX_VERSION="${5:-26}"

if ! [[ "${MOVETIME_MS}" =~ ^[0-9]+$ ]] || [[ "${MOVETIME_MS}" -lt 1 ]]; then
  echo "ERROR: movetime_ms must be a positive integer (got '${MOVETIME_MS}')." >&2
  exit 1
fi
if ! [[ "${MAX_DEPTH}" =~ ^[0-9]+$ ]] || [[ "${MAX_DEPTH}" -lt 1 ]]; then
  echo "ERROR: max_depth must be a positive integer (got '${MAX_DEPTH}')." >&2
  exit 1
fi
if ! [[ "${MIN_VERSION}" =~ ^[0-9]+$ ]] || ! [[ "${MAX_VERSION}" =~ ^[0-9]+$ ]]; then
  echo "ERROR: min_version and max_version must be integers." >&2
  exit 1
fi
if [[ "${MIN_VERSION}" -gt "${MAX_VERSION}" ]]; then
  echo "ERROR: min_version must be <= max_version." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/search_main"

if [[ ! -x "${BIN}" ]]; then
  echo "Building project..."
  cmake --build "${ROOT_DIR}/build"
fi

parse_nodes() {
  awk '/^nodes / { print $2; exit }'
}

parse_bestmove() {
  awk '/^bestmove / { print $2; exit }'
}

run_version() {
  local version="$1"
  local out bestmove nodes nps

  out="$("${BIN}" "${MAX_DEPTH}" "${FEN}" "${version}" "${MOVETIME_MS}")"
  bestmove="$(printf '%s\n' "${out}" | parse_bestmove)"
  nodes="$(printf '%s\n' "${out}" | parse_nodes)"
  nodes="${nodes:-0}"
  nps=$((nodes * 1000 / MOVETIME_MS))

  printf 'v%-2d  %10d  %10d  %s\n' "${version}" "${nodes}" "${nps}" "${bestmove:-nomove}"
}

echo "Node benchmark"
echo "  movetime_ms = ${MOVETIME_MS}"
echo "  max_depth   = ${MAX_DEPTH}"
echo "  fen         = ${FEN}"
echo "  versions    = v${MIN_VERSION}..v${MAX_VERSION}"
echo ""
printf '%-4s  %10s  %10s  %s\n' "ver" "nodes" "nps" "bestmove"
printf '%-4s  %10s  %10s  %s\n' "---" "-----" "---" "--------"

for ((version = MIN_VERSION; version <= MAX_VERSION; ++version)); do
  run_version "${version}"
done
