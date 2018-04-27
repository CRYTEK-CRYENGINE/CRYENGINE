// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Declaration of the class that will keep the ZipDir Cache object
// and will provide all its services to access Zip file, plus it will
// provide services to write to the zip file efficiently
// Time to time, the contained Cache object will be recreated during 
// an archive add operation.

#ifndef _ZIP_DIR_CACHE_RW_HDR_
#define _ZIP_DIR_CACHE_RW_HDR_

#include "SimpleStringPool.h"
#include <CryString/StringUtils.h>

struct PackFileJob;
namespace ZipDir
{

struct FileDataRecord;
TYPEDEF_AUTOPTR(FileDataRecord);
typedef FileDataRecord_AutoPtr FileDataRecordPtr;

struct IReporter
{
	virtual void ReportAdded(const char* filename) = 0;
	virtual void ReportMissing(const char* filename) = 0;
	virtual void ReportUpToDate(const char* filename) = 0;
	virtual void ReportSkipped(const char* filename) = 0;
	virtual void ReportFailed(const char* filename, const char* error) = 0;
	virtual void ReportSpeed(double bytesPerSecond) = 0;
};

struct ISplitter
{
	// Arguments:
	//   total - the current size of the pak
	//   add   - the size of the file to add
	//   sub   - the size of the old version of the file which will be removed from the pak
	// Return:
	//   true if adding the current file to the current pak is still permitted.
	virtual bool CheckWriteLimit(size_t total, size_t add, size_t sub) const = 0;

	// Arguments:
	//   total  - the current size of the pak
	//   add    - the size of the file to add
	//   sub    - the size of the old version of the file which will be removed from the pak
	//   offset - the position of the first file which has not been added to the pak
	//            in the array passed to "UpdateMultipleFiles()"
	virtual void SetLastFile(size_t total, size_t add, size_t sub, int offset) = 0;
};

struct IEncryptPredicate
{
	virtual bool Match(const char* filename) = 0;
};

class CacheRW
{
public:
	enum EncryptionChange
	{
		ENCRYPT,
		DECRYPT
	};
	// the size of the buffer that's using during re-linking the zip file
	enum 
	{
		g_nSizeRelinkBuffer = 128*1024*1024, // 128 Mbytes
		g_nMaxItemsRelinkBuffer = 1024 // max number of files to read before (without) writing
	};

	void AddRef();
	void Release();


	CacheRW(bool encryptHeaders, const EncryptionKey& encryptionKey);
	~CacheRW();

	bool IsValid () const
	{
		return m_pFile != NULL;
	}

	static char* UnifyPath(char *const str, const char *pPath);
	static char* ToUnixPath(char *const str, const char *pPath);
	char* AllocPath(const char *pPath);

	// opens the given zip file and connects to it. Creates a new file if no such file exists
	// if successful, returns true.
	//ErrorEnum Open (CMTSafeHeap* pHeap, InitMethodEnum nInitMethod, unsigned nFlags, const char* szFile);

	// Adds a new file to the zip or update an existing one
	// adds a directory (creates several nested directories if needed)
	ErrorEnum UpdateFile(const char* szRelativePath, void* pUncompressed, unsigned nSize, unsigned nCompressionMethod, int nCompressionLevel,__time64_t modTime);

	// Sets if Archive should be encrypted or decrypted on close.
	bool EncryptArchive(EncryptionChange change, IEncryptPredicate* encryptContentPredicate, int* numChanged, int* numSkipped);

	// Adds or updates a bunch of files. Creates directories if needed. Multithreaded when numExtraThreads > 0
	bool UpdateMultipleFiles(const char** realFilenames, const char** filenamesInZip, size_t fileCount,
													 int compressionLevel, bool encryptContent, size_t zipMaxSize, int sourceMinSize, int sourceMaxSize,
													 int numExtraThreads, ZipDir::IReporter* reporter, ZipDir::ISplitter* splitter = NULL);

	//   Adds a new file to the zip or update an existing one if it is not compressed - just stored  - start a big file
	ErrorEnum StartContinuousFileUpdate(const char* szRelativePath, unsigned nSize);

	// Adds a new file to the zip or update an existing's segment if it is not compressed - just stored 
	// adds a directory (creates several nested directories if needed)
	// Arguments:
	//   nOverwriteSeekPos - 0xffffffff means the seek pos should not be overwritten
	ErrorEnum UpdateFileContinuousSegment (const char* szRelativePath, unsigned nSize, void* pUncompressed, unsigned nSegmentSize, unsigned nOverwriteSeekPos );

	ErrorEnum UpdateFileCRC(const char* szRelativePath, unsigned dwCRC32 );

	// deletes the file from the archive
	ErrorEnum RemoveFile(const char* szRelativePath);

	// deletes the directory, with all its descendants (files and subdirs)
	ErrorEnum RemoveDir(const char* szRelativePath);

	// deletes all files and directories in this archive
	ErrorEnum RemoveAll();

	// closes the current zip file
	void Close();

	FileEntry* FindFile(const char* szPath, bool bFullInfo = false);

	ErrorEnum ReadFile(FileEntry* pFileEntry, void* pCompressed, void* pUncompressed);

	void* AllocAndReadFile (FileEntry* pFileEntry);

	void Free (void*p)
	{
		free(p);
	}

	// refreshes information about the given file entry into this file entry
	ErrorEnum Refresh (FileEntry* pFileEntry);

	// returns the size of memory occupied by the instance of this cache
	size_t GetSize() const;

	// returns the compressed size of all the entries
	size_t GetCompressedSize() const;

	// returns the total size of memory occupied by the instance of this cache and all the compressed files
	size_t GetTotalFileSize() const;

	// returns the total size of space occupied on disk by the instance of this cache and all the compressed files
	size_t GetTotalFileSizeOnDiskSoFar();

	// QUICK check to determine whether the file entry belongs to this object
	bool IsOwnerOf (const FileEntry* pFileEntry) const
	{
		return m_treeDir.IsOwnerOf(pFileEntry);
	}

	// returns the string - path to the zip file from which this object was constructed.
	// this will be "" if the object was constructed with a factory that wasn't created with FLAGS_MEMORIZE_ZIP_PATH
	const char* GetFilePath() const
	{
		return m_strFilePath.c_str();
	}

	FileEntryTree* GetRoot()
	{
		return &m_treeDir;
	}

	const FileEntryTree* GetRoot() const
	{
		return &m_treeDir;
	}

	// writes the CDR to the disk
	bool WriteCDR() {return WriteCDR(m_pFile, m_bEncryptedHeaders);}
	bool WriteCDR(FILE* fTarget, bool encryptHeaders);

	bool RelinkZip();
protected:
	bool RelinkZip(FILE* fTmp);
	// writes out the file data in the queue into the given file. Empties the queue
	bool WriteZipFiles(std::vector<FileDataRecordPtr>& queFiles, FILE* fTmp);
	// generates random file name
	string GetRandomName(int nAttempt);

	bool ReadCompressedData(char* data, size_t size);
	bool WriteCompressedData(const char* data, size_t size, bool encrypt, FILE* file);
	bool WriteNullData(size_t size);

	void StorePackedFile(PackFileJob* job);
protected:

	friend class CacheFactory;
	volatile signed int m_nRefCount; // the reference count
	FileEntryTree m_treeDir;
	FILE *m_pFile;
	string m_strFilePath;

	// offset to the start of CDR in the file,even if there's no CDR there currently
	// when a new file is added, it can start from here, but this value will need to be updated then
	ZipFile::ulong m_lCDROffset;	

	CSimpleStringPool m_tempStringPool;

	enum
	{
		// if this is set, the file needs to be compacted before it can be used by 
		// all standard zip tools, because gaps between file datas can be present
		FLAGS_UNCOMPACTED = 1 << 0,
		// if this is set, the CDR needs to be written to the file
		FLAGS_CDR_DIRTY   = 1 << 1,
		// if this is set, the file is opened in read-only mode. no write operations are to be performed
		FLAGS_READ_ONLY   = 1 << 2,
		// when this is set, compact operation is not performed
		FLAGS_DONT_COMPACT = 1 << 3
	};
	unsigned m_nFlags;
	size_t m_fileAlignment;

	// CDR buffer.
	std::vector<char> m_CDR_buffer;
	// unified names buffer
	std::vector<char> m_unifiedNameBuffer;

	EncryptionKey m_encryptionKey;
	bool m_bEncryptedHeaders;
	bool m_bHeadersEncryptedOnClose;
};

TYPEDEF_AUTOPTR(CacheRW);
typedef CacheRW_AutoPtr CacheRWPtr;

// creates and if needed automatically destroys the file entry
class FileEntryTransactionAdd
{
	class CacheRW* m_pCache;
	char m_szPath[_MAX_PATH];
	FileEntry* m_pFileEntry;
	bool m_bComitted;
public:
	operator FileEntry* () {return m_pFileEntry;}
	operator bool() const{return m_pFileEntry != NULL;}
	FileEntry* operator -> () {return m_pFileEntry;}
	FileEntryTransactionAdd(class CacheRW* pCache, char* szPath, char* szUnifiedPath):
		m_pCache(pCache),
		m_bComitted (false)
	{
		// we need to copy path, because original one will be destroyed by FileEntryTree::Add call
		cry_strcpy(m_szPath, szUnifiedPath);
		m_pFileEntry = m_pCache->GetRoot()->Add(szPath, szUnifiedPath);
	}
	~FileEntryTransactionAdd()
	{
		if (m_pFileEntry && !m_bComitted)
			m_pCache->RemoveFile(m_szPath);
	}
	void Commit()
	{
		m_bComitted = true;
	}
};


}

#endif
