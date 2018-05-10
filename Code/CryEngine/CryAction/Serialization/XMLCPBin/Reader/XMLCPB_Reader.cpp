// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "XMLCPB_Reader.h"
#include "CryActionCVars.h"
#include <CrySystem/ZLib/IZLibCompressor.h>

using namespace XMLCPB;

int CReader::MAX_NUM_LIVE_NODES = 64;

//////////////////////////////////////////////////////////////////////////
_smart_ptr<IGeneralMemoryHeap> CReader::CreateHeap()
{
	return CryGetIMemoryManager()->CreateGeneralExpandingMemoryHeap(8 * 1024 * 1024, 0, "Load heap");
}

//////////////////////////////////////////////////////////////////////////

CReader::CReader(IGeneralMemoryHeap* pHeap)
	: m_pHeap(pHeap)
	, m_liveNodes(NAlloc::GeneralHeapAlloc(pHeap))
	, m_buffer(pHeap)
	, m_tableTags(pHeap)
	, m_tableAttrNames(pHeap)
	, m_tableStrData(pHeap)
	, m_tableAttrSets(pHeap)
	, m_firstFreeLiveNode(0)
	, m_maxNumActiveNodes(0)
	, m_numActiveNodes(0)
	, m_errorReading(false)
	, m_nodesAddrTable(NAlloc::GeneralHeapAlloc(pHeap))
	, m_totalSize(0)
	, m_nodesDataSize(0)
	, m_pZLibBuffer(NULL)
	, m_pZLibCompressedBuffer(NULL)
	, m_ZLibBufferSizeWithData(0)
	, m_ZLibBufferSizeAlreadyRead(0)
	, m_numNodes(0)
{
	InitializeDataTypeInfo();
	m_liveNodes.resize(MAX_NUM_LIVE_NODES, CNodeLiveReader(*this));
}

//////////////////////////////////////////////////////////////////////////

CReader::~CReader()
{
	SAFE_DELETE_ARRAY(m_pZLibBuffer);
	SAFE_DELETE_ARRAY(m_pZLibCompressedBuffer);
	//	CryLog(" Binary SaveGame Reader: max live nodes active in the reader: %d", m_maxNumActiveNodes );
}

//////////////////////////////////////////////////////////////////////////

CNodeLiveReaderRef CReader::GetRoot()
{
	CNodeLiveReaderRef nodeRef(*this, XMLCPB_ROOTNODE_ID);    // root node always have same live ID and is always valid

	assert(m_liveNodes[0].IsValid());
	return nodeRef;
}

//////////////////////////////////////////////////////////////////////////

CNodeLiveReaderRef CReader::CreateNodeRef()
{
	return CNodeLiveReaderRef(*this);
}

//////////////////////////////////////////////////////////////////////////

const CNodeLiveReader& CReader::ActivateLiveNodeFromCompact(NodeGlobalID nodeId)
{
	NodeLiveID liveId = m_firstFreeLiveNode;
	CNodeLiveReader& node = m_liveNodes[liveId];
	node.ActivateFromCompact(liveId, nodeId);

	// find the now first free live node
	bool found = false;
	uint32 size = m_liveNodes.size();
	for (int i = m_firstFreeLiveNode + 1; i < size; ++i)
	{
		const CNodeLiveReader& nodeIter = m_liveNodes[i];
		if (!nodeIter.IsValid())
		{
			found = true;
			m_firstFreeLiveNode = i;
			break;
		}
	}

	assert(found);
	m_numActiveNodes++;
	if (m_numActiveNodes > m_maxNumActiveNodes)
		m_maxNumActiveNodes = m_numActiveNodes;

	return node;
}

//////////////////////////////////////////////////////////////////////////

CNodeLiveReader* CReader::GetNodeLive(NodeLiveID nodeId)
{
	CNodeLiveReader* pNode = NULL;
	assert(nodeId < m_liveNodes.size());

	if (nodeId < m_liveNodes.size())
		pNode = &(m_liveNodes[nodeId]);

	return pNode;
}

//////////////////////////////////////////////////////////////////////////

void CReader::FreeNodeLive(NodeLiveID nodeId)
{
	if (nodeId < m_firstFreeLiveNode)
		m_firstFreeLiveNode = nodeId;

	assert(nodeId < m_liveNodes.size());

	m_liveNodes[nodeId].Reset();
	m_numActiveNodes--;
}

//////////////////////////////////////////////////////////////////////////

void CReader::CheckErrorFlag(IPlatformOS::EFileOperationCode code)
{
	if (!m_errorReading)
		m_errorReading = (code != IPlatformOS::eFOC_Success);
}

//////////////////////////////////////////////////////////////////////////

FlatAddr CReader::ReadFromBuffer(FlatAddr addr, uint8*& rdata, uint32 len)
{
	m_buffer.CopyTo(rdata, addr, len);

	return addr + len;
}

//////////////////////////////////////////////////////////////////////////
// high level function for reading data from the file. it will use (or not) zlib compression depending on the cvar
// TODO: remove all those pOSSaveReader parameter chains and make it a member, passing it along is not needed anymore
// TODO: extract all zlib code into a separate object
void CReader::ReadDataFromFile(IPlatformOS::ISaveReaderPtr& pOSSaveReader, void* pDst, uint32 numBytes)
{
	if (!CCryActionCVars::Get().g_XMLCPBUseExtraZLibCompression)
	{
		ReadDataFromFileInternal(pOSSaveReader, pDst, numBytes);
		return;
	}

	uint32 numBytesLeft = numBytes;
	uint8* pNextDst = (uint8*)pDst;
	do
	{
		ReadDataFromZLibBuffer(pOSSaveReader, pNextDst, numBytesLeft);
	}
	while (numBytesLeft > 0 && !m_errorReading);
}

//////////////////////////////////////////////////////////////////////////
// reads uncompressed data from the buffer, and decompress the next block if needed
// TODO: extract all zlib code into a separate object
void CReader::ReadDataFromZLibBuffer(IPlatformOS::ISaveReaderPtr& pOSSaveReader, uint8*& pDst, uint32& numBytesToRead)
{
	// decompress next block
	if (m_ZLibBufferSizeAlreadyRead == m_ZLibBufferSizeWithData)
	{
		SZLibBlockHeader blockHeader;
		ReadDataFromFileInternal(pOSSaveReader, &blockHeader, sizeof(blockHeader));
		if (!m_errorReading)
		{
			bool isCompressedData = blockHeader.m_compressedSize != SZLibBlockHeader::NO_ZLIB_USED;

			if (isCompressedData)
			{
				assert(blockHeader.m_compressedSize < XMLCPB_ZLIB_BUFFER_SIZE);
				ReadDataFromFileInternal(pOSSaveReader, m_pZLibCompressedBuffer, blockHeader.m_compressedSize);
				if (!m_errorReading)
				{
					size_t uncompressedLength = XMLCPB_ZLIB_BUFFER_SIZE;
					bool ok = gEnv->pSystem->DecompressDataBlock(m_pZLibCompressedBuffer, blockHeader.m_compressedSize, m_pZLibBuffer, uncompressedLength);
					if (!ok)
						m_errorReading = true;
					m_ZLibBufferSizeWithData = uncompressedLength;
				}
			}
			else // when is not compressed data, reads directly into the uncompressed buffer
			{
				ReadDataFromFileInternal(pOSSaveReader, m_pZLibBuffer, blockHeader.m_uncompressedSize);
				m_ZLibBufferSizeWithData = blockHeader.m_uncompressedSize;
			}
			m_ZLibBufferSizeAlreadyRead = 0;
		}
	}

	// read from the decompressed data
	uint32 bytesAvailableInBuffer = m_ZLibBufferSizeWithData - m_ZLibBufferSizeAlreadyRead;
	uint32 bytesToCopy = min(bytesAvailableInBuffer, numBytesToRead);
	memcpy(pDst, m_pZLibBuffer + m_ZLibBufferSizeAlreadyRead, bytesToCopy);

	m_ZLibBufferSizeAlreadyRead += bytesToCopy;
	pDst += bytesToCopy;
	numBytesToRead -= bytesToCopy;
}

//////////////////////////////////////////////////////////////////////////
// low level function, reads directly from the file
// TODO: remove all those pOSSaveReader parameter chains and make it a member, passing it along is not needed anymore
void CReader::ReadDataFromFileInternal(IPlatformOS::ISaveReaderPtr& pOSSaveReader, void* pDst, uint32 numBytes)
{
	assert(pOSSaveReader.get());

	if (!m_errorReading)
	{
		IPlatformOS::EFileOperationCode code = pOSSaveReader->ReadBytes(pDst, numBytes);
		CheckErrorFlag(code);
	}
}

//////////////////////////////////////////////////////////////////////////
// file structure: see CWriter::WriteBinaryFile

bool CReader::ReadBinaryFile(const char* pFileName)
{
	IPlatformOS::ISaveReaderPtr pOSSaveReader = gEnv->pSystem->GetPlatformOS()->SaveGetReader(pFileName);
	if (!m_pZLibBuffer)
		m_pZLibBuffer = new uint8[XMLCPB_ZLIB_BUFFER_SIZE];
	if (!m_pZLibCompressedBuffer)
		m_pZLibCompressedBuffer = new uint8[XMLCPB_ZLIB_BUFFER_SIZE];
	m_errorReading = pOSSaveReader.get() == NULL;

	if (!m_errorReading)
	{
		size_t totalNumBytesInFile = 0;
		IPlatformOS::EFileOperationCode code = pOSSaveReader->GetNumBytes(totalNumBytesInFile);
		m_totalSize = totalNumBytesInFile;
		CheckErrorFlag(code);

		if (!m_errorReading)
		{
			SFileHeader fileHeader;
			pOSSaveReader->Seek(-int(sizeof(fileHeader)), IPlatformOS::ISaveReader::ESM_END);
			ReadDataFromFileInternal(pOSSaveReader, &fileHeader, sizeof(fileHeader));
#ifdef XMLCPB_CHECK_FILE_INTEGRITY
			m_errorReading = CheckFileCorruption(pOSSaveReader, fileHeader, m_totalSize);
#endif

			pOSSaveReader->Seek(0, IPlatformOS::ISaveReader::ESM_BEGIN);

			if (fileHeader.m_fileTypeCheck != fileHeader.FILETYPECHECK)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: file type signature not correct. Savegame File corrupted!");
				m_errorReading = true;
			}

			if (!m_errorReading)
			{
				m_nodesDataSize = fileHeader.m_sizeNodes;
				m_numNodes = fileHeader.m_numNodes;
				m_buffer.ReadFromFile(*this, pOSSaveReader, fileHeader.m_sizeNodes);
				m_tableTags.ReadFromFile(*this, pOSSaveReader, fileHeader.m_tags);
				m_tableAttrNames.ReadFromFile(*this, pOSSaveReader, fileHeader.m_attrNames);
				m_tableStrData.ReadFromFile(*this, pOSSaveReader, fileHeader.m_strData);
				m_tableAttrSets.ReadFromFile(*this, pOSSaveReader, fileHeader);
				CreateNodeAddressTables();
			}

			if (!m_errorReading)
			{
				const CNodeLiveReader& root = ActivateLiveNodeFromCompact(m_numNodes - 1);   // the last node is always the root
				assert(root.GetLiveId() == XMLCPB_ROOTNODE_ID);

				pOSSaveReader->TouchFile();
			}
		}

		pOSSaveReader->Close();
	}

	CryLog("[LOAD GAME] --Binary saveload: reading done--");

	if (m_errorReading)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: while reading the file: '%s'", pFileName);

	return !m_errorReading;
}

//////////////////////////////////////////////////////////////////////////
// The current implementation is not console friendly: it causes the savegame file to be read 2 times, and it uses an extra memory block the size of the full savegame.
// This is not a problem because right now this integrity check is used only on the PC version.
// But if at any point we need it for consoles too, we probably will need to make it more efficient.
//   Using an md5 check on each block instead of the current full file check would probably be the right way. That could be done on the fly, without any extra reading or memory reservation.
#ifdef XMLCPB_CHECK_FILE_INTEGRITY

bool CReader::CheckFileCorruption(IPlatformOS::ISaveReaderPtr& pOSSaveReader, const SFileHeader& fileHeader, uint32 totalSize)
{
	bool error = false;

	if (totalSize <= sizeof(fileHeader))
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: size check failed. savegame File corrupted! (probably not fully saved)");
		error = true;
		return error;
	}

	uint32 sizeToCheck = totalSize - sizeof(fileHeader);

	char* pBuf = new char[sizeToCheck];
	pOSSaveReader->Seek(0, IPlatformOS::ISaveReader::ESM_BEGIN);
	ReadDataFromFileInternal(pOSSaveReader, pBuf, sizeToCheck);

	IZLibCompressor* pZLib = GetISystem()->GetIZLibCompressor();

	if (!pZLib)
	{
		SAFE_DELETE_ARRAY(pBuf);
		error = true;
		return error;
	}

	SMD5Context context;
	char MD5signature[SFileHeader::MD5_SIGNATURE_SIZE];
	pZLib->MD5Init(&context);
	pZLib->MD5Update(&context, pBuf, sizeToCheck);

	SFileHeader tempFileHeader = fileHeader;
	for (uint32 i = 0; i < SFileHeader::MD5_SIGNATURE_SIZE; ++i)
		tempFileHeader.m_MD5Signature[i] = 0;                                                        // the original signature is always calculated with this zeroed.
	pZLib->MD5Update(&context, (const char*)(&tempFileHeader), sizeof(tempFileHeader));
	pZLib->MD5Final(&context, MD5signature);

	SAFE_DELETE_ARRAY(pBuf);

	for (uint32 i = 0; i < SFileHeader::MD5_SIGNATURE_SIZE; ++i)
	{
		if (fileHeader.m_MD5Signature[i] != MD5signature[i])
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: md5 check failed. savegame File corrupted!");
			error = true;
			break;
		}
	}

	return error;
}

#endif

//////////////////////////////////////////////////////////////////////////

void CReader::ReadDataFromMemory(const uint8* pData, uint32 dataSize, void* pSrc, uint32 numBytes, uint32& outReadLoc)
{
	assert(pData);
	assert(pSrc);

	if (!m_errorReading && pData && pSrc && numBytes > 0)
	{
		const int dataRemaining = (dataSize - numBytes + outReadLoc);
		m_errorReading = (dataRemaining < 0);

		if (!m_errorReading)
		{
			memcpy(pSrc, &pData[outReadLoc], numBytes);
			outReadLoc += numBytes;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CReader::ReadBinaryMemory(const uint8* pData, uint32 uSize)
{
	m_errorReading = !(pData && uSize > 0);

	if (!m_errorReading)
	{
		uint32 uReadLoc = 0;
		m_totalSize = uSize;

		SFileHeader fileHeader;
		ReadDataFromMemory(pData, uSize, &fileHeader, sizeof(fileHeader), uReadLoc);

		if (fileHeader.m_fileTypeCheck != fileHeader.FILETYPECHECK)
			m_errorReading = true;

		if (!m_errorReading)
		{
			m_nodesDataSize = fileHeader.m_sizeNodes;
			m_numNodes = fileHeader.m_numNodes;
			m_buffer.ReadFromMemory(*this, pData, uSize, fileHeader.m_sizeNodes, uReadLoc);
			m_tableTags.ReadFromMemory(*this, pData, uSize, fileHeader.m_tags, uReadLoc);
			m_tableAttrNames.ReadFromMemory(*this, pData, uSize, fileHeader.m_attrNames, uReadLoc);
			m_tableStrData.ReadFromMemory(*this, pData, uSize, fileHeader.m_strData, uReadLoc);
			m_tableAttrSets.ReadFromMemory(*this, pData, uSize, fileHeader, uReadLoc);
			CreateNodeAddressTables();
		}

		if (!m_errorReading)
		{
			const CNodeLiveReader& root = ActivateLiveNodeFromCompact(m_numNodes - 1);   // the last node is always the root
			assert(root.GetLiveId() == XMLCPB_ROOTNODE_ID);
		}
	}

	if (m_errorReading)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "XMLCPB ERROR: while reading from memory location");

	return !m_errorReading;
}

//////////////////////////////////////////////////////////////////////////
// for debug purposes
#ifndef _RELEASE
void CReader::SaveTestFiles()
{
	m_tableTags.WriteStringsIntoTextFile("tableTags.txt");
	m_tableAttrNames.WriteStringsIntoTextFile("tableAttrNames.txt");
	m_tableStrData.WriteStringsIntoTextFile("tableStrData.txt");
}
#endif

//////////////////////////////////////////////////////////////////////////

void CReader::CreateNodeAddressTables()
{
	m_nodesAddrTable.resize(m_numNodes);

	FlatAddr addr = 0;

	// TODO (improve): first, because the partially initialized table is used in the node calls,
	//   and also because those nodes are temporary and manually activated just to calculate the address of the next node
	for (uint32 n = 0; n < m_numNodes; ++n)
	{
		m_nodesAddrTable[n] = addr;
		CNodeLiveReader node(*this);
		node.ActivateFromCompact(0, n);
		addr = node.GetAddrNextNode();
		assert(addr > m_nodesAddrTable[n]);
	}

	assert(addr == m_nodesDataSize);
}
