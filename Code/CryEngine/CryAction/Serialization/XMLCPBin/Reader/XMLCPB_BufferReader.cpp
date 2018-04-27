// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "XMLCPB_BufferReader.h"
#include "XMLCPB_Reader.h"

using namespace XMLCPB;

//////////////////////////////////////////////////////////////////////////

void SBufferReader::ReadFromFile(CReader& Reader, IPlatformOS::ISaveReaderPtr& pOSSaveReader, uint32 readSize)
{
	assert(m_bufferSize == 0);
	assert(!m_pBuffer);

	m_pBuffer = (uint8*)m_pHeap->Malloc(readSize, "");

	m_bufferSize = readSize;

	Reader.ReadDataFromFile(pOSSaveReader, const_cast<uint8*>(GetPointer(0)), readSize);
}

//////////////////////////////////////////////////////////////////////////

void SBufferReader::ReadFromMemory(CReader& Reader, const uint8* pData, uint32 dataSize, uint32 readSize, uint32& outReadLoc)
{
	assert(m_bufferSize == 0);
	assert(!m_pBuffer);

	m_pBuffer = (uint8*)m_pHeap->Malloc(readSize, "");

	m_bufferSize = readSize;

	Reader.ReadDataFromMemory(pData, dataSize, const_cast<uint8*>(GetPointer(0)), readSize, outReadLoc);
}
