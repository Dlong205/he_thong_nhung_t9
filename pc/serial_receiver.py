"""Serial receiver - Đọc UART, ghi CSV và gọi các hàm cập nhật."""
import csv
import os
import time
import serial
from vpython import rate

# Import module từ 2 file cùng thư mục
from realtime_plot import update_plot
from visualizer_3d import update_3d

PORT_NAME = 'COM21'
BAUD_RATE = 115200

try:
    ser = serial.Serial(PORT_NAME, BAUD_RATE, timeout=0.01)
    print(f"Đã kết nối {PORT_NAME}")
except Exception as e:
    print(f"Lỗi mở cổng COM: {e}")
    raise SystemExit(1)

current_dir = os.path.dirname(os.path.abspath(__file__))
CSV_FILE_NAME = os.path.join(current_dir, "IMU_Data_Log.csv")
file_exists = os.path.isfile(CSV_FILE_NAME)

csv_file = open(CSV_FILE_NAME, mode='a', newline='', encoding='utf-8')
csv_writer = csv.writer(csv_file)

if not file_exists:
    csv_writer.writerow(['timestamp', 'roll_acc', 'roll_gyro', 'roll_cf', 'roll_kalman', 
                         'pitch_acc', 'pitch_gyro', 'pitch_cf', 'pitch_kalman'])

print("Đang chờ dữ liệu từ STM32... Nhấn Ctrl+C ở Terminal để dừng chương trình an toàn.")
t0 = time.time()

try:
    while True:
        rate(50)
        while ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8').strip()
                data = line.split(',')
                if len(data) == 9:
                    csv_writer.writerow(data)
                    
                    t_s = time.time() - t0
                    roll = [float(x) for x in data[1:5]]
                    pitch = [float(x) for x in data[5:9]]
                    
                    # Gọi hàm cập nhật
                    update_plot(t_s, roll, pitch)
                    update_3d(roll[3], pitch[3])
            except Exception:
                pass
        csv_file.flush()
        
except KeyboardInterrupt:
    print("\nNgười dùng chủ động ngắt chương trình.")
finally:
    # Luôn chạy khi thoát app: giải phóng tài nguyên
    csv_file.close()
    ser.close()
    print("Đã đóng cổng COM và lưu file CSV thành công.")