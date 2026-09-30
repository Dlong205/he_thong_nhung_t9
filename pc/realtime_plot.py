"""Realtime Plot - Hiển thị 4 đường nét đồ thị."""
from vpython import graph, gcurve, color

# Đã mở rộng kích thước width=1200
graph_roll = graph(title="Góc Roll (deg)", xtitle="t (s)", ytitle="deg", width=1200, height=250)
c_roll_acc  = gcurve(graph=graph_roll, color=color.gray(0.5), label="Accel")
c_roll_gyro = gcurve(graph=graph_roll, color=color.blue, label="Gyro")
c_roll_cf   = gcurve(graph=graph_roll, color=color.orange, label="Complementary")
c_roll_kf   = gcurve(graph=graph_roll, color=color.red, label="Kalman")

graph_pitch = graph(title="Góc Pitch (deg)", xtitle="t (s)", ytitle="deg", width=1200, height=250)
c_pitch_acc  = gcurve(graph=graph_pitch, color=color.gray(0.5), label="Accel")
c_pitch_gyro = gcurve(graph=graph_pitch, color=color.blue, label="Gyro")
c_pitch_cf   = gcurve(graph=graph_pitch, color=color.orange, label="Complementary")
c_pitch_kf   = gcurve(graph=graph_pitch, color=color.red, label="Kalman")

def update_plot(t_s: float, roll: list, pitch: list) -> None:
    """roll và pitch là mảng 4 phần tử: [acc, gyro, cf, kalman]."""
    c_roll_acc.plot(t_s, roll[0]);   c_roll_gyro.plot(t_s, roll[1])
    c_roll_cf.plot(t_s, roll[2]);    c_roll_kf.plot(t_s, roll[3])
    
    c_pitch_acc.plot(t_s, pitch[0]); c_pitch_gyro.plot(t_s, pitch[1])
    c_pitch_cf.plot(t_s, pitch[2]);  c_pitch_kf.plot(t_s, pitch[3])