"""Giai ma goi telemetry BINARY tu STM32 -> ghi CSV (giong het dinh dang cu,
nen analysis.py / eval_ref.py / realtime_plot.py dung lai duoc nguyen).

Khung: AA 55 | LEN | TS(u32) | 14xf32 | EXEC(u16) | DT(u16) | rkfix(f32) | pkfix(f32) | ref(i8) | yawr(f32) | yawt(f32) | CRC8
Chay: python3 bin_logger.py --port /dev/ttyUSB0 --baud 115200 --out log.csv --sec 60
"""
import argparse, struct, time, sys, re
import serial

ap = argparse.ArgumentParser()
ap.add_argument('--port', default='/dev/ttyUSB0')
ap.add_argument('--baud', type=int, default=115200)
ap.add_argument('--out', default='../docs/measurements/log.csv')
ap.add_argument('--sec', type=int, default=0, help='0 = chay lien tuc, Ctrl+C de dung')
ap.add_argument('--cmd', default='', help='gui 1 lenh truoc khi log, vd MAGCAL hoac "BIN 0"')
a = ap.parse_args()

FMT = '<I14fHH2fb2f'
PAYLOAD = struct.calcsize(FMT)  # 81
HDR = 'ts,axg,ayg,azg,gxdps,gydps,gzdps,racc,rpacc,rgyro,pgyro,rcf,pcf,rkf,pkf,exec_us,dt_us,rkfix,pkfix,ref,yawr,yawt'
INT_FIELDS = {0, 15, 16, 19}  # ts, exec, dt, ref giu dang int

def crc8(d):
    crc = 0
    for b in d:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc

s = serial.Serial(a.port, a.baud, timeout=0.1)
print(f'Mo {a.port} @ {a.baud}, tim goi AA55 (payload {PAYLOAD}B)...')
if a.cmd:
    s.write((a.cmd + '\n').encode())
    print(f'Da gui lenh: {a.cmd}')
f = open(a.out, 'w')
f.write(HDR + '\n')
buf = bytearray()
n = bad = 0
t0 = time.time()
try:
    while True:
        chunk = s.read(512)
        if chunk:
            for m in re.finditer(rb'#[ -~]{3,}', bytes(chunk)):
                print('FW:', m.group(0).decode(errors='ignore'))
            buf += chunk
        while True:
            i = buf.find(b'\xAA\x55')
            if i < 0:
                buf[:] = buf[-1:]  # giu 1 byte phong sync bi cat doi
                break
            if i > 0:
                del buf[:i]
            if len(buf) < 3:
                break
            ln = buf[2]
            need = 3 + ln + 1
            if len(buf) < need:
                break
            frame = bytes(buf[:need])
            del buf[:need]
            if crc8(frame[2:3 + ln]) != frame[3 + ln] or ln != PAYLOAD:
                bad += 1
                continue
            vals = struct.unpack(FMT, frame[3:3 + ln])
            row = []
            for idx, v in enumerate(vals):
                row.append(str(v) if idx in INT_FIELDS else f'{v:.6g}')
            f.write(','.join(row) + '\n')
            f.flush()
            n += 1
            if n % 100 == 0:
                print(f'... {n} goi (bad={bad})')
        if a.sec and (time.time() - t0 > a.sec):
            break
except KeyboardInterrupt:
    pass
f.close()
print(f'Da luu {n} goi -> {a.out} (loi CRC/len: {bad})')
