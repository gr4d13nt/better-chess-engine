#!/usr/bin/env bash
# Download a standard Polyglot opening book used by many engines.
# Source: python-chess test data (Crafty "performance.bin" mirror).
# URL: https://github.com/niklasf/python-chess/tree/master/data/polyglot
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ROOT}/data/opening_book.bin"
URL="https://raw.githubusercontent.com/niklasf/python-chess/master/data/polyglot/performance.bin"

mkdir -p "${ROOT}/data"

if [[ -f "${OUT}" ]]; then
  echo "Opening book already exists: ${OUT} ($(wc -c < "${OUT}" | tr -d ' ') bytes)"
  exit 0
fi

echo "Downloading Polyglot opening book to ${OUT} ..."
curl -fsSL "${URL}" -o "${OUT}.tmp"
mv "${OUT}.tmp" "${OUT}"

BYTES="$(wc -c < "${OUT}" | tr -d ' ')"
ENTRIES=$((BYTES / 16))
echo "Done: ${ENTRIES} entries (${BYTES} bytes)"
