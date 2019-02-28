// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once
#ifndef XMLCPB_STRING_TABLE_READER_H
	#define XMLCPB_STRING_TABLE_READER_H

	#include "../XMLCPB_Common.h"
	#include "XMLCPB_BufferReader.h"
	#include <CryCore/Platform/IPlatformOS.h>

namespace XMLCPB {

class CReader;

class CStringTableReader
{
public:
	explicit CStringTableReader(IGeneralMemoryHeap* pHeap);

	const char* GetString(StringID stringId) const;
	int         GetNumStrings() const { return m_stringAddrs.size(); }

	void        ReadFromFile(CReader& Reader, IPlatformOS::ISaveReaderPtr pOSSaveReader, const SFileHeader::SStringTable& headerInfo);
	void        ReadFromMemory(CReader& Reader, const uint8* pData, uint32 dataSize, const SFileHeader::SStringTable& headerInfo, uint32& outReadLoc);

	// for debug and testing
	#ifndef _RELEASE
	void WriteStringsIntoTextFile(const char* pFileName);
	#endif

private:
	typedef DynArray<FlatAddr, int, NArray::SmallDynStorage<NAlloc::GeneralHeapAlloc>> FlatAddrVec;

private:
	SBufferReader m_buffer;
	FlatAddrVec   m_stringAddrs;
};

}  // end namespace

#endif
