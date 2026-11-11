"""Contract tests for typed Python API talking to real C++ simulator."""

import pytest
from framework.device import Device, DeviceError, DeviceStatus, Reading


def test_typed_api_round_trip(device):
    assert device.ping() is True
    assert device.set_temperature(77) == "OK"
    assert device.set_calibration(3) == "OK"
    assert device.set_threshold(80) == "OK"
    assert device.read() == Reading(temperature=80.0, alarm=True)
    assert device.status() == DeviceStatus(fault="NONE", samples=1)


def test_typed_api_invalid_range_is_atomic(device):
    with pytest.raises(DeviceError, match="OUT_OF_RANGE"):
        device.set_temperature(126)
    assert device.read().temperature == 25


@pytest.mark.parametrize("value", [float("inf"), float("nan"), "123", True])
def test_typed_api_rejects_invalid_numeric_types(device, value):
    with pytest.raises(ValueError, match="finite number"):
        device.set_temperature(value)
    assert device.read().temperature == 25


def test_stuck_fault_and_recovery(device):
    device.set_temperature(70)
    device.set_fault("STUCK")
    assert device.status() == DeviceStatus(fault="STUCK", samples=0)
    assert device.read().temperature == 25
    device.set_fault("NONE")
    assert device.read().temperature == 70
    assert device.status().samples == 2


def test_disconnected_reads_do_not_increment_samples(device):
    device.set_fault("DISCONNECTED")
    with pytest.raises(DeviceError, match="DISCONNECTED"):
        device.read()
    assert device.status().samples == 0


def test_reset_restores_defaults(device):
    device.set_temperature(100)
    device.set_calibration(4)
    device.set_fault("STUCK")
    assert device.reset() == "OK"
    assert device.status() == DeviceStatus(fault="NONE", samples=0)
    assert device.read() == Reading(temperature=25.0, alarm=False)


def test_invalid_fault_mode(device):
    with pytest.raises(ValueError, match="unknown fault mode"):
        device.set_fault("BROKEN")
    assert device.ping()


def test_raw_command_for_malformed_wire_input(device):
    with pytest.raises(DeviceError, match="BAD_ARGUMENT"):
        device.command("SET nan")
    assert device.ping()


def test_typed_timeout_requires_restart(binary):
    with Device(binary, timeout=0.1) as device:
        device.set_fault("TIMEOUT")
        with pytest.raises(TimeoutError):
            device.read()
        with pytest.raises(DeviceError, match="restart"):
            device.status()
