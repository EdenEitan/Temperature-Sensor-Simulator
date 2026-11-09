
# My Development Journal 

**Project Goal**

I want to build a temperature sensor simulator in C++, followed by a Python test automation framework that validates the simulator's behavior.

The main objective is to simulate embedded device behavior in a controlled environment, allowing software functionality, communication, error handling, and fault recovery to be tested without physical hardware.

**My Goal :) .**

practice C++ programming, object-oriented design, software testing, and automation.
Here, I will document the steps I took, the challenges I faced, the design decisions I made, and what I learned at each stage.

**Journal Contents**

[0] 
[1] Building the Temperature Sensor
[2] 

---------------------------------------------------------------------------------------------------------



## Step 0 
---------------------------------------------------------------------------------------------------------

>General Planned Architecture

I want three main parts:

1. **C++ simulator:** Implements sensor state, command handling, readings, and simulated faults.
2. **Python driver:** Starts the C++ executable, sends commands, reads replies, handles errors and timeouts, and cleans up the process.
3. **pytest tests:** Check normal behavior, boundary values, input validation, faults, and recovery.

Once the core flow works, I plan to add **GitHub Actions** to build the executable and run the tests automatically.


## Step 1 — Building the Temperature Sensor
---------------------------------------------------------------------------------------------------------

**Description:**

-Research how temperature sensors work, define the simulation requirements, and implement the core sensor behavior in C++.


[Dev Journal](./temperature_sensor_DEV_JOURNAL.md)

---------------------------------------------------------------------------------------------------------


## Step 2 — Designing the Communication Protocol
---------------------------------------------------------------------------------------------------------

**Description:**

Define how external software will communicate with the sensor simulator.

Design a simple command-response protocol supporting operations such as `SET`, `READ`, `CALIBRATE`, `THRESHOLD`, `FAULT`, `STATUS`, `RESET`, and `PING`.

Specify expected responses, input validation rules, and error messages.

[Dev Journal]()

---------------------------------------------------------------------------------------------------------


## Step 3 — Implementing the C++ Command Interface

**Description:**

Implement the command-processing logic that connects the communication protocol to the sensor.

The application should continuously read commands from standard input (`stdin`), interpret them, execute the corresponding sensor operations, and return responses through standard output (`stdout`).

Handle invalid commands and malformed input without crashing or unexpectedly changing the sensor state.

---

## Step 4 — Building and Manually Testing the Simulator

**Description:**

Set up the C++ build process using CMake and compile the simulator into an executable.

Run the program manually, send commands, and verify the expected responses.

Check the basic functionality, boundary conditions, invalid inputs, and reset behavior before developing the automated testing framework.

---

## Step 5 — Building the Python Device Driver

**Description:**

Develop a Python interface that communicates with the C++ simulator.

Use `subprocess` to launch the executable as a separate process and exchange commands and responses through pipes.

Implement methods for sending commands, reading sensor values, parsing responses, handling exceptions, enforcing timeouts, and managing process cleanup.

---

## Step 6 — Setting Up the pytest Testing Framework

**Description:**

Create the automated testing environment using pytest.

Organize the test files and implement reusable fixtures for compiling the simulator and creating test instances.

Ensure every test runs with a fresh sensor process to prevent state from previous tests from affecting later results.

---

## Step 7 — Writing Functional and Boundary Tests

**Description:**

Develop automated integration tests that validate the simulator's expected behavior.

Cover temperature measurements, calibration, alarm thresholds, input validation, command parsing, state consistency, and reset functionality.

Use `pytest.mark.parametrize` to test multiple input values and boundary conditions without duplicating test code.

---

## Step 8 — Fault Injection and Reliability Testing

**Description:**

Extend the automated tests to cover abnormal conditions and system failures.

Simulate disconnected sensors, frozen readings, missing responses, and unexpected process termination.

Verify timeout handling, error reporting, recovery behavior, resource cleanup, and repeated command execution.

---

## Step 9 — Continuous Integration with GitHub Actions

**Description:**

Set up a CI workflow that automatically builds and tests the project whenever changes are pushed to GitHub or a pull request is opened.

Configure the Python test environments, compile the C++ simulator with strict compiler checks and UndefinedBehaviorSanitizer, execute the pytest suite, and generate test reports.

The goal is to detect regressions automatically and ensure that changes do not break existing functionality.

---

## Step 10 — Documentation, Validation, and Final Review

**Description:**

Document the project architecture, communication protocol, setup instructions, and testing approach.

Review the complete test suite, validate the results, and identify limitations in the current implementation.

Evaluate potential improvements such as separating sensor logic from command parsing, improving the C++ class design, and extending the simulator with additional capabilities.

Summarize the main technical challenges, design decisions, and lessons learned throughout the project.

