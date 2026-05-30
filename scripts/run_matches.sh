#!/usr/bin/env bash
# Requires bash (process substitution, [[ ]], arrays).
if [[ -z "${BASH_VERSION:-}" ]]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

# Runs multiple self-play games between two engine versions and prints W/D/L.
#
# Usage:
#   ./scripts/run_matches.sh [games_per_side] [depth] [max_plies] [movetime_ms] [fen_or_book] [version_a] [version_b]
#
# Defaults:
#   games_per_side = 2
#   depth          = 3
#   max_plies      = 120
#   movetime_ms    = 0 (disabled)
#   fen_or_book    = book (random from scripts/equal_openings.txt)
#   version_a      = 1
#   version_b      = 2
#
# Engine versions:
#   v1 = no pruning
#   v2 = alpha-beta
#   v3 = improved eval
#   v4 = quiescence (alpha-beta + qsearch)
#   v5 = move ordering (v4 + ordered main/qsearch moves)
#   v6 = v5 search + pawn structure & king safety eval
#   v7 = v5 search + phased/tapered eval (MG/EG) + v6 terms + tempo
#   v8 = v7 eval + knight outpost bonuses
#   v9 = v8 eval + rook on open/semi-open files; rank handled by rook PST
#   v10 = v5 search + transposition table; v7 phased eval only
#   v11 = v10 + TT hash-move ordering + iterative-deepening PV move at root
#   v12 = v11 + one-ply check extension per line (max 1)
#   v13 = v12 search + v7 eval + endgame mop-up (king approach / drive to edge)
#   v14 = v13 search/eval + treat revisiting any prior position as a draw in search
#   v15 = v14 search + v9 piece terms (knight/rook/bishop) + v13 mop-up eval
#   v16 = v15 search/eval + SEE pruning for losing captures in qsearch (depth>=1 only)
#   v17 = v16 search + delta pruning for hopeless captures in qsearch (qdepth>=1, margin 150)
#   v18 = v17 search + null-move pruning in main search (depth>=3, R=2/3)

GAMES_PER_SIDE="${1:-2}"
DEPTH="${2:-3}"
MAX_PLIES="${3:-120}"
MOVETIME_MS=0
FEN_OR_BOOK="book"

if [[ $# -ge 4 ]]; then
  if [[ "${4}" =~ ^[0-9]+$ ]]; then
    MOVETIME_MS="${4}"
    FEN_OR_BOOK="${5:-book}"
  else
    FEN_OR_BOOK="${4}"
  fi
fi

VERSION_A="${6:-1}"
VERSION_B="${7:-2}"

if ! [[ "${GAMES_PER_SIDE}" =~ ^[0-9]+$ ]] || [[ "${GAMES_PER_SIDE}" -lt 1 ]]; then
  echo "ERROR: games_per_side must be a positive integer (got '${GAMES_PER_SIDE}')." >&2
  exit 1
fi
if ! [[ "${VERSION_A}" =~ ^[0-9]+$ ]] || ! [[ "${VERSION_B}" =~ ^[0-9]+$ ]]; then
  echo "ERROR: version_a and version_b must be integers." >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/selfplay_main"
OPENINGS_FILE="${ROOT_DIR}/scripts/equal_openings.txt"

if [[ ! -x "${BIN}" ]]; then
  echo "Building project..."
  cmake --build "${ROOT_DIR}/build"
fi

version_a_wins=0
version_b_wins=0
draws=0

pick_opening_fen() {
  if [[ "${FEN_OR_BOOK}" != "book" ]]; then
    printf "%s\n" "${FEN_OR_BOOK}"
    return
  fi

  if [[ ! -f "${OPENINGS_FILE}" ]]; then
    echo "ERROR: openings file not found: ${OPENINGS_FILE}" >&2
    exit 1
  fi

  local openings=()
  while IFS= read -r line; do
    openings+=("${line}")
  done < <(awk 'NF && $1 !~ /^#/' "${OPENINGS_FILE}")
  if [[ ${#openings[@]} -eq 0 ]]; then
    echo "ERROR: openings file has no usable FENs: ${OPENINGS_FILE}" >&2
    exit 1
  fi
  local idx=$((RANDOM % ${#openings[@]}))
  printf "%s\n" "${openings[idx]}"
}

run_game() {
  local white_version="$1"
  local black_version="$2"
  local game_fen
  game_fen="$(pick_opening_fen)"

  local out
  out="$("${BIN}" "${white_version}" "${black_version}" "${DEPTH}" "${MAX_PLIES}" "${MOVETIME_MS}" "${game_fen}")"
  local result_line
  result_line="$(printf "%s\n" "${out}" | awk '/^result /{print $0}' | tail -1)"
  local result
  result="$(printf "%s\n" "${result_line}" | awk '{print $2}')"

  if [[ -z "${result}" ]]; then
    echo "ERROR: could not parse result."
    printf "%s\n" "${out}"
    exit 1
  fi

  if [[ "${result}" == "1-0" ]]; then
    if [[ "${white_version}" -eq "${VERSION_A}" ]]; then
      version_a_wins=$((version_a_wins + 1))
    else
      version_b_wins=$((version_b_wins + 1))
    fi
  elif [[ "${result}" == "0-1" ]]; then
    if [[ "${black_version}" -eq "${VERSION_A}" ]]; then
      version_a_wins=$((version_a_wins + 1))
    else
      version_b_wins=$((version_b_wins + 1))
    fi
  else
    draws=$((draws + 1))
  fi

  # %q safely escapes FEN for display (no fragile single-quote wrapping).
  printf 'Game (W:v%s vs B:v%s) FEN=%q -> %s\n' \
    "${white_version}" "${black_version}" "${game_fen}" "${result_line}"
}

run_games_for_side() {
  local white_version="$1"
  local black_version="$2"
  local i

  for i in $(seq 1 "${GAMES_PER_SIDE}"); do
    run_game "${white_version}" "${black_version}"
  done
}

echo "Running matches: games_per_side=${GAMES_PER_SIDE}, depth=${DEPTH}, max_plies=${MAX_PLIES}, movetime_ms=${MOVETIME_MS}, fen_or_book=${FEN_OR_BOOK}, version_a=v${VERSION_A}, version_b=v${VERSION_B}"
echo ""

echo "Batch 1/2: v${VERSION_A} white vs v${VERSION_B} black, ${GAMES_PER_SIDE} games"
run_games_for_side "${VERSION_A}" "${VERSION_B}"

echo "Batch 2/2: v${VERSION_B} white vs v${VERSION_A} black, ${GAMES_PER_SIDE} games"
run_games_for_side "${VERSION_B}" "${VERSION_A}"

total_games=$((2 * GAMES_PER_SIDE))
echo ""
echo "=== Match Summary ==="
echo "Total games: ${total_games}"
echo "v${VERSION_A} wins: ${version_a_wins}"
echo "v${VERSION_B} wins: ${version_b_wins}"
echo "draws:   ${draws}"
