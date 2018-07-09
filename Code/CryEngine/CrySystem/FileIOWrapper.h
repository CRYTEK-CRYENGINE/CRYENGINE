// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FileIOWrapper_h__
#define __FileIOWrapper_h__

class CIOWrapper
{
public:
	static void   SetLoggerState(int b);
	static size_t Fread(void* pData, size_t nSize, size_t nCount, FILE* hFile);
	static int    Fclose(FILE* hFile) { return FcloseEx(hFile); }
	static int    FcloseEx(FILE* hFile);
#ifdef USE_FILE_HANDLE_CACHE
	static FILE*  Fopen(const char* file, const char* mode, FileIoWrapper::FileAccessType type = FileIoWrapper::GENERAL, bool bSysAppHome = false) { return FopenEx(file, mode, type, bSysAppHome); }
	static FILE*  FopenEx(const char* file, const char* mode, FileIoWrapper::FileAccessType type = FileIoWrapper::GENERAL, bool bSysAppHome = false);
#else
	static FILE*  Fopen(const char* file, const char* mode) { return FopenEx(file, mode); }
	static FILE*  FopenEx(const char* file, const char* mode);
#endif

	// open file in locked/exclusive mode, so no other processes can then open the file.
	// on non-windows platforms this forwards to Fopen.
	static FILE* FopenLocked(const char* file, const char* mode);

	static int64 FTell(FILE* hFile);
	static int   Fseek(FILE* hFile, int64 seek, int mode);

	static int   FEof(FILE* hFile);
	static int   FError(FILE* hFile);

	static void  LockReadIO(bool lock);

private:
	static bool               m_bLockReads;
	static CryCriticalSection m_ReadCS;
};

#endif //__FileIOWrapper_h__
