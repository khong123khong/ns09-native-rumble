#!/system/bin/sh
[ "$BOOTMODE" = true ] || abort "Install with Magisk, KernelSU or SukiSU manager."
[ "$ARCH" = arm64 ] || abort "This release requires arm64 Android."
[ "$API" -ge 31 ] || abort "Native dual-rumble requires Android 12 or newer."
ui_print "NS09 Native Rumble - Xbox360 Edition 1.0.0"
ui_print "Automatic virtual gamepad + two-channel FF_RUMBLE to Bluetooth HID."
ui_print "Virtual045E:028E; physical Bluetooth045E:02E0 remains unchanged."
ui_print "HyperOS vibration redirection setting is preserved."
ui_print "No manual motor tests. Service starts after boot."
ui_print "Action enables/disables the bridge; it never plays a test pulse."
. "$MODPATH/keylayout.sh"
ns09_select_keylayout || abort "Xbox360 keylayout setup failed; original controller is unchanged."
ui_print "Xbox360 buttons/axes keylayout checked."
set_perm_recursive "$MODPATH" 0 0 0755 0644
for SCRIPT in service.sh action.sh uninstall.sh runner.sh diagnose.sh; do
  set_perm "$MODPATH/$SCRIPT" 0 0 0755
done
set_perm "$MODPATH/bin/ns09-bridged" 0 0 0755
set_perm "$MODPATH/bin/framework-probe.dex" 0 0 0444
