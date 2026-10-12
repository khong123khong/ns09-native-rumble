#!/system/bin/sh
MODDIR=${0%/*}
umask 077
mkdir -p "$MODDIR/logs"
{
  date -u
  getprop ro.build.version.release
  getprop ro.product.cpu.abi
  uname -a
  getenforce
  printf 'vibrate_input_devices='
  settings get system vibrate_input_devices
  ls -lZ /dev/uinput /dev/input/uinput 2>/dev/null
  if [ -r /proc/config.gz ]; then zcat /proc/config.gz | grep '^CONFIG_INPUT_UINPUT='; else printf 'CONFIG_INPUT_UINPUT=UNAVAILABLE\n'; fi
} > "$MODDIR/logs/environment.txt" 2>&1
dumpsys input > "$MODDIR/logs/dumpsys-input.txt" 2>&1
dumpsys vibrator > "$MODDIR/logs/dumpsys-vibrator.txt" 2>&1
