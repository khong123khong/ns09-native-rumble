# NS09 Native Rumble

Module rung gamepad native cho **GuliKit KingKong 2 Pro NS09** trên Android đã root. Xbox 360 Edition tạo profile tay cầm tương thích Xbox và gửi lệnh rung đến NS09 thật qua Bluetooth HID.

[English](README.md) · [Hướng dẫn build](docs/BUILDING.md) · [Changelog](CHANGELOG.md) · [Giấy phép](THIRD-PARTY-NOTICES.md)

## Chức năng

Game/trình duyệt → InputDevice VibratorManager → FF_RUMBLE → daemon uinput → HID Output Report03 → NS09 Bluetooth.

Hai motor strong/weak độc lập, logical magnitude0–100, gain mặc định100%. Có upload/update/start/stop/erase, timeout và watchdog. Dịch vụ tự chạy sau boot, phát hiện/reconnect tay cầm; không cần Termux hoặc nhấn Test. Khi idle chờ sự kiện, không lặp xung rung hoặc gửi report khi motor không đổi.

Thiết bị ảo là `Microsoft X-Box 360 pad`, VID/PID `045E:028E`, có adapter chuẩn hóa nút, stick, trigger analog và D-pad. Tay cầm vật lý giữ Bluetooth `045E:02E0`. Đây là profile input Android, không phải triển khai giao thức Windows XInput. Không sửa kernel, boot image, firmware hay driver Bluetooth.

## Yêu cầu và thiết bị hỗ trợ

Android ARM64 từ Android 12/API 31, root Magisk/KernelSU/SukiSU, BusyBox của root manager, quyền dùng uinput/evdev/hidraw. Đường rung native đã có xác nhận hoạt động trên Android 16/SukiSU; không suy rộng thành hỗ trợ mọi ROM/kernel/firmware.

NS09 cần ở chế độ Bluetooth PC/XInput với ID045E:02E0. Bản public chỉ tìm một gamepad phù hợp thuộc OUI nhà sản xuất98:B6:EA đã thử. Unique ID đầy đủ lấy trên máy và khóa theo phiên, không nhúng địa chỉ Bluetooth cá nhân. Chưa hỗ trợ họ tay cầm khác hoặc nhiều NS09 phù hợp cùng lúc.

Bộ cài dùng keylayout Xbox360 có sẵn nếu đúng; chỉ bổ sung fallback version-specific systemless khi thiếu/sai. Generic.kl và layout của tay cầm vật lý giữ nguyên. Fallback cần chức năng mount của root manager, có thể cần mount backend/metamodule ở các bản KernelSU/SukiSU tương ứng. Không đặt SELinux permissive; nếu quyền hoặc probe framework không đạt, bridge trả input vật lý.

## Cài đặt

1. Tải `NS09-Native-Rumble-Xbox360-v1.0.0-public.zip` trong mục **Releases**.
2. Cài bằng trình quản lý root rồi reboot.
3. Kết nối Bluetooth và mở game/ứng dụng có hỗ trợ rung tay cầm.

ID module giữ nguyên `ns09_rumble_bridge`. Action chỉ bật/tắt dịch vụ, không phát rung thử. Tắt/gỡ module để trả về input vật lý. Khi dừng bình thường, daemon gửi đúng Stop, nhả grab và khôi phục node; guardian/startup recovery bảo vệ takeover. Không thể bảo đảm gửi Stop tới thiết bị đã offline.

## Giới hạn hiện tại

- **Motion/gyro hiện chưa hoạt động qua bridge.** Không có sensor passthrough hoặc gyro-to-stick.
- Một số game/giả lập không gửi yêu cầu rung gamepad. Module không tự tạo yêu cầu bị thiếu hoặc hook rung của mọi ứng dụng.
- Rung điện thoại và dual-rumble của gamepad là các đường khác nhau. Công tắc chuyển hướng rung HyperOS được giữ nguyên; nếu bật có thể chuyển thêm haptic hệ thống vào controller. FF không chứa UID ứng dụng để lọc riêng nguồn này.
- Chưa đo cường độ cơ học toàn dải, latency Stop và mọi tình huống crash/reconnect trên nhiều thiết bị.
- Profile Xbox được kiểm tra bằng host tests và đối chiếu Chromium; không cam kết browser/game nào cũng có mapping standard và rung.

## Trạng thái bản public

Đây là **public preview đầu tiên từ Xbox 360 Edition1.0.0 hiện có**. Chủ dự án đã xác nhận bản đang bật là1.0.0 và trước đó xác nhận đường rung native trên điện thoại. Phần chọn identity sạch của bản public mới được kiểm tra trên host, chưa chạy trực tiếp trên thiết bị. Không gọi tính năng chưa xác minh là Stable.

Bản local đang chạy giữ nguyên. Chuẩn bị public chỉ loại bỏ địa chỉ Bluetooth nhúng sẵn và ràng buộc identity lấy tại máy. Payload/Stop, motor scale, FF lifecycle, watchdog, session, Xbox mapping và guardian giữ nguyên. Xem [ghi chú đóng gói](docs/PUBLIC-PACKAGING.md).

## Build và log

Windows x64 + PowerShell7: chạy `pwsh -File ./build.ps1`. Script tự chuẩn bị công cụ có kiểm tra hash ngoài thư mục repository, chạy test và tạo ZIP/checksum ở dist/. Không đưa SDK/toolchain vào Git. [Build guide](docs/BUILDING.md) ghi rõ phạm vi kiểm tra.

Trạng thái/log local ở `/data/adb/modules/ns09_rumble_bridge/`. Log có thể chứa định danh thiết bị: **hãy che địa chỉ Bluetooth và dữ liệu riêng trước khi đăng issue**. Repository/Release không chứa log máy, capture thô, credential hoặc ZIP riêng có MAC cá nhân.

Code dự án dùng [MIT](LICENSE); keylayout AOSP dùng Apache-2.0. Notices runtime và dependencies build-only ở [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
