// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once
#ifndef XMLCPB_ATTRSET_TABLE_READER_H
	#define XMLCPB_ATTRSET_TABLE_READER_H

	#include "../XMLCPB_Common.h"
	#include "XMLCPB_BufferReader.h"
	#include <CryCore/Platform/IPlatformOS.h>

// an "AttrSet" defines the datatype + tagId of all the attributes of a node.  each datatype+tagId entry is called a "header".
// those sets are stored in a common table because many nodes use the same type of attrs

namespace XMLCPB {

class CReader;

class CAttrSetTableReader
{
public:
	explicit CAttrSetTableReader(IGeneralMemoryHeap* pHeap);

	uint32 GetNumAttrs(AttrSetID setId) const;
	uint16 GetHeaderAttr(AttrSetID setId, uint32 indAttr) const;

	uint32 GetNumSets() const { return m_setAddrs.size(); }

	void   ReadFromFile(CReader& Reader, IPlatformOS::ISaveReaderPtr& pOSSaveReader, const SFileHeader& headerInfo);
	void   ReadFromMemory(CReader& Reader, const uint8* pData, uint32 dataSize, const SFileHeader& headerInfo, uint32& outReadLoc);

private:
	typedef DynArray<FlatAddr16, int, NArray::SmallDynStorage<NAlloc::GeneralHeapAlloc>> SetAddrVec;
	typedef DynArray<uint8, int, NArray::SmallDynStorage<NAlloc::GeneralHeapAlloc>>      NumAttrsVec;

	SetAddrVec    m_setAddrs;
	NumAttrsVec   m_numAttrs;
	SBufferReader m_buffer;
};

}  // end namespace

#endif
