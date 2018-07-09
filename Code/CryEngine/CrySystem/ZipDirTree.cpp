// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MTSafeAllocator.h"
#include "ZipFileFormat.h"
#include "ZipDirStructures.h"
#include "ZipDirTree.h"

// Adds or finds the file. Returns non-initialized structure if it was added,
// or an IsInitialized() structure if it was found
ZipDir::FileEntry* ZipDir::FileEntryTree::Add(char* szPath)
{
	// find the slash; if we found it, it's a subdirectory - add a subdirectory and
	// add the file to it.
	// if we didn't find it, it's a file - add the file to this dir

	char* pSlash;
	for (pSlash = szPath; *pSlash && *pSlash != '/' && *pSlash != '\\'; ++pSlash)
		continue; // find the next slash

	if (*pSlash)
	{
		FileEntryTree* pSubdir;
		// we have a subdirectory here - create the file in it
		{
			char* dir = szPath;
			*pSlash = '\0';

			SubdirMap::iterator it = m_mapDirs.find(dir);
			if (it == m_mapDirs.end())
				m_mapDirs.insert(SubdirMap::value_type(dir, pSubdir = new FileEntryTree));
			else
				pSubdir = it->second;
		}

		return pSubdir->Add(pSlash + 1);
	}
	else
	{
		return &m_mapFiles[szPath];
	}
}

// adds a file to this directory
ZipDir::ErrorEnum ZipDir::FileEntryTree::Add(char* szPath, const FileEntry& file)
{
	FileEntry* pFile = Add(szPath);
	if (!pFile)
		return ZD_ERROR_INVALID_PATH;
	if (pFile->IsInitialized())
		return ZD_ERROR_FILE_ALREADY_EXISTS;
	*pFile = file;
	return ZD_ERROR_SUCCESS;
}

// returns the number of files in this tree, including this and sublevels
unsigned int ZipDir::FileEntryTree::NumFilesTotal() const
{
	unsigned int numFiles = (unsigned int)m_mapFiles.size();
	for (SubdirMap::const_iterator it = m_mapDirs.begin(); it != m_mapDirs.end(); ++it)
		numFiles += it->second->NumFilesTotal();
	return numFiles;
}

//////////////////////////////////////////////////////////////////////////
unsigned int ZipDir::FileEntryTree::NumDirsTotal() const
{
	unsigned int numDirs = 1;
	for (SubdirMap::const_iterator it = m_mapDirs.begin(); it != m_mapDirs.end(); ++it)
	{
		numDirs += it->second->NumDirsTotal();
	}
	return numDirs;
}

#ifdef _TEST_
size_t g_nSF = 0, g_nSS = 0, g_nSN = 0, g_nSNa = 0, g_nSH;
size_t g_nGF = 0, g_nGS = 0, g_nGN = 0, g_nGNa = 0, g_nGH;
#endif

// returns the size required to serialize the tree
size_t ZipDir::FileEntryTree::GetSizeSerialized() const
{
	// the total size of name pool gets aligned on 4-byte boundary
	size_t nSizeOfNamePool = 0;
	size_t nSizeOfFileEntries = 0, nSizeOfDirEntries = 0;
	size_t nSizeOfSubdirs = 0;

	for (SubdirMap::const_iterator itDir = m_mapDirs.begin(); itDir != m_mapDirs.end(); ++itDir)
	{
		nSizeOfDirEntries += sizeof(DirEntry);
		const char* dirname = itDir->first;
		nSizeOfNamePool += strlen(dirname) + 1;
		nSizeOfSubdirs += itDir->second->GetSizeSerialized();
	}

	// for each file, we need to have an entry in the name pool and in the file list
	for (FileMap::const_iterator itFile = m_mapFiles.begin(); itFile != m_mapFiles.end(); ++itFile)
	{
		nSizeOfFileEntries += sizeof(FileEntry);
		const char* fname = itFile->first;
		nSizeOfNamePool += strlen(fname) + 1;
	}

	//Timur, not relevant anymore
	/*
	   if (nSizeOfNamePool > 0xFFFF)
	   {
	   // we don't support so long names/directories
	   THROW_ZIPDIR_ERROR(ZD_ERROR_UNSUPPORTED,"Name pool larger then 65536 bytes");
	   }
	 */

#ifdef _TEST_
	g_nGF += nSizeOfFileEntries;
	g_nGS += nSizeOfDirEntries;
	g_nGN += nSizeOfNamePool;
	g_nGNa += ((nSizeOfNamePool + 3) & ~3);
	g_nGH += sizeof(DirHeader);
#endif

	return sizeof(DirHeader) + ((nSizeOfNamePool + 3) & ~3) + nSizeOfDirEntries + nSizeOfFileEntries + nSizeOfSubdirs;
}

// serializes into the memory
size_t ZipDir::FileEntryTree::Serialize(DirHeader* pDirHeader) const
{
	pDirHeader->numDirs = (uint16)m_mapDirs.size();
	pDirHeader->numFiles = (uint16)m_mapFiles.size();
	DirEntry* pDirEntries = (DirEntry*)(pDirHeader + 1);
	FileEntry* pFileEntries = (FileEntry*)(pDirEntries + pDirHeader->numDirs);
	char* pNamePool = (char*)(pFileEntries + pDirHeader->numFiles);

	char* pName = pNamePool;
	DirEntry* pDirEntry = pDirEntries;
	FileEntry* pFileEntry = pFileEntries;

	SubdirMap::const_iterator itDir;
	for (itDir = m_mapDirs.begin(); itDir != m_mapDirs.end(); ++itDir)
	{
		pDirEntry->nNameOffset = (uint32)(pName - pNamePool);
		size_t nNameLen = strlen(itDir->first);
		memcpy(pName, itDir->first, nNameLen + 1);
		pName += nNameLen + 1;
		++pDirEntry;
	}

	assert((FileEntry*)pDirEntry == pFileEntry);

	// for each file, we need to have an entry in the name pool and in the file list
	for (FileMap::const_iterator itFile = m_mapFiles.begin(); itFile != m_mapFiles.end(); ++itFile)
	{
		*pFileEntry = itFile->second;
		const char* filename = itFile->first;
		pFileEntry->nNameOffset = (uint32)(pName - pNamePool);
		size_t nNameLen = strlen(filename);
		memcpy(pName, filename, nNameLen + 1);
		pName += nNameLen + 1;
		++pFileEntry;
	}
	assert((const char*)pFileEntry == pNamePool);

	// now the name pool is full. Go on and fill the other directories
	const char* pSubdirHeader = (const char*)(((UINT_PTR)(pName + 3)) & ~3);

#ifdef _TEST_
	g_nSF += pDirHeader->numFiles * sizeof(FileEntry);
	g_nSS += pDirHeader->numDirs * sizeof(DirEntry);
	g_nSN += pName - pNamePool;
	g_nSNa += pSubdirHeader - pNamePool;
	g_nSH += sizeof(DirHeader);
#endif

	pDirEntry = pDirEntries;
	for (itDir = m_mapDirs.begin(); itDir != m_mapDirs.end(); ++itDir)
	{
		pDirEntry->nDirHeaderOffset = (uint32)(pSubdirHeader - (const char*)pDirEntry);
		pSubdirHeader += itDir->second->Serialize((DirHeader*)pSubdirHeader);
		++pDirEntry;
	}

	return pSubdirHeader - (const char*)pDirHeader;
}

void ZipDir::FileEntryTree::Clear()
{
	for (SubdirMap::iterator it = m_mapDirs.begin(); it != m_mapDirs.end(); ++it)
		delete it->second;
	m_mapDirs.clear();
	m_mapFiles.clear();
}

size_t ZipDir::FileEntryTree::GetSize() const
{
	size_t nSize = sizeof(*this);
	for (SubdirMap::const_iterator itDir = m_mapDirs.begin(); itDir != m_mapDirs.end(); ++itDir)
		nSize += strlen(itDir->first) + sizeof(*itDir) + itDir->second->GetSize();

	for (FileMap::const_iterator itFile = m_mapFiles.begin(); itFile != m_mapFiles.end(); ++itFile)
		nSize += strlen(itFile->first) + sizeof(*itFile);
	return nSize;
}

bool ZipDir::FileEntryTree::IsOwnerOf(const FileEntry* pFileEntry) const
{
	for (FileMap::const_iterator itFile = m_mapFiles.begin(); itFile != m_mapFiles.end(); ++itFile)
		if (pFileEntry == &itFile->second)
			return true;

	for (SubdirMap::const_iterator itDir = m_mapDirs.begin(); itDir != m_mapDirs.end(); ++itDir)
		if (itDir->second->IsOwnerOf(pFileEntry))
			return true;

	return false;
}

ZipDir::FileEntryTree* ZipDir::FileEntryTree::FindDir(const char* szDirName)
{
	SubdirMap::iterator it = m_mapDirs.find(szDirName);
	if (it == m_mapDirs.end())
		return NULL;
	else
		return it->second;
}

ZipDir::FileEntryTree::FileMap::iterator ZipDir::FileEntryTree::FindFile(const char* szFileName)
{
	return m_mapFiles.find(szFileName);
}

ZipDir::FileEntry* ZipDir::FileEntryTree::GetFileEntry(FileMap::iterator it)
{
	return it == GetFileEnd() ? NULL : &it->second;
}

ZipDir::FileEntryTree* ZipDir::FileEntryTree::GetDirEntry(SubdirMap::iterator it)
{
	return it == GetDirEnd() ? NULL : it->second;
}

ZipDir::ErrorEnum ZipDir::FileEntryTree::RemoveDir(const char* szDirName)
{
	SubdirMap::iterator itRemove = m_mapDirs.find(szDirName);
	if (itRemove == m_mapDirs.end())
		return ZD_ERROR_FILE_NOT_FOUND;

	delete itRemove->second;
	m_mapDirs.erase(itRemove);
	return ZD_ERROR_SUCCESS;
}

ZipDir::ErrorEnum ZipDir::FileEntryTree::RemoveFile(const char* szFileName)
{
	FileMap::iterator itRemove = m_mapFiles.find(szFileName);
	if (itRemove == m_mapFiles.end())
		return ZD_ERROR_FILE_NOT_FOUND;

	m_mapFiles.erase(itRemove);
	return ZD_ERROR_SUCCESS;
}
