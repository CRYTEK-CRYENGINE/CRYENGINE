// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/smartptr.h>
#include "Util.h"
#include "ZipFileFormat.h"
#include "ZipDirStructures.h"
#include "ZipDirTree.h"
#include "ZipDirList.h"
#include "ZipDirCache.h"
#include "ZipDirCacheRW.h"
#include "ZipDirCacheFactory.h"
#include "ZipDirFindRW.h"

#include "ThreadUtils.h"

#include <zlib.h>  // declaration of Z_OK for ZipRawDecompress

using namespace ZipFile;

enum PackFileStatus
{
	PACKFILE_COMPRESSED,

	PACKFILE_ADDED,
	PACKFILE_UPTODATE,
	PACKFILE_SKIPPED,
	PACKFILE_MISSING,
	PACKFILE_FAILED
};

class PackFilePool;
struct PackFileBatch
{
	PackFilePool* pool;

	int zipMaxSize;
	int sourceMinSize;
	int sourceMaxSize;
	int compressionMethod;
	int compressionLevel;

	PackFileBatch()
	: pool(0)
	, sourceMinSize(0)
	, sourceMaxSize(0)
	, zipMaxSize(0)
	, compressionMethod(0)
	, compressionLevel(0)
	{
	}
};

class PackFilePool;
struct PackFileJob
{
	int index;
	int key;
	PackFileBatch* batch;
	const char* relativePathSrc;
	const char* realFilename;

	unsigned int existingCRC;

	void* compressedData;
	unsigned long compressedSize;
	unsigned long compressedSizePreviously;

	void* uncompressedData;
	unsigned long uncompressedSize;
	unsigned long uncompressedSizePreviously;

	__int64 modTime;
	ZipDir::ErrorEnum zdError;
	PackFileStatus status;

	PackFileJob()
	: index(0)
	, key(0)
	, batch(0)
	, realFilename(0)
	, relativePathSrc(0)
	, existingCRC(0)
	, compressedData(0)
	, compressedSize(0)
	, compressedSizePreviously(0)
	, uncompressedData(0)
	, uncompressedSize(0)
	, uncompressedSizePreviously(0)
	, modTime(0)
	, zdError(ZipDir::ZD_ERROR_NOT_IMPLEMENTED)
	, status(PACKFILE_FAILED)
	{
	}

	void DetachUncompressedData()
	{
		if (uncompressedData && uncompressedData == compressedData)
		{
			compressedData = 0;
			compressedSize = 0;
		}

		uncompressedData = 0;
		uncompressedSize = 0;
	}

	~PackFileJob()
	{
		if (compressedData && compressedData != uncompressedData)
		{
			free(compressedData);
			compressedData = 0;
		}

		if (uncompressedData)
		{
			free(uncompressedData);
			uncompressedData = 0;
		}
	}
};


// ---------------------------------------------------------------------------

static void PackFileFromDisc(PackFileJob* job);

class PackFilePool
{
public:
	PackFilePool(int numFiles, size_t memoryLimit)
	: m_pool(false)
	, m_skip(false)
	, m_awaitedFile(0)
	, m_memoryLimit(memoryLimit)
	, m_allocatedMemory(0)
	{
		m_files.reserve(numFiles);
	}

	~PackFilePool()
	{
	}

	void Submit(int key, const PackFileJob& job)
	{
		PackFileJob* newJob = new PackFileJob(job);

		// index in queue, and custom key for identification
		newJob->index = int(m_files.size());
		newJob->key = key;
		
		m_files.push_back(newJob);
	}

	PackFileJob* WaitForFile(int index)
	{
		while(true)
		{
			{
				ThreadUtils::AutoLock lock(m_filesLock);
				m_awaitedFile = index;
				if (size_t(index) >= m_files.size())
					return 0;
				if (m_files[index])
					return m_files[index];
			}
			Sleep(0);
		}

		assert(0);
		return 0;
	}

	void Start(int numThreads)
	{
		for (size_t i = 0; i < m_files.size(); ++i)
		{
			PackFileJob* job = m_files[i];
			m_files[i] = 0;
			m_pool.Submit(&ProcessFile, job);
		}

		m_pool.Start(numThreads);
	}

	size_t GetJobCount() const
	{
		return m_files.size();
	}

	void SkipPendingFiles()
	{
		m_skip = true;
	}

	void ReleaseFile(int index)
	{
		assert(m_files[index] != 0);
		if (m_files[index])
		{
			if (m_memoryLimit != 0)
			{
				ThreadUtils::AutoLock lock(m_filesLock);

				m_allocatedMemory -= m_files[index]->uncompressedSize;
				m_allocatedMemory -= m_files[index]->compressedSize;
			}

			delete m_files[index];
			m_files[index] = 0;
		}
	}

private:

	// called from non-main thread
	static void ProcessFile(PackFileJob* job)
	{
		PackFilePool* self = job->batch->pool;

		if (!self->m_skip)
		{
			if (self->m_memoryLimit != 0)
			{
				while (true)
				{
					size_t allocatedMemory = 0;
					int awaitedFile = 0;
					{
						ThreadUtils::AutoLock lock(self->m_filesLock);
						allocatedMemory = self->m_allocatedMemory;
						awaitedFile = self->m_awaitedFile;
					}

					if (allocatedMemory > self->m_memoryLimit && job->index > awaitedFile + 1)
						Sleep(10); // give time to main thread to write data to file
					else
						break;
				}
			}

			PackFileFromDisc(job);
		}

		self->FileCompleted(job);
	}

	// called from non-main thread
	void FileCompleted(PackFileJob* job)
	{
		ThreadUtils::AutoLock lock(m_filesLock);

		assert(job);
		assert(job->index < m_files.size());
		assert(m_files[job->index] == 0);
		m_files[job->index] = job;

		if (m_memoryLimit != 0)
		{
			m_allocatedMemory += job->uncompressedSize;
			m_allocatedMemory += job->compressedSize;
		}
	}

	size_t m_memoryLimit;

	ThreadUtils::CriticalSection m_filesLock;
	std::vector<PackFileJob*> m_files;
	int m_awaitedFile;
	size_t m_allocatedMemory;
	bool m_skip;

	ThreadUtils::SimpleThreadPool m_pool;
};

//////////////////////////////////////////////////////////////////////////
static size_t AlignTo(size_t offset, size_t alignment)
{
	const size_t remainder = offset % alignment;
	return remainder ? offset + alignment - remainder : offset;
}
//////////////////////////////////////////////////////////////////////////
// Calculates new offset of the header to make sure that following data are
// aligned properly
static size_t CalculateAlignedHeaderOffset(const char* fileName, size_t currentOffset, size_t alignment)
{
	// Since file should start from header
	if (currentOffset == 0)
	{
		return 0;
	}

	// Local header is followed by filename
	const size_t totalHeaderSize = sizeof(LocalFileHeader) + strlen(fileName);

	// Align end of the header
	const size_t dataOffset = AlignTo(currentOffset + totalHeaderSize, alignment);

	return dataOffset - totalHeaderSize;
}

//////////////////////////////////////////////////////////////////////////
ZipDir::CacheRW::CacheRW(bool encryptHeaders, const EncryptionKey& encryptionKey)
: m_pFile (NULL)
, m_nFlags (0)
, m_lCDROffset (0)
, m_fileAlignment (1)
, m_bEncryptedHeaders(encryptHeaders)
, m_bHeadersEncryptedOnClose(encryptHeaders)
, m_encryptionKey(encryptionKey)
{

	m_nRefCount = 0;
}
//////////////////////////////////////////////////////////////////////////
ZipDir::CacheRW::~CacheRW()
{
	Close();
}
//////////////////////////////////////////////////////////////////////////
void ZipDir::CacheRW::AddRef()
{
	++m_nRefCount;
}

//////////////////////////////////////////////////////////////////////////
void ZipDir::CacheRW::Release()
{
	if (--m_nRefCount <= 0)
		delete this;
}

void ZipDir::CacheRW::Close()
{
	if (m_pFile)
	{
		if (!(m_nFlags & FLAGS_READ_ONLY))
		{
			if ((m_nFlags & FLAGS_UNCOMPACTED) && !(m_nFlags&FLAGS_DONT_COMPACT))
			{
				if (!RelinkZip())
					WriteCDR();
			}
			else
			if (m_nFlags & FLAGS_CDR_DIRTY)
				WriteCDR();
		}

		if(m_pFile)						// RelinkZip() might have closed the file
			fclose (m_pFile);

		m_pFile = NULL;
	}
	m_treeDir.Clear();
}

//////////////////////////////////////////////////////////////////////////
char* ZipDir::CacheRW::UnifyPath( char *const str, const char *pPath )
{
	assert(str);
	const char *src = pPath;
	char *trg = str;
	while (*src)
	{
		if (*src != '/')
		{
			*trg++ = ::tolower(*src++);
		}
		else
		{
			*trg++ = '\\';
			src++;
		}
	}
	*trg = 0;
	return str;
}

//////////////////////////////////////////////////////////////////////////
char* ZipDir::CacheRW::ToUnixPath( char *const str, const char *pPath )
{
	assert(str);
	const char *src = pPath;
	char *trg = str;
	while (*src)
	{
		if (*src != '/')
		{
			*trg++ = *src++;
		}
		else
		{
			*trg++ = '\\';
			src++;
		}
	}
	*trg = 0;
	return str;
}

//////////////////////////////////////////////////////////////////////////
char* ZipDir::CacheRW::AllocPath( const char *pPath )
{
	char str[_MAX_PATH];
	char *temp = ToUnixPath(str, pPath);
	temp = m_tempStringPool.Append( temp,strlen(temp) );
	return temp;
}

static void PackFileFromMemory(PackFileJob* job)
{
	if (job->existingCRC != 0)
	{
		unsigned int crcCode = (unsigned int)crc32(0, (unsigned char*)job->uncompressedData, job->uncompressedSize);
		if (crcCode == job->existingCRC)
		{
			job->compressedData = 0;
			job->compressedSize = 0;
			job->status = PACKFILE_UPTODATE;
			job->zdError = ZipDir::ZD_ERROR_SUCCESS;
			// This file with same data already in pak, skip it.
			return;
		}
	}

	switch (job->batch->compressionMethod)
	{
	case METHOD_DEFLATE_AND_ENCRYPT:
	case METHOD_DEFLATE:
	{
		// allocate memory for compression. Min is nSize * 1.001 + 12
		if (job->uncompressedSize > 0)
		{
			job->compressedSize = job->uncompressedSize + (job->uncompressedSize >> 3) + 32;
			job->compressedData = malloc(job->compressedSize);
			int error = ZipDir::ZipRawCompress(job->uncompressedData, &job->compressedSize, job->compressedData, job->uncompressedSize, job->batch->compressionLevel);
			if (error == Z_OK)
			{
				job->status = PACKFILE_COMPRESSED;
				job->zdError = ZipDir::ZD_ERROR_SUCCESS;
			}
			else
			{
				job->status = PACKFILE_FAILED;
				job->zdError = ZipDir::ZD_ERROR_ZLIB_FAILED;
			}
		}
		else
		{
			job->status = PACKFILE_COMPRESSED;
			job->zdError = ZipDir::ZD_ERROR_SUCCESS;

			job->compressedSize = 0;
			job->compressedData = 0;
		}
		break;
	}
	case METHOD_STORE:
		job->compressedData = job->uncompressedData;
		job->compressedSize = job->uncompressedSize;
		job->status = PACKFILE_COMPRESSED;
		job->zdError = ZipDir::ZD_ERROR_SUCCESS;
		break;

	default:
		job->status = PACKFILE_FAILED;
		job->zdError = ZipDir::ZD_ERROR_UNSUPPORTED;
		break;
	}
}

bool ZipDir::CacheRW::WriteCompressedData(const char* data, size_t size, bool encrypt, FILE* file)
{
	if (size == 0)
	{
		return true;
	}

	std::vector<char> buffer;
	if (encrypt)
	{
		buffer.resize(size);
		memcpy(&buffer[0], data, size);
		ZipDir::Encrypt(&buffer[0], size, m_encryptionKey);
		data = &buffer[0];
	}

	// Danny - writing a single large chunk (more than 6MB?) causes
	// Windows fwrite to (silently?!) fail. So we're writing data
	// in small chunks.
	while (size > 0)
	{
		const size_t sizeToWrite = Util::getMin(size, size_t(1024 * 1024));
		if (fwrite(data, sizeToWrite, 1, file) != 1)
		{
			return false;
		}
		data += sizeToWrite;
		size -= sizeToWrite;
	}

	return true;
}

static bool WriteRandomData(FILE* file, size_t size)
{
	if (size == 0)
	{
		return true;
	}

	const size_t bufferSize = Util::getMin(size, size_t(1024 * 1024));
	std::vector<char> buffer(bufferSize);

	while (size > 0)
	{
		const size_t sizeToWrite = Util::getMin(size, bufferSize);

		for (size_t i = 0; i < sizeToWrite; ++i)
		{
			buffer[i] = rand() & 0xff;
		}

		if (fwrite(&buffer[0], sizeToWrite, 1, file) != 1)
		{
			return false;
		}

		size -= sizeToWrite;
	}
	
	return true;
}

bool ZipDir::CacheRW::WriteNullData(size_t size)
{
	if (size == 0)
	{
		return true;
	}

	const size_t bufferSize = Util::getMin(size, size_t(1024 * 1024));
	std::vector<char> buffer(bufferSize, 0);

	while (size > 0)
	{
		const size_t sizeToWrite = Util::getMin(size, bufferSize);

		if (fwrite(&buffer[0], sizeToWrite, 1, m_pFile) != 1)
		{
			return false;
		}

		size -= sizeToWrite;
	}
	
	return true;
}

void ZipDir::CacheRW::StorePackedFile(PackFileJob* job)
{
	if (job->batch->zipMaxSize > 0 && GetTotalFileSize() > job->batch->zipMaxSize)
	{
		job->status = PACKFILE_SKIPPED;
		job->zdError = ZipDir::ZD_ERROR_SUCCESS;
		return;
	}
	
	job->status = PACKFILE_FAILED;

	char str[_MAX_PATH];
	char* relativePath = UnifyPath(str, job->relativePathSrc);

	// create or find the file entry.. this object will rollback (delete the object
	// if the operation fails) if needed.
	FileEntryTransactionAdd pFileEntry(this, AllocPath(job->relativePathSrc), AllocPath(relativePath));

	if (!pFileEntry)
	{
		job->zdError = ZipDir::ZD_ERROR_INVALID_PATH;
		return;
	}

	pFileEntry->OnNewFileData(job->uncompressedData, job->uncompressedSize,
														job->compressedSize, job->batch->compressionMethod, false);
	pFileEntry->SetFromFileTimeNTFS(job->modTime);

	// since we changed the time, we'll have to update CDR
	m_nFlags |= FLAGS_CDR_DIRTY;

	// the new CDR position, if the operation completes successfully
	unsigned lNewCDROffset = m_lCDROffset;

	if (pFileEntry->IsInitialized())
	{
		// this file entry is already allocated in CDR

		// check if the new compressed data fits into the old place
		unsigned nFreeSpace = pFileEntry->nEOFOffset - pFileEntry->nFileHeaderOffset - (unsigned)sizeof(ZipFile::LocalFileHeader) - (unsigned)strlen(relativePath);

		if (nFreeSpace != job->compressedSize)
			m_nFlags |= FLAGS_UNCOMPACTED;

		if (nFreeSpace >= job->compressedSize)
		{
			// and we can just override the compressed data in the file
			ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, job->relativePathSrc, m_bEncryptedHeaders);
			if (e != ZipDir::ZD_ERROR_SUCCESS)
			{
				job->zdError = e;
				return;
			}
		}
		else
		{
			// we need to write the file anew - in place of current CDR
			pFileEntry->nFileHeaderOffset = CalculateAlignedHeaderOffset(job->relativePathSrc, m_lCDROffset, m_fileAlignment);
			ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, job->relativePathSrc, m_bEncryptedHeaders);
			lNewCDROffset = pFileEntry->nEOFOffset;
			if (e != ZipDir::ZD_ERROR_SUCCESS)
			{
				job->zdError = e;
				return;
			}
		}
	}
	else
	{
		pFileEntry->nFileHeaderOffset = CalculateAlignedHeaderOffset(job->relativePathSrc, m_lCDROffset, m_fileAlignment);
		ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, job->relativePathSrc, m_bEncryptedHeaders);
		if (e != ZipDir::ZD_ERROR_SUCCESS)
		{
			job->zdError = e;
			return;
		}

		lNewCDROffset = pFileEntry->nFileDataOffset + job->compressedSize;

		m_nFlags |= FLAGS_CDR_DIRTY;
	}

	// now we have the fresh local header and data offset

#if CRY_PLATFORM_WINDOWS
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileDataOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, pFileEntry->nFileDataOffset, SEEK_SET)!=0)
#endif
	{
		job->zdError = ZD_ERROR_IO_FAILED;
		return;
	}

	const bool encrypt = pFileEntry->nMethod == METHOD_DEFLATE_AND_ENCRYPT;

	if (!WriteCompressedData((char*)job->compressedData, job->compressedSize, encrypt, m_pFile))
	{
		job->zdError = ZD_ERROR_IO_FAILED;
		return;
	}

	// since we wrote the file successfully, update the new CDR position
	m_lCDROffset = lNewCDROffset;
	pFileEntry.Commit();

	job->status = PACKFILE_ADDED;
	job->zdError = ZD_ERROR_SUCCESS;
}

// Adds a new file to the zip or update an existing one
// adds a directory (creates several nested directories if needed)
ZipDir::ErrorEnum ZipDir::CacheRW::UpdateFile (const char* szRelativePathSrc, void* pUncompressed, unsigned nSize,
																							 unsigned nCompressionMethod, int nCompressionLevel, __time64_t modTime)
{
	char str[_MAX_PATH];
	char *szRelativePath = UnifyPath(str, szRelativePathSrc);


	PackFileBatch batch;
	batch.compressionMethod = nCompressionMethod;
	batch.compressionLevel = nCompressionLevel;

	PackFileJob job;
	job.relativePathSrc = szRelativePathSrc;
	job.modTime = modTime;
	job.uncompressedData = pUncompressed;
	job.uncompressedSize = nSize;
	job.batch = &batch;

	// crc will be used to check if this file need to be updated at all
	ZipDir::FileEntry* entry = FindFile(szRelativePath);
	if (entry)
		job.existingCRC = entry->desc.lCRC32;

	PackFileFromMemory(&job);

	switch (job.status)
	{
    case PACKFILE_SKIPPED :
    case PACKFILE_MISSING :
    case PACKFILE_FAILED :
      return ZD_ERROR_IO_FAILED;
	}

	StorePackedFile(&job);
	job.DetachUncompressedData();
	return job.zdError;
}

static FILETIME GetFileWriteTimeAndSize( uint64* fileSize, const char *filename )
{
	// Warning: FindFirstFile on NTFS may report file size that
	// is not up-to-date with the actual file content.
	// http://blogs.msdn.com/b/oldnewthing/archive/2011/12/26/10251026.aspx
	
	FILETIME fileTime;

	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind = FindFirstFileA( filename, &FindFileData );

	if (hFind == INVALID_HANDLE_VALUE)
	{
		fileTime.dwLowDateTime = 0;
		fileTime.dwHighDateTime = 0;
		if (fileSize)
		{
			*fileSize = 0;
		}
	}
	else
	{
		fileTime.dwLowDateTime = FindFileData.ftLastWriteTime.dwLowDateTime;
		fileTime.dwHighDateTime = FindFileData.ftLastWriteTime.dwHighDateTime;
		if (fileSize)
		{
			*fileSize = (uint64(FindFileData.nFileSizeHigh) << 32) + FindFileData.nFileSizeLow;
		}
		FindClose(hFind);
	}

	return fileTime;
}

static void PackFileFromDisc(PackFileJob* job)
{
	const FILETIME ft = GetFileWriteTimeAndSize(0, job->realFilename);
	LARGE_INTEGER lt;
	lt.HighPart = ft.dwHighDateTime;
	lt.LowPart = ft.dwLowDateTime;
	job->modTime = lt.QuadPart;

		FILE *f = fopen(job->realFilename, "rb");
		if (!f)
		{
			job->status = PACKFILE_FAILED;
			job->zdError = ZipDir::ZD_ERROR_FILE_NOT_FOUND;
			return;
		}

		fseek(f, 0, SEEK_END);
		size_t fileSize = (size_t)ftell(f);

		if (fileSize < job->batch->sourceMinSize || (job->batch->sourceMaxSize > 0 && fileSize > job->batch->sourceMaxSize))
		{
			fclose(f);

			job->status = PACKFILE_SKIPPED;
			job->zdError = ZipDir::ZD_ERROR_SUCCESS;
			return;
		}

		job->uncompressedData = malloc(fileSize);

		fseek(f, 0, SEEK_SET);
		if (fread(job->uncompressedData, 1, fileSize, f) != fileSize)
		{
			free(job->uncompressedData);
			job->uncompressedData = 0;
			fclose(f);

			job->status = PACKFILE_FAILED;
			job->zdError = ZipDir::ZD_ERROR_IO_FAILED;
			return;
		}
		fclose(f);
		job->uncompressedSize = fileSize;

	PackFileFromMemory(job);
}

bool ZipDir::CacheRW::UpdateMultipleFiles(const char** realFilenames, const char** filenamesInZip, size_t fileCount,
																					int compressionLevel, bool encryptContent, size_t zipMaxSize, int sourceMinSize, int sourceMaxSize,
																					int numExtraThreads, ZipDir::IReporter* reporter, ZipDir::ISplitter* splitter)
{
	int compressionMethod = METHOD_DEFLATE;
	if (encryptContent)
		compressionMethod = METHOD_DEFLATE_AND_ENCRYPT;
	else if (compressionLevel == 0)
		compressionMethod = METHOD_STORE;

	uint64 totalSize = 0;

	clock_t startTime = clock();
	
	PackFileBatch batch;
	batch.compressionLevel = compressionLevel;
	batch.compressionMethod = compressionMethod;
	batch.pool = 0;
	batch.sourceMinSize = sourceMinSize;
	batch.sourceMaxSize = sourceMaxSize;
	batch.zipMaxSize = zipMaxSize;

	if (numExtraThreads == 0)
	{
		for (int i = 0; i < fileCount; ++i)
		{
			const char* realFilename = realFilenames[i];
			const char* filenameInZip = filenamesInZip[i];

			PackFileJob job;

			job.key = i;
			job.relativePathSrc = filenameInZip;
			job.realFilename = realFilename;
			job.batch = &batch;

			{
				// crc will be used to check if this file need to be updated at all
				ZipDir::FileEntry* entry = FindFile(filenameInZip);
				if (entry)
				{
					uint64 fileSize = 0;

					const FILETIME ft = GetFileWriteTimeAndSize(&fileSize, realFilename);
					LARGE_INTEGER lt;

					lt.HighPart = ft.dwHighDateTime;
					lt.LowPart = ft.dwLowDateTime;
					job.modTime = lt.QuadPart;
					job.existingCRC = entry->desc.lCRC32;
					job.compressedSizePreviously = entry->desc.lSizeCompressed;
					job.uncompressedSizePreviously = entry->desc.lSizeUncompressed;

					// Check if file with the same name, timestamp and size already exists in pak.
					if (entry->CompareFileTimeNTFS(job.modTime) && fileSize == entry->desc.lSizeUncompressed)
					{
						if (reporter)
							reporter->ReportUpToDate(filenameInZip);
						continue;
					}
				}
			}

			PackFileFromDisc(&job);

			if (job.status == PACKFILE_COMPRESSED)
			{
				if (splitter != NULL)
				{
					size_t dsk = GetTotalFileSizeOnDiskSoFar();
					size_t bse = 0;
					size_t add = 0;
					size_t sub = 0;

					bse += sizeof(ZipFile::CDRFileHeader)   + strlen(job.relativePathSrc);
					bse += sizeof(ZipFile::LocalFileHeader) + strlen(job.relativePathSrc);

					if (job.compressedSize)
						add += bse + job.compressedSize;
					if (job.compressedSizePreviously)
						sub += bse + job.compressedSizePreviously;

					if (splitter->CheckWriteLimit(dsk, add, sub))
					{
						splitter->SetLastFile(dsk, add, sub, job.key - 1);
						break;
					}
				}

				StorePackedFile(&job);
			}

			switch (job.status)
			{
			case PACKFILE_ADDED:
				if (reporter)
					reporter->ReportAdded(filenameInZip);

				totalSize += job.uncompressedSize;
				break;
			case PACKFILE_MISSING:
				if (reporter)
					reporter->ReportMissing(realFilename);
				break;
			case PACKFILE_UPTODATE:
				if (reporter)
					reporter->ReportUpToDate(realFilename);
				break;
			case PACKFILE_SKIPPED:
				if (reporter)
					reporter->ReportSkipped(realFilename);
				break;
			default:
				if (reporter)
					reporter->ReportFailed(realFilename, ""); // TODO reason
				continue;
			};
		}

	}
	else
	{
		const size_t memoryLimit = 1024 * 1024 * 1024; // prevents threads from generating more than 1GB of data
		PackFilePool pool(fileCount, memoryLimit);
		batch.pool = &pool;

		for (int i = 0; i < fileCount; ++i)
		{
			const char* realFilename = realFilenames[i];
			const char* filenameInZip = filenamesInZip[i];

			PackFileJob job;

			job.relativePathSrc = filenameInZip;
			job.realFilename = realFilename;
			job.batch = &batch;

			{
				// crc will be used to check if this file need to be updated at all
				ZipDir::FileEntry* entry = FindFile(filenameInZip);
				if (entry)
				{
					uint64 fileSize = 0;
					const FILETIME ft = GetFileWriteTimeAndSize(&fileSize, realFilename);
					LARGE_INTEGER lt;
					lt.HighPart = ft.dwHighDateTime;
					lt.LowPart = ft.dwLowDateTime;
					job.modTime = lt.QuadPart;
					job.existingCRC = entry->desc.lCRC32;
					job.compressedSizePreviously = entry->desc.lSizeCompressed;
					job.uncompressedSizePreviously = entry->desc.lSizeUncompressed;

					// Check if file with the same name, timestamp and size already exists in pak.
					if (entry->CompareFileTimeNTFS(job.modTime) && fileSize == entry->desc.lSizeUncompressed)
					{
						if (reporter)
							reporter->ReportUpToDate(filenameInZip);
						continue;
					}
				}
			}

			pool.Submit(i, job);
		}

		// Get the number of submitted jobs, which is at most
		// as large as the largest successfully submitted file-index.
		// Any number of files can be skipped for submittion.
		const int jobCount = pool.GetJobCount();
		if (jobCount == 0)
		{
			return true;
		}

		pool.Start(numExtraThreads);

		for (int i = 0; i < jobCount; ++i)
		{
			PackFileJob* job = pool.WaitForFile(i);
			if (!job)
			{
				assert(job);
				continue;
			}

			if (job->status == PACKFILE_COMPRESSED)
			{
				if (splitter != NULL)
				{
					size_t dsk = GetTotalFileSizeOnDiskSoFar();
					size_t bse = 0;
					size_t add = 0;
					size_t sub = 0;

					bse += sizeof(ZipFile::CDRFileHeader)   + strlen(job->relativePathSrc);
					bse += sizeof(ZipFile::LocalFileHeader) + strlen(job->relativePathSrc);

					if (job->compressedSize)
						add += bse + job->compressedSize;
					if (job->compressedSizePreviously)
						sub += bse + job->compressedSizePreviously;

					if (splitter->CheckWriteLimit(dsk, add, sub))
					{
						splitter->SetLastFile(dsk, add, sub, job->key - 1);

						// deplete the pool before leaving the loop
						pool.SkipPendingFiles();
						for (; i < jobCount; ++i)
						{
							pool.WaitForFile(i);
							pool.ReleaseFile(i);
						}

						break;
					}
				}

				StorePackedFile(job);
			}

			switch (job->status)
			{
			case PACKFILE_ADDED:
				if (reporter)
					reporter->ReportAdded(job->relativePathSrc);

				totalSize += job->uncompressedSize;
				break;
			case PACKFILE_MISSING:
				if (reporter)
					reporter->ReportMissing(job->realFilename);
				break;
			case PACKFILE_UPTODATE:
				if (reporter)
					reporter->ReportUpToDate(job->realFilename);
				break;
			case PACKFILE_SKIPPED:
				if (reporter)
					reporter->ReportSkipped(job->realFilename);
				break;
			default:
				if (reporter)
					reporter->ReportFailed(job->realFilename, ""); // TODO reason
				continue;
			};

			pool.ReleaseFile(i);
		}
	}

	clock_t endTime = clock();
	double timeSeconds = double(endTime - startTime) / CLOCKS_PER_SEC;
	double speed = (endTime - startTime) == 0 ? 0.0 : double(totalSize) / timeSeconds;

	if (reporter)
		reporter->ReportSpeed(speed);

	return true;
}


//   Adds a new file to the zip or update an existing one if it is not compressed - just stored  - start a big file
ZipDir::ErrorEnum ZipDir::CacheRW::StartContinuousFileUpdate( const char* szRelativePathSrc, unsigned nSize )
{
	char str[_MAX_PATH];
	char *szRelativePath = UnifyPath(str, szRelativePathSrc);

	SmartPtr pBufferDestroyer;

	// create or find the file entry.. this object will rollback (delete the object
	// if the operation fails) if needed.
	FileEntryTransactionAdd pFileEntry(this, AllocPath(szRelativePathSrc), AllocPath(szRelativePath));

	if (!pFileEntry)
		return ZD_ERROR_INVALID_PATH;

	pFileEntry->OnNewFileData (NULL, nSize, nSize, METHOD_STORE, false );
	// since we changed the time, we'll have to update CDR
	m_nFlags |= FLAGS_CDR_DIRTY;

	// the new CDR position, if the operation completes successfully
	unsigned lNewCDROffset = m_lCDROffset;
	if (pFileEntry->IsInitialized())
	{
		// check if the new compressed data fits into the old place
		unsigned nFreeSpace = pFileEntry->nEOFOffset - pFileEntry->nFileHeaderOffset - (unsigned)sizeof(ZipFile::LocalFileHeader) - (unsigned)strlen(szRelativePath);

		if (nFreeSpace != nSize)
			m_nFlags |= FLAGS_UNCOMPACTED;

		if (nFreeSpace >= nSize)
		{
			// and we can just override the compressed data in the file
			ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePathSrc, m_bEncryptedHeaders);
			if (e != ZD_ERROR_SUCCESS)
				return e;
		}
		else
		{
			// we need to write the file anew - in place of current CDR
			pFileEntry->nFileHeaderOffset = CalculateAlignedHeaderOffset(szRelativePathSrc, m_lCDROffset, m_fileAlignment);
			ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePathSrc, m_bEncryptedHeaders);
			lNewCDROffset = pFileEntry->nEOFOffset;
			if (e != ZD_ERROR_SUCCESS)
				return e;
		}
	}
	else
	{
		pFileEntry->nFileHeaderOffset = CalculateAlignedHeaderOffset(szRelativePathSrc, m_lCDROffset, m_fileAlignment);
		ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePathSrc, m_bEncryptedHeaders);
		if (e != ZD_ERROR_SUCCESS)
			return e;

		lNewCDROffset = pFileEntry->nFileDataOffset + nSize;

		m_nFlags |= FLAGS_CDR_DIRTY;
	}

#if CRY_PLATFORM_WINDOWS
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileDataOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, pFileEntry->nFileDataOffset, SEEK_SET)!=0)
#endif
	{
		return ZD_ERROR_IO_FAILED;
	}

	if (!WriteNullData(nSize))
	{
		return ZD_ERROR_IO_FAILED;
	}

	pFileEntry->nEOFOffset = pFileEntry->nFileDataOffset;

	// since we wrote the file successfully, update the new CDR position
	m_lCDROffset = lNewCDROffset;
	pFileEntry.Commit();

	return ZD_ERROR_SUCCESS;
}

// Adds a new file to the zip or update an existing's segment if it is not compressed - just stored 
// adds a directory (creates several nested directories if needed)
ZipDir::ErrorEnum ZipDir::CacheRW::UpdateFileContinuousSegment (const char* szRelativePathSrc, unsigned nSize, void* pUncompressed, unsigned nSegmentSize, unsigned nOverwriteSeekPos )
{
	char str[_MAX_PATH];
	char *szRelativePath = UnifyPath(str, szRelativePathSrc);

	SmartPtr pBufferDestroyer;

	// create or find the file entry.. this object will rollback (delete the object
	// if the operation fails) if needed.
	FileEntryTransactionAdd pFileEntry(this, AllocPath(szRelativePathSrc), AllocPath(szRelativePath));

	if (!pFileEntry)
		return ZD_ERROR_INVALID_PATH;

	pFileEntry->OnNewFileData (pUncompressed, nSegmentSize, nSegmentSize, METHOD_STORE, true);
	// since we changed the time, we'll have to update CDR
	m_nFlags |= FLAGS_CDR_DIRTY;

	// this file entry is already allocated in CDR
	unsigned lSegmentOffset = pFileEntry->nEOFOffset;

#if CRY_PLATFORM_WINDOWS
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileHeaderOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, pFileEntry->nFileHeaderOffset, SEEK_SET)!=0)
#endif
		return ZD_ERROR_IO_FAILED;

	// and we can just override the compressed data in the file
	ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath, m_bEncryptedHeaders);
	if (e != ZD_ERROR_SUCCESS)
		return e;

	if(nOverwriteSeekPos!=0xffffffff)
		lSegmentOffset = pFileEntry->nFileDataOffset + nOverwriteSeekPos;

	// now we have the fresh local header and data offset
#if CRY_PLATFORM_WINDOWS
	if (_fseeki64 (m_pFile, (__int64)lSegmentOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, lSegmentOffset, SEEK_SET)!=0)
#endif
		return ZD_ERROR_IO_FAILED;

	const bool encrypt = false; // encryption is not supported for continous updates
	if (!WriteCompressedData((char*)pUncompressed, nSegmentSize, encrypt, m_pFile))
	{
		return ZD_ERROR_IO_FAILED;
	}

	if(nOverwriteSeekPos==0xffffffff)
		pFileEntry->nEOFOffset = lSegmentOffset + nSegmentSize;

	// since we wrote the file successfully, update CDR
	pFileEntry.Commit();
	return ZD_ERROR_SUCCESS;
}


ZipDir::ErrorEnum ZipDir::CacheRW::UpdateFileCRC (const char* szRelativePathSrc, unsigned dwCRC32 )
{
	char str[_MAX_PATH];
	char *szRelativePath = UnifyPath(str, szRelativePathSrc);

	SmartPtr pBufferDestroyer;

	// create or find the file entry.. this object will rollback (delete the object
	// if the operation fails) if needed.
	FileEntryTransactionAdd pFileEntry(this, AllocPath(szRelativePathSrc), AllocPath(szRelativePath));

	if (!pFileEntry)
		return ZD_ERROR_INVALID_PATH;

	// since we changed the time, we'll have to update CDR
	m_nFlags |= FLAGS_CDR_DIRTY;

	pFileEntry->desc.lCRC32=dwCRC32;

#if CRY_PLATFORM_WINDOWS
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileHeaderOffset, SEEK_SET)!=0)
#else
  if (fseek (m_pFile, pFileEntry->nFileHeaderOffset, SEEK_SET)!=0)
#endif
		return ZD_ERROR_IO_FAILED;

	// and we can just override the compressed data in the file
	ErrorEnum e = WriteLocalHeader(m_pFile, pFileEntry, szRelativePath, m_bEncryptedHeaders);
	if (e != ZD_ERROR_SUCCESS)
		return e;

	// since we wrote the file successfully, update
	pFileEntry.Commit();
	return ZD_ERROR_SUCCESS;
}


// deletes the file from the archive
ZipDir::ErrorEnum ZipDir::CacheRW::RemoveFile (const char* szRelativePathSrc)
{
	char str[_MAX_PATH];
	char *szRelativePath = UnifyPath(str, szRelativePathSrc);

	// find the last slash in the path
	const char* pSlash = (std::max)(strrchr(szRelativePath, '/'), strrchr(szRelativePath, '\\'));

	const char* pFileName; // the name of the file to delete

	FileEntryTree* pDir; // the dir from which the subdir will be deleted

	if (pSlash)
	{
		FindDirRW fd (GetRoot());
		// the directory to remove
		pDir = fd.FindExact(string (szRelativePath, pSlash-szRelativePath).c_str());
		if (!pDir)
			return ZD_ERROR_DIR_NOT_FOUND;// there is no such directory
		pFileName = pSlash+1;
	}
	else
	{
		pDir = GetRoot();
		pFileName = szRelativePath;
	}

	ErrorEnum e = pDir->RemoveFile (pFileName);
	if (e == ZD_ERROR_SUCCESS)
		m_nFlags |= FLAGS_UNCOMPACTED|FLAGS_CDR_DIRTY;
	return e;
}


// deletes the directory, with all its descendants (files and subdirs)
ZipDir::ErrorEnum ZipDir::CacheRW::RemoveDir (const char* szRelativePathSrc)
{
	char str[_MAX_PATH];
	char *szRelativePath = UnifyPath(str, szRelativePathSrc);

	// find the last slash in the path
	const char* pSlash = (std::max)(strrchr(szRelativePath, '/'), strrchr(szRelativePath, '\\'));
	
	const char* pDirName; // the name of the dir to delete
	
	FileEntryTree* pDir; // the dir from which the subdir will be deleted

	if (pSlash)
	{
		FindDirRW fd (GetRoot());
		// the directory to remove
		pDir = fd.FindExact(string (szRelativePath, pSlash-szRelativePath).c_str());
		if (!pDir)
			return ZD_ERROR_DIR_NOT_FOUND;// there is no such directory
		pDirName = pSlash+1;
	}
	else
	{
		pDir = GetRoot();
		pDirName = szRelativePath;
	}

	ErrorEnum e = pDir->RemoveDir (pDirName);
	if (e == ZD_ERROR_SUCCESS)
		m_nFlags |= FLAGS_UNCOMPACTED|FLAGS_CDR_DIRTY;
	return e;
}

// deletes all files and directories in this archive
ZipDir::ErrorEnum ZipDir::CacheRW::RemoveAll()
{
	ErrorEnum e = m_treeDir.RemoveAll();
	if (e == ZD_ERROR_SUCCESS)
		m_nFlags |= FLAGS_UNCOMPACTED|FLAGS_CDR_DIRTY;
	return e;
}

ZipDir::ErrorEnum ZipDir::CacheRW::ReadFile (FileEntry* pFileEntry, void* pCompressed, void* pUncompressed)
{
	if (!pFileEntry)
		return ZD_ERROR_INVALID_CALL;

	if (pFileEntry->desc.lSizeUncompressed == 0)
	{
		assert (pFileEntry->desc.lSizeCompressed == 0);
		return ZD_ERROR_SUCCESS;
	}

	assert (pFileEntry->desc.lSizeCompressed > 0);

	ErrorEnum nError = Refresh(pFileEntry);
	if (nError != ZD_ERROR_SUCCESS)
		return nError;

#if CRY_PLATFORM_WINDOWS
	if (_fseeki64 (m_pFile, (__int64)pFileEntry->nFileDataOffset, SEEK_SET))
#else
  if (fseek (m_pFile, pFileEntry->nFileDataOffset, SEEK_SET))
#endif
		return ZD_ERROR_IO_FAILED;

	SmartPtr pBufferDestroyer;

	void* pBuffer = pCompressed; // the buffer where the compressed data will go

	if (pFileEntry->nMethod == 0 && pUncompressed)
	{
		// we can directly read into the uncompress buffer
		pBuffer = pUncompressed;
	}

	if (!pBuffer)
	{
		if (!pUncompressed)
			// what's the sense of it - no buffers at all?
			return ZD_ERROR_INVALID_CALL;

		pBuffer = malloc(pFileEntry->desc.lSizeCompressed);
		pBufferDestroyer.Attach(pBuffer); // we want it auto-freed once we return
	}

	if (fread((char*)pBuffer, pFileEntry->desc.lSizeCompressed, 1, m_pFile) != 1)
	{
		return ZD_ERROR_IO_FAILED;
	}

	if (pFileEntry->nMethod == METHOD_DEFLATE_AND_ENCRYPT)
	{
		ZipDir::Decrypt((char*)pBuffer, pFileEntry->desc.lSizeCompressed, m_encryptionKey);
	}

	// if there's a buffer for uncompressed data, uncompress it to that buffer
	if (pUncompressed)
	{
		if (pFileEntry->nMethod == 0)
		{
			assert (pBuffer == pUncompressed);
			//assert (pFileEntry->nSizeCompressed == pFileEntry->nSizeUncompressed);
			//memcpy (pUncompressed, pBuffer, pFileEntry->nSizeCompressed);
		}
		else
		{
			unsigned long nSizeUncompressed = pFileEntry->desc.lSizeUncompressed;
			if (nSizeUncompressed > 0)
			{
				if (Z_OK != ZipRawUncompress(pUncompressed, &nSizeUncompressed, pBuffer, pFileEntry->desc.lSizeCompressed))
					return ZD_ERROR_CORRUPTED_DATA;
			}
		}
	}

	return ZD_ERROR_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////
// finds the file by exact path
ZipDir::FileEntry* ZipDir::CacheRW::FindFile (const char* szPathSrc, bool bFullInfo)
{
	char str[_MAX_PATH];
	char *szPath = UnifyPath(str, szPathSrc);

	if (!this)
		return NULL;
	ZipDir::FindFileRW fd (GetRoot());
	if (!fd.FindExact(szPath))
	{
		assert (!fd.GetFileEntry());
		return NULL;
	}
	ZipDir::FileEntry* const p = fd.GetFileEntry();
	assert (p);
	return p;
}

// returns the size of memory occupied by the instance referred to by this cache
size_t ZipDir::CacheRW::GetSize() const
{
	return sizeof(*this) + m_strFilePath.capacity() + m_treeDir.GetSize() - sizeof(m_treeDir);
}

// returns the compressed size of all the entries
size_t ZipDir::CacheRW::GetCompressedSize() const
{
	return m_treeDir.GetCompressedFileSize();
}

// returns the total size of memory occupied by the instance of this cache and all the compressed files
size_t ZipDir::CacheRW::GetTotalFileSize() const
{
	return GetSize() + GetCompressedSize();
}

// returns the total size of space occupied on disk by the instance of this cache and all the compressed files
size_t ZipDir::CacheRW::GetTotalFileSizeOnDiskSoFar() 
{
	FileRecordList arrFiles(GetRoot());
	FileRecordList::ZipStats statFiles = arrFiles.GetStats();

	return m_lCDROffset + statFiles.nSizeCDR;
}

// refreshes information about the given file entry into this file entry
ZipDir::ErrorEnum ZipDir::CacheRW::Refresh (FileEntry* pFileEntry)
{
	if (!pFileEntry)
		return ZD_ERROR_INVALID_CALL;

	if (pFileEntry->nFileDataOffset != pFileEntry->INVALID_DATA_OFFSET)
		return ZD_ERROR_SUCCESS; // the data offset has been successfully read..

	if (!this)
		return ZD_ERROR_INVALID_CALL; // from which cache is this file entry???

	return ZipDir::Refresh(m_pFile, pFileEntry, m_bEncryptedHeaders);
}


// writes the CDR to the disk
bool ZipDir::CacheRW::WriteCDR(FILE* fTarget, bool encryptCDR)
{
	if (!fTarget)
		return false;

#if CRY_PLATFORM_WINDOWS
	if (_fseeki64(fTarget, (__int64)m_lCDROffset, SEEK_SET))
#else
  if (fseek(fTarget, m_lCDROffset, SEEK_SET))
#endif
		return false;

	FileRecordList arrFiles(GetRoot());
	//arrFiles.SortByFileOffset();
	size_t nSizeCDR = arrFiles.GetStats().nSizeCDR;
	void* pCDR = malloc(nSizeCDR);
  size_t nSizeCDRSerialized = arrFiles.MakeZipCDR(m_lCDROffset, pCDR, encryptCDR);
	assert (nSizeCDRSerialized == nSizeCDR);
	
	if (encryptCDR)
	{
		// We do not encrypt CDREnd, so we could find it by signature
		ZipDir::Encrypt((char*)pCDR, nSizeCDR - sizeof(ZipFile::CDREnd), m_encryptionKey);
	}

	size_t nWriteRes = fwrite (pCDR, nSizeCDR, 1, fTarget);
	free(pCDR);
	return nWriteRes == 1;
}

// generates random file name
string ZipDir::CacheRW::GetRandomName(int nAttempt)
{
	if (nAttempt)
	{
		char szBuf[8];
		int i;
		for (i = 0; i < sizeof(szBuf)-1;++i)
		{
			int r = rand()%(10 + 'z' - 'a' + 1);
			szBuf[i] = r > 9 ? (r-10)+'a' : '0' + r;
		}
		szBuf[i] = '\0';
		return szBuf;
	}
	else
		return string();
}

bool ZipDir::CacheRW::RelinkZip()
{
	for (int nAttempt = 0; nAttempt < 32; ++nAttempt)
	{
		string strNewFilePath = m_strFilePath + "$" + GetRandomName(nAttempt);

		FILE* f = fopen (strNewFilePath.c_str(), "wb");
		if (f)
		{
			bool bOk = RelinkZip(f);
			fclose (f); // we don't need the temporary file handle anyway

			if (!bOk)
			{
				// we don't need the temporary file
				DeleteFileA(strNewFilePath.c_str());
				return false;
			}

			// we successfully relinked, now copy the temporary file to the original file
			fclose (m_pFile);
			m_pFile = NULL;

			DeleteFileA(m_strFilePath.c_str());
			if (MoveFileA(strNewFilePath.c_str(), m_strFilePath.c_str()) == 0)
			{
				// successfully renamed - reopen
				m_pFile = fopen (m_strFilePath.c_str(), "r+b");
				return m_pFile == NULL;
			}
			else
			{
				// could not rename
				
				//m_pFile = fopen (strNewFilePath.c_str(), "r+b");
				return false;
			}
		}
	}

	// couldn't open temp file
	return false; 
}

bool ZipDir::CacheRW::RelinkZip(FILE* fTmp)
{
	FileRecordList arrFiles(GetRoot());
	arrFiles.SortByFileOffset();

	// we back up our file entries, because we'll need to restore them
	// in case the operation fails
	std::vector<FileEntry> arrFileEntryBackup;
	arrFiles.Backup (arrFileEntryBackup);

	// this is the set of files that are to be written out - compressed data and the file record iterator
	std::vector<FileDataRecordPtr> queFiles;
	queFiles.reserve (g_nMaxItemsRelinkBuffer);

	// the total size of data in the queue
	unsigned nQueueSize = 0;

	for (FileRecordList::iterator it = arrFiles.begin(); it != arrFiles.end(); ++it)
	{
		FileEntry* entry = it->pFileEntry;
		// find the file data offset
		if (ZD_ERROR_SUCCESS != Refresh(entry))
			return false;

		// go to the file data
#if CRY_PLATFORM_WINDOWS
		if (_fseeki64 (m_pFile, (__int64)entry->nFileDataOffset, SEEK_SET) != 0)
#else
    if (fseek (m_pFile, entry->nFileDataOffset, SEEK_SET) != 0)
#endif
			return false;

		// allocate memory for the file compressed data
		FileDataRecordPtr pFile = FileDataRecord::New (*it);

		if(!pFile)
			return false;

		// read the compressed data
		if (entry->desc.lSizeCompressed && fread (pFile->GetData(), entry->desc.lSizeCompressed, 1, m_pFile) != 1)
			return false;

		if (entry->nMethod == METHOD_DEFLATE_AND_ENCRYPT)
		{
			ZipDir::Decrypt((char*)pFile->GetData(), entry->desc.lSizeCompressed, m_encryptionKey);
		}

		// put the file into the queue for copying (writing)
		queFiles.push_back(pFile);
		nQueueSize += entry->desc.lSizeCompressed;

		// if the queue is big enough, write it out
		if(nQueueSize > g_nSizeRelinkBuffer || queFiles.size() >= g_nMaxItemsRelinkBuffer)
		{
			nQueueSize = 0;
			if (!WriteZipFiles(queFiles, fTmp))
				return false;
		}
	}

	if (!WriteZipFiles(queFiles, fTmp))
		return false;

	ZipFile::ulong lOldCDROffset = m_lCDROffset;
	// the file data has now been written out. Now write the CDR
#if CRY_PLATFORM_WINDOWS
	m_lCDROffset = (ZipFile::ulong)_ftelli64(fTmp);
#else
	m_lCDROffset = ftell(fTmp);
#endif
	if (m_lCDROffset != (ZipFile::ulong)-1L && WriteCDR(fTmp, m_bHeadersEncryptedOnClose) && 0 == fflush (fTmp))
	{
		// the new file positions are already there - just discard the backup and return
		return true;
	}
	// recover from backup
	arrFiles.Restore (arrFileEntryBackup);
	m_lCDROffset = lOldCDROffset;
	m_bEncryptedHeaders = m_bHeadersEncryptedOnClose;
	return false;
}

// writes out the file data in the queue into the given file. Empties the queue
bool ZipDir::CacheRW::WriteZipFiles(std::vector<FileDataRecordPtr>& queFiles, FILE* fTmp)
{
	for (std::vector<FileDataRecordPtr>::iterator it = queFiles.begin(); it != queFiles.end(); ++it)
	{
		// set the new header offset to the file entry - we won't need it
#if CRY_PLATFORM_WINDOWS
		const unsigned long currentPos = (unsigned long)_ftelli64 (fTmp);
#else
		const unsigned long currentPos = ftell (fTmp);
#endif
		(*it)->pFileEntry->nFileHeaderOffset = CalculateAlignedHeaderOffset((*it)->strPath.c_str(), currentPos, m_fileAlignment);

		// while writing the local header, the data offset will also be calculated
		if (ZD_ERROR_SUCCESS != WriteLocalHeader(fTmp, (*it)->pFileEntry, (*it)->strPath.c_str(), m_bHeadersEncryptedOnClose))
			return false;;

		// write the compressed file data
		const bool encrypt = (*it)->pFileEntry->nMethod == METHOD_DEFLATE_AND_ENCRYPT;
		if (!WriteCompressedData((char*)(*it)->GetData(), (*it)->pFileEntry->desc.lSizeCompressed, encrypt, fTmp))
			return false;

#if CRY_PLATFORM_WINDOWS
    assert ((*it)->pFileEntry->nEOFOffset == (unsigned long)_ftelli64 (fTmp));
#else
    assert ((*it)->pFileEntry->nEOFOffset == ftell (fTmp));
#endif
	}
	queFiles.clear();
	queFiles.reserve (g_nMaxItemsRelinkBuffer);
	return true;
}

void TruncateFile(FILE* file, size_t newLength)
{
	int filedes = _fileno(file);
	_chsize_s(filedes, newLength);
}

bool ZipDir::CacheRW::EncryptArchive(EncryptionChange change, IEncryptPredicate* encryptContentPredicate, int* numChanged, int* numSkipped)
{
	FileRecordList arrFiles(GetRoot());
	arrFiles.SortByFileOffset();

	size_t unusedSpace = 0;
	size_t lastDataEnd = 0;

	for (FileRecordList::iterator it = arrFiles.begin(); it != arrFiles.end(); ++it)
	{
		FileEntry* entry = it->pFileEntry;

		if (entry->nFileHeaderOffset > lastDataEnd)
		{
			fseek(m_pFile, lastDataEnd, SEEK_SET);
			size_t gapLength = entry->nFileHeaderOffset - lastDataEnd;
			unusedSpace += gapLength;
			if (change == ENCRYPT)
			{
				if (!WriteRandomData(m_pFile, gapLength))
				{
					return false;
				}
			}
			else
			{
				if (!WriteNullData(gapLength))
				{
					return false;
				}
			}
		}
		lastDataEnd = entry->nEOFOffset;

		if (numSkipped)
		{
			++(*numSkipped);
		}

		// find the file data offset
		if (ZD_ERROR_SUCCESS != Refresh (entry))
		{
			return false;
		}

		ZipFile::ushort oldMethod = entry->nMethod;
		ZipFile::ushort newMethod = oldMethod;
		if (change == ENCRYPT)
		{
			if (entry->nMethod == METHOD_DEFLATE)
			{
					newMethod = METHOD_DEFLATE_AND_ENCRYPT;
			}
		}
		else 
		{
			if (entry->nMethod == METHOD_DEFLATE_AND_ENCRYPT)
			{
				newMethod = METHOD_DEFLATE;
			}
		}

		// allow encryption only for matching files
		if (newMethod == METHOD_DEFLATE_AND_ENCRYPT &&
			  (!encryptContentPredicate || !encryptContentPredicate->Match(it->strPath.c_str())))
		{
			newMethod = METHOD_DEFLATE;
		}

		entry->nMethod = newMethod;

		const bool encryptHeaders = change == ENCRYPT;
		// encryption is toggled or compression method changed...
		if (newMethod != oldMethod || encryptHeaders != m_bEncryptedHeaders)
		{
			// ... update header
			if (ZipDir::WriteLocalHeader(m_pFile, entry, it->strPath.c_str(), encryptHeaders) != ZD_ERROR_SUCCESS)
			{
				return false;
			}
		}

		if (newMethod == oldMethod)
		{
			// no need to update file content
			continue;
		}
	
		// go to the file data
#if CRY_PLATFORM_WINDOWS
		if (_fseeki64 (m_pFile, (__int64)entry->nFileDataOffset, SEEK_SET) != 0)
#else
    if (fseek (m_pFile, entry->nFileDataOffset, SEEK_SET) != 0)
#endif
			return false;

		// allocate memory for the file compressed data
		FileDataRecordPtr pFile = FileDataRecord::New(*it);
		if (!pFile)
		{
			return false;
		}

		// read the compressed data
		if (entry->desc.lSizeCompressed && fread (pFile->GetData(), entry->desc.lSizeCompressed, 1, m_pFile) != 1)
		{
			return false;
		}

		if (oldMethod == METHOD_DEFLATE_AND_ENCRYPT)
		{
			ZipDir::Decrypt((char*)pFile->GetData(), entry->desc.lSizeCompressed, m_encryptionKey);
		}

#if CRY_PLATFORM_WINDOWS
		if (_fseeki64 (m_pFile, (__int64)entry->nFileDataOffset, SEEK_SET) != 0)
#else
    if (fseek (m_pFile, entry->nFileDataOffset, SEEK_SET) != 0)
#endif
		{
			return false;
		}

		const bool encryptContent = newMethod == METHOD_DEFLATE_AND_ENCRYPT;
		if (!WriteCompressedData((const char*)pFile->GetData(), entry->desc.lSizeCompressed, encryptContent, m_pFile))
		{
			return false;
		}

		if (numSkipped)
		{
			--(*numSkipped);
		}
		if (numChanged)
		{
			++(*numChanged);
		}
	}

	m_bEncryptedHeaders = change == ENCRYPT;
	m_bHeadersEncryptedOnClose = m_bEncryptedHeaders;

	if (!WriteCDR(m_pFile, m_bEncryptedHeaders))
	{
		return false;
	}

	if (fflush (m_pFile) != 0)
	{
		return false;
	}

	size_t endOfCDR = (size_t)ftell(m_pFile);

	fseek(m_pFile, 0, SEEK_END);
	size_t fileSize = (size_t)ftell(m_pFile);

	if (fileSize != endOfCDR)
	{
		TruncateFile(m_pFile, endOfCDR);
	}


	if (unusedSpace > 0)
	{
//		RCLog("Archive contains %i bytes of uncompacted space.", (int)unusedSpace);
	}

	fclose(m_pFile);
	m_pFile = 0;
	m_treeDir.Clear();
	return true;
}
