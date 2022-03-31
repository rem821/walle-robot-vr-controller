#include <string>
#include <sys/socket.h>
#include <linux/in.h>
#include <sys/endian.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "UDPSocket.h"

template<typename ... Args>
std::string string_format(const std::string &format, Args ... args) {
    int size_s =
            std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
    auto size = static_cast<size_t>( size_s );
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

int createSocket() {
    return socket(AF_INET, SOCK_DGRAM, 0);
}

void closeSocket(int socket) {
    close(socket);
}

unsigned long sendUDPPacket(int socket, float leftX, float leftY, float rightX, float rightY) {
    std::string hostname{"192.168.1.239"};
    uint16_t port = 5005;

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);
    destination.sin_addr.s_addr = inet_addr(hostname.c_str());

    // Motors
    int left = (int) (leftY * 1000);
    int right = (int) (rightY * 1000);

    int leftDir = 0;
    if (left >= 0) {
        leftDir = 1;
    }
    int rightDir = 0;
    if (right >= 0) {
        rightDir = 1;
    }

    //Servos
    int s0 = (int)((1 + rightX) * 70);; // Middle connector
    int s1 = (int)((1 + leftX) * 90); // Right connector (by the voltage input)

    std::string message = string_format("s0:%03d,s1:%03d,m0:%04d,%01d,m1:%04d,%01d\n", abs(s0), abs(s1), abs(left),
                                        leftDir, abs(right), rightDir);

    size_t n_bytes = sendto(socket, message.c_str(), message.length(), 0,
                            reinterpret_cast<sockaddr *>(&destination), sizeof(destination));

    return n_bytes;
}

