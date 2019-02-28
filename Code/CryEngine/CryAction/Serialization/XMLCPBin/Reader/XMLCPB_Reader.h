// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once
#ifndef XMLCPB_READER_H
	#define XMLCPB_READER_H

	#include "XMLCPB_StringTableReader.h"
	#include "XMLCPB_AttrSetTableReader.h"
	#include "XMLCPB_NodeLiveReader.h"
	#include "XMLCPB_BufferReader.h"
	#include <CryCore/Platform/IPlatformOS.h>

namespace XMLCPB {

// CReader should not be used directly from outside. use CReaderInterface instead.

class CReader
{
public:
	static _smart_ptr<IGeneralMemoryHeap> CreateHeap();

public:

	explicit CReader(IGeneralMemoryHeap* pHeap);
	~CReader();

	CNodeLiveReaderRef         GetRoot();
	CNodeLiveReaderRef         CreateNodeRef();
	bool                       ReadBinaryFile(const char* pFileName);
	bool                       ReadBinaryMemory(const uint8* pData, uint32 uSize);
	void                       SaveTestFiles();
	uint32                     GetTotalDataSize() const     { return m_totalSize; }
	uint32                     GetNodesDataSize() const     { return m_nodesDataSize; }
	uint32                     GetNumNodes() const          { return m_numNodes; }

	const CStringTableReader&  GetAttrNamesTable() const    { return m_tableAttrNames; }
	const CStringTableReader&  GetStrDataTable() const      { return m_tableStrData; }
	const CStringTableReader&  GetTagsTable() const         { return m_tableTags; }
	const CAttrSetTableReader& GetDataTypeSetsTable() const { return m_tableAttrSets; }
	const CNodeLiveReader&     ActivateLiveNodeFromCompact(NodeGlobalID nodeId);
	CNodeLiveReader*           GetNodeLive(NodeLiveID nodeId);
	void                       FreeNodeLive(NodeLiveID nodeId);
	template<class T>
	FlatAddr                   ReadFromBuffer(FlatAddr addr, T& data);
	template<class T>
	FlatAddr                   ReadFromBufferEndianAware(FlatAddr addr, T& data);
	FlatAddr                   ReadFromBuffer(FlatAddr addr, uint8*& rdata, uint32 len);
	void                       ReadDataFromFile(IPlatformOS::ISaveReaderPtr& pOSSaveReader, void* pDst, uint32 numBytes);
	void                       ReadDataFromMemory(const uint8* pData, uint32 dataSize, void* pDst, uint32 numBytes, uint32& outReadLoc);
	FlatAddr                   GetAddrNode(NodeGlobalID id) const;
	const uint8*               GetPointerFromFlatAddr(FlatAddr addr) const { return m_buffer.GetPointer(addr); }

private:

	void CheckErrorFlag(IPlatformOS::EFileOperationCode code);
	void ReadDataFromFileInternal(IPlatformOS::ISaveReaderPtr& pOSSaveReader, void* pSrc, uint32 numBytes);
	void ReadDataFromZLibBuffer(IPlatformOS::ISaveReaderPtr& pOSSaveReader, uint8*& pDst, uint32& numBytesToRead);
	void CreateNodeAddressTables();

	#ifdef XMLCPB_CHECK_FILE_INTEGRITY
	bool CheckFileCorruption(IPlatformOS::ISaveReaderPtr& pOSSaveReader, const SFileHeader& fileHeader, uint32 totalSize);
	#endif

private:
	typedef DynArray<CNodeLiveReader, int, NArray::SmallDynStorage<NAlloc::GeneralHeapAlloc>> LiveNodesVec;
	typedef DynArray<FlatAddr, int, NArray::SmallDynStorage<NAlloc::GeneralHeapAlloc>>        FlatAddrVec;

	_smart_ptr<IGeneralMemoryHeap> m_pHeap;
	LiveNodesVec                   m_liveNodes;
	SBufferReader                  m_buffer;
	uint32                         m_firstFreeLiveNode;
	CStringTableReader             m_tableTags;
	CStringTableReader             m_tableAttrNames;
	CStringTableReader             m_tableStrData;
	CAttrSetTableReader            m_tableAttrSets;
	uint32                         m_maxNumActiveNodes;
	uint32                         m_numActiveNodes;
	uint32                         m_totalSize;
	uint32                         m_nodesDataSize;
	uint8*                         m_pZLibBuffer;                // zlib output uncompressed data buffer
	uint8*                         m_pZLibCompressedBuffer;      // zlib input compressed data buffer
	uint32                         m_ZLibBufferSizeWithData;     // how much of m_pZLibBuffer is filled with actual data
	uint32                         m_ZLibBufferSizeAlreadyRead;  // how much of m_pZLibBuffer is already been read
	uint32                         m_numNodes;
	bool                           m_errorReading;
	FlatAddrVec                    m_nodesAddrTable;     // stores the address of each node. Index is the NodeGlobalId (which is the order in the big buffer)

	static int                     MAX_NUM_LIVE_NODES;
};

template<class T>
FlatAddr CReader::ReadFromBuffer(FlatAddr addr, T& data)
{
	uint8* pDst = (uint8*)(&data);

	m_buffer.CopyTo(pDst, addr, sizeof(T));

	return addr + sizeof(T);
}

template<class T>
FlatAddr CReader::ReadFromBufferEndianAware(FlatAddr addr, T& data)
{
	uint8* pDst = (uint8*)(&data);

	m_buffer.CopyTo(pDst, addr, sizeof(T));

	SwapIntegerValue(data);

	return addr + sizeof(T);
}

//////////////////////////////////////////////////////////////////////////

inline FlatAddr CReader::GetAddrNode(NodeGlobalID id) const
{
	assert(id < m_numNodes);

	FlatAddr addr = m_nodesAddrTable[id];
	return addr;
}

}  // end namespace

#endif
