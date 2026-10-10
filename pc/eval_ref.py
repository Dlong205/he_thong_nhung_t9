"""Đánh giá bàn đối chứng K03: so 4 PP (acc/gyro/cf/kalman-roll) với góc servo ref.
Dùng: python3 eval_ref.py log.csv [--plot out.png] [--settle 2.0]
- log.csv phải có cột ref (firmware mới). File cũ không có ref vẫn chạy được jitter/float-fix.
- Chỉ số: MAE/RMSE/bias/std/peak per-PP, drift gyro, latency step (tương quan chéo).
- --settle: bỏ N giây đầu mỗi nấc ref (qua độ) khi tính static.
"""
import sys, csv, argparse
import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument('csv', nargs='?', default='../docs/measurements/log.csv')
ap.add_argument('--plot', default='')
ap.add_argument('--settle', type=float, default=2.0, help='bỏ giây đầu mỗi nấc')
a = ap.parse_args()

cols = {}
with open(a.csv) as f:
    for row in csv.DictReader(f):
        for k, v in row.items():
            try: cols.setdefault(k.strip(), []).append(float(v))
            except ValueError: pass
for k in cols: cols[k] = np.array(cols[k])
n = len(next(iter(cols.values())))
print(f'File: {a.csv}, n={n} mau (~{n/50:.0f}s @50Hz)')

def stats(name, e):
    print(f'{name:6s} MAE={np.mean(np.abs(e)):7.3f} RMSE={np.sqrt(np.mean(e**2)):7.3f} '
          f'bias={np.mean(e):+7.3f} std={np.std(e):6.3f} peak={np.max(np.abs(e)):7.3f} deg')

FS = 50.0  # telemetry 50Hz
has_ref = 'ref' in cols
if has_ref:
    ref = cols['ref']
    # Tìm đoạn ref 거의 hằng (loại trừ lúc servo đang quay ~0.5s): nhóm theo giá trị làm tròn
    # Đơn giản: bỏ settle giây đầu sau mỗi lần ref đổi
    keep = np.ones(n, dtype=bool)
    last_change = 0
    for i in range(1, n):
        if abs(ref[i] - ref[i-1]) > 0.5:
            last_change = i
        if (i - last_change) / FS < a.settle:
            keep[i] = False
    # Bỏ thêm 100 mẫu đầu (transient Kalman/CF)
    keep[:100] = False
    print(f'Static (bỏ transient + {a.settle}s sau mỗi lần đổi ref, giữ {keep.sum()}/{n} mẫu):')
    for key, label in [('racc','acc'), ('rgyro','gyro'), ('rcf','cf'), ('rkf','kf')]:
        if key in cols:
            e = cols[key][keep] - ref[keep]
            stats(label, e)
    # Drift gyro trong lúc ref=0 (nếu có SEQ DRIFT): lấy đoạn ref==0 dài nhất
    z = np.where(np.abs(ref) < 0.5)[0]
    if len(z) > 100:
        g = cols['rgyro'][z] if 'rgyro' in cols else None
        if g is not None:
            print(f'Góc ref=0 dài {len(z)/FS:.0f}s: gyro drift {g[-1]-g[0]:+.2f}deg, '
                  f'cf span {np.ptp(cols["rcf"][z]):.2f}, kf span {np.ptp(cols["rkf"][z]):.2f}')
    # Latency step: tương quan chéo ref vs cf/kf trên toàn file (ước lượng trễ mẫu)
    if 'rkf' in cols:
        r = ref - ref.mean()
        for key in ['racc', 'rcf', 'rkf']:
            x = cols[key] - cols[key].mean()
            if np.std(x) < 1e-6 or np.std(r) < 1e-6:
                print(f'{key} latency: n/a (file tĩnh, std~0)'); continue
            corr = np.correlate((x - x.mean()) / (np.std(x) + 1e-9),
                                (r - r.mean()) / (np.std(r) + 1e-9), mode='full')
            lag = np.argmax(corr) - (n - 1)
            print(f'{key} latency ~ {lag/FS:+.2f}s ({lag:+d} mẫu @50Hz, + là ước lượng trễ hơn ref)')
else:
    print('KHÔNG có cột ref (file cũ / chưa gắn servo). Chạy jitter + float/fix + drift tương đối:')
    for k in ['racc', 'rgyro', 'rcf', 'rkf', 'exec_us', 'dt_us']:
        if k in cols:
            d = cols[k]
            print(f'{k:8s} mean={d.mean():9.3f} std={d.std():8.3f} min={d.min():9.2f} max={d.max():9.2f}')
    print('Muốn có MAE/RMSE vs ground-truth: nạp firmware mới (có servo_ref), gắn SG90 PA1, '
          'gõ SEQ STATIC rồi chạy lại serial_logger.')

if a.plot:
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        t = np.arange(n) / FS
        plt.figure(figsize=(10, 5))
        for k, lb in [('ref', 'REF servo'), ('racc', 'acc'), ('rgyro', 'gyro'), ('rcf', 'CF'), ('rkf', 'Kalman')]:
            if k in cols: plt.plot(t, cols[k], label=lb, linewidth=1 if k != 'ref' else 2)
        plt.xlabel('s'); plt.ylabel('deg'); plt.legend(ncol=5); plt.grid(True, alpha=0.3)
        plt.title('Ban doi chung: 4 PP vs REF')
        plt.tight_layout(); plt.savefig(a.plot, dpi=120)
        print(f'Đã vẽ -> {a.plot}')
    except ImportError:
        print('Chưa cài matplotlib: pip install matplotlib để vẽ.')
