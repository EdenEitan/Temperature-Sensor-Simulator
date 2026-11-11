# Manual validation log

I wrote the C++ sensor implementation and header myself. After using ChatGPT for the communication and driver code, I checked the integration through manual commands and a Python smoke test, alongside reviewing the code.

These notes preserve observations from development on Ubuntu/WSL. They are not a dated, complete test execution report; where the original notes only listed expected replies, that distinction is retained.

## Build and launch

```bash
cmake -S . -B build
cmake --build build
./build/device_sim
```

Rebuild after source changes before repeating a manual check.

## Routine operation

The original notes recorded a pass for startup, normal updates, alarm activation, invalid-input rejection, and reset. They also contained a malformed table row, so the original “9/9” total is not used here as verified evidence.

The following is a cleaned reproduction sequence with **expected** responses:

| Command | Expected response |
| --- | --- |
| `PING` | `PONG` |
| `STATUS` | `STATUS NONE 0` |
| `SET 80` | `OK` |
| `READ` | `DATA 80.00 ALARM` |
| `SET 200` | `ERR OUT_OF_RANGE` |
| `READ` | `DATA 80.00 ALARM` |
| `RESET` | `OK` |
| `STATUS` | `STATUS NONE 0` |
| `READ` | `DATA 25.00 NORMAL` |

The second read after the rejected update checks that valid state was preserved.

## Sensor fault scenarios

Run this sequence in a fresh process. These are expected responses from the original manual scenarios, not a separately captured output transcript.

| Command | Expected response |
| --- | --- |
| `FAULT STUCK` | `OK` |
| `SET 100` | `OK` |
| `READ` | `DATA 25.00 NORMAL` |
| `STATUS` | `STATUS STUCK 1` |
| `FAULT NONE` | `OK` |
| `READ` | `DATA 100.00 ALARM` |
| `FAULT DISCONNECTED` | `OK` |
| `READ` | `ERR DISCONNECTED` |
| `FAULT TIMEOUT` | `OK` |
| `READ` | No response |
| `STATUS` | `STATUS TIMEOUT 2` |
| `FAULT NONE` | `OK` |
| `PING` | `PONG` |

This manual sequence can continue after a silent `READ` because it uses the simulator directly. The Python driver has a stricter policy: after a response timeout, it requires a fresh process.

## Recorded issue: new fault rejected by an old binary

**Goal:** distinguish sensor-level failure from loss of communication with the whole device.

The first recorded attempt was:

```text
PING
PONG
FAULT COMMUNICATION_FAILURE
ERR BAD_ARGUMENT
```

**Cause recorded during development:** the updated source had not been rebuilt, so the executable did not recognize the new fault mode.

After rebuilding, the recorded exchange was:

```text
PING
PONG
FAULT COMMUNICATION_FAILURE
OK
PING
```

No reply followed the final `PING`. This established activation and silence for that command. The original transcript did not record all recovery steps, so the broader restart behavior is covered by the automated tests rather than inferred from this transcript.

Communication failure now belongs to `CommandProtocol`, following the extraction of parsing from `main.cpp`. It is separate from the sensor's fault enum.

**Takeaway:** check the build artifact as well as the source when observed behavior does not match a recent change.

## Recorded Python driver smoke test

Script: [scripts/try_driver.py](../scripts/try_driver.py).

The script starts the simulator, checks `PING`, sets raw 77 and offset 3, reads the result, inspects status, and closes the process.

Recorded output:

```text
PONG
Temperature: 80.0
Alarm: True
STATUS NONE 1
```

The original expected-output block used `PING: PONG`, but the script prints only `PONG`. The output above matches the script's actual formatting.

This verifies the basic communication path; automated fault, lifecycle, and parsing checks are documented in the [test plan](TEST_PLAN.md).
