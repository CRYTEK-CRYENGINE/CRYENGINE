// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileEnum.h"
#include <io.h>

CFileEnum::CFileEnum()
	: m_hEnumFile(0)
{
}

CFileEnum::~CFileEnum()
{
	if (m_hEnumFile)
	{
		_findclose(m_hEnumFile);
		m_hEnumFile = 0;
	}
}

bool CFileEnum::StartEnumeration(
  const char* szEnumPath,
  const char* szEnumPattern,
  __finddata64_t* pFile)
{
	//////////////////////////////////////////////////////////////////////
	// Take path and search pattern as separate arguments
	//////////////////////////////////////////////////////////////////////
	char szPath[_MAX_PATH];

	// Build enumeration path
	cry_strcpy(szPath, szEnumPath);

	if (szPath[strlen(szPath) - 1] != '\\' && szPath[strlen(szPath) - 1] != '/')
	{
		cry_strcat(szPath, "\\");
	}

	cry_strcat(szPath, szEnumPattern);

	return StartEnumeration(szPath, pFile);
}

bool CFileEnum::StartEnumeration(const char* szEnumPathAndPattern, __finddata64_t* pFile)
{
	// End any previous enumeration
	if (m_hEnumFile)
	{
		_findclose(m_hEnumFile);
		m_hEnumFile = 0;
	}

	// Start the enumeration
	if ((m_hEnumFile = _findfirst64(szEnumPathAndPattern, pFile)) == -1L)
	{
		// No files found
		_findclose(m_hEnumFile);
		m_hEnumFile = 0;

		return false;
	}

	return true;
}

bool CFileEnum::GetNextFile(__finddata64_t* pFile)
{
	// Fill file strcuture
	if (_findnext64(m_hEnumFile, pFile) == -1)
	{
		// No more files left
		_findclose(m_hEnumFile);
		m_hEnumFile = 0;

		return false;
	}
	;

	// At least one file left
	return true;
}

inline bool ScanDirectoryRecursive(
  const string& root,
  const string& path,
  const string& file,
  std::vector<string>& files,
  bool bRecursive,
  bool bSkipPaks)
{
	struct __finddata64_t foundFile;
	intptr_t hFile;
	bool bFoundAny = false;

	string fullPath = root + path + file;

	if ((hFile = _findfirst64(fullPath, &foundFile)) != -1L)
	{
		// Find the rest of the files.
		do
		{
			if (bSkipPaks && (foundFile.attrib & _A_IN_CRYPAK))
			{
				continue;
			}

			bFoundAny = true;
			files.push_back(path + foundFile.name);

		}
		while (_findnext64(hFile, &foundFile) == 0);

		_findclose(hFile);
	}

	if (bRecursive)
	{
		fullPath = root + path + "*.*";

		if ((hFile = _findfirst64(fullPath, &foundFile)) != -1L)
		{
			// Find directories.
			do
			{
				if (bSkipPaks && (foundFile.attrib & _A_IN_CRYPAK))
				{
					continue;
				}

				if (foundFile.attrib & _A_SUBDIR)
				{
					// If recursive.
					if (foundFile.name[0] != '.')
					{
						if (ScanDirectoryRecursive(
						      root,
						      path + foundFile.name + "\\",
						      file,
						      files,
						      bRecursive,
						      bSkipPaks))
							bFoundAny = true;
					}
				}
			}
			while (_findnext64(hFile, &foundFile) == 0);

			_findclose(hFile);
		}
	}

	return bFoundAny;
}

bool CFileEnum::ScanDirectory(
  const string& path,
  const string& file,
  std::vector<string>& files,
  bool bRecursive,
  bool bSkipPaks)
{
	return ScanDirectoryRecursive(path, "", file, files, bRecursive, bSkipPaks);
}

