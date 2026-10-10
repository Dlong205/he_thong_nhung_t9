"""3D cube xoay theo Roll/Pitch Kalman (rkf,pkf). Chay: python3 visualizer_3d.py --port /dev/ttyACM0"""
import argparse, serial, math
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # noqa
import numpy as np
ap = argparse.ArgumentParser()
ap.add_argument('--port', default='/dev/ttyACM0'); ap.add_argument('--baud', type=int, default=115200)
a = ap.parse_args()
s = serial.Serial(a.port, a.baud, timeout=1)
# Hinh hop chu nhat dai theo X
verts = np.array([[-2,-1,-0.3],[2,-1,-0.3],[2,1,-0.3],[-2,1,-0.3],[-2,-1,0.3],[2,-1,0.3],[2,1,0.3],[-2,1,0.3]])
edges = [(0,1),(1,2),(2,3),(3,0),(4,5),(5,6),(6,7),(7,4),(0,4),(1,5),(2,6),(3,7)]
fig = plt.figure(); ax = fig.add_subplot(111, projection='3d')
plt.ion(); plt.show()
roll = pitch = 0.0
while True:
    line = s.readline().decode(errors='ignore').strip()
    if not line or line.startswith('#') or line.startswith('ts,'): continue
    p = line.split(',')
    if len(p) < 16: continue
    try: roll, pitch = float(p[13]), float(p[14])
    except ValueError: continue
    r, pw = math.radians(roll), math.radians(pitch)
    Rx = np.array([[1,0,0],[0,math.cos(r),-math.sin(r)],[0,math.sin(r),math.cos(r)]])
    Ry = np.array([[math.cos(pw),0,math.sin(pw)],[0,1,0],[-math.sin(pw),0,math.cos(pw)]])
    v = (Ry @ Rx @ verts.T).T
    ax.cla(); ax.set_xlim(-3,3); ax.set_ylim(-3,3); ax.set_zlim(-3,3)
    ax.set_title(f'Roll={roll:.1f} Pitch={pitch:.1f}'); ax.set_xlabel('X'); ax.set_ylabel('Y')
    for i,j in edges: ax.plot([v[i,0],v[j,0]],[v[i,1],v[j,1]],[v[i,2],v[j,2]],'b-')
    plt.pause(0.02)
