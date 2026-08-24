#!/usr/bin/env bash
# pond-set-clock — set the pond node's DS3231 RTC from the PC clock, over serial.
#
# Usage: ./pond-set-clock.sh [serial-port]
#
# Flash rtc-set-clock.ino to the pond board first. The PC clock is
# NTP-synced (systemd-timesyncd), so its epoch is authoritative.
# The RTC holds UTC; the central node converts to local time for display.

set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
BAUD=115200

echo "[*] Reading PC clock (assumed NTP-synced)..."
EPOCH=$(date +%s)
echo "[*] UTC epoch: $EPOCH ($(date -u '+%Y-%m-%d %H:%M:%S UTC'))"

echo "[*] Opening $PORT at $BAUD..."
stty -F "$PORT" "$BAUD" raw -echo -crtscts

# Drain anything pending.
timeout 0.5 cat "$PORT" > /dev/null 2>&1 || true

# Add ~2s so the timestamp is fresh when it lands on the RTC.
EPOCH=$((EPOCH + 2))

echo "[*] Sending T$EPOCH..."
printf 'T%s\n' "$EPOCH" > "$PORT"
sleep 1

echo "[*] Reading response (5s)..."
RESP=$(timeout 5 cat "$PORT" || true)
echo "----- device output -----"
echo "$RESP" | tail -10
echo "-------------------------"

if echo "$RESP" | grep -q "ECHO: T$EPOCH"; then
  echo "[OK] Clock set and confirmed."
else
  echo "[FAIL] No confirmation from device." >&2
  exit 1
fi
