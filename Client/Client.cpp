
#include "../Socket.h"
#include <memory>


int main()
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
	if (!server.ConnectToServer("localhost", 8888))
		return 1;

	double last_frame = counter.Sample();
	for (int i = 0; i < 10000; i++)
	{
		// Request a new frame buffer from the server
		MessageHeader msg;
		msg.id = MSG_RequestNewFrameBuffer;
		msg.data_size = 0;
		msg.time_stamp = counter.Sample();
		if (!server.Send(msg))
			break;

		// Wait around for a response
		if (!server.Receive(msg))
			break;
		std::unique_ptr<char> data(new char[msg.data_size]);
		if (!server.Receive(data.get(), msg.data_size))
			break;

		if (msg.id == MSG_NewFrameBuffer)
		{
			double now = counter.Sample();
			printf("Data received: %d %f (%f) %f\n", msg.data_size, msg.time_stamp, now - msg.time_stamp, now - last_frame);
			last_frame = now;
		}

		Sleep(1);
		//SwitchToThread();
	}

	return 0;
}