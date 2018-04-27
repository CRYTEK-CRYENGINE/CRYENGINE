// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   SaveReaderWriter_CryPak.cpp
//  Created:     15/02/2010 by Alex McCarthy.
//  Description: Implementation of the ISaveReader and ISaveWriter
//               interfaces using CryPak
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>

#include "SaveReaderWriter_CryPak.h"
#include "SaveReaderWriter_Memory.h"
#include <CryNetwork/INetwork.h>

static const int INVALID_SEEK = -1;

int TranslateSeekMode(IPlatformOS::ISaveReader::ESeekMode mode)
{
	static_assert(INVALID_SEEK != SEEK_SET, "Invalid SEEK_SET value!");
	static_assert(INVALID_SEEK != SEEK_CUR, "Invalid SEEK_CUR value!");
	static_assert(INVALID_SEEK != SEEK_END, "INVALID SEEK_END value!");

	switch (mode)
	{
	case IPlatformOS::ISaveReader::ESM_BEGIN:
		return SEEK_SET;
	case IPlatformOS::ISaveReader::ESM_CURRENT:
		return SEEK_CUR;
	case IPlatformOS::ISaveReader::ESM_END:
		return SEEK_END;
	default:
		{
			CRY_ASSERT_TRACE(false, ("Unrecognized seek mode %i", static_cast<int>(mode)));
			return INVALID_SEEK;
		}
	}
}

#if !CRY_PLATFORM_ORBIS
NO_INLINE_WEAK FILE* CCryPakFile::FOpen(const char* pName, const char* mode, unsigned nFlags)
{
	return gEnv->pCryPak->FOpen(pName, mode, nFlags);
}

NO_INLINE_WEAK int CCryPakFile::FClose(FILE* fp)
{
	return gEnv->pCryPak->FClose(fp);
}

NO_INLINE_WEAK size_t CCryPakFile::FGetSize(FILE* fp)
{
	return gEnv->pCryPak->FGetSize(fp);
}

NO_INLINE_WEAK size_t CCryPakFile::FReadRaw(void* data, size_t length, size_t elems, FILE* fp)
{
	return gEnv->pCryPak->FReadRaw(data, length, elems, fp);
}

NO_INLINE_WEAK size_t CCryPakFile::FWrite(const void* data, size_t length, size_t elems, FILE* fp)
{
	return gEnv->pCryPak->FWrite(data, length, elems, fp);
}

NO_INLINE_WEAK size_t CCryPakFile::FSeek(FILE* fp, long offset, int mode)
{
	return gEnv->pCryPak->FSeek(fp, offset, mode);
}
#endif // !CRY_PLATFORM_ORBIS
////////////////////////////////////////////////////////////////////////////

CCryPakFile::CCryPakFile(const char* fileName, const char* szMode)
	: m_fileName(fileName)
	, m_filePos(0)
	, m_eLastError(IPlatformOS::eFOC_Success)
	, m_bWriting(!strchr(szMode, 'r'))
{
	m_pFile = FOpen(fileName, szMode, ICryPak::FOPEN_ONDISK);

	if (m_pFile == nullptr)
	{
		m_pFile = FOpen(fileName, szMode);
	}

	if (m_pFile != nullptr && !m_bWriting)
	{
		size_t fileSize = FGetSize(m_pFile);

		if (fileSize > 0)
		{
			m_data.resize(fileSize);

			const std::vector<char>* pMagic;
			const std::vector<uint8>* pKey;
			GetISystem()->GetPlatformOS()->GetEncryptionKey(&pMagic, &pKey);

			bool isEncrypted = false;

			if (pMagic->size() > 0 && fileSize >= pMagic->size())
			{
				std::vector<uint8> magic;
				magic.resize(pMagic->size());
				FReadRaw(&magic[0], 1, magic.size(), m_pFile);
				fileSize -= magic.size();
				isEncrypted = 0 == memcmp(&magic[0], &pMagic->front(), magic.size());

				if (isEncrypted)
				{
					m_data.resize(fileSize);
				}
				else
				{
					memcpy(&m_data[0], &magic[0], magic.size());
					m_filePos += magic.size();
				}
			}

			if (fileSize > 0)
			{
				FReadRaw(&m_data[m_filePos], 1, fileSize, m_pFile);

				if (isEncrypted)
				{
					gEnv->pNetwork->DecryptBuffer(&m_data[0], &m_data[0], fileSize, &pKey->front(), pKey->size());
				}
			}

			m_filePos = 0;
		}
	}
}

CCryPakFile::~CCryPakFile()
{
	CloseImpl();
}

IPlatformOS::EFileOperationCode CCryPakFile::CloseImpl()
{
	CRY_ASSERT(gEnv);
	CRY_ASSERT(gEnv->pCryPak);

	if (m_eLastError == IPlatformOS::eFOC_Success)
	{
		if (m_pFile)
		{
			if (m_bWriting)
			{
				if (!m_data.empty())
				{
#ifndef _RELEASE
					// Default true if CVar doesn't exist
					ICVar* pUseEncryption = gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_usePlatformSavingAPIEncryption") : NULL;
					bool bUseEncryption = pUseEncryption && pUseEncryption->GetIVal() != 0;
					if (bUseEncryption)
#endif
					{
						// Write the magic tag
						const std::vector<char>* pMagic;
						const std::vector<uint8>* pKey;
						GetISystem()->GetPlatformOS()->GetEncryptionKey(&pMagic, &pKey);
						if (pMagic->size() != FWrite(&pMagic->front(), 1, pMagic->size(), m_pFile))
						{
							m_eLastError = IPlatformOS::eFOC_ErrorWrite;
						}
						else
						{
							gEnv->pNetwork->EncryptBuffer(&m_data[0], &m_data[0], m_data.size(), &pKey->front(), pKey->size());
						}
					}

					if (m_data.size() != FWrite(&m_data[0], 1, m_data.size(), m_pFile))
					{
						m_eLastError = IPlatformOS::eFOC_ErrorWrite;
					}
				}
			}

			if (FClose(m_pFile) != 0)
			{
				m_eLastError = IPlatformOS::eFOC_Failure;
			}
		}
	}
	m_pFile = NULL;
	m_data.clear();

	return m_eLastError;
}

////////////////////////////////////////////////////////////////////////////

// Update file stats to make sure SaveGame system reads from new info
void UpdateFileStats(const char* fileName)
{
#if CRY_PLATFORM_ORBIS
	gEnv->pCryPak->GetFileSizeOnDisk(fileName);
#endif
}

////////////////////////////////////////////////////////////////////////////

CSaveReader_CryPak::CSaveReader_CryPak(const char* fileName)
	: CCryPakFile(fileName, "rbx") // x=don't cache full file
{
	m_eLastError = m_pFile == NULL ? IPlatformOS::eFOC_ErrorOpenRead : IPlatformOS::eFOC_Success;
}

IPlatformOS::EFileOperationCode CSaveReader_CryPak::Seek(long seek, ESeekMode mode)
{
	CRY_ASSERT(gEnv);
	CRY_ASSERT(gEnv->pCryPak);

	long pos = (long)m_filePos;

	switch (mode)
	{
	case ESM_BEGIN:
		pos = seek;
		break;
	case ESM_CURRENT:
		pos += seek;
		break;
	case ESM_END:
		pos = m_data.size() + seek;
		break;
	}

	if (pos < 0 || pos > (long)m_data.size())
		return IPlatformOS::eFOC_Failure;

	m_filePos = (size_t)pos;

	return IPlatformOS::eFOC_Success;
}

IPlatformOS::EFileOperationCode CSaveReader_CryPak::GetFileCursor(long& fileCursor)
{
	CRY_ASSERT(gEnv);
	CRY_ASSERT(gEnv->pCryPak);

	fileCursor = (long)m_filePos;

	return IPlatformOS::eFOC_Success;
}

IPlatformOS::EFileOperationCode CSaveReader_CryPak::ReadBytes(void* data, size_t numBytes)
{
	CRY_ASSERT(gEnv);
	CRY_ASSERT(gEnv->pCryPak);

	size_t readBytes = numBytes;
	if (m_filePos + readBytes > m_data.size())
		readBytes = m_data.size() - m_filePos;

	memcpy(data, &m_data[m_filePos], readBytes);
	m_filePos += readBytes;

	if (numBytes != readBytes)
	{
		m_eLastError = IPlatformOS::eFOC_ErrorRead;
		return m_eLastError;
	}
	return IPlatformOS::eFOC_Success;
}

IPlatformOS::EFileOperationCode CSaveReader_CryPak::GetNumBytes(size_t& numBytes)
{
	CRY_ASSERT(gEnv);
	CRY_ASSERT(gEnv->pCryPak);

	numBytes = m_data.size();

	return IPlatformOS::eFOC_Success;
}

void CSaveReader_CryPak::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddContainer(m_data);
}

void CSaveReader_CryPak::TouchFile()
{
	// Touch the file so it's used as the most previous checkpoint
	CloseImpl();
	m_pFile = FOpen(m_fileName, "r+b", ICryPak::FOPEN_ONDISK);
	if (m_pFile)
	{
		char c;
		if (1 == FReadRaw(&c, 1, sizeof(c), m_pFile))
		{
			FSeek(m_pFile, 0, SEEK_SET);
			FWrite(&c, 1, sizeof(c), m_pFile);
		}
		FClose(m_pFile);
		m_pFile = NULL;
	}

	UpdateFileStats(m_fileName.c_str());
}

////////////////////////////////////////////////////////////////////////////

CSaveWriter_CryPak::CSaveWriter_CryPak(const char* fileName)
	: CCryPakFile(fileName, "wb")
{
	UpdateFileStats(fileName);

	m_eLastError = m_pFile == NULL ? IPlatformOS::eFOC_ErrorOpenWrite : IPlatformOS::eFOC_Success;
}

IPlatformOS::EFileOperationCode CSaveWriter_CryPak::AppendBytes(const void* data, size_t length)
{
	CRY_ASSERT(gEnv);
	CRY_ASSERT(gEnv->pCryPak);

	if (m_pFile && length != 0)
	{
		size_t newLength = m_data.size() + length;
		if (newLength > m_data.capacity())
		{
			size_t numBlocks = newLength / s_dataBlockSize;
			size_t newCapaticy = (numBlocks + 1) * s_dataBlockSize;
			m_data.reserve(newCapaticy);
		}
		m_data.resize(newLength);
		memcpy(&m_data[m_filePos], data, length);
		m_filePos += length;
	}
	return IPlatformOS::eFOC_Success;
}

void CSaveWriter_CryPak::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddContainer(m_data);
}
