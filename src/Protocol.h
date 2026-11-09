#pragma once

#include "Sensor.h"

#include <optional>
#include <string>

// Converts one newline-delimited ASCII request into a response.
// std::nullopt indicates a deliberately suppressed response (fault injection).
// Keeping protocol parsing here separates I/O from the Sensor's domain logic.
class CommandProtocol {
public:
    std::optional<std::string> handle(const std::string& line);

private:
    Sensor sensor_;
    bool communication_failure_ = false;
};
