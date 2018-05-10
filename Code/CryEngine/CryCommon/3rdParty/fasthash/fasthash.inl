// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/* The MIT License


   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
 */

#include "fasthash.h"

namespace fasthash
{
#pragma warning(disable : 4307)

// Compression function for Merkle-Damgard construction.
// This function is generated using the framework provided.
ILINE uint64_t MerkleDamgard(uint64_t& h)
{
	(h) ^= (h) >> 23;
	(h) *= 0x2127599bf4325c37ULL;
	(h) ^= (h) >> 47;

	return h;
}

template<size_t len>
ILINE uint64_t fasthash64(const void* buf, uint32_t seed)
{
	const uint64_t m = 0x880355f21e6d1965ULL;
	const uint64_t* pos = (const uint64_t*)buf;
	const uint64_t* end = pos + (len / sizeof(*pos));
	const unsigned char* pos2;
	uint64_t h = seed ^ uint64_t(len * m);
	uint64_t v;

	while (pos != end)
	{
		v = *pos++;
		h ^= MerkleDamgard(v);
		h *= m;
	}

	pos2 = (const unsigned char*)pos;
	v = 0;

	switch (len & (sizeof(*pos) - 1))
	{
	case 7:
		v ^= (uint64_t)pos2[6] << 48;
	case 6:
		v ^= (uint64_t)pos2[5] << 40;
	case 5:
		v ^= (uint64_t)pos2[4] << 32;
	case 4:
		v ^= (uint64_t)pos2[3] << 24;
	case 3:
		v ^= (uint64_t)pos2[2] << 16;
	case 2:
		v ^= (uint64_t)pos2[1] << 8;
	case 1:
		v ^= (uint64_t)pos2[0];
		h ^= MerkleDamgard(v);
		h *= m;
	case 0:
		break;
	}

	return MerkleDamgard(h);
}

template<size_t len>
ILINE uint32_t fasthash32(const void* buf, uint32_t seed)
{
	// the following trick converts the 64-bit hashcode to Fermat
	// residue, which shall retain information from both the higher
	// and lower parts of hashcode.
	uint64_t h = fasthash64<len>(buf, seed);
	return uint32_t(h - (h >> 32));
}
}
