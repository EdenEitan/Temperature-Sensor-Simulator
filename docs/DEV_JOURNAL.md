# Development journal

This journal follows my learning and development process, from studying test automation to building the sensor and evaluating the communication code. It distinguishes the sensor I wrote myself, the integration code written with ChatGPT, and the checks recorded during development. Planned extensions are preserved in [Next steps](NEXT_STEPS.md).

## 1. From learning about testing to choosing a project

I started by learning about pytest, automation tools and frameworks, testing methodologies, and test-case design. I wanted to try these ideas on something I could build, control, and observe myself rather than only working through isolated examples.

A temperature sensor seemed like a good fit. The basic behavior is easy to understand, but there are enough decisions around calibration, thresholds, faults, and invalid inputs to make testing interesting.

Before writing code, I used Gemini to understand the basic behavior of temperature sensors. I asked for an explanation rather than code because I wanted to implement the sensor myself. My notes from that research are in [Research notes](RESEARCH_NOTES.md).

I decided to use explicitly controlled temperatures, not random or continuously changing readings. That made it possible to reproduce cases such as 79.99, 80, and 80.01 degrees around an alarm threshold of 80.

## 2. Designing and implementing the C++ sensor

**I designed and wrote the temperature sensor myself, both its behavior and its C++ code.** I used the initial research to decide what to simulate, then implemented the core logic before building the Python integration.

### Defining the behavior

I started with the state the sensor needed to hold: a raw temperature, a calibration offset, an alarm threshold, a fault mode, and a count of successful reads. I gave these values defaults so I could reset the model and know exactly what state to expect.

I implemented setters for temperature, calibration, and threshold, with separate accepted ranges. The setters check `std::isfinite` as well as the numeric boundaries. Invalid values return `false` without overwriting the last valid setting. That was important for tests where a rejected command should not change what a subsequent read returns.

A successful read applies calibration and checks the alarm threshold:

```text
calibrated_temperature = raw_temperature + calibration_offset
alarm = calibrated_temperature >= threshold
```

The alarm is inclusive: a reading exactly at the threshold activates it. I chose to calculate it during each read rather than maintain a separate alarm flag that could get out of sync when temperature, calibration, or threshold changes.

I also chose not to clamp the calibrated reading to the accepted range for raw input. For example, raw temperature 125 with offset 10 produces a valid reported reading of 135. The input constraints and the computed output are different things, and the tests need to reflect that distinction.

### Modeling failures

I wanted the sensor to expose more than a normal reading, so I added three sensor fault modes:

- `DISCONNECTED` returns a failed-read status rather than pretending a temperature measurement succeeded.
- `TIMEOUT` reports a timeout status from the sensor core; the protocol layer later decides to omit the reply.
- `STUCK` reports a fixed raw temperature of 25 degrees even if the configured raw input changes. Calibration still applies, and this read counts as successful.

I used a `ReadResult` structure to return status, temperature, and alarm together. That keeps a failed read distinguishable from a successful measurement of zero degrees. Only successful reads increment `samples`; failed reads leave the counter unchanged.

At this stage the sensor was an in-memory C++ class. It did not read from stdin, send responses to Python, or handle process deadlines. Those parts came later.

### Organizing the code

After writing the sensor implementation, I separated the interface into `Sensor.h` and the method definitions into `Sensor.cpp`. The header declares the `Sensor` class, its state, the fault and status enums, and the operations available to callers. The source file contains the implementation of those operations.

I kept the sensor state private and exposed methods to change or inspect it. This gave the integration code a defined interface instead of letting it modify sensor state directly. It also kept the sensor logic separate from the protocol and transport code added in later stages.

For reset, I used `*this = Sensor{}` to restore the default configuration, fault mode, and sample count together.

### What this stage gave me

By the time I moved to Python integration, I had a working C++ model with clear rules and predictable outputs. I could then write tests against defined behavior rather than changing the rules to match the tests. Building the sensor myself also meant I understood its internal state and could design scenarios around both successful operations and failures.

Source: [Sensor.h](../src/Sensor.h) and [Sensor.cpp](../src/Sensor.cpp).

## 3. Researching how to connect C++ to Python

After finishing the sensor, I investigated how a C++ program could communicate with tests written in Python. There were several possible approaches, but implementing cross-language integration was not the main focus of this project. I wanted to spend my effort on test development.

I used ChatGPT to write the initial communication and Python driver code. Before working through the complete driver, I focused on the basic question: how does Python send input to a running C++ program and receive its output?

The chosen approach uses a separate process. Python starts the executable with `subprocess.Popen`, writes commands to stdin, and reads replies from stdout. The sensor retains its state while the process is running, so several requests can form one scenario.

The first version exposed a generic `command()` method and a parsed `read()` result. Tests still had to construct strings for operations such as setting temperature, calibration, and faults. On the C++ side, `main.cpp` combined reading input, parsing commands, calling the sensor, and formatting output.

**What I learned:** connecting the languages introduces a second system to understand: process lifetime, framing, response parsing, and timeouts. A correct sensor does not automatically make that communication reliable.

## 4. Reviewing the first version and requesting changes

When I reviewed how the initial driver would be used, I found that tests had to know too much about the text protocol. For example, setting a temperature meant writing `device.command("SET 80")` instead of calling a method that expressed the operation.

I also wanted command interpretation separated from transport. In the initial C++ entry point, changing parsing behavior meant working in the same function that handled stdin/stdout.

I asked ChatGPT to revise those parts:

| Initial version | Problem I identified | Requested change |
| --- | --- | --- |
| Most operations used raw strings through `command()` | Tests had to construct wire commands instead of expressing sensor operations directly | Add methods such as `set_temperature()`, `set_calibration()`, `set_fault()`, and `status()` |
| `main.cpp` handled both I/O and command interpretation | Transport and protocol responsibilities were mixed | Extract parsing and dispatch into `CommandProtocol`; keep `main.cpp` as the I/O loop |

The revised API returns `Reading` and `DeviceStatus` objects. Raw `command()` remains available because negative protocol tests need to send input that the typed interface would reject.

Typed numeric setters reject invalid types and non-finite values locally, but send finite out-of-range numbers to C++. This lets the tests check the sensor's range validation rather than only the Python wrapper.

The resulting separation can be seen in [Protocol.cpp](../src/Protocol.cpp), [main.cpp](../src/main.cpp), and the typed [Python driver](../framework/device.py).

**What I learned:** reviewing generated code includes checking whether its interface supports the work I want to do. The first version could communicate, but I still needed to assess its usability and separation of responsibilities.

## 5. Checking the integration beyond reading the code

I used manual checks and a Python smoke test to compare observed behavior with the sensor rules. These checks gave me concrete evidence about the communication code rather than relying only on a code review.

### Manual command-interface checks

I compiled and ran the simulator in WSL, sent commands, and checked the responses:

| Check | Commands or sequence | What I checked |
| --- | --- | --- |
| Responsiveness and initial state | `PING`, `STATUS` | A reply arrives and the initial fault/count are correct |
| Setting a value and alarm boundary | `SET 80`, `READ` | The command reaches the sensor; 80 is returned with an active alarm |
| Rejected input and state preservation | `SET 200`, then `READ` | The invalid value is rejected and the previous 80-degree reading remains |
| Reset | `RESET`, `STATUS`, `READ` | Defaults return, the count resets, and the next reading is 25 with no alarm |

These observations are retained in the [manual test log](MANUAL_TEST_LOG.md). The log also lists sensor-fault scenarios, distinguishing expected responses from captured output.

### Python-to-C++ smoke test

I checked the basic path through [`scripts/try_driver.py`](../scripts/try_driver.py): start the simulator, send `PING`, set raw temperature to 77, apply offset 3, read temperature and alarm, query status, and leave the context manager.

The recorded output was:

```text
PONG
Temperature: 80.0
Alarm: True
STATUS NONE 1
```

This checked that commands sent through Python affected the C++ sensor and that the returned data was parsed consistently with its rules. It was a basic integration check; the printed output alone does not prove every cleanup or failure path.

### Communication failure and a stale executable

While examining faults, I distinguished a failed measurement from a device that no longer responds at all. A sensor `TIMEOUT` suppresses `READ`, while `PING` can still work. `COMMUNICATION_FAILURE` was added at the command-interface layer to silence all commands after acknowledging activation.

My first recorded attempt produced:

```text
PING
PONG
FAULT COMMUNICATION_FAILURE
ERR BAD_ARGUMENT
```

The updated source had not been rebuilt. After rebuilding, activation returned `OK` and the next `PING` had no response. That was a build-artifact problem, not evidence that the updated parser rejected the command.

The communication-failure flag now lives in `CommandProtocol` after the refactor. The sensor's own fault enum remains separate.

**What I learned:** validating generated integration code means checking real exchanges and their effects on state. When behavior differs from the source, the executable being tested is also part of the investigation.

## 6. Understanding what a timeout means for the next command

Raising `TimeoutError` answers “did this request finish?” It does not answer “can I safely send another one?”

This protocol has no request IDs. After a timeout, a delayed reply could be mistaken for the response to a later command. The driver therefore marks the stream desynchronized and refuses further requests. Recovery means closing it and creating a new `Device` process.

That policy is intentionally conservative. A sensor-level timeout does not make the C++ protocol permanently unresponsive: direct protocol tests can clear the fault. But once the Python driver has timed out, it will not attempt that recovery on the same stream. Restart also discards the sensor's configuration and counter.

**What I learned:** recovery is a state transition with consequences, not simply “try again.” The simulator's fault semantics and the client's recovery policy must be documented separately.

Evidence: `test_sensor_timeout_requires_restart`, `test_communication_failure_silences_all_commands`, and `test_typed_timeout_requires_restart`.

## 7. From manual checks to automated coverage

The current automated suite organizes checks around behavior: initialization, valid boundaries, invalid inputs, calibration, alarm decisions, faults, recovery, and resource cleanup. Parametrization keeps related input cases together.

The build fixture has session scope because the executable can be reused. The device fixture has function scope because sensor state cannot: one test's calibration or fault should not affect another test.

Some of the most useful checks involve sequences:

- Change the input several times while `STUCK` is active, then clear the fault and verify the new value becomes visible.
- Attempt an invalid update and verify the existing reading remains intact.
- Disconnect, verify failed reads do not increment the counter, then recover and check the next successful read.
- Kill the child process and verify the driver reports it instead of waiting for data.
- Perform 200 set/read cycles and verify both values and the final count.

C++ unit tests isolate sensor and protocol rules. Python tests cross real subprocess pipes, so they are integration tests, including those using the typed API.

**What I learned:** a single expected response is often insufficient. State before failure, behavior during failure, and state after recovery are all part of the contract. Test counts also do not prove every branch or malformed reply is covered.

Evidence and gaps: [Test plan](TEST_PLAN.md).

## 8. Making the checks repeatable in CI

The workflow builds the C++ project and runs CTest and pytest on Ubuntu with Python 3.11 and 3.12. It saves pytest JUnit reports so failures can be inspected after a run.

A separate Clang job builds with AddressSanitizer and UndefinedBehaviorSanitizer and runs the C++ unit suites. It does not run the Python integration suite. Keeping that scope explicit matters when describing what the pipeline actually validates.

**What I learned:** functional assertions and runtime instrumentation answer different questions. Both are useful, but neither replaces examining the actual run or identifying gaps in the test design.

Evidence: [CI workflow](../.github/workflows/tests.yml).

## Where I want to take this next

Two design directions came out of the work: a deterministic thermal-response model and a Reactor-based server for multiple clients sharing one sensor. Both are preserved in [Next steps](NEXT_STEPS.md), including the proposed interfaces, synchronization problem, failure handling, and tests. Neither is implemented in the current source.
