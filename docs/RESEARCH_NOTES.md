# Research notes: choosing the simulator's scope

## Starting question

I began this project after studying pytest, automation tools and frameworks, testing methodologies, and test-case design, with the goal of exploring Software Test Development Engineering.

I designed and wrote the sensor implementation and its header myself. For background on what to model, I asked Gemini for an explanation before writing the sensor core:

> I want to build a temperature sensor simulation in C++. Describe what a temperature sensor is supposed to do and what its function is. Don't build it for me; part of the goal is to try building it myself.

The request was for background information, not sensor code. I used ChatGPT later for the separate C++–Python communication layer, as described in the [development journal](DEV_JOURNAL.md).

These are the concepts I took from that exploration and how they affected the project. The explanation was a starting point for learning, not a hardware datasheet or a specification for a particular sensor.

## From concepts to decisions

| Concept explored | Decision for this project |
| --- | --- |
| Raw measurement and calibration | Store an explicit raw temperature and add a configurable offset when reading. |
| Threshold alerts | Calculate a boolean alarm from the calibrated value using an inclusive threshold. |
| Thermal response | Keep it out of the baseline; preserve an explicit-time model as a [next step](NEXT_STEPS.md#deterministic-thermal-response). |
| Noise and quantization | Leave them out of the baseline so exact boundary inputs remain reproducible. If noise is added, use a controllable seed. |
| Disconnection, frozen readings, missing replies | Give these separate fault behaviors, with different expectations for data and recovery. |
| Conversion timing and power modes | Do not model sleep states, busy flags, or physical conversion delays in this version. |
| ADC and electrical signals | Use Celsius directly; no simulated register map, voltage conversion, or raw ADC interface. |

## Model choices that need to be explicit

The numeric limits are simulation rules. They are not attributed to a specific physical device.

The project rejects out-of-range raw input instead of silently clamping it. Calibration is additive and the result is not clamped: raw 125 plus offset 10 produces 135. A disconnected read returns an explicit error rather than a sentinel temperature. A stuck reading uses fixed raw 25, rather than freezing the last successful reading.

These choices make expectations clear and testable. They should not be read as universal descriptions of hardware behavior.

## What I carried forward

The useful outcome of the research was deciding what to leave out as well as what to implement. I could build and test a small deterministic model first, while retaining the environment/measurement distinction for a later thermal extension.

The proposed thermal and concurrent-server designs are documented in [Next steps](NEXT_STEPS.md). The implementation decisions and learning progression are in the [development journal](DEV_JOURNAL.md).
