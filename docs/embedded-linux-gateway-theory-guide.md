# HƯỚNG DẪN KIẾN THỨC LÝ THUYẾT & BẢN CHẤT CÔNG NGHỆ
## ĐỒ ÁN TỐT NGHIỆP: EMBEDDED LINUX INDUSTRIAL IOT GATEWAY

Tài liệu này được biên soạn dành cho sinh viên ngành **Kỹ thuật Máy tính / Hệ thống Nhúng / Điện tử Viễn thông / Tự động hóa** nhằm chuẩn bị kiến thức lý thuyết vững chắc khi làm đồ án và trả lời phỏng vấn phản biện trước Hội đồng tốt nghiệp.

---

## 1. TỔNG QUAN KIẾN TRÚC HỆ THỐNG IIoT 3 TẦNG

```
+-----------------------------------------------------------------------+
|                       TẦNG 3: CLOUD / SCADA SERVER                    |
|          ThingsBoard CE (PostgreSQL + TimescaleDB + Web UI)           |
+-----------------------------------------------------------------------+
                                   ▲
                                   │  MQTT over TCP / TLS (Port 1883/8883)
                                   ▼
+-----------------------------------------------------------------------+
|             TẦNG 2: EDGE GATEWAY (TRỌNG TÂM ĐỒ ÁN TỐT NGHIỆP)         |
|                                                                       |
|  +-----------------------------------------------------------------+  |
|  | C++ Gateway Engine Daemon (gateway-engine)                      |  |
|  | - Modbus Poller (libmodbus)      - CAN Receiver (SocketCAN)     |  |
|  | - SQLite 3 WAL Store-and-Forward - Paho MQTT Async Client       |  |
|  +-----------------------------------------------------------------+  |
|                                  │  sd_notify()                        |
|  +-------------------------------▼---------------------------------+  |
|  | Systemd Init System & Watchdog Supervisor                       |  |
|  +-----------------------------------------------------------------+  |
|                                  │                                    |
|  +-------------------------------▼---------------------------------+  |
|  | Linux Kernel 6.6 LTS & Drivers (SocketCAN, RS485, Watchdog)     |  |
|  +-----------------------------------------------------------------+  |
|                                  │                                    |
|  +-------------------------------▼---------------------------------+  |
|  | Phân vùng & Bảo vệ (Read-Only RootFS, RAUC A/B OTA, U-Boot)      |  |
|  +-----------------------------------------------------------------+  |
+-----------------------------------------------------------------------+
               ▲                                        ▲
               │ RS-485 (Vi sai 2 dây)                  │ CAN Bus 2.0B (CAN_H, CAN_L)
               ▼                                        ▼
+-----------------------------+          +------------------------------+
| TẦNG 1: HIỆN TRƯỜNG MODBUS  |          | TẦNG 1: HIỆN TRƯỜNG CAN BUS  |
| Biến tần, Đồng hồ điện, PLC |          | Cảm biến động cơ, Pin BMS... |
+-----------------------------+          +------------------------------+
```

---

## 2. BẢN CHẤT CÁC CÔNG NGHỆ CỐT LÕI (CORE EMBEDDED CONCEPTS)

### 2.1. Yocto Project & Custom Embedded Linux
* **Tại sao không dùng Raspbian / Ubuntu Server?**
  * Ubuntu / Raspbian chứa hàng trăm packages desktop, thừa thãi và không kiểm soát được phiên bản (Bloatware, >2GB dung lượng).
  * Khó đạt chuẩn công nghiệp: Không thể đóng gói hệ thống thành các image phân vùng chuẩn A/B bất biến (immutable), boot lâu (>30s).
* **Bản chất của Yocto Project:**
  * Yocto không phải là một hệ điều hành Linux! Yocto là một **Build System (Hệ thống xây dựng)** giúp bạn biên dịch (cross-compile) từ mã nguồn Kernel, Bootloader, Thư viện C (`glibc/musl`) và User-space applications thành một bản phân phối Linux theo ý muốn (Tailor-made Distribution).
  * Kích thước OS tạo ra chỉ từ 30MB - 120MB, thời gian khởi động (Boot time) giảm xuống dưới 3-5 giây.

### 2.2. Phân vùng Read-Only RootFS (Chống hỏng thẻ nhớ / Flash)
* **Vấn đề trong công nghiệp:**
  * Các tủ điện công nghiệp thường xuyên bị ngắt điện đột ngột (Power outage / Cúp cầu dao). Nếu phân vùng root (`/`) được mount ở chế độ đọc-ghi (`rw`), các sector đang ghi dở của filesystem (ext4) sẽ bị corrupt inode -> Thiết bị không thể boot lại được. Thẻ nhớ SD/eMMC cũng nhanh hỏng do ghi liên tục vào cùng vị trí.
* **Giải pháp kiến trúc:**
  * Toàn bộ hệ điều hành (`/usr`, `/bin`, `/lib`, `/etc`) được mount **Read-Only (`ro`)**.
  * Các thư mục cần ghi dữ liệu tạm (logs, runtime sockets) được mount lên RAM qua **`tmpfs`** (`/run`, `/tmp`).
  * Chỉ duy nhất một phân vùng dữ liệu riêng biệt **`/data`** (hoặc SQLite database) được mount ở chế độ ghi (`rw`) với cơ chế journal chống corrupt.

### 2.3. Cập nhật an toàn RAUC A/B Dual-Slot OTA Update
* **Cơ chế hoạt động:**
  * Bộ nhớ Flash được chia thành 2 phân vùng RootFS giống hệt nhau: **Slot A** và **Slot B**.
  * Nếu hệ thống đang chạy trên Slot A:
    1. Bản cập nhật mới (Bundle `.raucb`) được tải về và ghi vào **Slot B**.
    2. RAUC kiểm tra chữ ký số mật mã (X.509 Cryptographic Certificate) để đảm bảo firmware không bị hacker sửa đổi.
    3. RAUC ra lệnh cho Bootloader (U-Boot) chuyển cờ ưu tiên khởi động sang Slot B với số lần thử khởi động (`bootcount=3`).
    4. Thiết bị khởi động lại (Reboot) vào Slot B.
    5. Nếu Gateway Engine khởi động thành công và ping Watchdog -> Đánh dấu Slot B là `good`.
    6. Nếu Slot B bị lỗi kernel panic hoặc crash liên tục -> U-Boot tự động đếm hết số lần thử và **Rollback (quay trở lại)** an toàn về Slot A. Thiết bị không bao giờ bị biến thành "cục gạch" (Bricked).

### 2.4. Hardware Watchdog & Systemd Watchdog Supervisor
* **Hardware Watchdog (`/dev/watchdog`):**
  * Một bộ đếm phần cứng độc lập với CPU. CPU phải định kỳ gửi tín hiệu "đá watchdog" (Kick the dog). Nếu CPU bị đơ phần cứng hoặc kernel panic -> Watchdog đếm về 0 và kéo chân Reset của SoC -> Reboot máy.
* **Systemd Watchdog Service (`sd_notify("WATCHDOG=1")`):**
  * Được tích hợp trong ứng dụng C++ `gateway-engine`. Cứ mỗi chu kỳ vòng lặp, ứng dụng gọi `sd_notify(0, "WATCHDOG=1")`. Nếu tiến trình bị rơi vào vòng lặp vô tận (Infinite loop) hoặc Deadlock ở tầng người dùng (Userspace), Systemd sẽ phát hiện và tự động kill tiến trình rồi khởi động lại (`Restart=always`).

---

## 3. TẦNG GIAO THỨC CÔNG NGHIỆP (INDUSTRIAL PROTOCOLS)

### 3.1. Modbus RTU qua RS-485
* **Tầng vật lý RS-485:**
  * Truyền tín hiệu vi sai 2 dây: $V_A - V_B$. Chống nhiễu điện từ trường (EMI) cực tốt trong môi trường nhà máy, khoảng cách truyền xa tới 1200m.
* **Cơ chế Master-Slave:**
  * Gateway đóng vai trò là **Modbus Master**.
  * Các thiết bị đo đếm (Inverter, Power Meter) là **Modbus Slave** (được đánh địa chỉ ID từ 1 đến 247).
  * Master gửi yêu cầu (Request frame): Mã hàm (Function code) `0x03` (Read Holding Registers), địa chỉ thanh ghi bắt đầu, số lượng thanh ghi cần đọc.
  * Slave tính toán mã kiểm tra dư thừa tuần hoàn **CRC16 (Cyclic Redundancy Check)** và phản hồi kết quả trong thời gian timeout định trước.

### 3.2. Linux SocketCAN (CAN Bus 2.0B)
* **Tại sao CAN Bus phổ biến trong công nghiệp & xe hơi?**
  * CAN Bus dùng phương pháp phân xử quyền truy cập đường truyền không phá hủy (Non-destructive bitwise arbitration) dựa trên CAN ID: ID có giá trị số càng nhỏ thì độ ưu tiên càng cao.
  * Tốc độ truyền cao (lên đến 1 Mbps), tự động phát hiện lỗi và truyền lại ở tầng phần cứng (Hardware auto-retransmission).
* **Kiến trúc SocketCAN của Linux:**
  * SocketCAN tích hợp CAN bus trực tiếp vào subsystem mạng của Linux Kernel (`AF_CAN`).
  * Khác với giao diện Serial truyền thống (chỉ cho phép 1 chương trình mở cổng COM tại một thời điểm), SocketCAN cho phép **nhiều tiến trình cùng mở và đọc/ghi song song trên cùng một bus CAN** nhờ hàng đợi (queuing) của Kernel.
  * Hỗ trợ bộ lọc phần cứng (Kernel-level CAN Filters): Kernel loại bỏ ngay các frame không cần thiết trước khi truyền lên Userspace, giúp tiết kiệm xung nhịp CPU.

---

## 4. BỘ ĐỆM NGOẠI TUYẾN CHỐNG MẤT DỮ LIỆU (STORE-AND-FORWARD VỚI SQLITE WAL)

* **Vấn đề thực tế:** Mạng công nghiệp (4G LTE, Wi-Fi nhà xưởng) rất dễ bị ngắt kết nối do nhiễu sóng hoặc bảo trì mạng viễn thông. Nếu không có bộ đệm, dữ liệu cảm biến đo được trong lúc mất mạng sẽ bị mất vĩnh viễn (Data Loss).
* **Tại sao chọn SQLite 3 WAL Mode?**
  * SQLite là cơ sở dữ liệu phi máy chủ (Serverless), lưu toàn bộ trong một file cục bộ, cực kỳ nhẹ (~600KB bộ nhớ) phù hợp hệ thống nhúng.
  * **WAL (Write-Ahead Logging)**: Thay vì ghi đè trực tiếp lên file cơ sở dữ liệu chính (dễ lỗi khi mất nguồn), SQLite WAL ghi tất cả thay đổi vào file log phụ (`-wal`).
  * **Ưu điểm vượt trội của WAL:**
    1. Đọc và Ghi diễn ra đồng thời (Non-blocking): Tiến trình đọc pending dữ liệu không block tiến trình ghi cảm biến mới.
    2. Tốc độ ghi tuần tự (Sequential Write) cực nhanh, giảm đáng kể hiện tượng mòn chip nhớ Flash (Wear-leveling friendly).
* **Quy tắc hàng đợi FIFO (First-In, First-Out):**
  * Dữ liệu sinh ra khi mất mạng được lưu vào bảng `telemetry_buffer`.
  * Khi mạng phục hồi, Gateway kéo các bản ghi cũ nhất (`ORDER BY id ASC`) đẩy dần lên Cloud. Chỉ khi Cloud xác nhận nhận thành công (MQTT ACK), bản ghi mới được xóa khỏi DB (`mark_delivered`).

---

## 5. BỘ CÂU HỎI VẤN ĐÁP BẢO VỆ TỐT NGHIỆP (Q&A CHEAT SHEET)

#### Câu 1: Tại sao em không cài Raspbian có sẵn cho tiện mà phải tốn thời gian build Yocto?
> **Trả lời:** Dạ thưa Thầy/Cô, Raspbian là hệ điều hành dành cho giáo dục và đa mục đích, dung lượng lớn (>2GB), chứa nhiều dịch vụ nền không cần thiết và mặc định là phân vùng Read-Write rất dễ hỏng hệ thống khi mất nguồn. Trong các ứng dụng công nghiệp (IIoT), sự ổn định và tối ưu là tiên quyết. Em sử dụng Yocto để tạo ra một bản Linux chuyên dụng (Minimal Image ~100MB), thời gian khởi động nhanh dưới 5 giây, tích hợp Read-Only RootFS chống hỏng file hệ thống và cấu hình chuẩn cơ chế cập nhật firmware kép A/B (RAUC) chuẩn công nghiệp.

#### Câu 2: Read-Only RootFS nghĩa là gì? Nếu Read-Only thì làm sao ứng dụng của em ghi log và lưu trữ dữ liệu tạm?
> **Trả lời:** Dạ thưa Thầy/Cô, Read-Only RootFS là cơ chế gắn phân vùng gốc (`/`) ở chế độ chỉ đọc, ngăn chặn mọi thao tác ghi vào mã thực thi và cấu hình cốt lõi. Để giải quyết việc ghi dữ liệu:
> - Các thư mục dữ liệu tạm thời và socket tiến trình (`/tmp`, `/run`, `/var/run`) được mount lên RAM thông qua hệ thống tập tin ảo `tmpfs`.
> - Dữ liệu cần lưu trữ lâu dài (Database SQLite của Gateway) được lưu trữ trên một phân vùng dữ liệu riêng biệt (`/data`) được mount ở chế độ đọc-ghi (`rw`) độc lập. Khi phân vùng dữ liệu có sự cố, hệ điều hành chính vẫn khởi động bình thường.

#### Câu 3: Làm thế nào để đảm bảo hệ thống không bị mất dữ liệu khi mất kết nối mạng Internet?
> **Trả lời:** Dạ thưa Thầy/Cô, em thiết kế cơ chế **Store-and-Forward**:
> - Khi MQTT Client báo mất kết nối, toàn bộ bản tin JSON Telemetry từ Modbus và CAN sẽ tự động chuyển hướng đẩy vào cơ sở dữ liệu SQLite cục bộ được kích hoạt chế độ **WAL (Write-Ahead Logging)**.
> - Khi kết nối Internet khôi phục, một luồng nền sẽ truy vấn các bản ghi cũ nhất theo thứ tự FIFO và gửi tuần tự lên Cloud. Chỉ sau khi gửi thành công thì bản ghi mới được đánh dấu xóa, đảm bảo 0% mất mát dữ liệu (Zero Data Loss).

#### Câu 4: Phân biệt Modbus RTU và Modbus TCP? Tại sao trong đồ án em dùng Modbus RTU?
> **Trả lời:** Dạ thưa Thầy/Cô:
> - **Modbus RTU** truyền qua giao tiếp nối tiếp RS-485, dữ liệu đóng gói dạng nhị phân, kiểm tra toàn vẹn bằng mã CRC16.
> - **Modbus TCP** truyền qua hạ tầng mạng Ethernet/IP, đóng gói dữ liệu Modbus PDU vào bên trong TCP frame với header MBAP (Modbus Application Protocol), kiểm tra toàn vẹn dựa vào checksum của TCP.
> - Em chọn Modbus RTU vì trong các nhà máy hiện hữu, phần lớn biến tần, đồng hồ đo điện năng và cảm biến ngoại vi đều giao tiếp qua bus nối tiếp RS-485 do chi phí thấp và khoảng cách kéo dây xa.

#### Câu 5: SocketCAN trên Linux có ưu điểm gì so với việc viết thư viện giao tiếp cổng nối tiếp thông thường?
> **Trả lời:** Dạ thưa Thầy/Cô, SocketCAN đưa bus CAN trở thành một giao diện mạng (Network Interface) của Linux Kernel. Nó cho phép tận dụng Berkeley Socket API tiêu chuẩn (`socket(PF_CAN, SOCK_RAW, CAN_RAW)`). Ưu điểm cốt lõi là:
> 1. Cho phép nhiều ứng dụng người dùng cùng truy cập song song vào 1 bus CAN mà không xung đột nhờ hàng đợi điều phối của Kernel.
> 2. Cho phép cài đặt bộ lọc phần cứng (Hardware/Kernel Filters) trực tiếp tại tầng Kernel, loại bỏ frame rác trước khi đẩy lên Userspace, tiết kiệm tài nguyên CPU của hệ thống nhúng.
