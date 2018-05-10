// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This file contains only the support definitions for CZipDir class
// implementation. This it to unload the ZipDir.h from secondary stuff.

#ifndef _ZIP_DIR_STRUCTURES
#define _ZIP_DIR_STRUCTURES

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include <ctype.h>
#endif

namespace ZipDir
{

// possible errors occuring during the method execution
// to avoid clushing with the global Windows defines, we prefix these with ZD_
enum ErrorEnum
{
	ZD_ERROR_SUCCESS = 0,
	ZD_ERROR_IO_FAILED,
	ZD_ERROR_UNEXPECTED,
	ZD_ERROR_UNSUPPORTED,
	ZD_ERROR_INVALID_SIGNATURE,
	ZD_ERROR_ZIP_FILE_IS_CORRUPT,
	ZD_ERROR_DATA_IS_CORRUPT,
	ZD_ERROR_NO_CDR,
	ZD_ERROR_CDR_IS_CORRUPT,
	ZD_ERROR_NO_MEMORY,
	ZD_ERROR_VALIDATION_FAILED,
	ZD_ERROR_CRC32_CHECK,
	ZD_ERROR_ZLIB_FAILED,
	ZD_ERROR_ZLIB_CORRUPTED_DATA,
	ZD_ERROR_ZLIB_NO_MEMORY,
	ZD_ERROR_CORRUPTED_DATA,
	ZD_ERROR_INVALID_CALL,
	ZD_ERROR_NOT_IMPLEMENTED,
	ZD_ERROR_FILE_NOT_FOUND,
	ZD_ERROR_DIR_NOT_FOUND,
	ZD_ERROR_NAME_TOO_LONG,
	ZD_ERROR_INVALID_PATH,
	ZD_ERROR_FILE_ALREADY_EXISTS
};

// the error describes the reason of the error, as well as the error code, line of code where it happened etc.
struct Error
{
	Error(ErrorEnum _nError, const char* _szDescription, const char* _szFunction, const char* _szFile, unsigned _nLine):
		nError(_nError),
		m_szDescription(_szDescription),
		szFunction(_szFunction),
		szFile(_szFile),
		nLine(_nLine)
	{
	}

	ErrorEnum nError;
	const char* getError();

	const char* getDescription() {return m_szDescription;}
	const char* szFunction, *szFile;
	unsigned nLine;
protected:
	// the description of the error; if needed, will be made as a dynamic string
	const char* m_szDescription;
};

//#define THROW_ZIPDIR_ERROR(ZD_ERR,DESC) throw Error (ZD_ERR, DESC, __FUNCTION__, __FILE__, __LINE__)
//#define THROW_ZIPDIR_ERROR(ZD_ERR,DESC) CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,DESC )

#define THROW_ZIPDIR_ERROR(ZD_ERR,DESC) 

struct EncryptionKey
{
	uint32 key[4];

	explicit EncryptionKey(const uint32 data[4])
	{
		memcpy(key, data, sizeof(key));
	}

	EncryptionKey()
	{
		memset(key, 0, sizeof(key));
	}
};

// possible initialization methods
enum InitMethodEnum
{
	// initialize as fast as possible, with minimal validation
	ZD_INIT_FAST,
	// after initialization, scan through all file headers, precache the actual file data offset values and validate the headers
	ZD_INIT_FULL,
	// scan all file headers and try to decompress the data, searching for corrupted files
	ZD_INIT_VALIDATE,
	// maximum level of validation, checks for integrity of the archive
	ZD_INIT_VALIDATE_MAX = ZD_INIT_VALIDATE
};

typedef void* (*FnAlloc) (void* pUserData, unsigned nItems, unsigned nSize);
typedef void (*FnFree)  (void* pUserData, void* pAddress);

//////////////////////////////////////////////////////////////////////////
// This structure contains the pointers to functions for memory management
// by default, it's initialized to default malloc/free
#if 0
struct Allocator
{
	FnAlloc fnAlloc;
	FnFree fnFree;
	void* pOpaque;

	static void* DefaultAlloc (void*, unsigned nItems, unsigned nSize)
	{
		return malloc (nItems*nSize);
	}

	static void DefaultFree (void*, void* pAddress)
	{
		free (pAddress);
	}

	void* Alloc (unsigned nItems, unsigned nSize)
	{
		return this->fnAlloc(this->pOpaque, nItems, nSize);
	}

	void Free (void*pAddress)
	{
		this->fnFree (this->pOpaque, pAddress);
	}

	// constructs the allocator object; by default, the stdlib functions are used
	Allocator (FnAlloc fnAllocIn = DefaultAlloc, FnFree fnFreeIn = DefaultFree, void* pOpaqueIn = NULL):
		fnAlloc(fnAllocIn),
		fnFree (fnFreeIn),
		pOpaque(pOpaqueIn)
	{
	}
};
#endif
// instance of this class just releases the memory when it's destructed
struct SmartHeapPtr
{
	SmartHeapPtr():
		m_pAddress(NULL)
	{
	}
	~SmartHeapPtr()
	{
		Release();
	}

	void Attach (void*p)
	{
		Release();
		m_pAddress = p;
	}

	void* Detach()
	{
		void* p = m_pAddress;
		m_pAddress = NULL;
		return p;
	}

	void Release()
	{
		if (m_pAddress)
		{
			free(m_pAddress);
			m_pAddress = NULL;
		}
	}
protected:
	// the pointer to free
	void* m_pAddress;
};

typedef SmartHeapPtr SmartPtr;

// Uncompresses raw (without wrapping) data that is compressed with method 8 (deflated) in the Zip file
// returns one of the Z_* errors (Z_OK upon success)
extern int ZipRawUncompress (void* pUncompressed, unsigned long* pDestSize, const void* pCompressed, unsigned long nSrcSize);

// compresses the raw data into raw data. The buffer for compressed data itself with the heap passed. Uses method 8 (deflate)
// returns one of the Z_* errors (Z_OK upon success), and the size in *pDestSize. the pCompressed buffer must be at least nSrcSize*1.001+12 size
extern int ZipRawCompress (const void* pUncompressed, unsigned long* pDestSize, void* pCompressed, unsigned long nSrcSize, int nLevel);

//////////////////////////////////////////////////////////////////////////
struct SExtraZipFileData
{
	SExtraZipFileData() : nLastModifyTime(0) {}

	uint64 nLastModifyTime;
};

// this is the record about the file in the Zip file.
struct FileEntry
{
	enum {INVALID_DATA_OFFSET = 0xFFFFFFFF};
	
	ZipFile::DataDescriptor desc;
	ZipFile::ulong nFileHeaderOffset; // offset of the local file header
	ZipFile::ulong nFileDataOffset;		// offset of the packed info inside the file; NOTE: this can be INVALID_DATA_OFFSET, if not calculated yet!
	ZipFile::ushort nMethod;					// the method of compression (0 if no compression/store)
	ZipFile::ushort nNameOffset;			// offset of the file name in the name pool for the directory

	// the file modification times
	ZipFile::ushort nLastModTime;
	ZipFile::ushort nLastModDate;

	uint64 nNTFS_LastModifyTime;

	// the offset to the start of the next file's header - this
	// can be used to calculate the available space in zip file
	ZipFile::ulong nEOFOffset;
	const char* szOriginalFileName; // original filename (for CacheRW)

	FileEntry()
		: nFileHeaderOffset(INVALID_DATA_OFFSET)
		, nFileDataOffset(INVALID_DATA_OFFSET)
		, nMethod(0)
		, nNameOffset(0)
		, nLastModTime(0)
		, nLastModDate(0)
		, nEOFOffset(0)
		, szOriginalFileName(0)
	{
	}
	
	FileEntry(const ZipFile::CDRFileHeader& header,const SExtraZipFileData &extra);

	bool IsInitialized ()
	{
		// structure marked as non-initialized should have nFileHeaderOffset == INVALID_DATA_OFFSET
		return nFileHeaderOffset != INVALID_DATA_OFFSET;
	}
	// returns the name of this file, given the pointer to the name pool
	const char* GetName(const char* pNamePool)const
	{
		return pNamePool + nNameOffset;
	}

	// sets the current time to modification time
	// calculates CRC32 for the new data
	void OnNewFileData(void* pUncompressed, unsigned nSize, unsigned nCompressedSize, unsigned nCompressionMethod, bool bContinuous);

	uint64 GetModificationTime();
	void SetFromFileTimeNTFS( __int64 timestamp );
	bool CompareFileTimeNTFS( __int64 timestamp );
};

// tries to refresh the file entry from the given file (reads fromthere if needed)
// returns the error code if the operation was impossible to complete
extern ErrorEnum Refresh (FILE*f, FileEntry* pFileEntry, bool encrpytedHeaders);

// writes into the file local header - without Extra data
// puts the new offset to the file data to the file entry
// in case of error can put INVALID_DATA_OFFSET into the data offset field of file entry
extern ErrorEnum WriteLocalHeader (FILE*f, FileEntry* pFileEntry, const char* szRelativePath, bool encrypt);

// conversion routines for the date/time fields used in Zip
extern ZipFile::ushort DOSDate(tm*);
extern ZipFile::ushort DOSTime(tm*);

extern const char* DOSTimeCStr(ZipFile::ushort nTime);
extern const char* DOSDateCStr(ZipFile::ushort nTime);

struct DirHeader;
// this structure represents a subdirectory descriptor in the directory record.
// it points to the actual directory info (list of its subdirs and files), as well 
// as on its name
struct DirEntry
{
	ZipFile::ulong  nDirHeaderOffset; // offset, in bytes, relative to this object, of the actual directory record header
	ZipFile::ulong nNameOffset; // offset of the dir name in the name pool of the parent directory
	// returns the name of this directory, given the pointer to the name pool of hte parent directory
	const char* GetName(const char* pNamePool)const
	{
		return pNamePool + nNameOffset;
	}

	// returns the pointer to the actual directory record. 
	// call this function only for the actual structure instance contained in a directory record and
	// followed by the other directory records
	const DirHeader* GetDirectory ()	const
	{
		return (const DirHeader*)(((const char*)this) + nDirHeaderOffset);
	}
	DirHeader* GetDirectory ()	
	{
		return (DirHeader*)(((char*)this) + nDirHeaderOffset);
	}
};

// this is the head of the directory record
// the name pool follows straight the directory and file entries.
struct DirHeader
{
	ZipFile::ushort numDirs; // number of directory entries - DirEntry structures
	ZipFile::ushort numFiles; // number of file entries - FileEntry structures

	// returns the pointer to the name pool that follows this object
	// you can only call this method for the structure instance actually followed by the dir record
	const char* GetNamePool()const
	{
		return ((char*)(this+1)) + (size_t)this->numDirs * sizeof(DirEntry) + (size_t)this->numFiles * sizeof(FileEntry);
	}
	char* GetNamePool()
	{
		return ((char*)(this+1)) + (size_t)this->numDirs * sizeof(DirEntry) + (size_t)this->numFiles * sizeof(FileEntry);
	}

	// returns the pointer to the i-th directory
	// call this only for the actual instance of the structure at the head of dir record
	const DirEntry* GetSubdirEntry(unsigned i)const
	{
		assert (i < numDirs);
		return reinterpret_cast<const DirEntry*>(this+1) + i;
	}
	DirEntry* GetSubdirEntry(unsigned i)
	{
		assert (i < numDirs);
		return reinterpret_cast<DirEntry*>(this+1) + i;
	}

	// returns the pointer to the i-th file
	// call this only for the actual instance of the structure at the head of dir record
	const FileEntry* GetFileEntry (unsigned i)const 
	{
		assert (i < numFiles);
		return reinterpret_cast<const FileEntry*>(reinterpret_cast<const DirEntry*>(this+1) + numDirs) + i;
	}
	FileEntry* GetFileEntry (unsigned i)
	{
		assert (i < numFiles);
		return reinterpret_cast<FileEntry*>(reinterpret_cast<DirEntry*>(this+1) + numDirs) + i;
	}

	// finds the subdirectory entry by the name, using the names from the name pool
	// assumes: all directories are sorted in alphabetical order.
	// case-sensitive (must be lower-case if case-insensitive search in Win32 is performed)
	DirEntry* FindSubdirEntry(const char* szName);

	// finds the file entry by the name, using the names from the name pool
	// assumes: all directories are sorted in alphabetical order.
	// case-sensitive (must be lower-case if case-insensitive search in Win32 is performed)
	FileEntry* FindFileEntry(const char* szName);

};

// this is the sorting predicate for directory entries
struct DirEntrySortPred
{
	explicit DirEntrySortPred (const char* pNamePool)
		: m_pNamePool(pNamePool)
	{
	}

	bool operator () (const FileEntry& left, const FileEntry& right)const
	{
		return strcmp(left.GetName(m_pNamePool), right.GetName(m_pNamePool)) < 0;
	}

	bool operator () (const FileEntry& left, const char* szRight)const
	{
		return strcmp(left.GetName(m_pNamePool), szRight) < 0;
	}

	bool operator () (const char* szLeft, const FileEntry& right)const
	{
		return strcmp(szLeft, right.GetName(m_pNamePool)) < 0;
	}

	bool operator () (const DirEntry& left, const DirEntry& right)const
	{
		return strcmp(left.GetName(m_pNamePool), right.GetName(m_pNamePool)) < 0;
	}

	bool operator () (const DirEntry& left, const char* szName)const
	{
		return strcmp(left.GetName(m_pNamePool), szName) < 0;
	}

	bool operator () (const char* szLeft, const DirEntry& right)const
	{
		return strcmp(szLeft, right.GetName(m_pNamePool)) < 0;
	}

	const char *m_pNamePool;
};

inline void tolower (string& str)
{
	for (size_t i = 0; i < str.length(); ++i)
		const_cast<char&>(str[i]) = ::tolower(str[i]);
}

void Encrypt(char* buffer, size_t size, const EncryptionKey& key);
void Decrypt(char* buffer, size_t size, const EncryptionKey& key);

}

#endif
