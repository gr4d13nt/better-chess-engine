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
# Example:
#   ./scripts/run_matches_parallel.sh 8 5 6 160 100 book 5 6
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

STAMP="$(date +"%Y%m%d_%H%M%S")"
LOG_DIR="${ROOT_DIR}/scripts/match_logs/${STAMP}"
mkdir -p "${LOG_DIR}"

echo "Launching ${WORKERS} workers..."
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
echo "All workers completed."
echo "Aggregating logs..."
/usr/bin/env bash "${AGG_SCRIPT}" "${LOG_DIR}"/*.log
