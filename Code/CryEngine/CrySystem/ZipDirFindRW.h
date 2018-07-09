// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _ZIP_DIR_SEARCH_RW_HDR_
#define _ZIP_DIR_SEARCH_RW_HDR_

namespace ZipDir
{

// create this structure and loop:
//  FindData fd (pZip);
//  for (fd.FindFirst("*.cgf"); fd.GetFileEntry(); fd.FindNext())
//  {} // inside the loop, use GetFileEntry() and GetFileName() to get the file entry and name records
class FindDataRW
{
public:
	FindDataRW(FileEntryTree* pRoot) :
		m_pRoot(pRoot),
		m_pDirHeader(NULL)
	{
		m_szWildcard[0] = 0;
	}

	// returns the directory to which the current object belongs
	FileEntryTree* GetParentDir() { return m_pDirHeader; }
protected:
	// initializes everything until the point where the file must be searched for
	// after this call returns successfully (with true returned), the m_szWildcard
	// contains the file name/wildcard and m_pDirHeader contains the directory where
	// the file (s) are to be found
	bool PreFind(const char* szWildcard);

	// matches the file wilcard in the m_szWildcard to the given file/dir name
	// this takes into account the fact that xxx. is the alias name for xxx
	bool MatchWildcard(const char* szName);

	// the directory inside which the current object (file or directory) is being searched
	FileEntryTree* m_pDirHeader;

	FileEntryTree* m_pRoot; // the root of the zip file in which to search

	// the actual wildcard being used in the current scan - the file name wildcard only!
	char m_szWildcard[_MAX_PATH];
};

class FindFileRW : public FindDataRW
{
public:
	FindFileRW(FileEntryTree* pRoot) :
		FindDataRW(pRoot)
	{
	}
	// if bExactFile is passed, only the file is searched, and besides with the exact name as passed (no wildcards)
	bool       FindFirst(const char* szWildcard);

	FileEntry* FindExact(const char* szPath);

	// goes on to the next file entry
	bool        FindNext();

	FileEntry*  GetFileEntry();
	const char* GetFileName();

protected:
	bool SkipNonMatchingFiles();
	FileEntryTree::FileMap::iterator m_itFile; // the current file iterator inside the parent directory
};

class FindDirRW : public FindDataRW
{
public:
	FindDirRW(FileEntryTree* pRoot) :
		FindDataRW(pRoot)
	{
	}
	// if bExactFile is passed, only the file is searched, and besides with the exact name as passed (no wildcards)
	bool           FindFirst(const char* szWildcard);

	FileEntryTree* FindExact(const char* szPath);

	// goes on to the next file entry
	bool           FindNext();

	FileEntryTree* GetDirEntry();
	const char*    GetDirName();

protected:
	bool SkipNonMatchingDirs();
	FileEntryTree::SubdirMap::iterator m_itDir; // the current dir index inside the parent directory
};
}

#endif
