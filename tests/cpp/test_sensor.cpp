#include "Sensor.h"
#include "test_support.h"

#include <limits>

int main() {
    return test_support::run({
        {"defaults and successful-read counter", [] {
            Sensor sensor;
            CHECK(sensor.getFault() == Fault::NONE);
            CHECK(sensor.getSamples() == 0);
            const auto result = sensor.read();
            CHECK(result.status == ReadStatus::OK);
            CHECK_NEAR(result.temperature, 25.0);
            CHECK(!result.alarm);
            CHECK(sensor.getSamples() == 1);
        }},
        {"valid raw-temperature boundaries", [] {
            Sensor sensor;
            CHECK(sensor.setTemperature(-40));
            CHECK_NEAR(sensor.read().temperature, -40);
            CHECK(sensor.setTemperature(125));
            CHECK_NEAR(sensor.read().temperature, 125);
            CHECK(sensor.getSamples() == 2);
        }},
        {"invalid temperature is atomic", [] {
            Sensor sensor;
            CHECK(sensor.setTemperature(42));
            CHECK(!sensor.setTemperature(-40.01));
            CHECK(!sensor.setTemperature(125.01));
            CHECK_NEAR(sensor.read().temperature, 42);
        }},
        {"non-finite numbers are rejected", [] {
            Sensor sensor;
            const double nan = std::numeric_limits<double>::quiet_NaN();
            const double inf = std::numeric_limits<double>::infinity();
            CHECK(!sensor.setTemperature(nan));
            CHECK(!sensor.setCalibration(inf));
            CHECK(!sensor.setThreshold(-inf));
            CHECK_NEAR(sensor.read().temperature, 25);
            CHECK(!sensor.read().alarm);
        }},
        {"calibration boundaries and atomic rejection", [] {
            Sensor sensor;
            CHECK(sensor.setCalibration(-10));
            CHECK_NEAR(sensor.read().temperature, 15);
            CHECK(sensor.setCalibration(10));
            CHECK(!sensor.setCalibration(10.01));
            CHECK(!sensor.setCalibration(-10.01));
            CHECK_NEAR(sensor.read().temperature, 35);
        }},
        {"alarm is inclusive at threshold", [] {
            Sensor sensor;
            CHECK(sensor.setTemperature(79.99));
            CHECK(!sensor.read().alarm);
            CHECK(sensor.setTemperature(80));
            CHECK(sensor.read().alarm);
            CHECK(sensor.setTemperature(80.01));
            CHECK(sensor.read().alarm);
            CHECK(sensor.setThreshold(80.02));
            CHECK(!sensor.read().alarm);
        }},
        {"calibration changes alarm; invalid threshold is atomic", [] {
            Sensor sensor;
            CHECK(sensor.setTemperature(77));
            CHECK(sensor.setCalibration(3));
            CHECK(sensor.read().alarm);
            CHECK(sensor.setThreshold(81));
            CHECK(!sensor.read().alarm);
            CHECK(!sensor.setThreshold(125.01));
            CHECK(!sensor.read().alarm);
        }},
        {"disconnection does not count reads and recovers", [] {
            Sensor sensor;
            sensor.setFault(Fault::DISCONNECTED);
            CHECK(sensor.getFault() == Fault::DISCONNECTED);
            CHECK(sensor.read().status == ReadStatus::DISCONNECTED);
            CHECK(sensor.getSamples() == 0);
            sensor.setFault(Fault::NONE);
            CHECK(sensor.read().status == ReadStatus::OK);
            CHECK(sensor.getSamples() == 1);
        }},
        {"timeout does not count reads and recovers", [] {
            Sensor sensor;
            sensor.setFault(Fault::TIMEOUT);
            CHECK(sensor.read().status == ReadStatus::TIMEOUT);
            CHECK(sensor.getSamples() == 0);
            sensor.setFault(Fault::NONE);
            CHECK(sensor.read().status == ReadStatus::OK);
            CHECK(sensor.getSamples() == 1);
        }},
        {"stuck fault freezes raw input but preserves calibration", [] {
            Sensor sensor;
            CHECK(sensor.setTemperature(70));
            CHECK(sensor.setCalibration(2));
            sensor.setFault(Fault::STUCK);
            CHECK_NEAR(sensor.read().temperature, 27);
            CHECK(sensor.setTemperature(40));
            CHECK_NEAR(sensor.read().temperature, 27);
            sensor.setFault(Fault::NONE);
            CHECK_NEAR(sensor.read().temperature, 42);
            CHECK(sensor.getSamples() == 3);
        }},
        {"reset restores entire state", [] {
            Sensor sensor;
            CHECK(sensor.setTemperature(100));
            CHECK(sensor.setCalibration(5));
            CHECK(sensor.setThreshold(10));
            CHECK(sensor.read().alarm);
            sensor.setFault(Fault::STUCK);
            sensor.reset();
            CHECK(sensor.getFault() == Fault::NONE);
            CHECK(sensor.getSamples() == 0);
            const auto result = sensor.read();
            CHECK_NEAR(result.temperature, 25);
            CHECK(!result.alarm);
            CHECK(sensor.getSamples() == 1);
        }}
    });
}
