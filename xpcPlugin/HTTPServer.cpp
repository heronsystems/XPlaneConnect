#include "HTTPServer.h"

#include "Log.h"

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

namespace XPC
{
	const static std::string tag = "HTTPSRV";

	HTTPServer::HTTPServer(unsigned short recvPort, unsigned short xpSocketPort)
	{

		
		_srv = NULL;
		
		_thread = new std::thread([this, recvPort, xpSocketPort]() {
			

#ifdef _WIN32
			WSADATA              wsaData;
			SOCKET               SendingSocket;
			// Server/receiver address
			SOCKADDR_IN          ServerAddr, ThisSenderInfo;
			int  RetCode;

			int BytesSent, nlen;



			// Initialize Winsock version 2.2
			WSAStartup(MAKEWORD(2, 2), &wsaData);
			Log::FormatLine(LOG_INFO, tag, "Client: Winsock DLL status is %s.\n", wsaData.szSystemStatus);

			// Create a new socket to make a client connection.
			// AF_INET = 2, The Internet Protocol version 4 (IPv4) address family, TCP protocol
			SendingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (SendingSocket == INVALID_SOCKET)
			{
				Log::FormatLine(LOG_INFO, tag, "Client: socket() failed! Error code: %ld\n", WSAGetLastError());

				// Do the clean up
				WSACleanup();

				// Exit with error
				return -1;
			}
			else
				Log::FormatLine(LOG_INFO, tag, "Client: socket() is OK!\n");



			// Set up a SOCKADDR_IN structure that will be used to connect
			// to a listening server on port 5150. For demonstration
			// purposes, let's assume our server's IP address is 127.0.0.1 or localhost
			ServerAddr.sin_family = AF_INET;
			ServerAddr.sin_port = htons(xpSocketPort);
			ServerAddr.sin_addr.s_addr = inet_addr("localhost");
			Log::FormatLine(LOG_INFO, tag, "Establishing connection to XPlane socket port %d", xpSocketPort);
			RetCode = connect(SendingSocket, (SOCKADDR *)&ServerAddr, sizeof(ServerAddr));
			if (RetCode != 0)
			{
				Log::FormatLine(LOG_INFO, tag, "Client: connect() failed! Error code: %ld\n", WSAGetLastError());

				// Close the socket
				closesocket(SendingSocket);

				// Do the clean up
				WSACleanup();

				// Exit with error
				return -1;
			}

#else
			int sock;
			throw "Not Implimented!"
#endif

			_srv = new Server();
			
			_srv->Post("/Get", [&](const Request& req, Response& res) {
				auto data = req.body;

				res.set_content("Hello World!", "text/plain");
				//const char* buff = "GETD\x00\x01\x13sim/test/test_float";
				Log::FormatLine(LOG_INFO, tag, "Received %s", data.c_str());
				send(SendingSocket, data.c_str(), data.length(), 0);

				char readBuff[1];
				int iResult = recv(SendingSocket, readBuff, 1, 0);
				if (iResult > 0)
					Log::FormatLine(LOG_INFO, tag, "Bytes received: %d\n", iResult);
				else if (iResult == 0)
					Log::FormatLine(LOG_INFO, tag, "Connection closed\n");
				else
					Log::FormatLine(LOG_INFO, tag, "recv failed: %d\n", WSAGetLastError());
			});

			_srv->Post("/Set", [&](const Request& req, Response& res) {
				auto data = req.body;

				res.set_content("Hello World!", "text/plain");
				//const char* buff = "GETD\x00\x01\x13sim/test/test_float";
				Log::FormatLine(LOG_INFO, tag, "Received %s", data.c_str());
				send(SendingSocket, data.c_str(), data.length(), 0);
			});

			Log::FormatLine(LOG_INFO, tag, "Starting HTTP Server on %d", recvPort);
			_srv->listen("0.0.0.0", recvPort);
			Log::FormatLine(LOG_INFO, tag, "Finished Listening", recvPort);
		});
	}

	HTTPServer::~HTTPServer()
	{
		Log::FormatLine(LOG_TRACE, tag, "Stopping HTTP Server");
		if(_srv != NULL)
			_srv->stop();

		_thread->join();

		if (_srv != NULL)
			delete _srv;

		delete _thread;
	}
}