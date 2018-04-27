// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   StreamAsyncFileRequest.h
//  Created:     22/07/2010 by Timur.
//  Description: Streaming Thread for IO
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __StreamAsyncFileRequest_h__
#define __StreamAsyncFileRequest_h__
#pragma once

#include <CrySystem/IStreamEngineDefs.h>
#include <CrySystem/TimeValue.h>
#include <CryCore/Platform/CryWindows.h>

class CStreamEngine;
class CAsyncIOFileRequest;
struct z_stream_s;
class CStreamingIOThread;
struct CCachedFileData;
class CCryFile;
struct SStreamJobEngineState;
class CMTSafeHeap;
class CAsyncIOFileRequest_TransferPtr;
struct SStreamEngineTempMemStats;

#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION  //Could check for INCLUDE_LIBTOMCRYPT here, but only decryption is implemented here, not signing
	#include "tomcrypt.h"
	#include <CryCore/Assert/CryAssert.h> // tomcrypt will re-define assert
	#undef byte                           // tomcrypt defines a byte macro which conflicts with out byte data type
#endif

#if !defined(USE_EDGE_ZLIB)
	#include <zlib.h>
	#include <zutil.h>
	#include <inftrees.h>
	#include <inflate.h>
//	Undefine macros defined in zutil.h to prevent compilation errors in 'steamclientpublic.h', 'OVR_Math.h' etc.
	#undef Assert
	#undef Trace
	#undef Tracev
	#undef Tracevv
	#undef Tracec
	#undef Tracecv
#endif

namespace ZipDir {
struct UncompressLookahead;
}

struct IAsyncIOFileCallback
{
	virtual ~IAsyncIOFileCallback(){}
	// Asynchronous finished event.
	// Must be thread safe, can be called from a different thread.
	virtual void OnAsyncFinished(CAsyncIOFileRequest* pFileRequest) = 0;
};

struct SStreamPageHdr
{
	explicit SStreamPageHdr(int nSize)
		: nRefs()
		, nSize(nSize)
	{}

	volatile int nRefs;
	int          nSize;
};

struct SStreamJobQueue
{
	enum
	{
		MaxJobs = 256,
	};

	struct Job
	{
		void*           pSrc;
		SStreamPageHdr* pSrcHdr;
		uint32          nOffs;
		uint32          nBytes : 31;
		uint32          bLast  : 1;
	};

	SStreamJobQueue()
		: m_sema(MaxJobs, MaxJobs)
	{
		m_nQueueLen = 0;
		m_nPush = 0;
		m_nPop = 0;
		memset(m_jobs, 0, sizeof(m_jobs));
	}

	void Flush(SStreamEngineTempMemStats& tms);

	int  Push(void* pSrc, SStreamPageHdr* pSrcHdr, uint32 nOffs, uint32 nBytes, bool bLast);
	int  Pop();

	CryFastSemaphore m_sema;
	Job              m_jobs[MaxJobs];
	volatile int     m_nQueueLen;
	volatile int     m_nPush;
	volatile int     m_nPop;
};

// This class represent a request to read some file from disk asynchronously via one of the IO threads.
class CAsyncIOFileRequest
{
public:
	enum EStatus
	{
		eStatusNotReady,
		eStatusInFileQueue,
		eStatusFailed,
		eStatusUnzipComplete,
		eStatusDone,
	};

	enum
	{
		BUFFER_ALIGNMENT = 128,
		WINDOW_SIZE      = 1 << 15,

#if CRY_PLATFORM_ANDROID
		STREAMING_PAGE_SIZE = (128 * 1024),
#else
		STREAMING_PAGE_SIZE = (1 * 1024 * 1024),
#endif

#if CRY_PLATFORM_ANDROID
		STREAMING_BLOCK_SIZE = (64 * 1024),
#else
		STREAMING_BLOCK_SIZE = (32 * 1024),
#endif
	};

public:
	static CAsyncIOFileRequest* Allocate(EStreamTaskType eType);
	static void                 Flush();

public:
	void AddRef();
	int  Release();

public:
	void       Init(EStreamTaskType eType);
	void       Finalize();

	void       Reset();

	ILINE bool IsCancelled() const { return m_nError == ERROR_USER_ABORT; }
	ILINE bool HasFailed() const   { return m_nError != 0; }
	void       Failed(uint32 nError)
	{
		CryInterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&m_nError), nError, 0);
	}

	uint32         OpenFile(CCryFile& file);

	uint32         ReadFile(CStreamingIOThread* pIOThread);
	uint32         ReadFileResume(CStreamingIOThread* pIOThread);
	uint32         ReadFileInPages(CStreamingIOThread* pIOThread, CCryFile& file);
	uint32         ReadFileCheckPreempt(CStreamingIOThread* pIOThread);

	uint32         ConfigureRead(CCachedFileData* pFileData);
	bool           CanReadInPages();
	uint32         AllocateOutput(CCachedFileData* pZipEntry);
	unsigned char* AllocatePage(size_t sz, bool bOnlyPakMem, SStreamPageHdr*& pHdrOut);

	static void    JobFinalize_Read(CAsyncIOFileRequest_TransferPtr& pSelf, const SStreamJobEngineState& engineState);

	uint32         PushDecompressPage(const SStreamJobEngineState& engineState, void* pSrc, SStreamPageHdr* pSrcHdr, uint32 nBytes, bool bLast);
	uint32         PushDecompressBlock(const SStreamJobEngineState& engineState, void* pSrc, SStreamPageHdr* pSrcHdr, uint32 nOffs, uint32 nBytes, bool bLast);
	static void    JobStart_Decompress(CAsyncIOFileRequest_TransferPtr& pSelf, const SStreamJobEngineState& engineState, int nSlot);
	void           DecompressBlockEntry(SStreamJobEngineState engineState, int nJob);

#if defined(STREAMENGINE_SUPPORT_DECRYPT)
	uint32      PushDecryptPage(const SStreamJobEngineState& engineState, void* pSrc, SStreamPageHdr* pSrcHdr, uint32 nBytes, bool bLast);
	uint32      PushDecryptBlock(const SStreamJobEngineState& engineState, void* pSrc, SStreamPageHdr* pSrcHdr, uint32 nOffs, uint32 nBytes, bool bLast);
	static void JobStart_Decrypt(CAsyncIOFileRequest_TransferPtr& pSelf, const SStreamJobEngineState& engineState, int nSlot);
	void        DecryptBlockEntry(SStreamJobEngineState engineState, int nJob);
#endif //STREAMENGINE_SUPPORT_DECRYPT

	void                   Cancel();
	bool                   TryCancel();
	void                   SyncWithDecrypt();
	void                   SyncWithDecompress();
	void                   ComputeSortKey(uint64 nCurrentKeyInProgress);
	void                   SetPriority(EStreamTaskPriority estp);
	void                   BumpSweep();
	void                   FreeBuffer();

	bool                   IgnoreOutofTmpMem() const;

	CStreamEngine*         GetStreamEngine();

	EStreamSourceMediaType GetMediaType();

private:
	void* operator new(size_t sz);
	void  operator delete(void* p);

private:
	static void JobFinalize_Decompress(CAsyncIOFileRequest_TransferPtr& pSelf, const SStreamJobEngineState& engineState);
	static void JobFinalize_Decrypt(CAsyncIOFileRequest_TransferPtr& pSelf, const SStreamJobEngineState& engineState);
	static void JobFinalize_Transfer(CAsyncIOFileRequest_TransferPtr& pSelf, const SStreamJobEngineState& engineState);

private:
	void JobFinalize_Buffer(const SStreamJobEngineState& engineState);
	void JobFinalize_Validate(const SStreamJobEngineState& engineState);

private:
	CAsyncIOFileRequest();
	~CAsyncIOFileRequest();

public:
	static volatile int                    s_nLiveRequests;
	static SLockFreeSingleLinkedListHeader s_freeRequests;

public:
	// Must be first
	SLockFreeSingleLinkedListEntry m_nextFree;

	volatile int                   m_nRefCount;

	// Locks to be held whilst the file is being read, and an external memory buffer is in use
	// (to ensure that if cancelled, the stream engine doesn't write to the external buffer)
	// Separate locks for read and decomp as they can overlap (block decompress)
	// Cancel() must acquire both
	CryCriticalSection m_externalBufferLockRead;
	CryCriticalSection m_externalBufferLockDecompress;
#if defined(STREAMENGINE_SUPPORT_DECRYPT)
	CryCriticalSection m_externalBufferLockDecrypt;
#endif  //STREAMENGINE_SUPPORT_DECRYPT

	CryStringLocal m_strFileName;
	string         m_pakFile;

#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	string m_decryptionCTRInitialisedAgainst;
#endif  //SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION

	// If request come from stream, it will be not 0.
	IReadStreamPtr m_pReadStream;

#if defined(STREAMENGINE_SUPPORT_DECRYPT)
	JobManager::SJobState m_DecryptJob;
#endif  //STREAMENGINE_SUPPORT_DECRYPT
	JobManager::SJobState m_DecompJob;

	// Only POD data should exist beyond this point - will be memsetted to 0 on Reset !

	uint64                 m_nSortKey;

	EStreamTaskPriority    m_ePriority;
	EStreamSourceMediaType m_eMediaType;
	EStreamTaskType        m_eType;

	volatile EStatus       m_status;
	volatile uint32        m_nError;

	uint32                 m_nRequestedOffset;
	uint32                 m_nRequestedSize;

	// the file size, or 0 if the file couldn't be opened
	uint32       m_nFileSize;
	uint32       m_nFileSizeCompressed;

	void*        m_pMemoryBuffer;
	uint32       m_nMemoryBufferSize;
	volatile int m_nMemoryBufferUsers;

	void*        m_pExternalMemoryBuffer;
	void*        m_pOutputMemoryBuffer;
	void*        m_pReadMemoryBuffer;
	uint32       m_nReadMemoryBufferSize;

	uint32       m_bCompressedBuffer  : 1;
	uint32       m_bEncryptedBuffer   : 1;
	uint32       m_bStatsUpdated      : 1;
	uint32       m_bStreamInPlace     : 1;
	uint32       m_bWriteOnlyExternal : 1;
	uint32       m_bSortKeyComputed   : 1;
	uint32       m_bOutputAllocated   : 1;
	uint32       m_bReadBegun         : 1;

	// Actual size of the data on the media.
	uint32                m_nSizeOnMedia;

	int64                 m_nDiskOffset;
	int32                 m_nReadHeadOffsetKB; // Offset of the Read Head when reading from media.
	int32                 m_nTimeGroup;
	int32                 m_nSweep;

	IAsyncIOFileCallback* m_pCallback;

	//
	//	Block based streaming
	//

	uint32                       m_nPageReadStart;
	uint32                       m_nPageReadCurrent;
	uint32                       m_nPageReadEnd;

	volatile uint32              m_nBytesDecompressed;
	volatile uint32              m_nBytesDecrypted;

	uint32                       m_crc32FromHeader;

	volatile LONG                m_nFinalised;

	z_stream_s*                  m_pZlibStream;
	ZipDir::UncompressLookahead* m_pLookahead;
	SStreamJobQueue*             m_pDecompQueue;
#if defined(STREAMENGINE_SUPPORT_DECRYPT)
	SStreamJobQueue*             m_pDecryptQueue;
#endif  //STREAMENGINE_SUPPORT_DECRYPT
#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION
	symmetric_CTR* m_pDecryptionCTR;
#endif  //SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION

#ifdef STREAMENGINE_ENABLE_STATS
	// Time that read operation took.
	CTimeValue m_readTime;
	CTimeValue m_unzipTime;
	CTimeValue m_verifyTime;
	CTimeValue m_decryptTime;
	CTimeValue m_startTime;
	CTimeValue m_completionTime;

	uint32     m_nReadCounter;
#endif
};
TYPEDEF_AUTOPTR(CAsyncIOFileRequest);

struct SStreamRequestQueue
{
	CryCriticalSection                m_lock;
	std::vector<CAsyncIOFileRequest*> m_requests;

	CryEvent                          m_awakeEvent;

	SStreamRequestQueue();
	~SStreamRequestQueue();

	void Reset();
	bool IsEmpty() const;

	// Transfers ownership (rather than shares ownership) to the queue
	void TransferRequest(CAsyncIOFileRequest_TransferPtr& pReq);
	bool TryPopRequest(CAsyncIOFileRequest_AutoPtr& pOut);

private:
	SStreamRequestQueue(const SStreamRequestQueue&);
	SStreamRequestQueue& operator=(const SStreamRequestQueue&);
};

#if defined(STREAMENGINE_ENABLE_STATS)
struct SStreamEngineDecompressStats
{
	uint64     m_nTotalBytesUnziped;
	uint64     m_nTempBytesUnziped;
	uint64     m_nTotalBytesDecrypted;
	uint64     m_nTempBytesDecrypted;
	uint64     m_nTotalBytesVerified;
	uint64     m_nTempBytesVerified;

	CTimeValue m_totalUnzipTime;
	CTimeValue m_tempUnzipTime;
	CTimeValue m_totalDecryptTime;
	CTimeValue m_tempDecryptTime;
	CTimeValue m_totalVerifyTime;
	CTimeValue m_tempVerifyTime;
};
#endif

class CAsyncIOFileRequest_TransferPtr
{
public:
	explicit CAsyncIOFileRequest_TransferPtr(CAsyncIOFileRequest* p)
		: m_p(p)
	{
	}

	~CAsyncIOFileRequest_TransferPtr()
	{
		if (m_p)
			m_p->Release();
	}

	CAsyncIOFileRequest*       operator->()       { return m_p; }
	CAsyncIOFileRequest&       operator*()        { return *m_p; }

	const CAsyncIOFileRequest* operator->() const { return m_p; }
	const CAsyncIOFileRequest& operator*() const  { return *m_p; }

	operator bool() const { return m_p != NULL; }

	CAsyncIOFileRequest* Relinquish()
	{
		CAsyncIOFileRequest* p = m_p;
		m_p = NULL;
		return p;
	}

	CAsyncIOFileRequest_TransferPtr& operator=(CAsyncIOFileRequest* p)
	{
#ifndef _RELEASE
		if (m_p)
			__debugbreak();
#endif
		m_p = p;
		return *this;
	}

private:
	CAsyncIOFileRequest_TransferPtr(const CAsyncIOFileRequest_TransferPtr&);
	CAsyncIOFileRequest_TransferPtr& operator=(const CAsyncIOFileRequest_TransferPtr&);

private:
	CAsyncIOFileRequest* m_p;
};

class CStreamEngineWakeEvent
{
public:
	CStreamEngineWakeEvent()
		: m_state(0)
	{
	}

	void Set()
	{
		volatile LONG oldState, newState;
		bool bSignalInner;

		do
		{
			bSignalInner = false;
			oldState = m_state;

			newState = oldState | 0x80000000;
			if (oldState & 0x7fffffff)
			{
				bSignalInner = true;
			}
		}
		while (CryInterlockedCompareExchange(&m_state, newState, oldState) != oldState);

		if (bSignalInner)
		{
			m_innerEvent.Set();
		}
	}

	bool Wait(uint32 timeout = 0)
	{
		bool bTimedOut = false;
		bool bAcquiredSignal = false;

		while (!bTimedOut && !bAcquiredSignal)
		{
			volatile long oldState, newState;
			do
			{
				bAcquiredSignal = false;

				oldState = m_state;
				if (oldState & 0x80000000)
				{
					// Signalled
					newState = oldState & 0x7fffffff;
					bAcquiredSignal = true;
				}
				else
				{
					newState = oldState + 1;
				}
			}
			while (CryInterlockedCompareExchange(&m_state, newState, oldState) != oldState);

			if (!bAcquiredSignal)
			{
				if (!timeout)
				{
					m_innerEvent.Wait();
				}
				else
				{
					bTimedOut = !m_innerEvent.Wait(timeout);
				}

				if (!bTimedOut)
				{
					m_innerEvent.Reset();
				}

				do
				{
					bAcquiredSignal = false;

					oldState = m_state;
					if (!bTimedOut && (oldState & 0x80000000))
					{
						newState = (oldState & 0x7fffffff) - 1;
						bAcquiredSignal = true;
					}
					else
					{
						newState = oldState - 1;
					}
				}
				while (CryInterlockedCompareExchange(&m_state, newState, oldState) != oldState);
			}
		}

		return bAcquiredSignal;
	}

private:
	CStreamEngineWakeEvent(const CStreamEngineWakeEvent&);
	CStreamEngineWakeEvent& operator=(const CStreamEngineWakeEvent&);

private:
	volatile LONG m_state;
	CryEvent      m_innerEvent;
};

struct SStreamEngineTempMemStats
{
	enum
	{
		MaxWakeEvents = 8,
	};

	SStreamEngineTempMemStats()
	{
		memset(this, 0, sizeof(*this));
	}

	void* TempAlloc(CMTSafeHeap* pHeap, size_t nSize, const char* szDbgSource, bool bFallBackToMalloc = true, bool bUrgent = false, uint32 align = 0);
	void  TempFree(CMTSafeHeap* pHeap, const void* p, size_t nSize);
	void  ReportTempMemAlloc(uint32 nSizeAlloc, uint32 nSizeFree, bool bTriggerWake);

	volatile LONG           m_nTempAllocatedMemory;
	volatile LONG           m_nTempAllocatedMemoryFrameMax;
	int                     m_nTempMemoryBudget;
	CStreamEngineWakeEvent* m_wakeEvents[MaxWakeEvents];
	int                     m_nWakeEvents;
};

struct SStreamJobEngineState
{
	std::vector<SStreamRequestQueue*>* pReportQueues;

#if defined(STREAMENGINE_ENABLE_STATS)
	SStreamEngineStatistics*      pStats;
	SStreamEngineDecompressStats* pDecompressStats;
#endif

	SStreamEngineTempMemStats* pTempMem;

	CMTSafeHeap*               pHeap;
};

#endif //StreamAsyncFileRequest
