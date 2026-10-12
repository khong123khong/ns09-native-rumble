# NS09 Native Rumble

Native gamepad rumble for the **GuliKit KingKong 2 Pro NS09** on rooted Android. The Xbox 360 Edition presents a compatible virtual gamepad while sending vibration commands to the real controller over Bluetooth HID.

[Tiếng Việt](README-VI.md) · [Build guide](docs/BUILDING.md) · [Changelog](CHANGELOG.md) · [Licenses](THIRD-PARTY-NOTICES.md)

## What it does

Applications using an Android input-device vibrator can reach the controller through the native force-feedback path:

```text
Game / browser → InputDevice VibratorManager → FF_RUMBLE
             → uinput daemon → HID Output Report 0x03 → NS09 Bluetooth
```

- Independent strong/weak motors, logical magnitude 0–100 and normal gain.
- Native effect upload, update, playback, cancellation and erase.
- Automatic boot, controller discovery and reconnection; no Termux or manual test button.
- Xbox-compatible buttons, sticks, analog triggers and D-pad; virtual identity `Microsoft X-Box 360 pad`, `045E:028E`.
- Watchdog, effect timeout and input recovery. Idle operation blocks on events; unchanged motor states do not generate repeated HID reports.

The physical controller remains Bluetooth `045E:02E0`. This is an Android input profile, not the Windows XInput transport. The module does not patch a kernel, firmware, boot image or Bluetooth driver.

## Requirements and compatibility

ARM64 Android 12/API 31 or newer, root through Magisk, KernelSU or SukiSU, a working root-manager BusyBox, and accessible `uinput`, `evdev` and `hidraw` interfaces are required. Native rumble has been reported working on an Android 16/SukiSU device. Other ROMs, kernels and controller firmware are not automatically supported.

Use the NS09 Bluetooth PC/XInput mode exposing `045E:02E0`. Public discovery is restricted to the tested manufacturer OUI `98:B6:EA` and one unambiguous matching gamepad. A complete device identity is bound locally for the session; no personal Bluetooth address is compiled into this release. Other controller families and simultaneous matching controllers are not supported by this initial public distribution.

The installer reuses a compatible ROM Xbox360 keylayout. It supplies a version-specific systemless fallback only when required, leaving `Generic.kl` and the physical controller's keylayout unchanged. That fallback requires the root manager's systemless mount support, including an appropriate mount backend on KernelSU/SukiSU versions that use metamodules. Access or framework validation failures release the virtual device and restore physical input; SELinux is never set permissive.

## Installation

1. Download `NS09-Native-Rumble-Xbox360-v1.0.0-public.zip` from this repository's **Releases** page.
2. Install it using the Magisk, KernelSU or SukiSU manager and reboot.
3. Connect the controller by Bluetooth and use an application with controller vibration support.

Module ID: `ns09_rumble_bridge`. Action toggles the service; it does not play a test effect. Disable or uninstall the module to return to the physical input device. Normal shutdown sends the verified Stop report and releases the grab. A guardian and startup recovery protect input takeover; a Stop cannot be delivered to a Bluetooth device that is already offline.

## Current limitations

- **Motion/gyro does not currently work through this bridge.** No sensor passthrough or gyro-to-stick conversion is provided.
- Some games and emulators never send controller force-feedback requests. The bridge cannot invent missing requests, and it does not hook every application's haptics.
- Phone vibration and controller dual-rumble are different API paths. HyperOS system-vibration redirection is left unchanged; enabled redirection can also deliver system haptics to the virtual pad. evdev FF carries no application identity to filter those requests.
- Full-range mechanical intensity, Stop latency and every crash/reconnection case have not been measured across devices.
- The Xbox-compatible profile is tested in host code. Its `standard` browser mapping follows Chromium's Xbox mapping; it is not a promise about every installed browser or game.

## Release status

This is the **initial public preview of the existing 1.0.0 Xbox 360 Edition**. The project owner's installed version was confirmed as1.0.0. The native controller-vibration path was previously confirmed on the owner's device; public address discovery is a privacy adaptation and has only been checked with host tests. This repository does not label unverified capabilities Stable.

The private working module is preserved. Public preparation changes identity handling only to remove its embedded personal address. Rumble payloads, motor scaling, FF lifecycle, watchdog, session processing, Xbox input mapping and guardian are unchanged. See [Public packaging notes](docs/PUBLIC-PACKAGING.md).

## Building and troubleshooting

Windows x64 with PowerShell7 can build the ARM64 module with `pwsh -File ./build.ps1`. The script downloads checked tools to a cache outside the repository, runs production-helper tests and creates the ZIP plus checksums in `dist/`. No SDK or toolchain is committed. Detailed prerequisites and validation scope are in the [build guide](docs/BUILDING.md).

Local state and diagnostics are stored under `/data/adb/modules/ns09_rumble_bridge/`. They can contain device identifiers. **Redact Bluetooth addresses and device-specific data before sharing an issue.** No machine logs, raw captures, credentials or personal build artifacts are included in this repository or release.

Original project code is [MIT licensed](LICENSE). The AOSP keylayout is Apache-2.0; statically linked runtime notices and build-only dependencies are documented in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
