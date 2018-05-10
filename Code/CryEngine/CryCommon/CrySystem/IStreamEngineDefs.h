// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ISTREAMENGINEDEFS_H
#define ISTREAMENGINEDEFS_H

#if defined(ENABLE_PROFILING_CODE)
	#define STREAMENGINE_ENABLE_LISTENER
	#define STREAMENGINE_ENABLE_STATS
#endif

#ifdef SUPPORT_RSA_AND_STREAMCIPHER_PAK_ENCRYPTION  //Could check for INCLUDE_LIBTOMCRYPT here, but only decryption is implemented in the streaming engine, not signing
	#define STREAMENGINE_SUPPORT_DECRYPT
#endif

enum : uint32
{
	ERROR_UNKNOWN_ERROR          = 0xF0000000,
	ERROR_UNEXPECTED_DESTRUCTION = 0xF0000001,
	ERROR_INVALID_CALL           = 0xF0000002,
	ERROR_CANT_OPEN_FILE         = 0xF0000003,
	ERROR_REFSTREAM_ERROR        = 0xF0000004,
	ERROR_OFFSET_OUT_OF_RANGE    = 0xF0000005,
	ERROR_REGION_OUT_OF_RANGE    = 0xF0000006,
	ERROR_SIZE_OUT_OF_RANGE      = 0xF0000007,
	ERROR_CANT_START_READING     = 0xF0000008,
	ERROR_OUT_OF_MEMORY          = 0xF0000009,
	ERROR_ABORTED_ON_SHUTDOWN    = 0xF000000A,
	ERROR_OUT_OF_MEMORY_QUOTA    = 0xF000000B,
	ERROR_ZIP_CACHE_FAILURE      = 0xF000000C,
	ERROR_USER_ABORT             = 0xF000000D,
	ERROR_DECRYPTION_FAIL        = 0xF000000E,
	ERROR_MISSCHEDULED           = 0xF000000F,
	ERROR_VERIFICATION_FAIL      = 0xF0000010,
	ERROR_PREEMPTED              = 0xF0000011,
	ERROR_DECOMPRESSION_FAIL     = 0xF0000012
};

//! Types of streaming tasks.
//! Affects priority directly.
enum EStreamTaskType
{
	eStreamTaskTypeCount      = 14,
	eStreamTaskTypeGeomCache  = 13,
	eStreamTaskTypePak        = 12,
	eStreamTaskTypeFlash      = 11,
	eStreamTaskTypeVideo      = 10,

	eStreamTaskTypeMergedMesh = 9,
	eStreamTaskTypeShader     = 8,
	eStreamTaskTypeSound      = 7,
	eStreamTaskTypeFSBCache   = 5,
	eStreamTaskTypeAnimation  = 4,
	eStreamTaskTypeTerrain    = 3,
	eStreamTaskTypeGeometry   = 2,
	eStreamTaskTypeTexture    = 1,
};

//! Priority types of streaming tasks.
//! Affects priority directly.
//! Limiting number of priority values allows streaming system to minimize seek time.
enum EStreamTaskPriority
{
	estpUrgent      = 0,
	estpPreempted   = 1,  //!< For internal use only.
	estpAboveNormal = 2,
	estpNormal      = 3,
	estpBelowNormal = 4,
	estpIdle        = 5,
};

enum EStreamSourceMediaType
{
	eStreamSourceTypeUnknown = 0,
	eStreamSourceTypeHDD,
	eStreamSourceTypeDisc,
	eStreamSourceTypeMemory,
};

#if defined(STREAMENGINE_ENABLE_STATS)
struct SStreamEngineStatistics
{
	struct SMediaTypeInfo
	{
		SMediaTypeInfo()
		{
			ResetStats();
		}
		void ResetStats()
		{
			memset(this, 0, sizeof(SMediaTypeInfo));
		}

		float  fActiveDuringLastSecond; //!< Amount of time media device was active during last second.
		float  fAverageActiveTime;      //!< Average time since last reset that the media device was active.

		uint32 nBytesRead;              //!< Bytes read during last second.
		uint32 nRequestCount;           //!< Amount of requests during last second.
		uint64 nTotalBytesRead;         //!< Read bytes total from reset.
		uint32 nTotalRequestCount;      //!< Number of request from reset.

		uint64 nSeekOffsetLastSecond;   //!< Average seek offset during the last second.
		uint64 nAverageSeekOffset;      //!< Average seek offset since last reset.

		uint32 nCurrentReadBandwidth;   //!< Bytes/second for last second.
		uint32 nSessionReadBandwidth;   //!< Bytes/second for last second.

		uint32 nActualReadBandwidth;        //!< Bytes/second for last second - only taking actual reading into account.
		uint32 nAverageActualReadBandwidth; //!< Average read bandwidth in total from reset - only taking actual read time into account.
	};

	SMediaTypeInfo hddInfo;
	SMediaTypeInfo memoryInfo;
	SMediaTypeInfo discInfo;

	uint32         nTotalSessionReadBandwidth; //!< Average read bandwidth in total from reset - taking full time into account from reset.
	uint32         nTotalCurrentReadBandwidth; //!< Total bytes/sec over all types and systems.

	int            nPendingReadBytes;      //!< How many bytes still need to be read.
	float          fAverageCompletionTime; //!< Time in seconds on average takes to complete file request.
	float          fAverageRequestCount;   //!< Average requests per second being done to streaming engine.

	uint64         nMainStreamingThreadWait;

	uint64         nTotalBytesRead;             //!< Read bytes total from reset.
	uint32         nTotalRequestCount;          //!< Number of request from reset to the streaming engine.
	uint32         nTotalStreamingRequestCount; //!< Number of request from reset which actually resulted in streaming data.

	int            nCurrentDecryptCount;    //!< Number of requests currently waiting to be decrypted.
	int            nCurrentDecompressCount; //!< Number of requests currently waiting to be decompresses.
	int            nCurrentAsyncCount;      //!< Number of requests currently waiting to be async callback.
	int            nCurrentFinishedCount;   //!< Number of requests currently waiting to be finished by mainthread.

	uint32         nDecompressBandwidth;        //!< Bytes/second for last second.
	uint32         nDecryptBandwidth;           //!< Bytes/second for last second.
	uint32         nVerifyBandwidth;            //!< Bytes/second for last second.
	uint32         nDecompressBandwidthAverage; //!< Bytes/second in total.
	uint32         nDecryptBandwidthAverage;    //!< Bytes/second in total.
	uint32         nVerifyBandwidthAverage;     //!< Bytes/second in total.

	bool           bTempMemOutOfBudget; //!< Was the temporary streaming memory out of budget during the last second.
	int            nMaxTempMemory;      //!< Maximum temporary memory used by the streaming system.
	int            nTempMemory;

	struct SRequestTypeInfo
	{
		SRequestTypeInfo() : nPendingReadBytes(0)
		{
			ResetStats();
		}
		void ResetStats()
		{
			nTmpReadBytes = 0;
			nTotalStreamingRequestCount = 0;
			nTotalReadBytes = 0;
			nTotalRequestDataSize = 0;
			nTotalRequestCount = 0;
			nCurrentReadBandwidth = 0;
			nSessionReadBandwidth = 0;
			fTotalCompletionTime = .0f;
			fAverageCompletionTime = .0f;
		}

		void Merge(const SRequestTypeInfo& _other)
		{
			nPendingReadBytes += _other.nPendingReadBytes;
			nTmpReadBytes += _other.nTmpReadBytes;
			nTotalStreamingRequestCount += _other.nTotalStreamingRequestCount;
			nTotalReadBytes += _other.nTotalReadBytes;
			nTotalRequestDataSize += _other.nTotalRequestDataSize;
			nTotalRequestCount += _other.nTotalRequestCount;
			fTotalCompletionTime += _other.fTotalCompletionTime;
		}

		int    nPendingReadBytes;       //!< How many bytes still need to be read from media.

		uint64 nTmpReadBytes;           //!< Read bytes since last update to compute current bandwidth.

		uint32 nTotalStreamingRequestCount; //!< Total actual streaming requests of this type.
		uint64 nTotalReadBytes;             //!< Total actual read bytes (compressed data).
		uint64 nTotalRequestDataSize;       //!< Total requested bytes from client (uncompressed data).
		uint32 nTotalRequestCount;          //!< Total number of finished requests.

		uint32 nCurrentReadBandwidth;   //!< Bytes/second for this type during last second.
		uint32 nSessionReadBandwidth;   //!< Average read bandwidth in total from reset - taking full time into account from reset.

		float  fTotalCompletionTime;    //!< Time it took to finish all current requests.
		float  fAverageCompletionTime;  //!< Average time it takes to fully complete a request of this type.
		float  fAverageRequestCount;    //!< Average amount of requests made per second.
	};

	SRequestTypeInfo typeInfo[eStreamTaskTypeCount];

	struct SAsset
	{
		CryStringLocal m_sName;
		int            m_nSize;
		const bool operator<(const SAsset& a) const { return m_nSize > a.m_nSize; }
		SAsset() {}
		SAsset(const CryStringLocal& sName, const int nSize) : m_sName(sName), m_nSize(nSize) {}

		friend void swap(SAsset& a, SAsset& b)
		{
			using std::swap;

			a.m_sName.swap(b.m_sName);
			swap(a.m_nSize, b.m_nSize);
		}
	};
	DynArray<SAsset> vecHeavyAssets;
};
#endif

struct SStreamEngineOpenStats
{
	int nOpenRequestCount;
	int nOpenRequestCountByType[eStreamTaskTypeCount];
};

class IReadStream;
TYPEDEF_AUTOPTR(IReadStream);

//! Typedef IReadStream_AutoPtr auto ptr wrapper.
typedef IReadStream_AutoPtr IReadStreamPtr;

#endif
