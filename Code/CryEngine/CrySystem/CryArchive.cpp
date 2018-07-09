// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryPak.h"
#include "CryArchive.h"

//////////////////////////////////////////////////////////////////////////
#ifndef OPTIMIZED_READONLY_ZIP_ENTRY

int CryArchiveRW::EnumEntries(Handle hFolder, IEnumerateArchiveEntries* pEnum)
{
	ZipDir::FileEntryTree* pRoot = (ZipDir::FileEntryTree*)hFolder;
	int nEntries = 0;
	bool bContinue = true;

	if (pEnum)
	{
		ZipDir::FileEntryTree::SubdirMap::iterator iter = pRoot->GetDirBegin();

		while (iter != pRoot->GetDirEnd() && bContinue)
		{
			bContinue = pEnum->OnEnumArchiveEntry(iter->first, iter->second, true, 0, 0);
			++iter;
			++nEntries;
		}

		ZipDir::FileEntryTree::FileMap::iterator iterFile = pRoot->GetFileBegin();

		while (iterFile != pRoot->GetFileEnd() && bContinue)
		{
			bContinue = pEnum->OnEnumArchiveEntry(iterFile->first, &iterFile->second, false, iterFile->second.desc.lSizeUncompressed, iterFile->second.GetModificationTime());
			++iterFile;
			++nEntries;
		}
	}

	return nEntries;
}

CryArchiveRW::Handle CryArchiveRW::GetRootFolderHandle()
{
	return m_pCache->GetRoot();
}

int CryArchiveRW::UpdateFileCRC(const char* szRelativePath, const uint32 dwCRC)
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char* pPath = AdjustPath(szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;

	m_pCache->UpdateFileCRC(pPath, dwCRC);

	return ZipDir::ZD_ERROR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
// deletes the file from the archive
int CryArchiveRW::RemoveFile(const char* szRelativePath)
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char* pPath = AdjustPath(szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->RemoveFile(pPath);
}

//////////////////////////////////////////////////////////////////////////
// deletes the directory, with all its descendants (files and subdirs)
int CryArchiveRW::RemoveDir(const char* szRelativePath)
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char* pPath = AdjustPath(szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->RemoveDir(pPath);
}

int CryArchiveRW::RemoveAll()
{
	return m_pCache->RemoveAll();

}

//////////////////////////////////////////////////////////////////////////
// Adds a new file to the zip or update an existing one
// adds a directory (creates several nested directories if needed)
// compression methods supported are 0 (store) and 8 (deflate) , compression level is 0..9 or -1 for default (like in zlib)
int CryArchiveRW::UpdateFile(const char* szRelativePath, void* pUncompressed, unsigned nSize, unsigned nCompressionMethod, int nCompressionLevel)
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char* pPath = AdjustPath(szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->UpdateFile(pPath, pUncompressed, nSize, nCompressionMethod, nCompressionLevel);
}

//////////////////////////////////////////////////////////////////////////
//   Adds a new file to the zip or update an existing one if it is not compressed - just stored  - start a big file
int CryArchiveRW::StartContinuousFileUpdate(const char* szRelativePath, unsigned nSize)
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char* pPath = AdjustPath(szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->StartContinuousFileUpdate(pPath, nSize);
}

//////////////////////////////////////////////////////////////////////////
// Adds a new file to the zip or update an existing's segment if it is not compressed - just stored
// adds a directory (creates several nested directories if needed)
int CryArchiveRW::UpdateFileContinuousSegment(const char* szRelativePath, unsigned nSize, void* pUncompressed, unsigned nSegmentSize, unsigned nOverwriteSeekPos)
{
	if (m_nFlags & FLAGS_READ_ONLY)
		return ZipDir::ZD_ERROR_INVALID_CALL;

	char szFullPath[CCryPak::g_nMaxPath];
	const char* pPath = AdjustPath(szRelativePath, szFullPath);
	if (!pPath)
		return ZipDir::ZD_ERROR_INVALID_PATH;
	return m_pCache->UpdateFileContinuousSegment(pPath, nSize, pUncompressed, nSegmentSize, nOverwriteSeekPos);
}

#endif //#ifndef OPTIMIZED_READONLY_ZIP_ENTRY
