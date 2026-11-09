from pathlib import Path
from framework.device import Device


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "device_sim"

with Device(BINARY) as device:
    print(device.command("PING"))

    device.command("SET 77")
    device.command("CALIBRATE 3")

    reading = device.read()

    print("Temperature:", reading.temperature)
    print("Alarm:", reading.alarm)
    print(device.command("STATUS"))