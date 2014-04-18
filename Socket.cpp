
#include "Socket.h"


Counter::Counter()
{
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	m_Frequency = double(freq.QuadPart) / 1000.0;
}


double Counter::Sample() const
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return double(counter.QuadPart) / m_Frequency;
}


Socket::Socket()
	: m_Socket(INVALID_SOCKET)
{
	WSADATA wsadata;
	WSAStartup(MAKEWORD(1, 1), &wsadata);

	m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}


Socket::~Socket()
{
	Close();
	WSACleanup();
}


bool Socket::MakeServer(unsigned short port) const
{
	if (!IsOpen())
		return false;

	SOCKADDR_IN sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = htons(port);

	if (bind(m_Socket, (LPSOCKADDR)&sa, sizeof(struct sockaddr)) == SOCKET_ERROR)
		return false;
	if (listen(m_Socket, 10) == SOCKET_ERROR)
		return false;

	return true;
}


bool Socket::AcceptClient(Socket& socket) const
{
	if (!IsOpen())
		return false;

	SOCKET s = accept(m_Socket, NULL, NULL);
	if (s == INVALID_SOCKET)
		return false;

	socket.m_Socket = s;
	return true;
}


bool Socket::ConnectToServer(const char* host_name, unsigned short port) const
{
	if (!IsOpen())
		return false;

	LPHOSTENT host_entry = gethostbyname(host_name);
	if (!host_entry)
		return false;

	SOCKADDR_IN sa;
	sa.sin_family = AF_INET;
	sa.sin_addr = *((LPIN_ADDR)*host_entry->h_addr_list);
	sa.sin_port = htons(port);

	if (connect(m_Socket, (LPSOCKADDR)&sa, sizeof(struct sockaddr)) == SOCKET_ERROR)
		return false;

	return true;
}


bool Socket::Receive(void* data, int data_size)
{
	if (!IsOpen())
		return false;

	char* cur_data = (char*)data;
	char* end_data = cur_data + data_size;

	// Loop until all data has been received
	while (cur_data < end_data)
	{
		int bytes_received = recv(m_Socket, cur_data, end_data - cur_data, 0);
		if (bytes_received == SOCKET_ERROR || bytes_received == 0)
		{
			// Fail if receiving fails for any other reason other than blocking
			int error = WSAGetLastError();
			if (error != WSAEWOULDBLOCK)
				return false;
		}
		else
		{
			// Jump over the data received
			cur_data += bytes_received;
		}
	}

	return true;
}


bool Socket::Send(const void* data, int data_size)
{
	if (!IsOpen())
		return false;

	char* cur_data = (char*)data;
	char* end_data = cur_data + data_size;

	while (cur_data < end_data)
	{
		// Attempt to send the remaining chunk of data
		int bytes_sent = send(m_Socket, cur_data, end_data - cur_data, 0);

		if (bytes_sent == SOCKET_ERROR || bytes_sent == 0)
		{
			// Fail if sending fails for any other reason other than blocking
			int error = WSAGetLastError();
			if (error != WSAEWOULDBLOCK)
				return false;
		}
		else
		{
			// Jump over the data sent
			cur_data += bytes_sent;
		}
	}

	return true;
}


bool Socket::IsOpen() const
{
	return m_Socket != INVALID_SOCKET;
}


void Socket::Close()
{
	if (m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
}
