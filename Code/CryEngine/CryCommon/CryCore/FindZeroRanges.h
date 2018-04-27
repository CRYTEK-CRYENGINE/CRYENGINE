// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef FINDZERORANGES_H
#define FINDZERORANGES_H
#include "BitFiddling.h"

template<typename Func>
inline void FindZeroRanges(const uint32* str, size_t strLen, Func& yield)
{
	size_t carry = 0;
	size_t bitIdx = 0;

	for (size_t wordIdx = 0; wordIdx < strLen; ++wordIdx)
	{
		size_t wordBitIdx = 0;
		int64 word = str[wordIdx];

		// Set up sign extension to insert bits that are the last bit, inverted.

		if (!(word & 0x80000000))
			reinterpret_cast<uint64&>(word) |= 0xffffffff00000000ULL;

		do
		{
			size_t wordZeroRunLen = size_t(countTrailingZeros64(word));

			wordBitIdx += wordZeroRunLen;
			carry += wordZeroRunLen;

			if (wordBitIdx == 32)
				break;

			yield(bitIdx, carry);
			word >>= wordZeroRunLen;
			bitIdx += carry;
			carry = 0;

			size_t wordOneRunLen = size_t(countTrailingZeros64(~word));
			bitIdx += wordOneRunLen;
			wordBitIdx += wordOneRunLen;

			if (wordBitIdx == 32)
				break;

			word >>= wordOneRunLen;
		}
		while (true);
	}

	if (carry)
	{
		yield(bitIdx, carry);
	}
}

#endif
