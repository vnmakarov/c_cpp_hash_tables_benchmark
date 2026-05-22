#!/bin/bash
# Extract heatmap SVGs from a result HTML file and convert to PNG.
# Usage: ./extract_heatmaps.sh result_YYYY-MM-DDTHH_MM_SS.html

set -e

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <result_*.html>"
  exit 1
fi

HTML="$1"
if [[ ! -f "$HTML" ]]; then
  echo "Error: file not found: $HTML"
  exit 1
fi

BASE="${HTML%.html}"

# Count total SVGs to find the last two (heatmaps).
TOTAL_SVGS=$(grep -c '<svg ' "$HTML")
PERF_IDX=$((TOTAL_SVGS - 1))  # second to last
MEM_IDX=$TOTAL_SVGS            # last

# Extract nth SVG block using awk.
extract_svg() {
  local n=$1
  awk -v target="$n" '
    /<svg / { count++; if (count == target) capturing = 1 }
    capturing { print }
    capturing && /<\/svg>/ { exit }
  ' "$HTML"
}

echo "Extracting performance heatmap (SVG #$PERF_IDX of $TOTAL_SVGS)..."
extract_svg "$PERF_IDX" > "${BASE}_heatmap_perf.svg"

echo "Extracting memory heatmap (SVG #$MEM_IDX of $TOTAL_SVGS)..."
extract_svg "$MEM_IDX" > "${BASE}_heatmap_mem.svg"

# Convert to PNG.
if command -v rsvg-convert &>/dev/null; then
  CONVERT="rsvg-convert"
  rsvg-convert -d 200 -p 200 "${BASE}_heatmap_perf.svg" -o "${BASE}_heatmap_perf.png"
  rsvg-convert -d 200 -p 200 "${BASE}_heatmap_mem.svg" -o "${BASE}_heatmap_mem.png"
elif command -v inkscape &>/dev/null; then
  CONVERT="inkscape"
  inkscape "${BASE}_heatmap_perf.svg" --export-type=png --export-dpi=200 --export-filename="${BASE}_heatmap_perf.png" 2>/dev/null
  inkscape "${BASE}_heatmap_mem.svg" --export-type=png --export-dpi=200 --export-filename="${BASE}_heatmap_mem.png" 2>/dev/null
elif python3 -c "import cairosvg" 2>/dev/null; then
  CONVERT="cairosvg"
  python3 -c "import cairosvg; cairosvg.svg2png(url='${BASE}_heatmap_perf.svg', write_to='${BASE}_heatmap_perf.png', dpi=200)"
  python3 -c "import cairosvg; cairosvg.svg2png(url='${BASE}_heatmap_mem.svg', write_to='${BASE}_heatmap_mem.png', dpi=200)"
elif command -v convert &>/dev/null; then
  CONVERT="imagemagick"
  convert -density 200 "${BASE}_heatmap_perf.svg" "${BASE}_heatmap_perf.png"
  convert -density 200 "${BASE}_heatmap_mem.svg" "${BASE}_heatmap_mem.png"
else
  echo "No SVG-to-PNG converter found (tried rsvg-convert, inkscape, cairosvg, imagemagick)."
  echo "SVG files saved:"
  echo "  ${BASE}_heatmap_perf.svg"
  echo "  ${BASE}_heatmap_mem.svg"
  exit 0
fi

echo "Done ($CONVERT). Files:"
echo "  ${BASE}_heatmap_perf.png"
echo "  ${BASE}_heatmap_mem.png"
