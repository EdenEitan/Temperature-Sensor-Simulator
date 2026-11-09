# Temperature Sensor Simulator

A deterministic C++ temperature sensor simulator with a Python test driver and pytest integration tests. Designed to practice test automation, fault injection, process communication, and failure handling without physical hardware.

## Components

- `src/Sensor.h`, `src/Sensor.cpp` — sensor state, validation, calibration, alarm threshold, and simulated sensor faults.
- `src/main.cpp` — stdin/stdout transport loop; `src/Protocol.h` / `src/Protocol.cpp` — command parser and dispatcher.
- `framework/device.py` — typed Python sensor API plus raw command access, C++ process management, parsing, response timeouts, and cleanup.
- `scripts/try_driver.py` — manual Python-to-C++ integration smoke test.
- `tests/test_device.py` and `tests/test_driver_api.py` — pytest integration and typed-API tests.
- `tests/cpp/` — lightweight C++ unit suites for `Sensor` and `CommandProtocol`, registered with CTest.
- `.github/workflows/tests.yml` — Ubuntu CI running CTest and pytest for Python 3.11 and 3.12.
- `docs/` — design notes and manual validation journal.

## Requirements

Linux or WSL (Ubuntu recommended), a C++17 compiler, CMake 3.16+, Python 3, and pytest.

The driver's `selectors`-based pipe handling is designed for Linux/WSL; native Windows pipe support is not implemented.

## Build and run

From the repository root, in a **Linux / WSL terminal**:

```bash
cmake -S . -B build
cmake --build build
./build/device_sim
```

Try commands interactively:

```text
PING
PONG
SET 77
OK
CALIBRATE 3
OK
READ
DATA 80.00 ALARM
STATUS
STATUS NONE 1
```

Press Ctrl+D to exit the interactive simulator.

## Python driver

Build the C++ executable first, then run the smoke test from the repository root:

```bash
python3 -m scripts.try_driver
```

Expected output:

```text
PONG
Temperature: 80.0
Alarm: True
STATUS NONE 1
```

## Integration tests

Install pytest if necessary:

```bash
python3 -m pip install pytest
```

Then, from the repository root:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure  # C++ unit suites
python3 -m pytest -v                     # Python integration tests
```

The CI workflow runs this pipeline on pushes to `main`, pull requests, and manual dispatch, and uploads a pytest JUnit XML artifact per Python version.

The `binary` fixture in `tests/conftest.py` uses CMake to build `build/device_sim` once per test session. The `device` fixture starts a fresh simulator process per test.

## Command protocol

For the complete command contract, response/error formats, and typed Python methods, see [docs/PROTOCOL.md](docs/PROTOCOL.md).


Each request and response is one newline-terminated ASCII line.

| Command | Example reply | Description |
| --- | --- | --- |
| `PING` | `PONG` | Check responsiveness |
| `SET 77` | `OK` | Set raw temperature (–40 to 125 °C) |
| `CALIBRATE 3` | `OK` | Set offset (–10 to 10 °C) |
| `THRESHOLD 80` | `OK` | Set alarm threshold (–40 to 125 °C) |
| `READ` | `DATA 80.00 ALARM` | Return calibrated value and alarm flag |
| `STATUS` | `STATUS NONE 1` | Report fault and successful-read count |
| `FAULT DISCONNECTED` | `OK` | Make future reads return `ERR DISCONNECTED` |
| `FAULT STUCK` | `OK` | Make future reads return a fixed raw 25 °C (plus calibration) |
| `FAULT TIMEOUT` | `OK` | Make reads produce no response |
| `FAULT COMMUNICATION_FAILURE` | `OK` | Stop replying to all commands until process restart |
| `FAULT NONE` | `OK` | Clear sensor-level fault |
| `RESET` | `OK` | Restore default sensor state |

The alarm is active when `raw_temperature + calibration_offset >= threshold`. A successful read increments the sample counter; disconnected and timed-out reads do not.

Malformed requests return `ERR BAD_ARGUMENT` or `ERR UNKNOWN_COMMAND`. Valid numeric commands outside permitted ranges return `ERR OUT_OF_RANGE`.

After a response timeout, the Python driver treats the command stream as potentially desynchronized and requires restarting the process.

## Scope

This is a software testing simulator, not a calibrated physical sensor or an operating-system hardware driver. Thermal inertia and concurrent multi-client access are described in the development journal as possible future extensions, **not implemented features**.

## Status

The C++ command interface and Python driver are implemented. Automated tests include C++ unit coverage for the Sensor and command protocol, plus Python integration tests covering the raw and typed driver APIs. GitHub Actions builds and runs both CTest and pytest; see the Actions tab for run-specific results. The design and test coverage are summarized in [docs/TEST_PLAN.md](docs/TEST_PLAN.md).
