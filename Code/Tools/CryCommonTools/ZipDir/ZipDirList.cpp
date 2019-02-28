// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#undef	 max
#include <algorithm>
#include "ZipFileFormat.h"
#include "ZipDirStructures.h"
#include "ZipDirList.h"
#include "ZipDirTree.h"

ZipDir::FileRecordList::FileRecordList(FileEntryTree* pTree)
{
	clear();
	reserve(pTree->NumFilesTotal());
	AddAllFiles(pTree);
}

//recursively adds the files from this directory and subdirectories
// the strRoot contains the trailing slash
void ZipDir::FileRecordList::AddAllFiles(FileEntryTree* pTree, string strRoot)
{
	for (FileEntryTree::SubdirMap::iterator it = pTree->GetDirBegin(); it != pTree->GetDirEnd(); ++it)
		AddAllFiles (it->second, strRoot + it->second->GetOriginalName() + "/");

	for (FileEntryTree::FileMap::iterator it = pTree->GetFileBegin(); it != pTree->GetFileEnd(); ++it)
	{
		FileRecord rec;
		rec.pFileEntry = pTree->GetFileEntry(it);
		const char* filename = rec.pFileEntry->szOriginalFileName ? rec.pFileEntry->szOriginalFileName : it->first;
		rec.strPath = strRoot + filename;
		push_back(rec);
	}
}


// sorts the files by the physical offset in the zip file
void ZipDir::FileRecordList::SortByFileOffset()
{
	std::sort (begin(), end(), FileRecordFileOffsetOrder());
}

// returns the size of CDR in the zip file
ZipDir::FileRecordList::ZipStats ZipDir::FileRecordList::GetStats() const
{
	ZipStats Stats;
	Stats.nSizeCDR = sizeof(ZipFile::CDREnd);
	Stats.nSizeCompactData = 0;
	// for each file, we'll need to store only its CDR header and the name
	for (const_iterator it = begin(); it != end(); ++it)
	{
		Stats.nSizeCDR         += sizeof(ZipFile::CDRFileHeader)   + it->strPath.length();
		Stats.nSizeCompactData += sizeof(ZipFile::LocalFileHeader) + it->strPath.length() + it->pFileEntry->desc.lSizeCompressed;
	}

	return Stats;
}

// puts the CDR into the given block of mem
size_t ZipDir::FileRecordList::MakeZipCDR(ZipFile::ulong lCDROffset, void*pBuffer, bool encryptedFlag) const
{
	const ZipFile::ushort nBaseVersion = std::max(encryptedFlag ? ZipFile::VERSION_ENCRYPTION_PKWARE : ZipFile::VERSION_DEFAULT, ZipFile::VERSION_COMPRESSION_DEFLATE);

	char* pCur = (char*)pBuffer;
	for (const_iterator it = begin(); it != end(); ++it)
	{
		ZipFile::CDRFileHeader& h = *(ZipFile::CDRFileHeader*)pCur;
		pCur = (char*)(&h+1);
		h.lSignature         = h.SIGNATURE;
		h.nVersionMadeBy     = nBaseVersion + (ZipFile::CREATOR_MSDOS << 8);
		h.nVersionNeeded     = nBaseVersion;
		h.nFlags             = 0;
		h.nMethod            = it->pFileEntry->nMethod;
		h.nLastModTime       = it->pFileEntry->nLastModTime;
		h.nLastModDate       = it->pFileEntry->nLastModDate;
		h.desc               = it->pFileEntry->desc;
		h.nFileNameLength    = (ZipFile::ushort)it->strPath.length();
		h.nExtraFieldLength  = 0;
		h.nFileCommentLength = 0;
		h.nDiskNumberStart   = 0;
		h.nAttrInternal      = 0;
		h.lAttrExternal      = 0;
		h.lLocalHeaderOffset = it->pFileEntry->nFileHeaderOffset;

		memcpy (pCur, it->strPath.c_str(), it->strPath.length());
		pCur += it->strPath.length();
	}	

	ZipFile::CDREnd& e = *(ZipFile::CDREnd*)pCur;
	e.lSignature       = e.SIGNATURE;
	e.nDisk            = encryptedFlag ? (1 << 15) : 0;
	e.nCDRStartDisk    = 0;
	e.numEntriesOnDisk = (ZipFile::ushort)this->size();
	e.numEntriesTotal  = (ZipFile::ushort)this->size();
	e.lCDRSize         = (ZipFile::ulong)(pCur - (char*)pBuffer);
	e.lCDROffset       = lCDROffset;
	e.nCommentLength   = 0;
	
	pCur = (char*)(&e+1);

	return pCur - (char*)pBuffer;
}


ZipDir::FileEntryList::FileEntryList (FileEntryTree* pTree, unsigned lCDROffset):
	m_lCDROffset (lCDROffset)
{
	Add (pTree);
}

void ZipDir::FileEntryList::Add(FileEntryTree* pTree)
{
	for (FileEntryTree::SubdirMap::iterator itDir = pTree->GetDirBegin(); itDir != pTree->GetDirEnd(); ++itDir)
	  Add(pTree->GetDirEntry(itDir));
	for (FileEntryTree::FileMap::iterator itFile = pTree->GetFileBegin(); itFile != pTree->GetFileEnd(); ++itFile)
		insert(pTree->GetFileEntry(itFile));
}

// updates each file entry's info about the next file entry
void ZipDir::FileEntryList::RefreshEOFOffsets()
{
	iterator it, itNext = begin();

	if (itNext != end())
	{
		while((it = itNext, ++itNext) != end())
		{
			// start scan
			(*it)->nEOFOffset = (*itNext)->nFileHeaderOffset;
		}
		// it is the last one..
		(*it)->nEOFOffset = m_lCDROffset;
	}
}


void ZipDir::FileRecordList::Backup(std::vector<FileEntry>&arrFiles) const
{
	arrFiles.resize (size());
	std::vector<FileEntry>::iterator itTgt = arrFiles.begin();

	for (const_iterator it = begin(); it != end(); ++it, ++itTgt)
		*itTgt = *it->pFileEntry;
}

void ZipDir::FileRecordList::Restore(const std::vector<FileEntry>&arrFiles)
{
	if (arrFiles.size() == size())
	{
		std::vector<FileEntry>::const_iterator itTgt = arrFiles.begin();
		for (iterator it = begin() ; it != end(); ++it, ++itTgt)
			*it->pFileEntry = *itTgt;
	}
}
