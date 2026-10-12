#!/system/bin/sh
MODDIR=${0%/*}
: > "$MODDIR/bridge.off"
PID=$(cat "$MODDIR/bridge.pid" 2>/dev/null)
case "$PID" in ''|*[!0-9]*) ;; *)
  if [ "$(readlink "/proc/$PID/exe")" = "$MODDIR/bin/ns09-bridged" ]; then kill -TERM "$PID" 2>/dev/null; fi
esac
# inotify wakes native cleanup: Stop, restore physical node, release grab.
sleep 3
"$MODDIR/bin/ns09-bridged" --recover 2>/dev/null
