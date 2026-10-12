#!/system/bin/sh
MODDIR=${0%/*}
BB=
for CANDIDATE in /data/adb/ksu/bin/busybox /data/adb/magisk/busybox; do
  if [ -x "$CANDIDATE" ]; then BB="$CANDIDATE"; break; fi
done
if [ -z "$BB" ]; then echo 'BusyBox unavailable'; /system/bin/sleep 8; exit 0; fi
export ASH_STANDALONE=1
if [ -e "$MODDIR/bridge.off" ]; then
  "$BB" rm -f "$MODDIR/bridge.off"
  "$BB" sh "$MODDIR/runner.sh" >/dev/null 2>&1 &
  echo 'NS09 Bridge: BAT. Se tu nhan tay cam va rung tu game.'
else
  : > "$MODDIR/bridge.off"
  PID=$("$BB" cat "$MODDIR/bridge.pid" 2>/dev/null)
  case "$PID" in ''|*[!0-9]*) ;; *)
    if [ "$("$BB" readlink "/proc/$PID/exe")" = "$MODDIR/bin/ns09-bridged" ]; then "$BB" kill -TERM "$PID" 2>/dev/null; fi
  esac
  echo 'NS09 Bridge: TAT. Daemon se Stop va tra nut/joystick ve tay cam goc.'
fi
"$BB" sleep 2
[ -f "$MODDIR/bridge-status.txt" ] && "$BB" cat "$MODDIR/bridge-status.txt"
echo 'Action chi bat/tat dich vu; khong phat rung test.'
"$BB" sleep 8
exit 0
