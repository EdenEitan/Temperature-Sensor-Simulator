from framework.device import Device

with Device("./device_sim") as device:
    print(device.command("PING"))

    device.command("SET 77")
    device.command("CALIBRATE 3")

    reading = device.read()

    print("Temperature:", reading.temperature)
    print("Alarm:", reading.alarm)
    print(device.command("STATUS"))