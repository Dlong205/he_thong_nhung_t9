"""Acc 6-mat: moi mat log 1 file CSV (giu yen 8s), roi chay script nay.
VD: python3 acc_calib.py px.csv nx.csv py.csv ny.csv pz.csv nz.csv
Quy uoc: file px = +X huong len (ax~+1g), nx = -X len, tuong tu Y Z.
Ra: offset/scale de paste vao AccCal_t trong main.cpp."""
import sys, csv
import numpy as np
files = sys.argv[1:]
if len(files) != 6:
    print('Dung: python3 acc_calib.py px.csv nx.csv py.csv ny.csv pz.csv nz.csv'); sys.exit(1)
def mean_ax(f):
    ax = []
    with open(f) as fh:
        for row in csv.DictReader(fh):
            try: ax.append((float(row['axg']), float(row['ayg']), float(row['azg'])))
            except (KeyError, ValueError): pass
    return np.mean(np.array(ax), axis=0)
m = np.array([mean_ax(f) for f in files])  # px,nx,py,ny,pz,nz
for name, v in zip(['+X','-X','+Y','-Y','+Z','-Z'], m):
    print(f'{name}: ax={v[0]:+.4f} ay={v[1]:+.4f} az={v[2]:+.4f} |A|={np.linalg.norm(v):.4f}')
# Truc X dung px/nx, tuong tu
ox = (m[0,0] + m[1,0]) / 2; sx = 2 / (m[0,0] - m[1,0])
oy = (m[2,1] + m[3,1]) / 2; sy = 2 / (m[2,1] - m[3,1])
oz = (m[4,2] + m[5,2]) / 2; sz = 2 / (m[4,2] - m[5,2])
print(f'\nPaste vao code:\nAccCal_t accal = {{{ox:.5f}, {oy:.5f}, {oz:.5f}, {sx:.5f}, {sy:.5f}, {sz:.5f}}};')
print('Kiem tra: offset ~+-0.05g la dep, scale ~0.95-1.05. |A| moi mat phai ~1.00g.');
