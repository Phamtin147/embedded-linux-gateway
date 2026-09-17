# HƯỚNG DẪN LỰA CHỌN PHẦN CỨNG (HARDWARE SELECTION GUIDE)
## Đồ án: Embedded Linux Industrial IoT Gateway

Tài liệu này tổng hợp phân tích kỹ thuật, phương án lựa chọn phần cứng từ mức tối thiểu (tiết kiệm chi phí) đến tối đa (chuẩn công nghiệp), bảng so sánh hiệu năng và checklist kiểm tra khi mua linh kiện.

---

## 1. Chiến lược Triển khai Phần cứng 2 Pha

Hệ thống được thiết kế theo kiến trúc tách biệt giữa logic phần mềm và tầng giao tiếp phần cứng (HAL/Kernel Driver), cho phép thực hiện 2 pha:

* **Pha 1 (Mô phỏng 0 đồng trên QEMU ARM)**:
  * Sử dụng `qemuarm` / `qemuaarch64`.
  * Giả lập cổng nối tiếp Modbus RTU qua `socat` (`/tmp/ttyVIRT0` $\leftrightarrow$ `/tmp/ttyVIRT1`).
  * Giả lập mạng CAN qua Linux Virtual CAN (`vcan0`).
  * Phát triển trọn vẹn: Base OS Yocto, U-Boot bootcount, RAUC OTA A/B, Store-and-Forward SQLite và C++ Engine.
* **Pha 2 (Nghiệm thu trên Phần cứng thật)**:
  * Nạp hệ điều hành đã đóng gói lên board nhúng vật lý.
  * Đấu nối module cảm biến Modbus RS-485 và node CAN thật.
  * Thực hiện đo đạc benchmark (thời gian boot, tiêu thụ tài nguyên, kiểm thử ngắt nguồn đột ngột).

---

## 2. Các Phương án Lựa chọn Phần cứng

### Phương án A: Cấu hình TỐI THIỂU (Khuyên dùng cho ĐATN - Ngân sách ~500k - 1.5 triệu)

Mục tiêu: Đạt 100% tiêu chí nghiệm thu đề tài với chi phí tối ưu nhất, dễ tìm mua tại Việt Nam.

| STT | Thành phần | Thiết bị đề xuất | Đơn giá ước tính | Ghi chú kỹ thuật |
| :---: | :--- | :--- | :---: | :--- |
| 1 | **Bo mạch chính** | **Raspberry Pi 3 Model B** *(hoặc Pi 4B 2GB)* | ~450.000 – 1.200.000 đ | Cortex-A53 4 nhân, 1GB RAM. Chạy Yocto cực mượt (chỉ ăn ~50MB RAM). Hỗ trợ Yocto & RAUC hoàn hảo. |
| 2 | **Bộ nhớ OS** | Thẻ nhớ **MicroSD 16GB / 32GB** (Class 10 / A1) | ~100.000 đ | SanDisk Ultra/Extreme. Chia 3 phân vùng: Slot A (2GB), Slot B (2GB), `/data` (phần còn lại). |
| 3 | **Cáp Debug Console** | Mạch **USB to TTL (CP2102 hoặc CH340)** | ~25.000 đ | **Bắt buộc phải có**: Nối TX/RX vào GPIO 14/15 của Pi để debug log bootloader U-Boot & Kernel. |
| 4 | **Giao tiếp Modbus** | Module **TTL to RS-485 (MAX485 / SP3485)** | ~15.000 đ | Nối vào cổng UART của Pi để xuất ra 2 dây vi sai A/B chuẩn RS-485. |
| 5 | **Giao tiếp CAN Bus** | Module **MCP2515 SPI CAN Bus** (có sẵn TJA1050) | ~25.000 đ | Giao tiếp qua chân SPI của Pi (CS, MOSI, MISO, SCK, INT), Linux nhận diện thành network interface `can0`. |
| 6 | **Cảm biến Modbus Slave**| Cảm biến nhiệt ẩm **RS-485 SHT20 / SHT30** | ~100.000 – 130.000 đ | Cảm biến công nghiệp vỏ chống nước, giao tiếp Modbus RTU Slave chuẩn xác. |
| 7 | **Node phát CAN mẫu** | 1 mạch **ESP32** (hoặc STM32) + Module **SN65HVD230** | ~90.000 đ | Lập trình phát giả lập khung tin CAN (vòng tua động cơ, thông số pin BMS). |
| 8 | **Nguồn cấp** | Củ sạc 5V 2.5A/3A Micro-USB (hoặc nguồn 24V nếu có mạch hạ áp) | ~50.000 – 100.000 đ | Nguồn ổn định chống sụt áp (`undervoltage`). |

> 🌟 **Lựa chọn đặc biệt (Combo thanh lý công nghiệp)**: 
> Bộ **Raspberry Pi 3B kèm vỏ hộp Samsung GTC & Mạch mở rộng công nghiệp** (~456.000 đ):
> * Đã có sẵn: Pi 3B + Vỏ hộp nhôm bắt vít tủ điện + Domino đấu dây + Mạch nguồn xung hạ áp 24V sang 5V + Chip ADC 7 kênh Analog (`A1 - A7`).
> * Chỉ cần mua thêm: 1 module TTL to RS485 (~15k) + 1 module MCP2515 CAN (~25k) là thành một Gateway công nghiệp hoàn chỉnh.

---

### Phương án B: Cấu hình TỐI ĐA (Chuẩn Thiết bị Công nghiệp Thương mại)

Mục tiêu: Đóng gói thành sản phẩm mẫu (Prototype) chuẩn công nghiệp cao cấp, chịu rung lắc nhiệt độ cao, gắn tủ điện DIN-Rail.

| Khối chức năng | Phần cứng chi tiết | Đặc điểm công nghiệp chuyên sâu |
| :--- | :--- | :--- |
| **Khối Xử lý chính** | **Raspberry Pi Compute Module 4 (CM4)** kèm bộ nhớ **eMMC** + **Industrial Carrier Board** *(hoặc BeagleBone Black Industrial)* | • eMMC 5.1 hàn chết trên mạch, tốc độ đọc/ghi cao và bền hơn thẻ SD gấp nhiều lần.<br>• Tích hợp chip RTC thời gian thực có pin backup (DS3231).<br>• Tích hợp Hardware Watchdog chuyên dụng độc lập.<br>• Dải nhiệt độ công nghiệp $-40^\circ\text{C} \to 85^\circ\text{C}$. |
| **Giao tiếp Cách ly** | Cổng RS-485 & CAN Bus **cách ly quang học (Optocoupler isolation)** | Sử dụng IC ADM2587E và ISO1050, chống sét lan truyền, chống sốc điện áp cao và triệt tiêu vòng lặp mass (ground loop). |
| **Thiết bị Đầu cuối Thật** | • Đồng hồ đo điện đa năng **PZEM-016 (Modbus RTU)**<br>• Module **4-Channel Relay Modbus RTU** điều khiển tải 220V<br>• Node pin **BMS CAN Bus** thật | Đo dòng điện, điện áp, công suất tiêu thụ thực tế; điều khiển đóng cắt thiết bị thật qua Cloud Downlink. |
| **Khối Nguồn & Vỏ hộp** | Nguồn công nghiệp **MeanWell MDR-20-24** (24V DC DIN-Rail) + Vỏ nhôm gắn thanh ray DIN | Lắp vừa vặn vào các tủ điện phân phối công nghiệp tiêu chuẩn. |
| **Kết nối Mạng** | Module **4G/LTE (SIM7600)** cắm qua khe PCIe/USB | Đảm bảo Gateway truyền dữ liệu độc lập không phụ thuộc mạng Wi-Fi cục bộ. |

---

## 3. So sánh Kỹ thuật Chuyên sâu

### A. So sánh Raspberry Pi 4B vs Compute Module 4 (CM4)
* **Sức mạnh xử lý (CPU/RAM)**: **BẰNG NHAU 100%**. Đều dùng chung chip Broadcom BCM2711 Quad-core Cortex-A72 @ 1.5GHz và RAM LPDDR4.
* **Bộ nhớ lưu trữ**: Pi 4B dùng thẻ MicroSD; CM4 dùng chip nhớ eMMC hàn chết (tốc độ đọc ghi nhanh gấp 2–3 lần, độ bền cao hơn).
* **Khả năng kết nối**:
  * Pi 4B là máy tính nhúng hoàn chỉnh (cắm là chạy).
  * CM4 chỉ là module lõi, **bắt buộc phải có bo mạch đế (Carrier Board)** mới cắm được dây nguồn, LAN, USB.

### B. So sánh Raspberry Pi 3B vs Raspberry Pi 4B
* **Khả năng đáp ứng đồ án**: Cả hai đều đáp ứng **100% yêu cầu**.
* **Mức tiêu thụ tài nguyên thực tế của Gateway**:
  * Edge Daemon C++17 chỉ tiêu tốn ~2% - 5% CPU và ~40MB - 60MB RAM.
  * Vì vậy, RAM 1GB của Pi 3B đã là **quá dư thừa** cho hệ điều hành Yocto tối giản (không chạy giao diện đồ họa nặng).
* **Nhiệt độ & Nguồn**: Pi 3B tiêu thụ dòng thấp hơn (~1.5A - 2.5A) và dùng cổng Micro-USB quen thuộc.

### C. Giải mã thuật ngữ bán hàng: Ký hiệu "2G/8G"
Trong các tin rao bán linh kiện nhúng/Android TV Box:
* Số đầu (**2G**) = **2GB RAM** (bộ nhớ tạm chạy ứng dụng).
* Số sau (**8G**) = **8GB ROM/eMMC** (bộ nhớ lưu trữ flash hàn trên board).

> ⚠️ **LƯU Ý QUAN TRỌNG**: Hãng Raspberry Pi **chưa từng sản xuất Raspberry Pi 3B/3B+ bản 2GB hay 8GB RAM**. Dòng Pi 3B chính hãng luôn chỉ có **1GB RAM** và không có bộ nhớ trong eMMC. Nếu người bán ghi "Pi 3 2G/8G", đó có thể là Orange Pi 3 (hãng Xunlong) hoặc người bán rao kèm thẻ nhớ 8GB. Cần kiểm tra kỹ chữ in trên bo mạch.

---

## 4. Checklist Kiểm tra khi Mua Phần cứng (Inspection Checklist)

Khi mua bo mạch cũ hoặc hàng tháo máy công nghiệp:
- [ ] **Nguồn & Khởi động**: Cắm nguồn kiểm tra đèn LED nguồn (đỏ) sáng ổn định, đèn ACT (xanh lá) nhấp nháy khi nạp thẻ nhớ.
- [ ] **Mạng Ethernet**: Cắm cáp mạng LAN kiểm tra 2 đèn LED cổng RJ45 sáng và router cấp IP.
- [ ] **Khe thẻ nhớ MicroSD**: Lẫy bấm thẻ hoạt động êm, chân đồng bên trong khe thẻ không bị cong vênh/gãy.
- [ ] **Cổng UART Console**: Cắm cáp USB-TTL xem terminal có xuất log U-Boot với baudrate 115200 không.
- [ ] **Mạch nguồn hạ áp 24V (nếu có)**: Cấp thử nguồn 12V/24V vào cổng domino, dùng đồng hồ vạn năng đo chân ra 5V và 3.3V xem điện áp có chuẩn không trước khi cấp cho Pi.
