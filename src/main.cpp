#include "Protocol.h"

#include <iostream>
#include <string>

int main() {
    CommandProtocol protocol;
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto reply = protocol.handle(line);
        if (reply) {
            std::cout << *reply << '\n' << std::flush;
        }
    }
    return 0;
}
