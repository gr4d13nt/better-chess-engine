#!/usr/bin/env bash

set -euo pipefail

# Aggregate one or more run_matches log files.
#
# Usage:
#   ./scripts/aggregate_match_logs.sh <log1> [log2 ...]
#   ./scripts/aggregate_match_logs.sh scripts/match_logs/20260528_120000/*.log
#
# Parses per-game lines like:
#   Game (W:v5 vs B:v6) ... -> result 1-0 ...
# and returns combined W/D/L for each version.

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <log1> [log2 ...]" >&2
  exit 1
fi

awk '
BEGIN {
  total_games = 0;
  draws = 0;
}

/^Game \(W:v[0-9]+ vs B:v[0-9]+\)/ {
  white = "";
  black = "";
  result = "";

  header = $0;
  sub(/^Game \(W:v/, "", header);          # "5 vs B:v6) ..."
  split(header, parts, " ");
  white = parts[1];
  gsub(/[^0-9]/, "", white);

  # Find "B:v<digits>)"
  for (i = 1; i <= NF; i++) {
    if ($i ~ /^B:v[0-9]+\)/) {
      black = $i;
      gsub(/[^0-9]/, "", black);
      break;
    }
  }

  # Result token follows the word "result"
  for (i = 1; i <= NF; i++) {
    if ($i == "result" && i + 1 <= NF) {
      result = $(i + 1);
      break;
    }
  }

  if (white == "" || black == "" || result == "") {
    next;
  }

  versions[white] = 1;
  versions[black] = 1;
  total_games++;

  if (result == "1-0") {
    wins[white]++;
    losses[black]++;
  } else if (result == "0-1") {
    wins[black]++;
    losses[white]++;
  } else {
    draws++;
    draws_by_version[white]++;
    draws_by_version[black]++;
  }
}

END {
  if (total_games == 0) {
    print "No parsable game lines found.";
    exit 1;
  }

  print "=== Aggregated Match Summary ===";
  print "Log files: " ARGC - 1;
  print "Total games: " total_games;
  print "Draws: " draws;
  print "";

  for (v in versions) {
    w = (v in wins) ? wins[v] : 0;
    l = (v in losses) ? losses[v] : 0;
    d = (v in draws_by_version) ? draws_by_version[v] : 0;
    score = w + 0.5 * d;
    pct = (100.0 * score) / total_games;
    printf("v%s: W=%d D=%d L=%d  score=%.1f/%.0f (%.2f%%)\n", v, w, d, l, score, total_games, pct);
  }
}
' "$@"
