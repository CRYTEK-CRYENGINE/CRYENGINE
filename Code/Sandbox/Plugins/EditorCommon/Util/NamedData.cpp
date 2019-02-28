// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NamedData.h"
#include <zlib.h>
#include "Util/PakFile.h"
#include "Util/CryMemFile.h"

IMPLEMENT_SERIAL(CNamedData, CObject, 1)

//////////////////////////////////////////////////////////////////////////
CNamedData::CNamedData()
{
}

//////////////////////////////////////////////////////////////////////////
CNamedData::~CNamedData()
{
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CNamedData::AddDataBlock(const string& blockName, void* pData, int nSize, bool bCompress)
{
	assert(pData);
	assert(nSize > 0);

	DataBlock* pBlock = stl::find_in_map(m_blocks, blockName, (DataBlock*)0);
	if (pBlock)
	{
		delete pBlock;
	}

	pBlock = new DataBlock;

	pBlock->bFastCompression = !bCompress;

	bCompress = false;

	if (bCompress)
	{
		pBlock->bCompressed = true;
		CMemoryBlock temp;
		temp.Attach(pData, nSize);
		temp.Compress(pBlock->compressedData);
	}
	else
	{
		pBlock->bCompressed = false;
		pBlock->data.Allocate(nSize);
		pBlock->data.Copy(pData, nSize);
	}
	m_blocks[blockName] = pBlock;
}

void CNamedData::AddDataBlock(const string& blockName, CMemoryBlock& mem)
{
	DataBlock* pBlock = stl::find_in_map(m_blocks, blockName, (DataBlock*)0);
	if (pBlock)
	{
		delete pBlock;
	}
	pBlock = new DataBlock;

	pBlock->bFastCompression = false;

	if (mem.GetUncompressedSize() != 0)
	{
		// This is compressed block.
		pBlock->bCompressed = true;
		pBlock->compressedData = mem;
	}
	else
	{
		pBlock->bCompressed = false;
		pBlock->data = mem;
	}
	m_blocks[blockName] = pBlock;
}

//////////////////////////////////////////////////////////////////////////
void CNamedData::Clear()
{
	for (TBlocks::iterator it = m_blocks.begin(); it != m_blocks.end(); ++it)
	{
		delete it->second;
	}
	m_blocks.clear();
}

//////////////////////////////////////////////////////////////////////////
bool CNamedData::GetDataBlock(const string& blockName, void*& pData, int& nSize)
{
	pData = 0;
	nSize = 0;

	bool bUncompressed = false;
	CMemoryBlock* mem = GetDataBlock(blockName, bUncompressed);
	if (mem)
	{
		pData = mem->GetBuffer();
		nSize = mem->GetSize();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
CMemoryBlock* CNamedData::GetDataBlock(const string& blockName, bool& bCompressed)
{
	DataBlock* pBlock = stl::find_in_map(m_blocks, blockName, (DataBlock*)0);
	if (!pBlock)
		return 0;

	if (bCompressed)
	{
		// Return compressed data.
		if (!pBlock->compressedData.IsEmpty())
			return &pBlock->compressedData;
	}
	else
	{
		// Return normal data.
		if (!pBlock->data.IsEmpty())
		{
			return &pBlock->data;
		}
		else
		{
			// Uncompress compressed block.
			if (!pBlock->compressedData.IsEmpty())
			{
				pBlock->compressedData.Uncompress(pBlock->data);
				return &pBlock->data;
			}
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CNamedData::Serialize(CArchive& ar)
{
	CObject::Serialize(ar);

	if (ar.IsStoring())
	{
		int iSize = m_blocks.size();
		ar << iSize;

		for (TBlocks::iterator it = m_blocks.begin(); it != m_blocks.end(); it++)
		{
			string key = it->first;
			DataBlock* pBlock = it->second;

			unsigned int nOriginalSize;
			unsigned int nSizeFlags;
			unsigned int flags = 0;

			if (pBlock->bCompressed)
			{
				nOriginalSize = pBlock->compressedData.GetUncompressedSize();
				// Compressed data.
				unsigned long destSize = pBlock->compressedData.GetSize();
				void* dest = pBlock->compressedData.GetBuffer();
				nSizeFlags = destSize | (1 << 31);

				ar << key;
				ar << nSizeFlags;    // Current size of data + 1 bit for compressed flag.
				ar << nOriginalSize; // Size of uncompressed data.
				ar << flags;         // Some additional flags.
				ar.Write(dest, destSize);
			}
			else
			{
				nOriginalSize = pBlock->data.GetSize();
				void* dest = pBlock->data.GetBuffer();

				nSizeFlags = nOriginalSize;
				ar << key;
				ar << nSizeFlags;
				ar << nOriginalSize; // Size of uncompressed data.
				ar << flags;         // Some additional flags.
				ar.Write(dest, nOriginalSize);
			}
		}
	}
	else
	{
		Clear();

		int iSize;
		ar >> iSize;

		for (int i = 0; i < iSize; i++)
		{
			CString key;
			unsigned int nSizeFlags = 0;
			unsigned int nSize = 0;
			unsigned int nOriginalSize = 0;
			unsigned int flags = 0;
			bool bCompressed = false;

			DataBlock* pBlock = new DataBlock;

			ar >> key;
			ar >> nSizeFlags;
			ar >> nOriginalSize;
			ar >> flags;

			nSize = nSizeFlags & (~(1 << 31));
			bCompressed = (nSizeFlags & (1 << 31)) != 0;

			if (nSize)
			{
				if (bCompressed)
				{
					pBlock->compressedData.Allocate(nSize, nOriginalSize);
					void* pSrcData = pBlock->compressedData.GetBuffer();
					// Read compressed data.
					ar.Read(pSrcData, nSize);
				}
				else
				{
					pBlock->data.Allocate(nSize);
					void* pSrcData = pBlock->data.GetBuffer();

					// Read uncompressed data.
					ar.Read(pSrcData, nSize);
				}
			}
			m_blocks[key.GetString()] = pBlock;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CNamedData::Save(CPakFile& pakFile)
{
	for (TBlocks::iterator it = m_blocks.begin(); it != m_blocks.end(); it++)
	{
		string key = it->first;
		DataBlock* pBlock = it->second;
		if (!pBlock->bCompressed)
		{
			string filename = key + ".editor_data";
			pakFile.UpdateFile(filename, pBlock->data, true, pBlock->bFastCompression ? ICryArchive::LEVEL_FASTEST : ICryArchive::LEVEL_BETTER);
		}
		else
		{
			int nOriginalSize = pBlock->compressedData.GetUncompressedSize();
			CCryMemFile memFile;
			// Write uncompressed data size.
			memFile.Write(&nOriginalSize, sizeof(nOriginalSize));
			// Write compressed data.
			memFile.Write(pBlock->compressedData.GetBuffer(), pBlock->compressedData.GetSize());
			pakFile.UpdateFile(key + ".editor_datac", memFile, false);
		}
	}
}

void CNamedData::Save(const string& levelPath)
{
	ICryPak* const pCryPak = GetISystem()->GetIPak();

	for (TBlocks::iterator it = m_blocks.begin(); it != m_blocks.end(); it++)
	{
		string key = it->first;
		DataBlock* pBlock = it->second;
		if (!pBlock->bCompressed)
		{
			const string filename = PathUtil::Make(levelPath, key, "editor_data");
			FILE* pFile = pCryPak->FOpen(filename, "wb");
			if (pFile)
			{
				pCryPak->FWrite(pBlock->data.GetBuffer(), pBlock->data.GetSize(), 1, pFile);
				pCryPak->FClose(pFile);
			}
		}
		else
		{
			const string filename = PathUtil::Make(levelPath, key, "editor_datac");
			FILE* pFile = pCryPak->FOpen(filename, "wb");
			if (pFile)
			{
				// Write uncompressed data size.
				int nOriginalSize = pBlock->compressedData.GetUncompressedSize();
				pCryPak->FWrite(&nOriginalSize, sizeof(nOriginalSize), 1, pFile);
				// Write compressed data.
				pCryPak->FWrite(pBlock->compressedData.GetBuffer(), pBlock->compressedData.GetSize(), 1, pFile);
				pCryPak->FClose(pFile);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CNamedData::Load(const string& levelPath)
{
	int i;
	std::vector<string> files;
	GetIEditor()->ScanDirectory(levelPath, "*.editor_data", files);
	for (i = 0; i < files.size(); i++)
	{
		const string& filename = files[i];
		CCryFile cfile;
		if (cfile.Open(PathUtil::Make(levelPath, filename), "rb"))
		{
			int fileSize = cfile.GetLength();
			if (fileSize > 0)
			{
				string key = PathUtil::GetFileName(filename);
				// Read data block.
				DataBlock* pBlock = new DataBlock;
				pBlock->data.Allocate(fileSize);
				cfile.ReadRaw(pBlock->data.GetBuffer(), fileSize);
				m_blocks[key] = pBlock;
			}
		}
	}
	files.clear();
	// Scan compressed data.
	GetIEditor()->ScanDirectory(levelPath, "*.editor_datac", files);
	for (i = 0; i < files.size(); i++)
	{
		const string& filename = files[i];
		CCryFile cfile;
		if (cfile.Open(PathUtil::Make(levelPath, filename), "rb"))
		{
			int fileSize = cfile.GetLength();
			if (fileSize > 0)
			{
				// Read uncompressed data size.
				int nOriginalSize = 0;
				cfile.ReadType(&nOriginalSize);
				// Read uncompressed data.
				int nDataSize = fileSize - sizeof(nOriginalSize);

				string key = PathUtil::GetFileName(filename);
				// Read data block.
				DataBlock* pBlock = new DataBlock;
				pBlock->compressedData.Allocate(nDataSize, nOriginalSize);
				cfile.ReadRaw(pBlock->compressedData.GetBuffer(), nDataSize);
				m_blocks[key] = pBlock;
			}
		}
	}

	return true;
}


