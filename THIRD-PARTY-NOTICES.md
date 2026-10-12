# Licenses and provenance

The original C daemon, protocol/effect/input helpers, reflection-only Java probe, tests and module/build scripts are covered by the root MIT license. They are independently written userspace code; GPL driver/project source used as architectural references is not copied into this distribution.

| Component | Use | License / notice |
|---|---|---|
| AOSP Xbox360 keylayout | `bridge-module/assets/Xbox360.kl`, distributed | Apache-2.0; original header retained; [full text](licenses/Apache-2.0.txt) |
| musl libc | Statically linked ARM64 runtime | MIT plus file-specific permissive notices; complete [COPYRIGHT](licenses/musl-COPYRIGHT.txt) |
| Arm runtime assembly | Part of the supplied musl AArch64 runtime | MIT, [notice](licenses/Arm-runtime-MIT.txt) |
| Zig runtime/compiler | Runtime/build tool | MIT, [license](licenses/Zig-MIT.txt); LLVM infrastructure has [Apache-2.0 with LLVM exceptions](licenses/LLVM.txt) |
| Linux UAPI | System interfaces compiled from toolchain headers; headers not vendored | GPL-2.0 WITH Linux-syscall-note permits use by non-GPL userspace programs |
| R8/D8 8.3.37 | Build-only bytecode conversion; JAR not distributed | BSD-3-Clause, [notice](licenses/R8-BSD-3-Clause.txt) |
| Eclipse Temurin/OpenJDK 17 | Build-only Java tool; JDK not distributed | GPL-2.0 with Classpath Exception and upstream component notices |
| BusyBox | Host shell tests and existing root-manager runtime; executable not distributed | GPL-2.0; obtain from its upstream/root-manager distribution |

No Android SDK, NDK, JDK, compiler archive, BusyBox executable, Linux driver source or GPL reference-project source is committed or attached to a release. Android framework APIs are used through reflection on the device; the DEX contains the project's probe, not framework implementation code.

License review covers the files in this repository and the supplied build/runtime components. MIT applies to original project code; it does not replace third-party licenses. Future vendored dependencies require a fresh review.

Primary sources:

- [AOSP keylayout and header](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/android16-release/data/keyboards/Vendor_045e_Product_028e.kl)
- [musl COPYRIGHT](https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT)
- [Zig 0.15.2 license](https://github.com/ziglang/zig/blob/0.15.2/LICENSE)
- [Linux licensing and syscall exception](https://www.kernel.org/doc/html/latest/process/license-rules.html)
- [R8 8.3.37 license](https://r8.googlesource.com/r8/+/refs/tags/8.3.37/LICENSE)
- [OpenJDK legal terms](https://openjdk.org/legal/)
- [BusyBox license](https://busybox.net/license.html)

Architecture references include AOSP inputflinger, Chromium GamepadDevice/GamepadMappings, xpadneo, GammaPad and Joy-con Bridge for Android. Referencing their interfaces and behavior does not relicense their source. This project is not affiliated with GuliKit, Microsoft, Google or those projects.
