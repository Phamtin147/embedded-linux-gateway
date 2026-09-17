# Embedded Linux Industrial IoT Gateway

Đồ án tốt nghiệp: Thiết bị **Industrial IoT Edge Gateway** chạy hệ điều hành Linux nhúng tùy biến xây dựng từ **Yocto Project**, tích hợp thu thập dữ liệu công nghiệp (Modbus RTU, SocketCAN), lưu trữ SQLite offline buffer, đồng bộ MQTT Cloud, và hỗ trợ nâng cấp firmware an toàn qua **RAUC A/B Dual-Slot OTA**.

---

## 📁 Cấu trúc Dự án (Monorepo)

```plaintext
embedded-linux-gateway/
├── docs/                      # Tài liệu đồ án, sơ đồ kiến trúc, hướng dẫn tích hợp OTA
├── layers/
│   └── meta-iiot-gateway/     # Custom Yocto Layer (BSP, recipes, RAUC, image)
├── scripts/
│   ├── env-setup.sh           # Script kéo Poky, meta-oe, meta-rauc và setup build env
│   ├── build-image.sh         # Script bọc lệnh BitBake build OS image
│   ├── run-qemu.sh            # Chạy giả lập QEMU ARM
│   ├── mock-sensors.sh        # Tạo SocketCAN vcan0 và cổng serial ảo socat
│   └── generate-ota-bundle.sh # Tạo X.509 certs và đóng gói RAUC OTA bundle
├── dashboard/                 # Môi trường Docker Compose (Mosquitto MQTT Broker + ThingsBoard)
└── src/
    └── gateway-engine/        # Mã nguồn C++17 Core Edge Daemon (CMake)
```

---

## 🚀 Hướng dẫn Bắt đầu Nhanh

### 1. Chuẩn bị môi trường Build Yocto
```bash
./scripts/env-setup.sh
```

### 2. Build Image Hệ điều hành (QEMU ARM)
```bash
./scripts/build-image.sh
```

### 3. Chạy giả lập trên QEMU
```bash
./scripts/run-qemu.sh
```

### 4. Khởi chạy Cloud Dashboard (Host)
```bash
cd dashboard
docker compose up -d
```
Truy cập giao diện Web: `http://localhost:8080`

---

## 📖 Chi tiết Kỹ thuật
- Xem toàn bộ đặc tả kỹ thuật và lộ trình 7 tuần tại [`SPECIFICATION.md`](SPECIFICATION.md).
- Xem khối kiến thức nền tảng tại [`KNOWLEDGE.md`](KNOWLEDGE.md).
- Xem hướng dẫn lựa chọn & phân tích phần cứng tại [`docs/hardware-selection-guide.md`](docs/hardware-selection-guide.md).

