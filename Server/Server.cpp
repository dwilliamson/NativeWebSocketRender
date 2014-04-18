
#include "../Socket.h"
#include "../WebSocket.h"
#include <memory>
#include <stdio.h>


int NativeServer()
{
	// Make sure the client/server always run on the same processor.
	// Ensures QPF is stable around kernel bugs and highlights worst-case target hardware.
	// In reality, the task scheduler will avoid the worst-case on most commodity machines.
	// On my quad core, I'm getting 13ms round-trip between frame buffer messages. This
	// leaps to 21ms with spikes of 40ms (and slow OS response times) when fixing processor
	// assignment.
	// Scheduling them both on a secondary processor fixes this, as well.
	//SetThreadAffinityMask(GetCurrentThread(), 1);

	Counter counter;

	Socket server;
	if (!server.MakeServer(8888))
		return 1;

	// Allocate a dummy frame buffer
	int width = 1280;
	int height = 1024;
	int depth = 4;
	int frame_buffer_size = width * height * depth;
	std::unique_ptr<char> frame_buffer(new char[frame_buffer_size]);
	for (int i = 0; i < frame_buffer_size; i++)
		frame_buffer.get()[i] = ((i + 98652342) % 57 << 12) ^ i;
	
	// Loop forever accepting serial connections
	Socket client;
	while (server.AcceptClient(client))
	{
		printf("Connection accepted\n");

		// Loop receiving messages from the connected client
		while (true)
		{
			MessageHeader msg;
			if (!client.Receive(msg))
				break;

			// Respond to requests for new frame buffers
			if (msg.id == MSG_RequestNewFrameBuffer)
			{
				msg.id = MSG_NewFrameBuffer;
				msg.data_size = frame_buffer_size;
				msg.time_stamp = counter.Sample();
				if (!client.Send(msg))
					break;
				if (!client.Send(frame_buffer.get(), frame_buffer_size))
					break;
			}
		}

		printf("Connection lost\n");
	}

	return 0;
}


int WebSocketServer()
{
	Counter counter;

	WebSocket server;
	if (!server.MakeServer(8888))
		return 1;

	// Allocate a dummy frame buffer
	int width = 1024;
	int height = 768;
	int depth = 3;
	int frame_buffer_size = width * height * depth + 3;
	std::unique_ptr<unsigned char> frame_buffer(new unsigned char[frame_buffer_size]);

	int table[256];
	for (int i = 0; i < 256; i++)
		table[i] = (int)(128 + 127.0 * sin(i * 2 * 3.141 / 256.0));
	
	// Loop forever accepting serial connections
	WebSocket client;
	while (server.AcceptClient(client))
	{
		printf("Connection accepted\n");

		// Loop receiving messages from the connected client
		double j = 0;
		while (true)
		{ 
			int msg_length;
			std::unique_ptr<unsigned char> msg_data = client.Receive(msg_length);
			if (msg_data == 0)
				break;

			unsigned int time_stamp = atoi((char*)msg_data.get());

			if (!client.Send(WSDT_Binary, frame_buffer.get(), frame_buffer_size))
				break;

			int t = (int)(128 + 127.0 * sin(0.0013 * j));
			int t2 = (int)(128 + 127.0 * sin(0.0023 * j));
			int t3 = (int)(128 + 127.0 * sin(0.0007 * j));

			// Generate before the next receive
			unsigned char* fb = frame_buffer.get();
			for (unsigned int i = 0; i < 3; i++)
				*fb++ = (time_stamp >> (i * 8)) & 0xFF;
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					int r = table[(x / 5 + t / 4 + table[(t2 / 3 + y / 8) & 0xFF]) & 0xFF];
					int g = table[(y / 3 + t + table[(t3 + x / 5) & 0xFF]) & 0xFF];
					int b = table[(y / 4 + t2 + table[(t + g / 4 + x / 3) & 0xFF]) & 0xFF];
					*fb++ = (unsigned char)r;
					*fb++ = (unsigned char)g;
					*fb++ = (unsigned char)b;
				}
			}

			j += 30;
		}

		printf("Connection lost\n");
	}

	return 0;
}


int main()
{
	return WebSocketServer();
	//return NativeServer();
}