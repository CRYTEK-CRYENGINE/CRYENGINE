// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "cry_sha1.h"

namespace CrySHA1
{
void sha1Block(const unsigned char block[64], uint32* digest);
};

// block computation as from RFC
void CrySHA1::sha1Block(const unsigned char block[64], uint32* digest)
{
	const uint32 K[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };

	uint32 W[80];

	for (int t = 0; t < 16; t++)
	{
		W[t] = block[t * 4 + 0] << 24 | block[t * 4 + 1] << 16 | block[t * 4 + 2] << 8 | block[t * 4 + 3];
	}

	for (int t = 16; t < 80; t++)
	{
		W[t] = circularShift(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
	}

	uint32 A = digest[0], B = digest[1], C = digest[2], D = digest[3], E = digest[4];

	for (int t = 0; t < 20; t++)
	{
		uint32 TEMP = circularShift(5, A) + ((B & C) | (~B & D)) + E + W[t] + K[t / 20];

		E = D;
		D = C;
		C = circularShift(30, B);
		B = A;
		A = TEMP;
	}
	for (int t = 20; t < 40; t++)
	{
		uint32 TEMP = circularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[t / 20];

		E = D;
		D = C;
		C = circularShift(30, B);
		B = A;
		A = TEMP;
	}
	for (int t = 40; t < 60; t++)
	{
		uint32 TEMP = circularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[t / 20];

		E = D;
		D = C;
		C = circularShift(30, B);
		B = A;
		A = TEMP;
	}
	for (int t = 60; t < 80; t++)
	{
		uint32 TEMP = circularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[t / 20];

		E = D;
		D = C;
		C = circularShift(30, B);
		B = A;
		A = TEMP;
	}

	digest[0] += A;
	digest[1] += B;
	digest[2] += C;
	digest[3] += D;
	digest[4] += E;
}

void CrySHA1::sha1Calc(const unsigned char* inbuf, const uint64 buflen, uint32* digest)
{
	// initial H[] from RFC
	uint32 H[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };

	// copy initial state
	memcpy(digest, H, sizeof(uint32) * 5);

	unsigned char block[64];

	// process through buffer. i tracks where we are in input buffer,
	// b where we are in the current block [0,63]
	uint32 b = 0;

	for (uint64 i = 0; i < buflen; i++)
	{
		block[b++] = inbuf[i];

		if (b == 64)
		{
			sha1Block(block, digest);
			b = 0;
		}
	}

	block[b++] = 0x80; // must add 1 bit at the end of message, minimum padding

	if (b <= 56) // enough for padding plus uint64 buffer length
	{
		// pad up to where length begins
		while (b < 56)
		{
			block[b++] = 0x0;
		}
		assert(b == 56);
	}
	else // need to pad this block, then make a block of padding + length
	{
		// pad up to this block (handful of more bytes)
		while (b < 64)
		{
			block[b++] = 0x0;
		}
		// process it and start new block
		sha1Block(block, digest);
		b = 0;

		// pad up to where length begins
		while (b < 56)
		{
			block[b++] = 0x0;
		}
		assert(b == 56);
	}

	// at this point we are guaranteed to be at the last 8 bytes of a block,
	// after padding. Write in the length in bits and process final block
	const uint64 bitlen = buflen * 8;

	block[56] = (unsigned char)((bitlen & 0xFF00000000000000) >> 56);
	block[57] = (unsigned char)((bitlen & 0x00FF000000000000) >> 48);
	block[58] = (unsigned char)((bitlen & 0x0000FF0000000000) >> 40);
	block[59] = (unsigned char)((bitlen & 0x000000FF00000000) >> 32);

	block[60] = (unsigned char)((bitlen & 0x00000000FF000000) >> 24);
	block[61] = (unsigned char)((bitlen & 0x0000000000FF0000) >> 16);
	block[62] = (unsigned char)((bitlen & 0x000000000000FF00) >> 8);
	block[63] = (unsigned char)((bitlen & 0x00000000000000FF) >> 0);

	sha1Block(block, digest);
}

void CrySHA1::sha1Calc(const char* str, uint32* digest)
{
	sha1Calc((const unsigned char*)str, (uint64)strlen(str), digest);
}
