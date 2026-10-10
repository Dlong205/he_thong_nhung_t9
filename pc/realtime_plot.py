"""Realtime plot 4 pp Roll: racc/rgyro/rcf/rkf. Chay: python3 realtime_plot.py --port /dev/ttyACM0"""
import argparse, serial, collections
import matplotlib.pyplot as plt
import matplotlib.animation as animation
ap = argparse.ArgumentParser()
ap.add_argument('--port', default='/dev/ttyACM0')
ap.add_argument('--baud', type=int, default=115200)
ap.add_argument('--n', type=int, default=300)
a = ap.parse_args()
IDX = {'racc':7,'rpacc':8,'rgyro':9,'pgyro':10,'rcf':11,'pcf':12,'rkf':13,'pkf':14}
bufs = {k: collections.deque([0]*a.n, maxlen=a.n) for k in ['racc','rgyro','rcf','rkf']}
s = serial.Serial(a.port, a.baud, timeout=1)
fig, ax = plt.subplots()
lines = {k: ax.plot(list(bufs[k]), label=k)[0] for k in bufs}
ax.legend(loc='upper right'); ax.set_title('Roll: acc/gyro/cf/kalman'); ax.set_xlabel('sample@50Hz')
def update(_):
    for _ in range(5):
        line = s.readline().decode(errors='ignore').strip()
        if not line or line.startswith('#') or line.startswith('ts,'): continue
        p = line.split(',')
        if len(p) < 16: continue
        try:
            bufs['racc'].append(float(p[IDX['racc']])); bufs['rgyro'].append(float(p[IDX['rgyro']]))
            bufs['rcf'].append(float(p[IDX['rcf']])); bufs['rkf'].append(float(p[IDX['rkf']]))
        except ValueError: pass
    for k in bufs: lines[k].set_ydata(list(bufs[k]))
    ax.relim(); ax.autoscale_view()
    return list(lines.values())
ani = animation.FuncAnimation(fig, update, interval=100)
plt.show()
