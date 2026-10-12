# v1.0.0 — NS09 Native Rumble, Xbox 360 Edition

**Initial public preview / pre-release.**

Native Android FF_RUMBLE is bridged to the GuliKit KingKong 2 Pro NS09 over Bluetooth HID03. This distribution preserves the existing motor backend, Xbox-compatible input mapping, automatic boot/reconnect, watchdog and input recovery. It removes the development build's personal address and uses local identity binding for one matching controller in the tested family.

Requirements: rooted ARM64 Android 12/API 31+, Magisk/KernelSU/SukiSU with BusyBox and access to uinput/evdev/hidraw. Native vibration was reported working on Android 16/SukiSU. The project owner confirmed the private1.0.0 Xbox360 Edition is currently installed/enabled. Public identity handling and compatibility beyond that setup remain unverified on-device.

**Motion/gyro is not working through the bridge.** Some games/emulators do not issue controller rumble and therefore will not vibrate. Full physical intensity, Stop latency, all reconnection/crash scenarios and every browser's standard mapping are not certified. No unverified feature is labeled Stable.

Install the module ZIP through the root manager and reboot. No Termux or manual HID test is needed. Action toggles the service; disable/uninstall restores the physical input path. Confirm the download against SHA256SUMS.txt.

Assets:

- `NS09-Native-Rumble-Xbox360-v1.0.0-public.zip`
- `SHA256SUMS.txt`
- `build-manifest.json`

Source, build guide, English/Vietnamese README and license notices are in the repository. No personal logs, raw captures, keys/tokens or toolchains are published. Original project code is MIT; AOSP keylayout and bundled runtime notices retain their licenses.
