
#include "Sensor.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>


//ensures the command contains exactly one valid number
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

int main() {

    Sensor sensor;
    std::string line;
    bool communicationFailure = false;

    std::cout << std::fixed << std::setprecision(2);

    while (std::getline(std::cin, line)) {

        std::istringstream input(line);
        std::string command;
        input >> command;

        if (communicationFailure) {
            continue;
        }

        //handling commands that receive number
        if (command == "SET" || command == "CALIBRATE" || command == "THRESHOLD") {
            double value;
            if (!parseNumber(input, value)) { //If the input is invalid:
                std::cout << "ERR BAD_ARGUMENT\n";
            } else { //
                bool success = false;
                if (command == "SET") success = sensor.setTemperature(value);
                else if (command == "CALIBRATE") success = sensor.setCalibration(value);
                else success = sensor.setThreshold(value);

                std::cout << (success ? "OK\n" : "ERR OUT_OF_RANGE\n");
            }

        } else if (command == "FAULT") {

            //Handling "FAULT" command
            std::string faultName;
            std::string extra;
            Fault mode;

            if (!(input >> faultName) || (input >> extra)) {
                std::cout << "ERR BAD_ARGUMENT\n";
            } else if (faultName == "COMMUNICATION_FAILURE") {
                communicationFailure = true;
                std::cout << "OK\n";
            } else if (!parseFault(faultName, mode)) {
                std::cout << "ERR BAD_ARGUMENT\n";
            } else {
                sensor.setFault(mode);
                std::cout << "OK\n";
            }

        } else if (command == "READ" || command == "STATUS" ||
                   command == "RESET" || command == "PING") {

            //Handling commands that dont accept arguments
            std::string extra;

            //These commands should not take any additional values,
            //check that no information remains after the command name:
            if (input >> extra) { //
                std::cout << "ERR BAD_ARGUMENT\n";
            } else if (command == "RESET") {
                sensor.reset();
                std::cout << "OK\n";
            } else if (command == "PING") {
                std::cout << "PONG\n";
            } else if (command == "STATUS") {
                std::cout << "STATUS " << faultToString(sensor.getFault())
                          << ' ' << sensor.getSamples() << '\n';
            } else {

                //Handling READ command
                //sensor is disconnected -> send "ERR DISCONNECTED"
                //successful measurement -> send temperature + alarm status
                ReadResult result = sensor.read();

                if (result.status == ReadStatus::DISCONNECTED) {
                    std::cout << "ERR DISCONNECTED\n";
                } else if (result.status == ReadStatus::OK) {
                    std::cout << "DATA " << result.temperature << ' '
                              << (result.alarm ? "ALARM" : "NORMAL") << '\n';
                }

                //TIMEOUT: deliberately do not send a response.
                //the Python driver responsible to detect this
            }

        } else {
            std::cout << "ERR UNKNOWN_COMMAND\n";
        }

        std::cout.flush();
    }

    return 0;
}
