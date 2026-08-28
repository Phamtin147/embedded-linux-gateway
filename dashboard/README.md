# Cloud & Host Dashboard Environment

Môi trường giả lập Cloud/Dashboard phục vụ demo đồ án tốt nghiệp bằng Docker Compose:
- **Eclipse Mosquitto (MQTT Broker):** Cổng `1883` (MQTT) và `9001` (WebSocket).
- **ThingsBoard CE:** Cổng `8080` (Web UI).

### Cách khởi chạy:
```bash
cd dashboard
docker compose up -d
```

Truy cập giao diện Web Dashboard: `http://localhost:8080`
(Tài khoản mặc định: `sysadmin@thingsboard.org` / `sysadmin` hoặc `tenant@thingsboard.org` / `tenant`).
