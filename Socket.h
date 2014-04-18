
#pragma once


#include "Windows.h"
#include "Winsock.h"


#pragma comment(lib, "ws2_32.lib")


class Counter
{
public:
	Counter();

	double Sample() const;

private:
	double m_Frequency;
};


class Socket
{
public:
	Socket();
	~Socket();

	bool MakeServer(unsigned short port) const;
	bool AcceptClient(Socket& socket) const;

	bool ConnectToServer(const char* host_name, unsigned short port) const;

	bool Receive(void* data, int data_size);
	bool Send(const void* data, int data_size);

	bool IsOpen() const;
	void Close();

	template <typename TYPE>
	bool Receive(TYPE& data)
	{
		return Receive(&data, sizeof(data));
	}

	template <typename TYPE>
	bool Send(const TYPE& data)
	{
		return Send(&data, sizeof(data));
	}

private:
	SOCKET m_Socket;
};


enum MessageID
{
	MSG_RequestNewFrameBuffer,
	MSG_NewFrameBuffer,
};


struct MessageHeader
{
	MessageID id;
	int data_size;
	double time_stamp;
};