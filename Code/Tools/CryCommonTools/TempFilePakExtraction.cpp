// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Opens a temporary file for read only access, where the file could be
// located in a zip or pak file. Note that if the file specified
// already exists it does not delete it when finished.

#include "stdafx.h"

#include "TempFilePakExtraction.h"
#include "FileUtil.h"
#include "PathHelpers.h"
#include "IPakSystem.h"

#include <CryString/CryPath.h>

TempFilePakExtraction::TempFilePakExtraction(const char* filename, const char* tempPath, IPakSystem* pPakSystem)
	: m_strOriginalFileName(filename)
	, m_strTempFileName(filename)
{
	if (!pPakSystem || !tempPath)
	{
		return;
	}

	{
		FILE* const fileOnDisk = fopen(m_strOriginalFileName.c_str(), "rb");
		if (fileOnDisk)
		{
			fclose(fileOnDisk);
			return;
		}
	}

	// Choose the name for the temporary file. 
	string tempFullFileName;
	{
		uint32 tempNumber = 0;
		{
			LARGE_INTEGER performanceCount;
			if (QueryPerformanceCounter(&performanceCount))
			{
				tempNumber = performanceCount.u.LowPart;
			}
		}

		string tempName;
		{
			// CryEngine's pak system supports filenames in format "@pakFilename|fileInPak",
			// so let's handle such cases by using fileInPak part of the filename.
			const size_t pos = m_strOriginalFileName.find_last_of('|');
			if (pos != string::npos)
			{
				tempName = m_strOriginalFileName.substr(pos + 1, string::npos);
				if (tempName.empty())
				{
					tempName ="BadFilenameSyntax";
				}
			}
			else
			{
				tempName = m_strOriginalFileName;
			}
			tempName = PathUtil::GetFile(tempName);
		}

		int tryCount = 2000;
		while (--tryCount >= 0)
		{
			tempFullFileName.Format("%sRC%04x_%s", tempPath, (tempNumber & 0xFFFF), tempName.c_str());

			if (!FileUtil::FileExists(tempFullFileName.c_str()))
			{
				FILE* const f = fopen(tempFullFileName.c_str(), "wb");
				if (f)
				{
					fclose(f);
					break;
				}
			}

			tempFullFileName.clear();
			++tempNumber;
		}

		if (tempFullFileName.empty())
		{
			return;
		}
	}

	if (pPakSystem->ExtractNoOverwrite(m_strOriginalFileName.c_str(), tempFullFileName.c_str()))
	{
		m_strTempFileName = tempFullFileName;
		SetFileAttributesA(m_strTempFileName.c_str(), FILE_ATTRIBUTE_READONLY);
	}
	else
	{
		DeleteFile(tempFullFileName.c_str());
	}
}


TempFilePakExtraction::~TempFilePakExtraction()
{
	if (HasTempFile())
	{
		SetFileAttributesA(m_strTempFileName.c_str(), FILE_ATTRIBUTE_ARCHIVE);
		DeleteFile(m_strTempFileName.c_str());
	}
}


bool TempFilePakExtraction::HasTempFile() const
{
	return (m_strOriginalFileName != m_strTempFileName);
}
