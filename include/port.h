#ifndef PORT_H
#define PORT_H

#include <cstdint> 

namespace HTTPServer {

class Port {
    public:
        explicit Port(int value);
        explicit operator int() const;

        int value() const;
        uint16_t toNetwork() const;

    private:
        int d_value;
};

} // namespace HTTPServer

#endif