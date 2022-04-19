
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC int createSocket();

EXTERNC void closeSocket(int socket);

EXTERNC unsigned long sendUDPPacket(int socket, ovrPoseInput *poseInput);

#undef EXTERNC
