// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/CryWindows.h>  // DWORD
#include <CryString/UnicodeFunctions.h>

namespace FileUtil
{
	//	Magic number explanation:
	//	Both epochs are Gregorian. 1970 - 1601 = 369. Assuming a leap
	//	year every four years, 369 / 4 = 92. However, 1700, 1800, and 1900
	//	were NOT leap years, so 89 leap years, 280 non-leap years.
	//	89 * 366 + 280 * 365 = 134744 days between epochs. Of course
	//	60 * 60 * 24 = 86400 seconds per day, so 134744 * 86400 =
	//	11644473600 = SECS_BETWEEN_EPOCHS.
	//
	//	This result is also confirmed in the MSDN documentation on how
	//	to convert a time_t value to a win32 FILETIME.
	#define SECS_BETWEEN_EPOCHS (__int64(11644473600))
		/* 10^7 */
	#define SECS_TO_100NS (__int64(10000000)) 

	// Find all files matching filespec.
	bool ScanDirectory(const string& path, const string& filespec, std::vector<string>& files, bool recursive, const string& dirToIgnore);

	// Ensures that directory specified by szPathIn exists by creating all needed (sub-)directories.
	// Returns false in case of a failure.
	// Example: "c:\temp\test" ("c:\temp\test\" also works) - ensures that "c:\temp\test" exists.
	bool EnsureDirectoryExists(const char* szPathIn);

	// converts the FILETIME to the C Timestamp (compatible with dbghelp.dll)
	inline DWORD FiletimeToUnixTime(const FILETIME& ft)
	{
		return (DWORD)((((__int64&)ft) / SECS_TO_100NS) - SECS_BETWEEN_EPOCHS);
	}

	// converts the C Timestamp (compatible with dbghelp.dll) to FILETIME
	inline FILETIME UnixTimeToFiletime(DWORD nCTime)
	{
		const __int64 time = (nCTime + SECS_BETWEEN_EPOCHS) * SECS_TO_100NS;
		return (FILETIME&)time;
	}

	inline FILETIME GetInvalidFileTime()
	{
		FILETIME fileTime;
		fileTime.dwLowDateTime = 0;
		fileTime.dwHighDateTime = 0;
		return fileTime;
	}

	// returns file time stamps
	inline bool GetFileTimes(const char* filename, FILETIME* ftimeModify, FILETIME* ftimeCreate)
	{
		wstring widePath;
		Unicode::Convert(widePath, filename);

		WIN32_FIND_DATAW FindFileData;
		const HANDLE hFind = FindFirstFileW(widePath.c_str(), &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			return false;
		}
		FindClose(hFind);
		if (ftimeCreate)
		{
			ftimeCreate->dwLowDateTime = FindFileData.ftCreationTime.dwLowDateTime;
			ftimeCreate->dwHighDateTime = FindFileData.ftCreationTime.dwHighDateTime;
		}
		if (ftimeModify)
		{
			ftimeModify->dwLowDateTime = FindFileData.ftLastWriteTime.dwLowDateTime;
			ftimeModify->dwHighDateTime = FindFileData.ftLastWriteTime.dwHighDateTime;
		}
		return true;
	}

	inline FILETIME GetLastWriteFileTime(const char* filename)
	{
		wstring widePath;
		Unicode::Convert(widePath, filename);

		WIN32_FIND_DATAW FindFileData;
		const HANDLE hFind = FindFirstFileW(widePath.c_str(), &FindFileData);

		if (hFind == INVALID_HANDLE_VALUE)
		{
			return GetInvalidFileTime();
		}

		FindClose(hFind);

		FILETIME fileTime;
		fileTime.dwLowDateTime = FindFileData.ftLastWriteTime.dwLowDateTime;
		fileTime.dwHighDateTime = FindFileData.ftLastWriteTime.dwHighDateTime;

		return fileTime;
	}

	inline bool FileTimesAreEqual(const FILETIME& fileTime0, const FILETIME& fileTime1)
	{
		return
			(fileTime0.dwLowDateTime == fileTime1.dwLowDateTime) &&
			(fileTime0.dwHighDateTime == fileTime1.dwHighDateTime);
	}

	inline bool FileTimeIsValid(const FILETIME& fileTime)
	{
		return !FileTimesAreEqual(GetInvalidFileTime(), fileTime);
	}

	inline bool SetFileTimes(const char* const filename, const FILETIME& fileTime)
	{
		wstring widePath;
		Unicode::Convert(widePath, filename);

		const HANDLE hf = CreateFileW(widePath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
		if (hf != INVALID_HANDLE_VALUE)
		{
			if (SetFileTime(hf, &fileTime, &fileTime, &fileTime))
			{
				if (CloseHandle(hf))
				{
					return true;
				}
			}

			CloseHandle(hf);
		}

		return false;
	}

	inline __int64 GetFileSize(const char* const filename)
	{
		wstring widePath;
		Unicode::Convert(widePath, filename);

		WIN32_FILE_ATTRIBUTE_DATA fileAttr;
		const BOOL ok = GetFileAttributesExW(widePath.c_str(), GetFileExInfoStandard, &fileAttr);

		const __int64 fileSize = (__int64)
			((((unsigned __int64)((unsigned __int32)fileAttr.nFileSizeHigh)) << 32) | 
			((unsigned __int32)fileAttr.nFileSizeLow));

		return (ok && (fileSize >= 0)) ? fileSize : -1;
	}

	inline bool FileExists(const wchar_t* szPath)
	{
		const DWORD dwAttr = GetFileAttributesW(szPath);
		return dwAttr != INVALID_FILE_ATTRIBUTES && !(dwAttr & FILE_ATTRIBUTE_DIRECTORY);
	}

	inline bool FileExists(const char* szPath)
	{
		wstring widePath;
		Unicode::Convert(widePath, szPath);

		return FileExists(widePath.c_str());
	}

	inline bool DirectoryExists(const wchar_t* szPath)
	{
		const DWORD dwAttr = ::GetFileAttributesW(szPath);
		return dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	// Examples of paths that will return true:
	//   "existing_dir", "existing_dir/", "existing_dir/subdir", "e:", "e://", "e:.", "e:/a", ".", "..", "//storage/builds"
	// Examples of paths that will return false:
	//   "", "//storage", "//storage/.", "nonexisting_dir", "f:/" (if f: drive doesn't exist)
	inline bool DirectoryExists(const char* szPath)
	{
		wstring widePath;
		Unicode::Convert(widePath, szPath);

		return DirectoryExists(widePath.c_str());
	}

	void FindFiles(
		std::vector<string>& resultFiles,
		const int maxFileCount,
		const string& root,
		const string& path,
		bool recursive,
		const string& fileMask,
		const std::vector<string>& dirsToIgnore);
};
