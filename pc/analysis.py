"""Offline Analysis - Tính toán chỉ số toán học."""
import pandas as pd
import numpy as np
import os

def main():
    current_dir = os.path.dirname(os.path.abspath(__file__))
    csv_file = os.path.join(current_dir, "IMU_Data_Log.csv")

    if not os.path.isfile(csv_file):
        print(f"Lỗi: Không tìm thấy file {csv_file}")
        return

    print("Đang xử lý dữ liệu...")
    df = pd.read_csv(csv_file)
    n_samples = len(df)
    
    if n_samples < 2:
        print("Dữ liệu quá ít để phân tích.")
        return

    # Tính toán thời gian thực thi (chu kỳ truyền)
    dt_array = df['timestamp'].diff().dropna()
    mean_dt = dt_array.mean()

    # Độ lệch chuẩn (Nhiễu)
    std_acc = df['roll_acc'].std()
    std_kf = df['roll_kalman'].std()

    # Sai số (MAE, RMSE)
    mae_roll = np.mean(np.abs(df['roll_acc'] - df['roll_kalman']))
    rmse_roll = np.sqrt(np.mean((df['roll_acc'] - df['roll_kalman'])**2))

    # Độ trôi (Drift)
    drift_rate = (df['roll_gyro'].iloc[-1] - df['roll_gyro'].iloc[0]) / (n_samples * (mean_dt/1000)) if mean_dt > 0 else 0

    print("\n" + "="*45)
    print(" BÁO CÁO PHÂN TÍCH OFFLINE BỘ LỌC KALMAN")
    print("="*45)
    print(f"Tổng số mẫu       : {n_samples} samples")
    print(f"Chu kỳ gửi (mean) : {mean_dt:.2f} ms (~{1000/mean_dt:.1f} Hz)")
    print("-" * 45)
    print(f"Nhiễu thô (Accel) : {std_acc:.4f} độ")
    print(f"Nhiễu sau lọc     : {std_kf:.4f} độ")
    if std_acc > 0:
        print(f"-> Giảm nhiễu     : {((std_acc - std_kf)/std_acc * 100):.1f}%")
    print("-" * 45)
    print(f"Sai số MAE        : {mae_roll:.4f} độ")
    print(f"Sai số RMSE       : {rmse_roll:.4f} độ")
    print(f"Độ trôi Gyro      : {drift_rate:.4f} độ/s")
    print("="*45)

if __name__ == "__main__":
    main()