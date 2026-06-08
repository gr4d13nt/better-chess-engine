#!/usr/bin/env bash
# Requires bash.
if [[ -z "${BASH_VERSION:-}" ]]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

# Compare timed search (v29 vs v30) across movetime budgets.
#
# Usage:
#   ./scripts/benchmark_timed.sh [mode] [depth_cap] [version_a] [version_b]
#
# Modes:
#   sweep   - one FEN, many movetime values (default)
#   suite   - random midgame sample, one movetime per row in MOVETIMES
#
# Environment:
#   BENCHMARK_TIMED_FEN      - FEN for sweep mode
#   BENCHMARK_TIMED_MS       - space-separated movetimes (default: 10 25 50 100 250 500 1000 2000)
#   BENCHMARK_SUITE_SEED     - fix random sample for suite mode
#   BENCHMARK_SUITE_COUNT    - positions for suite (default 32)
#   BENCHMARK_THREADS        - num_threads arg to search_main (default 0 = auto SMP)
#
# Examples:
#   ./scripts/benchmark_timed.sh sweep 8 29 30
#   BENCHMARK_TIMED_MS="50 500 2000" ./scripts/benchmark_timed.sh suite 8 29 30

MODE="${1:-sweep}"
DEPTH_CAP="${2:-8}"
VERSION_A="${3:-29}"
VERSION_B="${4:-30}"
MOVETIMES="${BENCHMARK_TIMED_MS:-10 25 50 100 250 500 1000 2000}"
THREADS="${BENCHMARK_THREADS:-0}"
FEN="${BENCHMARK_TIMED_FEN:-r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4}"
SUITE_COUNT="${BENCHMARK_SUITE_COUNT:-32}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/search_main"
OPENINGS="${ROOT_DIR}/scripts/equal_openings.txt"

if [[ ! -x "${BIN}" ]]; then
  echo "ERROR: ${BIN} not found. Build first." >&2
  exit 1
fi

run_timed() {
  local version="$1"
  local fen="$2"
  local movetime_ms="$3"
  local depth="$4"
  local threads="$5"

  local time_out
  time_out="$(/usr/bin/time -p "${BIN}" "${depth}" "${fen}" "${version}" "${movetime_ms}" "${threads}" 2>&1)"
  local real depth_reached nodes bestmove score
  real="$(printf '%s\n' "${time_out}" | awk '/^real /{print $2; exit}')"
  depth_reached="$(printf '%s\n' "${time_out}" | awk '/^depth_reached /{print $2; exit}')"
  nodes="$(printf '%s\n' "${time_out}" | awk '/^nodes /{print $2; exit}')"
  bestmove="$(printf '%s\n' "${time_out}" | awk '/^bestmove /{print $2; exit}')"
  score="$(printf '%s\n' "${time_out}" | awk '/^score /{print $2; exit}')"
  depth_reached="${depth_reached:-0}"
  nodes="${nodes:-0}"
  score="${score:-0}"
  bestmove="${bestmove:-nomove}"
  printf '%s %s %s %s %s %s\n' "${real}" "${depth_reached}" "${nodes}" "${score}" "${bestmove}" "${movetime_ms}"
}

print_header() {
  printf '%-8s %-6s %-6s %-8s %-8s %-12s %-12s %-8s %-8s\n' \
    "movetime" "vA" "vB" "depthA" "depthB" "realA" "realB" "dDiff" "sameMv"
}

compare_row() {
  local ms="$1"
  local fen="$2"
  read -r real_a depth_a nodes_a score_a move_a _ <<< "$(run_timed "${VERSION_A}" "${fen}" "${ms}" "${DEPTH_CAP}" "${THREADS}")"
  read -r real_b depth_b nodes_b score_b move_b _ <<< "$(run_timed "${VERSION_B}" "${fen}" "${ms}" "${DEPTH_CAP}" "${THREADS}")"
  local depth_diff=$((depth_b - depth_a))
  local same_move=0
  if [[ "${move_a}" == "${move_b}" ]]; then
    same_move=1
  fi
  printf '%-8s %-6s %-6s %-8s %-8s %-12s %-12s %-8s %-8s\n' \
    "${ms}" "${VERSION_A}" "${VERSION_B}" "${depth_a}" "${depth_b}" \
    "${real_a}" "${real_b}" "${depth_diff}" "${same_move}"
}

run_sweep() {
  echo "Timed sweep (single FEN, depth_cap=${DEPTH_CAP}, threads=${THREADS})"
  echo "FEN: ${FEN}"
  echo "Versions: v${VERSION_A} vs v${VERSION_B}"
  echo ""
  print_header
  for ms in ${MOVETIMES}; do
    compare_row "${ms}" "${FEN}"
  done
  echo ""
  echo "dDiff = depth(v${VERSION_B}) - depth(v${VERSION_A}). Positive => v${VERSION_B} deeper."
}

load_suite_fens() {
  local seed="${BENCHMARK_SUITE_SEED:-$RANDOM}"
  echo "sample_seed ${seed}" >&2
  python3 - "${OPENINGS}" "${SUITE_COUNT}" "${seed}" <<'PY'
import random, sys
path, count, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
random.seed(seed)
fens = []
with open(path) as f:
    for line in f:
        line = line.split("#", 1)[0].strip()
        if line:
            fens.append(line)
if count > len(fens):
    count = len(fens)
for fen in random.sample(fens, count):
    print(fen)
PY
}

run_suite() {
  echo "Timed suite (depth_cap=${DEPTH_CAP}, threads=${THREADS}, positions=${SUITE_COUNT})"
  echo "Versions: v${VERSION_A} vs v${VERSION_B}"
  echo "Movetimes: ${MOVETIMES}"
  echo ""

  FENS=()
  while IFS= read -r line; do
    FENS+=("${line}")
  done < <(load_suite_fens)

  for ms in ${MOVETIMES}; do
    local n="${#FENS[@]}"
    local sum_depth_a=0 sum_depth_b=0
    local v30_deeper=0 v29_deeper=0 same_depth=0 same_move=0
    local sum_real_a=0 sum_real_b=0

    for fen in "${FENS[@]}"; do
      read -r real_a depth_a _ _ move_a _ <<< "$(run_timed "${VERSION_A}" "${fen}" "${ms}" "${DEPTH_CAP}" "${THREADS}")"
      read -r real_b depth_b _ _ move_b _ <<< "$(run_timed "${VERSION_B}" "${fen}" "${ms}" "${DEPTH_CAP}" "${THREADS}")"
      sum_depth_a=$((sum_depth_a + depth_a))
      sum_depth_b=$((sum_depth_b + depth_b))
      sum_real_a=$(awk -v a="${sum_real_a}" -v b="${real_a}" 'BEGIN{printf "%.3f", a+b}')
      sum_real_b=$(awk -v a="${sum_real_b}" -v b="${real_b}" 'BEGIN{printf "%.3f", a+b}')
      if [[ "${depth_b}" -gt "${depth_a}" ]]; then
        v30_deeper=$((v30_deeper + 1))
      elif [[ "${depth_b}" -lt "${depth_a}" ]]; then
        v29_deeper=$((v29_deeper + 1))
      else
        same_depth=$((same_depth + 1))
      fi
      if [[ "${move_a}" == "${move_b}" ]]; then
        same_move=$((same_move + 1))
      fi
    done

    local mean_depth_a mean_depth_b mean_real_a mean_real_b
    mean_depth_a=$(awk -v s="${sum_depth_a}" -v n="${n}" 'BEGIN{printf "%.2f", s/n}')
    mean_depth_b=$(awk -v s="${sum_depth_b}" -v n="${n}" 'BEGIN{printf "%.2f", s/n}')
    mean_real_a=$(awk -v s="${sum_real_a}" -v n="${n}" 'BEGIN{printf "%.3f", s/n}')
    mean_real_b=$(awk -v s="${sum_real_b}" -v n="${n}" 'BEGIN{printf "%.3f", s/n}')

    printf 'movetime=%-5s mean_depth v%-2s=%-5s v%-2s=%-5s | v%-2s deeper=%-2d v%-2s deeper=%-2d tie=%-2d | same_move=%d/%d | mean_real v%-2s=%ss v%-2s=%ss\n' \
      "${ms}" "${VERSION_A}" "${mean_depth_a}" "${VERSION_B}" "${mean_depth_b}" \
      "${VERSION_B}" "${v30_deeper}" "${VERSION_A}" "${v29_deeper}" "${same_depth}" \
      "${same_move}" "${n}" "${VERSION_A}" "${mean_real_a}" "${VERSION_B}" "${mean_real_b}"
  done
}

case "${MODE}" in
  sweep) run_sweep ;;
  suite) run_suite ;;
  *)
    echo "ERROR: unknown mode '${MODE}' (use sweep or suite)." >&2
    exit 1
    ;;
esac
