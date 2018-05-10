// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SYNCEDFILE_H__
#define __SYNCEDFILE_H__

#pragma once

#include "Config.h"

#if SERVER_FILE_SYNC_MODE

	#include "Cryptography/Whirlpool.h"

class CSyncedFile : public CMultiThreadRefCount
{
public:
	static const uint32 MAX_FILE_SIZE = 16384;
	static const uint32 HASH_BYTES = CWhirlpoolHash::DIGESTBYTES;

	CSyncedFile(const string& filename);
	~CSyncedFile();

	const CWhirlpoolHash* GetHash();
	uint32                GetLength();
	bool                  FillData(uint8* pBuffer, uint32 bufLen);
	const uint8*          GetData();

	bool                  LockData();
	void                  UnlockData();

	bool                  BeginUpdate();
	bool                  PutData(const uint8* pData, uint32 length);
	bool                  EndUpdate(bool success);

	const string&         GetFilename() const
	{
		return m_filename;
	}

private:
	enum EState
	{
		eS_Updating,
		eS_NotLoaded,
		eS_Loaded,
		eS_Hashed,
	};

	const string   m_filename;
	EState         m_state;
	CWhirlpoolHash m_hash;
	uint32         m_length;
	uint32         m_capacity;
	uint8*         m_pData;
	int            m_locks;

	bool EnterState(EState state);
	bool LoadData();
	bool HashData();
};

typedef _smart_ptr<CSyncedFile> CSyncedFilePtr;

class CSyncedFileDataLock
{
public:
	explicit CSyncedFileDataLock(CSyncedFilePtr pFile);
	CSyncedFileDataLock();
	CSyncedFileDataLock(const CSyncedFileDataLock& other);
	CSyncedFileDataLock& operator=(CSyncedFileDataLock other);
	void                 Swap(CSyncedFileDataLock& other);
	~CSyncedFileDataLock();

	bool Ok() const
	{
		return m_pData != 0;
	}

	const uint8* GetData() const
	{
		return m_pData;
	}
	uint32 GetLength() const
	{
		return m_length;
	}
	const string& GetFilename() const
	{
		return m_pFile->GetFilename();
	}

private:
	CSyncedFilePtr m_pFile;
	const uint8*   m_pData;
	uint32         m_length;
};

#endif // SERVER_FILE_SYNC_MODE

#endif
