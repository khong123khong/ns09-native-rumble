#!/system/bin/sh
MODDIR=${0%/*}
for BB in /data/adb/ksu/bin/busybox /data/adb/magisk/busybox; do
  [ -x "$BB" ] || continue
  export ASH_STANDALONE=1
  "$BB" sh "$MODDIR/runner.sh" >/dev/null 2>&1 &
  exit 0
done
printf '%s\n' 'BLOCKED: root manager BusyBox not found' > "$MODDIR/bridge-status.txt"
