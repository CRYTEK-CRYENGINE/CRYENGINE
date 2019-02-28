// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <zlib.h>
#include "ZipFileFormat.h"
#include "ZipDirStructures.h"
#include <time.h>
#include <CryString/CryStringUtils.h>

using namespace ZipFile;

ZipDir::FileEntry::FileEntry(const CDRFileHeader& header,const SExtraZipFileData &extra)
{
	this->desc              = header.desc;
	this->nFileHeaderOffset = header.lLocalHeaderOffset;
	this->nFileDataOffset   = INVALID_DATA_OFFSET; // we don't know yet
	this->nMethod           = header.nMethod;
	this->nNameOffset       = 0; // we don't know yet
	this->nLastModTime      = header.nLastModTime;
	this->nLastModDate      = header.nLastModDate;
	this->nNTFS_LastModifyTime = extra.nLastModifyTime;
	this->szOriginalFileName = 0;

	// make an estimation (at least this offset should be there), but we don't actually know yet
	this->nEOFOffset        = header.lLocalHeaderOffset + sizeof (ZipFile::LocalFileHeader) + header.nFileNameLength + header.desc.lSizeCompressed;
}



// Uncompresses raw (without wrapping) data that is compressed with method 8 (deflated) in the Zip file
// returns one of the Z_* errors (Z_OK upon success)
// This function just mimics the standard uncompress (with modification taken from unzReadCurrentFile)
// with 2 differences: there are no 16-bit checks, and 
// it initializes the inflation to start without waiting for compression method byte, as this is the 
// way it's stored into zip file
int ZipDir::ZipRawUncompress (void* pUncompressed, unsigned long* pDestSize, const void* pCompressed, unsigned long nSrcSize)
{
	z_stream stream;
	int err;

	stream.next_in = (Bytef*)pCompressed;
	stream.avail_in = (uInt)nSrcSize;

	stream.next_out = (Bytef*)pUncompressed;
	stream.avail_out = (uInt)*pDestSize;

	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;

	err = inflateInit2(&stream, -MAX_WBITS);
	if (err != Z_OK) return err;

	// for some strange reason, passing Z_FINISH doesn't work - 
	// it seems the stream isn't finished for some files and 
	// inflate returns an error due to stream-end-not-reached (though expected) problem
	err = inflate(&stream, Z_SYNC_FLUSH);
	if (err != Z_STREAM_END && err != Z_OK)
	{
		inflateEnd(&stream);
		return err == Z_OK ? Z_BUF_ERROR : err;
	}

	*pDestSize = stream.total_out;

	err = inflateEnd(&stream);
	return err;
}

// compresses the raw data into raw data. The buffer for compressed data itself with the heap passed. Uses method 8 (deflate)
// returns one of the Z_* errors (Z_OK upon success)
int ZipDir::ZipRawCompress (const void* pUncompressed, unsigned long* pDestSize, void* pCompressed, unsigned long nSrcSize, int nLevel)
{
	z_stream stream;
	int err;

	stream.next_in   = (Bytef*)pUncompressed;
	stream.avail_in  = (uInt)nSrcSize;

	stream.next_out  = (Bytef*)pCompressed;
	stream.avail_out = (uInt)*pDestSize;

	stream.zalloc = Z_NULL;
	stream.zfree  = Z_NULL;
	stream.opaque = Z_NULL;

	err = deflateInit2 (&stream, nLevel, Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY);
	if (err != Z_OK) return err;

	err = deflate (&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&stream);
		return err == Z_OK ? Z_BUF_ERROR : err;
	}
	*pDestSize = stream.total_out;

	err = deflateEnd(&stream);
	return err;
}

// finds the subdirectory entry by the name, using the names from the name pool
// assumes: all directories are sorted in alphabetical order.
// case-sensitive (must be lower-case if case-insensitive search in Win32 is performed)
ZipDir::DirEntry* ZipDir::DirHeader::FindSubdirEntry(const char* szName)
{
	if (this->numDirs)
	{
		const char* pNamePool = GetNamePool();
		DirEntrySortPred pred(pNamePool);
		DirEntry* pBegin = GetSubdirEntry(0);
		DirEntry* pEnd = pBegin + this->numDirs;
		DirEntry* pEntry = std::lower_bound(pBegin,pEnd, szName, pred);
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		if (pEntry != pEnd && !strcasecmp(szName, pEntry->GetName(pNamePool)))
#else
		if (pEntry != pEnd && !strcmp(szName, pEntry->GetName(pNamePool)))
#endif
			return pEntry;
	}
	return NULL;
}

// finds the file entry by the name, using the names from the name pool
// assumes: all directories are sorted in alphabetical order.
// case-sensitive (must be lower-case if case-insensitive search in Win32 is performed)
ZipDir::FileEntry* ZipDir::DirHeader::FindFileEntry(const char* szName)
{
	if (this->numFiles)
	{
		const char* pNamePool = GetNamePool();
		DirEntrySortPred pred(pNamePool);
		FileEntry* pBegin = GetFileEntry(0);
		FileEntry* pEnd = pBegin + this->numFiles;
		FileEntry* pEntry = std::lower_bound(pBegin,pEnd, szName, pred);
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		if (pEntry != pEnd && !strcasecmp(szName, pEntry->GetName(pNamePool)))
#else
		if (pEntry != pEnd && !strcmp(szName, pEntry->GetName(pNamePool)))
#endif
			return pEntry;
	}
	return NULL;
}


// tries to refresh the file entry from the given file (reads fromthere if needed)
// returns the error code if the operation was impossible to complete
ZipDir::ErrorEnum ZipDir::Refresh(FILE* f, FileEntry* pFileEntry, bool encryptedHeaders)
{
	if (pFileEntry->nFileDataOffset != pFileEntry->INVALID_DATA_OFFSET)
		return ZD_ERROR_SUCCESS;

	if (pFileEntry->desc.lSizeCompressed == 0)
	{
		return ZD_ERROR_SUCCESS;
	}
		
#if CRY_PLATFORM_WINDOWS
  if (_fseeki64(f, (__int64)pFileEntry->nFileHeaderOffset, SEEK_SET))
#else
  if (fseek(f, pFileEntry->nFileHeaderOffset, SEEK_SET))
#endif
		return ZD_ERROR_IO_FAILED;

	if (encryptedHeaders)
	{
		// with encrypted headers FileEntries should always be initialized from CDR.
		return ZD_ERROR_IO_FAILED;
	}

	// read the local file header and the name (for validation) into the buffer
	LocalFileHeader fileHeader;
	if (1 != fread (&fileHeader, sizeof(fileHeader), 1, f))
		return ZD_ERROR_IO_FAILED;

	if (fileHeader.desc != pFileEntry->desc 
			|| fileHeader.nMethod                != pFileEntry->nMethod)
	{				
		/*
			 char szErrDesc[1024];
			 cry_sprintf(szErrDesc,"ERROR: File header doesn't match previously cached file entry record (%s) \n fileheader desc=(%d,%d,%d), method=%d, \n fileentry desc=(%d,%d,%d),method=%d", \
			 f->_tmpfname, \
			 fileHeader.desc.lCRC32,fileHeader.desc.lSizeCompressed,fileHeader.desc.lSizeUncompressed, \
			 fileHeader.nMethod, \
			 pFileEntry->desc.lCRC32,pFileEntry->desc.lSizeCompressed,pFileEntry->desc.lSizeUncompressed, \
			 pFileEntry->nMethod);
			 */
		return ZD_ERROR_IO_FAILED;		
		//CryFatalError(szErrDesc);				
		//THROW_ZIPDIR_ERROR(ZD_ERROR_VALIDATION_FAILED,szErrDesc);
	}

	pFileEntry->nFileDataOffset = pFileEntry->nFileHeaderOffset + sizeof(LocalFileHeader) + fileHeader.nFileNameLength + fileHeader.nExtraFieldLength;
	pFileEntry->nEOFOffset      = pFileEntry->nFileDataOffset + pFileEntry->desc.lSizeCompressed;
	return ZD_ERROR_SUCCESS;
}

// writes into the file local header - without Extra data
// puts the new offset to the file data to the file entry
// in case of error can put INVALID_DATA_OFFSET into the data offset field of file entry
ZipDir::ErrorEnum ZipDir::WriteLocalHeader (FILE*f, FileEntry* pFileEntry, const char* szRelativePath, bool encrypt)
{
	size_t nFileNameLength = strlen(szRelativePath);
	size_t nHeaderSize = sizeof(LocalFileHeader) + nFileNameLength;

	pFileEntry->nFileDataOffset = pFileEntry->nFileHeaderOffset + nHeaderSize;
	pFileEntry->nEOFOffset = pFileEntry->nFileDataOffset + pFileEntry->desc.lSizeCompressed;

#if CRY_PLATFORM_WINDOWS
	if (_fseeki64 (f, (__int64)pFileEntry->nFileHeaderOffset, SEEK_SET))
#else
  if (fseek (f, pFileEntry->nFileHeaderOffset, SEEK_SET))
#endif
		return ZD_ERROR_IO_FAILED;

	if (encrypt)
	{
		std::vector<uint8> garbage;
		garbage.resize(nHeaderSize);
		for (size_t i = 0; i < nHeaderSize; ++i)
		{
			garbage[i] = rand() & 0xff;
		}

		if (fwrite(&garbage[0], nHeaderSize, 1, f) != 1)
		{
			return ZD_ERROR_IO_FAILED;
		}
	}
	else
	{
		LocalFileHeader h;
		memset(&h, 0, sizeof(h));
		
		h.lSignature        = h.SIGNATURE;
		h.nVersionNeeded    = 10;
		h.nFlags            = 0;
		h.nMethod           = pFileEntry->nMethod;
		h.nLastModDate      = pFileEntry->nLastModDate;
		h.nLastModTime      = pFileEntry->nLastModTime;
		h.desc              = pFileEntry->desc;
		h.nFileNameLength   = (unsigned short)nFileNameLength;
		h.nExtraFieldLength = 0;

		if (1 != fwrite(&h, sizeof(h), 1, f))
			return ZD_ERROR_IO_FAILED;

		if (nFileNameLength > 0)
		{
			if (1 != fwrite (szRelativePath, nFileNameLength, 1, f))
				return ZD_ERROR_IO_FAILED;
		}
	}

	return ZD_ERROR_SUCCESS;
}


// conversion routines for the date/time fields used in Zip
ZipFile::ushort ZipDir::DOSDate(tm* t)
{
	return
		 ((t->tm_year - 80) << 9)
		|(t->tm_mon << 5)
		|t->tm_mday;
}

ZipFile::ushort ZipDir::DOSTime(tm* t)
{
	return
		 ((t->tm_hour) << 11)
		|((t->tm_min) << 5)
		|((t->tm_sec) >> 1);
}



// sets the current time to modification time
// calculates CRC32 for the new data
void ZipDir::FileEntry::OnNewFileData(void* pUncompressed, unsigned nSize, unsigned nCompressedSize, unsigned nCompressionMethod, bool bContinuous )
{
	time_t nTime;
	time(&nTime);
	tm* t = localtime(&nTime);
	this->nLastModTime = DOSTime(t);
	this->nLastModDate = DOSDate(t);
	if( !bContinuous )
	{
		this->desc.lCRC32 = crc32(0L, Z_NULL, 0);
		this->desc.lSizeCompressed = nCompressedSize;
		this->desc.lSizeUncompressed = nSize;
	}

	// we'll need CRC32 of the file to pack it
	this->desc.lCRC32 = crc32(this->desc.lCRC32, (Bytef*)pUncompressed, nSize);

	this->nMethod = nCompressionMethod;
}


const char* ZipDir::DOSTimeCStr(ZipFile::ushort nTime)
{
	static char szBuf[16];
	cry_sprintf(szBuf, "%02d:%02d.%02d", (nTime >> 11), ((nTime&((1<<11)-1))>>5), ((nTime&((1<<5)-1))<<1));
	return szBuf;
}

const char* ZipDir::DOSDateCStr(ZipFile::ushort nTime)
{
	static char szBuf[32];
	cry_sprintf(szBuf, "%02d.%02d.%04d", (nTime&0x1F), (nTime>>5)&0xF, (nTime >> 9) + 1980);
	return szBuf;
}

uint64 ZipDir::FileEntry::GetModificationTime()
{
	if (nNTFS_LastModifyTime != 0)
		return nNTFS_LastModifyTime;

	// TODO/TIME: check and test
	SYSTEMTIME st;
	st.wYear  = (nLastModDate>>9)+1980;
	st.wMonth = ((nLastModDate>>5)&0xF);
	st.wDay   =	(nLastModDate&0x1F);
	st.wHour  = (nLastModTime>>11);
	st.wMinute= (nLastModTime>>5)&0x3F;
	st.wSecond= (nLastModTime<<1)&0x3F;
	st.wMilliseconds = 0;
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	LARGE_INTEGER lt;
	lt.HighPart = ft.dwHighDateTime;
	lt.LowPart = ft.dwLowDateTime;
	return lt.QuadPart;
}

void ZipDir::FileEntry::SetFromFileTimeNTFS( __int64 timestamp )
{
	FILETIME ft;
	ft.dwHighDateTime = timestamp >> 32;
	ft.dwLowDateTime = timestamp & 0xFFFFFFFF;

	WORD dosTime,dosDate;
	FileTimeToDosDateTime( &ft,&dosDate,&dosTime );

	nLastModDate = dosDate;
	nLastModTime = dosTime;
}

bool ZipDir::FileEntry::CompareFileTimeNTFS( __int64 timestamp )
{
	FILETIME ft;
	ft.dwHighDateTime = timestamp >> 32;
	ft.dwLowDateTime = timestamp & 0xFFFFFFFF;

	WORD dosTime,dosDate;
	FileTimeToDosDateTime( &ft,&dosDate,&dosTime );

	return (nLastModTime == dosTime && nLastModDate == dosDate);
}

const char* ZipDir::Error::getError()
{
	switch (this->nError)
	{
#define DECLARE_ERROR(x) case ZD_ERROR_##x: return #x;
	DECLARE_ERROR(SUCCESS);
	DECLARE_ERROR(IO_FAILED);
	DECLARE_ERROR(UNEXPECTED);
	DECLARE_ERROR(UNSUPPORTED);
	DECLARE_ERROR(INVALID_SIGNATURE);
	DECLARE_ERROR(ZIP_FILE_IS_CORRUPT);
	DECLARE_ERROR(DATA_IS_CORRUPT);
	DECLARE_ERROR(NO_CDR);
	DECLARE_ERROR(CDR_IS_CORRUPT);
	DECLARE_ERROR(NO_MEMORY);
	DECLARE_ERROR(VALIDATION_FAILED);
	DECLARE_ERROR(CRC32_CHECK);
	DECLARE_ERROR(ZLIB_FAILED);
	DECLARE_ERROR(ZLIB_CORRUPTED_DATA);
	DECLARE_ERROR(ZLIB_NO_MEMORY);
	DECLARE_ERROR(CORRUPTED_DATA);
	DECLARE_ERROR(INVALID_CALL);
	DECLARE_ERROR(NOT_IMPLEMENTED);
	DECLARE_ERROR(FILE_NOT_FOUND);
	DECLARE_ERROR(DIR_NOT_FOUND);
	DECLARE_ERROR(NAME_TOO_LONG);
	DECLARE_ERROR(INVALID_PATH);
	DECLARE_ERROR(FILE_ALREADY_EXISTS);
#undef DECLARE_ERROR
	default: return "Unknown ZD_ERROR code";
	}
}


inline void btea(uint32 *v, int n, uint32 const k[4])
{
#define TEA_DELTA 0x9e3779b9
#define TEA_MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (k[(p&3)^e] ^ z)))
	uint32 y, z, sum;
	unsigned p, rounds, e;
	if (n > 1) {          /* Coding Part */
		rounds = 6 + 52/n;
		sum = 0;
		z = v[n-1];
		do {
			sum += TEA_DELTA;
			e = (sum >> 2) & 3;
			for (p=0; p<n-1; p++) {
				y = v[p+1]; 
				z = v[p] += TEA_MX;
			}
			y = v[0];
			z = v[n-1] += TEA_MX;
		} while (--rounds);
	} else if (n < -1) {  /* Decoding Part */
		n = -n;
		rounds = 6 + 52/n;
		sum = rounds*TEA_DELTA;
		y = v[0];
		do {
			e = (sum >> 2) & 3;
			for (p=n-1; p>0; p--) {
				z = v[p-1];
				y = v[p] -= TEA_MX;
			}
			z = v[n-1];
			y = v[0] -= TEA_MX;
		} while ((sum -= TEA_DELTA) != 0);
	}
#undef TEA_DELTA
#undef TEA_MX
}

static inline void SwapByteOrder(uint32* values, size_t count)
{
	for (uint32 *w = values, *e = values + count; w != e; ++w)
	{
		*w = (*w >> 24) + ((*w >> 8) & 0xff00) + ((*w & 0xff00) << 8) + (*w << 24);
	}
}

//////////////////////////////////////////////////////////////////////////
void ZipDir::Encrypt(char* buffer, size_t size, const EncryptionKey& key)
{
	uint32* intBuffer = (uint32*)buffer;
	const int encryptedLen = size >> 2;

	SwapByteOrder(intBuffer, encryptedLen);

	btea(intBuffer, encryptedLen, key.key);

	SwapByteOrder(intBuffer, encryptedLen);
}

//////////////////////////////////////////////////////////////////////////
void ZipDir::Decrypt(char* buffer, size_t size, const EncryptionKey& key)
{

	uint32* intBuffer = (uint32*)buffer;
	const int encryptedLen = size >> 2;

	SwapByteOrder(intBuffer, encryptedLen);

	btea(intBuffer, -encryptedLen, key.key);

	SwapByteOrder(intBuffer, encryptedLen);
}

