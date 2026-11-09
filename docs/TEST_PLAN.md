# Test Design Workflow

# Test Plan: Temperature-Sensor-Simulator

**Document status:** Updated for the current `main` implementation (2026-11-09).  
**Test type:** C++ unit tests, Python-to-C++ integration tests, and typed-driver contract testing.  
**Source of truth:** `src/`, `framework/device.py`, `tests/`, and [PROTOCOL.md](PROTOCOL.md).  
**Important:** The inventory below is based on test source. GitHub Actions runs CTest and pytest independently; check the actual workflow run for results.

## 1. Scope and Requirements

### Purpose

Validate the functional correctness, input validation, fault handling, and reliability of a C++ temperature sensor simulator using an automated Python/pytest integration-testing framework.

The primary **System Under Test (SUT)** is the C++ simulator. The Python `Device` driver is the test-facing abstraction and is also tested for command translation, response parsing, timeouts, lifecycle management, and recovery constraints.

### Current Architecture

```text
pytest tests
   |
   v
framework/device.py (Device: typed API + raw command())
   |  subprocess / stdin and stdout pipes
   v
src/main.cpp (I/O loop)
   |
   v
src/Protocol.h + src/Protocol.cpp (CommandProtocol: parse and dispatch)
   |
   v
src/Sensor.h + src/Sensor.cpp (state and sensor domain logic)
```

The new `CommandProtocol` layer separates **transport** from **command interpretation**; the typed Python driver separates **test intent** from **wire syntax**. Existing raw commands remain backward compatible.

### In Scope

1. **Sensor Logic (`src/Sensor.h`, `src/Sensor.cpp`)**
   - Raw temperature: finite values from -40 to 125 degrees C.
   - Calibration offset: -10 to 10 degrees C; threshold: -40 to 125 degrees C.
   - Alarm when calibrated temperature is **greater than or equal to** the threshold.
   - Sensor faults (`NONE`, `DISCONNECTED`, `STUCK`, `TIMEOUT`), recovery, reset, and successful-read count.
   - Atomic rejection of invalid numerical setters (no change to the relevant sensor value).

2. **C++ Command Protocol (`src/Protocol.h`, `src/Protocol.cpp`)**
   - Parsing, validation, routing, response formatting, and error codes.
   - Malformed or unsupported requests and extra/missing arguments.
   - Communication failure injection (`COMMUNICATION_FAILURE`), which silences all later replies.
   - Maintaining command behavior after moving parsing out of `main.cpp`.

3. **C++ Transport (`src/main.cpp`)**
   - One ASCII request line at a time from stdin; one reply line to stdout unless intentionally silent.
   - Flushing returned responses while keeping transport independent of domain logic.

4. **Python Driver (`framework/device.py`)**
   - Typed methods: `ping()`, `set_temperature()`, `set_calibration()`, `set_threshold()`, `set_fault()`, `read()`, `status()`, and `reset()`.
   - Raw `command()` for protocol-negative testing; `close()` and context-manager support.
   - Parsing `Reading(temperature, alarm)` and `DeviceStatus(fault, samples)`.
   - Client-side finite-number/type validation; server-side range validation.
   - Process startup, timeout detection, desynchronization protection, process termination, and resource cleanup.

5. **Unit, Integration, and Regression Testing (`tests/`)**
   - `tests/cpp/test_sensor.cpp`: standalone Sensor unit-test executable (11 test cases).
   - `tests/cpp/test_protocol.cpp`: standalone CommandProtocol unit-test executable (13 test cases).
   - Real Python-to-C++ subprocess communication (no sensor mock).
   - Both legacy raw-command tests and typed-API contract tests.
   - CMake/CTest and GitHub Actions for automated execution.

### Out of Scope

- Real sensor hardware, operating-system hardware drivers, and hardware-in-the-loop testing.
- Concurrent multi-client access or thread-safety guarantees.
- Production-scale performance/load testing, thermal inertia, and persistent telemetry.
- Native Windows pipe support (the current selector-based driver targets Linux/WSL).
- Exhaustive fuzzing and complete C++ branch-coverage measurement.

### Interfaces (Inputs, Outputs, APIs, Events)

**Wire protocol:** newline-terminated ASCII commands over subprocess stdin/stdout pipes.

| Command | Input / Example | Expected reply |
| --- | --- | --- |
| `PING` | No argument | `PONG` |
| `SET` | `SET 77` | `OK` |
| `CALIBRATE` | `CALIBRATE 3` | `OK` |
| `THRESHOLD` | `THRESHOLD 80` | `OK` |
| `READ` | No argument | `DATA 80.00 ALARM` or `DATA 79.00 NORMAL` |
| `STATUS` | No argument | `STATUS NONE 1` (fault and successful-read count) |
| `FAULT` | `FAULT STUCK`, `FAULT NONE`, etc. | `OK` |
| `RESET` | No argument | `OK` |

**Failure responses:** `ERR BAD_ARGUMENT`, `ERR OUT_OF_RANGE`, `ERR UNKNOWN_COMMAND`, `ERR DISCONNECTED`.

**Silent behavior:** `FAULT TIMEOUT` suppresses responses to `READ`; `FAULT COMMUNICATION_FAILURE` returns `OK` once, then suppresses all subsequent responses. These result in a Python `TimeoutError`, after which the driver requires a new process.

**Python API contract:**

| Method | Delegated wire command | Python result |
| --- | --- | --- |
| `command("SET 77")` | `SET 77` | `"OK"` or `DeviceError` |
| `set_temperature(77)` | `SET 77` | `"OK"` |
| `set_calibration(3)` | `CALIBRATE 3` | `"OK"` |
| `set_threshold(80)` | `THRESHOLD 80` | `"OK"` |
| `set_fault("STUCK")` | `FAULT STUCK` | `"OK"` |
| `ping()` | `PING` | `True` |
| `read()` | `READ` | `Reading(temperature: float, alarm: bool)` |
| `status()` | `STATUS` | `DeviceStatus(fault: str, samples: int)` |
| `reset()` | `RESET` | `"OK"` |

The typed methods do not replace raw `command()`: direct wire access is deliberately retained for malformed-input and protocol tests.

## 2. Test Scenarios

**Status legend:** **Implemented** means an automated test exists in the repository, not that the latest run has been independently verified.

| ID | Scenario | Category / Technique | Requirement | Priority | Existing test |
| --- | --- | --- | --- | --- | --- |
| TS-01 | Initial state, `PING`, and `STATUS` | Smoke / state | Correct startup state and responsiveness | P0 | `test_boot` |
| TS-02 | Valid raw temperature boundaries | Boundary / equivalence | Accept -40 through 125 | P0 | `test_temperature_range` (6 cases) |
| TS-03 | Out-of-range setters do not change state | Negative / atomicity | Reject invalid set/calibration/threshold values | P0 | `test_invalid_range_is_atomic` (6 cases) |
| TS-04 | Malformed command and next-command recovery | Negative / protocol | Reject bad input without breaking communication | P0 | `test_malformed_command_recovery` (12 cases) |
| TS-05 | Calibration offset | Functional / equivalence | Calibrated reading = raw + offset | P1 | `test_calibration` (3 cases) |
| TS-06 | Alarm boundary and calibrated threshold behavior | Boundary / decision | Alarm on reading >= threshold | P0 | `test_alarm_boundary` (3); `test_alarm_uses_calibrated_value` |
| TS-07 | Disconnection and recovery | Fault injection / state | No successful read during disconnection; recover afterward | P0 | `test_disconnect_recovery_and_counter` |
| TS-08 | Frozen (`STUCK`) sensor and recovery | Fault injection / state | Return fixed raw 25C until cleared | P1 | `test_stuck_sensor_detected_by_stimulus` |
| TS-09 | Reset after state changes | State transition | Restore initial sensor state | P1 | `test_reset_restores_all_state` |
| TS-10 | Read timeout and desynchronized driver | Reliability / timeout | Fail promptly and require restart | P0 | `test_sensor_timeout_requires_restart` |
| TS-11 | Full communication failure | Reliability / parametrized | Silence `PING`, `READ`, `STATUS`, `RESET`; restart recovers | P0 | `test_communication_failure_silences_all_commands` (4 cases) |
| TS-12 | C++ process dies during testing | Reliability / negative | Detect exited device process | P1 | `test_dead_process_is_reported` |
| TS-13 | Reject multi-line command injection | Security / input validation | One command per transaction | P1 | `test_command_injection_rejected` |
| TS-14 | 200 sequential commands/readings | Soak-lite / state | Consistent readings and counter | P1 | `test_long_sequence` |
| TS-15 | Process cleanup | Resource lifecycle | Driver stops child process | P1 | `test_cleanup` |
| TS-16 | Typed API round-trip and structured status | API contract / integration | Translate typed calls and parse returned objects | P0 | `test_typed_api_round_trip` |
| TS-17 | Typed API invalid types and range | Negative / API | Local type validation; server range validation | P1 | `test_typed_api_rejects_invalid_numeric_types` (4); `test_typed_api_invalid_range_is_atomic` |
| TS-18 | Typed fault/recovery/reset and raw-command escape hatch | API regression | Preserve old wire interface alongside new typed API | P1 | `test_stuck_fault_and_recovery`, `test_disconnected_reads_do_not_increment_samples`, `test_reset_restores_defaults`, `test_invalid_fault_mode`, `test_raw_command_for_malformed_wire_input`, `test_typed_timeout_requires_restart` |

Test implementations: [`tests/test_device.py`](../tests/test_device.py), [`tests/test_driver_api.py`](../tests/test_driver_api.py), [`tests/cpp/test_sensor.cpp`](../tests/cpp/test_sensor.cpp), and [`tests/cpp/test_protocol.cpp`](../tests/cpp/test_protocol.cpp).

**Additional C++ unit scenarios:** `sensor_unit` covers defaults, value boundaries, non-finite rejection, atomic setters, calibration, alarm, disconnection, timeout, stuck readings, and reset. `protocol_unit` covers wire parsing/formatting, valid and malformed commands, unknown commands, fault behavior, communication silence, and reset. CTest registers the two executables as `sensor_unit` and `protocol_unit`.

## 3. Risks and Priorities

| Risk | Impact | Likelihood | Mitigation / Tests | Residual Risk |
| --- | --- | --- | --- | --- |
| Sensor silently stops responding | High: test hangs or no results | Medium (injected) | Driver deadline, `TimeoutError`, restart policy; TS-10/11 | Timeout timing depends on host scheduling |
| A delayed response is mistaken for a later command | High: false-positive test result | Medium after timeout | Mark driver desynchronized; do not reuse after timeout; TS-10/11 | No message IDs or resynchronization protocol |
| Invalid input corrupts sensor state | High: unreliable subsequent results | Medium | Setter bounds and malformed-input tests; TS-03/04/17 | All cross-field invariants are not independently checked |
| Alarm or calibration is off by one at boundary | High: incorrect alarm | Medium | Threshold and calibration tests; TS-05/06 | Floating-point output is rounded to two decimals |
| Fault leaks into another test | Medium: nondeterministic results | Medium | Fresh child process per test; TS-07/08/09 | Environmental failures can still affect tests |
| Driver hides SUT errors or misparses a reply | High: misleading assertion | Medium | Structured parsing, raw commands, typed contract tests; TS-16/17/18 | Malformed fake-device responses not exhaustively tested |
| Subprocess crash or pipe cleanup is mishandled | Medium to high | Medium | Exit detection, context-manager cleanup; TS-12/15 | Missing-executable handling is not part of the active tests |
| Environment-specific test behavior | Medium | Medium | Target Linux/WSL; use CMake to build once; TS-01/15 | Native Windows unsupported; CI not configured |

**Priority interpretation:** P0 = core functional/transport safety; P1 = important regression, state, and API-contract behavior. Priorities are this test plan's assessment, not fields stored in pytest.

## 4. Detailed Test Cases

Each test starts with a fresh `Sensor` process via the `device` fixture unless noted. Default state: raw=25C, offset=0C, threshold=80C, fault=NONE, samples=0.

| Test ID | Preconditions | Steps / Inputs | Expected Result & State | Priority |
| --- | --- | --- | --- | --- |
| TC-01 | Fresh device | `PING`, `READ`, `STATUS` | `PONG`; reading 25C/no alarm; `STATUS NONE 1` | P0 |
| TC-02 | Fresh device | Set each of [-40, -39.99, 0, 25, 124.99, 125], then read | `OK`; returned temperature matches (within 0.005C in test) | P0 |
| TC-03 | Fresh device | Send setter values outside permitted ranges | `DeviceError: OUT_OF_RANGE`; subsequent reading remains 25C/no alarm | P0 |
| TC-04 | Fresh device | Send `SET abc`, `SET nan`, missing/extra arguments, empty command; then `PING` | Error raised; later `PING` still returns `PONG` | P0 |
| TC-05 | Fresh device | `CALIBRATE -10`, `2.5`, or `10`; then read | Reading equals 25C + offset | P1 |
| TC-06 | Threshold=80C | Set 79.99C, 80C, and 80.01C; read | Alarm false, true, true respectively | P0 |
| TC-07 | Fresh device | `SET 77`, `CALIBRATE 3`, read, then `THRESHOLD 81`, read | Alarm first true (80>=80), then false (80<81) | P0 |
| TC-08 | Fresh device | `FAULT DISCONNECTED`, `READ`, `STATUS`, `FAULT NONE`, `READ` | Read error; count stays 0 while disconnected, increases to 1 after recovery | P0 |
| TC-09 | Fresh device | `FAULT STUCK`; vary raw temperature 20/30/40 and read; clear fault | Frozen readings all 25C; next reading 40C | P1 |
| TC-10 | Modified state | Change temp, offset, threshold; read; inject fault; `RESET` | `STATUS NONE 0`; next reading 25C/no alarm | P1 |
| TC-11 | Fresh process, timeout=0.1s | `FAULT TIMEOUT`, `READ`, then `PING` | `TimeoutError`; later command rejected until fresh process | P0 |
| TC-12 | Fresh process, timeout=0.15s | `FAULT COMMUNICATION_FAILURE`; try each of `PING`, `READ`, `STATUS`, `RESET` | No response; timeout; new process responds normally | P0 |
| TC-13 | Fresh device | Kill child process, then `READ` | `DeviceError` indicating process exited | P1 |
| TC-14 | Fresh device | Send `SET 50\nRESET` via `command()` | `ValueError`; state remains unchanged | P1 |
| TC-15 | Fresh device | Execute 200 set/read cycles, then `STATUS` | Every reading matches; status ends in count 200 | P1 |
| TC-16 | Fresh process | Exit `with Device(binary)` block | Process has terminated | P1 |
| TC-17 | Fresh device | `set_temperature(77)`, `set_calibration(3)`, `set_threshold(80)`, `read()`, `status()` | `Reading(80.0, True)`; `DeviceStatus("NONE", 1)` | P0 |
| TC-18 | Fresh device | Pass NaN, Infinity, string or bool to typed setter; pass 126C separately | Bad types raise `ValueError` locally; out-of-range number raises `DeviceError` from C++; state remains unchanged | P1 |
| TC-19 | Fresh device | Inject STUCK and DISCONNECTED through typed API, clear faults, reset | Typed API preserves fault, read-count, and recovery behavior | P1 |
| TC-20 | Fresh device | Raw `command("SET nan")` through new API; then `PING` | SUT returns `BAD_ARGUMENT` and communication remains healthy | P1 |

These are design-level cases mapped to automated tests above; one row may represent multiple pytest parametrizations or multiple related assertions.

## 5. Execution and Automation

- **Test levels:** Standalone **C++ unit tests** for `Sensor` and `CommandProtocol`, automated end-to-end **integration** tests across real Python subprocess pipes and C++ simulator, and **typed API contract** tests. Physical hardware testing is out of scope.
- **Environment:** Linux or WSL; C++17 compiler; CMake 3.16+; Python 3; pytest. The driver uses `selectors` and is not designed for native Windows pipe handling.
- **Build:** `cmake -S . -B build` then `cmake --build build`. `sensor_core` is a reusable library shared by the simulator and unit-test executables.
- **Required fixtures and data:** `tests/conftest.py` provides session-scoped `binary` (CMake build once) and function-scoped `device` (new process per test). `DEVICE_BINARY` can override the executable path. `pytest.mark.parametrize` supplies boundary, invalid-input, calibration, alarm, and failure-command data.
- **Mocks:** No mock sensor. Tests exercise the compiled C++ process. Some tests instantiate `Device(binary, timeout=...)` directly.
- **Setup/teardown and isolation:** `Device` is a context manager; `close()` closes the selector and pipes and terminates/kills the child as needed. Each function-scoped test starts with clean sensor state.
- **Timeouts/failure handling:** Default command timeout is 0.5s; targeted tests use 0.1s or 0.15s. The driver rejects reuse after a timeout to prevent stale-response mix-ups. Protocol `ERR` replies become `DeviceError`.
- **Logging/reporting:** CTest reports failures using `--output-on-failure`. pytest produces console output and `pytest-results.xml` in GitHub Actions; uploaded as a workflow artifact per Python version. No separate driver event logging mechanism.
- **Execution:**

```bash
python3 -m pip install pytest
ctest --test-dir build --output-on-failure
python3 -m pytest -v
# Optional machine-readable local report:
python3 -m pytest -v --junitxml=pytest-results.xml
```

- **CI triggers:** `.github/workflows/tests.yml` executes on pushes to `main`, pull requests, and manual `workflow_dispatch`. Each Ubuntu job (Python 3.11 and 3.12) compiles the C++ simulator and both unit-test executables, runs CTest, runs pytest, and uploads the pytest JUnit XML report.

## 6. Coverage, Results, and Follow-up

### Requirements and Risks Covered

- Functional: startup, legal/illegal temperature values, calibration, alarm boundaries, reset, sample count.
- Protocol: malformed requests, error handling, fault injection and post-error recovery.
- Driver: raw and typed commands, parsed reading/status objects, timeouts, desynchronization policy, subprocess death, cleanup.
- Reliability: DISCONNECTED, STUCK, TIMEOUT, COMMUNICATION_FAILURE, and 200-operation sequence.

### Current Test Inventory (source-based)

| File | Pytest cases | Scope |
| --- | ---: | --- |
| `tests/test_device.py` | 44 | Original protocol, sensor behavior, faults, process handling |
| `tests/test_driver_api.py` | 12 | New typed-driver interface, state and error-contract regression |
| `tests/cpp/test_sensor.cpp` | 11 | Sensor domain unit cases run within the `sensor_unit` CTest executable |
| `tests/cpp/test_protocol.cpp` | 13 | Protocol unit cases run within the `protocol_unit` CTest executable |
| **Total** | **56 Python + 24 C++** | Python parametrized cases counted individually; CTest registers two executables |

**Execution results / report links:** The test-source inventory contains **56 pytest cases + 24 C++ unit cases**. The CI workflow produces run-specific logs and pytest JUnit artifacts under the repository's **Actions** tab. A committed workflow does not by itself prove a passing run; check the latest completed job and log before recording a pass.

**Defects and infrastructure failures:** No open defects are established by this document. Track future issues separately with reproduction commands, expected/actual behavior, and relevant logs.

**Untested areas and limitations:**
- Standalone C++ tests now exist for `Sensor` and `CommandProtocol`; branch-coverage metrics are not collected.
- No exhaustive fuzzing, non-ASCII/binary input, or systematic parsing of intentionally corrupted device replies.
- The C++ protocol unit suite explicitly asserts `ERR UNKNOWN_COMMAND`; the Python integration suite does not have a dedicated unknown-command assertion.
- `test_missing_binary_reports_device_error` is inside a triple-quoted string in `test_device.py` and **is not an active test**.
- No multi-process concurrency, native Windows transport, hardware validation, or production load testing.
- Several out-of-range tests verify that subsequent temperature/alarm readings remain unchanged, but do not independently assert every internal calibration/threshold value.

**Regression candidates:** Run the complete pytest suite after changing C++ domain logic, command protocol, Python driver, fault behavior, or build configuration. Add focused malformed-reply tests, fuzz tests, and branch-coverage collection when time permits.

**Completion decision:** Suitable as an **interview/demo-level automation project** with unit, integration, and typed-API tests plus a Linux CI workflow. Production readiness, complete code coverage, and hardware validation are **not** claimed. Release confidence should depend on a fresh local test run and review of the limitations above.
