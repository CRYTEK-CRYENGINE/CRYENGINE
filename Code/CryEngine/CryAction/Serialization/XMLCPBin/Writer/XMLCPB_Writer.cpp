// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

// overwiew of the writting process:
//
// - When a node is created, it is temporarily added to the "LiveNodes" list.
//
// - As nodes are being finished, their data is appended to the main buffer, and their spot in the live list is freed to be ready for another new node
//
// - if streaming is used, sub-buffers of the main buffer are written into the file as soon as they are filled.
//
// - tags, attribute names, and attribute string data is added to the StringTableWriter objects when every node/attr is created. They are written into the file only at the end of the process.
//

#include "StdAfx.h"
#include "XMLCPB_Writer.h"
#include "XMLCPB_ZLibCompressor.h"
#include "CryActionCVars.h"
#include "../XMLCPB_Utils.h"

namespace XMLCPB
{

uint64* AttrStringAllocatorImpl::s_buckets[AttrStringAllocatorImpl::k_maxItems / AttrStringAllocatorImpl::k_numPerBucket];
uint64* AttrStringAllocatorImpl::s_ptr;
uint64* AttrStringAllocatorImpl::s_end;
int AttrStringAllocatorImpl::s_currentBucket;
int AttrStringAllocatorImpl::s_poolInUse;

//////////////////////////////////////////////////////////////////////////

CWriter::CWriter()
	: m_compressor(nullptr)
	, m_firstFreeLiveNode(0)
	, m_tableTags(MAX_NUM_TAGS, 4096)
	, m_tableAttrNames(MAX_NUM_NAMES, 4096)
	, m_tableStrData(MAX_NUM_STRDATA, 32 * 1024)
	, m_tableStrDataConstants(DT_NUM_CONST_STR, 2048)
	, m_tableAttrSets(BIT(16), 4096)
	, m_mainBuffer(MAIN_BUFFER_SIZE_INCREMENT)
	, m_safecheckIDCounter(0)
	, m_isSavingIntoFile(false)
	, m_counterCompactedNodes(0)
	, m_hasInternalError(false)
{
	m_liveNodes.resize(MAX_NUM_LIVE_NODES);
	AttrStringAllocatorImpl::LockPool();
}

void CWriter::Init(const char* pNameRootNode, const char* pFileName)
{
	m_isSavingIntoFile = pFileName != NULL;
	bool useStreaming = m_isSavingIntoFile;
	m_mainBuffer.Init(this, useStreaming);
	m_tableTags.Init(this);
	m_tableAttrNames.Init(this);
	m_tableStrData.Init(this);
	m_tableStrDataConstants.Init(this);
	m_tableAttrSets.Init(this);
	if (m_isSavingIntoFile)
	{
#ifdef XMLCPB_DEBUGUTILS
		CDebugUtils::SetLastFileNameSaved(pFileName);
#endif
#ifdef XMLCPB_COLLECT_STATS
		CNodeLiveWriter::m_stats.Reset();
		CAttrWriter::m_statistics.Reset();
#endif
		m_tableStrDataConstants.CreateStringsFromConstants();
		{
			m_compressor = new CZLibCompressor(pFileName); // Don't delete this when finished, it is held by the compressor thread until finished with and destroyed by it.
		}
	}

	CNodeLiveWriter* pRoot = CreateAndAddLiveNode(pNameRootNode);
	assert(pRoot->GetID() == XMLCPB_ROOTNODE_ID);
}

//////////////////////////////////////////////////////////////////////////

CWriter::~CWriter()
{
#ifdef XMLCPB_COLLECT_STATS
	if (m_isSavingIntoFile)
	{
		LogStatistics();
		CAttrWriter::WriteFileStatistics(m_tableStrData);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////

CWriterBase::~CWriterBase()
{
	AttrStringAllocatorImpl::CleanPool();
}

//////////////////////////////////////////////////////////////////////////

CNodeLiveWriterRef CWriter::GetRoot()
{
	return CNodeLiveWriterRef(*this, XMLCPB_ROOTNODE_ID);
}

//////////////////////////////////////////////////////////////////////////

CNodeLiveWriter* CWriter::CreateAndAddLiveNode(const char* pName)
{
	assert(m_firstFreeLiveNode < m_liveNodes.size());

	// create and place in vector
	NodeLiveID ID = m_firstFreeLiveNode;
	StringID stringID = m_tableTags.GetStringID(pName);
	CNodeLiveWriter* pNode = m_liveNodes[ID];
	if (!pNode)
	{
		pNode = new CNodeLiveWriter(*this, ID, stringID, m_safecheckIDCounter);
		m_liveNodes[ID] = pNode;
	}
	else
		pNode->Reuse(stringID, m_safecheckIDCounter);

	m_safecheckIDCounter++;

	// find the now first free live node
	bool found = false;
	for (int i = m_firstFreeLiveNode + 1; i < m_liveNodes.size(); ++i)
	{
		CNodeLiveWriter* pNodeIter = m_liveNodes[i];
		if (pNodeIter == NULL || !pNodeIter->IsValid())
		{
			found = true;
			m_firstFreeLiveNode = i;
			break;
		}
	}

	assert(found);

	return pNode;
}

//////////////////////////////////////////////////////////////////////////

CNodeLiveWriter* CWriter::GetNodeLive(NodeLiveID nodeId)
{
	assert(nodeId < m_liveNodes.size());

	return m_liveNodes[nodeId];
}

//////////////////////////////////////////////////////////////////////////

void CWriter::FreeNodeLive(NodeLiveID nodeId)
{
	if (nodeId < m_firstFreeLiveNode)
		m_firstFreeLiveNode = nodeId;
}

//////////////////////////////////////////////////////////////////////////

void CWriter::Done()
{
	CNodeLiveWriter* pRoot = GetNodeLive(XMLCPB_ROOTNODE_ID);
	if (!pRoot->IsDone())
		pRoot->Done();
}

//////////////////////////////////////////////////////////////////////////

void CWriter::NotifyRootNodeStartCompact()
{
	assert(!m_rootAddr.IsValid());    // can only be one

	m_mainBuffer.GetCurrentAddr(m_rootAddr);
}

//////////////////////////////////////////////////////////////////////////
// for debug/statistics only.
#ifdef XMLCPB_COLLECT_STATS

void CWriter::LogStatistics()
{
	CryLog("-----------Binary SaveGame Writer statistics--------------");

	//// live nodes info
	{
		int nodesCreated = 0;
		int children = 0;
		int attrs = 0;
		for (int i = 0; i < m_liveNodes.size(); ++i)
		{
			CNodeLiveWriter* pNode = m_liveNodes[i];
			if (pNode)
			{
				assert(!pNode->IsValid());
				nodesCreated++;
				children += pNode->m_children.capacity();
				attrs += pNode->m_attrs.capacity();
			}
		}
		CryLog("live nodes.  created: %d/%d   children: %d    attrs: %d", nodesCreated, MAX_NUM_LIVE_NODES, children, attrs);
	}

	CryLog("stringtables.    Tags: %d/%d (%d kb)    AttrNames: %d/%d  (%d kb)    stringsData: %d/%d  (%d kb)   total memory string tables: %d kb",
	       m_tableTags.GetNumStrings(), MAX_NUM_TAGS, m_tableTags.GetDataSize() / 1024,
	       m_tableAttrNames.GetNumStrings(), MAX_NUM_NAMES, m_tableAttrNames.GetDataSize() / 1024,
	       m_tableStrData.GetNumStrings(), MAX_NUM_STRDATA, m_tableStrData.GetDataSize() / 1024,
	       (m_tableTags.GetDataSize() + m_tableAttrNames.GetDataSize() + m_tableStrData.GetDataSize()) / 1024
	       );

	CryLog("Nodes.  total: %d   maxNumChildrenPerNode: %d  total children: %d   maxAttrsPerNode:%d  total attrs: %d  AttrSets: %d (%d kb)",
	       CNodeLiveWriter::m_stats.m_totalNodesCreated, CNodeLiveWriter::m_stats.m_maxNumChildren, CNodeLiveWriter::m_stats.m_totalChildren,
	       CNodeLiveWriter::m_stats.m_maxNumAttrs, CNodeLiveWriter::m_stats.m_totalAttrs,
	       m_tableAttrSets.GetNumSets(), m_tableAttrSets.GetDataSize() / 1024);

	{
		uint32 totalSize = m_mainBuffer.GetDataSize() + m_tableAttrNames.GetDataSize() + m_tableStrData.GetDataSize() + m_tableTags.GetDataSize() + sizeof(SFileHeader) + m_tableAttrSets.GetDataSize();
		CryLog("size:  total: %d (%d kb)  nodes basic info: %d  attr: %d   tagsStringTable: %d   attrStringTable: %d  dataStringTable: %d   attrSets: %d",
		       totalSize, totalSize / 1024, CNodeLiveWriter::m_stats.m_totalSizeNodeData, CNodeLiveWriter::m_stats.m_totalSizeAttrData, m_tableTags.GetDataSize(), m_tableAttrNames.GetDataSize(), m_tableStrData.GetDataSize(), m_tableAttrSets.GetDataSize());
	}

	CryLog("-------------------");
}
#endif

//////////////////////////////////////////////////////////////////////////
// high level function for writing data into a file. it will use (or not) zlib compression depending on the cvar
void CWriter::WriteDataIntoFile(void* pSrc, uint32 numBytes)
{
	m_compressor->WriteDataIntoFile(pSrc, numBytes);
}

//////////////////////////////////////////////////////////////////////////

void CWriter::WriteDataIntoMemory(uint8*& rpData, void* pSrc, uint32 numBytes, uint32& outWriteLoc)
{
	assert(!m_isSavingIntoFile);
	assert(rpData);
	assert(pSrc);
	assert(numBytes > 0);

	if (rpData && pSrc && numBytes > 0)
	{
		memcpy(&rpData[outWriteLoc], pSrc, numBytes);
		outWriteLoc += numBytes;
	}
}

//////////////////////////////////////////////////////////////////////////
// file structure:
//
// - header
// - tagsTable
// - attrNamesTable
// - strDataTable
// - nodes data

bool CWriter::FinishWritingFile()
{
	// if this happens, most likely is an overflow of some of the hard coded limits ( which usually is caused by an error in game side ).
	if (m_hasInternalError)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB: ERROR in binary save generation. The savegame is corrupted.");

	if (!m_compressor->m_errorWritingIntoFile)
	{
		if (!m_rootAddr.IsValid())
			Done();

		assert(m_rootAddr.IsValid());

		m_mainBuffer.WriteToFile(); // this actually writes only the last data remains, because it has been writing all along the process.
		m_tableTags.WriteToFile();
		m_tableAttrNames.WriteToFile();
		m_tableStrData.WriteToFile();
		m_tableAttrSets.WriteToFile();

		CreateFileHeader(m_compressor->GetFileHeader());

		m_compressor->FlushZLibBuffer();
	}

	return !m_compressor->m_errorWritingIntoFile;
}

//////////////////////////////////////////////////////////////////////////

bool CWriter::WriteAllIntoMemory(uint8*& rpData, uint32& outSize)
{
	if (!m_rootAddr.IsValid())
		Done();

	SFileHeader fileHeader;
	CreateFileHeader(fileHeader);

	outSize = sizeof(fileHeader);
	outSize += m_tableTags.GetDataSize();
	outSize += m_tableAttrNames.GetDataSize();
	outSize += m_tableStrData.GetDataSize();
	outSize += m_tableAttrSets.GetDataSize();
	outSize += m_mainBuffer.GetDataSize();

	if (outSize > 0)
	{
		rpData = (uint8*)realloc((void*)rpData, outSize * sizeof(uint8));

		uint32 uWriteLoc = 0;
		WriteDataIntoMemory(rpData, &fileHeader, sizeof(fileHeader), uWriteLoc);
		m_mainBuffer.WriteToMemory(rpData, uWriteLoc);
		m_tableTags.WriteToMemory(rpData, uWriteLoc);
		m_tableAttrNames.WriteToMemory(rpData, uWriteLoc);
		m_tableStrData.WriteToMemory(rpData, uWriteLoc);
		m_tableAttrSets.WriteToMemory(rpData, uWriteLoc);
	}

	if (m_hasInternalError)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB: ERROR in binary save-into-memory generation. (probably something wrong in a pooled entity). The data will be corrupted");

	return (outSize > 0);
}

//////////////////////////////////////////////////////////////////////////
// a bit misleading, because the header is used when writing into memory too.

void CWriter::CreateFileHeader(SFileHeader& fileHeader)
{
	assert(m_rootAddr.IsValid());

	m_tableTags.FillFileHeaderInfo(fileHeader.m_tags);
	m_tableAttrNames.FillFileHeaderInfo(fileHeader.m_attrNames);
	m_tableStrData.FillFileHeaderInfo(fileHeader.m_strData);

	fileHeader.m_numAttrSets = m_tableAttrSets.GetNumSets();
	fileHeader.m_sizeAttrSets = m_tableAttrSets.GetSetsDataSize();

	fileHeader.m_sizeNodes = m_mainBuffer.GetUsedMemory();
	fileHeader.m_numNodes = m_counterCompactedNodes;

	fileHeader.m_hasInternalError = m_hasInternalError;
}

} // namespace XMLCPB
