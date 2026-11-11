# Test Development Journal — Six-Stage Testing Workflow

This journal describes how I developed the automated tests for my temperature sensor simulator. I organized the work around six stages of a test-design workflow, from understanding requirements to evaluating the results and refining the test suite.

The sensor itself was designed and implemented by me. The C++/Python communication and driver code were developed with ChatGPT assistance. My focus in this stage was learning how to approach software testing systematically, not just writing assertions that happen to pass.

## 1. Scope and Requirements

I started with the behavioral requirements defined at the beginning of the project, rather than building tests around whatever the implementation happened to do. The sensor should accept only **finite** raw temperatures from -40 to 125 °C, support calibration and configurable alarm thresholds, reject invalid updates without changing valid state, and behave predictably under faults.

I also identified separate responsibilities for the C++ command protocol and Python driver. The protocol should handle valid commands, malformed arguments and unknown commands; the driver should send requests, interpret responses, enforce timeouts and manage the simulator process.

## 2. Test Scenarios

I grouped scenarios by behavior: startup, accepted inputs, boundary values, invalid input, calibration, alarms, fault injection, recovery, reset, and process lifecycle. I considered both ordinary use and negative cases.

A useful scenario often involves more than one command. For example, to test invalid input properly I need to send an invalid setting and then read the sensor again. Receiving an error alone does not prove the previous valid setting was preserved.

## 3. Risks and Priorities

I prioritized cases that could silently produce misleading results: invalid state changes, incorrect alarm behavior at boundaries, hanging requests, and incorrectly reused communication streams after a timeout.

These risks informed the P0/P1 priorities and scenario mapping in the [Test Plan](TEST_PLAN.md).

## 4. Detailed Test Cases

I defined initial state, action, expected response and expected state after each case. I used boundary value analysis at -40/125 °C and around the alarm threshold; invalid-input tests; and state-transition sequences for faults and recovery.

I used parametrization for families of related inputs. I also left room to revisit these cases after the first executions: a test plan is not complete merely because all initially imagined cases have been implemented.

## 5. Test Implementation and Execution

I moved from manual C++ command checks to Python pytest tests communicating with the actual simulator process.

A session-scoped fixture handles the build, while function-scoped device fixtures give individual tests independent sensor state. The suite tests both raw protocol commands and typed Python methods. C++ unit suites separately test the sensor and protocol directly.

I ran tests and used the results as feedback on the implementation and the test design. CI is also configured to run CTest and pytest. The code in the repository shows the current coverage; check individual CI runs for execution status rather than treating the presence of a test as proof of a passing run.

## 6. Evaluation and Improvement

After the initial testing, I went back to the original requirements and reviewed the test cases more critically. That evaluation led to two findings, followed by changes:

1. **NaN validation:** the requirement said inputs must be finite. a pytest test failed while checking that requirement. Investigating the failure showed that validation based only on rejecting numbers outside the permitted range could allow `NaN`. **Following that finding, I fixed the sensor's validation** with `std::isfinite` and added regression coverage for non-finite values.
2. **Unknown commands:** I realized I had covered bad arguments to recognized commands, but had not explicitly covered an unrecognized command name in pytest. **Following that coverage finding, I added** a dedicated pytest case that checks the error and verifies the device still responds afterward.

I also made a separate Git working copy and deliberately changed the alarm comparison from `>=` to `>`, then separately bypassed calibration, rerunning tests after each change to evaluate whether my assertions exercised those requirements. These were controlled mutation-testing experiments, not permanent changes to the main code.

The full investigation, fixes, and deliberate fault experiments are described in the [Test Execution and Evaluation Journal](TEST_EVALUATION_JOURNAL.md).

## What I Took Away

The most useful lesson from this workflow was that testing is iterative. I did not stop at implementing the first set of cases. Revisiting the requirements after execution exposed a real validation issue and a gap in my test design. That feedback changed both the code and the tests.

Related files: [Test Plan](TEST_PLAN.md), [Integration Tests](../tests/test_device.py), [Typed API Tests](../tests/test_driver_api.py), [Sensor Unit Tests](../tests/cpp/test_sensor.cpp), and [Protocol Unit Tests](../tests/cpp/test_protocol.cpp).
