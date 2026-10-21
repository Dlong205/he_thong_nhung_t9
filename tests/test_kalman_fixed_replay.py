"""Replay logged measurements through the actual C float and fixed filters."""
import csv
import ctypes as ct
import math
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA = [
    "drift5p.csv",
    "dynamic.csv",
    "roll_servo_wire_reference_final_2026-10-20.csv",
]
FIELDS = ("angle", "bias", "P00", "P01", "P10", "P11", "Q_angle", "Q_bias", "R_measure")


class KalmanFloat(ct.Structure):
    _fields_ = [(name, ct.c_float) for name in FIELDS]


class KalmanFixed(ct.Structure):
    _fields_ = [(name, ct.c_int32) for name in FIELDS]


def check_replay(lib, filename, axis):
    float_state, fixed_state = KalmanFloat(), KalmanFixed()
    lib.kalman_init(ct.byref(float_state))
    lib.kfix_init(ct.byref(fixed_state))
    errors = []
    with (ROOT / "docs" / "measurements" / filename).open(newline="") as source:
        for index, row in enumerate(csv.DictReader(source)):
            acc_key = "racc" if axis == "roll" else "pacc" if "pacc" in row else "rpacc"
            gyro_key = "gxdps" if axis == "roll" else "gydps"
            dt = min(max(float(row["dt_us"]) * 1e-6, 0.0005), 0.020)
            if filename == "drift5p.csv" and index % 500 == 0:
                dt = 0.020  # exercise the longest prediction step allowed in firmware
            args = (float(row[acc_key]), float(row[gyro_key]), dt)
            result_float = lib.kalman_update(ct.byref(float_state), *args)
            result_fixed = lib.kfix_update(ct.byref(fixed_state), *args)
            assert math.isfinite(result_fixed)
            assert fixed_state.P00 >= 0 and fixed_state.P11 >= 0
            errors.append(abs(result_float - result_fixed))
    assert errors and max(errors) < 0.15, (filename, axis, max(errors))
    return len(errors), max(errors)


def main():
    with tempfile.TemporaryDirectory() as directory:
        library = Path(directory) / "kalman_compare.so"
        subprocess.run(
            ["cc", "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror",
             "-I", str(ROOT / "Core" / "Inc"),
             str(ROOT / "Core" / "Src" / "kalman.c"),
             str(ROOT / "Core" / "Src" / "kalman_fix.c"),
             "-o", str(library)],
            check=True,
        )
        lib = ct.CDLL(str(library))
        lib.kalman_init.argtypes = [ct.POINTER(KalmanFloat)]
        lib.kfix_init.argtypes = [ct.POINTER(KalmanFixed)]
        for name, state in (("kalman_update", KalmanFloat), ("kfix_update", KalmanFixed)):
            function = getattr(lib, name)
            function.argtypes = [ct.POINTER(state), ct.c_float, ct.c_float, ct.c_float]
            function.restype = ct.c_float
        for filename in DATA:
            for axis in ("roll", "pitch"):
                count, worst = check_replay(lib, filename, axis)
                print(f"{filename} {axis}: {count} samples, max |float-fixed|={worst:.4f} deg")


if __name__ == "__main__":
    main()
