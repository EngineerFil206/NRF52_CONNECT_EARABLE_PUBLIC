import time
import serial
import serial.tools.list_ports


BAUDRATE = 115200


def find_device():
    while True:
        for port in serial.tools.list_ports.comports():

            if "Bluetooth" in port.description:
                continue

            try:
                test = serial.Serial(port.device, BAUDRATE, timeout=0.2)
                test.close()
                return port.device
            except:
                pass

        time.sleep(0.5)


print("========================================")
print("Serial Monitor")
print("========================================")

while True:

    com = find_device()

    print(f"Connected to {com}")

    try:
        with serial.Serial(com, BAUDRATE, timeout=0.1) as ser:

            while True:

                data = ser.read(ser.in_waiting or 1)

                if data:
                    print(
                        data.decode("utf-8", errors="replace"),
                        end="",
                        flush=True,
                    )

    except (serial.SerialException, OSError):
        print("\nDisconnected. Reconnecting...\n")
        time.sleep(0.5)

    except KeyboardInterrupt:
        print("\nExited.")
        break