# ĐẶC TẢ KỸ THUẬT & LỘ TRÌNH DỰ ÁN: EMBEDDED LINUX IIOT GATEWAY
*(Đề tài Đồ án Tốt nghiệp - Hệ thống nhúng & IoT Công nghiệp)*

---

## 1. Mục tiêu Đề tài & Tổng quan Hệ thống
Xây dựng một thiết bị **Industrial IoT Edge Gateway** chạy hệ điều hành Linux nhúng tùy biến (Custom Embedded Linux phân phối qua Yocto Project), có khả năng thu thập dữ liệu công nghiệp (Modbus RTU qua RS-485, CAN Bus), truyền thông Cloud an toàn (MQTT/TLS), hoạt động tin cậy cao (Read-Only RootFS, Hardware/Systemd Watchdog, Store-and-Forward khi mất mạng) và hỗ trợ cập nhật phần mềm từ xa an toàn qua cơ chế A/B OTA (RAUC) với khả năng tự động rollback.

### Chiến lược Triển khai 2 Pha:
* **Pha 1 (Mô phỏng & Phát triển lõi trên QEMU):** Sử dụng `qemuarm` / `qemuaarch64`, tạo cổng nối tiếp ảo (`socat`/`pty`) cho Modbus RTU và Virtual CAN interface (`vcan0`). Giúp lập trình daemon, kiểm thử luồng OTA A/B, Watchdog và cơ chế Store-and-Forward nhanh chóng mà không phụ thuộc phần cứng.
* **Pha 2 (Triển khai & Đánh giá trên Phần cứng thật):** Porting Board Support Package (BSP) lên board thực tế (Raspberry Pi 4 / BeagleBone Black / STM32MP1), kết nối module cảm biến Modbus RS-485 thật và node CAN thật, tiến hành đo đạc benchmark thực nghiệm cho báo cáo đồ án.

---

## 2. Technology Stack & Kiến trúc Hệ thống

| Tầng hệ thống | Công nghệ / Thư viện lựa chọn | Ghi chú kỹ thuật & Ý nghĩa trong ĐATN |
| :--- | :--- | :--- |
| **Yocto Release** | Yocto LTS (Scarthgap 5.0 LTS / Kirkstone 4.0 LTS) | Chuẩn công nghiệp dài hạn, tính tương thích cao |
| **Target Platform** | **Pha 1:** QEMU ARM (`qemuarm`/`qemuaarch64`)<br>**Pha 2:** Raspberry Pi 4 (CM4/Pi4B) / BeagleBone Black | Phát triển & unit test trên QEMU; nghiệm thu trên bo mạch thật |
| **Init System** | `systemd` | Quản lý services, cgroups, socket activation, watchdog integration |
| **OTA Framework** | **RAUC** (Robust Auto-Update Controller) | Cơ chế cập nhật A/B phân vùng nguyên tử, ký số X.509/SHA256 |
| **Bootloader** | **U-Boot** | Cấu hình bootcount, bootlimit, tự động rollback slot khi lỗi boot |
| **Industrial Bus** | Linux SocketCAN (`can_raw`), Linux Serial RS-485 (`termios`) | Tận dụng subsystem chuẩn của Linux kernel |
| **Edge Daemon** | **C++17** (`libmodbus`, `paho-mqtt-cpp`, `sqlite3`, `nlohmann/json`) | Hiệu năng cao, memory footprint thấp, quản lý tài nguyên RAII an toàn |
| **Local Storage** | **SQLite 3** (WAL Mode - Write-Ahead Logging) | Bộ đệm Store-and-Forward chống mất dữ liệu khi ngắt kết nối/mất nguồn |
| **Security & Net** | `systemd-networkd`, `nftables`/`iptables`, OpenSSH (Key-based) | Read-Only RootFS, cô lập mạng, chỉ mở port cần thiết |
| **Visualization** | **ThingsBoard CE** / **Grafana + Mosquitto** | Dashboard hiển thị realtime telemetry, điều khiển downlink và trạng thái OTA |

---

## 3. Sơ đồ Kiến trúc Toàn diện

```
                             +----------------------------------------------+
                             |       Cloud / Platform (ThingsBoard / MQTT)  |
                             |  - Live Dashboard & Data Telemetry           |
                             |  - Downlink Control (Relay/Setpoints)        |
                             |  - Remote OTA Bundle Trigger                 |
                             +-----------------------+----------------------+
                                                     ^
                                            (TLS / JSON / MQTT)
                                                     v
+----------------------------------------------------+-----------------------------------------------------+
| EMBEDDED LINUX GATEWAY (Target / QEMU)                                                                   |
|                                                                                                          |
|  [ Industrial Ingest ]               [ Core Daemon: gateway-engine ]              [ Local Storage ]      |
|   - RS-485 (Modbus RTU) --------->   +-----------------------------+   (No Net)   +-------------------+  |
|   - CAN Bus (SocketCAN) --------->   | - Modbus / CAN Collectors   | -----------> | SQLite3 Buffer    |  |
|                                      | - JSON Parser & Normalizer  |              | (/data/buffer.db) |  |
|  [ Downlink Control ]                | - Config Manager (JSON/YAML)| <----------- +-------------------+  |
|   - GPIO / Relay Controller <-----   | - MQTT Dispatcher & Worker  |    (Replay)                         |
|                                      +-----------------------------+                                     |
|                                                     |                                                    |
|                                      (sd_notify / Watchdog Heartbeat)                                    |
|                                                     v                                                    |
|  [ System Reliability & Security ]   +-----------------------------+              [ Dual-Slot RootFS ]   |
|   - Systemd Watchdog (/dev/watchdog) | RAUC OTA Update Service     | <----------> | RootFS Slot A (RO)|  |
|   - U-Boot Bootcount & Rollback      | (X.509 Signature & SHA256)  |              | RootFS Slot B (RO)|  |
|   - Data Partition (RW: /data)       +-----------------------------+              +-------------------+  |
+----------------------------------------------------------------------------------------------------------+
```

---

## 4. Yêu cầu Chức năng Chi tiết (Functional Requirements)

* **F1 - Thu thập & Chuẩn hóa dữ liệu công nghiệp:**
  * Đọc định kỳ các thanh ghi (Holding/Input Registers) từ thiết bị Modbus RTU qua cổng Serial RS-485.
  * Lắng nghe và phân tích các khung tin CAN 2.0A/2.0B từ SocketCAN interface (`vcan0` trên QEMU, `can0` trên phần cứng).
  * Đóng gói dữ liệu chuẩn hóa sang JSON có timestamp (Unix epoch thời gian thực hoặc lấy từ NTP/RTC).

* **F2 - Đẩy dữ liệu & Bộ đệm lưu trữ Ngoại tuyến (Store-and-Forward):**
  * Đẩy dữ liệu JSON lên MQTT Broker qua giao thức bảo mật MQTT/TLS.
  * Tự động phát hiện mất kết nối mạng: Khi mất mạng, chuyển hướng ghi dữ liệu vào SQLite cục bộ (lưu tại `/data/buffer.db` ở chế độ WAL).
  * Cơ chế bù dữ liệu (Replay Worker): Khi kết nối mạng phục hồi, tiến trình nền tự động đọc dữ liệu tồn đọng trong SQLite theo thứ tự FIFO và đẩy lên Cloud song song với luồng dữ liệu thời gian thực mà không làm nghẽn hệ thống.

* **F3 - Quản lý Cấu hình Động (Configuration Management):**
  * Tải cấu hình hoạt động từ file `/data/config/gateway_config.json` (baudrate, serial port, can interface, polling interval, MQTT endpoint, topics, device mappings).
  * Hỗ trợ nạp cấu hình mặc định (fallback) nếu file cấu hình bị lỗi.

* **F4 - Nhận lệnh Điều khiển 2 chiều (Downlink / Command & Control):**
  * Đăng ký nhận lệnh từ Cloud qua MQTT Subscribe topic `gateway/{id}/command`.
  * Thực thi các lệnh: bật/tắt thiết bị chấp hành (Modbus coil / Relay GPIO), thay đổi tần suất lấy mẫu, hoặc yêu cầu khởi động lại hệ thống an toàn.

* **F5 - Cập nhật A/B OTA An toàn & Xác thực Chữ ký số (RAUC):**
  * Nhận gói bundle cập nhật `.raucb`, tiến hành kiểm tra chữ ký số (X.509 PKI) và tính toàn vẹn (SHA256).
  * Ghi bản cập nhật vào phân vùng không hoạt động (Inactive Slot).
  * Cấu hình U-Boot chuyển cờ khởi động sang Slot mới cho lần boot kế tiếp.

* **F6 - Tự phục hồi & Chống lỗi (Self-Healing & Rollback):**
  * **Watchdog:** Tiến trình `gateway-engine` gửi heartbeat `sd_notify("WATCHDOG=1")` định kỳ cho systemd. Nếu tiến trình bị deadlock/crash quá 30 giây, Hardware/Systemd Watchdog sẽ tự động kích hoạt khởi động lại gateway.
  * **U-Boot Bootcount Rollback:** Khi nâng cấp OTA, U-Boot đếm số lần khởi động (`bootcount`). Nếu sau 3 lần thử mà hệ điều hành ở Slot mới không xác nhận `rauc status mark-good` thành công, U-Boot sẽ tự động rollback về Slot cũ.

* **F7 - Hệ thống tệp Read-Only (Read-Only RootFS):**
  * Phân vùng hệ thống gốc hoàn toàn chỉ đọc (`IMAGE_FEATURES += "read-only-rootfs"`).
  * Phân tách riêng biệt:
    * `/` : Read-Only (chứa base OS, binaries, thư viện).
    * `/data` : Read-Write (chứa SQLite DB, file cấu hình, persistent logs).
    * `/tmp`, `/var/volatile` : `tmpfs` (RAM).

* **F8 - Giao diện Giám sát & Bảng điều khiển (Cloud Dashboard):**
  * Tích hợp nền tảng Dashboard (ThingsBoard / Grafana) để hiển thị: biểu đồ dữ liệu cảm biến thời gian thực, trạng thái kết nối Online/Offline, dung lượng hàng đợi SQLite offline, và trạng thái slot A/B OTA.

---

## 5. Cấu trúc Thư mục Dự án (Monorepo)

```plaintext
embedded-linux-gateway/
├── docs/
│   ├── system-architecture.md         # Sơ đồ kiến trúc chi tiết, sơ đồ phân vùng eMMC/SD
│   ├── ota-integration-guide.md       # Hướng dẫn tạo bundle, ký chứng chỉ và cập nhật RAUC
│   ├── api-telemetry-spec.md          # Đặc tả định dạng payload JSON, MQTT Topics
│   └── graduation-thesis-report/      # Báo cáo đồ án tốt nghiệp và bảng kết quả thực nghiệm
├── layers/
│   └── meta-iiot-gateway/             # Yocto Custom Layer
│       ├── conf/
│       │   └── layer.conf
│       ├── recipes-bsp/
│       │   └── u-boot/u-boot_%.bbappend          # Script bootcount và A/B logic
│       ├── recipes-core/
│       │   ├── images/iiot-gateway-image.bb      # Định nghĩa image, package, read-only rootfs
│       │   ├── rauc/rauc_%.bbappend              # Cấu hình system.conf cho RAUC A/B
│       │   └── systemd/systemd_%.bbappend        # Tối ưu boot time, cấu hình watchdog
│       ├── recipes-daemons/
│       │   └── gateway-engine/gateway-engine_1.0.bb # Cross-compile C++ daemon & systemd unit
│       └── recipes-kernel/
│           └── linux/linux-yocto_%.bbappend      # Bật CAN, Serial RS485, Watchdog, Crypto
├── scripts/
│   ├── env-setup.sh                   # Tự động clone Poky, meta-oe, meta-rauc, meta-raspberrypi
│   ├── build-image.sh                 # Wrapper lệnh bitbake build image
│   ├── run-qemu.sh                    # Script chạy QEMU kèm cấu hình mạng & socket ảo
│   ├── mock-sensors.sh                # Tạo vcan0 và cổng socat ảo phát dữ liệu Modbus giả lập
│   └── generate-ota-bundle.sh         # Đóng gói và ký chứng chỉ X.509 cho bundle .raucb
├── dashboard/
│   ├── docker-compose.yml             # Môi trường chạy Mosquitto MQTT Broker + ThingsBoard/Grafana
│   └── thingsboard-dashboard.json     # Template giao diện demo cho hội đồng chấm thi
└── src/
    └── gateway-engine/                # Mã nguồn C++17 Core Daemon
        ├── CMakeLists.txt
        ├── config/
        │   └── default_config.json
        ├── include/
        │   ├── modbus_collector.hpp
        │   ├── can_collector.hpp
        │   ├── mqtt_client.hpp
        │   ├── storage_manager.hpp
        │   ├── downlink_handler.hpp
        │   └── config_manager.hpp
        ├── src/
        │   ├── main.cpp
        │   ├── modbus_collector.cpp
        │   ├── can_collector.cpp
        │   ├── mqtt_client.cpp
        │   ├── storage_manager.cpp
        │   ├── downlink_handler.cpp
        │   └── config_manager.cpp
        └── tests/
            └── test_storage_failover.cpp
```

---

## 6. Lộ trình Thực hiện Chi tiết (7 Tuần)

### Tuần 1: Môi trường Build Yocto & Base Linux trên QEMU
* Chuẩn bị môi trường build (Ubuntu Host, dung lượng > 100GB).
* Viết script `env-setup.sh` kéo Poky (Scarthgap/Kirkstone), `meta-openembedded`, `meta-rauc`.
* Tạo layer `meta-iiot-gateway` và recipe image `iiot-gateway-image.bb`.
* Build thành công và khởi động Base OS trên QEMU (`runqemu qemuarm` / `qemuaarch64`).

### Tuần 2: Kernel Hardening, Drivers ảo & Read-Only RootFS
* Bật cấu hình Kernel: `CONFIG_CAN`, `CONFIG_CAN_VCAN`, `CONFIG_SERIAL_8250`, `CONFIG_WATCHDOG`.
* Kích hoạt `IMAGE_FEATURES += "read-only-rootfs"` trong Yocto.
* Cấu hình phân vùng `/data` (Read-Write) tự động mount qua `/etc/fstab` và thiết lập `tmpfs` cho các thư mục biến đổi.
* Viết script `mock-sensors.sh` tạo `vcan0` và cổng serial ảo `socat` để sẵn sàng phát dữ liệu giả lập trên QEMU.

### Tuần 3: Tích hợp U-Boot & Cơ chế A/B OTA với RAUC
* Tạo PKI Keys & X.509 Certificate cho RAUC.
* Tích hợp `meta-rauc`, cấu hình `/etc/rauc/system.conf` định nghĩa Slot A, Slot B và phân vùng `/data`.
* Cấu hình U-Boot environment script hỗ trợ các biến `BOOT_SLOT`, `bootcount`, `bootlimit` để quản lý luân phiên phân vùng.
* Kiểm thử cập nhật OTA bằng lệnh `rauc install update-bundle.raucb` và kiểm tra logic rollback khi slot mới bị lỗi trong QEMU.

### Tuần 4: Phát triển C++17 Core Engine (Tầng Thu thập Dữ liệu)
* Khởi tạo dự án CMake C++17 trong `src/gateway-engine/`.
* Xây dựng `ModbusCollector` (sử dụng `libmodbus`) đọc thanh ghi qua serial port.
* Xây dựng `CanCollector` (sử dụng Linux SocketCAN `AF_CAN`) nhận khung tin CAN.
* Đóng gói recipe `gateway-engine_1.0.bb` trong Yocto để cross-compile tự động và tạo file `gateway-engine.service` quản lý bởi `systemd`.

### Tuần 5: Lưu trữ SQLite Ngoại tuyến & MQTT Dispatcher + Dashboard Demo
* Tích hợp `paho-mqtt-cpp` đẩy dữ liệu JSON lên MQTT Broker.
* Xây dựng `StorageManager` (SQLite WAL) triển khai cơ chế **Store-and-Forward**: ghi offline khi mất kết nối, tự động phát luồng đẩy bù dữ liệu khi mạng phục hồi.
* Xây dựng `DownlinkHandler` tiếp nhận lệnh điều khiển từ Cloud.
* Dựng Docker Compose chạy local MQTT Broker + ThingsBoard/Grafana để hiển thị dữ liệu trực quan.

### Tuần 6: Tích hợp Watchdog & Kiểm thử Độ tin cậy (Reliability Benchmark)
* Tích hợp `sd_notify("WATCHDOG=1")` trong main loop của `gateway-engine`.
* Thực hiện các bài test độ bền:
  1. *Test Deadlock/Kill Process:* Làm treo tiến trình để kiểm tra Watchdog reboot OS trong 30 giây.
  2. *Test Sudden Power-Cut:* Cắt nguồn ảo đột ngột trong lúc ghi dữ liệu $\rightarrow$ Kiểm tra phân vùng Read-Only và file SQLite có bị corrupt không.
  3. *Test OTA Corruption:* Cài bundle lỗi $\rightarrow$ Kiểm tra U-Boot tự động rollback về slot cũ sau 3 lần boot.
  4. *Test Network Failover:* Ngắt mạng 30 phút rồi bật lại $\rightarrow$ Đo tỉ lệ thất thoát gói tin (yêu cầu đạt 0% packet loss).

### Tuần 7: Porting lên Board Thật, Đo đạc Chỉ số & Hoàn thiện Báo cáo ĐATN
* Tích hợp BSP Layer cho phần cứng thật (Raspberry Pi 4 / BeagleBone Black).
* Kết nối cảm biến Modbus RS-485 thật và node CAN thật.
* Thu thập số liệu định lượng cho báo cáo đồ án:
  * Thời gian khởi động hệ thống (`systemd-analyze`, `systemd-analyze blame/plot`).
  * Dung lượng Image, RAM tĩnh/động tiêu thụ, CPU load.
  * Tốc độ xử lý (Throughput & Latency) của gateway engine.
* Hoàn thiện tài liệu kiến trúc, sơ đồ phân vùng và viết hoàn chỉnh quyển Báo cáo Đồ án Tốt nghiệp.

---

## 7. Bộ Tiêu chí Đánh giá Định lượng (Dành cho Báo cáo Đồ án Tốt nghiệp)

| Hạng mục đánh giá | Chỉ số mục tiêu (Target KPI) | Công cụ / Phương pháp đo lường |
| :--- | :--- | :--- |
| **Thời gian Boot (Boot Time)** | $< 8 - 12$ giây (từ lúc cấp nguồn đến khi daemon chạy) | `systemd-analyze time`, `systemd-analyze plot` |
| **Dung lượng Hệ điều hành (Image Size)** | RootFS $< 80 - 150\text{ MB}$ (tối ưu, không chứa package thừa) | `du -sh /`, Bitbake build output stats |
| **Mức tiêu thụ RAM tĩnh** | $< 40 - 70\text{ MB}$ RAM khi hệ thống chạy toàn bộ services | `free -m`, `htop`, `/proc/meminfo` |
| **Độ tin cậy khi mất mạng (Failover)** | **0% mất gói tin** khi mất kết nối mạng (Store-and-Forward) | So sánh số lượng record tại cảm biến vs Database Cloud |
| **Thời gian phát hiện & Reboot Watchdog** | $\le 30$ giây khi tiến trình bị treo hoặc deadlock | Systemd Watchdog timer & Kernel dmesg logs |
| **Tính an toàn OTA & Rollback** | **100% Rollback thành công** khi nạp firmware lỗi; không biến thiết bị thành "brick" | Kịch bản kiểm thử RAUC + U-Boot Bootcount |
| **Tính toàn vẹn hệ thống (Power-cut Test)** | **0 lỗi hỏng phân vùng (0% corruption)** sau $\ge 50$ lần ngắt nguồn cưỡng bức | Kịch bản đóng ngắt relay nguồn tự động |