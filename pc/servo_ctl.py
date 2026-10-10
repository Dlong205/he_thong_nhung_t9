"""Dieu khien servo SG90 tuong tac qua UART (ket noi PA1 - TIM2_CH2 PWM 50Hz).

Chay:  python3 pc/servo_ctl.py [--port /dev/ttyUSB0] [--baud 115200]
Go:
  30            -> quay +30 do
  0             -> ve giua
  +5 / -5       -> nhich tuong doi tung do (can chinh co khi)
  seq static    -> sweep -60..+60 tu dong (70s)
  seq step      -> 0->30->60->0 (20s)
  seq drift     -> giu 0 do 330s
  seq stop      -> dung sequence
  magcal        -> hieu chuan la ban (xoay so 8 15s)
  bin 0 / bin 1 -> tat/bat stream goi binary
  oled          -> trang thai nhanh
  q             -> thoat
"""
import argparse, re, sys, threading
import serial

ap = argparse.ArgumentParser()
ap.add_argument('--port', default='/dev/ttyUSB0')
ap.add_argument('--baud', type=int, default=115200)
a = ap.parse_args()

s = serial.Serial(a.port, a.baud, timeout=0.2)
stop = False

def reader():
    buf = b''
    while not stop:
        try:
            d = s.read(256)
        except Exception:
            break
        if not d:
            continue
        buf += d
        for m in re.finditer(rb'#[ -~]{3,}', buf):
            print('\nFW:', m.group(0).decode(errors='ignore'))
        buf = buf[-64:]

threading.Thread(target=reader, daemon=True).start()

cur = 0.0
print(f'Mo {a.port} @ {a.baud}. Go goc (vd 30), +/- de nhich, "seq static", "magcal", "bin 0", "q" thoat.')

try:
    while True:
        try:
            line = input(f'[{cur:+.0f} deg] > ').strip()
        except (EOFError, KeyboardInterrupt):
            break
        if not line:
            continue
        low = line.lower()
        if low in ('q', 'quit', 'exit'):
            break
        if low.startswith(('seq', 'magcal', 'bin', 'oled')):
            s.write((line + '\n').encode())
            print('->', line)
            continue
        try:
            if line[0] in '+-' and len(line) > 1 and all(c.isdigit() or c == '.' for c in line[1:]):
                cur += float(line)          # nhich tuong doi
            else:
                cur = float(line)           # goc tuyet doi
        except ValueError:
            print('Khong hieu. Go so (30), +/-5, seq static, magcal, bin 0, q')
            continue
        cur = max(-90.0, min(90.0, cur))
        s.write(f'REF {cur:.1f}\n'.encode())
        print(f'-> REF {cur:.1f}')
finally:
    stop = True
    s.close()
    print('Thoat.')
