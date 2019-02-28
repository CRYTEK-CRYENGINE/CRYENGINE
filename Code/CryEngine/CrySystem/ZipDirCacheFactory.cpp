// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MTSafeAllocator.h"
#include <zlib.h>
#include <CryCore/smartptr.h>
#include "ZipFileFormat.h"
#include "ZipDirStructures.h"
#include "ZipDirTree.h"
#include "ZipDirCache.h"
#include "ZipDirCacheRW.h"
#include "ZipDirCacheFactory.h"
#include "ZipDirList.h"
#include "CryPak.h"
#include "System.h"
#include "ZipEncrypt.h"
#include <CryCore/Platform/IPlatformOS.h>

using namespace ZipFile;

// Defines
// SN - 01.06.11 - Encrypted paks are being disabled during early development.
//#if defined(_RELEASE) && CRY_PLATFORM_WINDOWS
//#define ENCRYPTED_PAKS_ONLY
//#endif

ZipDir::CacheFactory::CacheFactory(CMTSafeHeap* pHeap, InitMethodEnum nInitMethod, unsigned nFlags) :
	m_pHeap(pHeap)
{
	m_nCDREndPos = 0;
	m_bBuildFileEntryMap = false; // we only need it for validation/debugging
	m_bBuildFileEntryTree = true; // we need it to actually build the optimized structure of directories
	m_bBuildOptimizedFileEntry = false;
	m_nInitMethod = nInitMethod;
	m_nFlags = nFlags;
	m_nZipFileSize = 0;
	m_encryptedHeaders = HEADERS_NOT_ENCRYPTED;
	m_signedHeaders = HEADERS_NOT_SIGNED;

	if (m_nFlags & FLAGS_FILENAMES_AS_CRC32)
	{
		m_bBuildFileEntryMap = false;
		m_bBuildFileEntryTree = false;
		m_bBuildOptimizedFileEntry = true;
	}
}

ZipDir::CacheFactory::~CacheFactory()
{
	Clear();
}

ZipDir::CachePtr ZipDir::CacheFactory::New(const char* szFile, ICryPak::EInMemoryPakLocation eInMemLocal)
{
	m_szFilename = szFile;

	Clear();

	m_fileExt.m_file = CIOWrapper::Fopen(szFile, "rb");

	if (eInMemLocal != ICryPak::eInMemoryPakLocale_Unload)
	{
		if (m_fileExt.m_file)
		{
			m_fileExt.LoadToMemory(0);
		}
	}

	if (m_fileExt.m_file)
	{
#ifdef SUPPORT_UNBUFFERED_IO
		if (!m_fileExt.OpenUnbuffered(szFile))
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Failed to open '%s' for unbuffered IO.", szFile);
#endif

		return MakeCache(szFile);
	}
	Clear();
	THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Cannot open file in binary mode for reading, probably missing file");
	return 0;
	/*
	   if (!m_f)
	   THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED,"Cannot open file in binary mode for reading, probably missing file");
	   try
	   {
	   return MakeCache (szFile);
	   }
	   catch(Error)
	   {
	   Clear();
	   throw;
	   }
	 */
}

#if CRY_PLATFORM_ANDROID && defined(ANDROID_OBB)
ZipDir::CachePtr ZipDir::CacheFactory::New(const char* szFile, ICryPak::EInMemoryPakLocation eInMemLocale, FILE* pPakFileRef, int nAssetOffset, int nAssetLength, ZipDir::FileEntry* pPakEntry)
{
	m_szFilename = szFile;

	Clear();

	if (pPakFileRef && pPakEntry &&
	    pPakEntry->desc.lSizeUncompressed == pPakEntry->desc.lSizeCompressed)
	{
		m_fileExt.m_file = NULL;
		m_fileExt.m_pPakFileRef = pPakFileRef;
		m_fileExt.m_pPakFileEntry = pPakEntry;
		m_fileExt.m_nAssetOffset = nAssetOffset;
		m_fileExt.m_nAssetLength = nAssetLength;

		if (eInMemLocale != ICryPak::eInMemoryPakLocale_Unload)
		{
			m_fileExt.LoadToMemory(0, false);
		}
		return MakeCache(szFile);
	}
	Clear();
	if (pPakFileRef == NULL)
		THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Cannot open archived pak file: Handle of containing pak/asset is NULL.");

	if (pPakEntry == NULL)
		THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Pak file entry not valid");

	if (pPakEntry->desc.lSizeUncompressed != pPakEntry->desc.lSizeCompressed)
		THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Compressed Android OBB file not supported.");
	return 0;
}

ZipDir::CachePtr ZipDir::CacheFactory::New(const char* szFile, ICryPak::EInMemoryPakLocation eInMemLocale, AAssetManager* pAssetManager)
{
	m_szFilename = szFile;

	Clear();

	AAsset* pAsset = AAssetManager_open(pAssetManager, szFile, AASSET_MODE_RANDOM);
	int fd = -1;

	if (pAsset)
	{
		off_t offset = 0;
		off_t length = 0;
		int fd = AAsset_openFileDescriptor(pAsset, &offset, &length);
		AAsset_close(pAsset);

		if (fd >= 0)
		{
			m_fileExt.m_file = fdopen(fd, "r");
			m_fileExt.m_pPakFileRef = NULL;
			m_fileExt.m_pPakFileEntry = NULL;
			m_fileExt.m_nAssetOffset = (int) offset;
			m_fileExt.m_nAssetLength = (int) length;

			if (m_fileExt.m_file)
			{
				if (eInMemLocale != ICryPak::eInMemoryPakLocale_Unload)
				{
					m_fileExt.LoadToMemory(0, false);
				}
				/// Build directory cache for the asset.
				return MakeCache(szFile);
			}
		}
	}
	else
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Cannot open asset in apk package.");
	}
	Clear();

	if (fd < 0)
		THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Cannot open asset in apk package, asset possibly compressed.");
	if (m_fileExt.m_file == NULL)
		THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Cannot open asset in apk package");

	return 0;
}
#endif

ZipDir::CachePtr ZipDir::CacheFactory::New(const char* szFile, IMemoryBlock* pData)
{
	m_szFilename = szFile;

	Clear();

	if (pData)
	{
		m_fileExt.LoadToMemory(pData);
		return MakeCache(szFile);
	}
	Clear();
	THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "No valid data to create file in binary mode for reading");
	return 0;
}

#ifndef OPTIMIZED_READONLY_ZIP_ENTRY
ZipDir::CacheRWPtr ZipDir::CacheFactory::NewRW(const char* szFileName)
{
	m_szFilename = szFileName;

	CacheRWPtr pCache = new CacheRW;
	// opens the given zip file and connects to it. Creates a new file if no such file exists
	// if successful, returns true.
	pCache->m_pHeap = m_pHeap;
	if (!(m_nFlags & FLAGS_DONT_MEMORIZE_ZIP_PATH))
		pCache->m_strFilePath = szFileName;

	if (m_nFlags & FLAGS_DONT_COMPACT)
		pCache->m_nFlags |= CacheRW::FLAGS_DONT_COMPACT;

	// first, try to open the file for reading or reading/writing
	if (m_nFlags & FLAGS_READ_ONLY)
	{
		m_fileExt.m_file = CIOWrapper::Fopen(szFileName, "rb");
		pCache->m_nFlags |= CacheRW::FLAGS_CDR_DIRTY | CacheRW::FLAGS_READ_ONLY;

		if (!m_fileExt.m_file)
		{
			THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Could not open file in binary mode for reading");
			return 0;
		}
	}
	else
	{
		m_fileExt.m_file = NULL;
		if (!(m_nFlags & FLAGS_CREATE_NEW))
			m_fileExt.m_file = CIOWrapper::Fopen(szFileName, "r+b");

		bool bOpenForWriting = true;

		if (m_fileExt.m_file)
		{
			Seek(0, SEEK_END);
			size_t nFileSize = (size_t)Tell();
			Seek(0, SEEK_SET);

			if (nFileSize)
			{
				if (!ReadCacheRW(*pCache))
				{
					THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Could not open file in binary mode for reading");
					return 0;
				}
				bOpenForWriting = false;
			}
			else
			{
				// if file has 0 bytes (e.g. crash during saving) we don't want to open it
				assert(0);    // you can ignore, the system shold handle this gracefully
			}
		}

		if (bOpenForWriting)
		{
			if (m_fileExt.m_file)
				CIOWrapper::Fclose(m_fileExt.m_file);
			m_fileExt.m_file = CIOWrapper::Fopen(szFileName, "w+b");
			if (m_fileExt.m_file)
			{
				// there's no such file, but we'll create one. We'll need to write out the CDR here
				pCache->m_lCDROffset = 0;
				pCache->m_nFlags |= CacheRW::FLAGS_CDR_DIRTY;
			}
		}

		if (!m_fileExt.m_file)
		{
			THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Could not open file in binary mode for appending (read/write)");
			return 0;
		}
	}

	// give the cache the file handle:
	pCache->m_pFile = m_fileExt.m_file;
	// the factory doesn't own it after that
	m_fileExt.m_file = NULL;

	return pCache;
}

bool ZipDir::CacheFactory::ReadCacheRW(CacheRW& rwCache)
{
	m_bBuildFileEntryTree = true;
	if (!Prepare())
		return false;

	// since it's open for R/W, we need to know exactly how much space
	// we have for each file to use the gaps efficiently
	FileEntryList Adjuster(&m_treeFileEntries, m_CDREnd.lCDROffset);
	Adjuster.RefreshEOFOffsets();

	m_treeFileEntries.Swap(rwCache.m_treeDir);
	m_CDR_buffer.swap(rwCache.m_CDR_buffer);   // CDR Buffer contain actually the string pool for the tree directory.

	// very important: we need this offset to be able to add to the zip file
	rwCache.m_lCDROffset = m_CDREnd.lCDROffset;

	rwCache.m_encryptedHeaders = m_encryptedHeaders;
	rwCache.m_signedHeaders = m_signedHeaders;
	rwCache.m_headerSignature = m_headerSignature;
	rwCache.m_headerEncryption = m_headerEncryption;
	rwCache.m_headerExtended = m_headerExtended;
	#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	memcpy(rwCache.m_block_cipher_keys_table, m_block_cipher_keys_table, sizeof(m_block_cipher_keys_table));
	#endif //SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION

	return true;
}
#endif //#ifndef OPTIMIZED_READONLY_ZIP_ENTRY

static bool IsShippedLevel(const char* str)
{
	return false; // TODO: fix me!
}

// reads everything and prepares the maps
bool ZipDir::CacheFactory::Prepare()
{
	if (!FindCDREnd())
		return false;

	//Earlier pak file encryption techniques stored the encryption type in the disk number of the CDREnd.
	//This works, but can't be used by the more recent techniques that require signed paks to be readable by 7-Zip during dev.
	ZipFile::EHeaderEncryptionType headerEnc = (ZipFile::EHeaderEncryptionType)((m_CDREnd.nDisk & 0xC000) >> 14);
	if (headerEnc == HEADERS_ENCRYPTED_TEA || headerEnc == HEADERS_ENCRYPTED_STREAMCIPHER)
	{
		m_encryptedHeaders = headerEnc;
	}
	m_CDREnd.nDisk = m_CDREnd.nDisk & 0x3fff;

	//Pak may be encrypted with CryCustom technique and/or signed. Being signed is compatible (in principle) with the earlier encryption methods.
	//The information for this exists in some custom headers at the end of the archive (in the comment section)
	if (m_CDREnd.nCommentLength >= sizeof(m_headerExtended))
	{
		Seek(m_CDREnd.lCDROffset + m_CDREnd.lCDRSize + sizeof(CDREnd));
		Read(&m_headerExtended, sizeof(m_headerExtended));
		m_headerExtended.nEncryption = SwapEndianValue(m_headerExtended.nEncryption);
		m_headerExtended.nHeaderSize = SwapEndianValue(m_headerExtended.nHeaderSize);
		m_headerExtended.nSigning = SwapEndianValue(m_headerExtended.nSigning);
		if (m_headerExtended.nHeaderSize != sizeof(m_headerExtended))
		{
			// Extended Header is not valid
			THROW_ZIPDIR_ERROR(ZD_ERROR_DATA_IS_CORRUPT, "Bad extended header");
			return false;
		}
		//We have the header, so read the encryption and signing techniques
		m_signedHeaders = (ZipFile::EHeaderSignatureType)m_headerExtended.nSigning;

		//Prepare for a quick sanity check on the size of the comment field now that we know what it should contain
		//Also check that the techniques are supported
		uint16 expectedCommentLength = sizeof(m_headerExtended);

		if (m_headerExtended.nEncryption != HEADERS_NOT_ENCRYPTED && m_encryptedHeaders != HEADERS_NOT_ENCRYPTED)
		{
			//Encryption technique has been specified in both the disk number (old technique) and the custom header (new technique).
			THROW_ZIPDIR_ERROR(ZD_ERROR_DATA_IS_CORRUPT, "Unexpected encryption technique in header");
			return false;
		}
		else
		{
			//The encryption technique has been specified only in the custom header
			m_encryptedHeaders = (ZipFile::EHeaderEncryptionType)m_headerExtended.nEncryption;
			switch (m_encryptedHeaders)
			{
			case HEADERS_NOT_ENCRYPTED:
				break;
			case HEADERS_ENCRYPTED_STREAMCIPHER_KEYTABLE:
				expectedCommentLength += sizeof(ZipFile::CryCustomEncryptionHeader);
				break;
			default:
				// Unexpected technique
				THROW_ZIPDIR_ERROR(ZD_ERROR_DATA_IS_CORRUPT, "Bad encryption technique in header");
				return false;
			}
		}

		//Add the signature header to the expected size
		switch (m_signedHeaders)
		{
		case HEADERS_NOT_SIGNED:
			break;
		case HEADERS_CDR_SIGNED:
			expectedCommentLength += sizeof(ZipFile::CrySignedCDRHeader);
			break;
		default:
			// Unexpected technique
			THROW_ZIPDIR_ERROR(ZD_ERROR_DATA_IS_CORRUPT, "Bad signing technique in header");
			return false;
		}

		if (m_CDREnd.nCommentLength == expectedCommentLength)
		{
			if (m_signedHeaders == HEADERS_CDR_SIGNED)
			{
				Read(&m_headerSignature, sizeof(m_headerSignature));
				m_headerSignature.nHeaderSize = SwapEndianValue(m_headerSignature.nHeaderSize);
				if (m_headerSignature.nHeaderSize != sizeof(m_headerSignature))
				{
					THROW_ZIPDIR_ERROR(ZD_ERROR_DATA_IS_CORRUPT, "Bad signature header");
					return false;
				}
			}
			if (m_encryptedHeaders == HEADERS_ENCRYPTED_STREAMCIPHER_KEYTABLE)
			{
				Read(&m_headerEncryption, sizeof(m_headerEncryption));
				m_headerEncryption.nHeaderSize = SwapEndianValue(m_headerEncryption.nHeaderSize);
				if (m_headerEncryption.nHeaderSize != sizeof(m_headerEncryption))
				{
					THROW_ZIPDIR_ERROR(ZD_ERROR_DATA_IS_CORRUPT, "Bad encryption header");
					return false;
				}

				//We have a table of symmetric keys to decrypt
				DecryptKeysTable();
			}
		}
		else
		{
			// Unexpected technique
			THROW_ZIPDIR_ERROR(ZD_ERROR_DATA_IS_CORRUPT, "Comment field is the wrong length");
			return false;
		}
	}

	// HACK: Hardcoded check for PAK location before enforcing encryption requirement. For C2 Mod SDK Release.
	if (m_encryptedHeaders == HEADERS_NOT_ENCRYPTED)
	{
		CryStackStringT<char, 32> modsStr("M");
		modsStr += "o";
		modsStr += "d";
		modsStr += "s";
		modsStr += "\\";
		const bool bInModsFolder = strnicmp(m_szFilename.c_str(), modsStr.c_str(), modsStr.size()) == 0;
		if (!bInModsFolder)
		{
			CryStackStringT<char, 32> levelsStr("C");
			levelsStr += "3";
			levelsStr += "\\";
			levelsStr += "L";
			levelsStr += "e";
			levelsStr += "v";
			levelsStr += "e";
			levelsStr += "l";
			levelsStr += "s";
			levelsStr += "\\";
			const bool bStartsWithLevels = strnicmp(m_szFilename.c_str(), levelsStr.c_str(), levelsStr.size()) == 0;
			const bool bIsCustomSPLevel = bStartsWithLevels && !IsShippedLevel(m_szFilename.c_str() + levelsStr.size());
			if (!bIsCustomSPLevel)
			{
#ifdef ENCRYPTED_PAKS_ONLY
				THROW_ZIPDIR_ERROR(ZD_ERROR_CORRUPTED_DATA, "Archive contains corrupted CDR.");
				return false;
#else
				//THROW_ZIPDIR_ERROR(ZD_ERROR_UNSUPPORTED, "Warning: Loading non-encrypted pack. This will not work in Release.");
#endif
			}
		}

#if defined(ENCRYPTED_PAKS_ONLY)
		if (GetReferenceCRCForPak())
		{
			m_encryptedHeaders = HEADERS_ENCRYPTED_STREAMCIPHER;
		}
#endif
	}

#if defined(OPTIMIZED_READONLY_ZIP_ENTRY)
	if (m_encryptedHeaders == HEADERS_ENCRYPTED_STREAMCIPHER)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_UNSUPPORTED, "Warning: Using stream cipher encrypted pak on unsupported platform!");
		return false;
	}
#endif

	// we don't support multivolume archives
	if (m_CDREnd.nDisk != 0
	    || m_CDREnd.nCDRStartDisk != 0
	    || m_CDREnd.numEntriesOnDisk != m_CDREnd.numEntriesTotal)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_UNSUPPORTED, "Multivolume archive detected. Current version of ZipDir does not support multivolume archives");
		return false;
	}

	// if the central directory offset or size are out of range,
	// the CDREnd record is probably corrupt
	if (m_CDREnd.lCDROffset > m_nCDREndPos
	    || m_CDREnd.lCDRSize > m_nCDREndPos
	    || m_CDREnd.lCDROffset + m_CDREnd.lCDRSize > m_nCDREndPos)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_DATA_IS_CORRUPT, "The central directory offset or size are out of range, the pak is probably corrupt, try to repare or delete the file");
		return false;
	}

#if defined(ENCRYPTED_PAKS_ONLY)
	if (!BuildFileEntryMap())
		return false;
#else
	BuildFileEntryMap();
#endif

	return true;
}

struct SortFileEntryByNameOffsetPredicate
{
	bool operator()(const ZipDir::FileEntry& f1, const ZipDir::FileEntry& f2) const
	{
		return f1.nNameOffset < f2.nNameOffset;
	}
};

namespace
{

#if CRY_PLATFORM_DURANGO
inline uint64 GenPakFileOffsetOnMedia(const char* szFile)
{
	IPlatformOS* pOS = gEnv->pSystem->GetPlatformOS();

	uint64 chunkIdx = pOS->GetPakSortIndex(szFile);

	return chunkIdx << 31;   // 2GB
}
#endif
}

ZipDir::CachePtr ZipDir::CacheFactory::MakeCache(const char* szFile)
{
	if (!Prepare())
		return NULL;

	size_t nSizeRequired = 0;

	if (m_bBuildOptimizedFileEntry)
	{
		nSizeRequired = m_optimizedFileEntries.size() * sizeof(FileEntry);
	}
	else
	{
		// initializes this object from the given tree, which is a convenient representation of the file tree
		nSizeRequired = m_treeFileEntries.GetSizeSerialized();
	}

	// initializes this object from the given tree, which is a convenient representation of the file tree
	size_t nSizeZipPath = 1; // we need to remember the terminating 0
	if (!(m_nFlags & FLAGS_DONT_MEMORIZE_ZIP_PATH))
		nSizeZipPath += strlen(szFile);
	// allocate and initialize the memory that'll be the root now
	size_t nCacheInstanceSize = sizeof(Cache) + nSizeRequired + nSizeZipPath;

	Cache* pCacheInstance = (Cache*)m_pHeap->PersistentAlloc(nCacheInstanceSize); // Do not use pools for this allocation
	pCacheInstance->Construct(m_fileExt, m_pHeap, nSizeRequired, m_nFlags, nCacheInstanceSize);
#if CRY_PLATFORM_DURANGO
	pCacheInstance->SetPakFileOffsetOnMedia(GenPakFileOffsetOnMedia(szFile));
#endif
	pCacheInstance->SetZipFileSize(m_nZipFileSize);
	pCacheInstance->SetCDROffsetSize(m_CDREnd.lCDROffset, m_CDREnd.lCDRSize);

	CachePtr cache = pCacheInstance;

	// try to serialize into the memory
	if (m_bBuildOptimizedFileEntry)
	{
		if (m_optimizedFileEntries.size() > 0)
		{
			// Sort file entries, to allow fast binary search.
			std::sort(m_optimizedFileEntries.begin(), m_optimizedFileEntries.end(), SortFileEntryByNameOffsetPredicate());

#ifndef OPTIMIZED_READONLY_ZIP_ENTRY
			// check uniqness of CRC32 of files.
			for (int i = 0, num = m_optimizedFileEntries.size(); i < num - 1; i++)
			{
				if (m_optimizedFileEntries[i].nNameOffset == m_optimizedFileEntries[i + 1].nNameOffset)
				{
					// Duplicate CRC found, Fatal Error.
					CryFatalError("Duplicate CRC32 of filenames in pak file %s", m_szFilename.c_str());
				}
			}
#endif //#ifndef OPTIMIZED_READONLY_ZIP_ENTRY

			memcpy(cache->GetDataPointer(), &m_optimizedFileEntries[0], nSizeRequired);
		}
	}
	else
	{
		size_t nSizeSerialized = m_treeFileEntries.Serialize(cache->GetRoot());
		assert(nSizeSerialized == nSizeRequired);
	}

	char* pZipPath = ((char*)(pCacheInstance + 1)) + nSizeRequired;

	if (!(m_nFlags & FLAGS_DONT_MEMORIZE_ZIP_PATH))
		memcpy(pZipPath, szFile, nSizeZipPath);
	else
		pZipPath[0] = '\0';

	cache->m_encryptedHeaders = m_encryptedHeaders;
	cache->m_signedHeaders = m_signedHeaders;
	cache->m_headerSignature = m_headerSignature;
	cache->m_headerEncryption = m_headerEncryption;
	cache->m_headerExtended = m_headerExtended;
#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	memcpy(cache->m_block_cipher_keys_table, m_block_cipher_keys_table, sizeof(m_block_cipher_keys_table));
#endif //SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION

	Clear();

	return cache;
}

void ZipDir::CacheFactory::Clear()
{
	m_fileExt.Close();

	m_nCDREndPos = 0;
	memset(&m_CDREnd, 0, sizeof(m_CDREnd));
	m_mapFileEntries.clear();
	m_treeFileEntries.Clear();
	m_encryptedHeaders = HEADERS_NOT_ENCRYPTED;
}

//////////////////////////////////////////////////////////////////////////
// searches for CDREnd record in the given file
bool ZipDir::CacheFactory::FindCDREnd()
{
	// this buffer will be used to find the CDR End record
	// the additional bytes are required to store the potential tail of the CDREnd structure
	// when moving the window to the next position in the file

	//We cannot create it on the stack for RSX memory usage as we are not permitted to access it via SPU
	std::vector<char> pReservedBuffer(g_nCDRSearchWindowSize + sizeof(CDREnd) - 1);

	Seek(0, SEEK_END);
	int64 nFileSize = Tell();

	//There is a 2GB pak file limit
	if (nFileSize > 2147483648ull)
	{
		THROW_ZIPDIR_FATAL_ERROR(ZD_ERROR_ARCHIVE_TOO_LARGE, "The file is too large. Can't open a pak file that is greater than 2GB in size.");
	}

	m_nZipFileSize = (size_t)nFileSize;

	if (nFileSize < sizeof(ZipFile::CDREnd))
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_NO_CDR, "The file is too small, it doesn't even contain the CDREnd structure. Please check and delete the file. Truncated files are not deleted automatically");
		return false;
	}

	// this will point to the place where the buffer was loaded
	unsigned int nOldBufPos = (unsigned int)nFileSize;
	// start scanning well before the end of the file to avoid reading beyond the end

	unsigned int nScanPos = nOldBufPos - sizeof(CDREnd);

	m_CDREnd.lSignature = 0; // invalid signature as the flag of not-found CDR End structure
	while (true)
	{
		unsigned int nNewBufPos;             // the new buf pos
		char* pWindow = &pReservedBuffer[0]; // the window pointer into which data will be read (takes into account the possible tail-of-CDREnd)
		if (nOldBufPos <= g_nCDRSearchWindowSize)
		{
			// the old buffer position doesn't let us read the full search window size
			// therefore the new buffer pos will be 0 (instead of negative beyond the start of the file)
			// and the window pointer will be closer tot he end of the buffer because the end of the buffer
			// contains the data from the previous iteration (possibly)
			nNewBufPos = 0;
			pWindow = &pReservedBuffer[g_nCDRSearchWindowSize - (nOldBufPos - nNewBufPos)];
		}
		else
		{
			nNewBufPos = nOldBufPos - g_nCDRSearchWindowSize;
			assert(nNewBufPos > 0);
		}

		// since dealing with 32bit unsigned, check that filesize is bigger than
		// CDREnd plus comment before the following check occurs.
		if (nFileSize > (sizeof(CDREnd) + 0xFFFF))
		{
			// if the new buffer pos is beyond 64k limit for the comment size
			if (nNewBufPos < (unsigned int)(nFileSize - sizeof(CDREnd) - 0xFFFF))
			{
				nNewBufPos = (unsigned int)(nFileSize - sizeof(CDREnd) - 0xFFFF);
			}
		}

		// if there's nothing to search
		if (nNewBufPos >= nOldBufPos)
		{
			THROW_ZIPDIR_ERROR(ZD_ERROR_NO_CDR, ERROR_CANT_FIND_CENTRAL_DIRECTORY);  // we didn't find anything
			return false;
		}

		// seek to the start of the new window and read it
		Seek(nNewBufPos);
		Read(pWindow, nOldBufPos - nNewBufPos);

		while (nScanPos >= nNewBufPos)
		{
			CDREnd* pEnd = (CDREnd*)(pWindow + nScanPos - nNewBufPos);
			if (SwapEndianValue(pEnd->lSignature) == pEnd->SIGNATURE)
			{
				if (SwapEndianValue(pEnd->nCommentLength) == nFileSize - nScanPos - sizeof(CDREnd))
				{
					// the comment length is exactly what we expected
					m_CDREnd = *pEnd;
					SwapEndian(m_CDREnd);
					m_nCDREndPos = nScanPos;
					break;
				}
				else
				{
					THROW_ZIPDIR_ERROR(ZD_ERROR_DATA_IS_CORRUPT, "Central Directory Record is followed by a comment of inconsistent length. This might be a minor misconsistency, please try to repair the file. However, it is dangerous to open the file because I will have to guess some structure offsets, which can lead to permanent unrecoverable damage of the archive content");
					return false;
				}
			}
			if (nScanPos == 0)
				break;
			--nScanPos;
		}

		if (m_CDREnd.lSignature == m_CDREnd.SIGNATURE)
			return true; // we've found it

		nOldBufPos = nNewBufPos;
		memmove(&pReservedBuffer[g_nCDRSearchWindowSize], pWindow, sizeof(CDREnd) - 1);
	}
	THROW_ZIPDIR_ERROR(ZD_ERROR_UNEXPECTED, "The program flow may not have possibly lead here. This error is unexplainable");  // we shouldn't be here

	return false;
}

//////////////////////////////////////////////////////////////////////////
// uses the found CDREnd to scan the CDR and probably the Zip file itself
// builds up the m_mapFileEntries
bool ZipDir::CacheFactory::BuildFileEntryMap()
{
	LOADING_TIME_PROFILE_SECTION;

	Seek(m_CDREnd.lCDROffset);

	if (m_CDREnd.lCDRSize == 0)
		return true;

	DynArray<char>& pBuffer = m_CDR_buffer; // Use persistent buffer.

	pBuffer.resize(m_CDREnd.lCDRSize + 16); // Allocate some more because we use this memory as a strings pool.

	if (pBuffer.empty()) // couldn't allocate enough memory for temporary copy of CDR
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_NO_MEMORY, "Not enough memory to cache Central Directory record for fast initialization. This error may not happen on non-console systems");
		return false;
	}

	if (!ReadHeaderData(&pBuffer[0], m_CDREnd.lCDRSize))
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_CORRUPTED_DATA, "Archive contains corrupted CDR.");
		return false;
	}

	if (m_bBuildOptimizedFileEntry)
	{
		m_optimizedFileEntries.clear();
		m_optimizedFileEntries.reserve(m_CDREnd.numEntriesTotal);
	}

	// now we've read the complete CDR - parse it.
	CDRFileHeader* pFile = (CDRFileHeader*)(&pBuffer[0]);
	const char* pEndOfData = &pBuffer[0] + m_CDREnd.lCDRSize, * pFileName;

	while ((pFileName = (const char*)(pFile + 1)) <= pEndOfData)
	{
		// Hacky way to use CDR memory block as a string pool.
		pFile->lSignature = 0; // Force signature to always be 0 (First byte of signature maybe a zero termination of the previous file filename).

		SwapEndian(*pFile);
		if ((pFile->nVersionNeeded & 0xFF) > 20)
		{
			THROW_ZIPDIR_ERROR(ZD_ERROR_UNSUPPORTED, "Cannot read the archive file (nVersionNeeded > 20).");
			return false;
		}
		//if (pFile->lSignature != pFile->SIGNATURE) // Timur, Dont compare signatures as signatue in memory can be overwritten by the code below
		//break;
		// the end of this file record
		const char* pEndOfRecord = (pFileName + pFile->nFileNameLength + pFile->nExtraFieldLength + pFile->nFileCommentLength);
		// if the record overlaps with the End Of CDR structure, something is wrong
		if (pEndOfRecord > pEndOfData)
		{
			THROW_ZIPDIR_ERROR(ZD_ERROR_CDR_IS_CORRUPT, "Central Directory record is either corrupt, or truncated, or missing. Cannot read the archive directory");
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		// Analyze advanced section.
		//////////////////////////////////////////////////////////////////////////
		SExtraZipFileData extra;
		const char* pExtraField = (pFileName + pFile->nFileNameLength);
		const char* pExtraEnd = pExtraField + pFile->nExtraFieldLength;
		while (pExtraField < pExtraEnd)
		{
			const char* pAttrData = pExtraField + sizeof(ExtraFieldHeader);
			ExtraFieldHeader& hdr = *(ExtraFieldHeader*)pExtraField;
			switch (hdr.headerID)
			{
			case EXTRA_NTFS:
				{
					//					ExtraNTFSHeader &ntfsHdr = *(ExtraNTFSHeader*)pAttrData;
					memcpy(&extra, pAttrData + sizeof(ExtraNTFSHeader), sizeof(SExtraZipFileData));
					//					uint64 accTime = *(uint64*)(pAttrData + sizeof(ExtraNTFSHeader) + 8);
					//					uint64 crtTime = *(uint64*)(pAttrData + sizeof(ExtraNTFSHeader) + 16);
				}
				break;
			}
			pExtraField += sizeof(ExtraFieldHeader) + hdr.dataSize;
		}

		bool bDirectory = false;
		if (pFile->nFileNameLength > 0 && (pFileName[pFile->nFileNameLength - 1] == '/' || pFileName[pFile->nFileNameLength - 1] == '\\'))
			bDirectory = true;

		if (!bDirectory)
		{
			// Add this file entry.
			char* str = const_cast<char*>(pFileName);
			for (int i = 0; i < pFile->nFileNameLength; i++)
			{
				str[i] = ::tolower(str[i]);
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE  // On linux and Mac always forward slashes.
				if (str[i] == '\\')
					str[i] = '/';
#else
				if (str[i] == '/')
					str[i] = '\\';
#endif
			}
			str[pFile->nFileNameLength] = 0; // Not standart!, may overwrite signature of the next memory record data in zip.
			AddFileEntry(str, pFile, extra);
		}

		// move to the next file
		pFile = (CDRFileHeader*)pEndOfRecord;
	}

	// finished reading CDR
	return true;
}

//////////////////////////////////////////////////////////////////////////
// give the CDR File Header entry, reads the local file header to validate
// and determine where the actual file lies
void ZipDir::CacheFactory::AddFileEntry(char* strFilePath, const ZipFile::CDRFileHeader* pFileHeader, const SExtraZipFileData& extra)
{
	if (pFileHeader->lLocalHeaderOffset > m_CDREnd.lCDROffset)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_CDR_IS_CORRUPT, "Central Directory contains file descriptors pointing outside the archive file boundaries. The archive file is either truncated or damaged. Please try to repair the file");  // the file offset is beyond the CDR: impossible
		return;
	}

	if ((pFileHeader->nMethod == METHOD_STORE || pFileHeader->nMethod == METHOD_STORE_AND_STREAMCIPHER_KEYTABLE) && pFileHeader->desc.lSizeUncompressed != pFileHeader->desc.lSizeCompressed)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_VALIDATION_FAILED, "File with STORE compression method declares its compressed size not matching its uncompressed size. File descriptor is inconsistent, archive content may be damaged, please try to repair the archive");
		return;
	}

	FileEntry fileEntry(*pFileHeader, extra);

	// when using encrypted headers we should always initialize data offsets from CDR
	if ((m_encryptedHeaders != HEADERS_NOT_ENCRYPTED || m_nInitMethod >= ZD_INIT_FULL) && pFileHeader->desc.lSizeCompressed)
	{
		InitDataOffset(fileEntry, pFileHeader);
	}

	if (m_bBuildFileEntryMap)
		m_mapFileEntries.insert(FileEntryMap::value_type(strFilePath, fileEntry));

	if (m_bBuildFileEntryTree)
		m_treeFileEntries.Add(strFilePath, fileEntry);

	// Optimized File Entry.
	if (m_bBuildOptimizedFileEntry)
	{
		uint32 nHash = ZipDir::FileNameHash(strFilePath);

#ifndef _RELEASE
		if (g_cvars.pakVars.nValidateFileHashes)
		{
			ZipDir::Cache::ValidateFilenameHash(nHash, strFilePath, m_szFilename);
		}
#endif

		fileEntry.nNameOffset = nHash;
		m_optimizedFileEntries.push_back(fileEntry);
	}
}

//////////////////////////////////////////////////////////////////////////
// initializes the actual data offset in the file in the fileEntry structure
// searches to the local file header, reads it and calculates the actual offset in the file
void ZipDir::CacheFactory::InitDataOffset(FileEntry& fileEntry, const ZipFile::CDRFileHeader* pFileHeader)
{
	/*
	   // without validation, it would be like this:
	   ErrorEnum nError = Refresh(&fileEntry);
	   if (nError != ZD_ERROR_SUCCESS)
	   THROW_ZIPDIR_ERROR(nError,"Cannot refresh file entry. Probably corrupted file header inside zip file");
	 */

	if (m_encryptedHeaders != HEADERS_NOT_ENCRYPTED)
	{
		// use CDR instead of local header
		// The pak encryption tool asserts that there is no extra data at the end of the local file header, so don't add any extra data from the CDR header.
		fileEntry.nFileDataOffset = pFileHeader->lLocalHeaderOffset + sizeof(LocalFileHeader) + pFileHeader->nFileNameLength;
	}
	else
	{
		Seek(pFileHeader->lLocalHeaderOffset);

		// read the local file header and the name (for validation) into the buffer
		DynArray<char> pBuffer;
		unsigned nBufferLength = sizeof(LocalFileHeader) + pFileHeader->nFileNameLength;
		pBuffer.resize(nBufferLength);
		Read(&pBuffer[0], nBufferLength);

		// validate the local file header (compare with the CDR file header - they should contain basically the same information)
		const LocalFileHeader* pLocalFileHeader = (const LocalFileHeader*)&pBuffer[0];
		if (pFileHeader->desc != pLocalFileHeader->desc
		    || pFileHeader->nMethod != pLocalFileHeader->nMethod
		    || pFileHeader->nFileNameLength != pLocalFileHeader->nFileNameLength
		    // for a tough validation, we can compare the timestamps of the local and central directory entries
		    // but we won't do that for backward compatibility with ZipDir
		    //|| pFileHeader->nLastModDate != pLocalFileHeader->nLastModDate
		    //|| pFileHeader->nLastModTime != pLocalFileHeader->nLastModTime
		    )
		{
			THROW_ZIPDIR_ERROR(ZD_ERROR_VALIDATION_FAILED, "The local file header descriptor doesn't match the basic parameters declared in the global file header in the file. The archive content is misconsistent and may be damaged. Please try to repair the archive");
			return;
		}

		// now compare the local file name with the one recorded in CDR: they must match.
		if (memicmp((const char*)&pBuffer[sizeof(LocalFileHeader)], (const char*)pFileHeader + 1, pFileHeader->nFileNameLength))
		{
			// either file name, or the extra field do not match
			THROW_ZIPDIR_ERROR(ZD_ERROR_VALIDATION_FAILED, "The local file header contains file name which does not match the file name of the global file header. The archive content is misconsistent with its directory. Please repair the archive");
			return;
		}

		fileEntry.nFileDataOffset = pFileHeader->lLocalHeaderOffset + sizeof(LocalFileHeader) + pLocalFileHeader->nFileNameLength + pLocalFileHeader->nExtraFieldLength;
	}

#ifndef OPTIMIZED_READONLY_ZIP_ENTRY
	// make sure it's the same file and the fileEntry structure is properly initialized
	assert(fileEntry.nFileHeaderOffset == pFileHeader->lLocalHeaderOffset);

	fileEntry.nEOFOffset = fileEntry.nFileDataOffset + fileEntry.desc.lSizeCompressed;
#endif

	if (fileEntry.nFileDataOffset >= m_nCDREndPos)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_VALIDATION_FAILED, "The global file header declares the file which crosses the boundaries of the archive. The archive is either corrupted or truncated, please try to repair it");
		return;
	}

	if (m_nInitMethod >= ZD_INIT_VALIDATE)
		Validate(fileEntry);
}

//////////////////////////////////////////////////////////////////////////
// reads the file pointed by the given header and entry (they must be coherent)
// and decompresses it; then calculates and validates its CRC32
void ZipDir::CacheFactory::Validate(const FileEntry& fileEntry)
{
#ifndef OPTIMIZED_READONLY_ZIP_ENTRY
	DynArray<char> pBuffer;
	// validate the file contents
	// allocate memory for both the compressed data and uncompressed data
	pBuffer.resize(fileEntry.desc.lSizeCompressed + fileEntry.desc.lSizeUncompressed);
	char* pUncompressed = &pBuffer[fileEntry.desc.lSizeCompressed];
	char* pCompressed = &pBuffer[0];

	assert(fileEntry.nFileDataOffset != FileEntry::INVALID_DATA_OFFSET);
	Seek(fileEntry.nFileDataOffset);

	Read(pCompressed, fileEntry.desc.lSizeCompressed);

	if (0)
	{
		//Intentionally left empty
	}
	#if defined(SUPPORT_UNENCRYPTED_PAKS)
	else if (!fileEntry.IsEncrypted())
	{
		//Do nothing - we support unencrypted pak contents
	}
	#endif //SUPPORT_UNENCRYPTED_PAKS
	#if defined(SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION)
	if (fileEntry.nMethod == METHOD_STORE_AND_STREAMCIPHER_KEYTABLE || fileEntry.nMethod == METHOD_DEFLATE_AND_STREAMCIPHER_KEYTABLE)
	{
		unsigned char IV[ZipFile::BLOCK_CIPHER_KEY_LENGTH]; //16 byte
		int nKeyIndex = ZipEncrypt::GetEncryptionKeyIndex(&fileEntry);
		ZipEncrypt::GetEncryptionInitialVector(&fileEntry, IV);
		if (!ZipEncrypt::DecryptBufferWithStreamCipher((unsigned char*)pUncompressed, fileEntry.desc.lSizeCompressed, m_block_cipher_keys_table[nKeyIndex], IV))
		{
			// Decryption failed
			THROW_ZIPDIR_ERROR(ZD_ERROR_CORRUPTED_DATA, "Data is corrupt");  //Don't give a clue about the encryption support
		}
	}
	#endif //SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	#if defined(SUPPORT_XTEA_PAK_ENCRYPTION)
	else if (fileEntry.nMethod == METHOD_DEFLATE_AND_ENCRYPT)
	{
		ZipDir::Decrypt(pCompressed, fileEntry.desc.lSizeCompressed);
	}
	#endif
	#if defined(SUPPORT_STREAMCIPHER_PAK_ENCRYPTION)
	else if (fileEntry.nMethod == METHOD_DEFLATE_AND_STREAMCIPHER)
	{
		ZipDir::StreamCipher(pCompressed, &fileEntry);
	}
	#endif
	else
	{
	#if !defined(_RELEASE)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Tried to verify a file from an archive using an unsupported method");
	#endif                                                            //!_RELEASE
		THROW_ZIPDIR_ERROR(ZD_ERROR_CORRUPTED_DATA, "Data is corrupt"); //Don't give a clue about the encryption support
	}

	unsigned long nDestSize = fileEntry.desc.lSizeUncompressed;
	int nError = Z_OK;
	if (fileEntry.nMethod)
	{
		nError = ZipRawUncompress(m_pHeap, pUncompressed, &nDestSize, pCompressed, fileEntry.desc.lSizeCompressed);
	}
	else
	{
		assert(fileEntry.desc.lSizeCompressed == fileEntry.desc.lSizeUncompressed);
		memcpy(pUncompressed, pCompressed, fileEntry.desc.lSizeUncompressed);
	}
	switch (nError)
	{
	case Z_OK:
		break;
	case Z_MEM_ERROR:
		THROW_ZIPDIR_ERROR(ZD_ERROR_ZLIB_NO_MEMORY, "ZLib reported out-of-memory error");
		return;
	case Z_BUF_ERROR:
		THROW_ZIPDIR_ERROR(ZD_ERROR_ZLIB_CORRUPTED_DATA, "ZLib reported compressed stream buffer error");
		return;
	case Z_DATA_ERROR:
		THROW_ZIPDIR_ERROR(ZD_ERROR_ZLIB_CORRUPTED_DATA, "ZLib reported compressed stream data error");
		return;
	default:
		THROW_ZIPDIR_ERROR(ZD_ERROR_ZLIB_FAILED, "ZLib reported an unexpected unknown error");
		return;
	}

	if (nDestSize != fileEntry.desc.lSizeUncompressed)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_CORRUPTED_DATA, "Uncompressed stream doesn't match the size of uncompressed file stored in the archive file headers");
		return;
	}

	uLong uCRC32 = crc32(0L, Z_NULL, 0);
	uCRC32 = crc32(uCRC32, (Bytef*)pUncompressed, nDestSize);
	if (uCRC32 != fileEntry.desc.lCRC32)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_CRC32_CHECK, "Uncompressed stream CRC32 check failed");
		return;
	}
#endif //#ifndef OPTIMIZED_READONLY_ZIP_ENTRY
}

//////////////////////////////////////////////////////////////////////////
// extracts the file path from the file header with subsequent information
// may, or may not, put all letters to lower-case (depending on whether the system is to be case-sensitive or not)
// it's the responsibility of the caller to ensure that the file name is in readable valid memory
char* ZipDir::CacheFactory::GetFilePath(const char* pFileName, uint16 nFileNameLength)
{
	static char strResult[_MAX_PATH];
	assert(nFileNameLength < _MAX_PATH);
	memcpy(strResult, pFileName, nFileNameLength);
	strResult[nFileNameLength] = 0;
	for (int i = 0; i < nFileNameLength; i++)
	{
		strResult[i] = ::tolower(strResult[i]);
	}

	return strResult;
}

// seeks in the file relative to the starting position
void ZipDir::CacheFactory::Seek(uint32 nPos, int nOrigin)  // throw
{
	//#if CRY_PLATFORM_WINDOWS
	//	if (_fseeki64 (m_f, (__int64)nPos, nOrigin))
	//#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	//	if (fseeko(m_f, (off_t)nPos, nOrigin))
	//#else
	//	if (fseek (m_f, nPos, nOrigin))
	//#endif
	if (ZipDir::FSeek(&m_fileExt, nPos, nOrigin))
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Cannot fseek() to the new position in the file. This is unexpected error and should not happen under any circumstances. Perhaps some network or disk failure error has caused this");
		return;
	}
}

int64 ZipDir::CacheFactory::Tell()  // throw
{
	//#if CRY_PLATFORM_WINDOWS
	//	__int64 nPos = _ftelli64 (m_f);
	//#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	//	off_t nPos = ftello(m_f);
	//#else
	//	long nPos = ftell (m_f);
	//#endif
	int64 nPos = ZipDir::FTell(&m_fileExt);
	if (nPos == -1)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Cannot ftell() position in the archive. This is unexpected error and should not happen under any circumstances. Perhaps some network or disk failure error has caused this");
		return 0;
	}
	return nPos;
}

bool ZipDir::CacheFactory::Read(void* pDest, unsigned nSize)  // throw
{
	//fread (pDest, nSize, 1, m_f)
	if (ZipDir::FRead(&m_fileExt, pDest, nSize, 1) != 1)
	{
		THROW_ZIPDIR_ERROR(ZD_ERROR_IO_FAILED, "Cannot fread() a portion of data from archive");
		return false;
	}
	return true;
}

// returns the expected CRC for this pak. used for CDR cipher and also allowing the build to bork if the wrong pak is used
// accidentally or by hackers
unsigned long ZipDir::CacheFactory::GetReferenceCRCForPak()
{
	unsigned long result = 0;

	// TODO: add file for release build process; see //depot/dev/milestones/c2mp/PC_DX11/Patch/...

	//	struct SPakCRCRef
	//	{
	//		char					pak[32];
	//		unsigned long	refCRC;
	//	};
	//	SPakCRCRef			refs[]={
	//#include "../../../Patch/paks_generated.h"
	//	};
	//
	//	const char		*pPakName=m_szFilename.c_str();
	//
	//	for (size_t i=0; i<CRY_ARRAY_COUNT(refs); i++)
	//	{
	//		if (strstr(pPakName,refs[i].pak))
	//		{
	//			result=refs[i].refCRC;
	//			break;
	//		}
	//	}

	return result;
}

bool ZipDir::CacheFactory::ReadHeaderData(void* pDest, unsigned nSize)  // throw
{
	if (!Read(pDest, nSize))
		return false;

	switch (m_encryptedHeaders)
	{
#if defined(SUPPORT_XTEA_PAK_ENCRYPTION)
	case HEADERS_ENCRYPTED_TEA:
		ZipDir::Decrypt((char*)pDest, nSize);
		break;
#endif

#if !defined(OPTIMIZED_READONLY_ZIP_ENTRY) && defined(SUPPORT_STREAMCIPHER_PAK_ENCRYPTION)
	case HEADERS_ENCRYPTED_STREAMCIPHER:
		ZipDir::StreamCipher((char*)pDest, nSize, GetReferenceCRCForPak());
		break;
#endif

#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	case HEADERS_ENCRYPTED_STREAMCIPHER_KEYTABLE:
		// Decrypt CDR
		if (!ZipEncrypt::DecryptBufferWithStreamCipher((unsigned char*)pDest, nSize, m_block_cipher_keys_table[0], m_block_cipher_cdr_initial_vector))
		{
			// Decryption of CDR failed.
	#if !defined(_RELEASE)
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR_DBGBRK, "Failed to decrypt pak header");
	#endif //!_RELEASE
			return false;
		}
		break;
#endif  //SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION

#if defined(SUPPORT_UNENCRYPTED_PAKS)
	case HEADERS_NOT_ENCRYPTED:
		break; //Nothing to do here
#endif     //SUPPORT_UNENCRYPTED_PAKS

	default:
#if !defined(_RELEASE)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR_DBGBRK, "Attempting to load encrypted pak by unsupported method, or unencrypted pak when support is disabled");
#endif
		return false;
	}

	switch (m_signedHeaders)
	{
	case HEADERS_CDR_SIGNED:
#ifdef SUPPORT_RSA_PAK_SIGNING
		{
			//Verify CDR signature & pak name
			const char* szPathSep = strrchr(m_szFilename.c_str(), '\\');
			const char* szPathSepFS = strrchr(m_szFilename.c_str(), '/');
			szPathSep = szPathSep > szPathSepFS ? szPathSep : szPathSepFS;
			if (szPathSep == NULL)
			{
				szPathSep = m_szFilename.c_str();
			}
			else
			{
				szPathSep++;
			}
			const unsigned char* dataToVerify[2];
			dataToVerify[0] = (const unsigned char*)pDest;
			dataToVerify[1] = (const unsigned char*)szPathSep;
			unsigned int sizesToVerify[2];
			sizesToVerify[0] = nSize;
			sizesToVerify[1] = strlen(szPathSep);
			if (!ZipEncrypt::RSA_VerifyData(dataToVerify, sizesToVerify, 2, m_headerSignature.CDR_signed, 128, g_rsa_key_public_for_sign))
			{
	#if !defined(SUPPORT_UNSIGNED_PAKS)
		#if !defined(_RELEASE)
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR_DBGBRK, "Failed to verify RSA signature of pak header");
		#endif //!_RELEASE
				   // Could not verify signature
				return false;
	#endif
			}
		}
#else
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR_DBGBRK, "[ZipDir] HEADERS_CDR_SIGNED not yet supported");
#endif
		break;

#if defined(SUPPORT_UNSIGNED_PAKS)
	case HEADERS_NOT_SIGNED:
		//Nothing to do here
		break;
#endif

	default:
#if !defined(_RELEASE)
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR_DBGBRK, "Unsupported pak signature, or use of unsigned pak when support is disabled.");
#endif  //!_RELEASE
		return false;
		break;
	}

	return true;
}

#if !defined(OPTIMIZED_READONLY_ZIP_ENTRY) && defined(SUPPORT_STREAMCIPHER_PAK_ENCRYPTION)
//////////////////////////////////////////////////////////////////////////
// SECRET
// this function generates the stream cipher key for a file, and should be
// kept secret from hackers - must be a black box to make it as difficult
// as possible for hackers to decrypt our packs
unsigned long ZipDir::GetStreamCipherKey(const FileEntry* inEntry)
{
	// take crc 32, salt it and mod with a large prime to chew it up in a non obvious way
	// do same with file header offset
	// xor them together
	uint32 key = (((inEntry->desc.lCRC32 ^ 0x9b7c9df2) % 3302203733) ^ ((inEntry->nFileDataOffset ^ 0xce30acdf) % 30829));
	//CryLog("got key %x from crc %x file data offset %x",key,inEntry->desc.lCRC32,inEntry->nFileDataOffset);
	return key;
}
#endif

//////////////////////////////////////////////////////////////////////////
bool ZipDir::CacheFactory::DecryptKeysTable()
{
#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION

	unsigned char tmp[1024];
	unsigned long res, len;

	int hash_idx = find_hash("sha256");
	int prng_idx = find_prng("yarrow");

	// Decrypt CDR initial Vector
	{
		int stat = 0;
		len = sizeof(tmp);
		res = ZipEncrypt::custom_rsa_decrypt_key_ex(m_headerEncryption.CDR_IV, sizeof(m_headerEncryption.CDR_IV),
		                                            tmp, &len, NULL, 0, hash_idx, LTC_LTC_PKCS_1_OAEP, &stat, &g_rsa_key_public_for_sign);
		if (res != CRYPT_OK || stat != 1)
			return false;

		assert(sizeof(m_block_cipher_cdr_initial_vector) == len);
		if (sizeof(m_block_cipher_cdr_initial_vector) != len)
			return false;

		// Store RSA Encrypted block cipher key into the archive encryption header.
		memcpy(m_block_cipher_cdr_initial_vector, tmp, sizeof(m_block_cipher_cdr_initial_vector));
	}

	// Decrypt the table of cipher keys.
	for (int i = 0; i < BLOCK_CIPHER_NUM_KEYS; i++)
	{
		int stat = 0;
		len = sizeof(tmp);
		res = ZipEncrypt::custom_rsa_decrypt_key_ex(m_headerEncryption.keys_table[i], sizeof(m_headerEncryption.keys_table[i]),
		                                            tmp, &len, NULL, 0, hash_idx, LTC_LTC_PKCS_1_OAEP, &stat, &g_rsa_key_public_for_sign);
		if (res != CRYPT_OK || stat != 1)
			return false;

		assert(sizeof(m_block_cipher_keys_table[i]) == len);
		if (sizeof(m_block_cipher_keys_table[i]) != len)
			return false;

		// Store RSA Encrypted block cipher key into the archive encryption header.
		memcpy(m_block_cipher_keys_table[i], tmp, sizeof(m_block_cipher_keys_table[i]));
	}
#endif  //SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	return true;
}
