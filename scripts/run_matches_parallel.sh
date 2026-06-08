#!/usr/bin/env bash
# Requires bash.
if [[ -z "${BASH_VERSION:-}" ]]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

# Run multiple run_matches workers in parallel.
#
# Usage:
#   ./scripts/run_matches_parallel.sh <workers> [games_per_side] [depth] [max_plies] [movetime_ms] [fen_or_book] [version_a] [version_b]
#
# Each worker runs the full run_matches.sh suite (2 * games_per_side games).
# Total games = workers * 2 * games_per_side.
#
# Example (800 games = 8 workers * 50 per side * 2 color batches):
#   ./scripts/run_matches_parallel.sh 8 50 8 150 10 book 20 29
#
# Outputs:
#   logs under scripts/match_logs/<timestamp>/
#   and a combined summary via aggregate_match_logs.sh.

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <workers> [games_per_side] [depth] [max_plies] [movetime_ms] [fen_or_book] [version_a] [version_b]" >&2
  exit 1
fi

WORKERS="$1"
shift || true

GAMES_PER_SIDE="${1:-2}"
EXPECTED_GAMES_PER_WORKER=$((2 * GAMES_PER_SIDE))

if ! [[ "${WORKERS}" =~ ^[0-9]+$ ]] || [[ "${WORKERS}" -lt 1 ]]; then
  echo "ERROR: workers must be a positive integer (got '${WORKERS}')." >&2
  exit 1
fi
if ! [[ "${GAMES_PER_SIDE}" =~ ^[0-9]+$ ]] || [[ "${GAMES_PER_SIDE}" -lt 1 ]]; then
  echo "ERROR: games_per_side must be a positive integer (got '${GAMES_PER_SIDE}')." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_SCRIPT="${ROOT_DIR}/scripts/run_matches.sh"
AGG_SCRIPT="${ROOT_DIR}/scripts/aggregate_match_logs.sh"

if [[ ! -f "${RUN_SCRIPT}" ]]; then
  echo "ERROR: ${RUN_SCRIPT} not found." >&2
  exit 1
fi
if [[ ! -f "${AGG_SCRIPT}" ]]; then
  echo "ERROR: ${AGG_SCRIPT} not found." >&2
  exit 1
fi
if ! bash -n "${RUN_SCRIPT}"; then
  echo "ERROR: ${RUN_SCRIPT} has a bash syntax error. Fix it before launching workers." >&2
  exit 1
fi
if ! bash -n "$0"; then
  echo "ERROR: $0 has a bash syntax error." >&2
  exit 1
fi

STAMP="$(date +"%Y%m%d_%H%M%S")"
LOG_DIR="${ROOT_DIR}/scripts/match_logs/${STAMP}"
mkdir -p "${LOG_DIR}"

TOTAL_EXPECTED=$((WORKERS * EXPECTED_GAMES_PER_WORKER))

echo "Launching ${WORKERS} workers..."
echo "Games per worker: ${EXPECTED_GAMES_PER_WORKER} (${GAMES_PER_SIDE} per side, both colors)"
echo "Expected total games: ${TOTAL_EXPECTED}"
echo "Log directory: ${LOG_DIR}"
echo ""

for w in $(seq 1 "${WORKERS}"); do
  LOG_FILE="${LOG_DIR}/worker_${w}.log"
  (
    echo "=== Worker ${w} start ==="
    /usr/bin/env bash "${RUN_SCRIPT}" "$@"
    echo "=== Worker ${w} end ==="
  ) > "${LOG_FILE}" 2>&1 &
done

wait

echo ""
echo "Validating worker logs..."

failed_workers=0
short_workers=0
for w in $(seq 1 "${WORKERS}"); do
  LOG_FILE="${LOG_DIR}/worker_${w}.log"
  if [[ ! -f "${LOG_FILE}" ]]; then
    echo "ERROR: missing log ${LOG_FILE}" >&2
    failed_workers=$((failed_workers + 1))
    continue
  fi

  if ! grep -q "=== Worker ${w} end ===" "${LOG_FILE}"; then
    echo "ERROR: worker ${w} did not finish (no end marker)." >&2
    failed_workers=$((failed_workers + 1))
  fi

  if grep -qE 'unexpected EOF while looking for matching|syntax error|ERROR: selfplay exited' "${LOG_FILE}"; then
    echo "ERROR: worker ${w} log contains a failure:" >&2
    grep -E 'unexpected EOF while looking for matching|syntax error|ERROR: selfplay exited|ERROR:' "${LOG_FILE}" | tail -3 >&2
    failed_workers=$((failed_workers + 1))
  fi

  game_count="$(awk '/^Game \(W:v[0-9]+ vs B:v[0-9]+\)/ { n++ } END { print n + 0 }' "${LOG_FILE}")"
  if [[ "${game_count}" -lt "${EXPECTED_GAMES_PER_WORKER}" ]]; then
    echo "ERROR: worker ${w} ran ${game_count}/${EXPECTED_GAMES_PER_WORKER} games." >&2
    short_workers=$((short_workers + 1))
  fi
done

if [[ "${failed_workers}" -gt 0 || "${short_workers}" -gt 0 ]]; then
  echo "" >&2
  echo "Some workers failed or ran incomplete suites." >&2
  echo "Expected ${EXPECTED_GAMES_PER_WORKER} games per worker (${GAMES_PER_SIDE} per side, both batches)." >&2
  echo "Partial logs are in ${LOG_DIR}" >&2
  exit 1
fi

echo "All workers completed successfully."
echo "Aggregating logs..."
/usr/bin/env bash "${AGG_SCRIPT}" "${LOG_DIR}"/worker_*.log
