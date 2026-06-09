#!/usr/bin/env bash
# Requires bash.
if [[ -z "${BASH_VERSION:-}" ]]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

# Measure Elo for many versions using the two baseline scales.
#
# Baselines (elo_baselines.sh):
#   historical  — v5  = 0 Elo  →  measures v6–v24 (and optionally v25–v30)
#   development — v25 = 0 Elo  →  measures v26–v30
#
# Modes:
#   all         — v6–v24 vs v5, then v26–v30 vs v25 (recommended full ladder)
#   historical  — v6..max vs v5
#   development — v26..max vs v25
#
# Usage:
#   ./scripts/run_elo_ladder.sh [mode] [max_version] [workers] [games_per_side] [depth] [max_plies] [movetime_ms] [fen_or_book]
#
# Defaults:
#   mode=all, max_version=30, workers=8, games_per_side=50 (800 games/pair),
#   depth=8, max_plies=150, movetime_ms=100, fen_or_book=book
#
# Examples:
#   ./scripts/run_elo_ladder.sh
#   ./scripts/run_elo_ladder.sh all 30 8 50 8 150 100 book
#   ./scripts/run_elo_ladder.sh historical 30 8 10 8 150 100 book   # quick survey (~320 games/pair)
#
# Output:
#   scripts/match_logs/ladder_<timestamp>.csv
#   scripts/match_logs/ladder_<timestamp>.txt  (human-readable table)
#
# Time: `all` with defaults is ~29 match suites (v6–v30). Expect many hours.
#       Use games_per_side=10 first for a coarse ladder, then rerun key versions at 50.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=elo_baselines.sh
source "${ROOT_DIR}/scripts/elo_baselines.sh"

MODE="${1:-all}"
MAX_VERSION="${2:-30}"
WORKERS="${3:-8}"
GAMES_PER_SIDE="${4:-50}"
DEPTH="${5:-8}"
MAX_PLIES="${6:-150}"
MOVETIME_MS="${7:-100}"
FEN_OR_BOOK="${8:-book}"

if [[ "${MODE}" != "all" && "${MODE}" != "historical" && "${MODE}" != "development" && "${MODE}" != "hist" && "${MODE}" != "dev" ]]; then
  echo "ERROR: mode must be all, historical, or development (got '${MODE}')." >&2
  exit 1
fi

if ! [[ "${MAX_VERSION}" =~ ^[0-9]+$ ]] || [[ "${MAX_VERSION}" -lt 5 ]]; then
  echo "ERROR: max_version must be an integer >= 5 (got '${MAX_VERSION}')." >&2
  exit 1
fi

PARALLEL="${ROOT_DIR}/scripts/run_matches_parallel.sh"
ELO_SCRIPT="${ROOT_DIR}/scripts/elo_from_logs.sh"
BIN="${ROOT_DIR}/build/selfplay_main"

if [[ ! -x "${BIN}" ]]; then
  echo "Building project..."
  cmake --build "${ROOT_DIR}/build"
fi

STAMP="$(date +"%Y%m%d_%H%M%S")"
CSV="${ROOT_DIR}/scripts/match_logs/ladder_${STAMP}.csv"
TABLE="${ROOT_DIR}/scripts/match_logs/ladder_${STAMP}.txt"
mkdir -p "${ROOT_DIR}/scripts/match_logs"

echo "version,baseline,elo,elo_lo,elo_hi,score_pct,games,log_dir" > "${CSV}"

plan_pair() {
  local version="$1"
  local mode=""

  if [[ "${MODE}" == "all" ]]; then
    if [[ "${version}" -ge 6 && "${version}" -le 24 ]]; then
      mode="historical"
    elif [[ "${version}" -ge 26 ]]; then
      mode="development"
    else
      return 1
    fi
  elif [[ "${MODE}" == "historical" || "${MODE}" == "hist" ]]; then
    [[ "${version}" -ge 6 ]] || return 1
    mode="historical"
  else
    [[ "${version}" -ge 26 ]] || return 1
    mode="development"
  fi

  if [[ "${version}" -gt "${MAX_VERSION}" ]]; then
    return 1
  fi

  printf '%s %s\n' "${mode}" "${version}"
}

PAIRS=()
for ((v = 6; v <= MAX_VERSION; ++v)); do
  if out="$(plan_pair "${v}")"; then
    PAIRS+=("${out}")
  fi
done

if [[ ${#PAIRS[@]} -eq 0 ]]; then
  echo "ERROR: no version pairs to run for mode=${MODE} max_version=${MAX_VERSION}." >&2
  exit 1
fi

TOTAL_GAMES_PER_PAIR=$((2 * GAMES_PER_SIDE * WORKERS))
echo "Elo ladder run"
echo "  mode:              ${MODE}"
echo "  versions:          v5+ through v${MAX_VERSION}"
echo "  pairs to run:      ${#PAIRS[@]}"
echo "  games per pair:    ${TOTAL_GAMES_PER_PAIR}"
echo "  TC:                depth=${DEPTH} max_plies=${MAX_PLIES} movetime_ms=${MOVETIME_MS} openings=${FEN_OR_BOOK}"
echo "  anchors:           historical=v${ELO_HISTORICAL_BASELINE}  development=v${ELO_DEVELOPMENT_BASELINE}"
echo "  results:           ${CSV}"
echo ""

declare -A ELO_HIST=()
declare -A ELO_DEV=()
declare -A GAMES_HIST=()
declare -A GAMES_DEV=()

run_pair() {
  local mode="$1"
  local challenger="$2"
  local baseline_version
  baseline_version="$(elo_baseline_version "${mode}")"

  echo "================================================================"
  echo "Measuring v${challenger} (${mode} baseline v${baseline_version})"
  echo "================================================================"

  /usr/bin/env bash "${PARALLEL}" "${WORKERS}" "${GAMES_PER_SIDE}" "${DEPTH}" "${MAX_PLIES}" \
    "${MOVETIME_MS}" "${FEN_OR_BOOK}" "${baseline_version}" "${challenger}"

  local log_dir
  log_dir="$(ls -dt "${ROOT_DIR}/scripts/match_logs/"[0-9]* 2>/dev/null | head -1)"
  if [[ -z "${log_dir}" || ! -d "${log_dir}" ]]; then
    echo "ERROR: missing log dir for v${challenger}" >&2
    return 1
  fi

  local elo_out
  elo_out="$(/usr/bin/env bash "${ELO_SCRIPT}" "${mode}" "${challenger}" "${log_dir}"/worker_*.log)"

  local parsed
  parsed="$(printf '%s\n' "${elo_out}" | python3 -c "
import re, sys
text = sys.stdin.read()
elo = lo = hi = score_pct = games = None
m = re.search(r'v\\d+ vs v\\d+: ([+-]?[0-9.]+) Elo\\s+\\(95% CI: ([+-]?[0-9.]+) to ([+-]?[0-9.]+)\\)', text)
if m:
    elo, lo, hi = m.group(1), m.group(2), m.group(3)
m = re.search(r'Total games:\\s+(\\d+)', text)
if m:
    games = m.group(1)
m = re.search(r'v\\d+:\\s+W=\\d+ D=\\d+ L=\\d+\\s+score=[0-9.]+/\\d+ \\(([0-9.]+)%\\)', text)
if m:
    score_pct = m.group(1)
if elo is None:
    sys.exit(1)
print(f'{elo},{lo},{hi},{score_pct or \"\"},{games or \"\"}')
")"

  IFS=',' read -r elo elo_lo elo_hi score_pct games <<< "${parsed}"

  echo "${challenger},${mode},${elo},${elo_lo},${elo_hi},${score_pct},${games},${log_dir}" >> "${CSV}"

  if [[ "${mode}" == "historical" ]]; then
    ELO_HIST["${challenger}"]="${elo}"
    GAMES_HIST["${challenger}"]="${games}"
  else
    ELO_DEV["${challenger}"]="${elo}"
    GAMES_DEV["${challenger}"]="${games}"
  fi

  printf '\n'
}

set +e
FAILED=0
for entry in "${PAIRS[@]}"; do
  mode="${entry%% *}"
  challenger="${entry##* }"
  if ! run_pair "${mode}" "${challenger}"; then
    echo "ERROR: failed v${challenger} (${mode})" >&2
    FAILED=$((FAILED + 1))
  fi
done
set -e

{
  echo "Elo ladder — ${STAMP}"
  echo "mode=${MODE}  max_version=v${MAX_VERSION}  movetime_ms=${MOVETIME_MS}  games_per_side=${GAMES_PER_SIDE}"
  echo ""
  printf "%-8s %14s %14s\n" "version" "Elo vs v5" "Elo vs v25"
  printf "%-8s %14s %14s\n" "-------" "---------" "----------"
  printf "%-8s %14s %14s\n" "v5" "0" "—"
  for ((v = 6; v <= MAX_VERSION; ++v)); do
    h="${ELO_HIST[$v]:-—}"
    d="${ELO_DEV[$v]:-—}"
    if [[ "${v}" -eq 25 ]]; then
      d="0"
    fi
    if [[ "${h}" == "—" && "${d}" == "—" && "${v}" -ne 25 ]]; then
      continue
    fi
    if [[ "${h}" != "—" && "${h}" != "" ]]; then
      h="$(printf '%+.0f' "${h}")"
    fi
    if [[ "${d}" != "—" && "${d}" != "" && "${d}" != "0" ]]; then
      d="$(printf '%+.0f' "${d}")"
    fi
    printf "%-8s %14s %14s\n" "v${v}" "${h}" "${d}"
  done
} | tee "${TABLE}"

echo ""
echo "CSV:   ${CSV}"
echo "Table: ${TABLE}"
if [[ "${FAILED}" -gt 0 ]]; then
  echo "WARNING: ${FAILED} pair(s) failed." >&2
  exit 1
fi
