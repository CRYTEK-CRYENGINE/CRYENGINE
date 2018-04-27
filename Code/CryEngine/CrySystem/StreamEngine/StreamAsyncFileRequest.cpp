// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   StreamAsyncFileRequest.cpp
//  Created:     22/07/2010 by Timur.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>

#include <CrySystem/File/ICryPak.h>
#include "StreamAsyncFileRequest.h"

#include "StreamEngine.h"
#include <CrySystem/Profilers/IDiskProfiler.h>
#include "../System.h"
#include "../CryPak.h"

#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	#include "../ZipEncrypt.h"
#endif

//#pragma optimize("",off)
//#pragma("control %push O=0")             // to disable optimization

extern CMTSafeHeap* g_pPakHeap;

#if defined(STREAMENGINE_ENABLE_STATS)
extern SStreamEngineStatistics* g_pStreamingStatistics;
#endif

extern SStreamEngineOpenStats* g_pStreamingOpenStatistics;

volatile int CAsyncIOFileRequest::s_nLiveRequests;
SLockFreeSingleLinkedListHeader CAsyncIOFileRequest::s_freeRequests;

#ifdef STREAMENGINE_ENABLE_LISTENER

class NotifyListenerIO
{
public:
	NotifyListenerIO(IStreamEngineListener* pL, CAsyncIOFileRequest* pReq, CCachedFileData* pZipEntry, uint32 readSize) : m_pL(pL), m_pReq(pReq)
	{
		if (m_pL)
		{
			m_pL->OnStreamBeginIO(
			  m_pReq,
			  (pZipEntry && pZipEntry->m_pFileEntry->nMethod) ? pZipEntry->GetFileEntry()->desc.lSizeCompressed : m_pReq->m_nFileSize,
			  readSize,
			  pReq->m_eMediaType);
		}
	}
	~NotifyListenerIO()
	{
		End();
	}
	void End()
	{
		if (m_pL)
		{
			m_pL->OnStreamEndIO(m_pReq);
			m_pL = NULL;
		}
	}

private:
	NotifyListenerIO(const NotifyListenerIO&);
	NotifyListenerIO& operator=(const NotifyListenerIO&);

private:
	IStreamEngineListener* m_pL;
	CAsyncIOFileRequest*   m_pReq;
};

#endif //STREAMENGINE_ENABLE_LISTENER

//////////////////////////////////////////////////////////////////////////

void* CAsyncIOFileRequest::operator new(size_t sz)
{
	return CryModuleMemalign(sz, alignof(CAsyncIOFileRequest));
}

void CAsyncIOFileRequest::operator delete(void* p)
{
	CryModuleMemalignFree(p);
}

//////////////////////////////////////////////////////////////////////////
CAsyncIOFileRequest::CAsyncIOFileRequest()
	: m_nRefCount(0)
	, m_pMemoryBuffer(NULL)
{
	Reset();
}

//////////////////////////////////////////////////////////////////////////
CAsyncIOFileRequest::~CAsyncIOFileRequest()
{
#ifndef _RELEASE
	if (m_pMemoryBuffer)
		__debugbreak();
	if (m_eType)
		__debugbreak();
#endif
}

//////////////////////////////////////////////////////////////////////////
uint32 CAsyncIOFileRequest::ConfigureRead(CCachedFileData* pFileData)
{
	ZipDir::FileEntry* pFileEntry = pFileData ? pFileData->m_pFileEntry : NULL;

	if (!pFileData || !pFileEntry->IsCompressed())
	{
		m_bCompressedBuffer = false;
		m_nFileSizeCompressed = m_nFileSize;
		m_nSizeOnMedia = m_nRequestedSize;

		m_nPageReadStart = m_nRequestedOffset;
		m_nPageReadEnd = m_nRequestedOffset + m_nRequestedSize;
	}
	else
	{
		m_bCompressedBuffer = true;
		m_nFileSize = pFileEntry->desc.lSizeUncompressed;
		m_nFileSizeCompressed = pFileEntry->desc.lSizeCompressed;
		m_nSizeOnMedia = m_nFileSizeCompressed;

		m_nPageReadStart = 0;
		m_nPageReadEnd = m_nFileSizeCompressed;
	}

	if (pFileData && !m_bWriteOnlyExternal)
	{
		m_crc32FromHeader = pFileEntry->desc.lCRC32;
	}

	if (pFileData && pFileEntry->IsEncrypted())
	{
		switch (pFileEntry->nMethod)
		{
#if defined(STREAMENGINE_SUPPORT_DECRYPT)
	#if defined(SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION)
		case ZipFile::METHOD_DEFLATE_AND_STREAMCIPHER_KEYTABLE:
		case ZipFile::METHOD_STORE_AND_STREAMCIPHER_KEYTABLE:
			m_bEncryptedBuffer = true;
			break;
	#endif                        //SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
#endif                          //STREAMENGINE_SUPPORT_DECRYPT
		case ZipFile::METHOD_STORE: //This case label is here to fix a warning treated as an error when no valid methods are defined
		default:
#if !defined(_RELEASE)
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR_DBGBRK, "Unsupported encryption method detected in Streaming Engine");
#endif
			return ERROR_DECRYPTION_FAIL;
		}
	}
	else
	{
		m_bEncryptedBuffer = false;
	}

	m_nPageReadCurrent = 0;

	// FIXME later - see FIXME in DecryptBlockEntry if changing how m_bStreamInPlace is inited

	m_bStreamInPlace = !m_bCompressedBuffer || ((!m_pExternalMemoryBuffer || !m_bWriteOnlyExternal) && (m_nFileSize > m_nFileSizeCompressed));
	m_bReadBegun = 1;

	return 0;
}

bool CAsyncIOFileRequest::CanReadInPages()
{
	bool bReadInBlocks = false;

	if (g_cvars.sys_streaming_in_blocks)
	{
		//stream is compressed and uncompressed size is greater than one page
		if (!m_bCompressedBuffer || m_nFileSizeCompressed > STREAMING_BLOCK_SIZE)
		{
			bReadInBlocks = true;
		}
	}

	return bReadInBlocks;
}

uint32 CAsyncIOFileRequest::AllocateOutput(CCachedFileData* pZipEntry)
{
	if (!m_bOutputAllocated)
	{
		uint32 nAllocSize = 0;
		uint32 nReadAllocSize = 0;
		uint32 nZStreamOffs = 0;
		uint32 nLookaheadOffs = 0;

		if (m_pExternalMemoryBuffer)
		{
			nReadAllocSize = m_bCompressedBuffer
			                 ? (m_nRequestedSize < m_nFileSize ? m_nFileSize : 0)
			                 : 0;
		}
		else
		{
			nReadAllocSize = m_bCompressedBuffer
			                 ? m_nFileSize
			                 : m_nRequestedSize;
		}

		nAllocSize = Align(nReadAllocSize, BUFFER_ALIGNMENT);

		bool bReadInBlocks = CanReadInPages();
		bool bNeedsLookahead = m_bStreamInPlace;
		bool bBlockDecompress = m_bCompressedBuffer && bReadInBlocks;

		if (bBlockDecompress)
		{
			nZStreamOffs = nAllocSize;
			nAllocSize += Align(sizeof(z_stream), BUFFER_ALIGNMENT);

			if (bNeedsLookahead)
			{
				nLookaheadOffs = nAllocSize;
				nAllocSize += Align(sizeof(*m_pLookahead), BUFFER_ALIGNMENT);
			}
		}

		char* pBuffer = NULL;

		if (nAllocSize)
		{
#if defined(INCLUDE_MEMSTAT_CONTEXTS)
			char usageHint[512];
			cry_sprintf(usageHint, "AsyncIO TempBuffer: %s", m_strFileName.c_str());
#else
			const char* usageHint = "AsyncIO TempBuffer";
#endif

			pBuffer = (char*)GetStreamEngine()->TempAlloc(nAllocSize, usageHint, true, IgnoreOutofTmpMem(), BUFFER_ALIGNMENT);
			if (!pBuffer)
				return ERROR_OUT_OF_MEMORY;
		}

		if (pBuffer)
		{
			m_pMemoryBuffer = pBuffer;
			m_nMemoryBufferSize = nAllocSize;
		}

		if (nReadAllocSize)
		{
			m_pReadMemoryBuffer = pBuffer;
			m_nReadMemoryBufferSize = nReadAllocSize;
		}
		else
		{
			m_pReadMemoryBuffer = m_pExternalMemoryBuffer;
			m_nReadMemoryBufferSize = m_nRequestedSize;
		}

		m_pOutputMemoryBuffer = m_pExternalMemoryBuffer
		                        ? m_pExternalMemoryBuffer
		                        : m_pReadMemoryBuffer;

		if (bBlockDecompress)
		{
			m_pZlibStream = (z_stream*)&pBuffer[nZStreamOffs];
			memset(m_pZlibStream, 0, sizeof(z_stream));

			if (bNeedsLookahead)
			{
				m_pLookahead = new(&pBuffer[nLookaheadOffs]) ZipDir::UncompressLookahead;
			}

			m_pZlibStream->zalloc = CMTSafeHeap::StaticAlloc;
			m_pZlibStream->zfree = CMTSafeHeap::StaticFree;
			m_pZlibStream->opaque = g_pPakHeap;
		}

		if (m_bCompressedBuffer)
		{
			m_pDecompQueue = new SStreamJobQueue;
		}

#if defined(STREAMENGINE_SUPPORT_DECRYPT)
		if (m_bEncryptedBuffer)
		{
			m_pDecryptQueue = new SStreamJobQueue;
	#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
			m_pDecryptionCTR = new symmetric_CTR;
	#endif
		}

#endif  //STREAMENGINE_SUPPORT_DECRYPT

		// Doesn't need to be atomic, as there's no concurrency yet.
		int nMemoryBufferUsers = 0;
		if (nReadAllocSize > 0)
			++nMemoryBufferUsers;
		if (m_bCompressedBuffer)
			++nMemoryBufferUsers;
		if (m_bEncryptedBuffer)
			++nMemoryBufferUsers;
		m_nMemoryBufferUsers = nMemoryBufferUsers;

		m_bOutputAllocated = 1;
	}

#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	// In case the file was misscheduled, and the pak changes, the decryption
	// key will also change. Make sure that we reinitialise in that case.
	if (m_decryptionCTRInitialisedAgainst != m_pakFile)
	{
		if (m_pDecryptionCTR)
		{
			// Wait for any decryption tasks using the old key to complete first...
			SyncWithDecrypt();

			unsigned char IV[ZipFile::BLOCK_CIPHER_KEY_LENGTH]; //16 byte
			int nKeyIndex = ZipEncrypt::GetEncryptionKeyIndex(pZipEntry->m_pFileEntry);
			ZipEncrypt::GetEncryptionInitialVector(pZipEntry->m_pFileEntry, IV);
			if (!ZipEncrypt::StartStreamCipher(pZipEntry->m_pZip->GetBlockCipherKeyTable(nKeyIndex), IV, m_pDecryptionCTR, m_nPageReadStart))
			{
	#if !defined(_RELEASE)
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Failed to initialise a symmetric encryption cipher for a streaming file");
	#endif
				return ERROR_DECRYPTION_FAIL;
			}
		}

		m_decryptionCTRInitialisedAgainst = m_pakFile;
	}
#endif

	return 0;
}

byte* CAsyncIOFileRequest::AllocatePage(size_t sz, bool bOnlyPakMem, SStreamPageHdr*& pHdrOut)
{
#if defined(INCLUDE_MEMSTAT_CONTEXTS)
	char usageHint[512];
	cry_sprintf(usageHint, "streaming page: %s", m_strFileName.c_str());
#else
	const char* usageHint = "streaming page";
#endif

	size_t nSzAligned = Align(sz, BUFFER_ALIGNMENT);
	size_t nToAlloc = nSzAligned + sizeof(SStreamPageHdr);
	byte* pRet = (byte*)GetStreamEngine()->TempAlloc(nToAlloc, usageHint, true, !bOnlyPakMem, BUFFER_ALIGNMENT);
	if (pRet)
		pHdrOut = new(pRet + nSzAligned)SStreamPageHdr(nToAlloc);

	return pRet;
}

//////////////////////////////////////////////////////////////////////////
void CAsyncIOFileRequest::Cancel()
{
	if (!HasFailed())
	{
		CryOptionalAutoLock<CryCriticalSection> readLock(m_externalBufferLockRead, m_pExternalMemoryBuffer != NULL);
		CryOptionalAutoLock<CryCriticalSection> decompLock(m_externalBufferLockDecompress, m_pExternalMemoryBuffer != NULL);
#if defined(STREAMENGINE_SUPPORT_DECRYPT)
		CryOptionalAutoLock<CryCriticalSection> decryptLock(m_externalBufferLockDecrypt, m_pExternalMemoryBuffer != NULL);
#endif  //STREAMENGINE_SUPPORT_DECRYPT

		Failed(ERROR_USER_ABORT);
	}
}

void CAsyncIOFileRequest::SyncWithDecrypt()
{
#if defined(STREAMENGINE_SUPPORT_DECRYPT)
	m_DecryptJob.Wait();
#endif  //STREAMENGINE_SUPPORT_DECRYPT
}

void CAsyncIOFileRequest::SyncWithDecompress()
{
	m_DecompJob.Wait();
}

//////////////////////////////////////////////////////////////////////////
bool CAsyncIOFileRequest::TryCancel()
{
	if (!HasFailed())
	{
		bool bExt = false;
		if (m_pExternalMemoryBuffer != NULL)
		{
			if (!m_externalBufferLockRead.TryLock())
				return false;
			if (!m_externalBufferLockDecompress.TryLock())
			{
				m_externalBufferLockRead.Unlock();
				return false;
			}
#if defined(STREAMENGINE_SUPPORT_DECRYPT)
			if (!m_externalBufferLockDecrypt.TryLock())
			{
				m_externalBufferLockDecompress.Unlock();
				m_externalBufferLockRead.Unlock();
				return false;
			}
#endif  //STREAMENGINE_SUPPORT_DECRYPT
			bExt = true;
		}

		Failed(ERROR_USER_ABORT);

		if (bExt)
		{
#if defined(STREAMENGINE_SUPPORT_DECRYPT)
			m_externalBufferLockDecrypt.Unlock();
#endif
			m_externalBufferLockDecompress.Unlock();
			m_externalBufferLockRead.Unlock();
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CAsyncIOFileRequest::FreeBuffer()
{
	if (m_pZlibStream)
	{
		//if the stream was cancelled in flight, inform zlib to free internal allocs
		if (m_pZlibStream->state)
		{
			inflateEnd(m_pZlibStream);
		}

		m_pZlibStream = NULL;
	}

	m_pLookahead = NULL;

	SStreamEngineTempMemStats& tms = GetStreamEngine()->GetTempMemStats();

	if (m_pDecompQueue)
	{
		m_pDecompQueue->Flush(tms);
		delete m_pDecompQueue;
		m_pDecompQueue = NULL;
	}

#if defined(STREAMENGINE_SUPPORT_DECRYPT)
	if (m_pDecryptQueue)
	{
		m_pDecryptQueue->Flush(tms);
		delete m_pDecryptQueue;
		m_pDecryptQueue = NULL;
	}

	#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	if (m_pDecryptionCTR)
	{
		ZipEncrypt::FinishStreamCipher(m_pDecryptionCTR);
		delete m_pDecryptionCTR;
		m_pDecryptionCTR = NULL;
	}
	#endif

#endif  //STREAMENGINE_SUPPORT_DECRYPT

	if (m_pMemoryBuffer)
	{
		CStreamEngine* pStreamEngine = GetStreamEngine();

		pStreamEngine->TempFree(m_pMemoryBuffer, m_nMemoryBufferSize);
		m_pMemoryBuffer = 0;
	}

#ifdef STREAMENGINE_ENABLE_STATS
	// Update Streaming statistics.
	if (g_pStreamingStatistics && m_nSizeOnMedia != 0 && m_bStatsUpdated)
	{
		m_bStatsUpdated = false;
		int nSize = (int)m_nSizeOnMedia;
		SStreamEngineStatistics& stats = *g_pStreamingStatistics;
		CryInterlockedAdd(&stats.nPendingReadBytes, -nSize);
		CryInterlockedAdd(&stats.typeInfo[m_eType].nPendingReadBytes, -nSize);
	}
#endif
}

CAsyncIOFileRequest* CAsyncIOFileRequest::Allocate(EStreamTaskType eType)
{
	CAsyncIOFileRequest* pReq = static_cast<CAsyncIOFileRequest*>(CryInterlockedPopEntrySList(s_freeRequests));
	IF_UNLIKELY (!pReq)
	{
		pReq = new CAsyncIOFileRequest;
	}

	pReq->Init(eType);

	return pReq;
}

void CAsyncIOFileRequest::Flush()
{
	for (CAsyncIOFileRequest* pReq = static_cast<CAsyncIOFileRequest*>(CryInterlockedPopEntrySList(s_freeRequests));
	     pReq;
	     pReq = static_cast<CAsyncIOFileRequest*>(CryInterlockedPopEntrySList(s_freeRequests)))
	{
		delete pReq;
	}
}

void CAsyncIOFileRequest::Reset()
{
#ifndef _RELEASE
	if (m_pMemoryBuffer)
		__debugbreak();
#endif

	m_pReadStream = NULL;
	m_strFileName.resize(0);
	m_pakFile.resize(0);

#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	m_decryptionCTRInitialisedAgainst = m_pakFile;
#endif

	// Reset POD members of the structure
	memset(&m_nSortKey, 0, ((char*)(this + 1) - (char*)&m_nSortKey));
}

void CAsyncIOFileRequest::Init(EStreamTaskType eType)
{
#ifndef _RELEASE
	if (!eType)
		__debugbreak();
#endif

	m_eType = eType;

#ifdef STREAMENGINE_ENABLE_STATS
	m_startTime = gEnv->pTimer->GetAsyncTime();
#endif

	if (g_pStreamingOpenStatistics)
	{
		SStreamEngineOpenStats& stats = *g_pStreamingOpenStatistics;
		CryInterlockedIncrement(&stats.nOpenRequestCount);
		CryInterlockedIncrement(&stats.nOpenRequestCountByType[eType]);
	}

	CryInterlockedIncrement(&s_nLiveRequests);
}

void CAsyncIOFileRequest::Finalize()
{
#ifndef _RELEASE
	if (!m_eType)
		__debugbreak();
#endif

#ifdef STREAMENGINE_ENABLE_LISTENER
	IStreamEngineListener* pListener = gEnv->pSystem->GetStreamEngine()->GetListener();
	if (pListener)
		pListener->OnStreamDone(this);
#endif

	if (g_pStreamingOpenStatistics)
	{
		SStreamEngineOpenStats& stats = *g_pStreamingOpenStatistics;
		CryInterlockedDecrement(&stats.nOpenRequestCount);
		CryInterlockedDecrement(&stats.nOpenRequestCountByType[m_eType]);
	}

	CryInterlockedDecrement(&s_nLiveRequests);

	FreeBuffer();
	Reset();
}

uint32 CAsyncIOFileRequest::OpenFile(CCryFile& file)
{
	ICryPak* pIPak = gEnv ? gEnv->pCryPak : NULL;
	PREFAST_ASSUME(pIPak);

	CryStackStringT<char, MAX_PATH> fileName(m_strFileName.c_str());
	if (m_pReadStream && m_pReadStream->GetParams().nFlags & IStreamEngine::FLAGS_FILE_ON_DISK)
	{
		const int g_nMaxPath = 0x800;
		char szFullPathBuf[g_nMaxPath];
		fileName = pIPak->AdjustFileName(m_strFileName.c_str(), szFullPathBuf, ICryPak::FOPEN_HINT_QUIET);

		pIPak = 0;
	}

	file = CCryFile(pIPak);
	if (!file.Open(fileName.c_str(), "rb", ICryPak::FOPEN_FORSTREAMING))
	{
		return ERROR_CANT_OPEN_FILE;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
uint32 CAsyncIOFileRequest::ReadFile(CStreamingIOThread* pIOThread)
{
	uint32 nError = m_nError;

	if (nError)
		return nError;

	CCryFile file;
	if ((nError = OpenFile(file)))
		return nError;

	m_nFileSize = file.GetLength();

	if (m_nRequestedOffset >= m_nFileSize)
		return ERROR_OFFSET_OUT_OF_RANGE;

	if (m_nRequestedOffset + m_nRequestedSize > m_nFileSize)
		return ERROR_SIZE_OUT_OF_RANGE;

	if (m_nRequestedSize == 0)
	{
		// by default, we read the whole file
		m_nRequestedSize = m_nFileSize - m_nRequestedOffset;
	}

	if (!m_pExternalMemoryBuffer && m_pReadStream)
	{
		bool bAbortOnFailToAlloc = false;
		CReadStream* pReadStream = static_cast<CReadStream*>(&*m_pReadStream);
		m_pExternalMemoryBuffer = pReadStream->OnNeedStorage(m_nFileSize, bAbortOnFailToAlloc);

		if (!m_pExternalMemoryBuffer && bAbortOnFailToAlloc)
		{
			Cancel();
			m_pReadStream->Abort();
			return ERROR_USER_ABORT;
		}
	}

	if (HasFailed())
		return m_nError;

	CCachedFileDataPtr pZipEntry = ((CCryPak*)(gEnv->pCryPak))->GetOpenedFileDataInZip(file.GetHandle());
	if ((nError = ConfigureRead(pZipEntry)))
		return nError;

	if ((nError = AllocateOutput(pZipEntry)))
		return nError;

	return ReadFileInPages(pIOThread, file);
}

uint32 CAsyncIOFileRequest::ReadFileResume(CStreamingIOThread* pIOThread)
{
	uint32 nError = m_nError;

	if (nError)
		return nError;

	CCryFile file;
	if ((nError = OpenFile(file)))
		return nError;

	CCachedFileDataPtr pZipEntry = ((CCryPak*)(gEnv->pCryPak))->GetOpenedFileDataInZip(file.GetHandle());

	if ((nError = AllocateOutput(pZipEntry)))
		return nError;

#ifdef STREAMENGINE_ENABLE_LISTENER
	IStreamEngineListener* pListener = gEnv->pSystem->GetStreamEngine()->GetListener();
	if (pListener)
		pListener->OnStreamResumed(this);
#endif

	return ReadFileInPages(pIOThread, file);
}

uint32 CAsyncIOFileRequest::ReadFileInPages(CStreamingIOThread* pIOThread, CCryFile& file)
{
#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	const char* pFileNameShort = PathUtil::GetFile(m_strFileName.c_str());
	char eventName[128] = { 0 };
	cry_sprintf(eventName, "ReadFileInPages %s", pFileNameShort);
	CRY_PROFILE_REGION(PROFILE_SYSTEM, "ReadFileInPages");
	CRYPROFILE_SCOPE_PROFILE_MARKER(eventName);
	CRYPROFILE_SCOPE_PLATFORM_MARKER(eventName);
#endif

	CCryPak* pCryPak = static_cast<CCryPak*>(gEnv->pCryPak);
	CCachedFileDataPtr pZipEntry = pCryPak->GetOpenedFileDataInZip(file.GetHandle());

	ZipDir::Cache* pZip = NULL;
	unsigned int nZipFlags = 0;
	if (pZipEntry)
	{
		pZip = pZipEntry->GetZip();
		nZipFlags = pZipEntry->m_nArchiveFlags;
	}

	if (pIOThread->IsMisscheduled(pCryPak->GetMediaType(pZip, nZipFlags)))
	{
		// We're on the wrong IO thread!
		return ERROR_MISSCHEDULED;
	}

	bool bReadInPages = CanReadInPages();

	uint32 nPageReadLen = (m_nPageReadEnd - m_nPageReadStart);

	bool const bCompressed = m_bCompressedBuffer;
	bool const bEncrypted = m_bEncryptedBuffer;
	bool const bInPlace = m_bStreamInPlace;
	bool const bIgnoreOutOfTmp = IgnoreOutofTmpMem();

	size_t const nReadStartOffset = bCompressed
	                                ? (m_nFileSize - m_nFileSizeCompressed)
	                                : 0;

	byte* const pReadBase = (byte*)m_pReadMemoryBuffer + nReadStartOffset;
	byte* const pReadEnd = (byte*)m_pReadMemoryBuffer + m_nReadMemoryBufferSize;

	CStreamEngine* pStreamEngine = static_cast<CStreamEngine*>(gEnv->pSystem->GetStreamEngine());

	uint32 nPageSize = bReadInPages
	                   ? min((uint32)STREAMING_PAGE_SIZE, nPageReadLen - m_nPageReadCurrent)
	                   : nPageReadLen - m_nPageReadCurrent;

	while (nPageSize > 0)
	{
		CryOptionalAutoLock<CryCriticalSection> readLock(m_externalBufferLockRead, m_pExternalMemoryBuffer != NULL);

		uint32 nError = m_nError;

		if (nError)
			return nError;

		//check if job needs to be pre-empted
		if ((nError = ReadFileCheckPreempt(pIOThread)))
			return nError;

		byte* pReadTarget = pReadBase + m_nPageReadCurrent;
		byte* pReadTargetEnd = pReadTarget + nPageSize;
		bool bTemporaryReadTarget = false;
		SStreamPageHdr* pTemporaryPageHdr = NULL;

		if (bInPlace)
		{
			if (pReadTargetEnd > pReadEnd)
				__debugbreak();
		}
		else
		{
			pReadTarget = AllocatePage(nPageSize, !bIgnoreOutOfTmp, pTemporaryPageHdr);

			if (!pReadTarget)
				return ERROR_OUT_OF_MEMORY;

			bTemporaryReadTarget = true;
			pTemporaryPageHdr->nRefs = 1;
		}

#ifndef _RELEASE
		if (m_nPageReadCurrent + nPageSize > nPageReadLen)
			__debugbreak();
#endif

		{
#ifdef STREAMENGINE_ENABLE_LISTENER
			NotifyListenerIO IOListener(gEnv->pSystem->GetStreamEngine()->GetListener(), this, pZipEntry, nPageSize);
#endif

#ifdef STREAMENGINE_ENABLE_STATS
			CTimeValue t0 = gEnv->pTimer->GetAsyncTime();
#endif

			//printf("[StreamRead] %p %i %p %i %i\n", this, m_bCompressedBuffer, pReadTarget, m_nPageReadStart + m_nPageReadCurrent, nPageSize);

			bool bReadOk = false;

			if (pZipEntry)
			{
				bReadOk = pZipEntry->m_pZip->ReadFileStreaming(pZipEntry->m_pFileEntry, pReadTarget, m_nPageReadStart + m_nPageReadCurrent, nPageSize) == ZipDir::ZD_ERROR_SUCCESS;
			}
			else
			{
				file.Seek(m_nPageReadStart + m_nPageReadCurrent, SEEK_SET);
				bReadOk = file.ReadRaw(pReadTarget, nPageSize) == nPageSize;
			}

			if (bReadOk)
			{
				//send each block to listener
#ifdef STREAMENGINE_ENABLE_LISTENER
				IOListener.End();
#endif

#ifdef STREAMENGINE_ENABLE_STATS
				m_readTime += gEnv->pTimer->GetAsyncTime() - t0;
#endif

				//release external mem lock, allows jobs to be cancelled mid stream
				readLock.Release();
			}
			else
			{
				if (bTemporaryReadTarget)
					GetStreamEngine()->TempFree(pReadTarget, pTemporaryPageHdr->nSize);

				return ERROR_REFSTREAM_ERROR;
			}

			bool bLastBlock = (m_nPageReadCurrent + nPageSize) == nPageReadLen;

#if defined(STREAMENGINE_SUPPORT_DECRYPT)
			if (bEncrypted)
			{
				PushDecryptPage(pStreamEngine->GetJobEngineState(), pReadTarget, pTemporaryPageHdr, nPageSize, bLastBlock);
			}
			else
#endif                 //STREAMENGINE_SUPPORT_DECRYPT
			if (bCompressed) //Spawn the decompression jobs here only if the file isn't encrypted. Encryption and Decompression are strictly linear, the decryption jobs will spawn decompression jobs as they complete.
			{
				PushDecompressPage(pStreamEngine->GetJobEngineState(), pReadTarget, pTemporaryPageHdr, nPageSize, bLastBlock);
			}
			else if (bTemporaryReadTarget)
			{
				__debugbreak();
			}

			if (pTemporaryPageHdr && CryInterlockedDecrement(&pTemporaryPageHdr->nRefs) == 0)
				GetStreamEngine()->TempFree(pReadTarget, pTemporaryPageHdr->nSize);

			m_nPageReadCurrent += nPageSize;
			nPageSize = min((uint32)STREAMING_PAGE_SIZE, nPageReadLen - m_nPageReadCurrent);
		}
	}

	return 0;
}

uint32 CAsyncIOFileRequest::ReadFileCheckPreempt(CStreamingIOThread* pIOThread)
{
	if (m_ePriority != estpUrgent)
	{
		if (pIOThread->HasUrgentRequests())
		{
			//printf("Read Job %s pre-empted mid stream. Progress %d / %d bytes\n", m_strFileName.c_str(), m_nBytesRead, m_nFileSizeCompressed);
#ifdef STREAMENGINE_ENABLE_LISTENER
			IStreamEngineListener* pListener = gEnv->pSystem->GetStreamEngine()->GetListener();
			if (pListener)
				pListener->OnStreamPreempted(this);
#endif
			return ERROR_PREEMPTED;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
CStreamEngine* CAsyncIOFileRequest::GetStreamEngine()
{
	return (CStreamEngine*)GetISystem()->GetStreamEngine();
}

//////////////////////////////////////////////////////////////////////////
EStreamSourceMediaType CAsyncIOFileRequest::GetMediaType()
{
	EStreamSourceMediaType mediaType = m_eMediaType;

	if (mediaType == eStreamSourceTypeUnknown)
	{
		if (m_bSortKeyComputed)
		{
			return mediaType;
		}

		if (m_strFileName.empty())
		{
			mediaType = eStreamSourceTypeMemory;
			return mediaType;
		}

		mediaType = gEnv->pCryPak->GetFileMediaType(m_strFileName.c_str());
	}

	return mediaType;
}

//////////////////////////////////////////////////////////////////////////
void CAsyncIOFileRequest::ComputeSortKey(uint64 nCurrentKeyInProgress)
{
	if (m_bSortKeyComputed)
		return;

	m_bSortKeyComputed = true;

	if (m_strFileName.empty())
	{
		m_eMediaType = eStreamSourceTypeMemory;
		m_nSortKey = m_ePriority;
		return;
	}
	if (HasFailed())
	{
		m_nSortKey = 0;
		return;
	}

	const int g_nMaxPath = 0x800;
	char szFullPathBuf[g_nMaxPath];
	const char* szFullPath = gEnv->pCryPak->AdjustFileName(m_strFileName.c_str(), szFullPathBuf, ICryPak::FOPEN_HINT_QUIET);

	CCryPak* pCryPak = static_cast<CCryPak*>(gEnv->pCryPak);

	ZipDir::CachePtr pZip = 0;
	unsigned int archFlags = 0;
	ZipDir::FileEntry* pFileEntry = NULL;

	if (pCryPak->WillOpenFromPak(szFullPath))
		pFileEntry = pCryPak->FindPakFileEntry(szFullPath, archFlags, &pZip, false);

	EStreamSourceMediaType ssmt = pCryPak->GetMediaType(pZip, archFlags);

	if (pFileEntry)
	{
		m_nDiskOffset = pZip->GetPakFileOffsetOnMedia() + (uint64)pFileEntry->nFileDataOffset;

		m_nSizeOnMedia = pFileEntry->desc.lSizeCompressed;
		m_nFileSize = pFileEntry->desc.lSizeUncompressed;

		m_pakFile = pZip->GetFilePath();
	}

	m_eMediaType = ssmt;

	if (ssmt != eStreamSourceTypeMemory)
	{
		int32 nCurrentSweep = (nCurrentKeyInProgress >> 30) & ((1 << 10) - 1);
		int32 nCurrentTG = (nCurrentKeyInProgress >> 40) & ((1 << 20) - 1);

		if (pFileEntry && !pFileEntry->IsCompressed())
			m_nDiskOffset += m_nRequestedOffset;

		// group items by priority, then by snapped request time, then sort by disk offset
		m_nTimeGroup = (uint64)(gEnv->pTimer->GetAsyncTime().GetSeconds() / max(1, g_cvars.sys_streaming_requests_grouping_time_period));
		m_nSweep = (m_nTimeGroup == nCurrentTG)
		           ? nCurrentSweep
		           : 0;
		uint64 nPrioriry = m_ePriority;

		int64 nDiskOffsetKB = m_nDiskOffset >> 10; // KB
		m_nSortKey = (nDiskOffsetKB) | (((uint64)m_nTimeGroup) << 40) | ((uint64)m_nSweep << 30) | (nPrioriry << 60);

		// make sure we do not break incremental head movement within time group on every new request
		if (m_nSortKey <= nCurrentKeyInProgress)
		{
			++m_nSweep;
			m_nSortKey = (nDiskOffsetKB) | (((uint64)m_nTimeGroup) << 40) | ((uint64)m_nSweep << 30) | (nPrioriry << 60);
		}
	}
	else
	{
		m_nSortKey = m_ePriority;
	}

#if defined(STREAMENGINE_ENABLE_STATS)
	// Update Streaming statistics.
	if (!m_bStatsUpdated && g_pStreamingStatistics && m_nSizeOnMedia != 0)
	{
		m_bStatsUpdated = true;
		SStreamEngineStatistics& stats = *g_pStreamingStatistics;

		// if the file is not compressed then it will only read the requested size
		uint32 nReadSize = m_nSizeOnMedia;
		if (m_nSizeOnMedia == m_nFileSize)
			nReadSize = m_nRequestedSize;

		CryInterlockedAdd(&stats.nPendingReadBytes, nReadSize);
		CryInterlockedAdd(&stats.typeInfo[m_eType].nPendingReadBytes, nReadSize);
	}
#endif
}

void CAsyncIOFileRequest::SetPriority(EStreamTaskPriority estp)
{
	m_ePriority = estp;

	if (m_eMediaType != eStreamSourceTypeMemory)
	{
		m_nSortKey &= ~(15ULL << 60);
		m_nSortKey |= static_cast<uint64>(estp) << 60;
	}
	else
	{
		m_nSortKey = m_ePriority;
	}
}

void CAsyncIOFileRequest::BumpSweep()
{
	++m_nSweep;
	if (m_eMediaType != eStreamSourceTypeMemory)
	{
		m_nSortKey += 1 << 30;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAsyncIOFileRequest::IgnoreOutofTmpMem() const
{
	if (m_pReadStream &&
	    (m_pReadStream->GetParams().ePriority == estpUrgent ||
	     m_pReadStream->GetParams().nFlags & IStreamEngine::FLAGS_IGNORE_TMP_OUT_OF_MEM))
	{
		return true;
	}

	return false;
}

SStreamRequestQueue::SStreamRequestQueue()
{
	m_requests.reserve(4096);
}

SStreamRequestQueue::~SStreamRequestQueue()
{
	Reset();
}

void SStreamRequestQueue::Reset()
{
	CryAutoLock<CryCriticalSection> l(m_lock);
	for (size_t i = 0, c = m_requests.size(); i != c; ++i)
		m_requests[i]->Release();
	m_requests.clear();
}

bool SStreamRequestQueue::IsEmpty() const
{
	return m_requests.empty();
}

bool SStreamRequestQueue::TryPopRequest(CAsyncIOFileRequest_AutoPtr& pOut)
{
	CryAutoLock<CryCriticalSection> l(m_lock);
	if (!m_requests.empty())
	{
		pOut = m_requests.front();
		pOut->Release();
		m_requests.erase(m_requests.begin());
		return true;
	}
	return false;
}

void* SStreamEngineTempMemStats::TempAlloc(CMTSafeHeap* pHeap, size_t nSize, const char* szDbgSource, bool bFallBackToMalloc, bool bUrgent, uint32 align)
{
	// Only allow falling back to malloc if the size fits within the stream budget, the request is urgent, or the temp memory is 0 - for those files that are over budget on their own.
	long nInUse = m_nTempAllocatedMemory;
	bFallBackToMalloc =
	  bUrgent
	  || (bFallBackToMalloc && ((nInUse == 0) || (nInUse + static_cast<long>(nSize) <= m_nTempMemoryBudget)))
	;

	void* p = pHeap->TempAlloc(nSize, szDbgSource, bFallBackToMalloc, align);
#if MTSAFE_USE_GENERAL_HEAP
	bool bInGenHeap = pHeap->IsInGeneralHeap(p);
#else
	bool bInGenHeap = false;
#endif
	if (p && !bInGenHeap)
		ReportTempMemAlloc(nSize, 0, false);

	return p;
}
