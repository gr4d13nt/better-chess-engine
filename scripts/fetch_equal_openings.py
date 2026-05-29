#!/usr/bin/env python3

"""
Fetch a balanced online opening suite (EPD) and write sampled FENs.

Default source:
  official-stockfish/books -> Drawkiller_balanced_big.epd

Usage:
  ./scripts/fetch_equal_openings.py [count] [output_file] [seed]

Examples:
  ./scripts/fetch_equal_openings.py
  ./scripts/fetch_equal_openings.py 512
  ./scripts/fetch_equal_openings.py 128 scripts/equal_openings.txt 42
"""

from __future__ import annotations

import random
import sys
import urllib.request
from io import BytesIO
from pathlib import Path
from zipfile import ZipFile

SOURCE_URL = (
    "https://raw.githubusercontent.com/official-stockfish/books/master/"
    "Drawkiller_balanced_big.epd.zip"
)


def epd_to_fen(epd_line: str) -> str | None:
    line = epd_line.strip()
    if not line or line.startswith("#"):
        return None
    # EPD starts with 4 FEN fields (piece placement, side, castling, ep).
    parts = line.split()
    if len(parts) < 4:
        return None
    return f"{parts[0]} {parts[1]} {parts[2]} {parts[3]} 0 1"


def fetch_lines(url: str) -> list[str]:
    with urllib.request.urlopen(url, timeout=30) as response:  # nosec B310
        raw = response.read()

    if url.endswith(".zip"):
        with ZipFile(BytesIO(raw)) as zf:
            names = zf.namelist()
            if not names:
                raise RuntimeError("zip has no files")
            # Prefer first .epd entry.
            epd_name = next((n for n in names if n.lower().endswith(".epd")), names[0])
            data = zf.read(epd_name).decode("utf-8", errors="replace")
            return data.splitlines()

    data = raw.decode("utf-8", errors="replace")
    return data.splitlines()


def main() -> int:
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 256
    out_file = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("scripts/equal_openings.txt")
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 0

    if count <= 0:
        print("count must be > 0", file=sys.stderr)
        return 1

    raw_lines = fetch_lines(SOURCE_URL)
    fens = []
    seen = set()
    for line in raw_lines:
        fen = epd_to_fen(line)
        if fen and fen not in seen:
            seen.add(fen)
            fens.append(fen)

    if not fens:
        print("no openings parsed from source", file=sys.stderr)
        return 1

    rng = random.Random(seed)
    if count >= len(fens):
        sample = list(fens)
        rng.shuffle(sample)
    else:
        sample = rng.sample(fens, count)

    out_file.parent.mkdir(parents=True, exist_ok=True)
    with out_file.open("w", encoding="utf-8") as f:
        f.write("# Source: official-stockfish/books Drawkiller_balanced_big.epd\n")
        f.write(f"# URL: {SOURCE_URL}\n")
        f.write(f"# Sample size: {len(sample)}, seed: {seed}\n")
        f.write("# One FEN per line.\n\n")
        for fen in sample:
            f.write(f"{fen}\n")

    print(f"Wrote {len(sample)} openings to {out_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

