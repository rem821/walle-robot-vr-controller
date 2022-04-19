#include <string>
#include <sys/socket.h>
#include <linux/in.h>
#include <sys/endian.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <android/log.h>

#include "Framework_VrInput.h"
#include "UDPSocket.h"

#define DEBUG 1
#define OVR_LOG_TAG "WalleVrController"

#if !defined(ALOGE)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#endif

#if !defined(ALOGV)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif
#endif


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

unsigned long
sendUDPPacket(int socket, ovrPoseInput *poseInput) {
    std::string hostname{"192.168.1.239"};
    uint16_t port = 5005;

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);
    destination.sin_addr.s_addr = inet_addr(hostname.c_str());

    // Motors
    int left = (int) (poseInput->leftY * 1000);
    int right = (int) (poseInput->rightY * 1000);

    int leftDir = 0;
    if (left >= 0) {
        leftDir = 1;
    }
    int rightDir = 0;
    if (right >= 0) {
        rightDir = 1;
    }

    //Servos
    float vertical = poseInput->headsetVertical;
    if (vertical < -M_PI_2) {
        vertical = -M_PI_2;
    }

    if (vertical > 0) {
        vertical = 0;
    }

    float horizontal = poseInput->headsetHorizontal;
    if (horizontal < -M_PI_2) {
        horizontal = -M_PI_2;
        vertical = 0;
    }

    if (horizontal > M_PI_2) {
        horizontal = M_PI_2;
        vertical = 0;
    }

    int s0 = (int) abs((((vertical / M_PI_2)) * 180.0f) - 0.0f); // Vertical axis
    int s1 = (int) abs(((1.0f + (horizontal / M_PI_2)) * 90.0f)); // Horizontal axis

    ALOGV("Head: H:%d, V:%d, Motors: L:%d, R:%d", s1, s0, left, right);
    std::string message = string_format("s0:%03d,s1:%03d,m0:%04d,%01d,m1:%04d,%01d\n", abs(s0),
                                        abs(s1), abs(left),
                                        leftDir, abs(right), rightDir);

    size_t n_bytes = sendto(socket, message.c_str(), message.length(), 0,
                            reinterpret_cast<sockaddr *>(&destination), sizeof(destination));

    return n_bytes;
}

