// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include "CryHeaders.h"

//! Chunked File (.cgf, .chr etc.) interface.
struct IChunkFile : _reference_target_t
{
	//! Chunk Description.
	struct ChunkDesc
	{
		ChunkTypes chunkType;
		int        chunkVersion;
		int        chunkId;
		uint32     fileOffset;
		void*      data;
		uint32     size;
		bool       bSwapEndian;

		ChunkDesc()
			: chunkType(ChunkType_ANY)
			, chunkVersion(0)
			, chunkId(0)
			, fileOffset(0)
			, data(0)
			, size(0)
			, bSwapEndian(false)
		{
		}

		void               GetMemoryUsage(ICrySizer* pSizer) const                   { /*nothing*/ }

		static inline bool LessOffset(const ChunkDesc& d1, const ChunkDesc& d2)      { return d1.fileOffset < d2.fileOffset; }
		static inline bool LessOffsetByPtr(const ChunkDesc* d1, const ChunkDesc* d2) { return d1->fileOffset < d2->fileOffset; }
		static inline bool LessId(const ChunkDesc& d1, const ChunkDesc& d2)          { return d1.chunkId < d2.chunkId; }
	};

	// <interfuscator:shuffle>

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//! Releases chunk file interface.
	virtual void Release() = 0;

	virtual bool IsReadOnly() const = 0;
	virtual bool IsLoaded() const = 0;

	virtual bool Read(const char* filename) = 0;
	virtual bool ReadFromMemory(const void* pData, int nDataSize) = 0;

	//! Writes chunks to file.
	virtual bool Write(const char* filename) = 0;

	//! Writes chunks to a memory buffer (allocated inside) and returns a pointer to the allocated memory (pData) and its size (nSize).
	//! The memory will be released on destruction of the ChunkFile object, or on
	//! the next WriteToMemoryBuffer() call, or on ReleaseMemoryBuffer() call.
	virtual bool WriteToMemoryBuffer(void** pData, int* nSize) = 0;

	//! Releases memory that was allocated in WriteToMemoryBuffer().
	virtual void ReleaseMemoryBuffer() = 0;

	//! Adds chunk to file, returns ChunkID of the added chunk.
	virtual int        AddChunk(ChunkTypes chunkType, int chunkVersion, EEndianness eEndianness, const void* chunkData, int chunkSize) = 0;
	virtual void       DeleteChunkById(int nChunkId) = 0;
	virtual void       DeleteChunksByType(ChunkTypes nChunkType) = 0;

	virtual ChunkDesc* FindChunkByType(ChunkTypes nChunkType) = 0;
	virtual ChunkDesc* FindChunkById(int nChunkId) = 0;

	//! Gets the number of chunks.
	virtual int NumChunks() const = 0;

	//! Gets chunk description at i-th index.
	virtual ChunkDesc*       GetChunk(int nIndex) = 0;
	virtual const ChunkDesc* GetChunk(int nIndex) const = 0;

	virtual const char*      GetLastError() const = 0;

	// </interfuscator:shuffle>
};

//! \endcond