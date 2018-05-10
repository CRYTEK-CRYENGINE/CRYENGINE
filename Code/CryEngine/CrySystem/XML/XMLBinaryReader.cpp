// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Smith
//---------------------------------------------------------------------------

#include "StdAfx.h"
#include "XMLBinaryReader.h"
#include "XMLBinaryNode.h"

XMLBinary::XMLBinaryReader::XMLBinaryReader()
{
	m_errorDescription[0] = 0;
}

XMLBinary::XMLBinaryReader::~XMLBinaryReader()
{
}

const char* XMLBinary::XMLBinaryReader::GetErrorDescription() const
{
	return &m_errorDescription[0];
}

void XMLBinary::XMLBinaryReader::SetErrorDescription(const char* text)
{
	cry_strcpy(m_errorDescription, text);
}

XmlNodeRef XMLBinary::XMLBinaryReader::LoadFromFile(
  const char* filename,
  XMLBinary::XMLBinaryReader::EResult& result)
{
	m_errorDescription[0] = 0;
	result = eResult_Error;

	CCryFile xmlFile;

	if (!xmlFile.Open(filename, "rb"))
	{
		SetErrorDescription("Can't open file.");
		return 0;
	}

	const size_t fileSize = xmlFile.GetLength();
	if (fileSize < sizeof(BinaryFileHeader))
	{
		result = eResult_NotBinXml;
		SetErrorDescription("File is not a binary XML file (file size is too small).");
		return 0;
	}

	// Read in the entire file - this buffer will not be deallocated immediately, since the nodes
	// will contain pointers directly into it. It will be deleted once the reference count on the
	// CBinaryXmlData object reaches 0 again.

	char* const pFileContents = new char[fileSize];

	if (!pFileContents)
	{
		SetErrorDescription("Can't allocate memory for binary XML file contents.");
		return 0;
	}

	if (xmlFile.ReadRaw(pFileContents, fileSize) != fileSize)
	{
		delete[] pFileContents;
		SetErrorDescription("Failed to read binary XML file, the file is corrupt.");
		return 0;
	}

	Check(pFileContents, fileSize, result);

	if (result != eResult_Success)
	{
		delete[] pFileContents;
		return 0;
	}

	CBinaryXmlData* const pData = Create(pFileContents, fileSize, result);

	if (result != eResult_Success)
	{
		assert(pData == 0);
		delete[] pFileContents;
		// cppcheck-suppress memleak
		return 0;
	}

	assert(pData);
	pData->bOwnsFileContentsMemory = true;

	// Return first node
	return &pData->pBinaryNodes[0];
}

XmlNodeRef XMLBinary::XMLBinaryReader::LoadFromBuffer(
  EBufferMemoryHandling bufferMemoryHandling,
  const char* buffer,
  size_t size,
  XMLBinary::XMLBinaryReader::EResult& result)
{
	m_errorDescription[0] = 0;
	result = eResult_Error;

	Check(buffer, size, result);

	if (result != eResult_Success)
	{
		return 0;
	}

	CBinaryXmlData* pData = 0;

	if (bufferMemoryHandling == eBufferMemoryHandling_MakeCopy)
	{
		char* ownBuffer = new char[size];
		if (!ownBuffer)
		{
			SetErrorDescription("Can't allocate memory for binary XML data.");
			return 0;
		}
		memcpy(ownBuffer, buffer, size);

		pData = Create(ownBuffer, size, result);

		if (result != eResult_Success)
		{
			assert(pData == 0);
			delete[] ownBuffer;
			// cppcheck-suppress memleak
			return 0;
		}
		// cppcheck-suppress memleak
	}
	else
	{
		assert(bufferMemoryHandling == eBufferMemoryHandling_TakeOwnership);

		pData = Create(buffer, size, result);

		if (result != eResult_Success)
		{
			assert(pData == 0);
			return 0;
		}
	}

	assert(pData);
	pData->bOwnsFileContentsMemory = true;

	// Return first node
	return &pData->pBinaryNodes[0];
}

void XMLBinary::XMLBinaryReader::Check(const char* buffer, size_t size, EResult& result)
{
	m_errorDescription[0] = 0;
	result = eResult_Error;

	if (buffer == 0)
	{
		SetErrorDescription("Buffer is null.");
		return;
	}

	if (size < sizeof(BinaryFileHeader))
	{
		result = eResult_NotBinXml;
		SetErrorDescription("Not a binary XML - data size is too small.");
		return;
	}

	const BinaryFileHeader& header = *(reinterpret_cast<const BinaryFileHeader*>(buffer));

	CheckHeader(header, size, result);
}

void XMLBinary::XMLBinaryReader::CheckHeader(const BinaryFileHeader& header, size_t size, EResult& result)
{
	assert(size >= sizeof(BinaryFileHeader));

	m_errorDescription[0] = 0;

	// Check the signature of the file to make sure that it is a binary XML file.
	{
		static const char signature[] = "CryXmlB";
		static_assert(sizeof(signature) == sizeof(header.szSignature), "Wrong signature size!");
		if (memcmp(header.szSignature, signature, sizeof(header.szSignature)) != 0)
		{
			result = eResult_NotBinXml;
			SetErrorDescription("Not a binary XML - has no signature.");
			return;
		}
	}

	// Check contents of the file header.
	const uint32 nNodeTableEnd = header.nNodeTablePosition + header.nNodeCount * sizeof(Node);
	const uint32 nChildTableEnd = header.nChildTablePosition + header.nChildCount * sizeof(NodeIndex);
	const uint32 nAttributeTableEnd = header.nAttributeTablePosition + header.nAttributeCount * sizeof(Attribute);
	const uint32 nStringDataEnd = header.nStringDataPosition + header.nStringDataSize;

	bool bCorrupt = false;
	bCorrupt = bCorrupt || header.nXMLSize > size;
	bCorrupt = bCorrupt || nNodeTableEnd > header.nChildTablePosition;
	bCorrupt = bCorrupt || nChildTableEnd > header.nAttributeTablePosition;
	bCorrupt = bCorrupt || nAttributeTableEnd > header.nStringDataPosition;
	bCorrupt = bCorrupt || nStringDataEnd > header.nXMLSize;
	if (bCorrupt)
	{
		result = eResult_Error;
		SetErrorDescription("Binary XML data is corrupt.");
		return;
	}

	result = eResult_Success;
}

CBinaryXmlData* XMLBinary::XMLBinaryReader::Create(const char* buffer, size_t size, EResult& result)
{
	assert((buffer != 0) && (size >= sizeof(BinaryFileHeader)));

	m_errorDescription[0] = 0;
	result = eResult_Error;

	CBinaryXmlData* const pData = new CBinaryXmlData;
	if (!pData)
	{
		SetErrorDescription("Can't allocate memory for binary XML object.");
		return 0;
	}

	pData->pFileContents = buffer;
	pData->nFileSize = size;
	pData->bOwnsFileContentsMemory = false;

	const BinaryFileHeader& header = *(reinterpret_cast<const BinaryFileHeader*>(buffer));

	// Create nodes
	pData->pBinaryNodes = new CBinaryXmlNode[header.nNodeCount];
	if (!pData->pBinaryNodes)
	{
		delete pData;
		SetErrorDescription("Can't allocate memory for binary XML nodes.");
		return 0;
	}

	pData->pAttributes = reinterpret_cast<const Attribute*>(buffer + header.nAttributeTablePosition);
	pData->pChildIndices = reinterpret_cast<const NodeIndex*>(buffer + header.nChildTablePosition);
	pData->pNodes = reinterpret_cast<const Node*>(buffer + header.nNodeTablePosition);
	pData->pStringData = buffer + header.nStringDataPosition;

	for (uint32 nNode = 0; nNode < header.nNodeCount; ++nNode)
	{
		CBinaryXmlNode* const pNode = &pData->pBinaryNodes[nNode];
		pNode->m_nRefCount = 0;
		pNode->m_pData = pData;
	}

	result = eResult_Success;
	return pData;
}
