
#pragma once


#include "Socket.h"
#include <memory>


enum WebSocketDataType
{
	WSDT_Text,
	WSDT_Binary
};


class WebSocket
{
public:
	WebSocket();

	bool MakeServer(unsigned short port);
	bool AcceptClient(WebSocket& socket) const;

	std::unique_ptr<unsigned char> Receive(int& data_size);
	bool Send(WebSocketDataType data_type, const void* data, int data_size);

private:
	Socket m_NativeSocket;
};