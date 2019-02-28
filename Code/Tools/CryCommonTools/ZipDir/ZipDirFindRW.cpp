// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/smartptr.h>
#include "ZipFileFormat.h"
#include "ZipDirStructures.h"
#include "ZipDirTree.h"
#include "ZipDirCacheRW.h"
#include "ZipDirFindRW.h"
#include "StringHelpers.h"

bool ZipDir::FindFileRW::FindFirst (const char* szWildcard)
{
	if (!PreFind (szWildcard))
		return false;

	// finally, this is the name of the file
	m_itFile = m_pDirHeader->GetFileBegin();
	return SkipNonMatchingFiles();
}

bool ZipDir::FindDirRW::FindFirst (const char* szWildcard)
{
	if (!PreFind (szWildcard))
		return false;

	// finally, this is the name of the file
	m_itDir = m_pDirHeader->GetDirBegin();
	return SkipNonMatchingDirs();
}

// matches the file wilcard in the m_szWildcard to the given file/dir name
// this takes into account the fact that xxx. is the alias name for xxx
bool ZipDir::FindDataRW::MatchWildcard(const char* szName)
{
	if (StringHelpers::MatchesWildcards(szName, m_szWildcard))
		return true;

	// check if the file object name contains extension sign (.)
	const char* p;
	for (p = szName; *p && *p!='.'; ++p)
		continue;

	if (*p)
	{
		// there's an extension sign in the object, but it wasn't matched.. 
		assert (*p == '.');
		return false;
	}

	// no extension sign - add it
	char szAlias[_MAX_PATH+2];
	size_t nLength = p - szName;
	if (nLength > _MAX_PATH)
		nLength = _MAX_PATH;
	memcpy (szAlias, szName, nLength);
	szAlias[nLength]   = '.'; // add the alias
	szAlias[nLength+1] = '\0'; // terminate the string
	return StringHelpers::MatchesWildcards(szAlias, m_szWildcard);
}


ZipDir::FileEntry* ZipDir::FindFileRW::FindExact (const char* szPath)
{
	if (!PreFind (szPath))
		return NULL;

	FileEntryTree::FileMap::iterator itFile = m_pDirHeader->FindFile(m_szWildcard);
	if (itFile != m_pDirHeader->GetFileEnd())
		m_itFile = itFile;
	else
		m_pDirHeader = NULL; // we didn't find it, fail the search

	return m_pDirHeader?m_pDirHeader->GetFileEntry(itFile) : NULL;
}

ZipDir::FileEntryTree* ZipDir::FindDirRW::FindExact (const char* szPath)
{
	if (!PreFind(szPath))
		return NULL;

	// the wildcard will contain the target directory name
	return m_pDirHeader->FindDir(m_szWildcard);
}

//////////////////////////////////////////////////////////////////////////
// initializes everything until the point where the file must be searched for
// after this call returns successfully (with true returned), the m_szWildcard
// contains the file name/wildcard and m_pDirHeader contains the directory where
// the file (s) are to be found
bool ZipDir::FindDataRW::PreFind (const char* szWildcard)
{
	if (!m_pRoot)
		return false;

	// start the search from the root
	m_pDirHeader = m_pRoot;

	// for each path dir name, copy it into the buffer and try to find the subdirectory
	const char* pPath = szWildcard;
	for (;;)
	{
		char* pName = m_szWildcard;

		// at first we'll use the wildcard memory to save the directory names
		for (; *pPath && *pPath != '/' && *pPath != '\\' && pName < m_szWildcard+sizeof(m_szWildcard)-1; ++pPath, ++pName)
			*pName = ::tolower(*pPath);
		*pName = '\0';

		if (*pPath)
		{
			// this is the name of the directory
			FileEntryTree* pDirEntry = m_pDirHeader->FindDir(m_szWildcard);
			if (!pDirEntry)
			{
				m_pDirHeader = NULL; // finish the search
				return false;
			}
			m_pDirHeader = pDirEntry->GetDirectory();
			++pPath;
			assert(m_pDirHeader);
		}
		else
		{
			// finally, this is the name of the file (or directory)
			return true;
		}
	}
}

// goes on to the next entry
bool ZipDir::FindFileRW::FindNext ()
{
	if (m_pDirHeader && m_itFile != m_pDirHeader->GetFileEnd())
	{
		++m_itFile;
		return SkipNonMatchingFiles();
	}
	else
		return false;
}

// goes on to the next entry
bool ZipDir::FindDirRW::FindNext ()
{
	if (m_pDirHeader && m_itDir != m_pDirHeader->GetDirEnd())
	{
		++m_itDir;
		return SkipNonMatchingDirs();
	}
	else
		return false;
}

bool ZipDir::FindFileRW::SkipNonMatchingFiles()
{
	assert(m_pDirHeader);

	for(;m_itFile != m_pDirHeader->GetFileEnd(); ++m_itFile)
	{
		if (MatchWildcard(GetFileName()))
			return true;
	}
	// we didn't find anything other file else
	return false;
}

bool ZipDir::FindDirRW::SkipNonMatchingDirs()
{
	assert(m_pDirHeader);

	for(;m_itDir != m_pDirHeader->GetDirEnd(); ++m_itDir)
	{
		if (MatchWildcard(GetDirName()))
			return true;
	}
	// we didn't find anything other file else
	return false;
}


const ZipDir::FileEntry* ZipDir::FindFileRW::GetFileEntry() const
{
	return m_pDirHeader && m_itFile != m_pDirHeader->GetFileEnd()? m_pDirHeader->GetFileEntry(m_itFile) : NULL;
}
ZipDir::FileEntry* ZipDir::FindFileRW::GetFileEntry()
{
	return m_pDirHeader && m_itFile != m_pDirHeader->GetFileEnd()? m_pDirHeader->GetFileEntry(m_itFile) : NULL;
}

const ZipDir::FileEntryTree* ZipDir::FindDirRW::GetDirEntry() const
{
	return m_pDirHeader && m_itDir != m_pDirHeader->GetDirEnd() ? m_pDirHeader->GetDirEntry(m_itDir) : NULL;
}
ZipDir::FileEntryTree* ZipDir::FindDirRW::GetDirEntry()
{
	return m_pDirHeader && m_itDir != m_pDirHeader->GetDirEnd() ? m_pDirHeader->GetDirEntry(m_itDir) : NULL;
}

const char* ZipDir::FindFileRW::GetFileName ()
{
	if (m_pDirHeader && m_itFile != m_pDirHeader->GetFileEnd())
		return m_pDirHeader->GetFileName(m_itFile);
	else
		return ""; // default name
}

const char* ZipDir::FindDirRW::GetDirName ()
{
	if (m_pDirHeader && m_itDir != m_pDirHeader->GetDirEnd())
	{
		return m_pDirHeader->GetDirName(m_itDir);
	}
	else
		return ""; // default name
}
