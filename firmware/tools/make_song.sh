#!/usr/bin/env bash
# Regenerate the full-song SPIFFS asset for Xuss-C.
#
# Downloads the operator-owned source track and converts it to the product's
# on-device PCM format: unsigned 8-bit, mono, 22050 Hz (1 byte == 1 sample).
# The resulting firmware/spiffs/first.u8 is large (~4 MB) and is .gitignored;
# run this once on a fresh checkout before `idf.py build`.
set -euo pipefail

SRC_URL="${XUSSC_SONG_URL:-https://kindel.com/tig/First.mp3}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"       # firmware/
OUT_DIR="$HERE/spiffs"
OUT="$OUT_DIR/first.u8"
RATE="${XUSSC_SONG_RATE:-22050}"

mkdir -p "$OUT_DIR"
tmp="$(mktemp -t first_src.XXXX.mp3)"
trap 'rm -f "$tmp"' EXIT

echo "Downloading $SRC_URL ..."
curl -fsSL -o "$tmp" "$SRC_URL"

echo "Converting to u8/$RATE/mono -> $OUT"
ffmpeg -y -loglevel error -i "$tmp" -ac 1 -ar "$RATE" -f u8 "$OUT"

echo "Done: $(du -h "$OUT" | cut -f1) at $RATE Hz mono u8"
