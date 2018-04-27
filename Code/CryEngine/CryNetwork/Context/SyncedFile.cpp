// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SyncedFile.h"

#if SERVER_FILE_SYNC_MODE

	#include "Network.h"

static const uint32 BAD_UINT32 = 0xdeadbeef;

CSyncedFile::CSyncedFile(const string& filename)
	: m_filename(filename)
	, m_state(eS_NotLoaded)
	, m_pData(0)
	, m_locks(0)
	, m_length(BAD_UINT32)
	, m_capacity(BAD_UINT32)
{
	ASSERT_GLOBAL_LOCK;
}

CSyncedFile::~CSyncedFile()
{
	ASSERT_GLOBAL_LOCK;
	free(m_pData);
}

const CWhirlpoolHash* CSyncedFile::GetHash()
{
	ASSERT_GLOBAL_LOCK;
	if (!EnterState(eS_Hashed))
		return NULL;
	return &m_hash;
}

uint32 CSyncedFile::GetLength()
{
	ASSERT_GLOBAL_LOCK;
	if (!EnterState(eS_Loaded))
		return NULL;
	return m_length;
}

bool CSyncedFile::FillData(uint8* pBuffer, uint32 bufLen)
{
	ASSERT_GLOBAL_LOCK;
	if (!EnterState(eS_Loaded))
		return false;
	NET_ASSERT(m_length <= bufLen);
	if (m_length > bufLen)
		return false;
	memcpy(pBuffer, m_pData, bufLen);
	return true;
}

const uint8* CSyncedFile::GetData()
{
	ASSERT_GLOBAL_LOCK;
	if (!EnterState(eS_Loaded))
		return 0;
	return m_pData;
}

bool CSyncedFile::LockData()
{
	ASSERT_GLOBAL_LOCK;
	if (!EnterState(eS_Loaded))
		return false;
	m_locks++;
	return true;
}

void CSyncedFile::UnlockData()
{
	ASSERT_GLOBAL_LOCK;
	assert(m_locks > 0);
	--m_locks;
}

bool CSyncedFile::BeginUpdate()
{
	ASSERT_GLOBAL_LOCK;
	if (m_locks)
		return false;
	m_state = eS_Updating;

	free(m_pData);
	m_capacity = 16;
	m_pData = (uint8*)malloc(m_capacity);
	if (!m_pData)
	{
		m_length = BAD_UINT32;
		return false;
	}
	m_length = 0;
	return true;
}

bool CSyncedFile::PutData(const uint8* pData, uint32 length)
{
	ASSERT_GLOBAL_LOCK;
	if (m_state != eS_Updating)
		return false;
	NET_ASSERT(m_pData);
	NET_ASSERT(length);
	NET_ASSERT(m_length + length > m_length);
	NET_ASSERT(m_length + length <= MAX_FILE_SIZE);
	if (m_length + length > MAX_FILE_SIZE)
	{
		CryLog("Server tried to send a too-large file (>%d bytes)", MAX_FILE_SIZE);
		return false;
	}
	if (m_length + length < m_length)
		return false;
	if (m_length + length > m_capacity)
	{
		m_capacity = std::max(2 * m_capacity, m_length + length);
		NET_ASSERT(m_length + length <= m_capacity);
		if (m_length + length > m_capacity)
			return false;
		uint8* pNewData = (uint8*) malloc(m_capacity);
		if (!pNewData)
			return false;
		memcpy(pNewData, m_pData, m_length);
		free(m_pData);
		m_pData = pNewData;
	}
	memcpy(m_pData + m_length, pData, length);
	m_length += length;
	return true;
}

bool CSyncedFile::EndUpdate(bool success)
{
	ASSERT_GLOBAL_LOCK;
	if (m_state != eS_Updating)
		return false;
	if (success)
		m_state = eS_Loaded;
	else
	{
		free(m_pData);
		m_pData = 0;
		m_state = eS_NotLoaded;
	}
	return true;
}

bool CSyncedFile::EnterState(EState state)
{
	ASSERT_GLOBAL_LOCK;
	if (m_state == eS_Updating || state == eS_Updating)
		return false;

	while (m_state < state)
	{
		switch (m_state)
		{
		default:
			return false;
		case eS_NotLoaded:
			if (!LoadData())
				return false;
			m_state = eS_Loaded;
			break;
		case eS_Loaded:
			if (!HashData())
				return false;
			m_state = eS_Hashed;
			break;
		}
	}

	return true;
}

bool CSyncedFile::LoadData()
{
	ASSERT_GLOBAL_LOCK;
	assert(m_pData == 0);

	ICryPak* pak = gEnv->pCryPak;
	FILE* f = pak->FOpen(m_filename.c_str(), "rb");
	if (!f)
		goto fail;
	pak->FSeek(f, 0, SEEK_END);
	m_length = pak->FTell(f);
	m_pData = (uint8*) malloc(m_length);
	if (!m_pData)
		goto fail;
	pak->FSeek(f, 0, SEEK_SET);
	if (m_length != pak->FRead(m_pData, m_length, f))
		goto fail;
	pak->FClose(f);
	return true;

fail:
	free(m_pData);
	m_pData = 0;
	m_length = BAD_UINT32;
	if (f)
		fclose(f);
	return false;
}

bool CSyncedFile::HashData()
{
	ASSERT_GLOBAL_LOCK;
	m_hash = CWhirlpoolHash(m_pData, m_length);
	return true;
}

/*
 * CSyncedFileDataLock
 */

CSyncedFileDataLock::CSyncedFileDataLock() : m_pData(0)
{
}

CSyncedFileDataLock::CSyncedFileDataLock(CSyncedFilePtr pFile) : m_pFile(pFile)
{
	m_pData = m_pFile->GetData();
	if (!m_pData)
		goto fail;
	m_length = m_pFile->GetLength();
	if (!m_pFile->LockData())
		goto fail;

	return;

fail:
	m_pData = 0;
}

CSyncedFileDataLock::CSyncedFileDataLock(const CSyncedFileDataLock& other) : m_pFile(other.m_pFile), m_pData(other.m_pData), m_length(other.m_length)
{
	if (m_pData && !m_pFile->LockData())
		m_pData = 0;
}

CSyncedFileDataLock& CSyncedFileDataLock::operator=(CSyncedFileDataLock other)
{
	Swap(other);
	return *this;
}

void CSyncedFileDataLock::Swap(CSyncedFileDataLock& other)
{
	m_pFile.swap(other.m_pFile);
	std::swap(m_pData, other.m_pData);
	std::swap(m_length, other.m_length);
}

CSyncedFileDataLock::~CSyncedFileDataLock()
{
	if (m_pData)
		m_pFile->UnlockData();
}

#endif
