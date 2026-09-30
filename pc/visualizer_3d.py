"""3D Visualizer - Hiển thị mô hình board mạch."""
import math
from vpython import canvas, box, arrow, label, vector, color

# Đã mở rộng kích thước width=1200, height=450
scene = canvas(title="Mô phỏng MPU6500 3D - SV4", width=1200, height=450, background=color.gray(0.2))
board = box(length=5, width=3, height=0.2, color=color.green)
arrow(pos=vector(0, 0, 0), axis=vector(3, 0, 0), color=color.red, shaftwidth=0.1)
arrow(pos=vector(0, 0, 0), axis=vector(0, 0, 3), color=color.blue, shaftwidth=0.1)
label(text='Pitch (X)', pos=vector(3.5, 0, 0), box=False, color=color.red)
label(text='Roll (Z)', pos=vector(0, 0, 3.5), box=False, color=color.blue)

def update_3d(roll_kalman_deg: float, pitch_kalman_deg: float) -> None:
    """Reset về gốc trước khi xoay để tránh cộng dồn sai số qua các khung hình."""
    board.up = vector(0, 1, 0)
    board.axis = vector(1, 0, 0)
    board.rotate(angle=math.radians(pitch_kalman_deg), axis=vector(1, 0, 0))
    board.rotate(angle=-math.radians(roll_kalman_deg), axis=vector(0, 0, 1))