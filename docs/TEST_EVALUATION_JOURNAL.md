# Test Execution and Evaluation Journal

This journal records what I found when I ran and evaluated my pytest tests, returned to the original requirements, and revised the test cases. It describes **the findings first and the fixes that followed**, rather than presenting the corrected implementation as if it had been there from the beginning.

It is a retrospective of the development process, not a claim that a new test run was executed while writing this document.

## Returning to the Original Requirements

After writing the initial tests, I reviewed the system's requirements again. I wanted to check whether my test cases actually validated what I had said the system should do, not just whether the expected normal flows worked.

Two gaps stood out. One revealed a problem in the sensor's validation logic; the other revealed a missing category in my pytest coverage.

## Finding 1 — A NaN Value Could Pass the Sensor's Validation

**Original requirement:** numeric inputs to the sensor must be finite and within their allowed ranges.

While running the pytest suite, **one of the tests failed** when I checked the requirement that the sensor accept only finite values. I investigated the failure instead of changing the expected result to match the implementation. Looking at the sensor code, I realized that its original input check only rejected values below the minimum or above the maximum:

```cpp
if (value < -40.0 || value > 125.0) {
    return false;
}
```

The problem is that `NaN` does not behave like an ordinary number in comparisons. Both `NaN < -40.0` and `NaN > 125.0` evaluate to false, so the rejection condition is not triggered. That meant the original check could allow `NaN` into the sensor despite the finite-values requirement.

This was a particularly useful finding because a value can appear to have passed a range check without being a meaningful numeric measurement. Once stored, `NaN` can also affect later calculations and comparisons.

### The Fix I Made After the Finding

I updated the validation to reject non-finite values explicitly:

```cpp
if (!std::isfinite(value) || value < -40.0 || value > 125.0) {
    return false;
}
```

I applied finite-number validation to the sensor's numeric setters, and the current test suite includes regression cases for non-finite values. The C++ implementation uses `std::isfinite` from `<cmath>`; the Python typed driver separately uses `math.isfinite`.

### What I Learned

**1. Define what valid input is, rather than relying only on excluding obvious invalid ranges.** Here the valid set is finite numbers *inside* an inclusive interval. A positive form is `std::isfinite(value) && value >= min && value <= max`. Importantly, the finiteness check makes this requirement explicit; I would not rely on inequality checks alone to communicate that contract.

**2. Use the language's numeric tools instead of assuming floating-point comparisons behave like ordinary arithmetic.** I learned about `std::isfinite`, which rejects both `NaN` and infinities, and why that matters for robust input validation.

### Regression Coverage

The corrected code is in [Sensor.cpp](../src/Sensor.cpp). Non-finite behavior is covered by [C++ sensor tests](../tests/cpp/test_sensor.cpp), protocol cases such as `SET nan` in [C++ protocol tests](../tests/cpp/test_protocol.cpp), raw-command pytest cases in [test_device.py](../tests/test_device.py), and typed-driver tests using `float("nan")` in [test_driver_api.py](../tests/test_driver_api.py).

The current behavior reflects the **fix made in response to the finding**. The vulnerability above describes the earlier implementation.

## Finding 2 — I Had Tested Invalid Arguments, but Not Unknown Commands in pytest

When I reviewed the negative test cases, I noticed that I had tests such as `SET abc`, `READ extra`, and `FAULT INVALID`. All of them send an existing command with an invalid or unexpected argument.

I realized that was a different problem from sending a **command name the protocol does not recognize**. Those two situations should not be grouped together just because both are invalid input:

| Request | What is wrong | Expected response |
| --- | --- | --- |
| `SET abc` | Invalid argument to a known command | `ERR BAD_ARGUMENT` |
| `BOGUS` | Unknown command name | `ERR UNKNOWN_COMMAND` |

### The Test I Added After the Finding

After identifying this gap, I added `test_unknown_command_is_rejected_and_connection_recovers` to [test_device.py](../tests/test_device.py). It sends `BOGUS`, checks for `UNKNOWN_COMMAND`, and then sends `PING` and a normal `READ` to verify the communication continues.

The C++ protocol unit tests also cover unknown commands, but I wanted the pytest integration suite to exercise the actual process boundary as well.

### What I Learned

A negative-testing category is not automatically covered by checking a similar-looking error. **Bad arguments and unknown operations exercise different paths.** I also learned to verify the state of the interface after an error, not only the error message itself.

## Checking My Tests with Deliberately Introduced Bugs

After writing the tests and fixing the issues I found, I also wanted to check something different: **would the tests actually notice if important sensor behavior became incorrect?**

I used Git to create a separate working copy of the project so I could safely experiment without changing the working implementation. In that copy, I deliberately introduced faults **one at a time**, reran the relevant pytest tests, and compared the behavior with the original requirements. I restored the original behavior between experiments so that each change tested a separate assumption.

### Experiment 1 — Changing the Alarm Boundary

The requirement states that the alarm must activate when the calibrated temperature is **greater than or equal to** the threshold. I intentionally changed the comparison from:

```cpp
const bool alarm = calibrated >= threshold;
```

to:

```cpp
const bool alarm = calibrated > threshold;
```

This creates a subtle off-by-one-style boundary defect: a reading equal to the threshold is now incorrectly reported as normal. I reran the tests to check their ability to detect this change. The existing `test_alarm_boundary` includes the exact threshold (80 °C), alongside values just below and above it, so this experiment directly targets the importance of inclusive boundary testing.

### Experiment 2 — Ignoring the Calibration Offset

In a separate experiment, I changed the reading calculation so that it returned the raw temperature without applying the calibration offset:

```cpp
const double calibrated = raw;
```

instead of:

```cpp
const double calibrated = raw + offset;
```

I reran the tests again to examine whether they checked the effect of calibration rather than only successful reads. Tests such as `test_calibration`, `test_alarm_uses_calibrated_value`, and `test_typed_api_round_trip` exercise this requirement.

### What These Experiments Taught Me

These were **deliberate fault-injection experiments (a simple form of mutation testing)**, not defects discovered in the original application. Unlike an ordinary regression run, which checks that correct behavior is preserved, this approach challenges the test suite by making the implementation wrong on purpose.

It helped me think about test quality in a more practical way: a test is useful not only when it passes on the correct implementation, but when its assertions are specific enough to detect plausible mistakes. Boundary checks and calibration tests need to test the actual requirement, not just verify that the command returned a response.

The project code linked here contains the correct `>=` comparison and applies the calibration offset. I did not keep the deliberate faults in the main implementation.

## Evaluation — Why These Findings Matter

Rewriting and reviewing my cases against the original requirements helped me distinguish between:

- **Requirement coverage:** does a test address the stated behavior, such as finite numeric input?
- **Input categories:** have I considered different invalid inputs, not just different values of the same type?
- **Implementation correctness:** did a test uncover a real mismatch between requirement and code?
- **Recovery:** can the system continue to behave correctly after rejecting a request?
- **Regression protection:** did I add tests that would detect the problem if it returned?

The first finding began with a **failing pytest test**, which led me to investigate the numeric validation, **fix the code**, and retain regression coverage. The second led to **additional pytest coverage**. I then used deliberate code mutations in a separate Git copy to challenge the tests themselves. Together, these steps made evaluation an active part of development rather than a final checkbox.

## Evidence and Limits

The repository contains the corrected implementation and current regression tests. The [Test Plan](TEST_PLAN.md) maps implemented scenarios to requirements, and the [CI workflow](../.github/workflows/tests.yml) defines automated runs. The failing NaN pytest test and the deliberate mutation experiments are described from my development work. Exact historical terminal output and per-experiment pass/fail logs are not preserved here, so I have not invented those results or claimed a newly verified run.

See also: [Test Development Journal](TEST_DEVELOPMENT_JOURNAL.md) and [Development Journal](DEV_JOURNAL.md).
