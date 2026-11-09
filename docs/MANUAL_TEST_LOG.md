

## C++ Simulator — Manual Validation

**Build:** CMake / C++17 
'''bash 
cmake -S . -B build
cmake --build build
'''
**Execute**
'''bash 
./build/device_sim
'''
**Environment:** Ubuntu (WSL)


### 1.Test scenarios for routine operation (no fault):

1. Successful startup and PING response
2. Default sensor state
3. Valid temperature update
4. Alarm activation at threshold
5. Out-of-range input rejection
6. State preservation after invalid input
7. Sensor reset
8. Default reading after reset

### Expected vs. Actual Results:

| Command   | Expected Response | 
|-----------|-------------------|
| PING      | PONG              | 
| STATUS    | STATUS NONE 0|                | 
| High Temp | 85    | 
|SET 80|OK|
|READ|DATA 80.00 ALARM|
|SET 200|ERR OUT_OF_RANGE|
|READ|DATA 80.00 ALARM|
|RESET|OK|
|STATUS| STATUS NONE 0|
|READ | DATA 25.00 NORMAL|

Result: PASS — 9/9 expected responses matched.

### 2.Test scenarios for Fault Injection:


| Command   | Expected Response |
|FAULT STUCK| OK |
|SET 100| OK |
|READ| DATA 25.00 NORMAL | 
|STATUS| STATUS STUCK 1 | 
|FAULT NONE|  OK | 
|READ| DATA 100.00 ALARM| 
|FAULT DISCONNECTED| OK| 
|READ| ERR DISCONNECTED | 
|FAULT TIMEOUT| OK | 
|READ| -no response | 
|STATUS| STATUS TIMEOUT 2| 
|FAULT NONE| OK | 
|PING| PONG |

## Extending Fault Injection — Communication Failure

While manually testing the simulator's fault modes, I realized that the existing implementation only covered sensor-level failures.

For example, TIMEOUT simulates a sensor that does not return a measurement, but the simulator itself remains responsive to commands such as PING and STATUS.

I wanted to distinguish between two different failure scenarios:

--Sensor failure: The device is reachable, but the sensor cannot provide a valid measurement.

--Communication failure: The device stops responding entirely, including to health-check commands such as PING.

This distinction is important because a failed measurement does not necessarily mean that communication with the device has failed. The testing framework should be able to detect these situations separately.

I decided to introduce a new fault mode, COMMUNICATION_FAILURE, at the communication layer in main.cpp, rather than inside the Sensor class.

The intended behavior is that activating this mode returns OK, after which the simulator stops responding to commands until the process is restarted.

This would allow the Python driver to test communication timeouts independently of sensor measurement failures.

Test — Communication Failure Injection

Objective: Verify that the new COMMUNICATION_FAILURE mode can be activated and causes the simulator to stop responding to commands.

Environment: Ubuntu (WSL)
Executable: build/device_sim

Expected Behavior:

PING returns PONG.

FAULT COMMUNICATION_FAILURE returns OK.

Subsequent commands, including PING, receive no response.

Normal communication is restored after restarting the simulator.

Test Run #1 — Failed

Input and actual output:

PING
PONG
FAULT COMMUNICATION_FAILURE
ERR BAD_ARGUMENT

Result: FAIL

Observation:

The simulator responded correctly to PING, but rejected the new fault mode with ERR BAD_ARGUMENT.

This indicates that the running executable did not recognize COMMUNICATION_FAILURE as a valid command argument.

**causes**:** the updated source code was not rebuilt before running the executable.


Test Run #1 — PASS 

Input and actual output:

PING 
PONG
FAULT COMMUNICATION_FAILURE
OK
PING


## Python Driver — Initial Integration Test

This test checks basic communication between the Python
driver and the C++ simulator, including command execution,
response parsing, calibration, and process lifecycle.

**Script:** `scripts/try_driver.py`
**Environment:** Ubuntu (WSL)
**Executable:** `device_sim`

*result*:
 >PASS

### Test Scenario

1. Launch the C++ simulator from Python.
2. Send `PING` and verify the response.
3. Set the temperature to 77°C.
4. Apply a calibration offset of +3°C.
5. Read the calibrated temperature and alarm status.
6. Request the sensor status.
7. Close the simulator process.

### Expected Output
```text
PING: PONG
Temperature: 80.0
Alarm: True
STATUS NONE 1
```

### Actual Output
```text
PONG
Temperature: 80.0
Alarm: True
STATUS NONE 1
``` 



