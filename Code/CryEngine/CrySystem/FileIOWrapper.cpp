// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "FileIOWrapper.h"
#include <CrySystem/Profilers/IDiskProfiler.h>
#include <CryCore/Platform/IPlatformOS.h>
#include <CryString/StringUtils.h>
#include <CryCore/Platform/CryWindows.h>

bool CIOWrapper::m_bLockReads = false;
CryCriticalSection CIOWrapper::m_ReadCS;

size_t CIOWrapper::Fread(void* pData, size_t nSize, size_t nCount, FILE* hFile)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	PROFILE_DISK_READ(nSize * nCount);

	if (m_bLockReads)
	{
		AUTO_LOCK_CS(m_ReadCS);
		return ::fread(pData, nSize, nCount, hFile);
	}
	else
	{
		return ::fread(pData, nSize, nCount, hFile);
	}
}

FILE* CIOWrapper::FopenLocked(const char* file, const char* mode)
{
#if CRY_PLATFORM_WINDOWS
	HANDLE handle;

	// The file must exist if opens for 'r' or 'r+'.
	const auto creationDisposition = strchr(mode, 'r') ? OPEN_EXISTING : OPEN_ALWAYS;

	const auto accessMode = (creationDisposition == OPEN_EXISTING) && !strchr(mode, '+') ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);

	handle = CreateFile(file, accessMode, 0, 0, creationDisposition, 0, 0);

	if (handle == INVALID_HANDLE_VALUE)
	{
		return NULL; // failed to lock or open file
	}

	// Get file descriptor and FILE* from handle
	int fhandle = _open_osfhandle((intptr_t)handle, 0);

	if (fhandle == -1)
	{
		CloseHandle(handle);
		return NULL;
	}

	FILE* ret = _fdopen(fhandle, mode);

	if (ret == NULL)
	{
		CloseHandle(handle);
		return NULL;
	}

	return ret;
	// handle doesn't need to be freed, the eventual fclose on ret will do that for us.
#else
	return Fopen(file, mode);
#endif
}

#if defined(USE_FILE_HANDLE_CACHE)
FILE* CIOWrapper::FopenEx(const char* file, const char* mode, FileIoWrapper::FileAccessType type, bool bSysAppHome)
#else
FILE * CIOWrapper::FopenEx(const char* file, const char* mode)
#endif
{
	LOADING_TIME_PROFILE_SECTION;

#if !defined(SYS_ENV_AS_STRUCT)
	PREFAST_ASSUME(gEnv);
#endif

	FILE* f = NULL;

	IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();
	if (pOS != nullptr && pOS->IsFileAvailable(file))
	{
#if CRY_PLATFORM_WINDOWS
		f = ::_wfopen(CryStringUtils::UTF8ToWStr(file), CryStringUtils::UTF8ToWStr(mode));
#elif CRY_PLATFORM_ORBIS
		char buf[512];
		const char* const absoluteFile = ConvertFileName(buf, sizeof(buf), file);
		f = ::fopen(absoluteFile, mode);
#else
		f = ::fopen(file, mode);
#endif
	}

	return f;
}

//////////////////////////////////////////////////////////////////////////
int CIOWrapper::FcloseEx(FILE* hFile)
{
	return fclose(hFile);
}

int64 CIOWrapper::FTell(FILE* hFile)
{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE || CRY_PLATFORM_ORBIS
	return (int64)ftell(hFile);
#else
	return _ftelli64(hFile);
#endif
}

int CIOWrapper::FEof(FILE* hFile)
{
	return feof(hFile);
}

int CIOWrapper::FError(FILE* hFile)
{
	return ferror(hFile);
}

int CIOWrapper::Fseek(FILE* hFile, int64 seek, int mode)
{
	return fseek(hFile, (long)seek, mode);
}

void CIOWrapper::LockReadIO(bool lock)
{
	m_bLockReads = lock;
}
