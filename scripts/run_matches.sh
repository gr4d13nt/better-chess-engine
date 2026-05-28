#!/usr/bin/env bash

set -euo pipefail

# Runs multiple self-play games between two engine versions and prints W/D/L.
#
# Usage:
#   ./scripts/run_matches.sh [games_per_side] [depth] [max_plies] [movetime_ms] [fen_or_book]
#
# Defaults:
#   games_per_side = 2
#   depth          = 3
#   max_plies      = 120
#   movetime_ms    = 0 (disabled)
#   fen_or_book    = book (random from scripts/equal_openings.txt)
#
# Engine versions are currently fixed as:
#   v1 = no pruning
#   v2 = alpha-beta

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

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/selfplay_main"
OPENINGS_FILE="${ROOT_DIR}/scripts/equal_openings.txt"

if [[ ! -x "${BIN}" ]]; then
  echo "Building project..."
  cmake --build "${ROOT_DIR}/build"
fi

v1_wins=0
v2_wins=0
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
    if [[ "${white_version}" -eq 1 ]]; then
      v1_wins=$((v1_wins + 1))
    else
      v2_wins=$((v2_wins + 1))
    fi
  elif [[ "${result}" == "0-1" ]]; then
    if [[ "${black_version}" -eq 1 ]]; then
      v1_wins=$((v1_wins + 1))
    else
      v2_wins=$((v2_wins + 1))
    fi
  else
    draws=$((draws + 1))
  fi

  printf "Game (W:v%s vs B:v%s) FEN='%s' -> %s\n" \
    "${white_version}" "${black_version}" "${game_fen}" "${result_line}"
}

echo "Running matches: games_per_side=${GAMES_PER_SIDE}, depth=${DEPTH}, max_plies=${MAX_PLIES}, movetime_ms=${MOVETIME_MS}, fen_or_book=${FEN_OR_BOOK}"
echo ""

for ((i = 1; i <= GAMES_PER_SIDE; i++)); do
  run_game 1 2
done

for ((i = 1; i <= GAMES_PER_SIDE; i++)); do
  run_game 2 1
done

total_games=$((2 * GAMES_PER_SIDE))
echo ""
echo "=== Match Summary ==="
echo "Total games: ${total_games}"
echo "v1 wins: ${v1_wins}"
echo "v2 wins: ${v2_wins}"
echo "draws:   ${draws}"
