# Development Journal — Temperature Sensor Simulation

### Goal

build a temperature sensor simulator in C++


## Understanding the Problem and Defining the Scope


### Initial Research

**Goal:** understand what a temperature sensor does, what software can expect from one, and which behaviors would be useful to simulate.

I used Gemini for background research: 

**My prompt:**

> I want to build a temperature sensor simulation in C++. Describe what a temperature sensor is supposed to do and what its function is. Don't build it for me; part of the goal is to try building it myself.

**Full research response:** [Research Notes](./RESEARCH_NOTES.md)

#### What I Learned

A real temperature sensor measures temperature through a physical property, such as resistance or voltage, and makes the information available to software. Depending on the device, this can involve signal conditioning, analog-to-digital conversion (ADC), and a digital interface.

The research introduced several behaviors I could simulate:

- **Calibration:** Applying a correction to a raw reading. For example, if the raw reading is 28°C and the calibration offset is +2°C, the reported temperature becomes 30°C.
- **Threshold alerts:** Indicating when a measured temperature reaches a configured limit.
- **Thermal inertia:** A physical sensor may react gradually to changes in its environment.
- **Noise and quantization:** Hardware readings can fluctuate or have limited resolution.
- **Faults:** A device may become disconnected, return frozen readings, or fail to respond.

###Defining the First Version

A full physical model would include things like heat transfer, electronic noise, and ADC conversion. For this project, I want to focus on **software behavior and automated testing**, not physical accuracy.

I chose a **deterministic simulator**: a given state and command should produce a predictable response. This will make it easier to write repeatable tests with clear expected results.

**Design principle:** The simulator should be realistic enough for the software behaviors I want to test, but still easy to control and verify.


#### Functional Requirements

The simulator should let a client:

1. Set a raw temperature explicitly.
2. Apply a calibration offset.
3. Configure an alarm threshold.
4. Request a reading and receive both the calibrated temperature and alarm status.
5. Inject specific faults to test failure handling.
6. Query the device status and reset it to known defaults.
7. Check that the simulator is responsive.

#### First-Version Limits and Defaults

| Property | Default | Allowed input |
| --- | --- | --- |
| Raw temperature | 25°C | -40°C to 125°C |
| Calibration offset | 0°C | -10°C to 10°C |
| Alarm threshold | 80°C | -40°C to 125°C |
| Fault mode | `NONE` | `NONE`, `DISCONNECTED`, `STUCK`, `TIMEOUT` |
| Successful read count | 0 | Increments after a `READ` that returns data |

These are **simulation rules**, not claims about the specifications of a particular real sensor.

### Design Decisions

#### 1. Predictable Readings Instead of Random Data

I will control the raw temperature explicitly rather than generate random values. This will let me test exact boundary conditions, such as the difference between 79.99°C and 80.00°C when the alarm threshold is 80°C.

I may add controlled noise later, but it is not necessary for the first version.

#### 2. Calibration and Alarm Logic

The reported temperature will be calculated using a simple additive correction:

```text
reported_temperature = raw_temperature + calibration_offset
```

An alarm will be active when:

```text
reported_temperature >= alarm_threshold
```

This creates useful test cases for normal readings, calibration, and alarm boundaries. I don't need a separate stored alarm variable because it can be calculated from the current values.

#### 3.Controlled Fault Injection

I want to reproduce failure cases without relying on actual faulty hardware:

| Mode | Expected behavior on `READ` |
| --- | --- |
| `NONE` | Return a normal reading |
| `DISCONNECTED` | Return an explicit error |
| `STUCK` | Return a reading whose raw value is frozen at the initial 25°C (calibration still applies) |
| `TIMEOUT` | Do not send a response |

These distinct behaviors will help me test error reporting, frozen-data scenarios, timeout handling, and recovery in the Python driver.

#### 4.Validate Inputs Without Changing Valid State

I will reject invalid commands and values outside the defined ranges instead of silently accepting or clamping them. A rejected command should leave the sensor's existing state unchanged.

For example, `SET 150` should return an out-of-range error rather than changing the raw temperature.

#### 5. Keep Only the State I Need

I expect the simulator to maintain these fields:

| State variable | Why it is needed |
| --- | --- |
| `temperature` | Current raw temperature |
| `offset` | Calibration correction |
| `threshold` | Alarm limit |
| `fault` | Currently injected fault |
| `samples` | Number of `READ` commands that returned data since reset |

I want to avoid redundant state: for example, storing an alarm flag separately would mean updating it whenever the temperature, offset, or threshold changes. The sample counter *does* need to be stored because it represents history that cannot be calculated from the current temperature.

#### 6. A Simple Command/Response Interface

I plan to expose the simulator through text commands:

| Command | Purpose |
| --- | --- |
| `SET` | Update raw temperature |
| `CALIBRATE` | Update calibration offset |
| `THRESHOLD` | Update alarm threshold |
| `READ` | Request a reading and alarm status |
| `FAULT` | Select a simulated fault |
| `STATUS` | Query fault mode and successful read count |
| `RESET` | Restore default state |
| `PING` | Check that the program responds |

The C++ program will read commands from **standard input (`stdin`)** and write replies to **standard output (`stdout`)**. Later, a Python driver can launch it as a separate process and communicate with it through pipes.

I prefer this over testing only a local function because I also want to test communication, malformed requests, timeouts, and process management.

### Keeping the Scope Focused

The main focus is still testing software behavior, not reproducing every detail of physical hardware.

| Feature | Decision |
| --- | --- |
| Gaussian noise | Not in the first version. Fixed inputs make exact checks easier. |
| ADC / raw electrical signals | Out of scope. The simulator uses Celsius values directly. |
| Power-management modes | Not needed for the tests I want to write. |
| Physical conversion delays | Not required. I will control simulated time explicitly if I add thermal response. |
| Thermal inertia | Initially left out, but I want to explore a small, deterministic model as an extension. |

###  Writing the Sensor Header (`Sensor.h`)

**Goal:** Define what the sensor stores and what operations other parts of the program can call. I started with the interface before implementing the methods.

I created `src/Sensor.h` and used a `class Sensor`. I kept the internal fields `private` and exposed methods through `public`.

**Why private fields?** I do not want other code to change the temperature, calibration or threshold without going through validation. For example, an invalid `setTemperature(200)` should not modify a valid reading.

The first header included five state fields:

| Field | Type | Default | Reason |
| --- | --- | --- | --- |
| `temperature` | `double` | `25.0` | Raw reading, before calibration |
| `offset` | `double` | `0.0` | Calibration correction |
| `threshold` | `double` | `80.0` | Alarm limit |
| `fault` | `Fault` | `Fault::NONE` | Selected fault mode |
| `samples` | `unsigned int` | `0` | Successful reads since reset |

I used `double` for temperature-related values because the tests need decimal values such as `79.99`. I used an unsigned integer for the counter because it should not be negative.

#### Fault Types and Read Results

I defined `enum class Fault` with `NONE`, `DISCONNECTED`, `STUCK` and `TIMEOUT`, instead of storing fault names as arbitrary strings. That gives the sensor a fixed set of valid fault states.

I also separated the **sensor's fault mode** from the **result of a read operation**:

```cpp
enum class ReadStatus {
    OK,
    DISCONNECTED,
    TIMEOUT
};

struct ReadResult {
    ReadStatus status = ReadStatus::OK;
    double temperature = 0.0;
    bool alarm = false;
};
```

At first I considered returning just a temperature, but `read()` needs to communicate more than one thing: whether the operation worked, the calibrated temperature and the alarm state.

I grouped these values into one `ReadResult` so they belong to the same read operation. A frozen sensor (`STUCK`) can still return a reading with status `OK`; the data is frozen, but the operation itself did not fail.

The `temperature` and `alarm` fields in `ReadResult` are meaningful only when `status` is `OK`. The default values above are there to keep the struct initialized, not to represent a valid measurement after an error.

#### Public Interface

I declared the following methods in the header (the implementations will go into `Sensor.cpp`):

| Method | Responsibility |
| --- | --- |
| `setTemperature(double)` | Set raw temperature after validating the range |
| `setCalibration(double)` | Change calibration offset |
| `setThreshold(double)` | Change the alarm threshold |
| `read()` | Return the reading, alarm state and read status |
| `setFault(Fault)` | Choose the simulated fault |
| `getFault() const` | Inspect the current fault without changing it |
| `getSamples() const` | Inspect the successful-read counter |
| `reset()` | Return the sensor to its defaults |

I used `bool` for the configuration methods so they can indicate whether an update was accepted. I used `const` on the getters because they should not modify the sensor. `read()` is not `const` because it increments the sample counter when a reading succeeds.

I deliberately did **not** put commands like `PING` or the text response `STATUS NONE 2` inside the sensor class. Those belong to the command interface in `main.cpp`; the sensor should focus on sensor behavior.

### Step 7 — Extending the Header to Support a Simulated Measurement Process

While reviewing the design, I noticed an important limitation: `SET` puts a temperature directly into the sensor, and `READ` returns that value after calibration. This is useful for testing, but it is not the same as a sensor observing an environment.

I want to keep direct `SET` for the existing deterministic tests and add **an optional, controlled thermal-response model**. This will let me simulate what happens when the surrounding temperature changes, without using real hardware or depending on real elapsed time.

#### Additional State

I added these declarations to the header:

```cpp
double environmentTemperature = 25.0;
static constexpr double kThermalTimeConstantSeconds = 5.0;
```

- `environmentTemperature` is the temperature of the simulated surroundings.
- `temperature` remains the sensor's current raw reading (before calibration).
- `kThermalTimeConstantSeconds` is a fixed parameter that controls how quickly the sensor responds. Five seconds is a simulation choice, **not a specification of a real sensor**.

#### Additional Methods

```cpp
bool setEnvironmentTemperature(double value);
bool advanceTime(double dt);
```

The first method changes the simulated surroundings. The second advances **simulated time** and moves the sensor temperature toward the environment temperature. Both methods will validate their inputs before changing state.

I plan to implement a simple first-order response:

```text
alpha = 1 - exp(-dt / time_constant)
temperature += alpha * (environmentTemperature - temperature)
```

For example, if the sensor starts at 25°C and the environment changes to 90°C, calling `advanceTime()` will make the reading rise gradually rather than jumping immediately to 90°C. `READ` will still apply calibration and check the alarm threshold.

**Why advance time explicitly?** I want tests to control exactly how much simulated time passes. This avoids `sleep()` calls and keeps the behavior repeatable.

**Compatibility decision:** The existing `SET` operation will continue to set the raw temperature directly. The thermal model only changes the reading when `advanceTime()` is called. Later, I can add protocol commands for environment temperature and advancing time without changing the existing command behavior.

At this point, I have **updated the declarations in `Sensor.h` only**. I have not implemented the new methods or exposed them through the text protocol yet.

### Writing the Sensor Implementation (`Sensor.cpp`)

**Goal:** Implement the behavior declared in `Sensor.h` without mixing sensor logic with text-command parsing.

I created `src/Sensor.cpp` and included `Sensor.h`. The method definitions use `Sensor::` to indicate that they belong to the `Sensor` class.

**Configuration and validation.** I implemented `setTemperature`, `setCalibration`, and `setThreshold`. Each method checks that its input is finite (`std::isfinite`) and within the configured range before modifying the corresponding private field. It returns `false` on invalid input, leaving the previous state unchanged. This keeps invalid requests from corrupting an otherwise valid sensor state.

**Reading and alarm logic.** The `read()` method first checks the current fault mode. If the mode is `TIMEOUT` or `DISCONNECTED`, it returns the corresponding `ReadStatus` without producing a valid measurement or incrementing `samples`. For a normal read, it uses the stored raw temperature; in `STUCK` mode, it uses the fixed raw value of 25°C instead. It then adds the calibration offset, evaluates `calibrated >= threshold`, increments `samples`, and returns a `ReadResult` containing the status, temperature, and alarm flag.

**Fault state and status.** I added `setFault()` to select a fault mode and `getFault()` / `getSamples()` to expose the required status information. The getters are `const` because they do not change the sensor state.

**Reset.** I used `*this = Sensor{};` to restore the default state in one operation. This also resets any fields initialized in the class, including the future environment-temperature field.

**Important boundary:** `Sensor.cpp` reports `ReadStatus::TIMEOUT`; it does not block, sleep, or suppress output itself. The command interface decides not to send a reply in that case. I kept this distinction so the sensor model is independent of the transport.

**Not implemented yet:** `setEnvironmentTemperature()` and `advanceTime()` are declared in the updated header but do not yet have definitions. The thermal-response extension remains planned.

### Writing the Command Interface (`main.cpp`)

**Goal:** Make the C++ simulator usable as a standalone process and give a future Python driver a stable command/response interface.

I kept communication responsibilities in `src/main.cpp` and sensor behavior in the `Sensor` class. The program creates one `Sensor` instance, then enters a loop that reads one input line at a time. Each command is processed before the next one, and the sensor instance preserves its state between commands.

**Parsing a request.** I use `std::getline(std::cin, line)` to receive a complete line, then `std::istringstream` to read the command name and its arguments. This lets me reject extra tokens rather than accidentally treating them as another request.

I added three small helpers:

- `parseNumber()` checks that exactly one finite number follows a numeric command.
- `parseFault()` translates a fault name from text into `Fault`.
- `faultToString()` converts a `Fault` value back to text for `STATUS`.

**Dispatching commands.** `SET`, `CALIBRATE`, and `THRESHOLD` call the corresponding sensor methods. `FAULT` selects a simulated failure. `READ` requests a `ReadResult`. `STATUS` reads the fault mode and sample counter, `RESET` restores defaults, and `PING` responds with `PONG` without accessing the sensor.

**Errors and replies.** Parsing errors produce `ERR BAD_ARGUMENT`, while out-of-range numeric values produce `ERR OUT_OF_RANGE`. A disconnected read produces `ERR DISCONNECTED`. A successful read produces a line such as `DATA 80.00 ALARM`, with two decimal places. For a `TIMEOUT` read, the interface intentionally writes no response. I flush stdout after each command so a driver reading through a pipe does not wait for buffered output.

**Concurrency decision for the first version.** This entry point is intentionally single-threaded. It reads from one stdin stream, processes commands sequentially, and has no multi-client routing. I want to keep it as a simple, stable baseline for the Python integration tests, even if I add a concurrent server later.

### Planned Extension: Multiple Clients with a Shared Sensor (SPL Reactor)

**Status: design only — not implemented yet.** I want to preserve the existing C++ command-line simulator and add a second, optional server entry point for concurrent clients.

**Motivation.** In a real monitoring system, multiple software components may need access to the same device: for example, a monitoring service can read temperature while a management service updates the alarm threshold. The challenge is not just accepting multiple connections; it is maintaining a consistent **shared sensor state** when requests arrive concurrently.

**Reusing course work.** I reviewed the Java Reactor implementation from my SPL course. It already includes `Reactor`, `ActorThreadPool`, `NonBlockingConnectionHandler`, and `LineMessageEncoderDecoder`. The line-based decoder fits the current text protocol. I plan to reuse and adapt these components, clearly attributing the course framework rather than presenting it as a new implementation written from scratch.

**Planned architecture:**

```text
Multiple TCP clients
        |
        v
Java Reactor (Selector + configurable thread pool)
        |
        v
Sensor-specific protocol / shared request queue
        |
        v
One C++ simulator process (one shared Sensor)
        |
        v
Response routed back to the requesting client
```

The new Java server will accept TCP connections, decode newline-separated commands, and forward them through a Java-to-C++ bridge (for example, using `ProcessBuilder` and the existing stdin/stdout protocol). All clients will share **one C++ process**, not one simulator per connection. The original `main.cpp` and its single-client workflow will remain available.

**Concurrency challenge.** The course `ActorThreadPool` uses the connection handler as the actor key, so it orders tasks belonging to one connection. That alone does **not** serialize access to a sensor shared by different connections. I will need a separate shared command queue or another synchronization mechanism that ensures only one command is in flight to the C++ process at a time, while associating each response with the correct client. Blocking calls to the bridge must not block the Reactor's selector thread.

**Failure handling.** A simulated `TIMEOUT` intentionally produces no C++ response. The bridge must therefore enforce a timeout, fail or cancel the affected request, and recover or restart the process before accepting further requests if synchronization is uncertain. I also need to define what happens to queued requests during recovery and when a client disconnects.

**Tests to add later (Python):** concurrent connections, state sharing between clients, consistent sample counting under concurrent reads, conflicting `SET` / `READ` / `RESET` requests, correct response routing, disconnected clients, and timeout recovery. I will review the Reactor's thread-safety and shutdown behavior before relying on it under load.

**Design decision:** Keep concurrency outside the sensor's command-line entry point. This lets the existing deterministic integration tests continue to run unchanged while the concurrent server becomes a separately testable extension.

### Next Step

Finish building and running the C++ executable locally, then implement the Python device driver and pytest integration tests for the single-client baseline. The thermal-response model and the shared-sensor Reactor server are optional extensions, not completed features.


