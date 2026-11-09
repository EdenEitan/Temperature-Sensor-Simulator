"""Python driver for the C++ temperature sensor simulator (Linux/WSL)."""

from dataclasses import dataclass
import math
import selectors
import subprocess
import time


class DeviceError(RuntimeError):
    """A device or transport error reported to the test code."""


@dataclass(frozen=True)
class Reading:
    temperature: float
    alarm: bool


@dataclass(frozen=True)
class DeviceStatus:
    fault: str
    samples: int


class Device:
    """Typed sensor methods; command() remains available for negative tests."""

    FAULT_MODES = frozenset({
        "NONE", "DISCONNECTED", "STUCK", "TIMEOUT", "COMMUNICATION_FAILURE"
    })

    def __init__(self, binary="./device_sim", timeout=0.5):
        if timeout <= 0:
            raise ValueError("timeout must be positive")

        self.timeout = timeout
        self._desynchronized = False

        self.process = subprocess.Popen(
            [str(binary)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            bufsize=0,
        )

        self.selector = selectors.DefaultSelector()
        self.selector.register(
            self.process.stdout,
            selectors.EVENT_READ
        )

    def command(self, command):
        if "\n" in command or "\r" in command:
            raise ValueError("one command per transaction")

        if self._desynchronized:
            raise DeviceError(
                "transport timed out; restart device before reuse"
            )

        if self.process.poll() is not None:
            raise DeviceError("device process exited")

        try:
            self.process.stdin.write(
                (command + "\n").encode("ascii")
            )
            self.process.stdin.flush()
        except (BrokenPipeError, OSError) as exc:
            raise DeviceError("device pipe closed") from exc

        deadline = time.monotonic() + self.timeout
        response = bytearray()

        while True:
            remaining = deadline - time.monotonic()

            if remaining <= 0 or not self.selector.select(remaining):
                self._desynchronized = True
                raise TimeoutError(
                    f"no complete reply to {command!r}"
                )

            # Single-byte reads prevent hidden read-ahead.
            char = self.process.stdout.read(1)

            if not char:
                raise DeviceError("device closed its output")

            if char == b"\n":
                break

            response.extend(char)

            if len(response) > 4096:
                self._desynchronized = True
                raise DeviceError("response exceeds protocol limit")

        try:
            reply = response.decode("ascii")
        except UnicodeDecodeError as exc:
            raise DeviceError(
                "device returned non-ASCII data"
            ) from exc

        if reply.startswith("ERR "):
            raise DeviceError(reply[4:])

        return reply

    def _expect_ok(self, command):
        reply = self.command(command)
        if reply != "OK":
            raise DeviceError(f"unexpected response: {reply!r}")
        return reply

    def _set_number(self, command, value):
        if (isinstance(value, bool) or not isinstance(value, (int, float))
                or not math.isfinite(value)):
            raise ValueError("value must be a finite number")
        # Let C++ validate ranges: it is the system under test.
        return self._expect_ok(f"{command} {value}")

    def set_temperature(self, value):
        """Set raw temperature (-40 to 125 Celsius)."""
        return self._set_number("SET", value)

    def set_calibration(self, offset):
        """Set calibration offset (-10 to 10 Celsius)."""
        return self._set_number("CALIBRATE", offset)

    def set_threshold(self, value):
        """Set alarm threshold (-40 to 125 Celsius)."""
        return self._set_number("THRESHOLD", value)

    def set_fault(self, mode):
        """Inject a sensor fault; communication failure needs process restart."""
        if not isinstance(mode, str) or mode not in self.FAULT_MODES:
            raise ValueError(f"unknown fault mode: {mode!r}")
        return self._expect_ok(f"FAULT {mode}")

    def ping(self):
        """Check that the device responds to health checks."""
        reply = self.command("PING")
        if reply != "PONG":
            raise DeviceError(f"unexpected response: {reply!r}")
        return True

    def status(self):
        """Return current sensor fault and successful reading count."""
        fields = self.command("STATUS").split()
        if (len(fields) != 3 or fields[0] != "STATUS"
                or fields[1] not in {"NONE", "DISCONNECTED", "STUCK", "TIMEOUT"}):
            raise DeviceError("invalid status response")
        try:
            samples = int(fields[2])
        except ValueError as exc:
            raise DeviceError("invalid sample count") from exc
        if samples < 0 or str(samples) != fields[2]:
            raise DeviceError("invalid sample count")
        return DeviceStatus(fault=fields[1], samples=samples)

    def reset(self):
        """Restore sensor defaults; a broken communication channel needs restart."""
        return self._expect_ok("RESET")

    def read(self):
        fields = self.command("READ").split()

        if (len(fields) != 3
                or fields[0] != "DATA"
                or fields[2] not in {"ALARM", "NORMAL"}):
            raise DeviceError("invalid reading response")

        try:
            temperature = float(fields[1])
        except ValueError as exc:
            raise DeviceError("invalid temperature") from exc

        if not math.isfinite(temperature):
            raise DeviceError("invalid temperature")

        return Reading(
            temperature=temperature,
            alarm=(fields[2] == "ALARM")
        )

    def close(self):
        self.selector.close()

        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=1)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()

        self.process.stdin.close()
        self.process.stdout.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()
