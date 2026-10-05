"""Jitter + float/fixed + drift: doc 1 file CSV, in thong ke de paste bao cao.
Dung: python3 evaluate.py log.csv"""
import sys, csv
import numpy as np
path = sys.argv[1] if len(sys.argv) > 1 else '../docs/measurements/log.csv'
cols = {}
with open(path) as f:
    for row in csv.DictReader(f):
        for k, v in row.items():
            try: cols.setdefault(k.strip(), []).append(float(v))
            except ValueError: pass
def show(k):
    d = np.array(cols[k])
    print(f'{k:8s} mean={d.mean():9.3f} std={d.std():8.3f} min={d.min():9.2f} max={d.max():9.2f} n={len(d)}')
for k in ['racc','rgyro','rcf','rkf','rpacc','pgyro','pcf','pkf','exec_us','dt_us']:
    if k in cols: show(k)
if 'dt_us' in cols:
    d = np.array(cols['dt_us'])
    print(f'jitter: target 2000us, mean err={d.mean()-2000:+.1f}us, std={d.std():.1f}us, max-min={d.max()-d.min():.0f}us')
if 'rkf' in cols and 'rkfix' in cols:
    e = np.array(cols['rkf']) - np.array(cols['rkfix'])
    print(f'kalman float-fix roll: max|e|={np.abs(e).max():.4f}deg mean={e.mean():+.5f}')
if 'pkf' in cols and 'pkfix' in cols:
    e = np.array(cols['pkf']) - np.array(cols['pkfix'])
    print(f'kalman float-fix pitch: max|e|={np.abs(e).max():.4f}deg mean={e.mean():+.5f}')
for k in ['rgyro','pgyro']:
    if k in cols:
        g = np.array(cols[k]); secs = len(g) / 50
        print(f'{k} drift: {g[-1]-g[0]:+.3f}deg / {secs:.0f}s')
