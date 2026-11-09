
#include "Sensor.h"
#include <cmath>


bool Sensor::setTemperature(double value) {
    if (!std::isfinite(value) || value < -40.0 || value > 125.0) {
        return false;
    }

    temperature = value;
    return true;
}

bool Sensor::setCalibration(double value) {
    if (!std::isfinite(value) || value < -10.0 || value > 10.0) {
        return false;
    }

    offset = value;
    return true;
}

bool Sensor::setThreshold(double value) {
    if (!std::isfinite(value) || value < -40.0 || value > 125.0) {
        return false;
    }

    threshold = value;
    return true;
}

ReadResult Sensor::read() {

    //troubleshooting
    if (fault == Fault::TIMEOUT) {
        return {ReadStatus::TIMEOUT, 0.0, false};
    }

    if (fault == Fault::DISCONNECTED) {
        return {ReadStatus::DISCONNECTED, 0.0, false};
    }

    const double raw = (fault == Fault::STUCK) ? 25.0 : temperature;
    const double calibrated = raw + offset;
    const bool alarm = calibrated >= threshold;

    ++samples;

    return {ReadStatus::OK, calibrated, alarm};
}

void Sensor::setFault(Fault mode) {
    fault = mode;
}

Fault Sensor::getFault() const {
    return fault;
}

unsigned int Sensor::getSamples() const {
    return samples;
}

void Sensor::reset() {
    *this = Sensor{};
}
