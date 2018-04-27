// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Declarations of the class used to parse and cache Zipped directory.
// This class is actually an auto-pointer to the instance of the cache, so it can 
// be easily passed by value.
// The cache instance contains the optimized for memory usage and fast search tree
// of the files/directories inside the zip; each file has a descriptor with the
// info about where its compressed data lies within the file.

#ifndef _ZIP_DIR_CACHE_HDR_
#define _ZIP_DIR_CACHE_HDR_


/////////////////////////////////////////////////////////////
// THe Zip Dir uses a special memory layout for keeping the structure of zip file.
// This layout is optimized for small memory footprint (for big zip files)
// and quick binary-search access to the individual files.
//
// The serialized layout consists of a number of directory records.
// Each directory record starts with the DirHeader structure, then
// it has an array of DirEntry structures (sorted by name),
// array of FileEntry structures (sorted by name) and then
// the pool of names, followed by pad bytes to align the whole directory
// record on 4-byte boundray.

namespace ZipDir
{

// this is the header of the instance data allocated dynamically
// it contains the FILE* : it owns it and closes upon destruction
struct Cache
{
	void AddRef() { ++m_nRefCount; }
	void Release() { if (--m_nRefCount <= 0) Delete(); }
	int  NumRefs() const { return m_nRefCount; }

	// looks for the given file record in the Central Directory. If there's none, returns NULL.
	// if there is some, returns the pointer to it.
	// the Path must be the relative path to the file inside the Zip
	// if the file handle is passed, it will be used to find the file data offset, if one hasn't been initialized yet
	// if bFull is true, then the full information about the file is returned (the offset to the data may be unknown at this point)-
	// if needed, the file is accessed and the information is loaded
	FileEntry* FindFile (const char* szPath, bool bFullInfo = false);

	// loads the given file into the pCompressed buffer (the actual compressed data)
	// if the pUncompressed buffer is supplied, uncompresses the data there
	// buffers must have enough memory allocated, according to the info in the FileEntry
	// NOTE: there's no need to decompress if the method is 0 (store)
	// returns 0 if successful or error code if couldn't do something
	ErrorEnum ReadFile (FileEntry* pFileEntry, void* pCompressed, void* pUncompressed);

	// loads and unpacks the file into a newly created buffer (that must be subsequently freed with
	// Free()) Returns NULL if failed
	void* AllocAndReadFile (FileEntry* pFileEntry);

	// frees the memory block that was previously allocated by AllocAndReadFile
	void Free (void*);

	// refreshes information about the given file entry into this file entry
	ErrorEnum Refresh (FileEntry* pFileEntry);

	// Return FileEntity data offset inside zip file.
	uint32 GetFileDataOffset(FileEntry* pFileEntry);


	// returns the root directory record;
	// through this directory record, user can traverse the whole tree
	const DirHeader* GetRoot() const
	{
		return reinterpret_cast<const DirHeader*>(this + 1);
	}
	DirHeader* GetRoot()
	{
		return reinterpret_cast<DirHeader*>(this + 1);
	}

	// returns the size of memory occupied by the instance referred to by this cache
	// must be exact, because it's used by CacheRW to reallocate this cache
	size_t GetSize()const;

	// QUICK check to determine whether the file entry belongs to this object
	bool IsOwnerOf (const FileEntry*pFileEntry)const;

	// returns the string - path to the zip file from which this object was constructed.
	// this will be "" if the object was constructed with a factory that wasn't created with FLAGS_MEMORIZE_ZIP_PATH
	const char* GetFilePath()const
	{
		return ((const char*)(this+1)) + m_nZipPathOffset;
	}

	friend class CacheFactory; // the factory class creates instances of this class
	friend class CacheRW; // the Read-Write 2-way cache can modify this cache directly during write operations
protected:
	volatile signed int m_nRefCount; // the reference count
	FILE* m_pFile; // the opened file

	// the size of the serialized data following this instance (not including the extra fields after the serialized tree data)
	size_t m_nDataSize;
	// the offset to the path/name of the zip file relative to (char*)(this+1) pointer in bytes
	size_t m_nZipPathOffset;

	// tells if encryption used for zip-file
	EncryptionKey m_encryptionKey;
	bool m_bEncryptHeaders;
public:
	// initializes the instance structure
	static void Construct(ZipDir::Cache* p, FILE* fNew, size_t nDataSize, const EncryptionKey& key);
	void Delete();
private:
	bool ReadCompressedData(char* data, size_t size);

	// the constructor/destructor cannot be called at all - everything will go through the factory class
	Cache() { m_nRefCount = 0; }
	~Cache(){}
};

TYPEDEF_AUTOPTR(Cache);

typedef Cache_AutoPtr CachePtr;

 
}

#endif
