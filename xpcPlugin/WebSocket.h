#pragma once
#ifndef XPCPLUGIN_WEBSOCKET_H_
#define XPCPLUGIN_WEBSOCKET_H_

#include <cstdlib>
#include <string>
#include <thread>
#include <functional>

#define ASIO_STANDALONE 
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "ISocket.h"

typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

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
	class WebSocket : public ISocket {

	private:
		WebSocketServer _server;
		std::thread *_thread;

		std::string _received;

		

	public:

		WebSocket(unsigned short port);

		~WebSocket();

		/// Reads the specified number of bytes into the data buffer and stores
		/// the remote endpoint.
		///
		/// \param buffer	 The array to copy the data into.
		/// \param size	   The number of bytes to read.
		/// \param remoteAddr When at least one byte is read, contains the address
		///                   of the remote host.
		/// \returns		  The number of bytes read, or a negative number if
		///                   an error occurs.
		int Read(unsigned char* buffer, int size, sockaddr* remoteAddr);

		/// Sends data to the specified remote endpoint.
		///
		/// \param data   The data to be sent.
		/// \param len    The number of bytes to send.
		/// \param remote The destination socket.
		void SendTo(const unsigned char* buffer, std::size_t len, sockaddr* remote) const;
	};
}
#endif