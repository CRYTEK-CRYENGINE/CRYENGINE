// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLCryPlatform.hpp
//  Version:     v1.00
//  Created:     27/03/2014 by Valerio Guagliumi.
//  Description: Platform specific DXGL requirements implementation relying
//               on CryCommon and CrySystem.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLCRYPLATFORM__
#define __GLCRYPLATFORM__

#include "DriverD3D.h"

namespace NCryOpenGL
{

inline void* Malloc(size_t size)
{
	return CryModuleMalloc(size);
}

inline void* Calloc(size_t num, size_t size)
{
	return CryModuleCalloc(num, size);
}

inline void* Realloc(void* memblock, size_t size)
{
	return CryModuleRealloc(memblock, size);
}

inline void Free(void* memblock)
{
	CryModuleFree(memblock);
}

inline void* Memalign(size_t size, size_t alignment)
{
	return CryModuleMemalign(size, alignment);
}

inline void MemalignFree(void* memblock)
{
	return CryModuleMemalignFree(memblock);
}

void        LogMessage(ELogSeverity eSeverity, const char* szFormat, ...) PRINTF_PARAMS(2, 3);

inline void LogMessage(ELogSeverity eSeverity, const char* szFormat, ...)
{
	va_list kArgs;
	va_start(kArgs, szFormat);
	if (eSeverity == eLS_Fatal)
	{
		char szBuffer[4096];
		cry_vsprintf(szBuffer, szFormat, kArgs);
		GetISystem()->FatalError(szBuffer);
	}
	else
	{
		EValidatorSeverity eValidatorSeverity(VALIDATOR_COMMENT);
		switch (eSeverity)
		{
		case eLS_Error:
			eValidatorSeverity = VALIDATOR_ERROR;
			break;
		case eLS_Warning:
			eValidatorSeverity = VALIDATOR_WARNING;
			break;
		case eLS_Info:
			eValidatorSeverity = VALIDATOR_COMMENT;
			break;
		}
		GetISystem()->WarningV(VALIDATOR_MODULE_RENDERER, eValidatorSeverity, 0, 0, szFormat, kArgs);
	}
	va_end(kArgs);
}

inline void BreakUnique(const char* szFile, uint32 uLine)
{
	LogMessage(eLS_Warning, "Break at %s(%d)", szFile, uLine);
	CryDebugBreak();
}

inline uint32 GetCRC32(const void* pData, size_t uSize)
{
	return CCrc32::Compute(pData, uSize);
}

inline LONG Exchange(LONG volatile* piDestination, LONG iExchange)
{
#if CRY_PLATFORM_WINDOWS
	return InterlockedExchange(piDestination, iExchange);
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	LONG iOldValue(__sync_lock_test_and_set(piDestination, iExchange));
	__sync_synchronize();
	return iOldValue;
#else
	#error "Not implemented on this platform"
#endif
}

inline LONG CompareExchange(LONG volatile* piDestination, LONG iExchange, LONG iComparand)
{
	return CryInterlockedCompareExchange(piDestination, iExchange, iComparand);
}

inline LONG AtomicIncrement(LONG volatile* piDestination)
{
	return CryInterlockedIncrement(reinterpret_cast<int volatile*>(piDestination));
}

inline LONG AtomicDecrement(LONG volatile* piDestination)
{
	return CryInterlockedDecrement(reinterpret_cast<int volatile*>(piDestination));
}

typedef CryCriticalSection TCriticalSection;

inline void LockCriticalSection(TCriticalSection* pCriticalSection)
{
	pCriticalSection->Lock();
}

inline void UnlockCriticalSection(TCriticalSection* pCriticalSection)
{
	pCriticalSection->Unlock();
}

struct STraceFile
{
	STraceFile()
		: m_pFile(NULL)
	{
	}

	~STraceFile()
	{
		if (m_pFile != NULL)
			gEnv->pCryPak->FClose(m_pFile);
	}

	bool Open(const char* szFileName, bool bBinary)
	{
		static const char* s_szTraceDirectory = "DXGLTrace";

		if (m_pFile != NULL)
			return false;

		char acFullPath[MAX_PATH];
		cry_sprintf(acFullPath, "%%USER%%/%s/%s", s_szTraceDirectory, szFileName);
		const char* szMode(bBinary ? "wb" : "w");
		m_pFile = gEnv->pCryPak->FOpen(acFullPath, szMode, ICryPak::FLAGS_FOR_WRITING);
		if (m_pFile != NULL)
			return true;

		gEnv->pCryPak->MakeDir(s_szTraceDirectory, 0);
		m_pFile = gEnv->pCryPak->FOpen(acFullPath, szMode);
		return m_pFile != NULL;
	}

	void Write(const void* pvData, uint32 uSize)
	{
		gEnv->pCryPak->FWrite(pvData, (size_t)uSize, 1, m_pFile);
	}

	void Printf(const char* szFormat, ...)
	{
		va_list kArgs;
		va_start(kArgs, szFormat);
		vfprintf(m_pFile, szFormat, kArgs);
		va_end(kArgs);
	}

	FILE* m_pFile;
};

inline void RegisterConfigVariable(const char* szName, int* piVariable, int iDefaultValue)
{
	REGISTER_CVAR2(szName, piVariable, iDefaultValue, 0, "");
}

inline void Memcpy(void* pDst, const void* pSrc, size_t uLength)
{
	cryMemcpy(pDst, pSrc, uLength);
}

inline void PushProfileLabel(const char* szName)
{
	PROFILE_LABEL_PUSH(szName);
}

inline void PopProfileLabel(const char* szName)
{
	PROFILE_LABEL_POP(szName);
}

}

#endif //__GLCRYPLATFORM__
