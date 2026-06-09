#!/usr/bin/env bash
# Shared Elo anchor versions (source this file; do not run directly).
#
# historical  — long-arc progress (v5 move ordering). Strong versions will saturate
#               this scale (~95%+); use for milestones, not fine tuning.
# development — recent-feature comparisons (v25 opening book). Use for v26+ work.

ELO_HISTORICAL_BASELINE=5
ELO_DEVELOPMENT_BASELINE=25

elo_baseline_version() {
  case "${1}" in
    historical|hist|h)
      echo "${ELO_HISTORICAL_BASELINE}"
      ;;
    development|dev|d)
      echo "${ELO_DEVELOPMENT_BASELINE}"
      ;;
    *)
      echo "ERROR: unknown baseline '${1}' (use historical or development)." >&2
      return 1
      ;;
  esac
}

elo_baseline_label() {
  case "${1}" in
    historical|hist|h) echo "historical (v${ELO_HISTORICAL_BASELINE})" ;;
    development|dev|d) echo "development (v${ELO_DEVELOPMENT_BASELINE})" ;;
    *) echo "custom" ;;
  esac
}
