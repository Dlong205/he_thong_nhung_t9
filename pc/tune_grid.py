"""Tune alpha/Q/R offline bang replay file CSV tinh (giu yen) + dong (xoay).
Tieu chi: static RMSE(CF/KF vs mean acc) + do muot std. Chay:
  python3 tune_grid.py static.csv [dynamic.csv]
Ra bang de paste bao cao, khong can nap lai firmware tung lan."""
import sys, csv, math
import numpy as np

def load(p):
    d = {}
    with open(p) as f:
        for row in csv.DictReader(f):
            for k, v in row.items():
                try: d.setdefault(k.strip(), []).append(float(v))
                except ValueError: pass
    return {k: np.array(v) for k, v in d.items()}

def cf_replay(racc, gyro, alpha, dt=0.002):
    th = 0.0; out = np.zeros_like(racc)
    for i in range(len(racc)):
        th = alpha * (th + gyro[i] * dt) + (1 - alpha) * racc[i]
        out[i] = th
    return out

def kf_replay(racc, gyro, Qa, Qb, R, dt=0.002):
    ang, bias = 0.0, 0.0
    P00 = P01 = P10 = P11 = 0.0
    out = np.zeros_like(racc)
    for i in range(len(racc)):
        rate = gyro[i] - bias
        ang += dt * rate
        P00 += dt * (dt * P11 - P01 - P10 + Qa)
        P01 -= dt * P11; P10 -= dt * P11; P11 += Qb * dt
        y = racc[i] - ang
        S = P00 + R
        K0, K1 = P00 / S, P10 / S
        ang += K0 * y; bias += K1 * y
        P00t, P01t = P00, P01
        P00 -= K0 * P00t; P01 -= K0 * P01t; P10 -= K1 * P00t; P11 -= K1 * P01t
        out[i] = ang
    return out

static = load(sys.argv[1]) if len(sys.argv) > 1 else None
if static is None:
    print('Dung: python3 tune_grid.py static.csv [dynamic.csv]'); sys.exit(1)
racc, gx = static['racc'], static['gxdps']
WARM = min(100, len(racc) // 4)  # bo qua transient khoi tao
racc_w, gx_w = racc[WARM:], gx[WARM:]
ref = np.mean(racc_w)
print(f'--- CF alpha sweep (static, ref roll_acc mean={ref:.3f}, bo {WARM} mau dau) ---')
print('alpha, RMSE, std')
for alpha in [0.90, 0.95, 0.97, 0.98, 0.99, 0.995]:
    e = cf_replay(racc_w, gx_w, alpha)[WARM // 2:]
    print(f'{alpha:.3f}, {np.sqrt(np.mean((e-ref)**2)):.4f}, {e.std():.4f}')
print('--- Kalman Q/R sweep ---')
print('Qa, Qb, R, RMSE, std')
for Qa, Qb, R in [(0.0005,0.001,0.03),(0.001,0.003,0.03),(0.002,0.005,0.03),
                  (0.001,0.003,0.01),(0.001,0.003,0.1)]:
    e = kf_replay(racc_w, gx_w, Qa, Qb, R)[WARM // 2:]
    print(f'{Qa}, {Qb}, {R}, {np.sqrt(np.mean((e-ref)**2)):.4f}, {e.std():.4f}')
print('\nChon bo RMSE thap + std thap, uu tien on dinh. Paste bo chon vao cf_init/kalman_init.')
if len(sys.argv) > 2:
    print('\n(Goi y: chay them file dynamic de xem dap ung step 0->30->60 do, tranh chon alpha qua cao gay tre.)')
