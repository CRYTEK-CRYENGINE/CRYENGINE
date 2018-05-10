// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   GeomCacheWriter.cpp
//  Created:     6/8/2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

//  TODO: This assumes little endian (x86) for now

#include "stdafx.h"
#include "GeomCacheWriter.h"
#include "GeomCacheBlockCompressor.h"
#include <Cry3DEngine/GeomCacheFileFormat.h>

GeomCacheDiskWriteThread::GeomCacheDiskWriteThread(const string &fileName) 
	: m_fileName(fileName), m_bExit(false), m_currentReadBuffer(0), 
	m_currentWriteBuffer(0), m_outstandingWrites(0)
{
	for (unsigned int i = 0; i < m_kNumBuffers; ++i)
	{
		m_futures[i] = 0;
		m_offsets[i] = 0;
		m_origins[i] = SEEK_CUR;
	}

	// Start disk writer thread
	m_threadHandle = (HANDLE)_beginthreadex(0, 0, &GeomCacheDiskWriteThread::ThreadFunc, (void*)this, 0, 0);
	ThreadUtils::SetThreadName(GetThreadId(m_threadHandle), "GeomCacheDiskWriteThread");
}

GeomCacheDiskWriteThread::~GeomCacheDiskWriteThread()
{
	Flush();
}

unsigned int __stdcall GeomCacheDiskWriteThread::ThreadFunc(void *pParam)
{
	GeomCacheDiskWriteThread *pSelf = reinterpret_cast<GeomCacheDiskWriteThread*>(pParam);
	pSelf->Run();
	return 0;
}

void GeomCacheDiskWriteThread::Run()
{	
	FILE *pFileHandle = fopen(m_fileName, "w+b");

	while(true)
	{
		ThreadUtils::CriticalSection &criticalSection = m_criticalSections[m_currentReadBuffer];		
		criticalSection.Lock();

		// Wait until the current read buffer becomes filled or we need to exit
		while(m_buffers[m_currentReadBuffer].empty() && !IsReadyToExit())
		{
			m_dataAvailableCV.Sleep(criticalSection);
		}

		m_outstandingWritesCS.Lock();

		if (!IsReadyToExit())
		{
			// Seek to write position
			fseek(pFileHandle, m_offsets[m_currentReadBuffer], m_origins[m_currentReadBuffer]);

			// Fill future
			DiskWriteFuture *pFuture = m_futures[m_currentReadBuffer];
			if (pFuture)
			{
				fpos_t position;
				fgetpos(pFileHandle, &position);
				pFuture->m_position = (uint64)position;				
			}

			// Write and clear current read buffer			
			const size_t bufferSize = m_buffers[m_currentReadBuffer].size();
			m_bytesWritten += bufferSize;
			size_t bytesWritten = fwrite(m_buffers[m_currentReadBuffer].data(), 1, bufferSize, pFileHandle);
			//RCLog("Written %Iu/%Iu bytes from buffer %d, %d", bytesWritten, bufferSize, m_currentReadBuffer, ferror(pFileHandle));
			
			if (pFuture)
			{
				pFuture->m_size = (uint32)bytesWritten;
			}

			m_buffers[m_currentReadBuffer].clear();

			// Proceed to next buffer
			m_currentReadBuffer = (m_currentReadBuffer + 1) % m_kNumBuffers;

			InterlockedDecrement(&m_outstandingWrites);
		}
		else
		{		
			m_outstandingWritesCS.Unlock();
			criticalSection.Unlock();
			break;
		}		
		
		m_outstandingWritesCS.Unlock();
		criticalSection.Unlock();
		m_dataWrittenCV.WakeAll();
	}

	fclose(pFileHandle);	
}

void GeomCacheDiskWriteThread::Write(std::vector<char> &buffer, DiskWriteFuture *pFuture, long offset, int origin)
{
	assert(!buffer.empty() && !m_bExit);
	if (buffer.empty() || m_bExit)
	{
		return;
	}

	InterlockedIncrement(&m_outstandingWrites);

	ThreadUtils::CriticalSection &criticalSection = m_criticalSections[m_currentWriteBuffer];
	criticalSection.Lock();

	while(!m_buffers[m_currentWriteBuffer].empty())
	{
		m_dataWrittenCV.Sleep(criticalSection);
	}	

	assert(!buffer.empty());
	buffer.swap(m_buffers[m_currentWriteBuffer]);	
	m_futures[m_currentWriteBuffer] = pFuture;
	m_offsets[m_currentWriteBuffer] = offset;
	m_origins[m_currentWriteBuffer] = origin;	

	//RCLog("Updated buffer %u", m_currentWriteBuffer);

	m_currentWriteBuffer = (m_currentWriteBuffer + 1) % m_kNumBuffers;

	criticalSection.Unlock();
	m_dataAvailableCV.Wake();
}

void GeomCacheDiskWriteThread::Flush()
{
	ThreadUtils::AutoLock lock(m_outstandingWritesCS);
	while(InterlockedCompareExchange(&m_outstandingWrites, 0, 0) != 0)
	{
		m_dataWrittenCV.Sleep(m_outstandingWritesCS);
	}	
}

void GeomCacheDiskWriteThread::EndThread()
{
	for (unsigned int i = 0; i < m_kNumBuffers; ++i)
	{
		m_criticalSections[m_currentReadBuffer].Lock();
	}

	m_bExit = true;

	for (unsigned int i = 0; i < m_kNumBuffers; ++i)
	{
		m_criticalSections[m_currentReadBuffer].Unlock();
	}

	m_dataAvailableCV.Wake();

	WaitForSingleObject(m_threadHandle, INFINITE);
	CloseHandle(m_threadHandle);
	RCLog("  Disk write thread exited");
}

bool GeomCacheDiskWriteThread::IsReadyToExit()
{
	return m_bExit && (InterlockedCompareExchange(&m_outstandingWrites, 0, 0) == 0);
}

GeomCacheBlockCompressionWriter::GeomCacheBlockCompressionWriter(ThreadUtils::StealingThreadPool &threadPool, 
	IGeomCacheBlockCompressor *pBlockCompressor, GeomCacheDiskWriteThread &diskWriteThread)
	: m_threadPool(threadPool), m_pBlockCompressor(pBlockCompressor), m_diskWriteThread(diskWriteThread),
	m_nextBlockIndex(0), m_nextJobIndex(0), m_bExit(false), m_nextBlockToWrite(0), m_numJobsRunning(0)
{
	m_jobData.resize(m_threadPool.GetNumThreads() * 2);

	const uint numJobs = m_jobData.size();
	for (uint i = 0; i < numJobs; ++i)
	{
		m_jobData[i].m_dataIndex = i;
		m_jobData[i].m_pPrevJobData = &m_jobData[(i > 0) ? (i - 1) : (numJobs - 1)];
		m_jobData[i].m_pCompressionWriter = this;
	}

	// Start compression writer control thread
	m_threadHandle = (HANDLE)_beginthreadex(0, 0, &GeomCacheBlockCompressionWriter::ThreadFunc, (void*)this, 0, 0);
	ThreadUtils::SetThreadName(GetThreadId(m_threadHandle), "GeomCacheBlockCompressionWriter");
}

GeomCacheBlockCompressionWriter::~GeomCacheBlockCompressionWriter()
{
	Flush();

	m_bExit = true;
	m_jobFinishedCV.WakeAll();	

	WaitForSingleObject(m_threadHandle, INFINITE);
}

unsigned int __stdcall GeomCacheBlockCompressionWriter::ThreadFunc(void *pParam)
{
	GeomCacheBlockCompressionWriter *pSelf = reinterpret_cast<GeomCacheBlockCompressionWriter*>(pParam);
	pSelf->Run();
	return 0;
}

void GeomCacheBlockCompressionWriter::Run()
{
	ThreadUtils::AutoLock lock(m_jobFinishedCS);
	while(!m_bExit || m_numJobsRunning > 0)
	{
		PushCompletedBlocks();

		if (!m_bExit || m_numJobsRunning > 0)
		{
			m_jobFinishedCV.Sleep(m_jobFinishedCS);		
		}
	}	
}

void GeomCacheBlockCompressionWriter::Flush()
{
	ThreadUtils::AutoLock writeBlockLock(m_lockWriter);
		
	// Just wait until write thread has no more jobs running
	ThreadUtils::AutoLock lock(m_jobFinishedCS);
	while(m_numJobsRunning > 0)
	{
		PushCompletedBlocks();
		
		if (m_numJobsRunning > 0)
		{
			m_jobFinishedCV.Sleep(m_jobFinishedCS);		
		}
	}
}

void GeomCacheBlockCompressionWriter::PushCompletedBlocks()
{
	std::vector<JobData*> unwrittenFinishedJobs;

	const uint numJobs = m_jobData.size();
	for (uint i = 0; i < numJobs; ++i)
	{
		JobData &jobData = m_jobData[i];
		if (jobData.m_bFinished && !jobData.m_bWritten)
		{
			unwrittenFinishedJobs.push_back(&jobData);
		}
	}
	
	std::sort(unwrittenFinishedJobs.begin(), unwrittenFinishedJobs.end(), 
		[=](const JobData *pA, const JobData *pB) {
			return pA->m_blockIndex < pB->m_blockIndex;
		}
	);

	for (uint i = 0; i < unwrittenFinishedJobs.size(); ++i)
	{
		JobData &jobData = *unwrittenFinishedJobs[i];
		if (jobData.m_blockIndex == m_nextBlockToWrite)
		{
			m_diskWriteThread.Write(jobData.m_data, jobData.m_pWriteFuture, jobData.m_offset, jobData.m_origin);

			InterlockedDecrement(&m_numJobsRunning);
			jobData.m_bWritten = true;			
			++m_nextBlockToWrite;		
			m_jobFinishedCV.WakeAll();
		}
		else
		{
			break;
		}
	}
}

void GeomCacheBlockCompressionWriter::PushData(const void *data, size_t size)
{
	size_t writePosition = m_data.size();
	m_data.resize(m_data.size() + size);
	memcpy((void*)&m_data[writePosition], data, size);
}

uint64 GeomCacheBlockCompressionWriter::WriteBlock(bool bCompress, DiskWriteFuture *pFuture, long offset, int origin)
{
	ThreadUtils::AutoLock writeBlockLock(m_lockWriter);

	uint64 blockSize = m_data.size();

	if (m_data.empty())
	{		
		return blockSize;
	}

	ThreadUtils::CriticalSection cs;
	const uint numJobs = m_jobData.size();
	while(m_numJobsRunning == numJobs)
	{
		m_jobFinishedCV.Sleep(cs);
	}

	// Fill job data
	JobData &jobData = m_jobData[m_nextJobIndex];
	jobData.m_bFinished = false;
	jobData.m_bWritten = false;
	jobData.m_blockIndex = m_nextBlockIndex++;	
	jobData.m_pWriteFuture = pFuture;
	jobData.m_offset = offset;
	jobData.m_origin = origin;
	jobData.m_data.swap(m_data);

	m_nextJobIndex = (m_nextJobIndex + 1) % numJobs;
	InterlockedIncrement(&m_numJobsRunning);	

	if (bCompress)
	{		
		m_threadPool.Submit(&GeomCacheBlockCompressionWriter::RunJob, &jobData);		
	}
	else
	{
		// Just directly pass to write thread
		jobData.m_bFinished = true;
		m_jobFinishedCV.WakeAll();
	}

	return blockSize;
}

void GeomCacheBlockCompressionWriter::RunJob(JobData *pJobData)
{
	pJobData->m_pCompressionWriter->CompressJob(pJobData);
}

void GeomCacheBlockCompressionWriter::CompressJob(JobData *pJobData)
{	
	// Remember uncompressed size
	const uint32 uncompressedSize = pJobData->m_data.size();

	// Compress job data
	std::vector<char> compressedData;
	m_pBlockCompressor->Compress(pJobData->m_data, compressedData);		
	pJobData->m_data.clear();
				
	std::vector<char> dataBuffer;
	dataBuffer.reserve(sizeof(GeomCacheFile::SCompressedBlockHeader) + compressedData.size());
	dataBuffer.resize(sizeof(GeomCacheFile::SCompressedBlockHeader));

	GeomCacheFile::SCompressedBlockHeader *pBlockHeader = reinterpret_cast<GeomCacheFile::SCompressedBlockHeader*>(dataBuffer.data());
	pBlockHeader->m_uncompressedSize = uncompressedSize;
	pBlockHeader->m_compressedSize = compressedData.size();		

	dataBuffer.insert(dataBuffer.begin() + sizeof(GeomCacheFile::SCompressedBlockHeader), compressedData.begin(), compressedData.end());

	pJobData->m_data = dataBuffer;
	m_jobFinishedCS.Lock();
	pJobData->m_bFinished = true;
	m_jobFinishedCS.Unlock();
	m_jobFinishedCV.WakeAll();
}

GeomCacheWriter::GeomCacheWriter(const string &filename, GeomCacheFile::EBlockCompressionFormat compressionFormat, 
	ThreadUtils::StealingThreadPool &threadPool, const uint numFrames, const bool bPlaybackFromMemory, const bool b32BitIndices) 
	: m_compressionFormat(compressionFormat), m_threadPool(threadPool), m_pFileHandle(0), m_totalUncompressedAnimationSize(0)
{
	m_animationAABB.Reset();

	m_fileHeader.m_flags |= bPlaybackFromMemory ? GeomCacheFile::eFileHeaderFlags_PlaybackFromMemory : 0;	
	m_fileHeader.m_flags |= b32BitIndices ? GeomCacheFile::eFileHeaderFlags_32BitIndices : 0;

	m_pDiskWriteThread.reset(new GeomCacheDiskWriteThread(filename));

	switch (compressionFormat)
	{
	case GeomCacheFile::eBlockCompressionFormat_None:
		m_pBlockCompressor.reset(new GeomCacheStoreBlockCompressor);
		break;
	case GeomCacheFile::eBlockCompressionFormat_Deflate:
		m_pBlockCompressor.reset(new GeomCacheDeflateBlockCompressor);
		break;
	case GeomCacheFile::eBlockCompressionFormat_LZ4HC:
		m_pBlockCompressor.reset(new GeomCacheLZ4HCBlockCompressor);
		break;
	}
	
	m_pCompressionWriter.reset(new GeomCacheBlockCompressionWriter(m_threadPool, m_pBlockCompressor.get(), *m_pDiskWriteThread));

	for (uint i = 0; i < m_kNumProgressStatusReports; ++i)
	{
		m_bShowedStatus[i] = false;
	}
}

GeomCacheWriterStats GeomCacheWriter::FinishWriting()
{
	GeomCacheWriterStats stats = {0};	

	// Flush disk write thread and thereby make all write future valid
	m_pCompressionWriter->Flush();
	m_pDiskWriteThread->Flush();
	
	// Write frame offsets
	RCLog("  Writing frame offsets/sizes...");
	if (!WriteFrameInfos())
	{
		return stats;
	}

	// Last write header with proper signature, AABB & static mesh data offset
	m_fileHeader.m_signature = GeomCacheFile::kFileSignature;	
	m_fileHeader.m_totalUncompressedAnimationSize = m_totalUncompressedAnimationSize;
	for (unsigned int i = 0; i < 3; ++i)
	{
		m_fileHeader.m_aabbMin[i] = m_animationAABB.min[i];
		m_fileHeader.m_aabbMax[i] = m_animationAABB.max[i];
	}
	m_pCompressionWriter->PushData((void*)&m_fileHeader, sizeof(GeomCacheFile::SHeader));	
	m_pCompressionWriter->WriteBlock(false, nullptr, 0, SEEK_SET);

	// Destroy compression writer and block compressor
	m_pCompressionWriter.reset(nullptr);
	m_pBlockCompressor.reset(nullptr);

	// End disk write thread
	m_pDiskWriteThread->EndThread();	

	uint64 animationBytesWritten = 0;
	for (auto iter = m_frameWriteFutures.begin(); iter != m_frameWriteFutures.end(); ++iter)
	{
		uint64 blockSize = (*iter)->GetSize();
		animationBytesWritten += (*iter)->GetSize();
	}

	stats.m_headerDataSize = m_headerWriteFuture.GetSize() + m_frameInfosWriteFuture.GetSize() + m_staticNodeDataFuture.GetSize();
	stats.m_staticDataSize = m_staticMeshDataFuture.GetSize();
	stats.m_animationDataSize = animationBytesWritten;
	stats.m_uncompressedAnimationSize = m_totalUncompressedAnimationSize;

	return stats;
}

void GeomCacheWriter::WriteStaticData(const std::vector<Alembic::Abc::chrono_t> &frameTimes, const std::vector<GeomCache::Mesh*> &meshes, const GeomCache::Node &rootNode)
{	
	RCLog("Writing static data to disk...");
	
	// Write header with 0 signature, to avoid the engine to read incomplete caches. 
	// Correct signature will be written in FinishWriting at the end
	m_fileHeader.m_blockCompressionFormat = m_compressionFormat;
	m_fileHeader.m_numFrames = frameTimes.size();
	m_pCompressionWriter->PushData((void*)&m_fileHeader, sizeof(GeomCacheFile::SHeader));	
	m_pCompressionWriter->WriteBlock(false, &m_headerWriteFuture);

	// Leave space for frame offsets/sizes (don't know them until frames were written)
	// and pass future object to get write position for WriteFrameOffsets later
	std::vector<GeomCacheFile::SFrameInfo> frameInfos(frameTimes.size());	
	memset(frameInfos.data(), 0, sizeof(GeomCacheFile::SFrameInfo) * frameInfos.size());
	m_pCompressionWriter->PushData(frameInfos.data(), sizeof(GeomCacheFile::SFrameInfo) * frameInfos.size());
	m_pCompressionWriter->WriteBlock(false, &m_frameInfosWriteFuture);

	// Reserve frame type array and store frame times
	m_frameTypes.resize(frameTimes.size(), 0);
	m_frameTimes = frameTimes;

	// Write compressed physics geometries and node data
	RCLog("  Writing node data");
	WriteNodeStaticDataRec(rootNode, meshes);
	m_pCompressionWriter->WriteBlock(true, &m_staticNodeDataFuture);

	// Write compressed static mesh data
	RCLog("  Writing mesh data (%u meshes)", (uint)meshes.size());
	WriteMeshesStaticData(meshes);
	m_pCompressionWriter->WriteBlock(true, &m_staticMeshDataFuture);
}

bool GeomCacheWriter::WriteFrameInfos()
{
	const size_t numFrames = m_frameWriteFutures.size();

	std::vector<GeomCacheFile::SFrameInfo> frameInfos(numFrames);	

	for (size_t i = 0; i < numFrames; ++i)
	{
		frameInfos[i].m_frameType = m_frameTypes[i];		
		frameInfos[i].m_frameOffset = m_frameWriteFutures[i]->GetPosition();
		frameInfos[i].m_frameSize = m_frameWriteFutures[i]->GetSize();		
		frameInfos[i].m_frameTime = (float)m_frameTimes[i];
		
		if(frameInfos[i].m_frameOffset == 0 || frameInfos[i].m_frameSize == 0)
		{
			RCLogError("Invalid frame offset or size");
			return false;
		}
	}
	m_pCompressionWriter->PushData(frameInfos.data(), sizeof(GeomCacheFile::SFrameInfo) * frameInfos.size());	

	// Overwrite space left free for frame infos
	m_pCompressionWriter->WriteBlock(false, 0, (long)m_frameInfosWriteFuture.GetPosition(), SEEK_SET);

	return true;
}

void GeomCacheWriter::WriteMeshesStaticData(const std::vector<GeomCache::Mesh*> &meshes)
{
	uint32 numMeshes = meshes.size();
	m_pCompressionWriter->PushData((void*)&numMeshes, sizeof(uint32));
	
	for (auto iter = meshes.begin(); iter != meshes.end(); ++iter)
	{
		const GeomCache::Mesh *pMesh = *iter;

		const std::string &fullName = pMesh->m_abcMesh.getFullName();
		const uint nameLength = fullName.size() + 1;
		
		GeomCacheFile::SMeshInfo meshInfo;
		meshInfo.m_constantStreams = static_cast<uint8>(pMesh->m_constantStreams);
		meshInfo.m_animatedStreams = static_cast<uint8>(pMesh->m_animatedStreams);		
		meshInfo.m_positionPrecision[0] = pMesh->m_positionPrecision[0];
		meshInfo.m_positionPrecision[1] = pMesh->m_positionPrecision[1];
		meshInfo.m_positionPrecision[2] = pMesh->m_positionPrecision[2];
		meshInfo.m_numVertices = static_cast<uint32>(pMesh->m_staticMeshData.m_positions.size());
		meshInfo.m_numMaterials = static_cast<uint32>(pMesh->m_indicesMap.size());
		meshInfo.m_flags = pMesh->m_bUsePredictor ? GeomCacheFile::eMeshIFrameFlags_UsePredictor : 0;
		meshInfo.m_nameLength = nameLength;
		meshInfo.m_hash = pMesh->m_hash;
		
		for (unsigned int i = 0; i < 3; ++i)
		{
			meshInfo.m_aabbMin[i] = pMesh->m_aabb.min[i];
			meshInfo.m_aabbMax[i] = pMesh->m_aabb.max[i];
		}

		m_pCompressionWriter->PushData((void*)&meshInfo, sizeof(GeomCacheFile::SMeshInfo));
		m_pCompressionWriter->PushData(fullName.c_str(), nameLength);

		// Write out material IDs
		for (auto iter = pMesh->m_indicesMap.begin(); iter != pMesh->m_indicesMap.end(); ++iter)
		{
			const uint16 materialId = iter->first;
			m_pCompressionWriter->PushData((void*)&materialId, sizeof(uint16));
		}
	}

	for (auto iter = meshes.begin(); iter != meshes.end(); ++iter)
	{
		const GeomCache::Mesh &mesh = **iter;
		const GeomCacheFile::EStreams mandatoryStreams = GeomCacheFile::EStreams(GeomCacheFile::eStream_Indices | GeomCacheFile::eStream_Positions
			| GeomCacheFile::eStream_Texcoords | GeomCacheFile::eStream_QTangents);
		assert (((mesh.m_constantStreams | mesh.m_animatedStreams) & mandatoryStreams) == mandatoryStreams);
		WriteMeshStaticData(mesh, mesh.m_constantStreams);
	}
}

void GeomCacheWriter::WriteNodeStaticDataRec(const GeomCache::Node &node, const std::vector<GeomCache::Mesh*> &meshes)
{
	GeomCacheFile::SNodeInfo fileNode;
	fileNode.m_type = static_cast<uint8>(node.m_type);
	fileNode.m_transformType = static_cast<uint16>(node.m_transformType);
	fileNode.m_bVisible = node.m_staticNodeData.m_bVisible ? 1 : 0;
	fileNode.m_meshIndex = std::numeric_limits<uint32>::max();

	if (node.m_type == GeomCacheFile::eNodeType_Mesh)
	{
		auto findIter = std::find(meshes.begin(), meshes.end(), node.m_pMesh.get());
		if (findIter != meshes.end())
		{
			fileNode.m_meshIndex = static_cast<uint32>(findIter - meshes.begin());
		}
	}

	fileNode.m_numChildren = static_cast<uint32>(node.m_children.size());

	const std::string &fullName = (node.m_abcObject != NULL) ? node.m_abcObject.getFullName() : "root";
	const uint nameLength = fullName.size() + 1;
	fileNode.m_nameLength = nameLength;

	// Write out node infos
	m_pCompressionWriter->PushData((void*)&fileNode, sizeof(GeomCacheFile::SNodeInfo));
	m_pCompressionWriter->PushData(fullName.c_str(), nameLength);

	// Store full initial pose. We could optimize this by leaving out animated branches without any physics proxies.
	static_assert(sizeof(QuatTNS) == 10 * sizeof(float), "QuatTNS size should be 40 bytes");
	m_pCompressionWriter->PushData((void*)&node.m_staticNodeData.m_transform, sizeof(QuatTNS));

	if (node.m_type == GeomCacheFile::eNodeType_PhysicsGeometry)
	{
		uint32 geometrySize = node.m_physicsGeometry.size();
		m_pCompressionWriter->PushData((void*)&geometrySize, sizeof(uint32));
		m_pCompressionWriter->PushData((void*)node.m_physicsGeometry.data(), node.m_physicsGeometry.size());
	}

	for (auto iter = node.m_children.begin(); iter != node.m_children.end(); ++iter)
	{
		WriteNodeStaticDataRec(**iter, meshes);
	}
}

void GeomCacheWriter::WriteFrame(const uint frameIndex, const AABB &frameAABB, const GeomCacheFile::EFrameType frameType, 
	const std::vector<GeomCache::Mesh*> &meshes, GeomCache::Node &rootNode)
{
	m_animationAABB.Add(frameAABB);

	GeomCacheFile::SFrameHeader frameHeader;
	static_assert((sizeof(GeomCacheFile::SFrameHeader) % 16) == 0, "GeomCacheFile::SFrameHeader size must be a multiple of 16");
	
	m_frameTypes[frameIndex] = frameType;
	
	for (unsigned int i = 0; i < 3; ++i)
	{
		frameHeader.m_frameAABBMin[i] = frameAABB.min[i];
		frameHeader.m_frameAABBMax[i] = frameAABB.max[i];
	}	
	
	GetFrameData(frameHeader, meshes);
	m_pCompressionWriter->PushData((void*)&frameHeader, sizeof(GeomCacheFile::SFrameHeader));

	for (auto iter = meshes.begin(); iter != meshes.end(); ++iter)
	{
		GeomCache::Mesh *mesh = *iter;
		WriteMeshFrameData(*mesh);
	}

	uint32 bytesWritten = 0;
	WriteNodeFrameRec(rootNode, meshes, bytesWritten);

	// Pad node data to 16 bytes
	std::vector<uint8> paddingData(((bytesWritten + 15) & ~15) - bytesWritten, 0);
	m_pCompressionWriter->PushData(paddingData.data(), paddingData.size());

	m_frameWriteFutures.push_back(std::unique_ptr<DiskWriteFuture>(new DiskWriteFuture()));
	m_totalUncompressedAnimationSize += m_pCompressionWriter->WriteBlock(true, m_frameWriteFutures.back().get());

	const uint fraction = uint((float)m_kNumProgressStatusReports * ((float)(frameIndex + 1) / (float)m_fileHeader.m_numFrames));
	if (fraction > 0 && !m_bShowedStatus[fraction - 1])
	{
		const uint percent = (100 * fraction) / (m_kNumProgressStatusReports);
		RCLog("  %u%% processed", percent);
		m_bShowedStatus[fraction - 1] = true;
	}
}

void GeomCacheWriter::GetFrameData(GeomCacheFile::SFrameHeader &header, const std::vector<GeomCache::Mesh*> &meshes)
{
	uint32 dataOffset = 0;

	for (auto iter = meshes.begin(); iter != meshes.end(); ++iter)
	{
		const GeomCache::Mesh &mesh = **iter;		
		
		if (mesh.m_animatedStreams != 0)
		{
			ThreadUtils::AutoLock jobLock(mesh.m_encodedFramesCS);
			dataOffset += mesh.m_encodedFrames.front().size();
		}
	}

	header.m_nodeDataOffset = dataOffset;
}

void GeomCacheWriter::WriteNodeFrameRec(GeomCache::Node &node, const std::vector<GeomCache::Mesh*> &meshes, uint32 &bytesWritten)
{
	ThreadUtils::AutoLock jobLock(node.m_encodedFramesCS);
	m_pCompressionWriter->PushData(node.m_encodedFrames.front().data(), 
		node.m_encodedFrames.front().size());
	bytesWritten += node.m_encodedFrames.front().size();
	node.m_encodedFrames.pop_front();

	for (auto iter = node.m_children.begin(); iter != node.m_children.end(); ++iter)
	{
		WriteNodeFrameRec(**iter, meshes, bytesWritten);
	}
}

void GeomCacheWriter::WriteMeshFrameData(GeomCache::Mesh &mesh)
{
	if (mesh.m_animatedStreams != 0)
	{		
		ThreadUtils::AutoLock jobLock(mesh.m_encodedFramesCS);
		m_pCompressionWriter->PushData(mesh.m_encodedFrames.front().data(), mesh.m_encodedFrames.front().size());
		mesh.m_encodedFrames.pop_front();
	}
}

void GeomCacheWriter::WriteMeshStaticData(const GeomCache::Mesh &mesh, GeomCacheFile::EStreams streamMask)
{
	if (streamMask & GeomCacheFile::eStream_Indices)
	{
		for (auto iter = mesh.m_indicesMap.begin(); iter != mesh.m_indicesMap.end(); ++iter)
		{
			const std::vector<uint32> &indices = iter->second;
			const uint32 numIndices = indices.size();
			m_pCompressionWriter->PushData(&numIndices, sizeof(uint32));

			if ((m_fileHeader.m_flags & GeomCacheFile::eFileHeaderFlags_32BitIndices) != 0)
			{
				m_pCompressionWriter->PushData(indices.data(), sizeof(uint32) * numIndices);
			}
			else
			{
				std::vector<uint16> indices16Bit(indices.size());
				std::transform(indices.begin(), indices.end(), indices16Bit.begin(), [](uint32 index) { return static_cast<uint16>(index); });
				m_pCompressionWriter->PushData(indices16Bit.data(), sizeof(uint16) * numIndices);
			}			
		}
	}

	bool bWroteStream = false;
	unsigned int numElements = 0;

	const GeomCache::MeshData &meshData = mesh.m_staticMeshData;
	
	if (streamMask & GeomCacheFile::eStream_Positions)
	{
		m_pCompressionWriter->PushData(meshData.m_positions.data(), sizeof(GeomCacheFile::Position) * meshData.m_positions.size());
		bWroteStream = true;
		numElements = meshData.m_positions.size();
	}

	if (streamMask & GeomCacheFile::eStream_Texcoords)
	{			
		assert(!bWroteStream || meshData.m_texcoords.size() == numElements);
		m_pCompressionWriter->PushData(meshData.m_texcoords.data(), sizeof(GeomCacheFile::Texcoords) * meshData.m_texcoords.size());
		bWroteStream = true;
		numElements = meshData.m_texcoords.size();
	}

	if (streamMask & GeomCacheFile::eStream_QTangents)
	{
		assert(!bWroteStream || meshData.m_qTangents.size() == numElements);
		m_pCompressionWriter->PushData(meshData.m_qTangents.data(), sizeof(GeomCacheFile::QTangent) * meshData.m_qTangents.size());
		bWroteStream = true;
		numElements = meshData.m_qTangents.size();
	}

	if (streamMask & GeomCacheFile::eStream_Colors)
	{
		assert(!bWroteStream || meshData.m_reds.size() == numElements);
		assert(!bWroteStream || meshData.m_greens.size() == numElements);
		assert(!bWroteStream || meshData.m_blues.size() == numElements);
		assert(!bWroteStream || meshData.m_alphas.size() == numElements);
		m_pCompressionWriter->PushData(meshData.m_reds.data(), sizeof(GeomCacheFile::Color) * meshData.m_reds.size());
		m_pCompressionWriter->PushData(meshData.m_greens.data(), sizeof(GeomCacheFile::Color) * meshData.m_greens.size());
		m_pCompressionWriter->PushData(meshData.m_blues.data(), sizeof(GeomCacheFile::Color) * meshData.m_blues.size());
		m_pCompressionWriter->PushData(meshData.m_alphas.data(), sizeof(GeomCacheFile::Color) * meshData.m_alphas.size());
		bWroteStream = true;
		numElements = meshData.m_reds.size();
	}
	
	// Let's quiet compiler complains about the following variables assigned but not used
	(void)bWroteStream;
	(void)numElements;

	if (mesh.m_bUsePredictor)
	{
		const uint32 predictorDataSize = mesh.m_predictorData.size(); 
		m_pCompressionWriter->PushData(&predictorDataSize, sizeof(uint32));
		m_pCompressionWriter->PushData(mesh.m_predictorData.data(), sizeof(uint16) * mesh.m_predictorData.size());		
	}
}
