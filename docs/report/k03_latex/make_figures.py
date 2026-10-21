"""Generate report figures from the repository's measured CSV files.

Run with Python 3, matplotlib and numpy installed. The script never
modifies the input measurements.
"""
from pathlib import Path
import csv

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

BASE = Path(__file__).resolve().parent
DATA = BASE / "data"
OUT = Path(__file__).resolve().parent / "figures"
OUT.mkdir(exist_ok=True)

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 12,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.alpha": 0.22,
    "figure.dpi": 150,
})


def read(name):
    with (DATA / name).open(newline="") as f:
        rows = list(csv.DictReader(f))
    return {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}


d = read("drift5p.csv")
t = (d["ts"] - d["ts"][0])/1000
fig, ax = plt.subplots(figsize=(10.6, 4.2))
for key, label, color in [("rgyro","Tích phân gyro","#cf554c"),
                          ("rcf","Bộ lọc bù","#15876d"),
                          ("rkf","Kalman","#3a5fb5")]:
    ax.plot(t,d[key],label=label,color=color,lw=1.15)
ax.set(xlabel="Thời gian (s)",ylabel="Roll (độ)",title="Phép đo trôi 330 s (firmware trước khi chuyển driver)")
ax.legend(ncol=3,loc="upper left"); fig.tight_layout()
fig.savefig(OUT / "drift_330s.pdf"); plt.close(fig)

d = read("dynamic.csv")
t = (d["ts"] - d["ts"][0])/1000
fig, ax = plt.subplots(figsize=(10.6, 4.4))
for key, label, color in [("racc","Gia tốc","#d08b29"),
                          ("rgyro","Tích phân gyro","#cf554c"),
                          ("rcf","Bộ lọc bù","#15876d"),
                          ("rkf","Kalman","#3a5fb5")]:
    ax.plot(t,d[key],label=label,color=color,lw=.95,alpha=.95)
ax.set(xlabel="Thời gian (s)",ylabel="Roll (độ)",title="Bốn phương pháp trong cùng phép xoay tay")
ax.legend(ncol=2,loc="upper left"); fig.tight_layout()
fig.savefig(OUT / "bon_phuong_phap_dong.pdf"); plt.close(fig)

d = read("vibration.csv")
t = (d["ts"] - d["ts"][0])/1000
fig, ax = plt.subplots(figsize=(10.6, 4.2))
for key, label, color in [("racc","Gia tốc","#d08b29"),
                          ("rcf","Bộ lọc bù","#15876d"),
                          ("rkf","Kalman","#3a5fb5")]:
    ax.plot(t,d[key],label=label,color=color,lw=.9,alpha=.9)
ax.axvspan(10,20,color="#dce8f5",alpha=.55,label="Đoạn đánh giá rung")
ax.set(xlabel="Thời gian (s)",ylabel="Roll (độ)",title="Ảnh hưởng của rung lên góc ước lượng")
ax.legend(ncol=2,loc="upper left"); fig.tight_layout()
fig.savefig(OUT / "rung.pdf"); plt.close(fig)

print("Generated", len(list(OUT.glob("*.pdf"))), "figures in", OUT)
