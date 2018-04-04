// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include <StdAfx.h>
#include <CrySystem/ISystem.h>

#if USE_STEAM

	#include "SaveReaderWriter_Steam.h"
	#include "SaveReaderWriter_Memory.h"
	#include <CryNetwork/INetwork.h>
	#include "Steamworks/public/steam/isteamremotestorage.h"
	#include "Steamworks/public/steam/steam_api.h"
	#include "PatternMatcher.h"

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
		#pragma comment(lib, "steam_api64.lib")
	#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
		#pragma comment(lib, "steam_api.lib")
	#endif

////////////////////////////////////////////////////////////////////////////

CSaveReader_Steam::CSaveReader_Steam(const char* fileName)
{
	// we don't need the user folder in steamcloud
	if (!strnicmp(fileName, "%USER%/", 7))
		fileName += 7;

	if (SteamRemoteStorage()->FileExists(fileName))
	{
		m_fileName = fileName;
		m_lastError = IPlatformOS::eFOC_Success;
		m_filePointer = 0;

		// open file and parse
		int fileSize = SteamRemoteStorage()->GetFileSize(m_fileName);
		std::vector<uint8> rawData;
		rawData.resize(fileSize);
		uint32 bytesRead = SteamRemoteStorage()->FileRead(m_fileName, &rawData[0], fileSize);

		const std::vector<char>* pMagic;
		const std::vector<uint8>* pKey;
		GetISystem()->GetPlatformOS()->GetEncryptionKey(&pMagic, &pKey);

		bool isEncrypted = memcmp(&rawData[0], &(*pMagic)[0], pMagic->size()) == 0;

		if (isEncrypted)
		{
			m_data.resize(fileSize - pMagic->size());
			gEnv->pNetwork->DecryptBuffer(&m_data[0], &rawData[pMagic->size()], fileSize - pMagic->size(), &pKey->front(), pKey->size());
		}
		else
		{
			m_data.resize(fileSize);
			memcpy(&m_data[0], &rawData[0], fileSize);
		}
	}
	else
	{
		m_lastError = IPlatformOS::eFOC_Failure;
	}
}

IPlatformOS::EFileOperationCode CSaveReader_Steam::Seek(long seek, ESeekMode mode)
{
	switch (mode)
	{
	case IPlatformOS::ISaveReader::ESM_BEGIN:
		m_filePointer = 0 + seek;
		break;

	case IPlatformOS::ISaveReader::ESM_CURRENT:
		m_filePointer += seek;
		break;

	case IPlatformOS::ISaveReader::ESM_END:
		m_filePointer = m_data.size() + seek;
		break;

	default:
		assert(!"Invalid seek mode");
		m_lastError = IPlatformOS::eFOC_Failure;
		return m_lastError;
	}

	m_lastError = IPlatformOS::eFOC_Success;
	return m_lastError;
}

IPlatformOS::EFileOperationCode CSaveReader_Steam::GetFileCursor(long& fileCursor)
{
	// not supported
	m_lastError = IPlatformOS::eFOC_Failure;
	return m_lastError;
}

IPlatformOS::EFileOperationCode CSaveReader_Steam::ReadBytes(void* data, size_t numBytes)
{
	if (m_filePointer + numBytes <= m_data.size())
	{
		memcpy(data, &m_data[m_filePointer], numBytes);
		m_filePointer += numBytes;
		m_lastError = IPlatformOS::eFOC_Success;
	}
	else
	{
		m_lastError = IPlatformOS::eFOC_ErrorRead;
	}

	return m_lastError;
}

IPlatformOS::EFileOperationCode CSaveReader_Steam::GetNumBytes(size_t& numBytes)
{
	numBytes = m_data.size();
	m_lastError = IPlatformOS::eFOC_Success;
	return m_lastError;
}

void CSaveReader_Steam::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
}

void CSaveReader_Steam::TouchFile()
{
	// not supported
}

IPlatformOS::EFileOperationCode CSaveReader_Steam::Close()
{
	// not necessary, files is closed after read
	return IPlatformOS::eFOC_Success;
}

////////////////////////////////////////////////////////////////////////////

CSaveWriter_Steam::CSaveWriter_Steam(const char* fileName)
	: m_filePos(0)
{
	// we don't need the user folder in steamcloud
	if (!strnicmp(fileName, "%USER%/", 7))
		fileName += 7;

	m_fileName = fileName;
	m_lastError = IPlatformOS::eFOC_Success;
}

IPlatformOS::EFileOperationCode CSaveWriter_Steam::AppendBytes(const void* data, size_t length)
{
	if (!data || length == 0)
	{
		// no data provided
		return IPlatformOS::eFOC_ErrorWrite;
	}

	size_t newLength = length + m_data.size();
	m_data.resize(newLength);

	memcpy(&m_data[m_filePos], data, length);
	m_filePos += length;

	return IPlatformOS::eFOC_Success;
}

void CSaveWriter_Steam::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddContainer(m_data);
}

IPlatformOS::EFileOperationCode CSaveWriter_Steam::Close()
{
	m_lastError = IPlatformOS::eFOC_Success;

	if (m_data.empty())
	{
		// no data to write
		m_lastError = IPlatformOS::eFOC_ErrorWrite;
	}
	else
	{
		// Encrypt buffer - in release always, in dev check the cvar

		const std::vector<char>* pMagic; // needed here so the write code can see it

	#ifndef _RELEASE
		// Default true if CVar doesn't exist
		ICVar* pUseEncryption = gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_usePlatformSavingAPIEncryption") : NULL;
		bool bUseEncryption = pUseEncryption && pUseEncryption->GetIVal() != 0;
		if (bUseEncryption)
	#endif
		{
			// Write the magic tag
			const std::vector<uint8>* pKey;
			GetISystem()->GetPlatformOS()->GetEncryptionKey(&pMagic, &pKey);
			if (pMagic->size() == 0)
			{
				// no magic tag
				m_lastError = IPlatformOS::eFOC_ErrorWrite;
			}
			else
			{
				gEnv->pNetwork->EncryptBuffer(&m_data[0], &m_data[0], m_data.size(), &pKey->front(), pKey->size());
			}
		}

		bool success = false;
		UGCFileWriteStreamHandle_t handle = SteamRemoteStorage()->FileWriteStreamOpen(m_fileName);
		if (handle != k_UGCFileStreamHandleInvalid && m_lastError == IPlatformOS::eFOC_Success)
		{
			success = true;

	#ifndef _RELEASE
			if (bUseEncryption)
	#endif
			{
				success &= SteamRemoteStorage()->FileWriteStreamWriteChunk(handle, &(*pMagic)[0], pMagic->size());
			}

			success &= SteamRemoteStorage()->FileWriteStreamWriteChunk(handle, &m_data[0], m_data.size());
			success &= SteamRemoteStorage()->FileWriteStreamClose(handle);
		}

		if (!success)
		{
			m_lastError = IPlatformOS::eFOC_ErrorWrite;
		}

	}

	return m_lastError;
}

////////////////////////////////////////////////////////////////////////////

IPlatformOS::IFileFinder::EFileState CFileFinderSteam::FileExists(const char* path)
{
	if (SteamRemoteStorage()->FileExists(path))
	{
		return IFileFinder::eFS_File;
	}

	return IFileFinder::eFS_NotExist;
}

CFileFinderSteam::CFileFinderSteam()
{
	m_nextFileHandle = 1;
	if (!gEnv->pSystem->SteamInit())
	{
		return;
	}

	int32 count = SteamRemoteStorage()->GetFileCount();
	for (int i = 0; i < count; i++)
	{
		int32 size;
		string name = string(SteamRemoteStorage()->GetFileNameAndSize(i, &size));
		m_files.push_back(SFileInfo(name, size));

		stack_string path = name;
		int slashIdx = path.find_first_of('/');
		int slashPosInFullPath = slashIdx;
		while (slashIdx >= 0)
		{
			string folderName = name.substr(0, slashPosInFullPath);
			bool found = false;

			for (int i = 0; i < m_files.size(); i++)
			{
				if (m_files[i].m_name == folderName)
					found = true;
			}

			if (!found)
				m_files.push_back(SFileInfo(folderName, 0, _A_SUBDIR));

			path = path.substr(slashIdx + 1);
			slashPosInFullPath += slashIdx;
			slashIdx = path.find_first_of('/');
		}
	}
}

intptr_t CFileFinderSteam::FindFirst(const char* filePattern, _finddata_t* fd)
{
	// cut of the user folder since that doesn't exist in the steam cloud
	if (!strnicmp(filePattern, "%USER%/", 7))
		filePattern += 7;

	// and make it lowercase, steam always returns lowercase paths
	string fp = string(filePattern);
	fp.MakeLower();

	for (int i = 0; i < m_files.size(); i++)
	{
		if (PatternMatch(m_files[i].m_name, fp) > 0)
		{
			m_matchingFiles[m_nextFileHandle].m_files.push_back(m_files[i]);
		}
	}

	// if it ends with .* we want to cut the .* off and do a search for directories (attempt to mimic the crypak FindNext behavior)
	if (fp.find(".*") == fp.size() - 2)
	{
		fp = fp.substr(0, fp.size() - 2);

		for (int i = 0; i < m_files.size(); i++)
		{
			if ((m_files[i].m_flags & _A_SUBDIR) && PatternMatch(m_files[i].m_name, fp) > 0)
			{
				m_matchingFiles[m_nextFileHandle].m_files.push_back(m_files[i]);
			}
		}
	}

	if (!m_matchingFiles[m_nextFileHandle].m_files.empty())
	{
		m_matchingFiles[m_nextFileHandle].m_currentIteratorIdx = 0;
		m_matchingFiles[m_nextFileHandle].m_searchPath = fp.substr(0, fp.find_last_of('/') + 1);
		FillFindData(m_nextFileHandle, fd);
	}

	int32 handle = m_nextFileHandle;
	if (m_nextFileHandle++ >= std::numeric_limits<uint32>::max())
	{
		m_nextFileHandle = 0;
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Filefinder nextFileHandle looped around. potentially dangerous.");
	}

	return m_matchingFiles[handle].m_files.empty() ? -1 : handle;
}

int CFileFinderSteam::FindNext(intptr_t handle, _finddata_t* fd)
{
	if (m_matchingFiles[(uint32)handle].m_currentIteratorIdx >= m_matchingFiles[(uint32)handle].m_files.size())
	{
		return -1;
	}

	FillFindData((uint32)handle, fd);

	m_matchingFiles[(uint32)handle].m_currentIteratorIdx++;

	return 0;
}

int CFileFinderSteam::FindClose(intptr_t handle)
{
	m_matchingFiles.erase((uint32)handle);
	return 1;
}

void CFileFinderSteam::FillFindData(uint32 handle, _finddata_t* fd)
{
	SFileInfo file = m_matchingFiles[handle].m_files[m_matchingFiles[handle].m_currentIteratorIdx];
	string relativePath = file.m_name;
	string searchPath = m_matchingFiles[handle].m_searchPath;

	// if we searched in a directory, remove this from the result (users concat it themselves)
	if (!searchPath.empty())
		relativePath = relativePath.substr(searchPath.length(), relativePath.length() - searchPath.length());

	cry_strcpy(fd->name, relativePath.c_str());
	fd->size = file.m_size;
	fd->attrib = file.m_flags;
}

#endif // USE_STEAM
