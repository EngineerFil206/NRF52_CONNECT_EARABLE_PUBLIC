import time
import asyncio
import threading
import struct
import csv
from datetime import datetime

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
from bleak import BleakScanner, BleakClient
from PyQt6.QtGui import QGuiApplication

TARGET_NAME = "NRF52_EMG"
CHARACTERISTIC_UUID = "e9ea0002-e19b-482d-9293-c7907585fc48"

BUFFER_SIZE = 500
Y_LIMIT = 4095

# ==========================
# CSV
# ==========================

filename = datetime.now().strftime("results/%y-%m-%d-%H-%M-%S.csv")
csv_file = open(filename, "w", newline="")

csv_writer = csv.writer(csv_file)
csv_writer.writerow(["sample", "value"])

sample_number = 0

# ==========================
# Display Buffer
# ==========================

emg_data = deque([0] * BUFFER_SIZE, maxlen=BUFFER_SIZE)

# ==========================
# Statistics
# ==========================

notify_count = 0
notify_rate = 0.0

sample_count = 0
sample_rate = 0.0

last_print = time.perf_counter()

# ==========================
# BLE Callback
# ==========================

def handle_notify(sender, data):
    global notify_count
    global notify_rate
    global sample_count
    global sample_rate
    global last_print
    global sample_number

    try:
        # Decode 50 uint16 samples
        samples = struct.unpack("<50H", data)

        # Update graph
        emg_data.extend(samples)

        # Save samples to CSV
        for value in samples:
            sample_number += 1
            csv_writer.writerow([sample_number, value])

        csv_file.flush()

        notify_count += 1
        sample_count += len(samples)

        now = time.perf_counter()
        elapsed = now - last_print

        if elapsed >= 1.0:
            notify_rate = notify_count / elapsed
            sample_rate = sample_count / elapsed

            print(
                f"Notify: {notify_rate:.1f} Hz | "
                f"Samples: {sample_rate:.1f} Hz"
            )

            notify_count = 0
            sample_count = 0
            last_print = now

    except Exception as e:
        print("Notification decode error:", e)

# ==========================
# BLE
# ==========================

async def find_device():
    while True:
        try:
            print("Scanning for NRF52_EMG...")
            devices = await BleakScanner.discover(timeout=2)

            for d in devices:
                if d.name == TARGET_NAME:
                    print(f"Found {d.name} ({d.address})")
                    return d

            print("Device not found. Retrying...\n")

        except Exception as e:
            print("Scan error:", e)


async def ble_loop():
    while True:

        device = await find_device()

        try:
            print("Connecting...")

            async with BleakClient(device.address) as client:

                print("Connected!")

                await client.start_notify(
                    CHARACTERISTIC_UUID,
                    handle_notify
                )

                print("Notifications started.")

                while client.is_connected:
                    await asyncio.sleep(1)

        except Exception as e:
            print(f"Disconnected / Error: {e}")

        print("Reconnecting...\n")


def start_ble():
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(ble_loop())

# Start BLE thread
threading.Thread(
    target=start_ble,
    daemon=True
).start()

# ==========================
# Matplotlib
# ==========================

fig, ax = plt.subplots()

manager = plt.get_current_fig_manager()

screens = QGuiApplication.screens()

if len(screens) > 1:
    screen = screens[1]
else:
    screen = screens[0]

geom = screen.availableGeometry()

sizeX = 800
sizeY = 500

manager.window.setGeometry(
    geom.right() - sizeX - 250,
    geom.top() + 50,
    sizeX,
    sizeY
)

line, = ax.plot([], [], lw=2)

ax.set_xlim(0, BUFFER_SIZE)
ax.set_ylim(0, Y_LIMIT)

ax.set_title("Real-time EMG")
ax.set_xlabel("Samples")
ax.set_ylabel("Amplitude")

rate_text = ax.text(
    0.02,
    0.97,
    "",
    transform=ax.transAxes,
    fontsize=12,
    verticalalignment="top",
    bbox=dict(facecolor="white", alpha=0.8)
)

def update(frame):

    line.set_data(range(BUFFER_SIZE), emg_data)

    rate_text.set_text(
        f"Notify: {notify_rate:.1f} Hz\n"
        f"Samples: {sample_rate:.1f} Hz"
    )

    return line, rate_text

ani = animation.FuncAnimation(
    fig,
    update,
    interval=20,
    blit=True,
    cache_frame_data=False,
)

try:
    plt.tight_layout()
    plt.show()

finally:
    csv_file.close()