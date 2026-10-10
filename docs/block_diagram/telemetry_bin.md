# Telemetry binary packet (2026-10-09) — thay CSV ASCII

Theo gợi ý đề: `HEADER | LENGTH | TIMESTAMP | DATA | CHECKSUM`.

## Khung gói (little-endian)

| Trường | Kiểu | Byte | Giá trị / Ghi chú |
| :--- | :--- | :--- | :--- |
| HEADER | 2B | 0..1 | `0xAA 0x55` (sync) |
| LENGTH | u8 | 2 | 81 = số byte từ TS đến hết payload |
| TIMESTAMP | u32 | 3..6 | `HAL_GetTick()` ms |
| DATA | 14×f32 | 7..62 | ax,ay,az, gx,gy,gz, racc,rpacc, rgyro,pgyro, rcf,pcf, rkf,pkf |
| EXEC | u16 | 63..64 | thời gian xử lý mẫu (DWT, us) |
| DT | u16 | 65..66 | chu kỳ lấy mẫu thực (us) |
| rkfix | f32 | 67..70 | Kalman fixed-point roll |
| pkfix | f32 | 71..74 | Kalman fixed-point pitch |
| ref | i8 | 75 | góc servo lệnh (deg, -90..90) |
| yawr | f32 | 76..79 | yaw raw |
| yawt | f32 | 80..83 | yaw bù nghiêng |
| CHECKSUM | u8 | 84 | CRC8 poly 0x07 trên LEN+payload |

Tổng 85 byte/gói. Ở 50Hz = 4.25 KB/s → 115200 thừa sức (11.5 KB/s).
Nếu muốn stream cả 500Hz (38.5 KB/s) chỉ cần đổi `USART_BAUD` về 460800 và `SYS_TELEMETRY_DIV` về 1.

## Vì sao đổi từ ASCII sang binary

- Dòng CSV cũ ~115 byte × 50Hz dùng `printf` float **blocking** → chặn loop tới ~10ms @115200
  → nguy cơ mất mẫu 500Hz. Binary ngắn hơn + TX ngắt ring 512B nên **không chặn loop**.
- Có CRC8 kiểm lỗi truyền; sai gói bị PC loại, không làm bẩn dữ liệu phân tích.
- Đúng "final demo" của đề; baud thấp 115200 chạy thoải mái.

## Kiểm chứng

- Firmware: `telemetry_pkt_count` trong RAM — đọc qua ST-Link phải tăng 50/s.
- PC: `python3 pc/bin_logger.py --port /dev/ttyUSB0 --baud 115200 --out log.csv --sec 30`
  → in `... n goi (bad=0)`; file CSV giống định dạng cũ nên `analysis.py`, `eval_ref.py`,
  `realtime_plot.py`, `tune_grid.py` dùng lại không đổi.
