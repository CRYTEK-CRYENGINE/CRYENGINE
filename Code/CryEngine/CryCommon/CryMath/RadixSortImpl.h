// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

namespace detail
{

template<typename TValue, typename TByteType, typename TAllocator, const uint numHistograms>
void RadixSortTpl(uint32* pRanks, const TValue* pValues, uint count, TAllocator& allocator)
{
	typedef typename TAllocator::template Array<uint32, uint> Array;
	const uint histogramSize = 1 << (sizeof(TByteType) * 8);
	Array histogram(allocator, histogramSize * numHistograms);
	memset(histogram.data(), 0, sizeof(uint32) * histogramSize * numHistograms);
	uint32* h[numHistograms];
	for (uint i = 0; i < numHistograms; ++i)
		h[i] = &histogram[histogramSize * i];

	const TByteType* pBytes = reinterpret_cast<const TByteType*>(pValues);
	const TByteType* pEnd = reinterpret_cast<const TByteType*>(pValues + count);

	while (pBytes != pEnd)
	{
		for (uint i = 0; i < numHistograms; ++i)
			h[i][*pBytes++]++;
	}

	uint32* pLink[histogramSize];
	Array rankTmp(allocator, count);
	memset(rankTmp.data(), 0, sizeof(uint32) * count);
	memset(pRanks, 0, sizeof(uint32) * count);
	uint32* pRanks1 = pRanks;
	uint32* pRanks2 = rankTmp.data();

	for (uint j = 0; j < numHistograms; ++j)
	{
		uint32* curCount = &histogram[histogramSize * j];

		pLink[0] = pRanks2;
		for (uint i = 1; i < histogramSize; ++i)
			pLink[i] = pLink[i - 1] + curCount[i - 1];

		const TByteType* pInputBytes = reinterpret_cast<const TByteType*>(pValues);
		pInputBytes += j;
		if (j == 0)
		{
			for (uint i = 0; i < count; ++i)
				*pLink[pInputBytes[i * numHistograms]]++ = i;
		}
		else
		{
			uint32* pIndices = pRanks1;
			uint32* pIndicesEnd = pRanks1 + count;
			while (pIndices != pIndicesEnd)
			{
				uint32 id = *pIndices++;
				*pLink[pInputBytes[id * numHistograms]]++ = id;
			}
		}

		std::swap(pRanks1, pRanks2);
	}
}

}

template<typename TValue, typename TAllocator>
void RadixSort(uint32* pRanksBegin, uint32* pRanksEnd, const TValue* pValuesBegin, const TValue* pValuesEnd, TAllocator& allocator)
{
	static_assert(
	  sizeof(TValue) == 4 || sizeof(TValue) == 8,
	  "Unsupported TValue type for RadixSort");
	const uint count = uint(pValuesEnd - pValuesBegin);
	CRY_ASSERT_MESSAGE(
	  uint(pRanksEnd - pRanksBegin) >= count,
	  "Ranks array needs to be large enough to hold the ranks for the sorted values array.");

	if (sizeof(TValue) == 4)
	{
		detail::RadixSortTpl<uint32, uint8, TAllocator, 4>(
		  pRanksBegin, reinterpret_cast<const uint32*>(pValuesBegin), count, allocator);
	}
	else if (sizeof(TValue) == 8)
	{
		detail::RadixSortTpl<uint64, uint8, TAllocator, 8>(
		  pRanksBegin, reinterpret_cast<const uint64*>(pValuesBegin), count, allocator);
	}
}
