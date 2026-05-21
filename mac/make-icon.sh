#!/usr/bin/env bash
# Generate mac/AppIcon.icns from mac/icon-source.png.
# If mac/icon-source.png is missing, generates a placeholder via make-icon-png.py.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/icon-source.png"
ICONSET="$HERE/AppIcon.iconset"
OUT="$HERE/AppIcon.icns"
if [ ! -f "$SRC" ]; then
    echo "[icon] no icon-source.png — generating placeholder"
    python3 "$HERE/make-icon-png.py" "$SRC"
fi
rm -rf "$ICONSET"
mkdir -p "$ICONSET"
for spec in "16:icon_16x16.png" "32:icon_16x16@2x.png" "32:icon_32x32.png" \
            "64:icon_32x32@2x.png" "128:icon_128x128.png" "256:icon_128x128@2x.png" \
            "256:icon_256x256.png" "512:icon_256x256@2x.png" "512:icon_512x512.png" \
            "1024:icon_512x512@2x.png"; do
    size="${spec%%:*}"
    name="${spec##*:}"
    sips -z "$size" "$size" "$SRC" --out "$ICONSET/$name" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$OUT"
rm -rf "$ICONSET"
echo "[icon] wrote $OUT"
