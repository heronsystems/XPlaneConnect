#pragma once
#ifndef XPCPLUGIN_HTTPSERVER_H_
#define XPCPLUGIN_HTTPSERVER_H_

#include <cstdlib>
#include <string>
#include <thread>

#include "httplib.h"

using namespace httplib;

namespace XPC
{
	/// Represents a UDP datagram socket used for reading data from and sending
	/// data to XPC clients.
	///
	/// \author Jason Watkins
	/// \version 1.1
	/// \since 1.0
	/// \date Intial Version: 2015-04-10
	/// \date Last Updated: 2015-05-11
	class HTTPServer
	{
	public:
		/// Initializes a new instance of the XPCSocket class bound to the
		/// specified receive port.
		///
		/// \param recvPort The port on which this instance will receive data.
		explicit HTTPServer(unsigned short recvPort, unsigned short xpSocketPort);

		/// Closes the underlying socket for this instance.
		~HTTPServer();


	private:
		Server *_srv;
		std::thread *_thread;
	};
}
#endif

