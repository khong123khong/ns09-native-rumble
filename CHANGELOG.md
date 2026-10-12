# Changelog

## 1.0.0 — Initial public preview

- Publish the existing Xbox 360 Edition source and installable ARM64 module.
- Preserve HID03 framing/Stop, independent motor encoding, FF lifecycle, gain, timeouts, watchdog, Xbox input mapping and input recovery.
- Remove the original embedded personal Bluetooth identity. Discover one matching controller in the tested manufacturer family and bind its full identifier locally for the session.
- Include English/Vietnamese documentation, build instructions, original MIT license and third-party notices.
- Exclude personal logs, raw captures, private artifacts, credentials, SDKs, toolchains and temporary build outputs from Git.
- Public identity handling is host-tested and pending broader device validation. Motion/gyro is not implemented; application vibration support remains application-dependent.

## Development history

- 1.0.0 Xbox 360 Edition: existing virtual profile and normalized input adapter. Project owner confirmed this version is installed/enabled.
- 0.3.0-RC1: native uinput FF_RUMBLE route; owner reported controller recognition, working buttons and automatic Bluetooth vibration on Android 16/SukiSU.
- Earlier prototypes established HID replay. Personal prototype ZIPs/captures are not public release assets.
