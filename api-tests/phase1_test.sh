#!/bin/bash
# PolyHome API — Phase 1 smoke test
# Flow: register -> auth (token) -> list houses -> list devices -> send command
# NOTE: the device-list step only works if the house page is open in a browser:
#   https://polyhome.lesmoulinsdudev.com?houseId=<HOUSE_ID>

BASE="https://polyhome.lesmoulinsdudev.com/api"
LOGIN="${1:-kdijon26}"
PASSWORD="${2:-chalet26}"

echo "== 1) Register =="
curl -s -w "\nHTTP %{http_code}\n" -X POST "$BASE/users/register" \
  -H "Content-Type: application/json" \
  -d "{\"login\":\"$LOGIN\",\"password\":\"$PASSWORD\"}"
# 200 = created, 409 = already exists (fine, just log in)

echo
echo "== 2) Login (the only endpoint that returns a token) =="
AUTH=$(curl -s -X POST "$BASE/users/auth" \
  -H "Content-Type: application/json" \
  -d "{\"login\":\"$LOGIN\",\"password\":\"$PASSWORD\"}")
echo "$AUTH"
TOKEN=$(echo "$AUTH" | sed -n 's/.*"token"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')
if [ -z "$TOKEN" ]; then echo "!! no token — aborting"; exit 1; fi
echo "token: ${TOKEN:0:12}..."

echo
echo "== 3) List houses =="
HOUSES=$(curl -s -H "Authorization: Bearer $TOKEN" "$BASE/houses")
echo "$HOUSES"
HOUSE_ID=$(echo "$HOUSES" | sed -n 's/.*"houseId"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' | head -1)
echo "houseId: $HOUSE_ID"

echo
echo "== 4) List devices (requires house open in a browser tab!) =="
echo "   open: https://polyhome.lesmoulinsdudev.com?houseId=$HOUSE_ID"
curl -s -w "\nHTTP %{http_code}\n" -H "Authorization: Bearer $TOKEN" \
  "$BASE/houses/$HOUSE_ID/devices"

echo
echo "== 5) To send a command, e.g.: =="
echo "curl -X POST \"$BASE/houses/$HOUSE_ID/devices/<DEVICE_ID>/command\" \\"
echo "  -H \"Authorization: Bearer $TOKEN\" \\"
echo "  -H \"Content-Type: application/json\" -d '{\"command\":\"<CMD>\"}'"
