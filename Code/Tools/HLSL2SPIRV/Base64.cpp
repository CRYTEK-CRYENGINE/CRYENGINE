// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <stdint.h>
#include "Base64.h"

namespace Base64
{
size_t DecodedSize(const void* b64_buf, size_t b64_buf_size)
{
	if (!b64_buf_size)
		return 0;
	if (b64_buf_size % 4)
		return -1;
	const char* p = (const char*)b64_buf + b64_buf_size;
	if (p[-1] != '=')
		return b64_buf_size - b64_buf_size / 4;
	if (p[-2] != '=')
		return b64_buf_size - b64_buf_size / 4 - 1;
	return b64_buf_size - b64_buf_size / 4 - 2;
}

static const signed char BASE64_REVERSE_TABLE[256] =
{
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

bool Base64::DecodeBuffer(const void* b64_buf, size_t b64_buf_size, void* orig_buf)
{
	if (b64_buf_size % 4)
		return false;

	const uint8_t* p = (const uint8_t*)b64_buf;
	uint8_t* q = (uint8_t*)orig_buf;

	size_t full_block_count = b64_buf_size / 4;
	size_t last_block_orig_size = 0;
	if (p[b64_buf_size - 1] == '=')
	{
		--full_block_count;
		if (p[b64_buf_size - 2] == '=')
			last_block_orig_size = 1;
		else
			last_block_orig_size = 2;
	}

	int c0, c1, c2, c3;
	for (size_t i = 0; i < full_block_count; ++i, p += 4, q += 3)
	{
		c0 = BASE64_REVERSE_TABLE[p[0]];
		if (c0 < 0) return false;
		c1 = BASE64_REVERSE_TABLE[p[1]];
		if (c1 < 0) return false;
		c2 = BASE64_REVERSE_TABLE[p[2]];
		if (c2 < 0) return false;
		c3 = BASE64_REVERSE_TABLE[p[3]];
		if (c3 < 0) return false;
		q[0] = (uint8_t)((c0 << 2) | (c1 >> 4));
		q[1] = (uint8_t)((c1 << 4) | (c2 >> 2));
		q[2] = (uint8_t)((c2 << 6) | c3);
	}

	switch (last_block_orig_size)
	{
	case 1:
		c0 = BASE64_REVERSE_TABLE[p[0]];
		if (c0 < 0) return false;
		c1 = BASE64_REVERSE_TABLE[p[1]];
		if (c1 < 0) return false;
		q[0] = (uint8_t)((c0 << 2) | (c1 >> 4));
		break;
	case 2:
		c0 = BASE64_REVERSE_TABLE[p[0]];
		if (c0 < 0) return false;
		c1 = BASE64_REVERSE_TABLE[p[1]];
		if (c1 < 0) return false;
		c2 = BASE64_REVERSE_TABLE[p[2]];
		if (c2 < 0) return false;
		q[0] = (uint8_t)((c0 << 2) | (c1 >> 4));
		q[1] = (uint8_t)((c1 << 4) | (c2 >> 2));
		break;
	}

	return true;
}
}
