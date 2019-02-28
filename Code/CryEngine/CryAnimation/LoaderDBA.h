// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/IStreamEngine.h>
#include "Controller.h"
#include "ControllerPQ.h"
#include "ControllerOpt.h"

class CControllerOptNonVirtual;

struct CInternalDatabaseInfo : public IStreamCallback, public IControllerRelocatableChain
{
	uint32                    m_FilePathCRC;
	string                    m_strFilePath;

	bool                      m_bLoadFailed;
	string                    m_strLastError;

	uint32                    m_nRelocatableCAFs;
	int                       m_iTotalControllers;

	CControllerDefragHdl      m_hStorage;
	size_t                    m_nStorageLength;
	CControllerOptNonVirtual* m_pControllersInplace;

	CInternalDatabaseInfo(uint32 filePathCRC, const string& strFilePath);
	~CInternalDatabaseInfo();

	void         GetMemoryUsage(ICrySizer* pSizer) const;

	void*        StreamOnNeedStorage(IReadStream* pStream, unsigned nSize, bool& bAbortOnFailToAlloc);
	void         StreamAsyncOnComplete(IReadStream* pStream, unsigned nError);
	void         StreamOnComplete(IReadStream* pStream, unsigned nError);

	bool         LoadChunks(IChunkFile* pChunkFile, bool bStreaming);

	bool         ReadControllers(IChunkFile::ChunkDesc* pChunkDesc, bool bStreaming);
	bool         ReadController905(IChunkFile::ChunkDesc* pChunkDesc, bool bStreaming);

	void         Relocate(char* pDst, char* pSrc);

	static int32 FindFormat(uint32 num, std::vector<uint32>& vec)
	{
		for (size_t i = 0; i < vec.size(); ++i)
		{
			if (num < vec[i + 1])
				return (int)i;
		}
		return -1;
	}

private:
	CInternalDatabaseInfo(const CInternalDatabaseInfo&);
	CInternalDatabaseInfo& operator=(const CInternalDatabaseInfo&);
};

struct CGlobalHeaderDBA
{
	friend class CAnimationManager;

	CGlobalHeaderDBA();
	~CGlobalHeaderDBA();

	void         GetMemoryUsage(ICrySizer* pSizer) const;
	const char*  GetLastError() const { return m_strLastError.c_str(); }

	void         CreateDatabaseDBA(const char* filename);
	void         LoadDatabaseDBA(const char* sForCharacter);
	bool         StartStreamingDBA(bool highPriority);
	void         CompleteStreamingDBA(CInternalDatabaseInfo* pInfo);

	void         DeleteDatabaseDBA();
	const size_t SizeOf_DBA() const;
	bool         InMemory();
	void         ReportLoadError(const char* sForCharacter, const char* sReason);

	CInternalDatabaseInfo* m_pDatabaseInfo;
	IReadStreamPtr         m_pStream;
	string                 m_strFilePathDBA;
	string                 m_strLastError;
	uint32                 m_FilePathDBACRC32;
	uint16                 m_nUsedAnimations;
	uint16                 m_nEmpty;
	uint32                 m_nLastUsedTimeDelta;
	uint8                  m_bDBALock;
	bool                   m_bLoadFailed;
};

// mark CGlobalHeaderDBA as moveable, to prevent problems with missing copy-constructors
// and to not waste performance doing expensive copy-constructor operations
template<>
inline bool raw_movable<CGlobalHeaderDBA>(CGlobalHeaderDBA const& dest)
{
	return true;
}
