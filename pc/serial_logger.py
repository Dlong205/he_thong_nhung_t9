"""Logger CSV tu STM32 baremetal Cube+REG qua USART1 PA9/PA10 -> CH340.
Chay: python3 serial_logger.py --port /dev/ttyUSB0 --baud 460800 --out ../docs/measurements/log.csv --sec 80"""
import argparse, serial, time, sys
ap = argparse.ArgumentParser()
ap.add_argument('--port', default='/dev/ttyUSB0')
ap.add_argument('--baud', type=int, default=460800)
ap.add_argument('--out', default='../docs/measurements/log.csv')
ap.add_argument('--sec', type=int, default=0, help='0 = chay lien tuc')
a = ap.parse_args()
s = serial.Serial(a.port, a.baud, timeout=1)
print(f'Open {a.port}, bo dong # ...')
fout = open(a.out, 'w')
header_written = False
n = 0
_DEFAULT_HDR = 'ts,axg,ayg,azg,gxdps,gydps,gzdps,racc,rpacc,rgyro,pgyro,rcf,pcf,rkf,pkf,exec_us,dt_us,rkfix,pkfix,ref,yawr,yawt'
t0 = time.time()
try:
    while True:
        line = s.readline().decode(errors='ignore').strip()
        if not line: continue
        if line.startswith('#'):
            print(line); continue
        if line.startswith('ts,'):
            fout.write(line + '\n'); header_written = True; print('HEADER:', line); continue
        if line[0].isdigit() and line.count(',') >= 15:
            if not header_written:
                fout.write(_DEFAULT_HDR + '\n'); header_written = True
            fout.write(line + '\n'); fout.flush()
            n += 1
            if n % 100 == 0: print(f'... {n} mau')
        if a.sec and (time.time() - t0 > a.sec): break
except KeyboardInterrupt:
    pass
fout.close()
print(f'Da luu -> {a.out}')
