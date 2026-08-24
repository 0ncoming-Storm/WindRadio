#!/usr/bin/env bash
# pond-set-clock — set the pond node's DS3231 RTC from NTP, over serial.
#
# Usage: ./pond-set-clock.sh [serial-port]
#
# Flow:
#   1. Fetch current UTC epoch from worldtimeapi.org (NTP-backed).
#   2. Ask the sketch for a time-set window (SETCLOCK command).
#   3. Send "T<epoch>" — the sketch applies it as a UTC epoch and echoes
#      back the resulting local date/time for verification.

set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
BAUD=115200

echo "[*] Fetching time from worldtimeapi.org..."
EPOCH=$(curl -s --max-time 10 "https://worldtimeapi.org/api/timezone/Etc/UTC" \
        | python3 -c 'import json,sys; print(json.load(sys.stdin)["unixtime"])')
if ! [[ "$EPOCH" =~ ^[0-9]+$ ]]; then
  echo "ERROR: could not fetch NTP time (got: '$EPOCH')" >&2
  exit 1
fi
echo "[*] NTP epoch: $EPOCH"

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
