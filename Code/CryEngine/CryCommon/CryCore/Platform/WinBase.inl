// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   WinBase.cpp
//  Version:     v1.00
//  Created:     10/3/2004 by Michael Glueck
//  Compilers:   GCC
//  Description: Linux/Mac port support for Win32API calls
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#if !defined(CRY_PLATFORM_IMPL_H_FILE)
	#error This file is not allowed to be used directly, use platform_impl.h instead.
#endif

#include <CryCore/Assert/CryAssert.h>
#include <CryString/StringUtils.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#ifdef USE_FILE_HANDLE_CACHE
	#include <md5/md5.h>
#endif

#if CRY_PLATFORM_APPLE
	#include "../CrySystem/SystemUtilsApple.h"
#endif

#if CRY_PLATFORM_ANDROID
	#define FIX_FILENAME_CASE 0 // everything is lower case on android
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_APPLE
	#define FIX_FILENAME_CASE 1
#endif

#if !defined(PATH_MAX)
	#define PATH_MAX MAX_PATH
#endif

#if !defined(_RELEASE) || defined(_DEBUG)
	#include <set>
unsigned int g_EnableMultipleAssert = 0;//set to something else than 0 if to enable already reported asserts
#endif

#ifdef USE_FILE_HANDLE_CACHE
void OpenFromFileHandleCacheAsFD(const char* adjustedFilename, int flags, int& fd, int dummy, int& err);
void CloseFromFileHandleCacheAsFD(int fd);
#endif

//////////////////////////////////////////////////////////////////////////
// File System
//////////////////////////////////////////////////////////////////////////
typedef int FS_ERRNO_TYPE;

static const int FS_O_RDWR = O_RDWR;
static const int FS_O_RDONLY = O_RDONLY;
static const int FS_O_WRONLY = O_WRONLY;
static const FS_ERRNO_TYPE FS_EISDIR = EISDIR;

#define INCLUDED_FROM_CRY_WINBASE_IMPL

#if CRY_PLATFORM_ORBIS
	#include <CryCore/Platform/WinBase_sce.inl>
#elif CRY_PLATFORM_POSIX
	#include <CryCore/Platform/WinBase_posix.inl>
#endif

#undef INCLUDED_FROM_CRY_WINBASE_IMPL

//////////////////////////////////////////////////////////////////////////
// Assert
//////////////////////////////////////////////////////////////////////////

#if (!defined(_RELEASE) || defined(_DEBUG))
struct SAssertData
{
	int  line;
	char fileName[256 - sizeof(int)];
	const bool operator==(const SAssertData& crArg) const
	{
		return crArg.line == line && (strcmp(fileName, crArg.fileName) == 0);
	}

	const bool operator<(const SAssertData& crArg) const
	{
		if (line == crArg.line)
			return strcmp(fileName, crArg.fileName) < 0;
		else
			return line < crArg.line;
	}

	SAssertData() : line(-1){}
	SAssertData(const int cLine, const char* cpFile) : line(cLine)
	{
		cry_strcpy(fileName, cpFile);
	}

	SAssertData(const SAssertData& crAssertData)
	{
		memcpy((void*)this, &crAssertData, sizeof(SAssertData));
	}

	void operator=(const SAssertData& crAssertData)
	{
		memcpy((void*)this, &crAssertData, sizeof(SAssertData));
	}
};

//#define OUTPUT_ASSERT_TO_FILE

void HandleAssert(const char* cpMessage, const char* cpFunc, const char* cpFile, const int cLine)
{
	#if defined(OUTPUT_ASSERT_TO_FILE)
	static FILE* pAssertLogFile = fopen("Assert.log", "w+");
	#endif
	bool report = true;
	static std::set<SAssertData> assertSet;
	SAssertData assertData(cLine, cpFile);
	if (!g_EnableMultipleAssert)
	{
		std::set<SAssertData>::const_iterator it = assertSet.find(assertData);
		if (it != assertSet.end())
			report = false;
		else
			assertSet.insert(assertData);
	}
	else
		assertSet.insert(assertData);
	if (report)
	{
		//added function to be able to place a breakpoint here or to print out to other consoles
		printf("ASSERT: %s in %s (%s : %d)\n", cpMessage, cpFunc, cpFile, cLine);
	#if defined(OUTPUT_ASSERT_TO_FILE)
		if (pAssertLogFile)
		{
			fprintf(pAssertLogFile, "ASSERT: %s in %s (%s : %d)\n", cpMessage, cpFunc, cpFile, cLine);
			fflush(pAssertLogFile);
		}
	#endif
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
// File System
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
extern size_t __attribute__((visibility("default"))) fopenwrapper_basedir_maxsize;
extern char* fopenwrapper_basedir __attribute__((visibility("default")));
extern bool __attribute__((visibility("default"))) fopenwrapper_trace_fopen;
#endif

#if CRY_PLATFORM_ORBIS
const char* const ConvertFileName(char* const pBuf, const size_t bufSize, const char* const cpName)
{
	if (cpName[0] != '/')
	{
		// Convert relative path
		cry_strcpy(pBuf, bufSize, gEnv->pSystem->GetRootFolder());
		cry_strcat(pBuf, bufSize, cpName);
		return pBuf;
	}
	// Return absolute path
	pBuf[0] = '\0';
	return cpName;
}
#else
const bool        ConvertFilenameNoCase(char*);
const char* const ConvertFileName(char* const pBuf, const size_t bufSize, const char* const cpName)
{
	// This function will not change the length of the string
	// So if the target buffer is at least as large as the input, there's no risk of overflow.
	if (bufSize <= strlen(cpName))
	{
		if (bufSize && pBuf) *pBuf = '\0';
		return nullptr;
	}

	cry_strcpy(pBuf, bufSize, cpName);
	ConvertFilenameNoCase(pBuf);
	return pBuf;
}
#endif

inline void WrappedF_InitCWD()
{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#if !defined(APPLE_BUNDLE)
	if (getcwd(fopenwrapper_basedir, fopenwrapper_basedir_maxsize) == NULL)
	{
		fprintf(stderr, "getcwd(): %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	fopenwrapper_basedir[fopenwrapper_basedir_maxsize - 1] = 0;
	#endif

	// Move up to parent of BinXX directory.
	// /home/user/depot/Main/Build/Linux-Arc/Bin -> /home/user/depot/Main
	// [K01]: comment below code till real build
	/*
	   char *ptr = strrchr(fopenwrapper_basedir, '/');
	   if (ptr != NULL)
	   {
	   bool upOneDir = false;
	   if (strcasecmp(ptr, "/Bin32") == 0)
	    upOneDir = true;
	   else if (strcasecmp(ptr, "/Bin64") == 0)
	    upOneDir = true;

	   if (upOneDir)
	   {
	 * ptr = '\0';
	    chdir(fopenwrapper_basedir);
	   }
	   }*/
	SetModulePath(fopenwrapper_basedir);
	char* ptr = strstr(fopenwrapper_basedir, "/Build");
	if (ptr != NULL)
	{
		*ptr = '\0';
		chdir(fopenwrapper_basedir);
	}
#endif
}

bool IsBadReadPtr(void* ptr, unsigned int size)
{
	//too complicated to really support it
	return ptr ? false : true;
}

//////////////////////////////////////////////////////////////////////////
char* _strtime(char* date)
{
	strcpy(date, "0:0:0");
	return date;
}

//////////////////////////////////////////////////////////////////////////
char* _strdate(char* date)
{
	strcpy(date, "0");
	return date;
}

//////////////////////////////////////////////////////////////////////////
char* strlwr(char* str)
{
	char* cp;               /* traverses string for C locale conversion */

	for (cp = str; *cp; ++cp)
	{
		if ('A' <= *cp && *cp <= 'Z')
			*cp += 'a' - 'A';
	}
	return str;
}

char* strupr(char* str)
{
	char* cp;               /* traverses string for C locale conversion */

	for (cp = str; *cp; ++cp)
	{
		if ('a' <= *cp && *cp <= 'z')
			*cp += 'A' - 'a';
	}
	return str;
}

char* ltoa(long i, char* a, int radix)
{
	if (a == NULL) return NULL;
	strcpy(a, "0");
	if (i && radix > 1 && radix < 37)
	{
		char buf[35];
		unsigned long u = i, p = 34;
		buf[p] = 0;
		if (i < 0 && radix == 10) u = -i;
		while (u)
		{
			unsigned int d = u % radix;
			buf[--p] = d < 10 ? '0' + d : 'a' + d - 10;
			u /= radix;
		}
		if (i < 0 && radix == 10) buf[--p] = '-';
		strcpy(a, buf + p);
	}
	return a;
}

#if CRY_PLATFORM_ANDROID
// For Linux it's redefined to wcscasecmp and wcsncasecmp'
int wcsicmp(const wchar_t* s1, const wchar_t* s2)
{
	wint_t c1, c2;

	if (s1 == s2)
		return 0;

	do
	{
		c1 = towlower(*s1++);
		c2 = towlower(*s2++);
	}
	while (c1 && c1 == c2);

	return (int) (c1 - c2);
}

int wcsnicmp(const wchar_t* s1, const wchar_t* s2, size_t count)
{
	wint_t c1, c2;
	if (s1 == s2 || count == 0)
		return 0;

	do
	{
		c1 = towlower(*s1++);
		c2 = towlower(*s2++);
	}
	while ((--count) && c1 && (c1 == c2));
	return (int) (c1 - c2);
}
#endif

void _makepath(char* path, const char* drive, const char* dir, const char* filename, const char* ext)
{
	char ch;
	char tmp[MAX_PATH];
	if (!path)
		return;
	tmp[0] = '\0';
	if (drive && drive[0])
	{
		tmp[0] = drive[0];
		tmp[1] = ':';
		tmp[2] = 0;
	}
	if (dir && dir[0])
	{
		cry_strcat(tmp, dir);
		ch = tmp[strlen(tmp) - 1];
		if (ch != '/' && ch != '\\')
			cry_strcat(tmp, "\\");
	}
	if (filename && filename[0])
	{
		cry_strcat(tmp, filename);
		if (ext && ext[0])
		{
			if (ext[0] != '.')
				cry_strcat(tmp, ".");
			cry_strcat(tmp, ext);
		}
	}
	strcpy(path, tmp);
}

char* _ui64toa(unsigned long long value, char* str, int radix)
{
	if (str == 0)
		return 0;

	char buffer[65];
	char* pos;
	int digit;

	pos = &buffer[64];
	*pos = '\0';

	do
	{
		digit = value % radix;
		value = value / radix;
		if (digit < 10)
		{
			*--pos = '0' + digit;
		}
		else
		{
			*--pos = 'a' + digit - 10;
		} /* if */
	}
	while (value != 0L);

	memcpy(str, pos, &buffer[64] - pos + 1);
	return str;
}

long long _atoi64(const char* str)
{
	if (str == 0)
		return -1;
	unsigned long long RunningTotal = 0;
	char bMinus = 0;
	while (*str == ' ' || (*str >= '\011' && *str <= '\015'))
	{
		str++;
	} /* while */
	if (*str == '+')
	{
		str++;
	}
	else if (*str == '-')
	{
		bMinus = 1;
		str++;
	} /* if */
	while (*str >= '0' && *str <= '9')
	{
		RunningTotal = RunningTotal * 10 + *str - '0';
		str++;
	} /* while */
	return bMinus ? ((long long)-RunningTotal) : (long long)RunningTotal;
}

#if !CRY_PLATFORM_ORBIS
bool QueryPerformanceCounter(LARGE_INTEGER* counter)
{
	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	// replaced gettimeofday
	// http://fixunix.com/kernel/378888-gettimeofday-resolution-linux.html
	timespec tv;
	clock_gettime(CLOCK_MONOTONIC, &tv);
	counter->QuadPart = (uint64)tv.tv_sec * 1000000 + tv.tv_nsec / 1000;
	return true;
	#elif CRY_PLATFORM_APPLE
	counter->QuadPart = mach_absolute_time();
	return true;
	#else
	return false;
	#endif
}

bool QueryPerformanceFrequency(LARGE_INTEGER* frequency)
{
	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	// On Linux we'll use gettimeofday().  The API resolution is microseconds,
	// so we'll report that to the caller.
	frequency->u.LowPart = 1000000;
	frequency->u.HighPart = 0;
	return true;
	#elif CRY_PLATFORM_APPLE
	static mach_timebase_info_data_t s_kTimeBaseInfoData;
	if (s_kTimeBaseInfoData.denom == 0)
		mach_timebase_info(&s_kTimeBaseInfoData);
	// mach_timebase_info_data_t expresses the tick period in nanoseconds
	frequency->QuadPart = 1e+9 * (uint64_t)s_kTimeBaseInfoData.denom / (uint64_t)s_kTimeBaseInfoData.numer;
	return true;
	#else
	return false;
	#endif
}
#endif

void _splitpath(const char* inpath, char* drv, char* dir, char* fname, char* ext)
{
	if (drv)
		drv[0] = 0;

	typedef CryStackStringT<char, 256> path_stack_string;

	const path_stack_string inPath(inpath);
	string::size_type s = inPath.rfind('/', inPath.size());//position of last /
	path_stack_string fName;
	if (s == string::npos)
	{
		if (dir)
			dir[0] = 0;
		fName = inpath; //assign complete string as rest
	}
	else
	{
		if (dir)
			strcpy(dir, (inPath.substr((string::size_type)0, (string::size_type)(s + 1))).c_str()); //assign directory
		fName = inPath.substr((string::size_type)(s + 1));                                        //assign remaining string as rest
	}
	if (fName.size() == 0)
	{
		if (ext)
			ext[0] = 0;
		if (fname)
			fname[0] = 0;
	}
	else
	{
		//dir and drive are now set
		s = fName.find(".", (string::size_type)0);//position of first .
		if (s == string::npos)
		{
			if (ext)
				ext[0] = 0;
			if (fname)
				strcpy(fname, fName.c_str()); //assign filename
		}
		else
		{
			if (ext)
				strcpy(ext, (fName.substr(s)).c_str());   //assign extension including .
			if (fname)
			{
				if (s == 0)
					fname[0] = 0;
				else
					strcpy(fname, (fName.substr((string::size_type)0, s)).c_str());  //assign filename
			}
		}
	}
}

DWORD GetFileAttributes(LPCSTR lpFileName)
{
	struct stat fileStats;
#if CRY_PLATFORM_ORBIS
	char buf[512];
	lpFileName = ConvertFileName(buf, sizeof(buf), lpFileName);
#endif
	const int success = stat(lpFileName, &fileStats);
	if (success == -1)
	{
		char adjustedFilename[MAX_PATH];
		cry_strcpy(adjustedFilename, lpFileName);
		ConvertFilenameNoCase(adjustedFilename);
		if (stat(adjustedFilename, &fileStats) == -1)
			return (DWORD)INVALID_FILE_ATTRIBUTES;
	}
	DWORD ret = 0;

	const int acc = (fileStats.st_mode & S_IWRITE);

	if (acc != 0)
	{
		if (S_ISDIR(fileStats.st_mode) != 0)
		{
			ret |= FILE_ATTRIBUTE_DIRECTORY;
		}
	}
	return (ret == 0) ? FILE_ATTRIBUTE_NORMAL : ret;//return file attribute normal as the default value, must only be set if no other attributes have been found
}

//////////////////////////////////////////////////////////////////////////
int memicmp(LPCSTR s1, LPCSTR s2, DWORD len)
{
	int ret = 0;
	while (len--)
	{
		if ((ret = tolower(*s1) - tolower(*s2))) break;
		s1++;
		s2++;
	}
	return ret;
}

//////////////////////////////////////////////////////////////////////////
int strcmpi(const char* str1, const char* str2)
{
	for (;; )
	{
		int ret = tolower(*str1) - tolower(*str2);
		if (ret || !*str1) return ret;
		str1++;
		str2++;
	}
}

//-----------------------------------------other stuff-------------------------------------------------------------------

static const int YearLengths[2] = { DAYSPERNORMALYEAR, DAYSPERLEAPYEAR };
static const int MonthLengths[2][MONSPERYEAR] =
{
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static int IsLeapYear(int Year)
{
	return Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0) ? 1 : 0;
}

static void NormalizeTimeFields(short* FieldToNormalize, short* CarryField, int Modulus)
{
	*FieldToNormalize = (short) (*FieldToNormalize - Modulus);
	*CarryField = (short) (*CarryField + 1);
}

bool TimeFieldsToTime(PTIME_FIELDS tfTimeFields, PLARGE_INTEGER Time)
{
#define SECSPERMIN   60
#define MINSPERHOUR  60
#define HOURSPERDAY  24
#define MONSPERYEAR  12

#define EPOCHYEAR    1601

#define SECSPERDAY   86400
#define TICKSPERMSEC 10000
#define TICKSPERSEC  10000000
#define SECSPERHOUR  3600

	int CurYear, CurMonth;
	LONGLONG rcTime;
	TIME_FIELDS TimeFields = *tfTimeFields;

	rcTime = 0;
	while (TimeFields.Second >= SECSPERMIN)
	{
		NormalizeTimeFields(&TimeFields.Second, &TimeFields.Minute, SECSPERMIN);
	}
	while (TimeFields.Minute >= MINSPERHOUR)
	{
		NormalizeTimeFields(&TimeFields.Minute, &TimeFields.Hour, MINSPERHOUR);
	}
	while (TimeFields.Hour >= HOURSPERDAY)
	{
		NormalizeTimeFields(&TimeFields.Hour, &TimeFields.Day, HOURSPERDAY);
	}
	while (TimeFields.Day > MonthLengths[IsLeapYear(TimeFields.Year)][TimeFields.Month - 1])
	{
		NormalizeTimeFields(&TimeFields.Day, &TimeFields.Month, SECSPERMIN);
	}
	while (TimeFields.Month > MONSPERYEAR)
	{
		NormalizeTimeFields(&TimeFields.Month, &TimeFields.Year, MONSPERYEAR);
	}
	for (CurYear = EPOCHYEAR; CurYear < TimeFields.Year; CurYear++)
	{
		rcTime += YearLengths[IsLeapYear(CurYear)];
	}
	for (CurMonth = 1; CurMonth < TimeFields.Month; CurMonth++)
	{
		rcTime += MonthLengths[IsLeapYear(CurYear)][CurMonth - 1];
	}
	rcTime += TimeFields.Day - 1;
	rcTime *= SECSPERDAY;
	rcTime += TimeFields.Hour * SECSPERHOUR + TimeFields.Minute * SECSPERMIN + TimeFields.Second;

	rcTime *= TICKSPERSEC;
	rcTime += TimeFields.Milliseconds * TICKSPERMSEC;

	Time->QuadPart = rcTime;

	return true;
}

BOOL SystemTimeToFileTime(const SYSTEMTIME* syst, LPFILETIME ft)
{
	TIME_FIELDS tf;
	LARGE_INTEGER t;

	tf.Year = syst->wYear;
	tf.Month = syst->wMonth;
	tf.Day = syst->wDay;
	tf.Hour = syst->wHour;
	tf.Minute = syst->wMinute;
	tf.Second = syst->wSecond;
	tf.Milliseconds = syst->wMilliseconds;

	TimeFieldsToTime(&tf, &t);
	ft->dwLowDateTime = t.u.LowPart;
	ft->dwHighDateTime = t.u.HighPart;
	return TRUE;
}

//begin--------------------------------findfirst/-next implementation----------------------------------------------------

void __finddata64_t::CopyFoundData(const char* rMatchedFileName)
{
	FS_STAT_TYPE fsStat;
	FS_ERRNO_TYPE fsErr = 0;
	bool isDir = false, isReadonly = false;

	memset(&fsStat, 0, sizeof(fsStat));
	const int cCurStrLen = strlen(name);

	if (cCurStrLen > 0 && name[cCurStrLen - 1] != '/')
	{
		name[cCurStrLen] = '/';
		name[cCurStrLen + 1] = 0;
	}
	cry_strcpy(name, rMatchedFileName);
	// Remove trailing slash for directories.
	if (name[0] && name[strlen(name) - 1] == '/')
	{
		name[strlen(name) - 1] = 0;
		isDir = true;
	}

	//more eff.impl for:  string filename = string(m_DirectoryName) + "/" + name;
	char filename[MAX_PATH];
	const int cDirStrLen = strlen(m_DirectoryName);
	char* pDirCur = m_DirectoryName;
	char* pDst = filename;
	for (int i = 0; i < cDirStrLen; ++i)
		*pDst++ = *pDirCur++;
	if (pDirCur[-1] != '/')
		*pDirCur = '/';
	const int cNameLen = strlen(name);
	pDirCur = name;
	for (int i = 0; i < cNameLen; ++i)
		*pDst++ = *pDirCur++;
	*pDirCur = 0;
	*pDst = 0;

	// Check if the file is a directory and/or is read-only.
	if (!isDir)
	{
		CryFileSystem::FS_STAT(filename, fsStat, fsErr);
		if (fsErr) return;
	}
	(void)fsErr;

	// This is how it should be done. However, the st_mode field of
	// CellFsStat is currently unimplemented by the current version of the
	// Cell SDK (0.5.0).
	if (S_ISDIR(fsStat.st_mode))
	{
		isDir = true;
	}
	if (!(fsStat.st_mode & S_IWRITE))
	{
		isReadonly = true;
	}

	if (isDir)
		attrib |= _A_SUBDIR;
	else
		attrib &= ~_A_SUBDIR;
	if (isReadonly)
		attrib |= _A_RDONLY;
	else
		attrib &= ~_A_RDONLY;
	if (!isDir)
	{
		size = fsStat.st_size;
		time_access = fsStat.st_atime;
		time_write = fsStat.st_mtime;
		time_create = fsStat.st_ctime;
	}
	else
	{
		size = 0;
		time_access = time_write = time_create = 0;
	}
}

// Map all path components to the correct case.
// Slashes must be normalized to fwdslashes (/).
// Trailing path component is ignored (i.e. dirnames must end with a /).
// Returns 0 on success and -1 on error.
// In case of an error, all matched path components will have been case
// corrected.
int FixDirnameCase(char* path, int index = 0)
{

#if !FIX_FILENAME_CASE
	path += index;
	while (*path++)
		*path = tolower(*path);
	return 0;
#endif

	FS_ERRNO_TYPE fsErr = 0;
	char* slash;
	FS_DIR_TYPE dir = FS_DIR_NULL;
	bool pathOk = false;
	char* parentSlash;

	slash = strchr(path + index + 1, '/');
	if (!slash) return 0;
	*slash = 0;
	parentSlash = strrchr(path, '/');

	// Check if path is a valid directory.
	CryFileSystem::FS_OPENDIR(path, dir, fsErr);
	if (!fsErr)
	{
		pathOk = true;
		CryFileSystem::FS_CLOSEDIR_NOERR(dir);
		dir = FS_DIR_NULL;
	}
	else if (fsErr != FS_ENOENT && fsErr != FS_EINVAL)
	{
		*slash = '/';
		return -1;
	}

	if (!pathOk)
	{
		// Get the parent dir.
		const char* parent;
		char* name;
		if (parentSlash)
		{
			*parentSlash = 0;
			parent = path;
			if (!*parent) parent = "/";
			name = parentSlash + 1;
		}
		else
		{
			parent = ".";
			name = path;
		}

		// Scan parent.
		CryFileSystem::FS_OPENDIR(parent, dir, fsErr);
		if (fsErr)
		{
			if (parentSlash) *parentSlash = '/';
			*slash = '/';
			return -1;
		}
		FS_DIRENT_TYPE dirent;
		uint64_t direntSize = 0;
		while (true)
		{
			CryFileSystem::FS_READDIR(dir, dirent, direntSize, fsErr);
			if (fsErr)
			{
				CryFileSystem::FS_CLOSEDIR_NOERR(dir);
				if (parentSlash) *parentSlash = '/';
				*slash = '/';
				return -1;
			}
			if (direntSize == 0) break;
#if CRY_PLATFORM_ORBIS
			size_t len = dirent.d_namlen;
			const char* dirEntName = dirent.d_name;
			if (len > 0 && dirEntName[len - 1] == '/') len -= 1;
			if (!strncasecmp(dirEntName, name, len))
			{
				pathOk = true;
				if (parentSlash)
					memcpy(parentSlash + 1, dirEntName, len);
				else
					memcpy(path, dirEntName, len);
				break;
			}
#else
			size_t len = strlen(dirent.d_name);

			if (len > 0 && dirent.d_name[len - 1] == '/') len -= 1;
			if (!strncasecmp(dirent.d_name, name, len))
			{
				pathOk = true;
				if (parentSlash)
					memcpy(parentSlash + 1, dirent.d_name, len);
				else
					memcpy(path, dirent.d_name, len);
				break;
			}
#endif
		}
		CryFileSystem::FS_CLOSEDIR(dir, fsErr);
		if (parentSlash) *parentSlash = '/';
		if (fsErr)
		{
			*slash = '/';
			return -1;
		}
	}
	*slash = '/';

	// Recurse.
	if (pathOk)
	{
		return FixDirnameCase(path, slash - path);
	}

	return -1;
}

// Match the specified name against the specified glob-pattern.
// Returns true iff name matches pattern.
static bool matchPattern(const char* name, const char* pattern)
{
	while (true)
	{
		if (!*pattern) return !*name;
		switch (*pattern)
		{
		case '?':
			if (!*name) return false;
			++name;
			++pattern;
			break;
		case '*':
			++pattern;
			while (true)
			{
				if (matchPattern(name, pattern)) return true;
				if (!*name) return false;
				++name;
			}
			break; // Not reached.
		default:
			if (strnicmp(name, pattern, 1)) return false;
			++name;
			++pattern;
			break;
		}
	}
}

intptr_t _findfirst64(const char* pFileName, __finddata64_t* pFindData)
{
#if CRY_PLATFORM_ORBIS
	//path is still relative, convert to absolute path
	char buf[512];
	pFileName = ConvertFileName(buf, sizeof(buf), pFileName);
	//printf("_findfirst64:'%s' -> '%s'\n", pFileName, buf);
#endif
	FS_ERRNO_TYPE fsErr = 0;
	char filename[MAX_PATH];
	size_t filenameLength = 0;
	const char* dirname = 0;
	const char* pattern = 0;

	pFindData->m_LastIndex = -1;
	cry_strcpy(filename, pFileName);
	filenameLength = strlen(filename);

	// Normalize ".*" and "*.*" suffixes to "*".
	if (!strcmp(filename + filenameLength - 3, "*.*"))
		filename[filenameLength - 2] = 0;
	else if (!strcmp(filename + filenameLength - 2, ".*"))
	{
		filename[filenameLength - 2] = '*';
		filename[filenameLength - 1] = 0;
	}

	// Map backslashes to fwdslashes.
	const int cLen = strlen(pFileName);
	for (int i = 0; i < cLen; ++i)
		if (filename[i] == '\\') filename[i] = '/';

	// Get the dirname.
	char* slash = strrchr(filename, '/');
	if (slash)
	{
#if !CRY_PLATFORM_ORBIS
		if (FixDirnameCase(filename) == -1)
			return -1;
#endif
		pattern = slash + 1;
		dirname = filename;
		*slash = 0;
	}
	else
	{
		dirname = "./";
		pattern = filename;
	}
	cry_strcpy(pFindData->m_ToMatch, pattern);

	// Close old directory descriptor.
	if (pFindData->m_Dir != FS_DIR_NULL)
	{
		CryFileSystem::FS_CLOSEDIR(pFindData->m_Dir, fsErr);
		pFindData->m_Dir = FS_DIR_NULL;
		if (fsErr)
			return -1;
	}

	{
		// Open and read directory.
		CryFileSystem::FS_OPENDIR(dirname, pFindData->m_Dir, fsErr);

		if (fsErr)
			return -1;
		cry_strcpy(pFindData->m_DirectoryName, dirname);
		FS_DIRENT_TYPE dirent;
		uint64_t direntSize = 0;
		while (true)
		{
			CryFileSystem::FS_READDIR(pFindData->m_Dir, dirent, direntSize, fsErr);
			if (fsErr)
			{
				CryFileSystem::FS_CLOSEDIR_NOERR(pFindData->m_Dir);
				pFindData->m_Dir = FS_DIR_NULL;
				return -1;
			}
			if (direntSize == 0)
				break;
			if (!strcmp(dirent.d_name, ".")
			    || !strcmp(dirent.d_name, "./")
			    || !strcmp(dirent.d_name, "..")
			    || !strcmp(dirent.d_name, "../"))
				continue;
			// We'll add a trailing slash to the name to identify directory
			// entries.
			char d_name[MAX_PATH];
			cry_strcpy(d_name, dirent.d_name);
			// On Mac OS X Sometimes the \0 dilimeter does not get updated to the correct position.
#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
			d_name[dirent.d_namlen] = '\0';
#endif
			if (dirent.d_type == FS_TYPE_DIRECTORY)
			{
				const int cLen = strlen(d_name);
				if (d_name[0] || d_name[cLen - 1] != '/')
					cry_strcat(d_name, "/");
			}
			pFindData->m_Entries.push_back(d_name);
		}
		CryFileSystem::FS_CLOSEDIR_NOERR(pFindData->m_Dir);
		pFindData->m_Dir = FS_DIR_NULL;
	}

	// Locate first match.
	int i = 0;
	const std::vector<string>::const_iterator cEnd = pFindData->m_Entries.end();
	for (std::vector<string>::const_iterator iter = pFindData->m_Entries.begin(); iter != cEnd; ++iter)
	{
		const char* cpEntry = iter->c_str();
		if (matchPattern(cpEntry, pattern))
		{
			pFindData->CopyFoundData(cpEntry);
			pFindData->m_LastIndex = i;
			break;
		}
		++i;
	}
	return pFindData->m_LastIndex;
};

int _findnext64(intptr_t last, __finddata64_t* pFindData)
{
	if (last == -1 || pFindData->m_LastIndex == -1)
		return -1;
	if (pFindData->m_LastIndex + 1 >= pFindData->m_Entries.size())
		return -1;

	int found = -1;
	int i = pFindData->m_LastIndex + 1;
	pFindData->m_LastIndex = -1;
	for (
	  std::vector<string>::const_iterator iter = pFindData->m_Entries.begin() + i;
	  iter != pFindData->m_Entries.end();
	  ++iter)
	{
		if (matchPattern(iter->c_str(), pFindData->m_ToMatch))
		{
			pFindData->CopyFoundData(iter->c_str());
			pFindData->m_LastIndex = i;
			found = 0;
			break;
		}
		++i;
	}
	return found;
}

//////////////////////////////////////////////////////////////////////////
int _findclose(intptr_t handle)
{
	return 0;//we dont need this
}

__finddata64_t::~__finddata64_t()
{
	if (m_Dir != FS_DIR_NULL)
	{
		CryFileSystem::FS_CLOSEDIR_NOERR(m_Dir);
		m_Dir = FS_DIR_NULL;
	}
}

//end--------------------------------findfirst/-next implementation----------------------------------------------------
void adaptFilenameToLinux(string& rAdjustedFilename)
{
	//first replace all \\ by /
	string::size_type loc = 0;
	while ((loc = rAdjustedFilename.find("\\", loc)) != string::npos)
	{
		rAdjustedFilename.replace(loc, 1, "/");
	}
	loc = 0;
	//remove /./
	while ((loc = rAdjustedFilename.find("/./", loc)) != string::npos)
	{
		rAdjustedFilename.replace(loc, 3, "/");
	}
}

void replaceDoublePathFilename(char* szFileName)
{
	//replace "\.\" by "\"
	string s(szFileName);
	string::size_type loc = 0;
	//remove /./
	while ((loc = s.find("/./", loc)) != string::npos)
	{
		s.replace(loc, 3, "/");
	}
	loc = 0;
	//remove "\.\"
	while ((loc = s.find("\\.\\", loc)) != string::npos)
	{
		s.replace(loc, 3, "\\");
	}
	strcpy((char*)szFileName, s.c_str());
}

const int comparePathNames(const char* cpFirst, const char* cpSecond, unsigned int len)
{
	//create two strings and replace the \\ by / and /./ by /
	string first(cpFirst);
	string second(cpSecond);
	adaptFilenameToLinux(first);
	adaptFilenameToLinux(second);
	if (strlen(cpFirst) < len || strlen(cpSecond) < len)
		return -1;
	unsigned int length = std::min(std::min(first.size(), second.size()), (size_t)len);  //make sure not to access invalid memory
	return memicmp(first.c_str(), second.c_str(), length);
}

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
static bool FixOnePathElement(char* path)
{
	if (*path == '\0')
		return true;

	if ((path[0] == '/') && (path[1] == '\0'))
		return true;  // root dir always exists.

	if (strchr(path, '*') || strchr(path, '?'))
		return true; // wildcard...stop correcting path.

	struct stat statbuf;
	if (stat(path, &statbuf) != -1)    // current case exists.
		return true;

	char* name = path;
	char* ptr = strrchr(path, '/');
	if (ptr)
	{
		name = ptr + 1;
		*ptr = '\0';
	}

	if (*name == '\0')    // trailing '/' ?
	{
		*ptr = '/';
		return true;
	}

	const char* parent;
	if (ptr == path)
		parent = "/";
	else if (ptr == NULL)
		parent = ".";
	else
		parent = path;

	DIR* dirp = opendir(parent);
	if (ptr) *ptr = '/';

	if (dirp == NULL)
		return false;

	struct dirent* dent;
	bool found = false;
	while ((dent = readdir(dirp)) != NULL)
	{
		if (strcasecmp(dent->d_name, name) == 0)
		{
			strcpy(name, dent->d_name);
			found = true;
			break;
		}
	}

	closedir(dirp);
	return found;
}
#endif

#if !CRY_PLATFORM_ORBIS
const bool ConvertFilenameNoCase(char* szFilepathToAdj)
{
	assert(szFilepathToAdj);

	// Fix the dirname case.
	const int cLen = strlen(szFilepathToAdj);
	for (int i = 0; i < cLen; ++i)
	{
		if (szFilepathToAdj[i] == '\\')
		{
			szFilepathToAdj[i] = '/';
		}
	}

	char* slash;
	char* name;
	if ((szFilepathToAdj) == (char*)-1)
	{
		return false;
	}

	slash = strrchr(szFilepathToAdj, '/');
	if (slash)
	{
		name = slash + 1;
		*slash = 0;
	}
	else
	{
		name = szFilepathToAdj;
	}

	#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE // fix the parent path anyhow.
	// Check for wildcards. We'll always return true if the specified filename is
	// a wildcard pattern.
	if (strchr(name, '*') || strchr(name, '?'))
	{
		if (slash) *slash = '/';
		return true;
	}
	#endif

	if (slash)
	{
		*slash = '/';
	}

	#if FIX_FILENAME_CASE
	char* path = szFilepathToAdj;
	char* sep;
	while ((sep = strchr(path, '/')) != NULL)
	{
		*sep = '\0';
		const bool exists = FixOnePathElement(szFilepathToAdj);
		*sep = '/';
		if (!exists)
		{
			return false;
		}

		path = sep + 1;
	}
	if (!FixOnePathElement(szFilepathToAdj))    // catch last filename.
	{
		return false;
	}
	#else
	for (char* c = szFilepathToAdj; *c; ++c)
	{
		*c = tolower(*c);
	}
	#endif

	return true;
}
#endif //!CRY_PLATFORM_ORBIS

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
HANDLE CreateFile(
  const char* lpFileName,
  DWORD dwDesiredAccess,
  DWORD dwShareMode,
  void* lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile
  )
{
	int flags = 0;
	int fd = -1;
	FS_ERRNO_TYPE fserr;
	HANDLE h;

	if ((dwDesiredAccess & GENERIC_READ) == GENERIC_READ
	    && (dwDesiredAccess & GENERIC_WRITE) == GENERIC_WRITE)
		flags = O_RDWR;
	else if ((dwDesiredAccess & GENERIC_WRITE) == GENERIC_WRITE)
		flags = O_WRONLY;
	else
	{
		// On Windows files can be opened with no access. We'll map no access
		// to read only.
		flags = O_RDONLY;
	}
	if ((dwDesiredAccess & GENERIC_WRITE) == GENERIC_WRITE)
	{
		switch (dwCreationDisposition)
		{
		case CREATE_ALWAYS:
			flags |= O_CREAT | O_TRUNC;
			break;
		case CREATE_NEW:
			flags |= O_CREAT | O_EXCL;
			break;
		case OPEN_ALWAYS:
			flags |= O_CREAT;
			break;
		case OPEN_EXISTING:
			flags = O_RDONLY;
			break;
		case TRUNCATE_EXISTING:
			flags |= O_TRUNC;
			break;
		default:
			assert(0);
		}
	}

#if CRY_PLATFORM_ORBIS
	char buf[512];
	const char* adjustedFilename = ConvertFileName(buf, sizeof(buf), lpFileName);
#else
	char adjustedFilename[MAX_PATH];
	cry_strcpy(adjustedFilename, lpFileName);
	ConvertFilenameNoCase(adjustedFilename);
#endif

	bool failOpen = false;
	if (failOpen)
	{
		fd = -1;
		fserr = ENOENT;
	}
	else
	{
#if defined(USE_FILE_HANDLE_CACHE)
		OpenFromFileHandleCacheAsFD(adjustedFilename, flags, fd, 0, fserr);
#else
		CryFileSystem::FS_OPEN(adjustedFilename, flags, fd, 0, fserr);
#endif
	}

	if (fd == -1)
	{
		h = INVALID_HANDLE_VALUE;
	}
	else
	{
		h = (HANDLE)fd;
	}
	return h;
}

#define Int32x32To64(a, b) ((uint64)((uint64)(a)) * (uint64)((uint64)(b)))

/*
   //////////////////////////////////////////////////////////////////////////
   BOOL SetFileTime(
                        HANDLE hFile,
                        const FILETIME *lpCreationTime,
                        const FILETIME *lpLastAccessTime,
                        const FILETIME *lpLastWriteTime )
   {
   CRY_ASSERT_MESSAGE(0, "SetFileTime not implemented yet");
   return FALSE;
   }
 */
BOOL SetFileTime(const char* lpFileName, const FILETIME* lpLastAccessTime)
{
#if CRY_PLATFORM_ORBIS
	char buf[512];
	const char* const adjustedFilename = ConvertFileName(buf, sizeof(buf), lpFileName);
#else
	// Craig: can someone get a better impl here?
	char adjustedFilename[MAX_PATH];
	cry_strcpy(adjustedFilename, lpFileName);
	ConvertFilenameNoCase(adjustedFilename);
#endif

#if CRY_PLATFORM_ORBIS
	ORBIS_TO_IMPLEMENT;
	return false;
#else
	struct utimbuf timeBuf;
	memset(&timeBuf, 0, sizeof timeBuf);
	timeBuf.actime = *(time_t*)lpLastAccessTime;
	timeBuf.modtime = timeBuf.actime;
	return utime(adjustedFilename, &timeBuf) == 0;
#endif
}

//returns modification time of file
//for testing we do just store the time as is without conversion to win32 system time
const uint64 GetFileModifTime(FILE* hFile)
{
	FS_ERRNO_TYPE fsErr = 0;
	FS_STAT_TYPE st;
	CryFileSystem::FS_FSTAT(fileno(hFile), st, fsErr);

	(void)fsErr;
	// UnixTimeToFileTime
	// since the cache filesystem uses fat32 and has a 2 seconds resolution time, and the
	// cellfs has a 1 seconds resolution timer, it is necessary to mask out the last second
	time_t fixed_time = st.st_mtime & ~1ull;

	return Int32x32To64(fixed_time, 10000000) + 116444736000000000ll;
}

//////////////////////////////////////////////////////////////////////////
/*
   BOOL GetFileTime(HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime)
   {
   FS_ERRNO_TYPE err = 0;
   FS_STAT_TYPE st;
   int fd = (int)hFile;
   uint64 t;
   FILETIME creationTime, lastAccessTime, lastWriteTime;

   FS_FSTAT(fd, st, err);
   if (err != 0)
    return FALSE;
   t = st.st_ctime * 10000000UL + 116444736000000000ULL;
   creationTime.dwLowDateTime = (DWORD)t;
   creationTime.dwHighDateTime = (DWORD)(t >> 32);
   t = st.st_atime * 10000000UL + 116444736000000000ULL;
   lastAccessTime.dwLowDateTime = (DWORD)t;
   lastAccessTime.dwHighDateTime = (DWORD)(t >> 32);
   t = st.st_mtime * 10000000UL + 116444736000000000ULL;
   lastWriteTime.dwLowDateTime = (DWORD)t;
   lastWriteTime.dwHighDateTime = (DWORD)(t >> 32);
   if (lpCreationTime) *lpCreationTime = creationTime;
   if (lpLastAccessTime) *lpLastAccessTime = lastAccessTime;
   if (lpLastWriteTime) *lpLastWriteTime = lastWriteTime;
   return TRUE;
   }
 */

//////////////////////////////////////////////////////////////////////////
BOOL SetFileAttributes(
  LPCSTR lpFileName,
  DWORD dwFileAttributes)
{
	//TODO: implement
	printf("SetFileAttributes not properly implemented yet, should not matter\n");
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
DWORD GetFileSize(HANDLE hFile, DWORD* lpFileSizeHigh)
{
	FS_ERRNO_TYPE err = 0;
	FS_STAT_TYPE st;
	int fd = (int)hFile;
	DWORD r;

	CryFileSystem::FS_FSTAT(fd, st, err);
	if (err != 0)
		return INVALID_FILE_SIZE;
	if (lpFileSizeHigh)
		*lpFileSizeHigh = (DWORD)(st.st_size >> 32);
	r = (DWORD)st.st_size;
	return r;
}

//////////////////////////////////////////////////////////////////////////
BOOL CloseHandle(HANDLE hObject)
{
	int fd = (int)hObject;

	if (fd != -1)
	{
#if defined(USE_FILE_HANDLE_CACHE)
		CloseFromFileHandleCacheAsFD(fd);
#else
		close(fd);
#endif
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CancelIo(HANDLE hFile)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "CancelIo not implemented yet");
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
HRESULT GetOverlappedResult(HANDLE hFile, void* lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "GetOverlappedResult not implemented yet");
	return 0;
}

//////////////////////////////////////////////////////////////////////////
BOOL ReadFile
(
  HANDLE hFile,
  LPVOID lpBuffer,
  DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,
  LPOVERLAPPED lpOverlapped
)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "ReadFile not implemented yet");
	abort();
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BOOL ReadFileEx
(
  HANDLE hFile,
  LPVOID lpBuffer,
  DWORD nNumberOfBytesToRead,
  LPOVERLAPPED lpOverlapped,
  LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
)
{
	CRY_ASSERT_MESSAGE(0, "ReadFileEx not implemented yet");
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
DWORD SetFilePointer
(
  HANDLE hFile,
  LONG lDistanceToMove,
  PLONG lpDistanceToMoveHigh,
  DWORD dwMoveMethod
)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "SetFilePointer not implemented yet");
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Misc
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
HANDLE CreateEvent
(
  LPSECURITY_ATTRIBUTES lpEventAttributes,
  BOOL bManualReset,
  BOOL bInitialState,
  LPCSTR lpName
)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "CreateEvent not implemented yet");
	return 0;
}

//////////////////////////////////////////////////////////////////////////
DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable)
{
	//TODO: implement
	//	CRY_ASSERT_MESSAGE(0, "SleepEx not implemented yet");
	printf("SleepEx not properly implemented yet\n");
	Sleep(dwMilliseconds);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "WaitForSingleObjectEx not implemented yet");
	return 0;
}

#if 0
//////////////////////////////////////////////////////////////////////////
DWORD WaitForMultipleObjectsEx(
  DWORD nCount,
  const HANDLE* lpHandles,
  BOOL bWaitAll,
  DWORD dwMilliseconds,
  BOOL bAlertable)
{
	//TODO: implement
	return 0;
}
#endif

//////////////////////////////////////////////////////////////////////////
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "WaitForSingleObject not implemented yet");
	return 0;
}

//////////////////////////////////////////////////////////////////////////
BOOL SetEvent(HANDLE hEvent)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "SetEvent not implemented yet");
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BOOL ResetEvent(HANDLE hEvent)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "ResetEvent not implemented yet");
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
HANDLE CreateMutex
(
  LPSECURITY_ATTRIBUTES lpMutexAttributes,
  BOOL bInitialOwner,
  LPCSTR lpName
)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "CreateMutex not implemented yet");
	return 0;
}

//////////////////////////////////////////////////////////////////////////
BOOL ReleaseMutex(HANDLE hMutex)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "ReleaseMutex not implemented yet");
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

typedef DWORD (*              PTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;

//////////////////////////////////////////////////////////////////////////
HANDLE CreateThread
(
  LPSECURITY_ATTRIBUTES lpThreadAttributes,
  SIZE_T dwStackSize,
  LPTHREAD_START_ROUTINE lpStartAddress,
  LPVOID lpParameter,
  DWORD dwCreationFlags,
  LPDWORD lpThreadId
)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "CreateThread not implemented yet");
	return 0;
}

//////////////////////////////////////////////////////////////////////////
#if 0
BOOL SetCurrentDirectory(LPCSTR lpPathName)
{
	char dir[fopenwrapper_basedir_maxsize + 1];
	char* p = fopenwrapper_basedir, * p1;
	char* q, * q0;

	if (*p != '/')
		WrappedF_InitCWD();
	assert(*p == '/');
	++p;
	while (*p && *p != '/')
		++p;
	cry_strcpy(dir, fopenwrapper_basedir);
	q0 = dir + (p - fopenwrapper_basedir);

	/* Normalize the specified path name. */
	const int cLen = strlen(lpPathName);
	assert(cLen < MAX_PATH);
	char path[MAX_PATH];
	cry_strcpy(path, lpPathName);
	p = path;
	for (int i = 0; i < cLen; ++i)
	{
		if (*p == '\\') *p = '/';
		++p;
	}
	p = path;
	if (*p == '/')
	{
		q = q0;
		++p;
	}
	else
		q = q0 + strlen(q0);

	/* Apply the specified path to 'dir'. */
	while (*p)
	{
		for (p1 = p; *p1 && *p1 != '/'; ++p1)
			;
		if (p[0] == '.' && p[1] == '.' && p1 - p == 2)
		{
			if (q == q0)
			{
				/* Can not cd below the root folder, ignore. */
			}
			else
			{
				for (; q >= q0 && *q != '/'; --q)
					;
				assert(*q == '/');
			}
		}
		else if (p[0] == '.' && p1 - p == 1)
		{
			/* Skip. */
		}
		else if (p1 > p)
		{
			if ((q - dir) + (p1 - p) + 2 >= fopenwrapper_basedir_maxsize)
			{
				/* Path too long, fail. */
				return false;
			}
			*q++ = '/';
			memcpy(q, p, p1 - p);
			q += p1 - p1;
			*q = 0;
		}
		p = p1;
		if (*p == '/') ++p;
	}

	cry_strcpy(fopenwrapper_basedir, fopenwrapper_basedir_maxsize, dir);
	return TRUE;
}
#endif //0

//////////////////////////////////////////////////////////////////////////
BOOL DeleteFile(LPCSTR lpFileName)
{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	int err = unlink(lpFileName);
	return (0 == err);
#else
	CRY_ASSERT_MESSAGE(0, "DeleteFile not implemented yet");
	return TRUE;
#endif
}

//////////////////////////////////////////////////////////////////////////
BOOL MoveFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	int err = rename(lpExistingFileName, lpNewFileName);
	return (0 == err);
#else
	CRY_ASSERT_MESSAGE(0, "MoveFile not implemented yet");
#endif
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CopyFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists)
{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	if (bFailIfExists)
	{
		// return false if file already exists
		FILE* fTemp = fopen(lpNewFileName, "rb");
		if (fTemp != NULL)
		{
			fclose(fTemp);
			return FALSE;
		}
	}

	FILE* fFrom = fopen(lpExistingFileName, "rb");
	if (NULL == fFrom)
		return FALSE;

	FILE* fTo = fopen(lpNewFileName, "wb");
	if (NULL == fTo)
	{
		fclose(fFrom);
		return FALSE;
	}

	#define COPY_FILE_BUF_SIZE 1024
	char buf[COPY_FILE_BUF_SIZE];
	size_t lenRead;
	size_t lenWrite;

	while (!feof(fFrom))
	{
		lenRead = fread(buf, sizeof(char), COPY_FILE_BUF_SIZE, fFrom);
		lenWrite = fwrite(buf, sizeof(char), lenRead, fTo);
		assert(lenWrite == lenRead);
	}

	fclose(fFrom);
	fclose(fTo);

	return TRUE;

#else
	CRY_ASSERT_MESSAGE(0, "CopyFile not implemented yet");
	return FALSE;
#endif
}

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
BOOL GetComputerName(LPSTR lpBuffer, LPDWORD lpnSize)
{
	if (!lpBuffer || !lpnSize)
		return FALSE;

	int err = gethostname(lpBuffer, *lpnSize);

	if (-1 == err)
	{
		CryLog("GetComputerName falied [%d]\n", errno);
		return FALSE;
	}
	return TRUE;
}

DWORD GetCurrentProcessId(void)
{
	return (DWORD)getpid();
}
#endif

//////////////////////////////////////////////////////////////////////////
BOOL RemoveDirectory(LPCSTR lpPathName)
{
	CRY_ASSERT_MESSAGE(0, "RemoveDirectory not implemented yet");
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
bool CryCreateDirectory(const char* lpPathName)
{
	struct stat st_info;
#if CRY_PLATFORM_ORBIS
	char buf[512];
	lpPathName = ConvertFileName(buf, sizeof(buf), lpPathName);
#endif
	if (stat(lpPathName, &st_info) == 0)
	{
		if (!S_ISDIR(st_info.st_mode))
		{
			CryLog("%s failed: '%s' already exists and is not a directory", __FUNC__, lpPathName);
			return false;
		}
		return true;
	}

	const int res = CryFileSystem::FS_MKDIR(lpPathName, S_IRWXU);

	if (res != 0)
	{
		CryLog("%s: mkdir('%s') failed", __FUNC__, lpPathName);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CryDirectoryExists(const char* szPath)
{
	struct stat st_info;
	return (stat(szPath, &st_info) == 0 && S_ISDIR(st_info.st_mode));
}

//////////////////////////////////////////////////////////////////////////
DWORD GetCurrentDirectory(DWORD nBufferLength, char* lpBuffer)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CryGetCurrentDirectory(unsigned int nBufferLength, char* lpBuffer)
{
	if (nBufferLength > 0 && lpBuffer)
	{
		*lpBuffer = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
int CryGetWritableDirectory(unsigned int nBufferLength, char* lpBuffer)
{
	if (nBufferLength == 0)
		return 0;
#if CRY_PLATFORM_IOS
	return AppleGetUserLibraryDirectory(lpBuffer, nBufferLength);
#elif CRY_PLATFORM_ANDROID
	const char* path = CryGetUserStoragePath();
	const size_t len = strlen(path);
	if (len + 1 > nBufferLength)
		return 0;
	strcpy(lpBuffer, path);
	return len;
#else
	return 0;
#endif
}

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
//////////////////////////////////////////////////////////////////////////
void CrySetCurrentWorkingDirectory(const char* szWorkingDirectory)
{
	if (chdir(szWorkingDirectory) == -1)
	{
		CryFatalError("Unexpected error while trying to set current working directory to: %s", szWorkingDirectory);
	}
}

//////////////////////////////////////////////////////////////////////////
void CryGetExecutableFolder(unsigned int pathSize, char* szPath)
{
	size_t nLen = readlink("/proc/self/exe", szPath, pathSize);
	if (nLen <= 0)
	{
		CryFatalError("Unexpected error encountered trying to get executable path. readlink(\"/proc/self/exe\") failed.");
	}
	if (char* strEnd = strrchr(szPath, '/'))
	{
		strEnd[1] = '\0';
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
short CryGetAsyncKeyState(int vKey)
{
	//TODO: implement
	CRY_ASSERT_MESSAGE(0, "CryGetAsyncKeyState not implemented yet");
	return 0;
}

//////////////////////////////////////////////////////////////////////////
uint32 CryGetFileAttributes(const char* lpFileName)
{
#if CRY_PLATFORM_ORBIS
	char buf[512];
	const char* buffer = ConvertFileName(buf, sizeof(buf), lpFileName);
#else
	string fn = lpFileName;
	adaptFilenameToLinux(fn);
	const char* buffer = fn.c_str();
#endif
	//test if it is a file, a directory or does not exist
	FS_ERRNO_TYPE fsErr = 0;
	int fd = -1;
	CryFileSystem::FS_OPEN(buffer, FS_O_RDONLY, fd, 0, fsErr);
	if (!fsErr)
	{
		CryFileSystem::FS_CLOSE_NOERR(fd);
		return FILE_ATTRIBUTE_NORMAL;
	}
	return (fsErr == FS_EISDIR) ? FILE_ATTRIBUTE_DIRECTORY : INVALID_FILE_ATTRIBUTES;
}

//////////////////////////////////////////////////////////////////////////
bool CrySetFileAttributes(const char* lpFileName, uint32 dwFileAttributes)
{
	//TODO: implement
	printf("CrySetFileAttributes not properly implemented yet\n");
	return false;
}

int file_counter = 0;
long file_op_counter = 0;
long file_op_break = -1;

static void WrappedF_Break(long op_counter)
{
	printf("WrappedF_Break(op_counter = %li)\n", op_counter);
}

#if defined(USE_FILE_HANDLE_CACHE)
// undef the define overloads
	#undef fopen
	#undef stat
	#undef fclose
	#undef fseek
	#undef ftell
	#undef fread
	#undef fileno
	#undef fwrite
	#undef vfprintf
	#undef fgets
	#undef feof
	#undef ferror
	#undef getc
	#undef ungetc
	#undef fputs
	#undef fflush
	#undef fprintf
	#undef fscanf
	#undef vfscanf
	#undef fstat
	#undef fgetc

// LRU cache for filehandles
//we cache them, reuse older filedescriptors, and close
// files in case we ran out of file descriptors
// to prevent problems with streaming/direct file access, these get their LRU
// value modified, so that they have a higher priority
class CFileHandleCache
{
	enum { nFILEHANDLESALT = 0xFF000000 };  // special salt to build FILE* look alikes

	enum { nFILEENTRIES = 256 };            // number of supported entries in the cache
	#if CRY_PLATFORM_ORBIS
	enum { nMAXOPENFILES = FOPEN_MAX - 18 };  // maximum of simultaneous opened files (minus 3 as includes stdout, stderr and stdin minus 15 for miscellaneous other files the OS seems to have open)
	#else
	enum { nMAXOPENFILES = FOPEN_MAX - 3 }; // maximum of simultaneous opened files (minus 3 as includes stdout, stderr and stdin)
	#endif

	enum { nFILEIOBUFFERSIZE = 32 * 1024 };    // size of the buffer used for file io

public:

	static CFileHandleCache& Instance()
	{
		static CFileHandleCache singleton;
		return singleton;
	}

	~CFileHandleCache() {}

	// wrapped std file interface
	inline FILE*  FOpen(const char* name, const char* mode, FileIoWrapper::FileAccessType type = FileIoWrapper::GENERAL);
	inline int    FClose(FILE* pHandle);
	inline size_t Fileno(FILE* pHandle);
	inline size_t FRead(void* ptr, size_t size, size_t count, FILE* pHandle);
	inline size_t FWrite(const void* ptr, size_t size, size_t count, FILE* stream);
	inline size_t FTell(FILE* pHandle);
	inline size_t FSeek(FILE* pHandle, size_t offset, size_t origin);
	inline int    VFprintf(FILE* stream, const char* format, va_list arg);
	inline char*  FGets(char* str, int num, FILE* stream);
	inline int    Feof(FILE* stream);
	inline int    Ferror(FILE* stream);
	inline int    Getc(FILE* stream);
	inline int    Ungetc(int character, FILE* stream);
	inline int    Fputs(const char* str, FILE* stream);
	inline int    Fflush(FILE* stream);
	inline int    VFscanf(FILE* stream, const char* format, va_list arg);
	inline int    Fgetc(FILE* stream);

	// wrapper utils for cellfs functions(they work on fds)
	void OpenFromFileHandleCacheAsFD(const char* adjustedFilename, int flags, int& fd, int& err);
	void CloseFromFileHandleCacheAsFD(int fd);

	// util functions
	void DumpCacheContent();
private:
	CFileHandleCache();

	void SetDiscIsEjectedFlag(FILE* pHandle)
	{
		uint32 nCachePos = GetCachePos(pHandle);
		SCacheEntry& rEntry = m_Cache[nCachePos];
		rEntry.m_bDiscIsEjected = true;
	}

	// md5 key for fast search of open files
	typedef union
	{
		unsigned char c16[16];
		uint64        u64[2];
	} TPakNameKey;

	// entry holding all file handle informations per file for the cache
	struct SCacheEntry
	{
		enum States
		{
			Open,                     // file handle is open and in use
			Cached,                   // file not in use, but file descriptor still open for re-use
			Parked,                   // file in use, but file descriptor closed
			Closed
		};                          // File and file descriptor closed

		// members for LRU-cache
		uint32      m_lru;            //lru counter last set op
		TPakNameKey m_pakNameKey;     //md5 of file name (full path)

		// FILE informations
		FILE*                         m_pHandle;            //cached pak file handle
		char                          m_filename[MAX_PATH]; // file-name, in case the file needs to be re-opened
		char                          m_mode[16];           // access mode used for file io
		uint32                        m_nSeekPos;           // store seek position, in case files needs to be reopened
		States                        m_state;              // state of the cache entrie
		FileIoWrapper::FileAccessType m_type;               // type of access to this file
	#if defined(FILE_HANDLE_CACHE_USE_FILE_IO_BUFFER)
		char*                         m_fileIoBuf; // the allocated file io buffer, only valid when Open or Cached
	#endif
		bool                          m_bDiscIsEjected; // remember per file if the disc was ejected

		SCacheEntry();
	};

	uint32       GetCachePos(FILE* pHandle) const      { return (uint32)((TRUNCATE_PTR)pHandle) & ~nFILEHANDLESALT; }
	FILE*        GetFileHandle(uint32 nCachePos) const { return (FILE*)(EXPAND_PTR)((uint32)nCachePos | nFILEHANDLESALT); }

	FILE*        GetRealFileHandle(FILE* pHandle);

	inline void  CloseFile(uint32 nCachePos);
	inline FILE* OpenFile(uint32 nCachePos, const char* sName, const char* op, FileIoWrapper::FileAccessType type);
	inline void  ReOpenFile(uint32 nCachePos);

	void         ActivateEntry(uint32 nCachePos);
	void         FreeUpFD(uint32 nCachePosKeep);

	uint32       FindOpenSlot();

	char*        AllocateFileIoBuffer()      { char* buf = m_FileIoBufferPtrs.back(); m_FileIoBufferPtrs.pop_back(); return buf; }
	void         FreeFileIoBuffer(char* buf) { m_FileIoBufferPtrs.push_back(buf); }

	typedef std::map<int, FILE*> FDMap;

	uint32             m_LRU;               //current lru counter to replace slots properly
	uint32             m_nFileCount;        // number of open files
	FDMap              m_FdToFileMap;       // map to find FILE* from fd used by cellfsio
	CryCriticalSection m_Lock;              // Lock to protected the cache against parallel access
	CryCriticalSection m_mapLock;           // Lock to protect the FILE*->fd map from parallel access
	std::vector<char*> m_FileIoBufferPtrs;  // nMAXOPENFILES pointers into m_FileIoBuffer, wrapped in a vector for convenience

	SCacheEntry        m_Cache[nFILEENTRIES];                            //file handle cache
	char               m_FileIoBuffer[nMAXOPENFILES][nFILEIOBUFFERSIZE]; // static buffer for all file ios
};

CFileHandleCache::CFileHandleCache() :
	m_LRU(0),
	m_nFileCount(0)
{
	m_FileIoBufferPtrs.reserve(nMAXOPENFILES);

	for (uint32 i = 0; i < nMAXOPENFILES; i++)
	{
		m_FileIoBufferPtrs.push_back(&m_FileIoBuffer[i][0]);
	}
}

CFileHandleCache::SCacheEntry::SCacheEntry() :
	m_pHandle(NULL),
	m_lru(0),
	m_nSeekPos(0),
	m_state(Closed),
	m_type(FileIoWrapper::GENERAL),
	#if defined(FILE_HANDLE_CACHE_USE_FILE_IO_BUFFER)
	m_fileIoBuf(NULL)
	#endif
{
	m_pakNameKey.u64[0] = 0;
	m_pakNameKey.u64[1] = 0;
	m_filename[0] = '\0';
	m_mode[0] = '\0';
}

//retrieve cache one if existing, otherwise open new one
FILE* CFileHandleCache::FOpen(const char* name, const char* mode, FileIoWrapper::FileAccessType type)
{
	AUTO_LOCK_T(CryCriticalSection, m_Lock);
	// create md5 hash as a search key
	TPakNameKey key;
	MD5Context context;
	MD5Init(&context);
	MD5Update(&context, (unsigned char*)name, strlen(name));
	MD5Final(key.c16, &context);

	// check for a matching closed file
	for (uint32 nCachePos = 0; nCachePos < nFILEENTRIES; ++nCachePos)
	{
		SCacheEntry& rEntry = m_Cache[nCachePos];

		// try to find a cached handle for this file
		if (rEntry.m_state == SCacheEntry::Cached &&
		    rEntry.m_pakNameKey.u64[0] == key.u64[0] && rEntry.m_pakNameKey.u64[1] == key.u64[1])
		{
			// if different mode then, close it first. We never want 2 files open with different modes
			if (strcmp(rEntry.m_mode, mode) == 0)
			{
				ActivateEntry(nCachePos);
				return GetFileHandle(nCachePos);
			}
			else
			{
				CloseFile(nCachePos);
				break;
			}
		}

	}
	uint32 nOpenSlot = FindOpenSlot();
	return OpenFile(nOpenSlot, name, mode, type);
}

uint32 CFileHandleCache::FindOpenSlot()
{
	uint32 curLowestCachedSlot = 0xFFFFFFFF;    //invalid slot id
	uint32 curLowestCachedSlotLRU = 0xFFFFFFFF;
	uint32 curLowestClosedSlot = 0xFFFFFFFF;    // invalid slot id
	uint32 curLowestClosedSlotLRU = 0xFFFFFFFF;

	// find slot where we could close an fd
	for (uint32 nCachePos = 0; nCachePos < nFILEENTRIES; ++nCachePos)
	{
		SCacheEntry& rEntry = m_Cache[nCachePos];

		if (rEntry.m_state == SCacheEntry::Cached && rEntry.m_lru < curLowestCachedSlotLRU)
		{
			curLowestCachedSlotLRU = rEntry.m_lru;
			curLowestCachedSlot = nCachePos;
		}

		if (rEntry.m_state == SCacheEntry::Closed && rEntry.m_lru < curLowestClosedSlotLRU)
		{
			curLowestClosedSlotLRU = rEntry.m_lru;
			curLowestClosedSlot = nCachePos;
		}
	}

	if (curLowestClosedSlot != 0xFFFFFFFF) return curLowestClosedSlot;
	if (curLowestCachedSlot != 0xFFFFFFFF) return curLowestCachedSlot;

	gEnv->pLog->LogError("[FileHandleCache] Could not find a slot in state Cache or Closed to reused");
	DumpCacheContent();
	CryDebugBreak();
	return -1;
}

int CFileHandleCache::FClose(FILE* pHandle)
{
	AUTO_LOCK_T(CryCriticalSection, m_Lock);
	uint32 nCachePos = GetCachePos(pHandle);

	SCacheEntry& rEntry = m_Cache[nCachePos];
	switch (rEntry.m_state)
	{
	case SCacheEntry::Open:
		rEntry.m_state = SCacheEntry::Cached;
		if (strchr(rEntry.m_mode, 'w') == NULL)
		{
			CryFileSystem::fflush(rEntry.m_pHandle);
		}
		else
		{
			// fully close the file if it was opened for writing
			CloseFile(nCachePos);
		}
		break;
	case SCacheEntry::Parked:
		rEntry.m_state = SCacheEntry::Closed;
		rEntry.m_nSeekPos = 0;
		rEntry.m_filename[0] = '\0';
		rEntry.m_bDiscIsEjected = false;
		break;
	default:
		gEnv->pLog->LogError("[FileHandleCache] Tried to close a file not in state Open or Parked, nCachePos = %d", nCachePos);
		DumpCacheContent();
		CryDebugBreak();
	}

	return 0;
}

FILE* CFileHandleCache::GetRealFileHandle(FILE* pHandle)
{
	// handle stdout and stderr :/
	IF (pHandle == stdout || pHandle == stderr, 0)
		return pHandle;

	AUTO_LOCK_T(CryCriticalSection, m_Lock);
	uint32 nCachePos = GetCachePos(pHandle);

	ActivateEntry(nCachePos);
	SCacheEntry& rEntry = m_Cache[nCachePos];

	if (rEntry.m_bDiscIsEjected)
		return NULL;

	return rEntry.m_pHandle;
}

size_t CFileHandleCache::Fileno(FILE* pHandle)
{
	#if CRY_PLATFORM_ORBIS
	ORBIS_TO_IMPLEMENT; // Broken as of 0.915 SDK
	return 0;
	#else
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset errno
	size_t nRes = pRealHandle ? ::fileno(pRealHandle) : 0;
	return nRes;
	#endif
}

size_t CFileHandleCache::FRead(void* ptr, size_t size, size_t count, FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset errno
	return pRealHandle ? CryFileSystem::fread(ptr, size, count, pRealHandle) : 0;
}

size_t CFileHandleCache::FWrite(const void* ptr, size_t size, size_t count, FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset errno
	return pRealHandle ? CryFileSystem::fwrite(ptr, size, count, pRealHandle) : 0;
}

size_t CFileHandleCache::FTell(FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset errno
	return pRealHandle ? CryFileSystem::ftell(pRealHandle) : (size_t)(-1L);
}

size_t CFileHandleCache::FSeek(FILE* pHandle, size_t offset, size_t origin)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset errno
	return pRealHandle ? CryFileSystem::fseek(pRealHandle, offset, origin) : (size_t)(-1L);
}

int CFileHandleCache::VFprintf(FILE* pHandle, const char* format, va_list arg)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset errno
	return pRealHandle ? CryFileSystem::vfprintf(pRealHandle, format, arg) : -1;

}

int CFileHandleCache::VFscanf(FILE* pHandle, const char* format, va_list arg)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset ernno
	return pRealHandle ? CryFileSystem::vfscanf(pRealHandle, format, arg) : -1;
}

int CFileHandleCache::Fgetc(FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset ernno
	return pRealHandle ? CryFileSystem::fgetc(pRealHandle, format, arg) : -1;
}

char* CFileHandleCache::FGets(char* str, int num, FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset ernno
	pRealHandle ? CryFileSystem::fgets(str, num, pRealHandle) : NULL;
}

int CFileHandleCache::Feof(FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset ernno
	return pRealHandle ? CryFileSystem::feof(pRealHandle) : 0;
}

int CFileHandleCache::Ferror(FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset ernno
	return pRealHandle ? CryFileSystem::ferror(pRealHandle) : 0;
}

int CFileHandleCache::Getc(FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset ernno
	return pRealHandle ? CryFileSystem::getc(pRealHandle) : EOF;
}

int CFileHandleCache::Ungetc(int character, FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset ernno
	return pRealHandle ? CryFileSystem::ungetc(character, pRealHandle) : EOF;
}

int CFileHandleCache::Fputs(const char* str, FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset ernno
	return pRealHandle ? CryFileSystem::fputs(str, pRealHandle) : EOF;
}

int CFileHandleCache::Fflush(FILE* pHandle)
{
	FILE* pRealHandle = GetRealFileHandle(pHandle);
	errno = 0; // reset ernno
	return pRealHandle ? CryFileSystem::fflush(pRealHandle) : EOF;
}

// === cellfs wrapper utils
void CFileHandleCache::OpenFromFileHandleCacheAsFD(const char* adjustedFilename, int flags, int& fd, int& err)
{
	AUTO_LOCK_T(CryCriticalSection, m_mapLock);

	FILE* f = FOpen(adjustedFilename, "rb", FileIoWrapper::STREAMING);
	if (f)
	{
		fd = Fileno(f);
		m_FdToFileMap.insert(std::make_pair(fd, f));
		err = 0;
	}
	else
	{
		fd = -1;
		err = errno;
	}
}

void CFileHandleCache::CloseFromFileHandleCacheAsFD(int fd)
{
	AUTO_LOCK_T(CryCriticalSection, m_mapLock);

	FDMap::iterator it = m_FdToFileMap.find(fd);
	if (it != m_FdToFileMap.end())
	{
		FILE* f = it->second;
		FClose(f);
		m_FdToFileMap.erase(it);
		return;
	}

	gEnv->pLog->LogError("[FileHandleCache] Tried to close invalid FileDescriptor %d", fd);
}

// === intern FILEHandleCache functions
void CFileHandleCache::ActivateEntry(uint32 nCachePos)
{
	// make sure we can really open the file
	if (m_nFileCount == nMAXOPENFILES)
	{
		FreeUpFD(nCachePos);
	}

	SCacheEntry& rEntry = m_Cache[nCachePos];

	rEntry.m_lru = ++m_LRU;
	if (m_LRU == 0)
	{
		gEnv->pLog->LogError("[FileHandleCache] LRU OverFlow!");
		CryDebugBreak();
	}

	switch (rEntry.m_state)
	{
	case SCacheEntry::Open:
		return;
	case SCacheEntry::Cached:
		CryFileSystem::fseek(rEntry.m_pHandle, 0, SEEK_SET);
		rEntry.m_state = SCacheEntry::Open;
		rEntry.m_bDiscIsEjected = false;
		return;
	case SCacheEntry::Parked:
		ReOpenFile(nCachePos);
		return;
	case SCacheEntry::Closed:
		gEnv->pLog->LogError("[FileHandleCache Error] Tried to activate a closed entry, nCachePos = %d", nCachePos);
		DumpCacheContent();
		CryDebugBreak();
		return;
	}

	gEnv->pLog->LogError("[FileHandleCache] Data corruption!");
	DumpCacheContent();
	CryDebugBreak();
}

void CFileHandleCache::FreeUpFD(uint32 nCachePosKeep)
{

	uint32 curLowestCachedSlot = 0xFFFFFFFF;    //invalid slot id
	uint32 curLowestCachedSlotLRU = 0xFFFFFFFF;
	uint32 curLowestOpenSlot = 0xFFFFFFFF;    // invalid slot id
	uint32 curLowestOpenSlotLRU = 0xFFFFFFFF;

	// find slot where we could close an fd
	for (uint32 nCachePos = 0; nCachePos < nFILEENTRIES; ++nCachePos)
	{
		// don't free the slot we want to use
		if (nCachePos == nCachePosKeep)
			continue;

		SCacheEntry& rEntry = m_Cache[nCachePos];

		if (rEntry.m_state == SCacheEntry::Cached && rEntry.m_lru < curLowestCachedSlotLRU)
		{
			curLowestCachedSlotLRU = rEntry.m_lru;
			curLowestCachedSlot = nCachePos;
		}

		if (rEntry.m_state == SCacheEntry::Open && rEntry.m_lru < curLowestOpenSlotLRU)
		{
			curLowestOpenSlotLRU = rEntry.m_lru;
			curLowestOpenSlot = nCachePos;
		}
	}

	// first close Cached files
	if (curLowestCachedSlot != 0xFFFFFFFF)
	{
		CloseFile(curLowestCachedSlot);
		return;
	}

	// we we didn't find a cached one, park an opened one
	if (curLowestOpenSlot != 0xFFFFFFFF)
	{
		CloseFile(curLowestOpenSlot);
		return;
	}

	gEnv->pLog->LogError("[FileHandleCache] Could not find a slot in state Cached or Open to close");
	DumpCacheContent();
	CryDebugBreak();
}

void CFileHandleCache::CloseFile(uint32 nCachePos)
{
	SCacheEntry& rEntry = m_Cache[nCachePos];

	switch (rEntry.m_state)
	{
	case SCacheEntry::Cached:
		CryFileSystem::fclose(rEntry.m_pHandle);
		rEntry.m_pHandle = NULL;
		rEntry.m_filename[0] = '\0';
		rEntry.m_state = SCacheEntry::Closed;
		rEntry.m_nSeekPos = 0;
		rEntry.m_type = FileIoWrapper::GENERAL;
	#if defined(FILE_HANDLE_CACHE_USE_FILE_IO_BUFFER)
		FreeFileIoBuffer(rEntry.m_fileIoBuf);
		rEntry.m_fileIoBuf = NULL;
	#endif
		rEntry.m_bDiscIsEjected = false;
		memset(&rEntry.m_pakNameKey, 0, sizeof(rEntry.m_pakNameKey));
		break;
	case SCacheEntry::Open:
		CryFileSystem::ftell(rEntry.m_pHandle);
		CryFileSystem::fclose(rEntry.m_pHandle);
		rEntry.m_pHandle = NULL;
		rEntry.m_state = SCacheEntry::Parked;
	#if defined(FILE_HANDLE_CACHE_USE_FILE_IO_BUFFER)
		FreeFileIoBuffer(rEntry.m_fileIoBuf);
		rEntry.m_fileIoBuf = NULL;
	#endif
		rEntry.m_bDiscIsEjected = false;
		break;
	default:
		gEnv->pLog->LogError("[FileHandleCache] Tried to close a file not in state Cached or Opened, nCachePos = %d", nCachePos);
		DumpCacheContent();
		CryDebugBreak();
	}

	m_nFileCount -= 1;
}

FILE* CFileHandleCache::OpenFile(uint32 nCachePos, const char* sName, const char* op, FileIoWrapper::FileAccessType type)
{
	SCacheEntry& rEntry = m_Cache[nCachePos];

	// in case we are opening a non cached file, make sure we can open it
	if (m_nFileCount == nMAXOPENFILES && rEntry.m_state != SCacheEntry::Cached)
	{
		FreeUpFD(nCachePos);
	}

	// if we use a cached file, close it before
	if (rEntry.m_state == SCacheEntry::Cached)
	{
		CloseFile(nCachePos);
	}

	rEntry.m_pHandle = CryFileSystem::fopen(sName, op);

	/*if ( errno == EMFILE )
	   {
	   gEnv->pLog->LogError("[FileHandleCache] Opened more than 31 files on gamedata");
	   DumpCacheContent();
	   CryDebugBreak();
	   }*/

	if (rEntry.m_pHandle == NULL)
		return NULL;

	rEntry.m_bDiscIsEjected = false;
	#if defined(FILE_HANDLE_CACHE_USE_FILE_IO_BUFFER)
	rEntry.m_fileIoBuf = AllocateFileIoBuffer();
	setvbuf(rEntry.m_pHandle, rEntry.m_fileIoBuf, _IOFBF, nFILEIOBUFFERSIZE);
	#endif

	rEntry.m_state = SCacheEntry::Open;
	cry_strcpy(rEntry.m_filename, sName);

	stack_string reopenOp = op;
	reopenOp.replace("wb+", "rb+");
	reopenOp.replace("wt+", "rt+");
	reopenOp.replace("w+", "r+");
	reopenOp.replace("w", "r+");
	cry_strcpy(rEntry.m_mode, reopenOp.c_str());

	rEntry.m_type = type;
	// create md5 hash as a search key
	TPakNameKey key;
	MD5Context context;
	MD5Init(&context);
	MD5Update(&context, (unsigned char*)sName, strlen(sName));
	MD5Final(rEntry.m_pakNameKey.c16, &context);

	rEntry.m_lru = ++m_LRU;
	rEntry.m_lru |= type;

	if (m_LRU == 0)
	{
		gEnv->pLog->LogError("[FileHandleCache] LRU Overflow!");
		CryDebugBreak();
	}

	m_nFileCount += 1;

	return GetFileHandle(nCachePos);
}

void CFileHandleCache::ReOpenFile(uint32 nCachePos)
{
	SCacheEntry& rEntry = m_Cache[nCachePos];
	CryStackStringT<char, sizeof(rEntry.m_mode)> mode = rEntry.m_mode;
	rEntry.m_pHandle = CryFileSystem::fopen(rEntry.m_filename, mode.c_str());
	if (errno == EMFILE)
	{
		gEnv->pLog->LogError("[FileHandleCache] Opened more than 31 files on gamedata");
		DumpCacheContent();
		CryDebugBreak();
		return;
	}

	if (rEntry.m_pHandle == NULL)
	{
		// if we failed to open a file which was open before, assume a disc ejection
		rEntry.m_bDiscIsEjected = true;
		return;
	}

	#if defined(FILE_HANDLE_CACHE_USE_FILE_IO_BUFFER)
	rEntry.m_fileIoBuf = AllocateFileIoBuffer();
	setvbuf(rEntry.m_pHandle, rEntry.m_fileIoBuf, _IOFBF, nFILEIOBUFFERSIZE);
	#endif

	CryFileSystem::fseek(rEntry.m_pHandle, rEntry.m_nSeekPos, SEEK_SET)
	rEntry.m_state = SCacheEntry::Open;
	rEntry.m_nSeekPos = 0;
	rEntry.m_bDiscIsEjected = false;
	m_nFileCount += 1;
}

void CFileHandleCache::DumpCacheContent()
{
	uint32 nOpen = 0;
	uint32 nClosed = 0;
	uint32 nParked = 0;
	uint32 nCached = 0;

	gEnv->pLog->Log("\n==== FileDescriptor Cache Dump ===\n");
	for (uint32 nCachePos = 0; nCachePos < nFILEENTRIES; ++nCachePos)
	{
		SCacheEntry& rEntry = m_Cache[nCachePos];
		char buffer[1024] = { 0 };
		char* pDst = &buffer[0];
		pDst += sprintf(pDst, "Pos %d.\tName \"%s\"\t n, State: ", nCachePos, rEntry.m_filename);

		switch (rEntry.m_state)
		{
		case SCacheEntry::Open:
			pDst += sprintf(pDst, "Open");
			nOpen++;
			break;
		case SCacheEntry::Closed:
			pDst += sprintf(pDst, "Closed");
			nClosed++;
			break;
		case SCacheEntry::Parked:
			pDst += sprintf(pDst, "Parked");
			nParked++;
			break;
		case SCacheEntry::Cached:
			pDst += sprintf(pDst, "Cached");
			nCached++;
			break;
		}
		pDst += sprintf(pDst, "\tType: ");
		if (rEntry.m_type & FileIoWrapper::DIRECT)
			pDst += sprintf(pDst, "Direct");
		else if (rEntry.m_type & FileIoWrapper::STREAMING)
			pDst += sprintf(pDst, "Streaming");
		else
			pDst += sprintf(pDst, "General");

		pDst += sprintf(pDst, "\tLRU %d", rEntry.m_lru);
		gEnv->pLog->Log(buffer);
	}

	gEnv->pLog->Log("FileDescriptors open %d (Open %d, Closed %d, Parked %d, Cached %d)", m_nFileCount, nOpen, nClosed, nParked, nCached);
}
void LogOpenFiles(struct IConsoleCmdArgs*)
{
	CFileHandleCache::Instance().DumpCacheContent();
}

void OpenFromFileHandleCacheAsFD(const char* adjustedFilename, int flags, int& fd, int dummy, int& err)
{
	CFileHandleCache::Instance().OpenFromFileHandleCacheAsFD(adjustedFilename, flags, fd, err);
}

void CloseFromFileHandleCacheAsFD(int fd)
{
	CFileHandleCache::Instance().CloseFromFileHandleCacheAsFD(fd);
}

namespace std
{
// ==== file io wrapper ==== //
	#if defined(USE_FILE_HANDLE_CACHE)
// non-file handle cache versions are below
FILE* WrappedFopen(const char* __restrict filename, const char* __restrict mode, FileIoWrapper::FileAccessType type, bool bSysAppHome)
{
	char buf[512];

		#if CRY_PLATFORM_ORBIS
	FILE* fp = CFileHandleCache::Instance().FOpen(ConvertFileName(buf, sizeof(buf), filename), (const char*)mode, type);
		#else
	FILE* fp = CFileHandleCache::Instance().FOpen(filename, (const char*)mode, type);
		#endif
	return fp;
}

int WrappedStat(const char* pFilename, struct stat* pStat)
{
	if (strchr(pFilename, '*'))
	{
		return -1; // Wildcards should always fail a 'stat'
	}
	char buf[512];

		#if CRY_PLATFORM_ORBIS
	const char* pTrueFilename = ConvertFileName(buf, sizeof(buf), pFilename);
		#else
	const char* pTrueFilename = pFilename;
		#endif
	return CryFileSystem::FS_STAT(pTrueFilename, pStat)
}

int WrappedFStat(int files, struct stat* pStat)
{
	return CryFileSystem::FS_STAT(files, pStat);
}

int WrappedFclose(FILE* fp)
{
	return CFileHandleCache::Instance().FClose(fp);
}

size_t WrappedFileno(FILE* fp)
{
	return CFileHandleCache::Instance().Fileno(fp);
}

size_t WrappedFtell(FILE* pHandle)
{
	return CFileHandleCache::Instance().FTell(pHandle);
}

size_t WrappedFSeek(FILE* pHandle, size_t offset, size_t origin)
{
	return CFileHandleCache::Instance().FSeek(pHandle, offset, origin);
}

size_t WrappedFRead(void* ptr, size_t size, size_t count, FILE* pHandle)
{
	return CFileHandleCache::Instance().FRead(ptr, size, count, pHandle);
}

size_t WrappedFWrite(const void* ptr, size_t size, size_t count, FILE* pHandle)
{
	return CFileHandleCache::Instance().FWrite(ptr, size, count, pHandle);
}

int WrappedVFprintf(FILE* stream, const char* format, va_list arg)
{
	return CFileHandleCache::Instance().VFprintf(stream, format, arg);
}

char* WrappedFGets(char* str, int num, FILE* stream)
{
	return CFileHandleCache::Instance().FGets(str, num, stream);
}

int WrappedFeof(FILE* stream)
{
	return CFileHandleCache::Instance().Feof(stream);
}

int WrappedFerror(FILE* stream)
{
	return CFileHandleCache::Instance().Ferror(stream);
}

int WrappedGetc(FILE* stream)
{
	return CFileHandleCache::Instance().Getc(stream);
}

int WrappedUngetc(int character, FILE* stream)
{
	return CFileHandleCache::Instance().Ungetc(character, stream);
}

int WrappedFputs(const char* str, FILE* stream)
{
	return CFileHandleCache::Instance().Fputs(str, stream);
}

int WrappedFflush(FILE* stream)
{
	return CFileHandleCache::Instance().Fflush(stream);
}

int WrappedFprintf(FILE* stream, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = CFileHandleCache::Instance().VFprintf(stream, format, args);
	va_end(args);
	return ret;
}

int WrappedVFscanf(FILE* stream, const char* format, va_list arg)
{
	return CFileHandleCache::Instance().VFscanf(stream, format, arg);
}

int WrappedFgetc(FILE* stream)
{
	return CFileHandleCache::Instance().Fgetc(stream);
}

int WrappedFscanf(FILE* stream, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = CFileHandleCache::Instance().VFscanf(stream, format, args);
	va_end(args);
	return ret;
}

// ==== non file handle cache fopen/fclose wrapper
	#else

extern "C" FILE * WrappedFopen(const char* __restrict filename, const char* __restrict mode)
{
	bool isWrite = false;
	bool skipOpen = false;
	char buffer[MAX_PATH + 1];
	if (fopenwrapper_basedir[0] != '/')
		WrappedF_InitCWD();

	++file_op_counter;
	if (file_op_counter == file_op_break)
		WrappedF_Break(file_op_counter);

	const bool cContainsWrongSlash = strchr(filename, '\\');
	if ((cContainsWrongSlash || filename[0] != '/'))
	{
		char* bp = buffer, * const buffer_end = buffer + sizeof buffer;
		buffer_end[-1] = 0;
		if (filename[0] != '/')
		{
			strncpy(bp, fopenwrapper_basedir, buffer_end - bp - 1);
			bp += strlen(bp);
			if (bp > buffer && bp[-1] != '/' && bp < buffer_end - 2)
				*bp++ = '/';
		}
		strncpy(bp, filename, buffer_end - bp - 1);
		//replace '\\' by '/' and lower it
		const int cLen = strlen(buffer);
		for (int i = 0; i < cLen; ++i)
		{
			buffer[i] = tolower(buffer[i]);
			if (buffer[i] == '\\')
				buffer[i] = '/';
		}
		filename = buffer;
	}
	else if (cContainsWrongSlash)
	{
		const int cLen = strlen(filename);
		for (int i = 0; i < cLen; ++i)
		{
			buffer[i] = filename[i];
			if (buffer[i] == '\\')
				buffer[i] = '/';
		}
		filename = buffer;
	}
	// Note: "r+" is not considered to be a write-open, since fopen() will
	// fail if the specified file does not exist.
	if (strchr(mode, 'w') || strchr(mode, 'a')) isWrite = true;
		#if CRY_PLATFORM_ORBIS
	const bool skipInitial = true;
		#else
	const bool skipInitial = false;
		#endif

	bool failOpen = false;

	FILE* fp = 0;

	if (failOpen)
	{
		fp = NULL;
		errno = ENOENT;
	}
	else if (!skipOpen)
		fp = fopen(filename, mode);

	if (fp)
		++file_counter;

	return fp;
}

extern "C" int WrappedFclose(FILE* fp)
{
	const bool error = false;
	int err = 0;
	if (!error)
	{
		err = fclose(fp);
	}
	if (err == 0)
	{
		if (!error) --file_counter;
	}
	else
	{
		printf("WrappedFclose: fclose() failed: %s [%i]\n", strerror(err), err);
		WrappedF_Break(file_op_counter);
	}
	return err;
}
	#endif
} // namespace std

#endif

#if (CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE) && !defined(OutputDebugString)
// WinAPI debug functions.
DLL_EXPORT void OutputDebugString(const char* outputString)
{
	#if _DEBUG
	// There is no such thing as a debug console on XCode
	fprintf(stderr, "debug: %s\n", outputString);
	#endif
}

#endif

#undef FILE_HANDLE_CACHE_USE_FILE_IO_BUFFER
