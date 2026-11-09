#include "Protocol.h"
#include "test_support.h"

#include <string>

namespace {
std::string response(CommandProtocol& protocol, const std::string& command) {
    const auto result = protocol.handle(command);
    CHECK(result.has_value());
    return *result;
}
}

int main() {
    return test_support::run({
        {"health check and default status", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "PING") == "PONG");
            CHECK(response(protocol, "STATUS") == "STATUS NONE 0");
            CHECK(response(protocol, "READ") == "DATA 25.00 NORMAL");
            CHECK(response(protocol, "STATUS") == "STATUS NONE 1");
        }},
        {"set, calibration, threshold and formatted reading", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "SET 77") == "OK");
            CHECK(response(protocol, "CALIBRATE 3") == "OK");
            CHECK(response(protocol, "THRESHOLD 80") == "OK");
            CHECK(response(protocol, "READ") == "DATA 80.00 ALARM");
            CHECK(response(protocol, "THRESHOLD 81") == "OK");
            CHECK(response(protocol, "READ") == "DATA 80.00 NORMAL");
        }},
        {"valid boundary values", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "SET -40") == "OK");
            CHECK(response(protocol, "CALIBRATE -10") == "OK");
            CHECK(response(protocol, "THRESHOLD -40") == "OK");
            CHECK(response(protocol, "READ") == "DATA -50.00 NORMAL");
            CHECK(response(protocol, "SET 125") == "OK");
            CHECK(response(protocol, "CALIBRATE 10") == "OK");
            CHECK(response(protocol, "THRESHOLD 125") == "OK");
            CHECK(response(protocol, "READ") == "DATA 135.00 ALARM");
        }},
        {"out-of-range setters preserve reading", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "SET 42") == "OK");
            CHECK(response(protocol, "SET 125.01") == "ERR OUT_OF_RANGE");
            CHECK(response(protocol, "CALIBRATE -10.01") == "ERR OUT_OF_RANGE");
            CHECK(response(protocol, "THRESHOLD 125.01") == "ERR OUT_OF_RANGE");
            CHECK(response(protocol, "READ") == "DATA 42.00 NORMAL");
        }},
        {"malformed numeric commands", [] {
            CommandProtocol protocol;
            for (const auto& input : {"SET", "SET abc", "SET nan", "SET inf",
                                      "SET 1e999", "SET 1 trailing",
                                      "CALIBRATE", "THRESHOLD junk"}) {
                CHECK(response(protocol, input) == "ERR BAD_ARGUMENT");
            }
            CHECK(response(protocol, "PING") == "PONG");
        }},
        {"missing or extra arguments", [] {
            CommandProtocol protocol;
            for (const auto& input : {"PING extra", "READ extra", "STATUS 2",
                                      "RESET extra", "FAULT", "FAULT NONE extra"}) {
                CHECK(response(protocol, input) == "ERR BAD_ARGUMENT");
            }
        }},
        {"unknown and empty commands", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "BOGUS") == "ERR UNKNOWN_COMMAND");
            CHECK(response(protocol, "") == "ERR UNKNOWN_COMMAND");
            CHECK(response(protocol, "  ") == "ERR UNKNOWN_COMMAND");
        }},
        {"invalid fault is rejected", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "FAULT UNKNOWN") == "ERR BAD_ARGUMENT");
            CHECK(response(protocol, "STATUS") == "STATUS NONE 0");
        }},
        {"disconnect and recovery", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "FAULT DISCONNECTED") == "OK");
            CHECK(response(protocol, "READ") == "ERR DISCONNECTED");
            CHECK(response(protocol, "STATUS") == "STATUS DISCONNECTED 0");
            CHECK(response(protocol, "FAULT NONE") == "OK");
            CHECK(response(protocol, "READ") == "DATA 25.00 NORMAL");
            CHECK(response(protocol, "STATUS") == "STATUS NONE 1");
        }},
        {"stuck readings use fixed raw value then recover", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "SET 70") == "OK");
            CHECK(response(protocol, "CALIBRATE 2") == "OK");
            CHECK(response(protocol, "FAULT STUCK") == "OK");
            CHECK(response(protocol, "READ") == "DATA 27.00 NORMAL");
            CHECK(response(protocol, "FAULT NONE") == "OK");
            CHECK(response(protocol, "READ") == "DATA 72.00 NORMAL");
        }},
        {"timeout silences read only", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "FAULT TIMEOUT") == "OK");
            CHECK(!protocol.handle("READ").has_value());
            CHECK(response(protocol, "PING") == "PONG");
            CHECK(response(protocol, "STATUS") == "STATUS TIMEOUT 0");
            CHECK(response(protocol, "FAULT NONE") == "OK");
            CHECK(response(protocol, "READ") == "DATA 25.00 NORMAL");
        }},
        {"communication failure permanently silences requests", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "FAULT COMMUNICATION_FAILURE") == "OK");
            for (const auto& input : {"PING", "READ", "STATUS", "RESET", "FAULT NONE"}) {
                CHECK(!protocol.handle(input).has_value());
            }
        }},
        {"reset restores protocol sensor state", [] {
            CommandProtocol protocol;
            CHECK(response(protocol, "SET 95") == "OK");
            CHECK(response(protocol, "CALIBRATE 5") == "OK");
            CHECK(response(protocol, "READ") == "DATA 100.00 ALARM");
            CHECK(response(protocol, "FAULT STUCK") == "OK");
            CHECK(response(protocol, "RESET") == "OK");
            CHECK(response(protocol, "STATUS") == "STATUS NONE 0");
            CHECK(response(protocol, "READ") == "DATA 25.00 NORMAL");
        }}
    });
}
