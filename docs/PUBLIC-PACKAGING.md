# Public packaging boundary

Base: existing1.0.0 Xbox360 Edition, module ID `ns09_rumble_bridge`. The original private source, binary and installed phone module are preserved outside the public repository.

The private build embedded a complete personal controller address in daemon discovery, uinput phys and framework inspection. Publishing that binary or its source bundle would disclose the identity. The public copy therefore replaces only this binding:

1. Check Bluetooth bus, physical045E:02E0 and tested manufacturer OUI98:B6:EA.
2. Require one unambiguous matching gamepad under the same HID parent.
3. Retain the complete unique ID in memory and use exact matching for active-session hidraw/evdev revalidation.
4. Supply that runtime identity to the existing framework probe and uinput phys field.

Unbound startup recovery checks only bridge-owned backup inodes, live sysfs/input identity and the same supported family. Existing occupied-path and stale-node safeguards are unchanged. Multiple matching controllers are not selected arbitrarily.

This is a privacy/distribution adaptation, not a new rumble feature. Protected motor/protocol/Stop/FF/writer/watchdog/session/guardian code retains the existing fingerprints. The full Xbox input adapter is unchanged. Public identity behavior has host tests; it is not represented as the exact private binary or as device-validated on every controller.

Only source/module templates, minimal report fixtures, build/test scripts, documentation and license notices belong in Git. Release assets contain newly built, stripped public binaries and documentation. Personal rollback ZIPs are excluded because their binaries still embed the original identity. Source bundles must be created from the approved tracked-file manifest, never from the entire development folder.

Local runtime diagnostics can contain the current device identity. They are not uploaded automatically and must be redacted before issue reports.
