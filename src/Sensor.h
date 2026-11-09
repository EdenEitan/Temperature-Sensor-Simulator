#pragma once

enum class Fault {
    NONE,
    DISCONNECTED,
    STUCK,
    TIMEOUT
};

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

class Sensor {
private:
    double temperature = 25.0;
    double offset = 0.0;
    double threshold = 80.0;
    Fault fault = Fault::NONE;
    unsigned int samples = 0;

public:
    // Directly set the raw reading; valid range: [-40, 125].
    bool setTemperature(double value);

    // Valid range: [-10, 10].
    bool setCalibration(double value);

    // Valid range: [-40, 125].
    bool setThreshold(double value);

    // STUCK returns a raw value of 25C; only successful reads count.
    ReadResult read();

    void setFault(Fault mode);
    Fault getFault() const;
    unsigned int getSamples() const;
    void reset();
};
