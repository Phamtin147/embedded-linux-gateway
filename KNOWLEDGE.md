Để làm chủ và triển khai trọn vẹn đề tài **Industrial IoT Gateway trên Embedded Linux**, bạn cần trang bị khối kiến thức trải rộng từ tầng hệ thống cấp thấp (Low-level / Kernel) đến tầng ứng dụng phân tán (Application / Networking).

Dưới đây là các nhóm kiến thức cốt lõi được phân loại chi tiết:

---

### 1. Kiến thức cốt lõi về Yocto Project & Build System

Đây là công cụ nền tảng để tạo ra hệ điều hành:

* **Khái niệm cơ bản của OpenEmbedded/BitBake:** Cơ chế hoạt động của `recipes` (`.bb`), `append files` (`.bbappend`), `classes` (`.bbclass`), `layers` (`meta-*`), và cấu hình `conf/local.conf`, `conf/bblayers.conf`.
* **Quy trình Build (Task Execution):** Hiểu các task cơ bản như `do_fetch`, `do_patch`, `do_configure`, `do_compile`, `do_install`, `do_package`.
* **Cross-Compilation Toolchain:** Hiểu cách toolchain cross-compile mã nguồn từ x86_64 (Host) sang ARM/AArch64 (Target), khái niệm `sysroot` và link thư viện C (`glibc`/`musl`).
* **Custom Image & Layer Creation:** Cách viết một layer mới từ đầu, tạo custom image recipe kế thừa từ `core-image-minimal` hoặc `core-image-base`.

---

### 2. Kiến trúc Linux Nhúng & Quản trị Hệ thống cấp thấp

* **Quy trình khởi động Linux (Boot Flow):**
* `ROM Bootloader` $\rightarrow$ `SPL` $\rightarrow$ `U-Boot` $\rightarrow$ `Linux Kernel` $\rightarrow$ `Init System (systemd)`.


* **Cấu hình Bootloader (U-Boot):** Cách quản lý biến môi trường U-Boot (`u-boot-env`), viết script boot (`boot.scr`/`boot.cmd`), cấu hình đếm lượt boot (`bootcount`) phục vụ fallback A/B.
* **Linux Kernel & Device Tree:**
* Cách tùy biến `defconfig` (bật/tắt driver qua `menuconfig`).
* Hiểu cơ bản về `Device Tree Source` (`.dts`/`.dtsi`) để map chân ngoại vi (UART, SPI, I2C, CAN).


* **Quản lý File System & Storage:**
* Phân vùng bộ nhớ (`GPT`, `MBR`).
* Khái niệm **Read-Only RootFS**, cơ chế ghi tạm qua `tmpfs` và `overlayfs`.
* Cơ chế hoạt động của **systemd**: Quản lý service (`.service`), systemd-networkd, và cơ chế gửi heartbeat cho Watchdog (`sd_notify`).



---

### 3. Cơ chế Độ tin cậy & Nâng cấp Hệ thống (Reliability & OTA)

* **Cơ chế Dual-Boot (A/B Partitioning):** Nguyên lý chuyển đổi phân vùng Active/Inactive, cách ly dữ liệu user (`/data`) và dữ liệu OS.
* **Framework RAUC:**
* Hiểu cấu trúc file bundle `.raucb`.
* Cách tạo và quản lý chứng chỉ bảo mật X.509 (Public Key Infrastructure - PKI) để ký số firmware.
* Cấu hình file `system.conf` ánh xạ với các phân vùng trên flash/eMMC.


* **Hardware Watchdog:** Cơ chế tương tác với `/dev/watchdog` và chống treo phần cứng.

---

### 4. Giao thức Truyền thông Công nghiệp & IoT (Networking & Protocols)

* **Serial & RS-485 / Modbus RTU:**
* Cấu hình baudrate, parity, stop bits qua API `termios` trên Linux.
* Cấu trúc khung truyền Modbus (Function codes: 03 Read Holding Registers, 06 Write Single Register, CRC16).


* **CAN Bus & Linux SocketCAN:**
* Cách Linux trừu tượng hóa mạng CAN thành network interface (`can0`, `vcan0`).
* Lập trình mạng SocketCAN (sử dụng `AF_CAN`, struct `sockaddr_can`, `can_frame`).


* **MQTT Protocol:**
* Kiến trúc Publish/Subscribe, Topics, QoS levels (0, 1, 2), Keep-Alive, Last Will and Testament (LWT).


* **Bảo mật mạng cơ bản:** Cấu hình tường lửa `nftables`/`iptables`, cấu hình SSH Key, vô hiệu hóa các cổng không bảo mật.

---

### 5. Lập trình Hệ thống (System Programming - C++ hoặc Rust)

* **Linux System Calls:** Quản lý tiến trình/luồng (`pthreads`/`std::thread`), lập trình I/O (`open`, `read`, `write`, `select`/`epoll`), xử lý tín hiệu hệ thống (`SIGTERM`, `SIGINT`).
* **Tương tác Thư viện:**
* Dùng `libmodbus` hoặc tự viết parser Modbus qua serial.
* Dùng thư viện `paho-mqtt-cpp` (hoặc `rumqttc` nếu dùng Rust) để quản lý kết nối MQTT bất đồng bộ.
* Dùng `sqlite3` C API (bật chế độ WAL mode để đọc/ghi đồng thời an toàn) phục vụ cơ chế Offline Buffer.


* **Tư duy thiết kế phần mềm nhúng:** Xử lý lỗi ngoại lệ chặt chẽ, chống memory leak (dùng Smart Pointers trong C++ hoặc tính năng Ownership của Rust), thiết kế Non-blocking I/O.

---

### Bảng lộ trình tự học / ôn tập theo độ ưu tiên

| Giai đoạn | Nội dung cần nắm chắc | Mục tiêu cần đạt |
| --- | --- | --- |
| **Pha 1 (Nền tảng)** | C/C++ System Programming, Linux Command Line, Git, Socket Network | Viết được một chương trình C++ đọc socket và ghi SQLite. |
| **Pha 2 (Yocto cơ bản)** | BitBake syntax, Layer structure, chạy QEMU | Build thành công một bản Linux tối giản chạy được trên QEMU. |
| **Pha 3 (Protocols & Daemons)** | SocketCAN (`vcan`), Modbus RTU, MQTT Broker | Chạy daemon C++ giao tiếp qua CAN ảo và đẩy MQTT lên Cloud. |
| **Pha 4 (Nâng cao & System)** | U-Boot scripting, RAUC OTA, Read-only FS, Watchdog | Thực hiện update A/B thành công và test rollback khi gặp lỗi. |