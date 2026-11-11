# Sensor command protocol (v1)

## Architecture

Python pytest -> `framework/device.py` (`Device`) -> stdin/stdout pipes -> `src/main.cpp` (I/O loop) -> `src/Protocol.h/.cpp` (parsing and command dispatch) -> `src/Sensor.h/.cpp` (domain model).

This is an ASCII, newline-delimited **command/response protocol**. 
Each command has one line; 
responses have one line except intentionally silenced fault scenarios.
The driver sends one request at a time.

## Command reference

| Command | Input | Response | Behavior |
| --- | --- | --- | --- |
| `PING` | no arguments | `PONG` | Connectivity check |
| `SET 77` | finite number, -40..125 | `OK` | Set raw temperature (C) |
| `CALIBRATE 3` | finite number, -10..10 | `OK` | Set calibration offset (C) |
| `THRESHOLD 80` | finite number, -40..125 | `OK` | Set alarm threshold (C) |
| `READ` | no arguments | `DATA 80.00 ALARM` or `DATA 79.00 NORMAL` | Read calibrated value and alarm state |
| `STATUS` | no arguments | `STATUS NONE 1` | Current fault and successful-read count |
| `FAULT STUCK` | `NONE`, `DISCONNECTED`, `STUCK`, `TIMEOUT`, `COMMUNICATION_FAILURE` | `OK` | Inject or clear a fault |
| `RESET` | no arguments | `OK` | Restore sensor defaults |

Initial state: raw temperature 25C, offset 0, threshold 80C, fault NONE, successful reads 0. Each successful READ increments the counter. Alarm uses the calibrated reading and triggers at **temperature >= threshold**. An invalid setter does not alter state. The calibrated result is not clamped to the raw-input range: raw -40 plus offset -10 yields -50, and raw 125 plus offset 10 yields 135. Alarm evaluation uses the unrounded value; the wire temperature is formatted to two decimal places.

## Errors, silence, and recovery

| Case | Wire response | Driver behavior |
| --- | --- | --- |
| Unknown command | `ERR UNKNOWN_COMMAND` | `DeviceError` |
| Missing/invalid/extra argument | `ERR BAD_ARGUMENT` | `DeviceError` |
| Valid number outside allowed range | `ERR OUT_OF_RANGE` | `DeviceError` |
| `FAULT DISCONNECTED` followed by `READ` | `ERR DISCONNECTED` | `DeviceError` |
| `FAULT STUCK` followed by `READ` | `DATA <25 + offset> ...` | Normal `Reading`, tests identify frozen readings |
| `FAULT TIMEOUT` followed by `READ` | **No response** | `TimeoutError` |
| `FAULT COMMUNICATION_FAILURE` | First returns `OK`, all later commands receive **no response** | `TimeoutError` |

Failed/timeout reads do not increase sample count. At the C++ protocol level, a sensor `TIMEOUT` suppresses only `READ`; `PING`, `STATUS`, and fault-clearing commands can still respond. `FAULT NONE` clears ordinary sensor faults when communication remains usable. `RESET` restores sensor state but **cannot restore communication** after communication failure. Once the driver encounters a timeout, it treats the stream as desynchronized and requires a fresh process: a late reply could otherwise be mistaken for the next command's reply.

## Typed Python API

| Method | Wire request | Returns |
| --- | --- | --- |
| `ping()` | `PING` | `True` |
| `set_temperature(77)` | `SET 77` | `"OK"` |
| `set_calibration(3)` | `CALIBRATE 3` | `"OK"` |
| `set_threshold(80)` | `THRESHOLD 80` | `"OK"` |
| `set_fault("STUCK")` | `FAULT STUCK` | `"OK"` |
| `read()` | `READ` | `Reading(temperature: float, alarm: bool)` |
| `status()` | `STATUS` | `DeviceStatus(fault: str, samples: int)` |
| `reset()` | `RESET` | `"OK"` |
| `command("SET nan")` | `SET nan` | Raw reply or `DeviceError` |

Numeric typed methods reject non-numeric, boolean, and non-finite values locally, but numeric **range** validation belongs to the C++ system under test. Use the raw `command()` method to inject malformed protocol requests and test server validation.

Example:

```python
from framework.device import Device

with Device("build/device_sim") as sensor:
    sensor.set_temperature(77)
    sensor.set_calibration(3)
    result = sensor.read()
    assert result.temperature == 80.0
    assert result.alarm
    assert sensor.status().samples == 1
```

Run `python3 -m pytest -q` from the repository root in Linux/WSL; the test fixture builds the executable unless `DEVICE_BINARY` overrides it. Native Windows pipe support is not implemented.

The API sends one command per transaction and rejects embedded CR/LF before writing. This is a client framing guard, not a server security boundary: the standalone executable intentionally accepts multiple newline-separated commands. `Device` is not designed for concurrent callers.
