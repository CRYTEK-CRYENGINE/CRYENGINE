// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _ZIP_DIR_LIST_HDR_
#define _ZIP_DIR_LIST_HDR_

namespace ZipDir
{

// this is the array of file entries that's convenient to use to construct CDR
struct FileRecord
{
	string     strPath;    // relative path to the file inside zip
	FileEntry* pFileEntry; // the file entry itself

	void       ConstructFileRecord()
	{
		new(&strPath)string();
	}
};

struct FileDataRecord : public FileRecord
{
	FileDataRecord() : m_nRefCount(0), pHeap(nullptr) {}
	void AddRef()  { CryInterlockedIncrement(&m_nRefCount); }
	void Release() { if (CryInterlockedDecrement(&m_nRefCount) <= 0) Delete(); }

	void Delete()
	{
		pHeap->FreeTemporary(this);
	}

	static FileDataRecord* New(const FileRecord& rThat, CMTSafeHeap* pHeap)
	{
		FileDataRecord* pThis = (FileDataRecord*)pHeap->TempAlloc(
		  sizeof(FileDataRecord) + rThat.pFileEntry->desc.lSizeCompressed, "FileDataRecord::New");

		if (pThis)
		{
			pThis->m_nRefCount = 0;
			pThis->ConstructFileRecord();
			pThis->pHeap = pHeap;
			*static_cast<FileRecord*>(pThis) = rThat;
		}
		return pThis;
	}

	void* GetData() { return this + 1; }

	volatile signed int m_nRefCount; // the reference count
	CMTSafeHeap*        pHeap;
};

TYPEDEF_AUTOPTR(FileDataRecord);
typedef FileDataRecord_AutoPtr FileDataRecordPtr;

// this is used for construction of CDR
class FileRecordList : public std::vector<FileRecord>
{
public:
	FileRecordList(class FileEntryTree* pTree);

	struct ZipStats
	{
		// the size of the CDR in the file
		size_t nSizeCDR;
		// the size of the file data part (local file descriptors and file datas)
		// if it's compacted
		size_t nSizeCompactData;
	};

	// sorts the files by the physical offset in the zip file
	void SortByFileOffset();

	// returns the size of CDR in the zip file
	ZipStats GetStats() const;

	// puts the CDR into the given block of mem
	size_t MakeZipCDR(uint32 lCDROffset, void* p, bool encryptedFlag) const;

	void   Backup(std::vector<FileEntry>& arrFiles) const;
	void   Restore(const std::vector<FileEntry>& arrFiles);

protected:
	//recursively adds the files from this directory and subdirectories
	// the strRoot contains the trailing slash
	void AddAllFiles(class FileEntryTree* pTree, string strRoot = string());
};

struct FileRecordFileOffsetOrder
{
	bool operator()(const FileRecord& left, const FileRecord& right)
	{
#ifdef OPTIMIZED_READONLY_ZIP_ENTRY
		return left.pFileEntry->nFileDataOffset < right.pFileEntry->nFileDataOffset;
#else
		return left.pFileEntry->nFileHeaderOffset < right.pFileEntry->nFileHeaderOffset;
#endif
	}
};

struct FileEntryFileOffsetOrder
{
	bool operator()(FileEntry* pLeft, FileEntry* pRight) const
	{
#ifdef OPTIMIZED_READONLY_ZIP_ENTRY
		return pLeft->nFileDataOffset < pRight->nFileDataOffset;
#else
		return pLeft->nFileHeaderOffset < pRight->nFileHeaderOffset;
#endif
	}
};

// this is used for refreshing EOFOffsets
class FileEntryList : public std::set<FileEntry*, FileEntryFileOffsetOrder>
{
public:
	FileEntryList(class FileEntryTree* pTree, unsigned lCDROffset);
	// updates each file entry's info about the next file entry
	void RefreshEOFOffsets();
protected:
	void     Add(class FileEntryTree* pTree);
	unsigned m_lCDROffset;
};

}

#endif
