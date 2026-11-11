"""Shared fixtures for C++ simulator integration tests."""

import os
import subprocess
from pathlib import Path

import pytest

from framework.device import Device


ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="session")
def binary():
    """Build the C++ executable only once per pytest session."""
    override = os.environ.get("DEVICE_BINARY")
    if override:
        executable = Path(override).resolve()
        if not executable.is_file():
            pytest.fail(f"DEVICE_BINARY not found: {executable}")
        return executable

    build_dir = ROOT / "build"
    subprocess.run(["cmake", "-S", str(ROOT), "-B", str(build_dir)], check=True)
    subprocess.run(["cmake", "--build", str(build_dir), "--parallel", "2"], check=True)

    executable = build_dir / "device_sim"
    if not executable.is_file():
        pytest.fail(f"C++ executable was not created: {executable}")
    return executable


@pytest.fixture
def device(binary):
    """Launch a fresh simulator process for every test."""
    with Device(binary) as instance:
        yield instance
