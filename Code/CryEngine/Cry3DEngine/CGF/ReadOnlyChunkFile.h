// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ReadOnlyChunkFile.h
//  Created:     2004/11/15 by Timur
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ReadOnlyChunkFile_h__
#define __ReadOnlyChunkFile_h__

#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CryCore/smartptr.h>
#include <Cry3DEngine/CGF/IChunkFile.h>

////////////////////////////////////////////////////////////////////////
// Chunk file reader.
// Accesses a chunked file structure through file mapping object.
// Opens a chunk file and checks for its validity.
// If it's invalid, closes it as if there was no open operation.
// Error handling is performed through the return value of Read: it must
// be true for successfully open files
////////////////////////////////////////////////////////////////////////

class CReadOnlyChunkFile : public IChunkFile
{
public:
	//////////////////////////////////////////////////////////////////////////
	CReadOnlyChunkFile(bool bCopyFileData, bool bNoWarningMode = false);
	virtual ~CReadOnlyChunkFile();

	// interface IChunkFile --------------------------------------------------

	virtual void             Release()          { delete this; }

	virtual bool             IsReadOnly() const { return true; }
	virtual bool             IsLoaded() const   { return m_bLoaded; }

	virtual bool             Read(const char* filename);
	virtual bool             ReadFromMemory(const void* pData, int nDataSize);

	virtual bool             Write(const char* filename)                                                                                     { return false; }
	virtual bool             WriteToMemoryBuffer(void** pData, int* nSize)                                                                   { return false; }
	virtual void             ReleaseMemoryBuffer()                                                                                           {}

	virtual int              AddChunk(ChunkTypes chunkType, int chunkVersion, EEndianness eEndianness, const void* chunkData, int chunkSize) { return -1; }
	virtual void             DeleteChunkById(int nChunkId)                                                                                   {}
	virtual void             DeleteChunksByType(ChunkTypes nChunkType)                                                                       {}

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
	}
private:
	bool ReadChunkTableFromBuffer();
	void FreeBuffer();
	void CloseFile();

private:
	// this variable contains the last error occurred in this class
	string                 m_LastError;

	std::vector<ChunkDesc> m_chunks;

	char*                  m_pFileBuffer;
	int                    m_nBufferSize;
	bool                   m_bOwnFileBuffer;
	bool                   m_bNoWarningMode;
	bool                   m_bLoaded;
	bool                   m_bCopyFileData;

	FILE*                  m_hFile;
};

TYPEDEF_AUTOPTR(CReadOnlyChunkFile);

#endif
