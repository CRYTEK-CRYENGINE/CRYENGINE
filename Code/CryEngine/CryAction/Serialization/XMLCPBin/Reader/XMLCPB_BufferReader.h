// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once

#ifndef XMLCPB_BUFFERREADER_H
	#define XMLCPB_BUFFERREADER_H

	#include "../XMLCPB_Common.h"
	#include <CryCore/Platform/IPlatformOS.h>

namespace XMLCPB {

class CReader;

struct SBufferReader
{
	explicit SBufferReader(IGeneralMemoryHeap* pHeap)
		: m_pHeap(pHeap)
		, m_pBuffer(NULL)
		, m_bufferSize(0)
	{
	}

	~SBufferReader()
	{
		if (m_pBuffer)
			m_pHeap->Free(m_pBuffer);
	}

	inline const uint8* GetPointer(FlatAddr addr) const
	{
		assert(addr < m_bufferSize);
		return &(m_pBuffer[addr]);
	}

	inline void CopyTo(uint8* pDst, FlatAddr srcAddr, uint32 bytesToCopy) const
	{
		assert(srcAddr < m_bufferSize);
		assert(srcAddr + bytesToCopy <= m_bufferSize);

		memcpy(pDst, GetPointer(srcAddr), bytesToCopy);
	}

	void ReadFromFile(CReader& Reader, IPlatformOS::ISaveReaderPtr& pOSSaveReader, uint32 readSize);
	void ReadFromMemory(CReader& Reader, const uint8* pData, uint32 dataSize, uint32 readSize, uint32& outReadLoc);

private:
	SBufferReader(const SBufferReader&);
	SBufferReader& operator=(const SBufferReader&);

private:
	_smart_ptr<IGeneralMemoryHeap> m_pHeap;
	uint8*                         m_pBuffer;
	size_t                         m_bufferSize;
};

}  // end namespace

#endif
