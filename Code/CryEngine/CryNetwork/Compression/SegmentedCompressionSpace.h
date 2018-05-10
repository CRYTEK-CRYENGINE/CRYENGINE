// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SEGMENTEDCOMPRESSIONSPACE_H__
#define __SEGMENTEDCOMPRESSIONSPACE_H__

#pragma once

#include "Config.h"

#if USE_MEMENTO_PREDICTORS

	#include "Streams/CommStream.h"

class CSegmentedCompressionSpace
{
public:
	CSegmentedCompressionSpace();
	bool Load(const char* filename);

	void Encode(CCommOutputStream& stm, const int32* pValue, int dim) const;
	bool CanEncode(const int32* pValue, int dim) const;
	void Decode(CCommInputStream& stm, int32* pValue, int dim) const;
	int  GetBitCount() const { return m_bits; }

	void GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CSegmentedCompressionSpace");

		if (countingThis)
			pSizer->Add(*this);
		pSizer->AddContainer(m_data);
	}

private:
	static const int    MAX_DIMENSION = 4;

	int                 m_steps;
	int                 m_dimensions;
	int32               m_center[MAX_DIMENSION];
	int                 m_outerSizeSteps;
	int32               m_outerSizeSpace;
	int                 m_chunkSize;
	int                 m_bits;
	std::vector<uint32> m_data;

	ILINE uint32 GetTot(uint32 chunk) const
	{
		return m_data[(chunk + 1) * m_chunkSize - 3] + m_data[(chunk + 1) * m_chunkSize - 2];
	}
	ILINE uint32 GetLow(uint32 chunk, uint32 ofs) const
	{
		return m_data[chunk * m_chunkSize + ofs * 3 + 0];
	}
	ILINE uint32 GetSym(uint32 chunk, uint32 ofs) const
	{
		return m_data[chunk * m_chunkSize + ofs * 3 + 1];
	}
	ILINE uint32 GetChild(uint32 chunk, uint32 ofs) const
	{
		return m_data[chunk * m_chunkSize + ofs * 3 + 2];
	}
};

#endif

#endif
