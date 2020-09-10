#include "WebSocket.h"
#include "Log.h"

#include <mutex>


namespace XPC
{
	const static std::string tag = "WSSRV";

	std::mutex g_buffer_mutex;

	WebSocket::WebSocket(unsigned short port) {

		_received = "";

		// Set logging settings
		_server.set_error_channels(websocketpp::log::elevel::all);
		_server.set_access_channels(websocketpp::log::alevel::all ^ websocketpp::log::alevel::frame_payload);

		// Initialize Asio
		_server.init_asio();

		_thread = new std::thread([this, port]() {

			std::function<void(websocketpp::connection_hdl, std::shared_ptr<websocketpp::message_buffer::message<websocketpp::message_buffer::alloc::con_msg_manager>>)> lambda =
				[this](websocketpp::connection_hdl connection, std::shared_ptr<websocketpp::message_buffer::message<websocketpp::message_buffer::alloc::con_msg_manager>> msg)
			{
				std::lock_guard<std::mutex> guard(g_buffer_mutex);
				_received += msg->get_payload();
			};

			_server.set_message_handler(lambda);

			_server.listen(port);

			_server.start_accept();

			_server.run();
		});
	}


	WebSocket::~WebSocket() {
		Log::FormatLine(LOG_TRACE, tag, "Stopping Websocket Server");
		_server.stop();

		_thread->join();

		delete _thread;
	}

	/// Reads the specified number of bytes into the data buffer and stores
	/// the remote endpoint.
	///
	/// \param buffer	 The array to copy the data into.
	/// \param size	   The number of bytes to read.
	/// \param remoteAddr When at least one byte is read, contains the address
	///                   of the remote host.
	/// \returns		  The number of bytes read, or a negative number if
	///                   an error occurs.
	int WebSocket::Read(unsigned char* buffer, int size, sockaddr* remoteAddr)
	{
		std::string data;
		{
			std::lock_guard<std::mutex> guard(g_buffer_mutex);
			int subStrSize = std::min(size, (int)this->_received.size());
			data = this->_received.substr(0, subStrSize);
			this->_received.erase(0, subStrSize);
		}

		memcpy((char*)buffer, data.c_str(), data.length());
		if (this->_received.size() > 0)
		{
			Log::FormatLine(LOG_ERROR, tag, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Buffer Size After Read %d, THIS INDICATES A GROWING BUFFER\n", this->_received.size());
		}
		return data.length();
	}

	/// Sends data to the specified remote endpoint.
	///
	/// \param data   The data to be sent.
	/// \param len    The number of bytes to send.
	/// \param remote The destination socket.
	void WebSocket::SendTo(const unsigned char* buffer, std::size_t len, sockaddr* remote) const
	{
		throw std::runtime_error("Not Implemented");
	}
}