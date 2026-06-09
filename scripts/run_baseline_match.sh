#!/usr/bin/env bash
# Requires bash.
if [[ -z "${BASH_VERSION:-}" ]]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

# Run a match suite against a named Elo baseline, then print Elo estimate.
#
# Baselines (see elo_baselines.sh):
#   historical  — v5  (long-arc progress)
#   development — v25 (recent versions, v26+)
#
# Usage:
#   ./scripts/run_baseline_match.sh <historical|development> <challenger_version> \
#       [workers] [games_per_side] [depth] [max_plies] [movetime_ms] [fen_or_book]
#
# Defaults (same spirit as run_matches_parallel):
#   workers=8, games_per_side=50, depth=8, max_plies=150, movetime_ms=100, fen_or_book=book
#
# Examples:
#   ./scripts/run_baseline_match.sh development 30
#   ./scripts/run_baseline_match.sh historical 18
#   ./scripts/run_baseline_match.sh dev 30 8 50 8 150 10 book

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=elo_baselines.sh
source "${ROOT_DIR}/scripts/elo_baselines.sh"

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <historical|development> <challenger_version> \\" >&2
  echo "         [workers] [games_per_side] [depth] [max_plies] [movetime_ms] [fen_or_book]" >&2
  exit 1
fi

BASELINE_MODE="${1}"
CHALLENGER="${2}"
shift 2

if ! [[ "${CHALLENGER}" =~ ^[0-9]+$ ]]; then
  echo "ERROR: challenger_version must be an integer (got '${CHALLENGER}')." >&2
  exit 1
fi

BASELINE_VERSION="$(elo_baseline_version "${BASELINE_MODE}")"
BASELINE_NAME="$(elo_baseline_label "${BASELINE_MODE}")"

WORKERS="${1:-8}"
GAMES_PER_SIDE="${2:-50}"
DEPTH="${3:-8}"
MAX_PLIES="${4:-150}"
MOVETIME_MS="${5:-100}"
FEN_OR_BOOK="${6:-book}"

PARALLEL="${ROOT_DIR}/scripts/run_matches_parallel.sh"
ELO_SCRIPT="${ROOT_DIR}/scripts/elo_from_logs.sh"

if [[ ! -x "${PARALLEL}" ]]; then
  echo "ERROR: ${PARALLEL} not found." >&2
  exit 1
fi

echo "Baseline:    ${BASELINE_NAME} = 0 Elo"
echo "Challenger:  v${CHALLENGER}"
echo "Config:      workers=${WORKERS} games_per_side=${GAMES_PER_SIDE} depth=${DEPTH} max_plies=${MAX_PLIES} movetime_ms=${MOVETIME_MS} openings=${FEN_OR_BOOK}"
echo ""

/usr/bin/env bash "${PARALLEL}" "${WORKERS}" "${GAMES_PER_SIDE}" "${DEPTH}" "${MAX_PLIES}" \
  "${MOVETIME_MS}" "${FEN_OR_BOOK}" "${BASELINE_VERSION}" "${CHALLENGER}"

LOG_DIR="$(ls -dt "${ROOT_DIR}/scripts/match_logs/"[0-9]* 2>/dev/null | head -1)"
if [[ -z "${LOG_DIR}" || ! -d "${LOG_DIR}" ]]; then
  echo "ERROR: could not find match log directory." >&2
  exit 1
fi

echo ""
echo "Log directory: ${LOG_DIR}"
echo ""
/usr/bin/env bash "${ELO_SCRIPT}" "${BASELINE_MODE}" "${CHALLENGER}" "${LOG_DIR}"/worker_*.log
