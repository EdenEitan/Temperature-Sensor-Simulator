"""End-to-end tests: Python driver -> pipes -> C++ sensor simulator."""

import time

import pytest

from framework.device import Device, DeviceError




def test_boot(device):
    assert device.command("PING") == "PONG"
    assert device.read().temperature == 25
    assert device.command("STATUS") == "STATUS NONE 1"


@pytest.mark.parametrize("value", [-40, -39.99, 0, 25, 124.99, 125])
def test_temperature_range(device, value):
    assert device.command(f"SET {value}") == "OK"
    assert device.read().temperature == pytest.approx(value, abs=0.005)


@pytest.mark.parametrize("command", [
    "SET -40.01", "SET 125.01",
    "CALIBRATE -10.01", "CALIBRATE 10.01",
    "THRESHOLD -40.01", "THRESHOLD 125.01",
])
def test_invalid_range_is_atomic(device, command):
    with pytest.raises(DeviceError, match="OUT_OF_RANGE"):
        device.command(command)
    reading = device.read()
    assert reading.temperature == 25
    assert not reading.alarm


@pytest.mark.parametrize("command", [
    "SET", "SET abc", "SET nan", "SET inf", "SET 1e999",
    "SET 1 trailing", "READ extra", "RESET extra", "FAULT",
    "FAULT INVALID", "FAULT NONE extra", "",
])
def test_malformed_command_recovery(device, command):
    with pytest.raises(DeviceError):
        device.command(command)
    assert device.command("PING") == "PONG"


def test_unknown_command_is_rejected_and_connection_recovers(device):
    """Unknown command names are different from bad arguments to known commands."""
    with pytest.raises(DeviceError, match="UNKNOWN_COMMAND"):
        device.command("BOGUS")
    assert device.command("PING") == "PONG"
    assert device.read().temperature == 25


@pytest.mark.parametrize("offset", [-10, 2.5, 10])
def test_calibration(device, offset):
    assert device.command(f"CALIBRATE {offset}") == "OK"
    assert device.read().temperature == 25 + offset


@pytest.mark.parametrize("temperature, alarm", [
    (79.99, False), (80, True), (80.01, True),
])
def test_alarm_boundary(device, temperature, alarm):
    device.command(f"SET {temperature}")
    assert device.read().alarm is alarm


def test_alarm_uses_calibrated_value(device):
    device.command("SET 77")
    device.command("CALIBRATE 3")
    assert device.read().alarm
    device.command("THRESHOLD 81")
    assert not device.read().alarm


def test_disconnect_recovery_and_counter(device):
    device.command("FAULT DISCONNECTED")
    with pytest.raises(DeviceError, match="DISCONNECTED"):
        device.read()
    assert device.command("STATUS") == "STATUS DISCONNECTED 0"
    device.command("FAULT NONE")
    assert device.read().temperature == 25
    assert device.command("STATUS") == "STATUS NONE 1"


def test_stuck_sensor_detected_by_stimulus(device):
    device.command("FAULT STUCK")
    readings = []
    for value in [20, 30, 40]:
        device.command(f"SET {value}")
        readings.append(device.read().temperature)
    assert readings == [25, 25, 25]
    device.command("FAULT NONE")
    assert device.read().temperature == 40


def test_reset_restores_all_state(device):
    for command in ["SET 90", "CALIBRATE 5", "THRESHOLD 10", "READ", "FAULT STUCK"]:
        device.command(command)
    device.command("RESET")
    assert device.command("STATUS") == "STATUS NONE 0"
    assert device.read().temperature == 25
    assert not device.read().alarm


def test_sensor_timeout_requires_restart(binary):
    with Device(binary, timeout=0.1) as instance:
        assert instance.command("FAULT TIMEOUT") == "OK"
        start = time.monotonic()
        with pytest.raises(TimeoutError):
            instance.read()
        assert time.monotonic() - start < 2
        with pytest.raises(DeviceError, match="restart"):
            instance.command("PING")

    with Device(binary) as fresh:
        assert fresh.command("PING") == "PONG"


@pytest.mark.parametrize("command", ["PING", "READ", "STATUS", "RESET"])
def test_communication_failure_silences_all_commands(binary, command):
    """A communication fault must silence even health checks, not only READ."""
    with Device(binary, timeout=0.15) as instance:
        assert instance.command("PING") == "PONG"
        assert instance.command("FAULT COMMUNICATION_FAILURE") == "OK"
        with pytest.raises(TimeoutError):
            instance.command(command)
        with pytest.raises(DeviceError, match="restart"):
            instance.command("PING")

    # Restarting the simulator restores its ability to respond.
    with Device(binary) as fresh:
        assert fresh.command("PING") == "PONG"


def test_dead_process_is_reported(device):
    device.process.kill()
    device.process.wait(timeout=1)
    with pytest.raises(DeviceError, match="exited"):
        device.read()


def test_command_injection_rejected(device):
    with pytest.raises(ValueError, match="one command"):
        device.command("SET 50\nRESET")
    assert device.read().temperature == 25


def test_long_sequence(device):
    for i in range(200):
        value = i % 166 - 40
        assert device.command(f"SET {value}") == "OK"
        assert device.read().temperature == value
    assert device.command("STATUS") == "STATUS NONE 200"


def test_cleanup(binary):
    with Device(binary) as instance:
        assert instance.command("PING") == "PONG"
    assert instance.process.poll() is not None
