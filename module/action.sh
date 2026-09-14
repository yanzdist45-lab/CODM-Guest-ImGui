#!/system/bin/sh
BASE=/data/local/codm/accounts
mkdir -p "$BASE"
echo "=== CODM Guest Slots ==="
i=1
while [ "$i" -le 8 ]; do
  if [ -d "$BASE/slot$i/shared_prefs" ]; then
    echo "slot$i : saved"
  else
    echo "slot$i : empty"
  fi
  i=$((i+1))
done
