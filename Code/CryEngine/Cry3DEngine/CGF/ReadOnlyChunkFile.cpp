// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ReadOnlyChunkFile.h
//  Created:     2004/11/15 by Timur
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ReadOnlyChunkFile.h"
#include "ChunkFileComponents.h"
#include "ChunkFileReaders.h"

#define MAX_CHUNKS_NUM 10000000

#if !defined(FUNCTION_PROFILER_3DENGINE)
	#define FUNCTION_PROFILER_3DENGINE
#endif

//////////////////////////////////////////////////////////////////////////
CReadOnlyChunkFile::CReadOnlyChunkFile(bool bCopyFileData, bool bNoWarningMode)
{
	m_pFileBuffer = 0;
	m_nBufferSize = 0;

	m_bNoWarningMode = bNoWarningMode;

	m_bOwnFileBuffer = false;
	m_bLoaded = false;
	m_bCopyFileData = bCopyFileData;

	m_hFile = 0;
}

CReadOnlyChunkFile::~CReadOnlyChunkFile()
{
	CloseFile();
	FreeBuffer();
}

//////////////////////////////////////////////////////////////////////////
void CReadOnlyChunkFile::CloseFile()
{
	if (m_hFile)
	{
		gEnv->pCryPak->FClose(m_hFile);
		m_hFile = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CReadOnlyChunkFile::FreeBuffer()
{
	if (m_pFileBuffer && m_bOwnFileBuffer)
	{
		delete[] m_pFileBuffer;
	}
	m_pFileBuffer = 0;
	m_nBufferSize = 0;
	m_bOwnFileBuffer = false;
	m_bLoaded = false;
}

//////////////////////////////////////////////////////////////////////////
CReadOnlyChunkFile::ChunkDesc* CReadOnlyChunkFile::GetChunk(int nIndex)
{
	assert(size_t(nIndex) < m_chunks.size());
	return &m_chunks[nIndex];
}

//////////////////////////////////////////////////////////////////////////
const CReadOnlyChunkFile::ChunkDesc* CReadOnlyChunkFile::GetChunk(int nIndex) const
{
	assert(size_t(nIndex) < m_chunks.size());
	return &m_chunks[nIndex];
}

// number of chunks
int CReadOnlyChunkFile::NumChunks() const
{
	return (int)m_chunks.size();
}

//////////////////////////////////////////////////////////////////////////
CReadOnlyChunkFile::ChunkDesc* CReadOnlyChunkFile::FindChunkByType(ChunkTypes nChunkType)
{
	for (size_t i = 0, count = m_chunks.size(); i < count; ++i)
	{
		if (m_chunks[i].chunkType == nChunkType)
		{
			return &m_chunks[i];
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CReadOnlyChunkFile::ChunkDesc* CReadOnlyChunkFile::FindChunkById(int id)
{
	ChunkDesc chunkToFind;
	chunkToFind.chunkId = id;

	std::vector<ChunkDesc>::iterator it = std::lower_bound(m_chunks.begin(), m_chunks.end(), chunkToFind, IChunkFile::ChunkDesc::LessId);
	if (it != m_chunks.end() && id == (*it).chunkId)
	{
		return &(*it);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CReadOnlyChunkFile::ReadChunkTableFromBuffer()
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_pFileBuffer == 0)
	{
		m_LastError.Format("Unexpected empty buffer");
		return false;
	}

	{
		ChunkFile::MemoryReader f;

		if (!f.Start(m_pFileBuffer, m_nBufferSize))
		{
			m_LastError.Format("Empty memory chunk file");
			return false;
		}

		bool bStripHeaders = false;
		const char* err = 0;

		err = ChunkFile::GetChunkTableEntries_0x746(&f, m_chunks);
		if (err)
		{
			err = ChunkFile::GetChunkTableEntries_0x744_0x745(&f, m_chunks);
			bStripHeaders = true;
		}

		if (!err)
		{
			for (size_t i = 0; i < m_chunks.size(); ++i)
			{
				ChunkDesc& cd = m_chunks[i];
				cd.data = m_pFileBuffer + cd.fileOffset;
			}
			if (bStripHeaders)
			{
				err = ChunkFile::StripChunkHeaders_0x744_0x745(&f, m_chunks);
			}
		}

		if (err)
		{
			m_LastError = err;
			return false;
		}
	}

	// Sort chunks by Id, for faster queries later (see FindChunkById()).
	std::sort(m_chunks.begin(), m_chunks.end(), IChunkFile::ChunkDesc::LessId);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CReadOnlyChunkFile::Read(const char* filename)
{
	FUNCTION_PROFILER_3DENGINE;

	CloseFile();
	FreeBuffer();

	m_hFile = gEnv->pCryPak->FOpen(filename, "rb", (m_bNoWarningMode ? ICryPak::FOPEN_HINT_QUIET : 0));
	if (!m_hFile)
	{
		m_LastError.Format("Failed to open file '%s'", filename);
		return false;
	}

	size_t nFileSize = 0;

	if (m_bCopyFileData)
	{
		nFileSize = gEnv->pCryPak->FGetSize(m_hFile);
		m_pFileBuffer = new char[nFileSize];
		m_bOwnFileBuffer = true;
		if (gEnv->pCryPak->FReadRawAll(m_pFileBuffer, nFileSize, m_hFile) != nFileSize)
		{
			m_LastError.Format("Failed to read %u bytes from file '%s'", (uint)nFileSize, filename);
			return false;
		}
	}
	else
	{
		m_pFileBuffer = (char*)gEnv->pCryPak->FGetCachedFileData(m_hFile, nFileSize);
		m_bOwnFileBuffer = false;
	}

	if (!m_pFileBuffer)
	{
		m_LastError.Format("Failed to get memory for file '%s'", filename);
		return false;
	}

	m_nBufferSize = nFileSize;

	if (!ReadChunkTableFromBuffer())
	{
		return false;
	}

	m_bLoaded = true;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CReadOnlyChunkFile::ReadFromMemory(const void* pData, int nDataSize)
{
	FUNCTION_PROFILER_3DENGINE;

	CloseFile();
	FreeBuffer();

	m_pFileBuffer = (char*)pData;
	m_bOwnFileBuffer = false;
	m_nBufferSize = nDataSize;

	if (!ReadChunkTableFromBuffer())
	{
		return false;
	}
	m_bLoaded = true;
	return true;
}
