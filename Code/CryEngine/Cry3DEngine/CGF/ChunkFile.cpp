// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ChunkFile.h
//  Created:     2004/11/15/11 by Timur
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ChunkFile.h"
#include "ChunkFileReaders.h"
#include "ChunkFileWriters.h"

#if !defined(FUNCTION_PROFILER_3DENGINE)
	#define FUNCTION_PROFILER_3DENGINE
#endif

#if !defined(LOADING_TIME_PROFILE_SECTION)
	#define LOADING_TIME_PROFILE_SECTION
#endif

namespace
{
inline bool ChunkLessOffset(const IChunkFile::ChunkDesc* const p0, const IChunkFile::ChunkDesc* const p1)
{
	return IChunkFile::ChunkDesc::LessOffset(*p0, *p1);
}
}

CChunkFile::CChunkFile()
	: m_pInternalData(NULL)
{
	Clear();
}

CChunkFile::~CChunkFile()
{
	Clear();
}

void CChunkFile::Clear()
{
	ReleaseMemoryBuffer();
	ReleaseChunks();

	m_nLastChunkId = 0;
	m_pInternalData = NULL;
	m_bLoaded = false;
}

//////////////////////////////////////////////////////////////////////////
CChunkFile::ChunkDesc* CChunkFile::GetChunk(int nChunkIdx)
{
	assert(nChunkIdx >= 0 && nChunkIdx < (int)m_chunks.size());
	return m_chunks[nChunkIdx];
}

const CChunkFile::ChunkDesc* CChunkFile::GetChunk(int nChunkIdx) const
{
	assert(nChunkIdx >= 0 && nChunkIdx < (int)m_chunks.size());
	return m_chunks[nChunkIdx];
}

// number of chunks
int CChunkFile::NumChunks() const
{
	return (int)m_chunks.size();
}

//////////////////////////////////////////////////////////////////////////
int CChunkFile::AddChunk(ChunkTypes chunkType, int chunkVersion, EEndianness eEndianness, const void* chunkData, int chunkSize)
{
	ChunkDesc* const pChunk = new ChunkDesc;

	pChunk->bSwapEndian = (eEndianness == eEndianness_NonNative);

	// This block of code is used for debugging only
	if (false)
	{
		for (size_t i = 0, n = m_chunks.size(); i < n; ++i)
		{
			if (m_chunks[i]->bSwapEndian != pChunk->bSwapEndian)
			{
				break;
			}
		}
	}

	pChunk->chunkType = chunkType;
	pChunk->chunkVersion = chunkVersion;

	const int chunkId = ++m_nLastChunkId;
	pChunk->chunkId = chunkId;

	pChunk->data = new char[chunkSize];
	pChunk->size = chunkSize;
	memcpy(pChunk->data, chunkData, chunkSize);

	m_chunks.push_back(pChunk);
	m_chunkIdMap[chunkId] = pChunk;

	return chunkId;
}

//////////////////////////////////////////////////////////////////////////
void CChunkFile::DeleteChunkById(int nChunkId)
{
	for (size_t i = 0, n = m_chunks.size(); i < n; ++i)
	{
		if (m_chunks[i]->chunkId == nChunkId)
		{
			m_chunkIdMap.erase(nChunkId);
			if (m_chunks[i]->data)
			{
				delete[] (char*)m_chunks[i]->data;
				m_chunks[i]->data = 0;
			}
			delete m_chunks[i];
			m_chunks.erase(m_chunks.begin() + i);
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CChunkFile::DeleteChunksByType(ChunkTypes nChunkType)
{
	size_t j = 0;
	for (size_t i = 0, n = m_chunks.size(); i < n; ++i)
	{
		if (m_chunks[i]->chunkType == nChunkType)
		{
			m_chunkIdMap.erase(m_chunks[i]->chunkId);
			if (m_chunks[i]->data)
			{
				delete[] (char*)m_chunks[i]->data;
				m_chunks[i]->data = 0;
			}
			delete m_chunks[i];
		}
		else
		{
			m_chunks[j] = m_chunks[i];
			++j;
		}
	}
	m_chunks.resize(j);
}

//////////////////////////////////////////////////////////////////////////
void CChunkFile::ReleaseChunks()
{
	m_bLoaded = false;
	for (size_t i = 0; i < m_chunks.size(); ++i)
	{
		if (m_chunks[i]->data)
		{
			delete[] (char*) m_chunks[i]->data;
		}
		delete m_chunks[i];
	}
	m_chunks.clear();
	m_chunkIdMap.clear();
}

//////////////////////////////////////////////////////////////////////////
CChunkFile::ChunkDesc* CChunkFile::FindChunkByType(ChunkTypes nChunkType)
{
	for (size_t i = 0; i < m_chunks.size(); ++i)
	{
		if (m_chunks[i]->chunkType == nChunkType)
		{
			return m_chunks[i];
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CChunkFile::ChunkDesc* CChunkFile::FindChunkById(int nChunkId)
{
	ChunkIdMap::iterator it = m_chunkIdMap.find(nChunkId);
	if (it != m_chunkIdMap.end())
	{
		return it->second;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CChunkFile::Write(const char* filename)
{
	if (m_chunks.empty())
	{
		m_LastError.Format("Writing *empty* chunk files is not supported (file '%s')", filename);
		return false;
	}

	// Validate requested endianness. Kept here for debug.
	if (false)
	{
		bool bHasSwapEndianTrue = false;
		bool bHasSwapEndianFalse = false;
		for (size_t i = 0; i < m_chunks.size(); ++i)
		{
			const ChunkDesc& cd = *m_chunks[i];
			if (cd.bSwapEndian)
			{
				bHasSwapEndianTrue = true;
			}
			else
			{
				bHasSwapEndianFalse = true;
			}
		}
		if (bHasSwapEndianTrue && bHasSwapEndianFalse)
		{
			//Warning("Writing chunk files with *mixed* endianness is not supported (file '%s')", filename);
		}
	}

	ChunkFile::OsFileWriter writer;

	if (!writer.Create(filename))
	{
		m_LastError.Format("Failed to open '%s' for writing", filename);
		return false;
	}

	ChunkFile::MemorylessChunkFileWriter wr(ChunkFile::MemorylessChunkFileWriter::eChunkFileFormat_0x746, &writer);
	wr.SetAlignment(4);

	while (wr.StartPass())
	{
		for (size_t i = 0; i < m_chunks.size(); ++i)
		{
			const ChunkDesc& cd = *m_chunks[i];
			wr.StartChunk((cd.bSwapEndian ? eEndianness_NonNative : eEndianness_Native), cd.chunkType, cd.chunkVersion, cd.chunkId);
			wr.AddChunkData(cd.data, cd.size);
		}
	}

	if (!wr.HasWrittenSuccessfully())
	{
		m_LastError.Format("Failed to write '%s'", filename);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CChunkFile::WriteToMemoryBuffer(void** pData, int* nSize)
{
	ReleaseMemoryBuffer();
	*pData = 0;

	if (m_chunks.empty())
	{
		m_LastError.Format("Writing *empty* chunk files is not supported");
		return false;
	}

	// Do writing in *two* stages:
	//  1) computing required size (see sizeWriter below)
	//  2) allocating and writing data (see memoryWriter below)

	ChunkFile::SizeWriter sizeWriter;
	ChunkFile::MemoryWriter memoryWriter;

	for (int stage = 0; stage < 2; ++stage)
	{
		ChunkFile::IWriter* pWriter;
		if (stage == 0)
		{
			sizeWriter.Start();
			pWriter = &sizeWriter;
		}
		else
		{
			assert(m_pInternalData == 0);
			*nSize = sizeWriter.GetPos();
			assert(*nSize > 0);

			m_pInternalData = (char*)malloc(*nSize);
			if (m_pInternalData == 0)
			{
				m_LastError.Format("Failed to allocate %u bytes", uint(*nSize));
				return false;
			}

			if (!memoryWriter.Start(m_pInternalData, *nSize))
			{
				assert(0);
				m_LastError.Format("Internal error");
				ReleaseMemoryBuffer();
				return false;
			}
			pWriter = &memoryWriter;
		}

		ChunkFile::MemorylessChunkFileWriter wr(ChunkFile::MemorylessChunkFileWriter::eChunkFileFormat_0x746, pWriter);
		wr.SetAlignment(4);

		while (wr.StartPass())
		{
			for (size_t i = 0; i < m_chunks.size(); ++i)
			{
				const ChunkDesc& cd = *m_chunks[i];
				wr.StartChunk((cd.bSwapEndian ? eEndianness_NonNative : eEndianness_Native), cd.chunkType, cd.chunkVersion, cd.chunkId);
				wr.AddChunkData(cd.data, cd.size);
			}
		}

		if (!wr.HasWrittenSuccessfully())
		{
			m_LastError.Format("Failed to write");
			ReleaseMemoryBuffer();
			return false;
		}

		assert(stage != 2 || memoryWriter.GetPos() == *nSize);
	}

	*pData = m_pInternalData;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CChunkFile::ReleaseMemoryBuffer()
{
	if (m_pInternalData)
	{
		free(m_pInternalData);
		m_pInternalData = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CChunkFile::Read(const char* filename)
{
	LOADING_TIME_PROFILE_SECTION;

	ReleaseChunks();

	// Loading chunks
	{
		ChunkFile::CryFileReader f;

		if (!f.Open(filename))
		{
			m_LastError.Format("File %s failed to open for reading", filename);
			return false;
		}

		{
			const char* err = 0;

			err = ChunkFile::GetChunkTableEntries_0x746(&f, m_chunks);
			if (err)
			{
				err = ChunkFile::GetChunkTableEntries_0x744_0x745(&f, m_chunks);
				if (!err)
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

		for (size_t i = 0; i < m_chunks.size(); ++i)
		{
			ChunkDesc& cd = *m_chunks[i];

			assert(cd.data == 0);

			cd.data = new char[cd.size];

			if (!f.SetPos(cd.fileOffset) ||
			    !f.Read(cd.data, cd.size))
			{
				m_LastError.Format(
				  "Failed to read chunk data (offset:%u, size:%u) from file %s",
				  cd.fileOffset, cd.size, filename);
				return false;
			}
		}
	}

	m_nLastChunkId = 0;

	for (size_t i = 0; i < m_chunks.size(); ++i)
	{
		// Add chunk to chunk map.
		m_chunkIdMap[m_chunks[i]->chunkId] = m_chunks[i];

		// Update last chunk ID.
		if (m_chunks[i]->chunkId > m_nLastChunkId)
		{
			m_nLastChunkId = m_chunks[i]->chunkId;
		}
	}

	if (m_chunks.size() != m_chunkIdMap.size())
	{
		const int duplicateCount = (int)(m_chunks.size() - m_chunkIdMap.size());
		m_LastError.Format(
		  "%d duplicate chunk ID%s found in file %s",
		  duplicateCount, ((duplicateCount > 1) ? "s" : ""), filename);
		return false;
	}

	m_bLoaded = true;

	return true;
}
