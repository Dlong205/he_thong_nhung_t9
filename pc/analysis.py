"""Offline analysis: doc CSV logger, in mean/std/RMSE vs gia tri trung binh (static test) + exec_us."""
import sys, csv, math
import numpy as np
path = sys.argv[1] if len(sys.argv) > 1 else '../docs/measurements/log.csv'
cols = {}
with open(path) as f:
    r = csv.DictReader(f)
    for row in r:
        for k, v in row.items():
            cols.setdefault(k.strip(), []).append(float(v))
for k in ['racc','rgyro','rcf','rkf','rpacc','pgyro','pcf','pkf','exec_us']:
    if k in cols:
        d = np.array(cols[k])
        print(f'{k:8s} mean={d.mean():8.3f} std={d.std():7.3f} min={d.min():8.2f} max={d.max():8.2f} n={len(d)}')
# Drift gyro-only: do lech cuoi-dau
if 'rgyro' in cols:
    g = np.array(cols['rgyro']); print(f'gyro drift roll: {g[-1]-g[0]:.3f} deg / {len(g)/50:.0f}s (~50Hz log)')
