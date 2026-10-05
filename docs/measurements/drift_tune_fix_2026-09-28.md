# Drift 5.5 phut + Tune + Float/Fixed 2026-09-28 (file drift5p.csv, 16519 mau @50Hz = 330s)

## 1. Drift (de yen tuyet doi)

| PP | Drift/330s | Bien do | std | Ket luan |
| :--- | :--- | :--- | :--- | :--- |
| Gyro-only roll | **+10.46 deg** (3.85 -> 14.31) | 10.5 deg | 3.03 | Troi nang, khong dung duoc |
| Gyro-only pitch | **-3.82 deg** (-0.91 -> -4.74) | 3.8 deg | 1.09 | Troi |
| CF roll | ~0.0 | -0.09..0.16 (0.25) | 0.025 | Khong troi |
| Kalman roll | ~0.0 | -0.05..0.10 (0.15) | 0.024 | Khong troi, min hon CF |
| CF/KF pitch | ~0.0 | span ~0.2 | 0.016-0.017 | Khong troi |

Gate de K03 "Kalman khong troi sau 5 phut": **PASS**. Acc co outlier (min -0.76/max +1.32, ban bi cham nhe) nhung fusion loai bo.

## 2. Tune bang (pc/tune_grid.py, file tinh 30s, bo transient)

- CF alpha: 0.90/0.95/0.97/0.98/0.99/0.995 -> RMSE 0.0173/0.0131/0.0114/**0.0109**/0.0127/0.0179. Chon **0.98** (dang dung).
- Kalman Q/R: mac dinh (0.001/0.003/0.03) RMSE 0.0194, ban (0.002/0.005/0.03) 0.0161 tot hon chut nhung kem on dinh hon; giu mac dinh cho an toan. R=0.01 tot hon R=0.1 (tin acc vua phai).
- Quy trinh: replay offline, khong nap tung bo. Ghi nhan bao cao.

## 3. Float vs Fixed (Q16.16 angle/bias + P float)

- `kalman float-fix: max|e|=0.01deg, mean~0` ca roll/pitch, on dinh suot 330s.
- Ly do kien truc: covariance P/Q/R ~1e-3..1e-6 < LSB Q16 (1.5e-5) nen giu float cho P; angle/bias Q16 du (range +-500 do).
- exec tong ~764us (them kfix so voi 658us ban dau), van < 2000us.
- Can lay so Flash/RAM: `pio run -t size` truoc/sau khi bat kfix (ghi them bao cao).
