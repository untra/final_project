#!/usr/bin/env bash
#
# Launch ./final, screenshot its window, kill it. macOS only.
#
# First run on a clean machine triggers macOS Accessibility + Screen Recording
# permission prompts for whatever terminal/agent is invoking the script. There
# is no permission-free path. Subsequent runs work without prompts.

set -euo pipefail

if [[ "$(uname)" != "Darwin" ]]; then
  echo "native-shot.sh: macOS only" >&2
  exit 2
fi

OUT="${1:-build/native/probe/latest.png}"
SETTLE_SEC="${NATIVE_SHOT_SETTLE_SEC:-1.5}"

mkdir -p "$(dirname "$OUT")"

if [[ ! -x ./final ]]; then
  echo "native-shot.sh: ./final not found or not executable. Run 'make' first." >&2
  exit 2
fi

./final &
PID=$!
trap 'kill "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true' EXIT
sleep "$SETTLE_SEC"

# AXWindowNumber is a CGWindowID screencapture -l understands. Retina-correct
# (avoids point-vs-pixel math you'd hit with -R x,y,w,h).
WID="$(osascript <<'EOF'
tell application "System Events"
  tell process "final"
    return value of attribute "AXWindowNumber" of window 1
  end tell
end tell
EOF
)" || {
  echo "native-shot.sh: osascript failed. If this is the first run, click through" >&2
  echo "                the macOS Accessibility + Screen Recording prompts and retry." >&2
  exit 1
}

screencapture -x -l "$WID" "$OUT"

STAMP=$(date -u +%Y-%m-%dT%H-%M-%SZ)
cp "$OUT" "$(dirname "$OUT")/${STAMP}.png"

echo "wrote $OUT"
