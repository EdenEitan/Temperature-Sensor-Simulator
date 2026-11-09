#include "Protocol.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace {
bool parseNumber(std::istringstream& input, double& value) {
    std::string extra;
    return (input >> value) && std::isfinite(value) && !(input >> extra);
}

bool parseFault(const std::string& text, Fault& mode) {
    if (text == "NONE") mode = Fault::NONE;
    else if (text == "DISCONNECTED") mode = Fault::DISCONNECTED;
    else if (text == "STUCK") mode = Fault::STUCK;
    else if (text == "TIMEOUT") mode = Fault::TIMEOUT;
    else return false;
    return true;
}

std::string faultToString(Fault mode) {
    switch (mode) {
        case Fault::NONE: return "NONE";
        case Fault::DISCONNECTED: return "DISCONNECTED";
        case Fault::STUCK: return "STUCK";
        case Fault::TIMEOUT: return "TIMEOUT";
    }
    return "UNKNOWN";
}
}  // namespace

std::optional<std::string> CommandProtocol::handle(const std::string& line) {
    if (communication_failure_) return std::nullopt;

    std::istringstream input(line);
    std::string command;
    input >> command;

    if (command == "SET" || command == "CALIBRATE" || command == "THRESHOLD") {
        double value;
        if (!parseNumber(input, value)) return "ERR BAD_ARGUMENT";

        bool success = false;
        if (command == "SET") success = sensor_.setTemperature(value);
        else if (command == "CALIBRATE") success = sensor_.setCalibration(value);
        else success = sensor_.setThreshold(value);

        return success ? "OK" : "ERR OUT_OF_RANGE";
    }

    if (command == "FAULT") {
        std::string faultName;
        std::string extra;
        Fault mode;
        if (!(input >> faultName) || (input >> extra)) return "ERR BAD_ARGUMENT";
        if (faultName == "COMMUNICATION_FAILURE") {
            communication_failure_ = true;
            return "OK";
        }
        if (!parseFault(faultName, mode)) return "ERR BAD_ARGUMENT";
        sensor_.setFault(mode);
        return "OK";
    }

    if (command == "READ" || command == "STATUS" ||
        command == "RESET" || command == "PING") {
        std::string extra;
        if (input >> extra) return "ERR BAD_ARGUMENT";

        if (command == "RESET") {
            sensor_.reset();
            return "OK";
        }
        if (command == "PING") return "PONG";
        if (command == "STATUS") {
            return "STATUS " + faultToString(sensor_.getFault()) +
                   " " + std::to_string(sensor_.getSamples());
        }

        const ReadResult result = sensor_.read();
        if (result.status == ReadStatus::DISCONNECTED) return "ERR DISCONNECTED";
        if (result.status == ReadStatus::TIMEOUT) return std::nullopt;

        std::ostringstream reply;
        reply << "DATA " << std::fixed << std::setprecision(2)
              << result.temperature << ' '
              << (result.alarm ? "ALARM" : "NORMAL");
        return reply.str();
    }

    return "ERR UNKNOWN_COMMAND";
}
