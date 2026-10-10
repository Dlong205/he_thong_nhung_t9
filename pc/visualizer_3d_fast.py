"""3D solid: may bay khoi dac (Poly3DCollection) Roll/Pitch only, 20fps latest-only."""
import argparse, serial, math, time
import matplotlib.pyplot as plt
import numpy as np
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
ap = argparse.ArgumentParser()
ap.add_argument('--port', default='/dev/ttyACM0'); ap.add_argument('--baud', type=int, default=115200)
ap.add_argument('--fps', type=int, default=20)
a = ap.parse_args()
s = serial.Serial(a.port, a.baud, timeout=0.05)
s.reset_input_buffer()

def box(x0,x1,y0,y1,z0,z1):
    v = np.array([[x0,y0,z0],[x1,y0,z0],[x1,y1,z0],[x0,y1,z0],
                  [x0,y0,z1],[x1,y0,z1],[x1,y1,z1],[x0,y1,z1]], float)
    faces = [[v[0],v[1],v[2],v[3]],[v[4],v[5],v[6],v[7]],
             [[v[0],v[1],v[5],v[4]]],[[v[2],v[3],v[7],v[6]]],
             [[v[1],v[2],v[6],v[5]]],[[v[0],v[3],v[7],v[4]]]]
    # flatten one level
    out = []
    for f in faces:
        if len(f)==1 and isinstance(f[0],list): out.append(f[0])
        else: out.append(f)
    return v, out

# Than, canh, duoi ngang, duoi dung (khoi dac)
parts = []
parts.append(box(-2.2, 2.2, -0.25, 0.25, -0.25, 0.25))   # fuselage xam
parts.append(box(-0.1, 0.7, -2.2, 2.2, -0.04, 0.04))     # wing xanh
parts.append(box(-2.2,-1.5, -0.8, 0.8, -0.04, 0.04))     # tail ngang
parts.append(box(-2.2,-1.6, -0.05, 0.05, 0.0, 0.9))      # tail dung
face_colors = ['gray','royalblue','royalblue','forestgreen']

fig = plt.figure(figsize=(6,6)); ax = fig.add_subplot(111, projection='3d')
ax.set_xlim(-3.5,3.5); ax.set_ylim(-3.5,3.5); ax.set_zlim(-3.5,3.5)
ax.set_xlabel('X'); ax.set_ylabel('Y'); ax.view_init(elev=20, azim=-60)
polys = []
for (_, faces), c in zip(parts, face_colors):
    pc = Poly3DCollection(faces, facecolors=c, edgecolors='k', linewidths=0.5, alpha=0.9)
    ax.add_collection3d(pc); polys.append(pc)
# Mui do danh dau huong +X
nose_dot, = ax.plot([],[],[],'ro',ms=9)
gx, gy = np.meshgrid(np.linspace(-3,3,7), np.linspace(-3,3,7))
ax.plot_wireframe(gx, gy, np.full_like(gx,-1.6), color='gray', alpha=0.2, linewidth=0.5)
plt.ion(); plt.show()

roll = pitch = 0.0
period = 1.0 / a.fps
next_t = time.time()
while True:
    latest = None
    while s.in_waiting:
        try: latest = s.readline().decode(errors='ignore').strip()
        except Exception: break
    if latest is None:
        latest = s.readline().decode(errors='ignore').strip()
    if latest and not latest.startswith('#') and not latest.startswith('ts,'):
        p = latest.split(',')
        if len(p) >= 16 and p[0] and p[0][0].isdigit():
            try: roll, pitch = float(p[13]), float(p[14])
            except ValueError: pass
    if time.time() < next_t: continue
    next_t = time.time() + period
    r, pw = math.radians(roll), math.radians(pitch)
    Rx = np.array([[1,0,0],[0,math.cos(r),-math.sin(r)],[0,math.sin(r),math.cos(r)]])
    Ry = np.array([[math.cos(pw),0,math.sin(pw)],[0,1,0],[-math.sin(pw),0,math.cos(pw)]])
    R = Ry @ Rx  # yaw = 0
    for pc, (verts, _) in zip(polys, parts):
        v = (R @ verts.T).T
        # rebuild faces tu verts da xoay: dung index goc
        # (don gian: xoay tung face cu)
        pass
    # Dung lai faces tu verts xoay: biet index box nhu tren
    new_verts_list = [(R @ v.T).T for (v,_) in parts]
    for pc, v in zip(polys, new_verts_list):
        f = [[v[0],v[1],v[2],v[3]],[v[4],v[5],v[6],v[7]],
             [v[0],v[1],v[5],v[4]],[v[2],v[3],v[7],v[6]],
             [v[1],v[2],v[6],v[5]],[v[0],v[3],v[7],v[4]]]
        pc.set_verts(f)
    nose = (R @ np.array([[2.25,0,0.0]]).T).T
    nose_dot.set_data([nose[0,0]],[nose[0,1]]); nose_dot.set_3d_properties([nose[0,2]])
    ax.set_title(f'Roll={roll:.1f} Pitch={pitch:.1f} (solid, no yaw)')
    fig.canvas.draw_idle(); fig.canvas.flush_events()
