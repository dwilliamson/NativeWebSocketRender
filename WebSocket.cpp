
#define _CRT_SECURE_NO_WARNINGS

#include "WebSocket.h"
#include "sha1.h"
#include "base64.h"
#include <stdio.h>


namespace
{
	char* GetField(char* buffer, const char* field_name)
	{
		// Search for the start of the field
		char* field = strstr(buffer, field_name);
		if (field == 0)
			return 0;

		// Skip over the field name and any trailing whitespace
		field += strlen(field_name);
		while (*field == ' ')
			field++;

		return field;
	}


	bool WebSocketHandshake(Socket& client)
	{
		// Really inefficient way of receiving the handshake data from the browser
		// Not really sure how to do this any better, as the termination requirement is \r\n\r\n
		char buffer[1024];
		char* buffer_ptr = buffer;
		while (true)
		{
			if (!client.Receive(buffer_ptr, 1))
				return false;
			if (buffer_ptr - buffer >= 4)
			{
				if (*(buffer_ptr - 3) == '\r' &&
					*(buffer_ptr - 2) == '\n' &&
					*(buffer_ptr - 1) == '\r' &&
					*(buffer_ptr - 0) == '\n')
					break;
			}
			buffer_ptr++;
		}
		*buffer_ptr = 0;

		// HTTP GET instruction
		if (memcmp(buffer, "GET", 3) != 0)
			return false;

		// Look for the version number and verify that it's supported
		char* version = GetField(buffer, "Sec-WebSocket-Version:");
		if (version == 0)
			return false;
		int api_version = atoi(version);
		if (api_version != 13 && api_version != 8)
			return false;

		// Make sure this is a localhost connection only
		char* host = GetField(buffer, "Host:");
		if (host == 0)
			return false;
		const char* localhost = "localhost:";
		if (memcmp(host, localhost, strlen(localhost)) != 0)
			return false;

		// Look for the key start and null-terminate it within the receive buffer
		char* key = GetField(buffer, "Sec-WebSocket-Key:");
		if (key == 0)
			return false;
		char* key_end = strstr(key, "\r\n");
		if (key_end == 0)
			return false;
		*key_end = 0;

		// Concatenate the browser's key with the WebSocket Protocol GUID and base64 encode
		// the hash, to prove to the browser that this is a bonafide WebSocket server
		char hash_string[256];
		strcpy(hash_string, key);
		strcat(hash_string, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
		unsigned char hash[5 * sizeof(unsigned int)];
		sha1::calc(hash_string, strlen(hash_string), hash);
		base64_encode(hash, sizeof(hash), hash_string);

		// Send the response back to the server
		char response_string[256];
		sprintf(response_string,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n\r\n", hash_string);
		return client.Send(response_string, strlen(response_string));
	}
}


WebSocket::WebSocket()
{
}


bool WebSocket::MakeServer(unsigned short port)
{
	return m_NativeSocket.MakeServer(port);
}


bool WebSocket::AcceptClient(WebSocket& client) const
{
	if (!m_NativeSocket.AcceptClient(client.m_NativeSocket))
		return false;

	// Need a successful handshake between client/server before allowing the connection
	if (!WebSocketHandshake(client.m_NativeSocket))
	{
		client.m_NativeSocket.Close();
		return false;
	}

	return true;
}


std::unique_ptr<unsigned char> WebSocket::Receive(int& data_size)
{
	if (!m_NativeSocket.IsOpen())
		return 0;

	// Get message header
	unsigned char msg_header[2] = { 0, 0 };
	if (!m_NativeSocket.Receive(msg_header, 2))
		return 0;

	// Check for WebSocket Protocol disconnect
	if (msg_header[0] == 0x88)
	{
		m_NativeSocket.Close();
		return 0;
	}

	// Check that the client isn't sending messages we don't understand
	if (msg_header[0] != 0x81)
	{
		m_NativeSocket.Close();
		return 0;
	}

	// Get message length and check to see if it's a marker for a wider length
	int msg_length = msg_header[1] & 0x7F;
	int size_bytes_remaining = 0;
	switch (msg_length)
	{
		case 126: size_bytes_remaining = 2; break;
		case 127: size_bytes_remaining = 4; break;
	}

	if (size_bytes_remaining > 0)
	{
		// Receive the wider bytes of the length
		char size_bytes[4];
		if (!m_NativeSocket.Receive(size_bytes, size_bytes_remaining))
		{
			m_NativeSocket.Close();
			return 0;
		}

		// Calculate new length
		msg_length = 0;
		for (int i = size_bytes_remaining - 1; i >= 0; i--)
			msg_length |= size_bytes[i] << (i * 8);
	}

	// Receive any message data masks
	bool mask_present = (msg_header[1] & 0x80) != 0;
	unsigned char mask[4] = { 0, 0, 0, 0 };
	if (mask_present)
	{
		if (!m_NativeSocket.Receive(mask, 4))
		{
			m_NativeSocket.Close();
			return 0;
		}
	}

	// Receive the message data and null terminate
	std::unique_ptr<unsigned char> msg_data(new unsigned char[msg_length + 1]);
	unsigned char* msg_data_ptr = msg_data.get();
	if (!m_NativeSocket.Receive(msg_data_ptr, msg_length))
	{
		m_NativeSocket.Close();
		return 0;
	}
	msg_data_ptr[msg_length] = 0;

	// Apply data mask
	if (mask_present)
	{
		for (int i = 0; i < msg_length; i++)
			msg_data_ptr[i] ^= mask[i & 3];
	}

	data_size = msg_length;
	return msg_data;
}


static void WriteSize(int size, unsigned char* dest, int dest_size, int dest_offset)
{
	int size_size = dest_size - dest_offset;
	for (int i = 0; i < dest_size; i++)
	{
		int j = i - dest_offset;
		dest[i] = (j < 0) ? 0 : (size >> ((size_size - j - 1) * 8)) & 0xFF;
	}
}


bool WebSocket::Send(WebSocketDataType data_type, const void* data, int data_size)
{
	if (!m_NativeSocket.IsOpen())
		return false;

	unsigned char final_fragment = 0x1 << 7;
	unsigned char frame_type = data_type == WSDT_Text ? 1 : 2;
	unsigned char frame_header[10];
	frame_header[0] = final_fragment | frame_type;

	// Construct the frame header, correctly applying the narrowest size
	int frame_header_size = 0;
	//unsigned char frame_header[10];
	//frame_header[0] = 0x81 | (0x2 << 4);
	if (data_size <= 125)
	{
		frame_header_size = 2;
		frame_header[1] = data_size;
	}
	else if (data_size <= 65535)
	{
		frame_header_size = 2 + 2;
		frame_header[1] = 126;
		WriteSize(data_size, frame_header + 2, 2, 0);
	}
	else
	{
		frame_header_size = 2 + 8;
		frame_header[1] = 127;
		WriteSize(data_size, frame_header + 2, 8, 4);
	}

	// Allocate the frame and copy in the header and data
	int frame_size = frame_header_size + data_size;
	std::unique_ptr<unsigned char> frame(new unsigned char[frame_size]);
	memcpy(frame.get(), frame_header, frame_header_size);
	memcpy(frame.get() + frame_header_size, data, data_size);

	// Send back to the WebSocket
	return m_NativeSocket.Send(frame.get(), frame_size);
}