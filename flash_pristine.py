import subprocess
import sys
import time

import serial
import serial.tools.list_ports


def find_xiao_port():
    while True:
        for port in serial.tools.list_ports.comports():
            if "Bluetooth" not in port.description:
                return port.device
        time.sleep(0.25)


print("========================================")
print("Building...")
print("========================================")

result = subprocess.run(["west", "build", "-p", "always", "-b", "xiao_ble"])

if result.returncode != 0:
    sys.exit(result.returncode)

print()
print("Searching for XIAO...")

com = find_xiao_port()

print(f"Using {com}")

print()
print("Triggering bootloader...")

try:
    with serial.Serial(com, 115200, timeout=1) as ser:
        time.sleep(0.1)
        ser.write(b"BOOT67")
        ser.flush()

except Exception as e:
    print(e)
    sys.exit(1)

print("Waiting for bootloader...")
time.sleep(1.5)

print()
print("Flashing...")
print()

try:
    subprocess.run(
        ["west", "flash", "-r", "uf2"],
        check=True,
        stderr=subprocess.DEVNULL,
    )
except:
    pass

import myserial

