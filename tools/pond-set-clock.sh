#!/usr/bin/env bash
# pond-set-clock — set the pond node's DS3231 RTC from the PC clock, over serial.
#
# Usage: ./pond-set-clock.sh [serial-port]
#
# The PC clock is NTP-synced (systemd-timesyncd), so its epoch is authoritative.
# The RTC holds UTC; the central node converts to local time for display only.

set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
BAUD=115200

echo "[*] Reading PC clock (assumed NTP-synced)..."
EPOCH=$(date +%s)
echo "[*] UTC epoch: $EPOCH ($(date -u '+%Y-%m-%d %H:%M:%S UTC'))"

# Small delay so the timestamp is fresh when it lands on the RTC.
sleep 1
EPOCH=$((EPOCH + 2))

echo "[*] Opening $PORT at $BAUD..."
# stty raw 8N1; the Feather's USB CDC doesn't need DTR tricks.
stty -F "$PORT" "$BAUD" raw -echo -crtscts

# Drain anything pending.
timeout 0.5 cat "$PORT" > /dev/null 2>&1 || true

echo "[*] Sending SETCLOCK request..."
printf 'SETCLOCK\n' > "$PORT"
sleep 0.5

echo "[*] Sending T$EPOCH..."
printf 'T%s\n' "$EPOCH" > "$PORT"
sleep 1

echo "[*] Reading response (5s)..."
RESP=$(timeout 5 cat "$PORT" || true)
echo "----- device output -----"
echo "$RESP" | tail -20
echo "-------------------------"

if echo "$RESP" | grep -q "RTC SET"; then
  echo "[OK] Clock set."
else
  echo "[FAIL] No confirmation from device." >&2
  exit 1
fi
