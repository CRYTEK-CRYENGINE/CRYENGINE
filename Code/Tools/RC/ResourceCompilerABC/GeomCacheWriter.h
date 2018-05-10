// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   GeomCacheWriter.h
//  Created:     6/8/2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GeomCache.h"
#include "GeomCacheBlockCompressor.h"
#include "StealingThreadPool.h"

// This contains the position of an asynchronous 
// write after it was completed
class DiskWriteFuture
{
	friend class GeomCacheDiskWriteThread;	

public:
	DiskWriteFuture() : m_position(0), m_size(0) {};

	uint64 GetPosition() const 
	{
		return m_position;
	}
	uint32 GetSize() const 
	{ 
		return m_size;
	}

private:	
	uint64 m_position;
	uint32 m_size;
};

// This writes the results from the compression job to disk and will 
// not run in the thread pool, because it requires almost no CPU.
class GeomCacheDiskWriteThread
{
	static const unsigned int m_kNumBuffers = 8;

public:
	explicit GeomCacheDiskWriteThread(const string &fileName);	
	~GeomCacheDiskWriteThread();

	// Flushes the buffers to disk
	void Flush();

	// Flushes the buffers to disk and exits the thread
	void EndThread();

	// Write with FIFO buffering. This will acquire ownership of the buffer.
	void Write(std::vector<char> &buffer, DiskWriteFuture *pFuture = 0, long offset = 0, int origin = SEEK_CUR);

	size_t GetBytesWritten() { return m_bytesWritten; }
	
private:
	static unsigned int __stdcall ThreadFunc(void *pParam);
	void Run();

	bool IsReadyToExit();

	HANDLE m_threadHandle;
	string m_fileName;

	bool m_bExit;
	
	ThreadUtils::ConditionVariable m_dataAvailableCV;	
	ThreadUtils::ConditionVariable m_dataWrittenCV;	
	
	unsigned int m_currentReadBuffer;
	unsigned int m_currentWriteBuffer;	
	
	ThreadUtils::CriticalSection m_outstandingWritesCS;
	LONG m_outstandingWrites;

	ThreadUtils::CriticalSection m_criticalSections[m_kNumBuffers];
	std::vector<char> m_buffers[m_kNumBuffers];
	DiskWriteFuture *m_futures[m_kNumBuffers];
	long m_offsets[m_kNumBuffers];
	int m_origins[m_kNumBuffers];

	// Stats
	size_t m_bytesWritten;
};

// This class will receive the data from the GeomCacheWriter.
class GeomCacheBlockCompressionWriter
{
	struct JobData
	{		
		JobData()
			: m_bFinished(false)
			, m_bWritten(false)
			, m_offset(-1)
			, m_origin(-1)
			, m_pPrevJobData(nullptr)
			, m_pCompressionWriter(nullptr)
			, m_pWriteFuture(nullptr)
		{
		}
			
		volatile bool m_bFinished;
		volatile bool m_bWritten;
		uint m_blockIndex;
		uint m_dataIndex;
		long m_offset;
		long m_origin;
		JobData *m_pPrevJobData;
		GeomCacheBlockCompressionWriter *m_pCompressionWriter;
		DiskWriteFuture *m_pWriteFuture;
		std::vector<char> m_data;
	};

public:
	GeomCacheBlockCompressionWriter(ThreadUtils::StealingThreadPool &threadPool,
		IGeomCacheBlockCompressor *pBlockCompressor, GeomCacheDiskWriteThread &diskWriteThread);
	~GeomCacheBlockCompressionWriter();

	// This adds to the current buffer.
	void PushData(const void *data, size_t size);

	// Create job to compress data and write it to disk. Returns size of block.
	uint64 WriteBlock(bool bCompress, DiskWriteFuture *pFuture, long offset = 0, int origin = SEEK_CUR);	

	// Flush to write thread
	void Flush();

private:
	static unsigned int __stdcall ThreadFunc(void *pParam);
	void Run();

	void PushCompletedBlocks();
	static void RunJob(JobData *pJobData);
	void CompressJob(JobData *pJobData);

	// Write thread handle	
	HANDLE m_threadHandle;

	// Flag to exit write thread
	bool m_bExit;

	ThreadUtils::StealingThreadPool &m_threadPool;
	IGeomCacheBlockCompressor *m_pBlockCompressor;
	GeomCacheDiskWriteThread &m_diskWriteThread;
	std::vector<char> m_data;
	
	ThreadUtils::CriticalSection m_lockWriter;

	// Next block index hat will be used
	uint m_nextBlockIndex; 

	// Next job that will be used in the job array
	uint m_nextJobIndex;

	// Index of frame that needs to be written next
	uint m_nextBlockToWrite;

	volatile uint m_numJobsRunning;	
	ThreadUtils::CriticalSection m_jobFinishedCS;
	ThreadUtils::ConditionVariable m_jobFinishedCV;

	// The job data
	std::vector<JobData> m_jobData;
};

struct GeomCacheWriterStats
{
	uint64 m_headerDataSize;
	uint64 m_staticDataSize;
	uint64 m_animationDataSize;
	uint64 m_uncompressedAnimationSize;
};

class GeomCacheWriter
{
public:
	GeomCacheWriter(const string &filename, GeomCacheFile::EBlockCompressionFormat compressionFormat,
		ThreadUtils::StealingThreadPool &threadPool, const uint numFrames, const bool bPlaybackFromMemory,
		const bool b32BitIndices);

	void WriteStaticData(const std::vector<Alembic::Abc::chrono_t> &frameTimes, const std::vector<GeomCache::Mesh*> &meshes, const GeomCache::Node &rootNode);

	void WriteFrame(const uint frameIndex, const AABB &frameAABB, const GeomCacheFile::EFrameType frameType, 
		const std::vector<GeomCache::Mesh*> &meshes, GeomCache::Node &rootNode);

	// Flush all buffers, write frame offsets and return the size of the animation stream
	GeomCacheWriterStats FinishWriting();

private:
	bool WriteFrameInfos();
	void WriteMeshesStaticData(const std::vector<GeomCache::Mesh*> &meshes);
	void WriteNodeStaticDataRec(const GeomCache::Node &node, const std::vector<GeomCache::Mesh*> &meshes);

	void GetFrameData(GeomCacheFile::SFrameHeader &header, const std::vector<GeomCache::Mesh*> &meshes);
	void WriteNodeFrameRec(GeomCache::Node &node, const std::vector<GeomCache::Mesh*> &meshes, uint32 &bytesWritten);

	void WriteMeshFrameData(GeomCache::Mesh &meshData);	
	void WriteMeshStaticData(const GeomCache::Mesh &meshData, GeomCacheFile::EStreams streamMask);	

	GeomCacheFile::EBlockCompressionFormat m_compressionFormat;

	ThreadUtils::StealingThreadPool &m_threadPool;
	GeomCacheFile::SHeader m_fileHeader;
	AABB m_animationAABB;
	static const uint m_kNumProgressStatusReports = 10;
	bool m_bShowedStatus[m_kNumProgressStatusReports];
	FILE *m_pFileHandle;
	uint64 m_totalUncompressedAnimationSize;
	
	std::unique_ptr<IGeomCacheBlockCompressor> m_pBlockCompressor;
	std::unique_ptr<GeomCacheDiskWriteThread> m_pDiskWriteThread;
	std::unique_ptr<GeomCacheBlockCompressionWriter> m_pCompressionWriter;

	DiskWriteFuture m_headerWriteFuture;
	DiskWriteFuture m_frameInfosWriteFuture;
	DiskWriteFuture m_staticNodeDataFuture;
	DiskWriteFuture m_staticMeshDataFuture;
	std::vector<std::unique_ptr<DiskWriteFuture> > m_frameWriteFutures;	

	std::vector<Alembic::Abc::chrono_t> m_frameTimes;
	std::vector<uint> m_frameTypes;
};