#include "port.h"

#include <cstdint>
#include <string>
#include <arpa/inet.h>

namespace HTTPServer {

Port::Port(int value) : d_value(value) {}

Port::operator int() const {
    return d_value;
}

int Port::value() const {
    return d_value;
}

std::string Port::toString() const {
    return std::to_string(d_value);
}

uint16_t Port::toNetwork() const {
    return htons(d_value);
}

} // namespace HTTPServer