// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:ChunkFile.h
//  Declaration of class CChunkFile
//
//	History:
//
//////////////////////////////////////////////////////////////////////
#ifndef _CHUNK_FILE_READER_HDR_
#define _CHUNK_FILE_READER_HDR_

#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CryCore/smartptr.h>
#include <Cry3DEngine/CGF/IChunkFile.h>

////////////////////////////////////////////////////////////////////////
// Chunk file reader.
// Accesses a chunked file structure through file mapping object.
// Opens a chunk file and checks for its validity.
// If it's invalid, closes it as if there was no open operation.
// Error handling is performed through the return value of Read():
// it must be true for successfully open files
////////////////////////////////////////////////////////////////////////

class CChunkFile : public IChunkFile
{
public:
	//////////////////////////////////////////////////////////////////////////
	CChunkFile();
	virtual ~CChunkFile();

	// interface IChunkFile --------------------------------------------------

	virtual void             Release() { delete this; }

	void                     Clear();

	virtual bool             IsReadOnly() const { return false; }
	virtual bool             IsLoaded() const   { return m_bLoaded; }

	virtual bool             Read(const char* filename);
	virtual bool             ReadFromMemory(const void* pData, int nDataSize) { return false; }

	virtual bool             Write(const char* filename);
	virtual bool             WriteToMemoryBuffer(void** pData, int* nSize);
	virtual void             ReleaseMemoryBuffer();

	virtual int              AddChunk(ChunkTypes chunkType, int chunkVersion, EEndianness eEndianness, const void* chunkData, int chunkSize);
	virtual void             DeleteChunkById(int nChunkId);
	virtual void             DeleteChunksByType(ChunkTypes nChunkType);

	virtual ChunkDesc*       FindChunkByType(ChunkTypes nChunkType);
	virtual ChunkDesc*       FindChunkById(int nChunkId);

	virtual int              NumChunks() const;

	virtual ChunkDesc*       GetChunk(int nIndex);
	virtual const ChunkDesc* GetChunk(int nIndex) const;

	virtual const char*      GetLastError() const { return m_LastError; }

	// -----------------------------------------------------------------------

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_LastError);
		pSizer->AddObject(m_chunks);
		pSizer->AddObject(m_chunkIdMap);
	}

private:
	void ReleaseChunks();

private:
	// this variable contains the last error occurred in this class
	string                  m_LastError;

	int                     m_nLastChunkId;
	std::vector<ChunkDesc*> m_chunks;
	typedef std::map<int, ChunkDesc*> ChunkIdMap;
	ChunkIdMap              m_chunkIdMap;
	char*                   m_pInternalData;
	bool                    m_bLoaded;
};

TYPEDEF_AUTOPTR(CChunkFile);

#endif
