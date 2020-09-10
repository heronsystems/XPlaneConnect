#ifndef XPCPLUGIN_ISOCKET_H_
#define XPCPLUGIN_ISOCKET_H_

#include <cstdlib>
#include <string>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // Winsock Library
#elif (__APPLE__ || __linux)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

class ISocket {
public:
	virtual void SendTo(const unsigned char* buffer, std::size_t len, sockaddr* remote) const = 0;

	virtual int Read(unsigned char* buffer, int size, sockaddr* remoteAddr) = 0;
};

#endif