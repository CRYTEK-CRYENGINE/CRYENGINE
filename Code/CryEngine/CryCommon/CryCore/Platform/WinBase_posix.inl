// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(INCLUDED_FROM_CRY_WINBASE_IMPL)
#endif
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// File System
//////////////////////////////////////////////////////////////////////////

#include <CryCore/Platform/CryLibrary.h>

#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include <dirent.h>
#include <signal.h>

#if CRY_PLATFORM_APPLE
	#include <mach/mach.h>
	#include <mach/mach_time.h>
	#include <sys/sysctl.h>                    // for total physical memory on Mac
	#include <CoreFoundation/CoreFoundation.h> // for CryMessageBox
	#include <mach/vm_statistics.h>            // host_statistics
	#include <mach/mach_types.h>
	#include <mach/mach_init.h>
	#include <mach/mach_host.h>
#endif

typedef mode_t CRYMODE_T;

#if CRY_PLATFORM_APPLE
typedef struct stat FS_STAT_TYPE;
	#define FS_STAT_FUNC  ::stat
	#define FS_FSTAT_FUNC ::fstat
#else
typedef struct stat64 FS_STAT_TYPE;
	#define FS_STAT_FUNC  ::stat64
	#define FS_FSTAT_FUNC ::fstat64
#endif

typedef DIR*   FS_DIR_TYPE;
typedef dirent FS_DIRENT_TYPE;
static const FS_ERRNO_TYPE FS_ENOENT = ENOENT;
static const FS_ERRNO_TYPE FS_EINVAL = EINVAL;
static const FS_DIR_TYPE FS_DIR_NULL = NULL;
static const unsigned char FS_TYPE_DIRECTORY = DT_DIR;

namespace CryFileSystem
{
ILINE void FS_OPEN(const char* szFileName, int iFlags, int& iFileDesc, CRYMODE_T uMode, FS_ERRNO_TYPE& rErr)
{
	rErr = ((iFileDesc = ::open(szFileName, iFlags, uMode)) != -1) ? 0 : errno;
}

ILINE void FS_CLOSE(int iFileDesc, FS_ERRNO_TYPE& rErr)
{
	rErr = ::close(iFileDesc) != -1 ? 0 : errno;
}

ILINE void FS_CLOSE_NOERR(int iFileDesc)
{
	::close(iFileDesc);
}

ILINE void FS_STAT(const char* szFileName, FS_STAT_TYPE& kStat, FS_ERRNO_TYPE& rErr)
{
	rErr = FS_STAT_FUNC(szFileName, &kStat) != -1 ? 0 : errno;
}

ILINE void FS_FSTAT(int iFileDesc, FS_STAT_TYPE& kStat, FS_ERRNO_TYPE& rErr)
{
	rErr = FS_FSTAT_FUNC(iFileDesc, &kStat) != -1 ? 0 : errno;
}

ILINE void FS_OPENDIR(const char* szDirName, FS_DIR_TYPE& pDir, FS_ERRNO_TYPE& rErr)
{
	rErr = (pDir = ::opendir(szDirName)) != NULL ? 0 : errno;
}

ILINE void FS_READDIR(FS_DIR_TYPE pDir, FS_DIRENT_TYPE& kEnt, uint64_t& uEntSize, FS_ERRNO_TYPE& rErr)
{
	errno = 0; // errno is used to determine if readdir succeeds after
	FS_DIRENT_TYPE* pDirent(readdir(pDir));
	if (pDirent == NULL)
	{
		uEntSize = 0;
		rErr = (errno == FS_ENOENT) ? 0 : errno;
	}
	else
	{
		kEnt = *pDirent;
		uEntSize = static_cast<uint64_t>(sizeof(FS_DIRENT_TYPE));
		rErr = 0;
	}
}

ILINE void FS_CLOSEDIR(FS_DIR_TYPE pDir, FS_ERRNO_TYPE& rErr)
{
	errno = 0;
	rErr = ::closedir(pDir) == 0 ? 0 : errno;
}

ILINE void FS_CLOSEDIR_NOERR(FS_DIR_TYPE pDir)
{
	::closedir(pDir);
}

ILINE int FS_MKDIR(const char* dirname, CRYMODE_T mode)
{
	const int suc = ::mkdir(dirname, mode);
	return (suc == 0) ? 0 : -1;
}
}

namespace CryFileSystem
{
ILINE FILE* fopen(const char* filename, const char* mode)
{
	return ::fopen(filename, mode);
}

ILINE int fclose(FILE* stream)
{
	return ::fclose(stream);
}

ILINE int fflush(FILE* stream)
{
	return ::fflush(stream);
}

ILINE size_t fread(void* ptr, size_t size, size_t nitems, FILE* stream)
{
	return ::fread(ptr, size, nitems, stream);
}

ILINE size_t fread(const void* ptr, size_t size, size_t nitems, FILE* stream)
{
	return ::fwrite(ptr, size, nitems, stream);
}

ILINE long ftell(FILE* stream)
{
	return ::ftell(stream);
}

ILINE long fseek(FILE* stream, off_t offset, int whence)
{
	return ::fseek(stream, offset, whence);
}

ILINE int vfscanf(FILE* stream, const char* format, va_list arg)
{
	return ::vfscanf(stream, format, arg);
}

ILINE int vfprintf(FILE* stream, const char* format, va_list arg)
{
	return ::vfprintf(stream, format, arg);
}

ILINE int feof(FILE* stream)
{
	return ::feof(stream);
}
ILINE int fgetc(FILE* stream)
{
	return ::fgetc(stream);
}

ILINE char* fgets(char* s, int n, FILE* stream)
{
	return ::fgets(s, n, stream);
}

ILINE int getc(FILE* stream)
{
	return ::getc(stream);
}

ILINE int ungetc(int c, FILE* stream)
{
	return ::ungetc(c, stream);
}

ILINE int ferror(FILE* stream)
{
	return ::ferror(stream);
}

ILINE int fputs(const char* s, FILE* stream)
{
	return ::fputs(s, stream);
}
}

//////////////////////////////////////////////////////////////////////////
// Misc
//////////////////////////////////////////////////////////////////////////

threadID GetCurrentThreadId()
{
	return threadID(pthread_self());
}

void GlobalMemoryStatus(LPMEMORYSTATUS lpmem)
{
#if CRY_PLATFORM_APPLE

	// Retrieve dwTotalPhys
	int kMIB[] = { CTL_HW, HW_MEMSIZE };
	uint64_t dwTotalPhys;
	size_t ulength = sizeof(dwTotalPhys);
	if (sysctl(kMIB, 2, &dwTotalPhys, &ulength, NULL, 0) != 0)
	{
		gEnv->pLog->LogError("sysctl failed\n");
	}
	else
	{
		lpmem->dwTotalPhys = static_cast<SIZE_T>(dwTotalPhys);
	}

	// Get the page size
	mach_port_t kHost(mach_host_self());
	vm_size_t uPageSize;
	if (host_page_size(kHost, &uPageSize) != 0)
	{
		gEnv->pLog->LogError("host_page_size failed\n");
	}
	else
	{
		// Get memory statistics
		vm_statistics_data_t kVMStats;
		mach_msg_type_number_t uCount(sizeof(kVMStats) / sizeof(natural_t));
		if (host_statistics(kHost, HOST_VM_INFO, (host_info_t)&kVMStats, &uCount) != 0)
		{
			gEnv->pLog->LogError("host_statistics failed\n");
		}
		else
		{
			// Calculate dwAvailPhys
			lpmem->dwAvailPhys = uPageSize * kVMStats.free_count;
		}
	}
#else
	FILE* f;
	lpmem->dwMemoryLoad = 0;
	lpmem->dwTotalPhys = 16 * 1024 * 1024;
	lpmem->dwAvailPhys = 16 * 1024 * 1024;
	lpmem->dwTotalPageFile = 16 * 1024 * 1024;
	lpmem->dwAvailPageFile = 16 * 1024 * 1024;
	f = ::fopen("/proc/meminfo", "r");
	if (f)
	{
		char buffer[256];
		memset(buffer, '0', 256);
		int total, used, free, shared, buffers, cached;

		lpmem->dwLength = sizeof(MEMORYSTATUS);
		lpmem->dwTotalPhys = lpmem->dwAvailPhys = 0;
		lpmem->dwTotalPageFile = lpmem->dwAvailPageFile = 0;
		while (fgets(buffer, sizeof(buffer), f))
		{
			if (sscanf(buffer, "Mem: %d %d %d %d %d %d", &total, &used, &free, &shared, &buffers, &cached))
			{
				lpmem->dwTotalPhys += total;
				lpmem->dwAvailPhys += free + buffers + cached;
			}
			if (sscanf(buffer, "Swap: %d %d %d", &total, &used, &free))
			{
				lpmem->dwTotalPageFile += total;
				lpmem->dwAvailPageFile += free;
			}
			if (sscanf(buffer, "MemTotal: %d", &total))
				lpmem->dwTotalPhys = total * 1024;
			if (sscanf(buffer, "MemFree: %d", &free))
				lpmem->dwAvailPhys = free * 1024;
			if (sscanf(buffer, "SwapTotal: %d", &total))
				lpmem->dwTotalPageFile = total * 1024;
			if (sscanf(buffer, "SwapFree: %d", &free))
				lpmem->dwAvailPageFile = free * 1024;
			if (sscanf(buffer, "Buffers: %d", &buffers))
				lpmem->dwAvailPhys += buffers * 1024;
			if (sscanf(buffer, "Cached: %d", &cached))
				lpmem->dwAvailPhys += cached * 1024;
		}
		fclose(f);
		if (lpmem->dwTotalPhys)
		{
			DWORD TotalPhysical = lpmem->dwTotalPhys + lpmem->dwTotalPageFile;
			DWORD AvailPhysical = lpmem->dwAvailPhys + lpmem->dwAvailPageFile;
			lpmem->dwMemoryLoad = (TotalPhysical - AvailPhysical) / (TotalPhysical / 100);
		}
	}
#endif
}

DWORD Sleep(DWORD dwMilliseconds)
{
	timespec req;
	timespec rem;

	memset(&req, 0, sizeof(req));
	memset(&rem, 0, sizeof(rem));

	time_t sec = (int)(dwMilliseconds / 1000);
	req.tv_sec = sec;
	req.tv_nsec = (dwMilliseconds - (sec * 1000)) * 1000000L;
	if (nanosleep(&req, &rem) == -1)
		nanosleep(&rem, 0);

	return 0;
}
