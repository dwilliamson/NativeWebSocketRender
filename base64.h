
#pragma once


inline void base64_encode(const unsigned char* in_bytes, int length, char* out_bytes)
{
	static const char* encoding_table =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"0123456789+/";

	for (int i = 0, j = 0; i < length; )
	{
		// Read input 3 values at a time, null terminating
		unsigned int c0 = i < length ? in_bytes[i++] : 0;
		unsigned int c1 = i < length ? in_bytes[i++] : 0;
		unsigned int c2 = i < length ? in_bytes[i++] : 0;

		// Encode 4 bytes for ever 3 input bytes
		unsigned int triple = (c0 << 0x10) + (c1 << 0x08) + c2;
		out_bytes[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
		out_bytes[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
		out_bytes[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
		out_bytes[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
	}

	// Pad output to multiple of 3 bytes with terminating '='
	int encoded_length = 4 * ((length + 2) / 3);
	int remaining_bytes = (3 - ((length + 2) % 3)) - 1;
	for (int i = 0; i < remaining_bytes; i++)
		out_bytes[encoded_length - 1 - i] = '=';

	out_bytes[encoded_length] = 0;
}
