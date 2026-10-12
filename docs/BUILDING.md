# Building

Supported build host: Windows x64, PowerShell7 and internet access to official tool downloads. No Android SDK/NDK or system-wide tool installation is required. Output architecture is ARM64 Linux userspace with static musl, suitable for the tested rooted Android environment; host compilation does not prove ROM runtime compatibility.

```powershell
pwsh -File ./build.ps1
```

The script downloads Zig 0.15.2, a checksum-verified Temurin/OpenJDK 17 build, R8/D8 8.3.37 and the pinned host BusyBox. Tools reside in a sibling `.ns09-tools` cache outside this repository. For an existing cache, set `NS09_TOOL_ROOT` to that directory before running the script. Tools are not included in Git or module ZIPs. JDK17 patch downloads can change upstream; compiled artifacts are not claimed byte-for-byte reproducible across every JDK patch.

The build runs tests for report encoding, 16-bit motor scaling, independent motors, effect lifecycles, upload/erase errors, Stop/timeouts, input mapping/resync, inode recovery decisions, HID partial/error writes, target identity, startup behavior and conditional keylayout selection. Regression fingerprints protect the unchanged haptic backend. It compiles a stripped static ARM64 ELF with16KiB load alignment and a read-only framework probe DEX, checks shell syntax and ZIP permissions, then writes:

- `dist/NS09-Native-Rumble-Xbox360-v1.0.0-public.zip`
- `dist/SHA256SUMS.txt`
- `dist/build-manifest.json`

Generated binaries are under ignored `bridge-module/bin/`; tests/temporary objects are under ignored `build/`. Do not commit either directory. Release assets are uploaded separately after review.

These are host tests with injected I/O and static packaging validation. They do not run a phone kernel's uinput ioctls, measure physical intensity/Stop latency, or validate every crash/reconnect/SELinux case. The public privacy adaptation has not been tested on a connected Android device in the preparation session.
