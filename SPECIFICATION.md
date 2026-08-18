Dưới đây là tài liệu đặc tả yêu cầu kỹ thuật, stack công nghệ và lộ trình triển khai chi tiết từng tuần cho dự án **`embedded-linux-gateway`**.

---

### 1. Technology Stack & Tooling

| Tầng hệ thống | Công nghệ / Thư viện lựa chọn | Ghi chú kỹ thuật |
| --- | --- | --- |
| **Yocto Release** | Yocto LTS (Scarthgap 5.0 LTS hoặc Kirkstone 4.0 LTS) | Đảm bảo tính ổn định lâu dài, tài liệu đầy đủ |
| **Target Hardware** | QEMU ARM (`qemuarm` / `qemuaarch64`) & Board thật (Raspberry Pi 4 / BeagleBone Black) | Phát triển trước trên QEMU, nạp board thật ở pha cuối |
| **Init System** | `systemd` | Quản lý services, socket activation, tích hợp watchdog |
| **OTA Framework** | `RAUC` (Robust Auto-Update Controller) | Cơ chế cập nhật A/B phân vùng, kiểm tra chữ ký số (x509) |
| **Bootloader** | `U-Boot` | Patch boot logic đếm boot attempt, switch slot A/B |
| **Industrial Bus** | Linux SocketCAN, Linux Serial RS-485 (`termios`) | Tận dụng driver chuẩn trong Linux kernel |
| **Edge Daemon** | C++17 hoặc Rust (dùng `libmodbus`, `paho-mqtt-cpp`, `sqlite3`) | Tối ưu memory footprint, hiệu năng cao, không crash |
| **Local Storage** | SQLite (WAL mode) | Bộ đệm offline buffer khi mất kết nối mạng |
| **Security & Net** | `nftables`/`iptables`, OpenSSH (key-based), systemd-networkd | Chặn port thừa, cấu hình mạng tĩnh/DHCP |

---

### 2. Yêu cầu chức năng chi tiết (Functional Requirements)

```
                       +-------------------------------+
                       |      Cloud / Broker (MQTT)    |
                       +---------------+---------------+
                                       ^  (TLS / JSON)
                                       |
+--------------------------------------v--------------------------------------+
|  embedded-linux-gateway                                                     |
|                                                                             |
|  [ Industrial Ports ]        [ Core Daemon ]           [ Local Storage ]   |
|   - RS-485 (Modbus RTU) ---> | Data Engine | --------> | SQLite Buffer |   |
|   - CAN Bus (SocketCAN) ---> |  (Parsing,  |           | (Failover)    |   |
|                              |   Filter)   |                               |
|                                                                             |
|  [ System Reliability ]                                                     |
|   - RAUC Daemon (A/B OTA) <--- Watchdog Heartbeat (/dev/watchdog)           |
|   - RootFS Slot A (RO)    <---> RootFS Slot B (RO)                          |
+-----------------------------------------------------------------------------+

```

* **F1 - Thu thập & Chuẩn hóa dữ liệu:** Đọc thanh ghi Modbus RTU từ cảm biến công nghiệp và khung CAN 2.0B từ CAN bus; chuẩn hóa dữ liệu thành định dạng JSON có timestamp (Unix epoch).
* **F2 - Đẩy dữ liệu & Offline Buffer (Store-and-Forward):** Gửi dữ liệu qua MQTT broker. Nếu mất kết nối mạng, chuyển hướng ghi vào database SQLite cục bộ; khi có mạng trở lại, tự động đọc và đẩy bù dữ liệu cũ mà không làm nghẽn luồng dữ liệu thời gian thực.
* **F3 - Cập nhật OTA an toàn (Atomic A/B Update):** Nhận gói bundle `.raucb`, ghi vào slot không hoạt động, verify SHA256/chữ ký số và cấu hình bootloader chuyển slot.
* **F4 - Tự phục hồi khi có sự cố (Watchdog & Rollback):**
* Hardware Watchdog khởi động lại hệ thống nếu daemon bị treo quá 30 giây.
* U-Boot tự động rollback về slot cũ nếu slot mới không boot thành công sau 3 lần thử.


* **F5 - Read-Only RootFS:** Phân vùng hệ thống chỉ đọc (RO), toàn bộ dữ liệu ghi tạm đưa vào `tmpfs` (RAM) hoặc phân vùng `/data` riêng biệt để chống hỏng phân vùng khi ngắt nguồn đột ngột.

---

### 3. Cấu trúc thư mục Monorepo

```text
embedded-linux-gateway/
├── docs/
│   ├── system-architecture.md
│   └── ota-integration-guide.md
├── layers/
│   └── meta-iiot-gateway/
│       ├── conf/
│       │   └── layer.conf
│       ├── recipes-bsp/
│       │   └── u-boot/u-boot_%.bbappend
│       ├── recipes-core/
│       │   ├── images/iiot-gateway-image.bb
│       │   └── rauc/rauc_%.bbappend
│       ├── recipes-daemons/
│       │   └── gateway-engine/gateway-engine_1.0.bb
│       └── recipes-kernel/
│           └── linux/linux-yocto_%.bbappend
├── scripts/
│   ├── env-setup.sh               # Clone poky, meta-openembedded, meta-rauc
│   ├── build-image.sh             # Wrapper lệnh bitbake
│   └── generate-ota-bundle.sh     # Tạo file bundle .raucb có ký cert
└── src/
    └── gateway-engine/            # Mã nguồn C++ / Rust
        ├── CMakeLists.txt (hoặc Cargo.toml)
        ├── src/
        │   ├── main.cpp
        │   ├── modbus_collector.cpp
        │   ├── can_collector.cpp
        │   ├── mqtt_client.cpp
        │   └── storage_manager.cpp
        └── tests/

```

---

### 4. Lộ trình thực hiện chi tiết (7 Tuần)

**Tuần 1: Dựng môi trường & Custom Base Image**

* Cài đặt môi trường build Yocto (Ubuntu/Debian host).
* Soạn script `env-setup.sh` kéo Poky, `meta-openembedded`, `meta-rauc` và layer phần cứng.
* Tạo `meta-iiot-gateway` và công thức image `iiot-gateway-image.bb`.
* Build thử nghiệm thành công base Linux khởi động trên QEMU (`runqemu qemuarm`).

**Tuần 2: Kernel Hardening, Drivers & Read-Only RootFS**

* Bật cấu hình Kernel: `CONFIG_CAN`, `CONFIG_CAN_VCAN`, `CONFIG_SERIAL_8250`, `CONFIG_WATCHDOG`.
* Cấu hình Read-Only RootFS trong Yocto (`IMAGE_FEATURES += "read-only-rootfs"`).
* Cấu hình `fstab` tách riêng phân vùng `/data` (Read-Write) và `/etc` (sử dụng `overlayfs` nếu cần chỉnh sửa cấu hình động).

**Tuần 3: Tích hợp U-Boot & RAUC A/B Update**

* Tạo chứng chỉ tự ký (X.509 keys/cert) cho RAUC.
* Tích hợp `meta-rauc` vào build image.
* Viết script U-Boot environment để quản lý biến `BOOT_SLOT` (A hoặc B) và biến đếm số lần boot lỗi (`bootcount`).
* Kiểm thử cập nhật phân vùng bằng lệnh `rauc install update-bundle.raucb` trong môi trường QEMU.

**Tuần 4: Phát triển Gateway Engine Daemon (Tầng thu thập dữ liệu)**

* Khởi tạo dự án C++/Rust trong thư mục `src/gateway-engine/`.
* Lập trình module đọc Modbus RTU (qua virtual serial port `socat` / `/dev/ttyUSB0`) và CAN Bus (qua SocketCAN `vcan0`).
* Đóng gói recipe `gateway-engine_1.0.bb` để Yocto tự động cross-compile và cài binary vào `/usr/bin/`.
* Viết file `gateway-engine.service` cho `systemd` tự khởi chạy khi boot.

**Tuần 5: Phát triển Module Lưu trữ Offline & MQTT Dispatcher**

* Tích hợp `paho-mqtt` đẩy dữ liệu lên MQTT Broker (Mosquitto/HiveMQ).
* Viết logic phát hiện mất mạng: Nếu MQTT publish thất bại, lưu payload vào SQLite (`/data/gateway.db`).
* Viết worker chạy nền kiểm tra kết nối để đẩy dữ liệu tồn đọng theo cơ chế FIFO khi mạng online trở lại.

**Tuần 6: Tích hợp Watchdog & Kiểm thử độ tin cậy (Reliability Testing)**

* Kết nối tiến trình `gateway-engine` với `systemd watchdog` (gọi hàm `sd_notify("WATCHDOG=1")` định kỳ).
* Thử nghiệm kịch bản lỗi:
* Force kill hoặc làm deadlock tiến trình để kiểm tra Watchdog reboot hệ điều hành.
* Cắt nguồn đột ngột trong lúc ghi dữ liệu để kiểm tra tính toàn vẹn của phân vùng RO và SQLite database.
* Cài đặt một bundle OTA bị lỗi (corrupted image) để kiểm tra tính năng tự rollback về Slot trước.



**Tuần 7: Porting lên Board thực tế, Benchmark & Viết Báo cáo**

* Chuyển build target từ QEMU sang Board thực tế (nếu có phần cứng).
* Đo đạc và ghi nhận số liệu:
* Thời gian khởi động (dùng `systemd-analyze` và `systemd-analyze blame`).
* Dung lượng Image tổng thể (RootFS size) và mức tiêu thụ RAM tĩnh.
* Tốc độ xử lý gói tin (throughput/latency) của gateway daemon.


* Hoàn thiện tài liệu kiến trúc, sơ đồ phân vùng và tài liệu hướng dẫn vận hành vào thư mục `docs/`.