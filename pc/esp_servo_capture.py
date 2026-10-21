"""Quay bàn bằng ESP32-S3 và ghi telemetry STM32 kèm góc tham chiếu."""
import argparse
import csv
import struct
import time

import serial


FMT = '<I14fHHb2f'
PAYLOAD = struct.calcsize(FMT)
FIELDS = [
    'ts', 'axg', 'ayg', 'azg', 'gxdps', 'gydps', 'gzdps',
    'racc', 'pacc', 'rgyro', 'pgyro', 'rcf', 'pcf', 'rkf', 'pkf',
    'exec_us', 'dt_us', 'fw_ref', 'yawr', 'yawt'
]


def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def move_servo(port, angle):
    port.reset_input_buffer()
    port.write(f'{angle}\n'.encode())
    # Firmware moves 1 degree every 80 ms; a full -90 -> +90 sweep needs
    # about 14.4 s, so leave enough time for the confirmation line.
    deadline = time.monotonic() + 20.0
    reply = bytearray()
    marker = f'ANGLE {angle} PWM'.encode()
    while time.monotonic() < deadline:
        reply.extend(port.read(128))
        if marker in reply:
            return reply.decode(errors='replace').strip()
    raise RuntimeError(f'ESP khong xac nhan goc {angle}: {reply!r}')


def capture_frames(port, seconds):
    deadline = time.monotonic() + seconds
    buf = bytearray()
    rows = []
    bad = 0
    while time.monotonic() < deadline:
        buf.extend(port.read(512))
        while True:
            pos = buf.find(b'\xAA\x55')
            if pos < 0:
                buf[:] = buf[-1:]
                break
            del buf[:pos]
            if len(buf) < 3:
                break
            length = buf[2]
            needed = 3 + length + 1
            if len(buf) < needed:
                break
            frame = bytes(buf[:needed])
            del buf[:needed]
            if length != PAYLOAD or crc8(frame[2:3 + length]) != frame[3 + length]:
                bad += 1
                continue
            rows.append(struct.unpack(FMT, frame[3:3 + length]))
    return rows, bad


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--esp', default='/dev/ttyACM0')
    parser.add_argument('--stm', default='/dev/ttyUSB0')
    parser.add_argument('--out', required=True)
    parser.add_argument('--settle', type=float, default=2.0)
    parser.add_argument('--hold', type=float, default=3.0)
    parser.add_argument(
        '--sequence', default='-90,-60,-30,0,30,60,90,60,30,0,-30,-60,-90',
        help='Danh sach goc ESP, cach nhau boi dau phay',
    )
    args = parser.parse_args()

    sequence = [int(value.strip()) for value in args.sequence.split(',')]
    if not sequence or any(angle < -90 or angle > 90 for angle in sequence):
        parser.error('--sequence chi chap nhan cac goc -90..90 (0 la vi tri giua)')
    esp = serial.Serial(args.esp, 115200, timeout=0.05)
    stm = serial.Serial(args.stm, 115200, timeout=0.05)
    total = 0
    total_bad = 0
    try:
        with open(args.out, 'w', newline='') as output:
            writer = csv.writer(output)
            writer.writerow(FIELDS + ['esp_ref'])
            for angle in sequence:
                print(move_servo(esp, angle), flush=True)
                time.sleep(args.settle)
                stm.reset_input_buffer()
                rows, bad = capture_frames(stm, args.hold)
                for row in rows:
                    writer.writerow(row + (angle,))
                output.flush()
                total += len(rows)
                total_bad += bad
                print(f'  STM samples={len(rows)} bad={bad}', flush=True)
    finally:
        try:
            move_servo(esp, 0)
        except Exception:
            pass
        esp.close()
        stm.close()
    print(f'DONE samples={total} bad={total_bad} out={args.out}')


if __name__ == '__main__':
    main()
