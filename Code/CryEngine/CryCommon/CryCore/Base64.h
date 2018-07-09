// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef BASE64_H
#define BASE64_H

namespace Base64
{
static unsigned char indexTable[65] = \
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"        \
  "abcdefghijklmnopqrstuvwxyz"        \
  "0123456789+/";

static unsigned char invertTable[256];
static bool invertTableBuilt = false;

static void buildinverttable(void)
{
	unsigned int u, v;
	for (u = 0; u < 256; ++u)
	{
		invertTable[u] = '\0';
		for (v = 0; v < 64; ++v)
		{
			if (indexTable[v] == u)
			{
				invertTable[u] = v;
				break;
			}
		}
	}
}

static unsigned int encode_base64(char* out, const char* __restrict const in, const unsigned int size, bool terminate /*= true*/)
{
	unsigned char buf[3];
	unsigned int u;
	const char* const start = out;

	// initial aligned bytes

	const int remainder = size % 3;
	const unsigned int initial = size - remainder;
	for (u = 0; u < initial; u += 3)
	{
		for (unsigned int v = 0; v < 3; ++v)
			buf[v] = in[u + v];

		*(out++) = indexTable[(buf[0] & 0xfe) >> 2];
		*(out++) = indexTable[((buf[0] & 0x03) << 4) | ((buf[1] & 0xf0) >> 4)];
		*(out++) = indexTable[((buf[1] & 0x0f) << 2) | ((buf[2] & 0xc0) >> 6)];
		*(out++) = indexTable[buf[2] & 0x3f];
	}

	// remaining unaligned bytes

	for (int v = 0; v < remainder; ++v)
		buf[v] = in[u + v];

	if (remainder == 2)
	{
		*(out++) = indexTable[(buf[0] & 0xfe) >> 2];
		*(out++) = indexTable[((buf[0] & 0x03) << 4) | ((buf[1] & 0xf0) >> 4)];
		*(out++) = indexTable[((buf[1] & 0x0f) << 2)];
		*(out++) = '=';
	}
	else if (remainder == 1)
	{
		*(out++) = indexTable[(buf[0] & 0xfe) >> 2];
		*(out++) = indexTable[(buf[0] & 0x03) << 4];
		*(out++) = '=';
		*(out++) = '=';
	}

	if (terminate) *(out++) = '\0';

	return (unsigned int)(out - start);
}

static unsigned int decode_base64(char* out, const char* __restrict const in, const unsigned int size, bool terminate /*= true*/)
{
	unsigned char buf[4];
	unsigned int u;
	const char* const start = out;

	if (!invertTableBuilt)
	{
		invertTableBuilt = true;
		buildinverttable();
	}

	assert(size % 4 == 0 && "Expected padding on Base64 encoded string.");

	for (u = 0; u < size; u += 4)
	{
		for (unsigned int v = 0; v < 4; ++v)
			buf[v] = in[u + v];

		*(out++) = (invertTable[buf[0]] << 2) | ((invertTable[buf[1]] & 0x30) >> 4);
		if (buf[1] == '=' || buf[2] == '=')
		{
			break;
		}
		*(out++) = ((invertTable[buf[1]] & 0x0f) << 4) | ((invertTable[buf[2]] & 0x3c) >> 2);
		if (buf[2] == '=' || buf[3] == '=')
		{
			break;
		}
		*(out++) = ((invertTable[buf[2]] & 0x03) << 6) | (invertTable[buf[3]] & 0x3f);
	}

	if (terminate) *(out++) = '\0';

	return (unsigned int)(out - start);
}

static unsigned int encodedsize_base64(const unsigned int size)
{
	return (size + 2 - ((size + 2) % 3)) * 4 / 3;
}

static unsigned int decodedsize_base64(const unsigned int size)
{
#define PADDINGP2(offset, align) \
  ((align) + (((offset) - 1) & ~((align) - 1))) - (offset)

	unsigned int nSize = size * 3 / 4 + PADDINGP2(size * 3 / 4, 4);

#undef PADDINGP2

	return nSize;
}
};

#endif
