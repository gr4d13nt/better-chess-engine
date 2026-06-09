#!/usr/bin/env bash
# Requires bash.
if [[ -z "${BASH_VERSION:-}" ]]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

# Estimate Elo difference from run_matches / run_matches_parallel logs.
#
# Named baselines (see elo_baselines.sh):
#   historical  — v5  = 0 Elo (long-arc progress)
#   development — v25 = 0 Elo (v26+ tuning)
#
# Usage:
#   ./scripts/elo_from_logs.sh <log1> [log2 ...]
#   ./scripts/elo_from_logs.sh <version_a> <version_b> <log1> [log2 ...]
#   ./scripts/elo_from_logs.sh <historical|development> <challenger> <log1> [log2 ...]
#
# Examples:
#   ./scripts/elo_from_logs.sh dev 30 scripts/match_logs/20260609_*/worker_*.log
#   ./scripts/elo_from_logs.sh historical 18 scripts/match_logs/20260609_*/worker_*.log
#   ./scripts/elo_from_logs.sh 25 30 scripts/match_logs/latest/worker_*.log
#
# One-shot match + Elo:
#   ./scripts/run_baseline_match.sh development 30

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=elo_baselines.sh
source "${ROOT_DIR}/scripts/elo_baselines.sh"

VERSION_A=""
VERSION_B=""
BASELINE_LABEL=""
LOGS=()

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 [historical|development challenger | version_a version_b] <log1> [log2 ...]" >&2
  exit 1
fi

if [[ $# -ge 3 && "${1}" =~ ^^(historical|hist|h|development|dev|d)$ && "${2}" =~ ^[0-9]+$ && -f "${3}" ]]; then
  BASELINE_MODE="${1}"
  VERSION_A="$(elo_baseline_version "${BASELINE_MODE}")"
  VERSION_B="${2}"
  BASELINE_LABEL="$(elo_baseline_label "${BASELINE_MODE}")"
  shift 2
elif [[ $# -ge 3 && "${1}" =~ ^[0-9]+$ && "${2}" =~ ^[0-9]+$ && -f "${3}" ]]; then
  VERSION_A="${1}"
  VERSION_B="${2}"
  shift 2
fi

while [[ $# -gt 0 ]]; do
  if [[ ! -f "${1}" ]]; then
    echo "ERROR: log file not found: ${1}" >&2
    exit 1
  fi
  LOGS+=("${1}")
  shift
done

if [[ ${#LOGS[@]} -eq 0 ]]; then
  echo "ERROR: no log files given." >&2
  exit 1
fi

STATS_FILE="$(mktemp "${TMPDIR:-/tmp}/elo_from_logs.XXXXXX")"
trap 'rm -f "${STATS_FILE}"' EXIT

awk '
/^Game \(W:v[0-9]+ vs B:v[0-9]+\)/ {
  white = "";
  black = "";
  result = "";

  header = $0;
  sub(/^Game \(W:v/, "", header);
  split(header, parts, " ");
  white = parts[1];
  gsub(/[^0-9]/, "", white);

  for (i = 1; i <= NF; i++) {
    if ($i ~ /^B:v[0-9]+\)/) {
      black = $i;
      gsub(/[^0-9]/, "", black);
      break;
    }
  }

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
    exit 1;
  }

  n_versions = 0;
  for (v in versions) {
    order[n_versions++] = v;
  }

  print "total_games " total_games;
  print "draws " draws;
  print "n_versions " n_versions;
  for (i = 0; i < n_versions; i++) {
    v = order[i];
    w = (v in wins) ? wins[v] : 0;
    l = (v in losses) ? losses[v] : 0;
    d = (v in draws_by_version) ? draws_by_version[v] : 0;
    printf("version %s wins %d losses %d draws %d\n", v, w, l, d);
  }
}
' "${LOGS[@]}" > "${STATS_FILE}"

if [[ ! -s "${STATS_FILE}" ]]; then
  echo "ERROR: no parsable game lines found in logs." >&2
  exit 1
fi

if [[ -z "${VERSION_A}" || -z "${VERSION_B}" ]]; then
  FOUND_VERSIONS=()
  while IFS= read -r v; do
    FOUND_VERSIONS+=("${v}")
  done < <(awk '/^version / { print $2 }' "${STATS_FILE}" | sort -n)
  if [[ ${#FOUND_VERSIONS[@]} -ne 2 ]]; then
    echo "ERROR: expected exactly 2 versions in logs; found: ${FOUND_VERSIONS[*]:-none}" >&2
    echo "Pass versions explicitly: $0 <version_a> <version_b> <logs...>" >&2
    exit 1
  fi
  VERSION_A="${FOUND_VERSIONS[0]}"
  VERSION_B="${FOUND_VERSIONS[1]}"
fi

python3 - "${STATS_FILE}" "${VERSION_A}" "${VERSION_B}" "${#LOGS[@]}" "${BASELINE_LABEL}" <<'PY'
import math
import sys

stats_path, version_a, version_b, n_logs, baseline_label = sys.argv[1:6]
version_a = int(version_a)
version_b = int(version_b)
n_logs = int(n_logs)

total_games = 0
draws = 0
records = {}

with open(stats_path, encoding="utf-8") as f:
    for line in f:
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "total_games":
            total_games = int(parts[1])
        elif parts[0] == "draws":
            draws = int(parts[1])
        elif parts[0] == "version":
            v = int(parts[1])
            records[v] = {
                "wins": int(parts[3]),
                "losses": int(parts[5]),
                "draws": int(parts[7]),
            }

for v in (version_a, version_b):
    if v not in records:
        print(f"ERROR: version v{v} not found in logs.", file=sys.stderr)
        sys.exit(1)

ra = records[version_a]
rb = records[version_b]
score_a = ra["wins"] + 0.5 * ra["draws"]
score_b = rb["wins"] + 0.5 * rb["draws"]
pa = score_a / total_games
pb = score_b / total_games

if abs((score_a + score_b) - total_games) > 0.01:
    print(
        f"WARNING: scores do not sum cleanly ({score_a} + {score_b} vs {total_games} games).",
        file=sys.stderr,
    )

def elo_diff(p: float) -> float:
    p = min(max(p, 1e-6), 1.0 - 1e-6)
    return 400.0 * math.log10(p / (1.0 - p))

def wilson_interval(successes: float, n: int, z: float = 1.96) -> tuple[float, float]:
    if n <= 0:
        return (0.0, 0.0)
    phat = successes / n
    z2 = z * z
    denom = 1.0 + z2 / n
    center = (phat + z2 / (2.0 * n)) / denom
    margin = (z / denom) * math.sqrt((phat * (1.0 - phat) / n) + (z2 / (4.0 * n * n)))
    low = max(0.0, center - margin)
    high = min(1.0, center + margin)
    return (low, high)

def fmt_pct(p: float) -> str:
    return f"{100.0 * p:.2f}%"

elo_b = elo_diff(pb)
low_b, high_b = wilson_interval(score_b, total_games)
elo_low = elo_diff(low_b)
elo_high = elo_diff(high_b)

print("=== Elo estimate ===")
if baseline_label:
    print(f"Baseline:      {baseline_label} = 0 Elo")
print(f"Log files:     {n_logs}")
print(f"Total games:   {total_games}")
print(f"Draws:         {draws}")
print("")
print(f"v{version_a} (reference): W={ra['wins']} D={ra['draws']} L={ra['losses']}  "
      f"score={score_a:.1f}/{total_games} ({fmt_pct(pa)})")
print(f"v{version_b}:              W={rb['wins']} D={rb['draws']} L={rb['losses']}  "
      f"score={score_b:.1f}/{total_games} ({fmt_pct(pb)})")
print("")
print(f"v{version_b} vs v{version_a}: {elo_b:+.1f} Elo  (95% CI: {elo_low:+.1f} to {elo_high:+.1f})")
print(f"Score-rate CI (v{version_b}): {fmt_pct(low_b)} to {fmt_pct(high_b)}")
print("")
if total_games < 200:
    print("Note: <200 games — expect wide CI; treat as directional only.")
elif total_games < 800:
    print("Note: 200–799 games — reasonable estimate; 800+ is better for publishing a number.")
else:
    print("Note: 800+ games — solid single-batch estimate; rerun once to confirm stability.")
PY
