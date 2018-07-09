// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __UpToDateFileHelpers_h__
#define __UpToDateFileHelpers_h__
#pragma once

#include <assert.h>
#include "FileUtil.h"
#include "IRCLog.h"  // RCLog

namespace UpToDateFileHelpers
{
inline bool FileExistsAndUpToDate(
	const char* const pDstFileName,
	const char* const pSrcFileName)
{
	const FILETIME dstFileTime = FileUtil::GetLastWriteFileTime(pDstFileName);
	if (!FileUtil::FileTimeIsValid(dstFileTime))
	{
		return false;
	}

	const FILETIME srcFileTime = FileUtil::GetLastWriteFileTime(pSrcFileName);
	if (!FileUtil::FileTimeIsValid(srcFileTime))
	{
		RCLogWarning("%s: Source file \"%s\" doesn't exist", __FUNCTION__, pSrcFileName);
		return false;
	}

	return FileUtil::FileTimesAreEqual(srcFileTime, dstFileTime);
}


inline bool SetMatchingFileTime(
	const char* const pDstFileName,
	const char* const pSrcFileName)
{
	const FILETIME srcFileTime = FileUtil::GetLastWriteFileTime(pSrcFileName);
	if (!FileUtil::FileTimeIsValid(srcFileTime))
	{
		RCLogError("%s: Source file \"%s\" doesn't exist", __FUNCTION__, pSrcFileName);
		return false;
	}

	const FILETIME dstFileTime = FileUtil::GetLastWriteFileTime(pDstFileName);
	if (!FileUtil::FileTimeIsValid(dstFileTime))
	{
		RCLogError("%s: Destination file \"%s\" doesn't exist", __FUNCTION__, pDstFileName);
		return false;
	}

	if (!FileUtil::SetFileTimes(pDstFileName, srcFileTime))
	{
		RCLogError("%s: Copying the date and time from \"%s\" to \"%s\" failed", __FUNCTION__, pSrcFileName, pDstFileName);
		return false;
	}

	assert(FileExistsAndUpToDate(pDstFileName, pSrcFileName));
	return true;
}
} // namespace UpToDateFileHelpers

#endif //__UpToDateFileHelpers_h__
