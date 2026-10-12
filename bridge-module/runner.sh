#!/system/bin/sh
MODDIR=${0%/*}
umask 077
mkdir -p "$MODDIR/logs" || exit 1
LOG="$MODDIR/logs/bridge.log"
exec 9>"$MODDIR/supervisor.lock"
flock -n 9 || exit 0
printf '%s\n' "$$" > "$MODDIR/supervisor.pid"
CHILD=
finish() {
  trap - EXIT INT TERM HUP
  if [ -n "$CHILD" ]; then kill -TERM "$CHILD" 2>/dev/null; wait "$CHILD" 2>/dev/null; fi
  rm -f "$MODDIR/supervisor.pid"
}
trap finish EXIT
trap 'exit 0' INT TERM HUP
while [ "$(getprop sys.boot_completed)" != 1 ]; do
  [ -e "$MODDIR/disable" ] || [ -e "$MODDIR/remove" ] || [ -e "$MODDIR/bridge.off" ] && exit 0
  sleep 2
done
# Read-only snapshot; no test pulses or settings changes.
timeout 8 sh "$MODDIR/diagnose.sh" 2>/dev/null
FAILURES=0
while [ ! -e "$MODDIR/disable" ] && [ ! -e "$MODDIR/remove" ] && [ ! -e "$MODDIR/bridge.off" ]; do
  if [ -f "$LOG" ] && [ "$(wc -c < "$LOG")" -gt 262144 ]; then mv -f "$LOG" "$LOG.previous"; fi
  "$MODDIR/bin/ns09-bridged" --recover >>"$LOG" 2>&1
  "$MODDIR/bin/ns09-bridged" --module "$MODDIR" >>"$LOG" 2>&1 &
  CHILD=$!
  wait "$CHILD"
  RC=$?
  CHILD=
  [ "$RC" = 0 ] && break
  FAILURES=$((FAILURES + 1))
  printf 'daemon exit=%s attempt=%s\n' "$RC" "$FAILURES" >>"$LOG"
  if [ "$FAILURES" -ge 3 ]; then
    printf '%s\n' 'BLOCKED: daemon failed three times; use Action to restart after fixing cause' > "$MODDIR/bridge-status.txt"
    break
  fi
  sleep 5
done
