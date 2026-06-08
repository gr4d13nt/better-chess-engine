#!/usr/bin/env bash
# Requires bash ([[ ]], arrays).
if [[ -z "${BASH_VERSION:-}" ]]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

# Compare nodes searched at a fixed depth across engine versions.
#
# Fixed depth (movetime_ms=0) is the right metric for pruning: every version
# searches the same tree depth, so fewer nodes means more cutoffs. Time-based
# benchmarks mix eval cost and search speed and are less useful here.
#
# Usage:
#   ./scripts/benchmark_depth.sh [depth] [fen] [min_version] [max_version] [baseline_version]
#
# Defaults:
#   depth             = 5
#   fen               = midgame tactical FEN (out of opening book)
#   min_version       = 2   (v1 unpruned is extremely slow; pass 1 explicitly if needed)
#   max_version       = 26
#   baseline_version  = 2   (alpha-beta; report node ratio vs this)
#
# Examples:
#   ./scripts/benchmark_depth.sh
#   ./scripts/benchmark_depth.sh 6 startpos
#   ./scripts/benchmark_depth.sh 4 startpos 18 26 18
#
# For mean/median nodes over many positions, use benchmark_depth_suite.sh.

DEPTH="${1:-5}"
FEN="${2:-6k1/1pp2rp1/p2pNq2/3P4/8/3b4/PP1Q2PP/4R1K1 w - - 2 1}"
MIN_VERSION="${3:-2}"
MAX_VERSION="${4:-28}"
BASELINE_VERSION="${5:-2}"

if ! [[ "${DEPTH}" =~ ^[0-9]+$ ]] || [[ "${DEPTH}" -lt 1 ]]; then
  echo "ERROR: depth must be a positive integer (got '${DEPTH}')." >&2
  exit 1
fi
for v in "${MIN_VERSION}" "${MAX_VERSION}" "${BASELINE_VERSION}"; do
  if ! [[ "${v}" =~ ^[0-9]+$ ]]; then
    echo "ERROR: version arguments must be integers." >&2
    exit 1
  fi
done
if [[ "${MIN_VERSION}" -gt "${MAX_VERSION}" ]]; then
  echo "ERROR: min_version must be <= max_version." >&2
  exit 1
fi
if [[ "${BASELINE_VERSION}" -lt "${MIN_VERSION}" ]] || [[ "${BASELINE_VERSION}" -gt "${MAX_VERSION}" ]]; then
  echo "ERROR: baseline_version must fall within [min_version, max_version]." >&2
  exit 1
fi
if [[ "${MIN_VERSION}" -eq 1 ]] && [[ "${DEPTH}" -ge 5 ]]; then
  echo "WARNING: v1 (no pruning) at depth >= 5 can take a very long time." >&2
fi
if [[ "${FEN}" == "startpos" ]] && [[ "${MAX_VERSION}" -ge 25 ]]; then
  echo "WARNING: v25/v26 on startpos use the opening book (nodes=0). Use a non-book FEN." >&2
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/search_main"

if [[ ! -x "${BIN}" ]]; then
  echo "Building project..."
  cmake --build "${ROOT_DIR}/build"
fi

RESULTS_FILE="$(mktemp "${TMPDIR:-/tmp}/benchmark_depth.XXXXXX")"
trap 'rm -f "${RESULTS_FILE}"' EXIT

echo "Fixed-depth node benchmark (pruning comparison)"
echo "  depth     = ${DEPTH}"
echo "  fen       = ${FEN}"
echo "  versions  = v${MIN_VERSION}..v${MAX_VERSION}"
echo "  baseline  = v${BASELINE_VERSION} (alpha-beta unless overridden)"
echo ""
echo "Running searches..."

now_ms() {
  python3 -c 'import time; print(int(time.time() * 1000))'
}

for ((version = MIN_VERSION; version <= MAX_VERSION; ++version)); do
  printf '  v%-2d ...\n' "${version}" >&2
  start_ms="$(now_ms)"
  out="$("${BIN}" "${DEPTH}" "${FEN}" "${version}" 0)"
  elapsed_ms=$(( $(now_ms) - start_ms ))
  nodes="$(printf '%s\n' "${out}" | awk '/^nodes / { print $2; exit }')"
  bestmove="$(printf '%s\n' "${out}" | awk '/^bestmove / { print $2; exit }')"
  printf '%d %d %d %s\n' "${version}" "${nodes:-0}" "${elapsed_ms}" "${bestmove:-nomove}" >> "${RESULTS_FILE}"
done

baseline_nodes="$(awk -v b="${BASELINE_VERSION}" '$1 == b { print $2; exit }' "${RESULTS_FILE}")"
if [[ -z "${baseline_nodes}" ]] || [[ "${baseline_nodes}" -le 0 ]]; then
  echo "ERROR: baseline v${BASELINE_VERSION} returned ${baseline_nodes:-0} nodes." >&2
  exit 1
fi

echo ""
printf '%-4s  %12s  %8s  %8s  %10s  %10s  %s\n' "ver" "nodes" "time" "nps" "vs_base" "pruned" "bestmove"
printf '%-4s  %12s  %8s  %8s  %10s  %10s  %s\n' "---" "-----" "----" "---" "-------" "------" "--------"

while read -r version nodes elapsed_ms bestmove; do
  marker=""
  ratio="n/a"
  pruned="n/a"
  nps="n/a"
  if [[ "${elapsed_ms}" -gt 0 ]]; then
    time_s="$(awk -v ms="${elapsed_ms}" 'BEGIN { printf "%.2fs", ms / 1000 }')"
  else
    time_s="0.00s"
  fi
  if [[ "${nodes}" -gt 0 ]] && [[ "${elapsed_ms}" -gt 0 ]]; then
    nps="$(awk -v n="${nodes}" -v ms="${elapsed_ms}" 'BEGIN { printf "%.0f", n * 1000 / ms }')"
  fi
  if [[ "${nodes}" -gt 0 ]]; then
    ratio="$(awk -v n="${nodes}" -v b="${baseline_nodes}" 'BEGIN { printf "%.2fx", n / b }')"
    pruned="$(awk -v n="${nodes}" -v b="${baseline_nodes}" 'BEGIN { printf "%.1f%%", (1 - n / b) * 100 }')"
  fi
  if [[ "${version}" -eq "${BASELINE_VERSION}" ]]; then
    marker=" *"
  fi
  printf 'v%-2d  %12d  %8s  %8s  %10s  %10s  %s%s\n' \
    "${version}" "${nodes}" "${time_s}" "${nps}" "${ratio}" "${pruned}" "${bestmove}" "${marker}"
done < "${RESULTS_FILE}"

echo ""
echo "* baseline (v${BASELINE_VERSION}: ${baseline_nodes} nodes)"
