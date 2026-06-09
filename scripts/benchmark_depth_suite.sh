#!/usr/bin/env bash
# Requires bash ([[ ]], arrays).
if [[ -z "${BASH_VERSION:-}" ]]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

# Average fixed-depth node counts across many positions.
#
# Single-FEN benchmarks (benchmark_depth.sh) swing a lot by position; this script
# runs the same depth/version sweep on many FENs and reports mean (and median)
# nodes per version.
#
# Usage:
#   ./scripts/benchmark_depth_suite.sh [depth] [positions_file] [max_positions] [min_version] [max_version] [baseline_version]
#
# Defaults:
#   depth             = 5
#   positions_file    = scripts/equal_openings.txt  (1024 balanced midgame FENs)
#   max_positions     = 64 (random sample without replacement)
#   min_version       = 2
#   max_version       = 30
#   baseline_version  = 19
#
# Set BENCHMARK_SUITE_SEED to fix the random sample (e.g. BENCHMARK_SUITE_SEED=7).
#
# Examples:
#   ./scripts/benchmark_depth_suite.sh
#   ./scripts/benchmark_depth_suite.sh 4
#   BENCHMARK_SUITE_SEED=7 ./scripts/benchmark_depth_suite.sh 5 scripts/equal_openings.txt 128 19 28 26
#   ./scripts/benchmark_depth_suite.sh 5 scripts/equal_openings.txt 32 26 28 26

DEPTH="${1:-5}"
POSITIONS_FILE="${2:-}"
MAX_POSITIONS="${3:-64}"
MIN_VERSION="${4:-2}"
MAX_VERSION="${5:-30}"
BASELINE_VERSION="${6:-19}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -z "${POSITIONS_FILE}" ]]; then
  POSITIONS_FILE="${ROOT_DIR}/scripts/equal_openings.txt"
elif [[ "${POSITIONS_FILE}" != /* ]]; then
  POSITIONS_FILE="${ROOT_DIR}/${POSITIONS_FILE}"
fi

if ! [[ "${DEPTH}" =~ ^[0-9]+$ ]] || [[ "${DEPTH}" -lt 1 ]]; then
  echo "ERROR: depth must be a positive integer (got '${DEPTH}')." >&2
  exit 1
fi
for n in "${MAX_POSITIONS}" "${MIN_VERSION}" "${MAX_VERSION}" "${BASELINE_VERSION}"; do
  if ! [[ "${n}" =~ ^[0-9]+$ ]]; then
    echo "ERROR: numeric arguments must be integers." >&2
    exit 1
  fi
done
if [[ ! -f "${POSITIONS_FILE}" ]]; then
  echo "ERROR: positions file not found: ${POSITIONS_FILE}" >&2
  exit 1
fi
if [[ "${MIN_VERSION}" -gt "${MAX_VERSION}" ]]; then
  echo "ERROR: min_version must be <= max_version." >&2
  exit 1
fi
if [[ "${BASELINE_VERSION}" -lt "${MIN_VERSION}" ]] || [[ "${BASELINE_VERSION}" -gt "${MAX_VERSION}" ]]; then
  echo "ERROR: baseline_version must fall within [min_version, max_version]." >&2
  exit 1
fi

FENS=()
POOL_SIZE="$(grep -v '^#' "${POSITIONS_FILE}" | grep -cv '^[[:space:]]*$' || true)"
if [[ "${POOL_SIZE}" -eq 0 ]]; then
  echo "ERROR: no FENs loaded from ${POSITIONS_FILE}" >&2
  exit 1
fi
if [[ "${MAX_POSITIONS}" -gt "${POOL_SIZE}" ]]; then
  echo "WARNING: max_positions=${MAX_POSITIONS} exceeds pool size ${POOL_SIZE}; using all." >&2
  MAX_POSITIONS="${POOL_SIZE}"
fi

SUITE_SEED="${BENCHMARK_SUITE_SEED:-$RANDOM}"
FENS=()
while IFS= read -r line; do
  FENS+=("${line}")
done < <(
  python3 - "${MAX_POSITIONS}" "${SUITE_SEED}" "${POSITIONS_FILE}" <<'PY'
import random
import sys

count = int(sys.argv[1])
seed = int(sys.argv[2])
path = sys.argv[3]
with open(path) as f:
    fens = [line.strip() for line in f if line.strip() and not line.startswith("#")]
random.seed(seed)
for fen in random.sample(fens, count):
    print(fen)
PY
)
if [[ "${#FENS[@]}" -eq 0 ]]; then
  echo "ERROR: failed to sample FENs." >&2
  exit 1
fi

BIN="${ROOT_DIR}/build/search_main"
if [[ ! -x "${BIN}" ]]; then
  echo "Building project..."
  cmake --build "${ROOT_DIR}/build"
fi

RAW_FILE="$(mktemp "${TMPDIR:-/tmp}/benchmark_depth_suite_raw.XXXXXX")"
SUMMARY_FILE="$(mktemp "${TMPDIR:-/tmp}/benchmark_depth_suite_sum.XXXXXX")"
trap 'rm -f "${RAW_FILE}" "${SUMMARY_FILE}"' EXIT

echo "Fixed-depth suite benchmark (mean/median nodes)"
echo "  depth        = ${DEPTH}"
echo "  positions    = ${#FENS[@]} random from $(basename "${POSITIONS_FILE}") (${POOL_SIZE} total)"
echo "  sample_seed  = ${SUITE_SEED}"
echo "  versions     = v${MIN_VERSION}..v${MAX_VERSION}"
echo "  baseline     = v${BASELINE_VERSION}"
echo ""
echo "Running searches..."

now_ms() {
  python3 -c 'import time; print(int(time.time() * 1000))'
}

for ((version = MIN_VERSION; version <= MAX_VERSION; ++version)); do
  total_nodes=0
  total_ms=0
  pos_idx=0
  printf '  v%-2d (%d positions)...\n' "${version}" "${#FENS[@]}" >&2
  for fen in "${FENS[@]}"; do
    start_ms="$(now_ms)"
    out="$("${BIN}" "${DEPTH}" "${fen}" "${version}" 0)"
    elapsed_ms=$(( $(now_ms) - start_ms ))
    nodes="$(printf '%s\n' "${out}" | awk '/^nodes / { print $2; exit }')"
    nodes="${nodes:-0}"
    total_nodes=$(( total_nodes + nodes ))
    total_ms=$(( total_ms + elapsed_ms ))
    printf '%d %d %d\n' "${version}" "${nodes}" "${elapsed_ms}" >> "${RAW_FILE}"
    pos_idx=$(( pos_idx + 1 ))
  done
  printf '%d %d %d\n' "${version}" "${total_nodes}" "${total_ms}" >> "${SUMMARY_FILE}"
done

python3 - "${RAW_FILE}" "${SUMMARY_FILE}" "${#FENS[@]}" "${BASELINE_VERSION}" <<'PY'
import sys

raw_path, summary_path, num_positions, baseline_version = sys.argv[1:5]
num_positions = int(num_positions)
baseline_version = int(baseline_version)

per_version_nodes = {}
with open(raw_path) as f:
    for line in f:
        version, nodes, _elapsed = map(int, line.split())
        per_version_nodes.setdefault(version, []).append(nodes)

baseline_avg = None
version_totals = {}
version_times = {}
with open(summary_path) as f:
    for line in f:
        version, total_nodes, total_ms = map(int, line.split())
        version_totals[version] = total_nodes
        version_times[version] = total_ms
        avg = total_nodes / num_positions
        if version == baseline_version:
            baseline_avg = avg

if baseline_avg is None or baseline_avg <= 0:
    print("ERROR: invalid baseline average", file=sys.stderr)
    sys.exit(1)

def median(values):
    s = sorted(values)
    n = len(s)
    if n == 0:
        return 0
    mid = n // 2
    if n % 2:
        return s[mid]
    return (s[mid - 1] + s[mid]) / 2

print("")
print(f"{'ver':<4}  {'avg_nodes':>12}  {'med_nodes':>12}  {'total':>12}  {'time':>8}  {'vs_base':>10}  {'pruned':>10}")
print(f"{'---':<4}  {'---------':>12}  {'---------':>12}  {'-----':>12}  {'----':>8}  {'-------':>10}  {'------':>10}")

for version in sorted(version_totals):
    nodes_list = per_version_nodes[version]
    avg = version_totals[version] / num_positions
    med = median(nodes_list)
    total = version_totals[version]
    total_ms = version_times[version]
    time_s = f"{total_ms / 1000:.1f}s"
    ratio = avg / baseline_avg
    pruned = (1 - ratio) * 100
    marker = " *" if version == baseline_version else ""
    print(
        f"v{version:<2}  {avg:12.0f}  {med:12.0f}  {total:12d}  {time_s:>8}  {ratio:9.2f}x  {pruned:9.1f}%{marker}"
    )

print("")
print(f"* baseline avg (v{baseline_version}: {baseline_avg:,.0f} nodes/position over {num_positions} positions)")
PY
