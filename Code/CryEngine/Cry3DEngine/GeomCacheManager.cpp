// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   GeomCacheManager.cpp
//  Created:     20/7/2012 by Axel Gneiting
//  Description: Manages geometry cache instances and streaming
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(USE_GEOM_CACHES)

	#include "GeomCacheManager.h"
	#include "GeomCache.h"
	#include "GeomCacheRenderNode.h"
	#include <CryMath/Cry_Color.h>

	#include <CryThreading/IJobManager_JobDelegator.h>

DECLARE_JOB("GeomCacheDecomp", TGeomCacheDecompressJob, CGeomCacheManager::DecompressFrame_JobEntry);
DECLARE_JOB("GeomCacheIFrameDecode", TGeomCacheIFrameDecodeJob, CGeomCacheManager::DecodeIFrame_JobEntry)
DECLARE_JOB("GeomCacheBFrameDecode", TGeomCacheBFrameDecodeJob, CGeomCacheManager::DecodeBFrame_JobEntry)
DECLARE_JOB("GeomCacheFill", TGeomCacheFillRenderNodeAsyncJob, CGeomCacheManager::FillRenderNodeAsync_JobEntry);

namespace
{
const uint kMinBufferSizeInMiB = 8;
const uint kMaxBufferSizeInMiB = 2048;
}

CGeomCacheManager::CGeomCacheManager()
	: m_pPoolBaseAddress(NULL)
	, m_pPool(NULL)
	, m_poolSize(0)
	, m_lastRequestStream(0)
	, m_numMissedFrames(0)
	, m_numStreamAborts(0)
	, m_numErrorAborts(0)
	, m_numDecompressStreamAborts(0)
	, m_numReadStreamAborts(0)
	, m_numFailedAllocs(0)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_GeomCache, 0, "Geometry cache streaming pool");

	ChangeBufferSize(GetCVars()->e_GeomCacheBufferSize);

	ICVar* pGeomCacheBufferSizeCVar = gEnv->pConsole->GetCVar("e_GeomCacheBufferSize");
	if (pGeomCacheBufferSizeCVar)
	{
		pGeomCacheBufferSizeCVar->SetOnChangeCallback(&CGeomCacheManager::OnChangeBufferSize);
	}
}

CGeomCacheManager::~CGeomCacheManager()
{
	Reset();
	UnloadGeomCaches();

	#if !defined(DEDICATED_SERVER)
	m_pPool->Release();
	m_pPool = NULL;

	CryGetIMemoryManager()->FreePages(m_pPoolBaseAddress, m_poolSize);
	#endif
}

void CGeomCacheManager::Reset()
{
	const uint numStreams = m_streamInfos.size();
	for (uint i = 0; i < numStreams; ++i)
	{
		SGeomCacheStreamInfo* pStreamInfo = m_streamInfos[i];
		AbortStreamAndWait(*pStreamInfo);
		delete pStreamInfo;
	}

	stl::free_container(m_streamInfos);

	GetMeshManager().Reset();
}

void CGeomCacheManager::StopCacheStreamsAndWait(CGeomCache* pGeomCache)
{
	const uint numStreams = m_streamInfos.size();
	for (uint i = 0; i < numStreams; ++i)
	{
		SGeomCacheStreamInfo* pStreamInfo = m_streamInfos[i];
		if (pStreamInfo->m_pGeomCache == pGeomCache)
		{
			AbortStreamAndWait(*pStreamInfo);
		}
	}
}

CGeomCache* CGeomCacheManager::FindGeomCacheByFilename(const char* filename)
{
	return stl::find_in_map(m_nameToGeomCacheMap, CONST_TEMP_STRING(filename), NULL);
}

CGeomCache* CGeomCacheManager::LoadGeomCache(const char* szFileName)
{
	LOADING_TIME_PROFILE_SECTION;
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Geometry Caches");

	// Normalize file name
	char sFilename[_MAX_PATH];

	// Remap %level% alias if needed an unify filename
	int nAliasNameLen = sizeof("%level%") - 1;
	if (strncmp(szFileName, "%level%", nAliasNameLen) == 0)
	{
		cry_strcpy(sFilename, Get3DEngine()->GetLevelFilePath(szFileName + nAliasNameLen));
	}
	else
	{
		cry_strcpy(sFilename, szFileName);
	}
	std::replace(sFilename, sFilename + strlen(sFilename), '\\', '/'); // To Unix Path

	// Try to find existing object for that file
	CGeomCache* pGeomCache = stl::find_in_map(m_nameToGeomCacheMap, CONST_TEMP_STRING(sFilename), NULL);
	if (pGeomCache)
	{
		return pGeomCache;
	}

	// Load geom cache
	pGeomCache = new CGeomCache(sFilename);
	m_nameToGeomCacheMap[sFilename] = pGeomCache;
	return pGeomCache;
}

void CGeomCacheManager::UnloadGeomCaches()
{
	for (TGeomCacheMap::iterator iter = m_nameToGeomCacheMap.begin(); iter != m_nameToGeomCacheMap.end(); ++iter)
	{
		delete iter->second;
	}

	stl::free_container(m_nameToGeomCacheMap);
}

void CGeomCacheManager::DeleteGeomCache(CGeomCache* pGeomCache)
{
	const char* pFilename = pGeomCache->GetFilePath();
	m_nameToGeomCacheMap.erase(pFilename);
	delete pGeomCache;
}

void CGeomCacheManager::RegisterForStreaming(CGeomCacheRenderNode* pRenderNode)
{
	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	if (!pRenderNode)
	{
		return;
	}

	for (uint i = 0; i < m_streamInfos.size(); ++i)
	{
		const SGeomCacheStreamInfo* pStreamInfo = m_streamInfos[i];
		if (pStreamInfo->m_pRenderNode == pRenderNode)
		{
			return;
		}
	}

	CGeomCache* pGeomCache = static_cast<CGeomCache*>(pRenderNode->GetGeomCache());
	pGeomCache->IncreaseNumStreams();

	const uint numFrames = pGeomCache->GetNumFrames();

	SGeomCacheStreamInfo* pStreamInfo = new SGeomCacheStreamInfo(pRenderNode, pGeomCache, numFrames);
	m_streamInfos.push_back(pStreamInfo);

	// If cache is too short we need to allocate double the amount of frame data. Otherwise we can't loop without aborts,
	// because IssueDiskReadRequest prevents the same frame info from being used twice.
	const uint preferredDiskRequestSize = (size_t)std::max(0, GetCVars()->e_GeomCachePreferredDiskRequestSize * 1024);
	const uint64 compressedAnimationDataSize = pGeomCache->GetCompressedAnimationDataSize();

	const float maxBufferAheadTime = std::max(1.0f, GetCVars()->e_GeomCacheMaxBufferAheadTime);
	const float duration = pGeomCache->GetDuration();

	const bool bNeedDoubleFrameData = (compressedAnimationDataSize < (preferredDiskRequestSize * 2)) || (duration < (maxBufferAheadTime * 2));

	const uint numFrameData = bNeedDoubleFrameData ? (numFrames * 2) : numFrames;
	pStreamInfo->m_frameData.resize(numFrameData);
	ReinitializeStreamFrameData(*pStreamInfo, 0, numFrameData - 1);
}

void CGeomCacheManager::OnChangeBufferSize(ICVar* pCVar)
{
	GetGeomCacheManager()->ChangeBufferSize(pCVar->GetIVal());
}

void CGeomCacheManager::ChangeBufferSize(const uint newSizeInMiB)
{
	const uint numStreams = m_streamInfos.size();
	for (uint i = 0; i < numStreams; ++i)
	{
		SGeomCacheStreamInfo* pStreamInfo = m_streamInfos[i];
		AbortStreamAndWait(*pStreamInfo);
	}

	#if !defined(DEDICATED_SERVER)
	SAFE_RELEASE(m_pPool);
	if (m_pPoolBaseAddress)
	{
		CryGetIMemoryManager()->FreePages(m_pPoolBaseAddress, m_poolSize);
	}

	const int geomCacheBufferSize = clamp_tpl(newSizeInMiB, kMinBufferSizeInMiB, kMaxBufferSizeInMiB);
	GetCVars()->e_GeomCacheBufferSize = geomCacheBufferSize;

	const uint kMiBtoBytesFactor = 1024 * 1024;
	m_poolSize = geomCacheBufferSize * kMiBtoBytesFactor;

	m_pPoolBaseAddress = CryGetIMemoryManager()->AllocPages(m_poolSize);
	m_pPool = gEnv->pSystem->GetIMemoryManager()->CreateGeneralMemoryHeap(m_pPoolBaseAddress, m_poolSize, "GEOMCACHE_POOL");
	#endif
}

void CGeomCacheManager::ReinitializeStreamFrameData(SGeomCacheStreamInfo& streamInfo, uint startFrame, uint endFrame)
{
	const CGeomCache* pGeomCache = streamInfo.m_pGeomCache;

	const uint frameDataSize = streamInfo.m_frameData.size();
	startFrame = std::min(frameDataSize - 1, startFrame);
	endFrame = std::min(frameDataSize - 1, endFrame);

	for (uint i = startFrame; i <= endFrame; ++i)
	{
		SGeomCacheStreamInfo::SFrameData& frameData = streamInfo.m_frameData[i];

		frameData.m_bDecompressJobLaunched = false;
		frameData.m_pDecompressHandle = NULL;

		const GeomCacheFile::EFrameType frameType = pGeomCache->GetFrameType(i);

		if (frameType == GeomCacheFile::eFrameType_IFrame)
		{
			frameData.m_decodeDependencyCounter = 1;
		}
		else if (frameType == GeomCacheFile::eFrameType_BFrame)
		{
			assert(i > 0);

			if (pGeomCache->GetFrameType(i - 1) == GeomCacheFile::eFrameType_IFrame)
			{
				frameData.m_decodeDependencyCounter = 3;
			}
			else
			{
				frameData.m_decodeDependencyCounter = 2;
			}
		}
	}
}

void CGeomCacheManager::UnRegisterForStreaming(CGeomCacheRenderNode* pRenderNode, bool bWaitForJobs)
{
	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	TStreamInfosIter iter = m_streamInfos.begin();
	while (iter != m_streamInfos.end())
	{
		SGeomCacheStreamInfo* pStreamInfo = *iter;
		if (pStreamInfo->m_pRenderNode == pRenderNode)
		{
			if (pStreamInfo->m_pNewestReadRequestHandle || pStreamInfo->m_pOldestDecompressHandle)
			{
				gEnv->pLog->LogWarning("Unregistering stream %s while still active", pStreamInfo->m_pRenderNode->GetName());
			}

			if (!bWaitForJobs)
			{
				AbortStream(*pStreamInfo);
			}
			else
			{
				AbortStreamAndWait(*pStreamInfo);
			}

			m_streamInfosAbortList.push_back(pStreamInfo);
			iter = m_streamInfos.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	RetireRemovedStreams();
}

void CGeomCacheManager::StreamingUpdate()
{
	FUNCTION_PROFILER_3DENGINE;

	const bool bCachesActive = GetCVars()->e_GeomCaches != 0;

	RetireRemovedStreams();

	const uint numStreams = m_streamInfos.size();
	for (uint i = 0; i < numStreams; ++i)
	{
		SGeomCacheStreamInfo& streamInfo = *m_streamInfos[i];

		if (streamInfo.m_fillRenderNodeJobState.IsRunning())
		{
			CRY_PROFILE_REGION(PROFILE_3DENGINE, "CGeomCacheManager::StreamingUpdate_WaitForLastFillJob");
			gEnv->pJobManager->WaitForJob(streamInfo.m_fillRenderNodeJobState);
		}

		// Update wanted playback frame
		CGeomCacheRenderNode* pRenderNode = streamInfo.m_pRenderNode;
		const CGeomCache* pGeomCache = streamInfo.m_pGeomCache;
		streamInfo.m_wantedPlaybackTime = pRenderNode->GetPlaybackTime();
		streamInfo.m_wantedFloorFrame = pGeomCache->GetFloorFrameIndex(streamInfo.m_wantedPlaybackTime);
		streamInfo.m_wantedCeilFrame = pGeomCache->GetCeilFrameIndex(streamInfo.m_wantedPlaybackTime);
		streamInfo.m_bLooping = streamInfo.m_pRenderNode->IsLooping();

		if (!streamInfo.m_bLooping)
		{
			const uint numFrames = streamInfo.m_numFrames;
			streamInfo.m_wantedFloorFrame = std::min((uint)streamInfo.m_wantedFloorFrame, numFrames - 1);
			streamInfo.m_wantedCeilFrame = std::min((uint)streamInfo.m_wantedCeilFrame, numFrames - 1);
		}

		assert(streamInfo.m_wantedFloorFrame + 1 == streamInfo.m_wantedCeilFrame
		       || streamInfo.m_wantedFloorFrame == streamInfo.m_wantedCeilFrame);

		// Update bbox for this frame
		pRenderNode->UpdateBBox();

		// Abort stream and trash it if it's not valid anymore
		ValidateStream(streamInfo);

		// Try to trash as many aborted handles as possible
		RetireAbortedHandles(streamInfo);

		// Retire handles that are not needed anymore
		RetireHandles(streamInfo);
	}

	if (bCachesActive)
	{
		const CTimeValue currentFrameTime = GetTimer()->GetFrameStartTime();
		LaunchStreamingJobs(numStreams, currentFrameTime);
	}

	// Start disk read requests alternating between geom cache render nodes until no more
	// can be issued (buffers full, max read ahead time reached, no free request object)
	bool bMoreRequests = true;
	while (bMoreRequests && bCachesActive)
	{
		bMoreRequests = false;
		uint nextRequestStream = m_lastRequestStream + 1;

		for (uint i = 0; i < numStreams; ++i)
		{
			const uint requestStream = (nextRequestStream + i) % numStreams;

			SGeomCacheStreamInfo& streamInfo = *m_streamInfos[i];
			const CGeomCacheRenderNode* pRenderNode = streamInfo.m_pRenderNode;
			const bool bIsStreaming = pRenderNode->IsStreaming();
			const CGeomCache* pGeomCache = streamInfo.m_pGeomCache;
			const bool bPlaybackFromMemory = pGeomCache->PlaybackFromMemory();
			const float playbackFrame = streamInfo.m_wantedPlaybackTime;
			const float displayedFrame = streamInfo.m_displayedFrameTime;

			if (!bPlaybackFromMemory && (bIsStreaming || (displayedFrame != playbackFrame)))
			{
				const bool bRequestIssued = IssueDiskReadRequest(streamInfo);

				if (bRequestIssued)
				{
					m_lastRequestStream = requestStream;
				}

				bMoreRequests |= bRequestIssued;
			}
		}
	}

	#ifndef _RELEASE
	for (uint i = 0; i < numStreams; ++i)
	{
		SGeomCacheStreamInfo& streamInfo = *m_streamInfos[i];
		streamInfo.m_pRenderNode->DebugRender();
	}
	#endif
}

void CGeomCacheManager::LaunchStreamingJobs(const uint numStreams, const CTimeValue currentFrameTime)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	// Launch streaming jobs
	for (uint i = 0; i < numStreams; ++i)
	{
		SGeomCacheStreamInfo* pStreamInfo = m_streamInfos[i];
		CGeomCacheRenderNode* pRenderNode = pStreamInfo->m_pRenderNode;
		const CGeomCache* pGeomCache = pStreamInfo->m_pGeomCache;

		if (!pGeomCache)
		{
			continue;
		}

		const bool bIsStreaming = pRenderNode->IsStreaming();
		const float playbackFrameTime = pStreamInfo->m_wantedPlaybackTime;
		const float displayedFrameTime = pStreamInfo->m_displayedFrameTime;

		LaunchDecompressJobs(pStreamInfo, currentFrameTime);

		const bool bSameFrame = playbackFrameTime == displayedFrameTime;
		if (!bSameFrame || pStreamInfo->m_sameFrameFillCount < 2)
		{
			pRenderNode->StartAsyncUpdate();
			TGeomCacheFillRenderNodeAsyncJob fillRenderNodeAsyncJob(pStreamInfo);
			fillRenderNodeAsyncJob.SetClassInstance(this);
			fillRenderNodeAsyncJob.RegisterJobState(&pStreamInfo->m_fillRenderNodeJobState);
			fillRenderNodeAsyncJob.SetPriorityLevel(JobManager::eHighPriority);
			fillRenderNodeAsyncJob.Run();
		}
	}
}

void CGeomCacheManager::RetireRemovedStreams()
{
	FUNCTION_PROFILER_3DENGINE;

	TStreamInfosIter iter = m_streamInfosAbortList.begin();

	while (iter != m_streamInfosAbortList.end())
	{
		SGeomCacheStreamInfo* pStreamInfo = *iter;

		gEnv->GetJobManager()->WaitForJob(pStreamInfo->m_fillRenderNodeJobState);
		RetireAbortedHandles(*pStreamInfo);

		if (pStreamInfo->m_fillRenderNodeJobState.IsRunning()
		    || pStreamInfo->m_pReadAbortListHead || pStreamInfo->m_pDecompressAbortListHead)
		{
			++iter;
		}
		else
		{
			// Nothing left running, we can finally delete the stream
			CGeomCache* pGeomCache = pStreamInfo->m_pGeomCache;
			pGeomCache->DecreaseNumStreams();

			pStreamInfo->m_pRenderNode->ClearFillData();

			delete pStreamInfo;
			iter = m_streamInfosAbortList.erase(iter);
		}
	}

	for (TGeomCacheMap::iterator iter = m_nameToGeomCacheMap.begin(); iter != m_nameToGeomCacheMap.end(); ++iter)
	{
		CGeomCache* pGeomCache = iter->second;
		if (pGeomCache->GetNumStreams() == 0)
		{
			pGeomCache->UnloadData();
		}
	}
}

void CGeomCacheManager::ValidateStream(SGeomCacheStreamInfo& streamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	const CGeomCacheRenderNode* pRenderNode = streamInfo.m_pRenderNode;
	const bool bIsStreaming = pRenderNode->IsStreaming();
	const float wantedPlaybackTime = streamInfo.m_wantedPlaybackTime;
	const float displayedFrameTime = streamInfo.m_displayedFrameTime;

	if (!pRenderNode->IsStreaming() && (displayedFrameTime == wantedPlaybackTime) && streamInfo.m_sameFrameFillCount >= 2)
	{
		AbortStream(streamInfo);
		return;
	}

	// Abort if there was an error in the stream. Also set the render node to not play back in this case.
	if (streamInfo.m_pOldestReadRequestHandle && streamInfo.m_pOldestReadRequestHandle->m_error != 0)
	{
		++m_numStreamAborts;
		++m_numErrorAborts;
		gEnv->pLog->LogError("Error in cache stream %s", streamInfo.m_pRenderNode->GetName());
		AbortStream(streamInfo);
		streamInfo.m_pRenderNode->StopStreaming();
		return;
	}

	const CGeomCache* pGeomCache = streamInfo.m_pGeomCache;
	const float currentCacheStreamingTime = streamInfo.m_pRenderNode->GetStreamingTime();
	const uint wantedFloorFrame = pGeomCache->GetFloorFrameIndex(currentCacheStreamingTime);

	// Check if stream is invalid
	bool bAbort = false;

	if (streamInfo.m_pOldestDecompressHandle)
	{
		if ((streamInfo.m_pOldestDecompressHandle->m_startFrame > wantedFloorFrame)
		    || (streamInfo.m_pNewestDecompressHandle->m_endFrame < wantedFloorFrame))
		{
			gEnv->pLog->LogWarning("Aborting cache stream %s (decompress stream: [%u, %u], wanted frame: %u)", streamInfo.m_pRenderNode->GetName(),
			                       streamInfo.m_pOldestDecompressHandle->m_startFrame, streamInfo.m_pNewestDecompressHandle->m_endFrame, wantedFloorFrame);
			++m_numDecompressStreamAborts;
			bAbort = true;
		}
	}
	else if (streamInfo.m_pOldestReadRequestHandle)
	{
		if (streamInfo.m_pOldestReadRequestHandle->m_startFrame > wantedFloorFrame)
		{
			gEnv->pLog->LogWarning("Aborting cache stream %s (read stream start: %u, wanted frame: %u)", streamInfo.m_pRenderNode->GetName(),
			                       streamInfo.m_pOldestReadRequestHandle->m_startFrame, wantedFloorFrame);
			++m_numReadStreamAborts;
			bAbort = true;
		}
	}

	if (bAbort)
	{
		++m_numStreamAborts;
		AbortStream(streamInfo);
	}
}

void CGeomCacheManager::AbortStream(SGeomCacheStreamInfo& streamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	streamInfo.m_bAbort = true;

	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "CGeomCacheManager::AbortStream_LockFillRenderNode");
		streamInfo.m_abortCS.Lock();
	}

	if (streamInfo.m_pNewestReadRequestHandle)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "CGeomCacheManager::AbortStream_AbortReads");

		assert(streamInfo.m_pOldestReadRequestHandle != NULL);

		// Abort read requests if possible
		for (SGeomCacheReadRequestHandle* pCurrentReadRequestHandle = streamInfo.m_pOldestReadRequestHandle;
		     pCurrentReadRequestHandle; pCurrentReadRequestHandle = static_cast<SGeomCacheReadRequestHandle*>(pCurrentReadRequestHandle->m_pNext))
		{
			if (pCurrentReadRequestHandle->m_pReadStream)
			{
				pCurrentReadRequestHandle->m_pReadStream->TryAbort();
			}
		}

		// Put all handles in the read request list on the abort list
		assert(streamInfo.m_pNewestReadRequestHandle->m_pNext == NULL);
		streamInfo.m_pNewestReadRequestHandle->m_pNext = streamInfo.m_pReadAbortListHead;
		streamInfo.m_pReadAbortListHead = streamInfo.m_pOldestReadRequestHandle;

		// Mark read request list as empty
		streamInfo.m_pOldestReadRequestHandle = NULL;
		streamInfo.m_pNewestReadRequestHandle = NULL;
	}

	if (streamInfo.m_pOldestDecompressHandle)
	{
		CRY_PROFILE_REGION(PROFILE_3DENGINE, "CGeomCacheManager::AbortStream_AbortDecompress");

		assert(streamInfo.m_pOldestDecompressHandle != NULL);

		// Put all handles in the decompress list on the abort list
		assert(streamInfo.m_pNewestDecompressHandle->m_pNext == NULL);
		streamInfo.m_pNewestDecompressHandle->m_pNext = streamInfo.m_pDecompressAbortListHead;
		streamInfo.m_pDecompressAbortListHead = streamInfo.m_pOldestDecompressHandle;

		// Mark read decompress list as empty
		streamInfo.m_pNewestDecompressHandle = NULL;
		streamInfo.m_pOldestDecompressHandle = NULL;
	}

	assert(streamInfo.m_pOldestDecompressHandle == NULL
	       && streamInfo.m_pNewestDecompressHandle == NULL);

	streamInfo.m_numFramesMissed = 0;
	streamInfo.m_bAbort = false;
	streamInfo.m_bLooping = false;
	streamInfo.m_abortCS.Unlock();
}

void CGeomCacheManager::AbortStreamAndWait(SGeomCacheStreamInfo& streamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	AbortStream(streamInfo);

	gEnv->pJobManager->WaitForJob(streamInfo.m_fillRenderNodeJobState);

	CryMutex dummyCS;

	// Wait for all read requests to finish
	for (SGeomCacheReadRequestHandle* pCurrentAbortedHandle = streamInfo.m_pReadAbortListHead;
	     pCurrentAbortedHandle; pCurrentAbortedHandle = static_cast<SGeomCacheReadRequestHandle*>(pCurrentAbortedHandle->m_pNext))
	{
		CryAutoLock<CryMutex> lock(dummyCS);
		while (pCurrentAbortedHandle->m_numJobReferences > 0)
		{
			pCurrentAbortedHandle->m_jobReferencesCV.Wait(dummyCS);
		}

		if (pCurrentAbortedHandle->m_pReadStream)
		{
			pCurrentAbortedHandle->m_pReadStream->Wait();
		}
	}

	for (SGeomCacheBufferHandle* pCurrentAbortedHandle = streamInfo.m_pDecompressAbortListHead;
	     pCurrentAbortedHandle; pCurrentAbortedHandle = pCurrentAbortedHandle->m_pNext)
	{
		CryAutoLock<CryMutex> lock(dummyCS);
		while (pCurrentAbortedHandle->m_numJobReferences > 0)
		{
			pCurrentAbortedHandle->m_jobReferencesCV.Wait(dummyCS);
		}
	}

	// And finally retire their handles
	RetireAbortedHandles(streamInfo);

	assert(streamInfo.m_pReadAbortListHead == NULL);
}

void CGeomCacheManager::RetireAbortedHandles(SGeomCacheStreamInfo& streamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	SGeomCacheReadRequestHandle* pNextReadRequestAbortHandle = NULL;
	for (SGeomCacheReadRequestHandle* pCurrentAbortedHandle = streamInfo.m_pReadAbortListHead;
	     pCurrentAbortedHandle; pCurrentAbortedHandle = pNextReadRequestAbortHandle)
	{
		// If jobs are still running, wait till next frame
		if (pCurrentAbortedHandle->m_numJobReferences > 0 || (pCurrentAbortedHandle->m_pReadStream && !pCurrentAbortedHandle->m_pReadStream->IsFinished()))
		{
			break;
		}

		// Remove from abort list
		pNextReadRequestAbortHandle = static_cast<SGeomCacheReadRequestHandle*>(pCurrentAbortedHandle->m_pNext);
		streamInfo.m_pReadAbortListHead = pNextReadRequestAbortHandle;

		// And finally retire handle
		RetireBufferHandle(pCurrentAbortedHandle);
	}

	SGeomCacheBufferHandle* pNextDecompressAbortHandle = NULL;
	for (SGeomCacheBufferHandle* pCurrentAbortedHandle = streamInfo.m_pDecompressAbortListHead;
	     pCurrentAbortedHandle; pCurrentAbortedHandle = pNextDecompressAbortHandle)
	{
		// If jobs are still running, wait till next frame
		if (pCurrentAbortedHandle->m_numJobReferences > 0)
		{
			break;
		}

		// Remove from abort list
		pNextDecompressAbortHandle = pCurrentAbortedHandle->m_pNext;
		streamInfo.m_pDecompressAbortListHead = pNextDecompressAbortHandle;

		// And finally retire handle
		RetireDecompressHandle(streamInfo, pCurrentAbortedHandle);
	}
}

bool CGeomCacheManager::IssueDiskReadRequest(SGeomCacheStreamInfo& streamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	// Constants
	const CGeomCacheRenderNode* pRenderNode = streamInfo.m_pRenderNode;
	const CGeomCache* pGeomCache = streamInfo.m_pGeomCache;
	const bool bIsStreaming = pRenderNode->IsStreaming();

	const float currentCacheStreamingTime = streamInfo.m_pRenderNode->GetStreamingTime();
	const uint wantedFloorFrame = pGeomCache->GetFloorFrameIndex(currentCacheStreamingTime);
	const uint wantedCeilFrame = pGeomCache->GetCeilFrameIndex(currentCacheStreamingTime);
	const float minBufferAheadTime = std::max(0.1f, GetCVars()->e_GeomCacheMinBufferAheadTime);
	const float maxBufferAheadTime = std::max(1.0f, GetCVars()->e_GeomCacheMaxBufferAheadTime);
	const float cacheMinBufferAhead = currentCacheStreamingTime + minBufferAheadTime;
	const float cacheMaxBufferAhead = currentCacheStreamingTime + maxBufferAheadTime;

	const bool bLooping = streamInfo.m_bLooping;
	const uint numFrames = streamInfo.m_numFrames;

	const uint preferredDiskRequestSize = (size_t)std::max(0, GetCVars()->e_GeomCachePreferredDiskRequestSize * 1024);

	// Compute frame range that we want to read from disk
	uint frameRangeBegin = pGeomCache->GetPrevIFrame(wantedFloorFrame);
	uint frameRangeEnd = pGeomCache->GetNextIFrame(bIsStreaming ? pGeomCache->GetCeilFrameIndex(cacheMaxBufferAhead) : wantedFloorFrame);

	// Avoid reading an entire block if we are not streaming and time is precisely at an index frame.
	// This primarily helps when streaming in the first frame after a render node is created.
	if (!bIsStreaming && (wantedFloorFrame == wantedCeilFrame) &&
	    pGeomCache->GetFrameType(wantedFloorFrame) == GeomCacheFile::eFrameType_IFrame)
	{
		frameRangeBegin = wantedFloorFrame;
		frameRangeEnd = frameRangeBegin;
	}

	// Make sure not to re-request frames that are already in the decode buffer
	if (streamInfo.m_pNewestDecompressHandle)
	{
		uint decodedFramesEnd = streamInfo.m_pNewestDecompressHandle->m_endFrame;
		frameRangeBegin = std::max(decodedFramesEnd + 1, frameRangeBegin);
	}

	// Read request params and handle to be filled
	StreamReadParams params;
	SGeomCacheReadRequestHandle* pRequestHandle;

	{
		assert(!streamInfo.m_pOldestReadRequestHandle || streamInfo.m_pNewestReadRequestHandle);
		if (streamInfo.m_pNewestReadRequestHandle)
		{
			uint streamEndFrame = streamInfo.m_pNewestReadRequestHandle->m_endFrame;

			// Check if we already requested up to frameRangeEnd
			if (streamInfo.m_pNewestReadRequestHandle->m_endFrame >= frameRangeEnd)
			{
				return false;
			}

			frameRangeBegin = streamEndFrame + 1;
		}

		const float frameRangeBeginTime = pGeomCache->GetFrameTime(frameRangeBegin);
		if (frameRangeBeginTime > cacheMinBufferAhead)
		{
			return false;
		}

		if (!bLooping && frameRangeEnd >= (numFrames - 1))
		{
			frameRangeEnd = numFrames - 1;
		}

		if (frameRangeBegin > frameRangeEnd)
		{
			return false;
		}

		if ((frameRangeEnd - frameRangeBegin + 1) > numFrames)
		{
			frameRangeEnd = frameRangeBegin + numFrames - 1;
		}

		pGeomCache->ValidateReadRange(frameRangeBegin, frameRangeEnd);

		// Now that we have a final range of unread frames, make a read request
		uint requestSize = 0;
		for (uint currentFrame = frameRangeBegin; currentFrame <= frameRangeEnd; ++currentFrame)
		{
			requestSize += pGeomCache->GetFrameSize(currentFrame);

			// Stop if size has reached preferred request size and current frame is an index frame
			if (requestSize >= preferredDiskRequestSize && pGeomCache->GetFrameType(currentFrame) == GeomCacheFile::eFrameType_IFrame && frameRangeBegin != currentFrame)
			{
				frameRangeEnd = currentFrame;
				break;
			}
		}

		assert(requestSize > 0);

		// Allocate new request handle & buffer space
		pRequestHandle = NewReadRequestHandle(requestSize, streamInfo);
		if (!pRequestHandle)
		{
			return false;
		}

		// Init handle
		pRequestHandle->m_startFrame = frameRangeBegin;
		pRequestHandle->m_endFrame = frameRangeEnd;

		// Add request handle to stream linked list
		if (streamInfo.m_pNewestReadRequestHandle)
		{
			streamInfo.m_pNewestReadRequestHandle->m_pNext = pRequestHandle;
			streamInfo.m_pNewestReadRequestHandle = pRequestHandle;
		}
		else
		{
			streamInfo.m_pOldestReadRequestHandle = pRequestHandle;
			streamInfo.m_pNewestReadRequestHandle = pRequestHandle;
		}

		const float timeLeft = std::max(pGeomCache->GetFrameTime(frameRangeBegin) - currentCacheStreamingTime
		                                - static_cast<float>(GetCVars()->e_GeomCacheDecodeAheadTime), 0.0f);

		// Fill read request params
		params.nOffset = static_cast<uint>(pGeomCache->GetFrameOffset(frameRangeBegin));
		params.nSize = requestSize;
		params.pBuffer = pRequestHandle->m_pBuffer;
		params.ePriority = estpAboveNormal;
		params.nLoadTime = static_cast<uint>(timeLeft * 1000);
		params.nPerceptualImportance = 255;
		params.nFlags = IStreamEngine::FLAGS_NO_SYNC_CALLBACK;
	}

	// Issue request
	CryInterlockedIncrement(&pRequestHandle->m_numJobReferences);
	pRequestHandle->m_pReadStream = GetSystem()->GetStreamEngine()->StartRead(
	  eStreamTaskTypeGeomCache, pGeomCache->GetFilePath(), pRequestHandle, &params);

	// This can happen if streaming system is already shutting down. There will be no callback in this case, so decrement m_numJobReferences.
	if (pRequestHandle->m_pReadStream == NULL)
	{
		if (CryInterlockedDecrement(&pRequestHandle->m_numJobReferences) == 0)
		{
			pRequestHandle->m_jobReferencesCV.Notify();
		}
	}

	return true;
}

void CGeomCacheManager::LaunchDecompressJobs(SGeomCacheStreamInfo* pStreamInfo, const CTimeValue currentFrameTime)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	const CGeomCacheRenderNode* pRenderNode = pStreamInfo->m_pRenderNode;
	const CGeomCache* pGeomCache = pStreamInfo->m_pGeomCache;
	const GeomCacheFile::EBlockCompressionFormat blockCompressionFormat = pGeomCache->GetBlockCompressionFormat();
	const float currentCacheStreamingTime = pStreamInfo->m_pRenderNode->GetStreamingTime();
	const uint wantedFloorFrame = pGeomCache->GetFloorFrameIndex(currentCacheStreamingTime);
	const uint numStreamFrames = pStreamInfo->m_numFrames;
	const uint frameDataSize = pStreamInfo->m_frameData.size();

	// Need to check if there are still jobs running for the same render node on the stream abort list
	for (TStreamInfosIter iter = m_streamInfosAbortList.begin(); iter != m_streamInfosAbortList.end(); ++iter)
	{
		SGeomCacheStreamInfo* pCurrentStreamInfo = *iter;
		if (pStreamInfo->m_pRenderNode == pCurrentStreamInfo->m_pRenderNode)
		{
			return;
		}
	}

	// Wait until abort list has been processed before spawning new jobs
	if (pStreamInfo->m_pDecompressAbortListHead || pStreamInfo->m_pReadAbortListHead)
	{
		return;
	}

	for (SGeomCacheReadRequestHandle* pReadRequestHandle = pStreamInfo->m_pOldestReadRequestHandle; pReadRequestHandle;
	     pReadRequestHandle = static_cast<SGeomCacheReadRequestHandle*>(pReadRequestHandle->m_pNext))
	{
		if (pReadRequestHandle->m_frameTime == currentFrameTime)
		{
			// Don't decode frames that were read in the same render frame
			return;
		}

		if (pStreamInfo->m_bAbort)
		{
			return;
		}

		if (pReadRequestHandle->m_error)
		{
			return;
		}

		if (pReadRequestHandle->m_state != SGeomCacheReadRequestHandle::eRRHS_FinishedRead)
		{
			return;
		}

		// Stop decoding after e_GeomCacheDecodeAheadTime
		const float blockDeltaFromPlaybackTime = (pGeomCache->GetFrameTime(pReadRequestHandle->m_startFrame) - currentCacheStreamingTime);
		const float decodeAheadTime = GetCVars()->e_GeomCacheDecodeAheadTime;
		if (blockDeltaFromPlaybackTime > decodeAheadTime)
		{
			return;
		}

		const uint startFrame = pReadRequestHandle->m_startFrame;
		const uint endFrame = pReadRequestHandle->m_endFrame;

		// Need to check if stream is already referencing same frames when looping
		if (pStreamInfo->m_pOldestDecompressHandle && pStreamInfo->m_bLooping)
		{
			const uint checkRangeStart = pStreamInfo->m_pOldestDecompressHandle->m_startFrame;
			const uint checkRangeEnd = pStreamInfo->m_pNewestDecompressHandle->m_endFrame;

			const uint startRangeMod = checkRangeStart % frameDataSize;
			const uint endRangeMod = checkRangeEnd % frameDataSize;
			const uint startFrameMod = startFrame % frameDataSize;
			// Check range must be extended to next index frame because retiring of stream begin could otherwise overwrite frame data
			const uint endFrameMod = pGeomCache->GetNextIFrame(endFrame) % frameDataSize;

			const bool bRangeWraps = endRangeMod < startRangeMod;
			const bool bFramesWrap = endFrameMod < startFrameMod;

			// check all four different cases for range overlapping
			if ((bRangeWraps && bFramesWrap)
			    || (bRangeWraps && !bFramesWrap && (startFrameMod <= endRangeMod || endFrameMod >= startRangeMod))
			    || (!bRangeWraps && bFramesWrap && (startRangeMod <= endFrameMod || endRangeMod >= startFrameMod))
			    || (!bRangeWraps && !bFramesWrap && (startFrameMod <= endRangeMod && startRangeMod <= endFrameMod)))
			{
				return;
			}
		}

		// Determine size for decompression buffer
		const uint numFrames = (endFrame - startFrame) + 1;
		const uint32 decompressBlockSize = GeomCacheDecoder::GetDecompressBufferSize(pReadRequestHandle->m_pBuffer, numFrames);

		SGeomCacheBufferHandle* pNewDecompressBufferHandle = NewBufferHandle<SGeomCacheBufferHandle>(decompressBlockSize, *pStreamInfo);
		if (!pNewDecompressBufferHandle)
		{
			// Could not allocate space for decompression
			return;
		}

		// Zero frame headers
		memset(pNewDecompressBufferHandle->m_pBuffer, 0, sizeof(SGeomCacheFrameHeader) * numFrames);

		pReadRequestHandle->m_state = SGeomCacheReadRequestHandle::eRRHS_Decompressing;
		pNewDecompressBufferHandle->m_startFrame = startFrame;
		pNewDecompressBufferHandle->m_endFrame = endFrame;

		for (uint i = startFrame; i <= endFrame; ++i)
		{
			const uint frameIndex = i % frameDataSize;
			assert(!pStreamInfo->m_frameData[frameIndex].m_pDecompressHandle);
			pStreamInfo->m_frameData[frameIndex].m_pDecompressHandle = pNewDecompressBufferHandle;
		}

		// Add to decompress request handle linked list
		{
			assert(!pStreamInfo->m_pOldestDecompressHandle || pStreamInfo->m_pNewestDecompressHandle);

			// Add request handle to stream linked list
			if (pStreamInfo->m_pNewestDecompressHandle)
			{
				pStreamInfo->m_pNewestDecompressHandle->m_pNext = pNewDecompressBufferHandle;
				pStreamInfo->m_pNewestDecompressHandle = pNewDecompressBufferHandle;
			}
			else
			{
				pStreamInfo->m_pOldestDecompressHandle = pNewDecompressBufferHandle;
				pStreamInfo->m_pNewestDecompressHandle = pNewDecompressBufferHandle;
			}
		}

		for (uint i = 0; i < numFrames; ++i)
		{
			const uint frameIndex = startFrame + i;

			// For b frames we need to make sure that jobs for previous frames were launched. Otherwise m_numJobReferences will
			// never reach zero, because the frame decode job has a dependency job that was never launched.
			if (pGeomCache->GetFrameType(frameIndex) == GeomCacheFile::eFrameType_IFrame
			    || pStreamInfo->m_frameData[(frameIndex - 1) % frameDataSize].m_bDecompressJobLaunched)
			{
				CryInterlockedIncrement(&pReadRequestHandle->m_numJobReferences);
				CryInterlockedIncrement(&pNewDecompressBufferHandle->m_numJobReferences);
				CryInterlockedIncrement(&pNewDecompressBufferHandle->m_numJobReferences);

				pStreamInfo->m_frameData[frameIndex % frameDataSize].m_bDecompressJobLaunched = true;
				TGeomCacheDecompressJob decompressJob(pStreamInfo, i, pNewDecompressBufferHandle, pReadRequestHandle);
				decompressJob.SetClassInstance(this);
				decompressJob.SetPriorityLevel(JobManager::eStreamPriority);
				decompressJob.Run();
			}
		}
	}
}

void CGeomCacheManager::DecompressFrame_JobEntry(SGeomCacheStreamInfo* pStreamInfo, const uint blockIndex,
                                                 SGeomCacheBufferHandle* pDecompressHandle, SGeomCacheReadRequestHandle* pReadRequestHandle)
{
	FUNCTION_PROFILER_3DENGINE;

	const CGeomCacheRenderNode* pRenderNode = pStreamInfo->m_pRenderNode;
	const CGeomCache* pGeomCache = pStreamInfo->m_pGeomCache;
	const uint frameIndex = pDecompressHandle->m_startFrame + blockIndex;

	if (!pStreamInfo->m_bAbort && !pStreamInfo->m_pDecompressAbortListHead)
	{
		const GeomCacheFile::EBlockCompressionFormat blockCompressionFormat = pGeomCache->GetBlockCompressionFormat();
		const uint numFrames = pReadRequestHandle->m_endFrame - pReadRequestHandle->m_startFrame + 1;

		SGeomCacheFrameHeader* pHeader = GetFrameDecompressHeader(pStreamInfo, frameIndex);
		if (!pHeader || pHeader->m_state != SGeomCacheFrameHeader::eFHS_Uninitialized)
		{
			CryFatalError("Trying to access uninitialized data while decoding an index frame");
		}

		if (!GeomCacheDecoder::DecompressBlocks(blockCompressionFormat, pDecompressHandle->m_pBuffer,
		                                        pReadRequestHandle->m_pBuffer, blockIndex, 1, numFrames))
		{
			// Decompress block size failed: Flag error
			pReadRequestHandle->m_error = 1;
		}
	}

	if (CryInterlockedDecrement(&pReadRequestHandle->m_numJobReferences) == 0)
	{
		pReadRequestHandle->m_state = SGeomCacheReadRequestHandle::eRRHS_Done;
		pReadRequestHandle->m_jobReferencesCV.Notify();
	}

	if (CryInterlockedDecrement(&pDecompressHandle->m_numJobReferences) == 0)
	{
		pDecompressHandle->m_jobReferencesCV.Notify();
	}

	const int newDependencyCounter = CryInterlockedDecrement(GetDependencyCounter(pStreamInfo, frameIndex));

	if (newDependencyCounter < 0 || newDependencyCounter > 2)
	{
		CryFatalError("Invalid dependency counter");
	}
	else if (newDependencyCounter == 0)
	{
		SDecodeFrameJobData jobData;
		jobData.m_frameIndex = frameIndex;
		jobData.m_pGeomCache = pGeomCache;
		jobData.m_pStreamInfo = pStreamInfo;
		LaunchDecodeJob(jobData);
	}
}

void CGeomCacheManager::LaunchDecodeJob(SDecodeFrameJobData jobData)
{
	const GeomCacheFile::EFrameType frameType = jobData.m_pGeomCache->GetFrameType(jobData.m_frameIndex);

	switch (frameType)
	{
	case GeomCacheFile::eFrameType_IFrame:
		{
			TGeomCacheIFrameDecodeJob decodeJob(jobData);
			decodeJob.SetClassInstance(this);
			decodeJob.SetPriorityLevel(JobManager::eStreamPriority);
			decodeJob.Run();
			break;
		}
	case GeomCacheFile::eFrameType_BFrame:
		{
			TGeomCacheBFrameDecodeJob decodeJob(jobData);
			decodeJob.SetClassInstance(this);
			decodeJob.SetPriorityLevel(JobManager::eStreamPriority);
			decodeJob.Run();
			break;
		}
	}
}

void CGeomCacheManager::DecodeIFrame_JobEntry(SDecodeFrameJobData jobData)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!jobData.m_pStreamInfo->m_bAbort && !jobData.m_pStreamInfo->m_pDecompressAbortListHead)
	{
		char* pFrameData = GetFrameDecompressData(jobData.m_pStreamInfo, jobData.m_frameIndex);
		GeomCacheDecoder::DecodeIFrame(jobData.m_pGeomCache, pFrameData);

		SGeomCacheFrameHeader* pHeader = GetFrameDecompressHeader(jobData.m_pStreamInfo, jobData.m_frameIndex);

		if (pHeader->m_state != SGeomCacheFrameHeader::eFHS_Undecoded)
		{
			CryFatalError("Trying to access uninitialized data while decoding an index frame");
		}

		pHeader->m_state = SGeomCacheFrameHeader::eFHS_Decoded;
	}

	SGeomCacheBufferHandle* pHandle = GetFrameDecompressHandle(jobData.m_pStreamInfo, jobData.m_frameIndex);
	if (CryInterlockedDecrement(&pHandle->m_numJobReferences) == 0)
	{
		pHandle->m_jobReferencesCV.Notify();
	}

	// Decrement dependency counter of first b frame after previous index frame and launch job if ready
	const uint prevIFrame = jobData.m_pGeomCache->GetPrevIFrame(jobData.m_frameIndex);
	if (prevIFrame < jobData.m_frameIndex)
	{
		const uint bFrameIndex = prevIFrame + 1;
		if (jobData.m_pGeomCache->GetFrameType(bFrameIndex) == GeomCacheFile::eFrameType_BFrame)
		{
			const int newDependencyCounter = CryInterlockedDecrement(GetDependencyCounter(jobData.m_pStreamInfo, bFrameIndex));

			if (newDependencyCounter < 0 || newDependencyCounter > 2)
			{
				CryFatalError("Invalid dependency counter");
			}
			else if (newDependencyCounter == 0)
			{
				SDecodeFrameJobData bFrameJobState = jobData;
				bFrameJobState.m_frameIndex = bFrameIndex;
				LaunchDecodeJob(bFrameJobState);
			}
		}
	}

	// Decrement dependency counter of b frame right after index frame and launch job if ready
	const uint numFrames = jobData.m_pStreamInfo->m_numFrames;
	if ((jobData.m_frameIndex % numFrames) + 1 < numFrames)
	{
		const uint bFrameIndex = jobData.m_frameIndex + 1;
		if (jobData.m_pGeomCache->GetFrameType(bFrameIndex) == GeomCacheFile::eFrameType_BFrame)
		{
			const int newDependencyCounter = CryInterlockedDecrement(GetDependencyCounter(jobData.m_pStreamInfo, bFrameIndex));

			if (newDependencyCounter < 0 || newDependencyCounter > 2)
			{
				CryFatalError("Invalid dependency counter");
			}
			else if (newDependencyCounter == 0)
			{
				SDecodeFrameJobData bFrameJobState = jobData;
				bFrameJobState.m_frameIndex = bFrameIndex;
				LaunchDecodeJob(bFrameJobState);
			}
		}
	}
}

void CGeomCacheManager::DecodeBFrame_JobEntry(SDecodeFrameJobData jobData)
{
	FUNCTION_PROFILER_3DENGINE;

	const uint prevIFrame = jobData.m_pGeomCache->GetPrevIFrame(jobData.m_frameIndex);
	const uint nextIFrame = jobData.m_pGeomCache->GetNextIFrame(jobData.m_frameIndex);

	if (!jobData.m_pStreamInfo->m_bAbort && !jobData.m_pStreamInfo->m_pDecompressAbortListHead)
	{
		char* pFrameData = GetFrameDecompressData(jobData.m_pStreamInfo, jobData.m_frameIndex);

		// For frames that have 0 influence for motion the predictor will still read data
		// from the prev frame pointers, so just set it to the current frame's data.
		char* pPrevFramesData[2] = { pFrameData, pFrameData };
		if (jobData.m_pGeomCache->NeedsPrevFrames(jobData.m_frameIndex))
		{
			pPrevFramesData[0] = GetFrameDecompressData(jobData.m_pStreamInfo, jobData.m_frameIndex - 2);
			pPrevFramesData[1] = GetFrameDecompressData(jobData.m_pStreamInfo, jobData.m_frameIndex - 1);
		}

		SGeomCacheFrameHeader* pPrevIFrameHeader = GetFrameDecompressHeader(jobData.m_pStreamInfo, prevIFrame);
		SGeomCacheFrameHeader* pNextIFrameHeader = GetFrameDecompressHeader(jobData.m_pStreamInfo, nextIFrame);

		if (pPrevIFrameHeader->m_state != SGeomCacheFrameHeader::eFHS_Decoded
		    || pNextIFrameHeader->m_state != SGeomCacheFrameHeader::eFHS_Decoded)
		{
			CryFatalError("Trying to access invalid data while decoding a b frame");
		}

		char* pFloorIndexFrameData = GetFrameDecompressData(jobData.m_pStreamInfo, prevIFrame);
		char* pCeilIndexFrameData = GetFrameDecompressData(jobData.m_pStreamInfo, nextIFrame);

		GeomCacheDecoder::DecodeBFrame(jobData.m_pGeomCache, pFrameData, pPrevFramesData, pFloorIndexFrameData, pCeilIndexFrameData);

		SGeomCacheFrameHeader* pHeader = GetFrameDecompressHeader(jobData.m_pStreamInfo, jobData.m_frameIndex);

		if (pHeader->m_state != SGeomCacheFrameHeader::eFHS_Undecoded)
		{
			CryFatalError("Trying to access invalid data while decoding a b frame");
		}

		pHeader->m_state = SGeomCacheFrameHeader::eFHS_Decoded;
	}

	SGeomCacheBufferHandle* pHandle = GetFrameDecompressHandle(jobData.m_pStreamInfo, jobData.m_frameIndex);
	if (CryInterlockedDecrement(&pHandle->m_numJobReferences) == 0)
	{
		pHandle->m_jobReferencesCV.Notify();
	}

	// Decrement dependency counter of b frame right after frame and launch job if ready
	const uint numFrames = jobData.m_pStreamInfo->m_numFrames;
	if ((jobData.m_frameIndex % numFrames) + 1 < numFrames)
	{
		const uint bFrameIndex = jobData.m_frameIndex + 1;
		if (jobData.m_pGeomCache->GetFrameType(bFrameIndex) == GeomCacheFile::eFrameType_BFrame)
		{
			const int newDependencyCounter = CryInterlockedDecrement(GetDependencyCounter(jobData.m_pStreamInfo, bFrameIndex));

			if (newDependencyCounter < 0 || newDependencyCounter > 2)
			{
				CryFatalError("Invalid dependency counter");
			}
			else if (newDependencyCounter == 0)
			{
				SDecodeFrameJobData bFrameJobState = jobData;
				bFrameJobState.m_frameIndex = bFrameIndex;
				LaunchDecodeJob(bFrameJobState);
			}
		}
	}
}

void CGeomCacheManager::FillRenderNodeAsync_JobEntry(SGeomCacheStreamInfo* pStreamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	// Prevent the stream from aborting while filling the render buffer
	CryAutoLock<CryCriticalSection> abortLock(pStreamInfo->m_abortCS);

	CGeomCacheRenderNode* pRenderNode = pStreamInfo->m_pRenderNode;
	const CGeomCache* pGeomCache = pStreamInfo->m_pGeomCache;

	if (pStreamInfo->m_bAbort || pStreamInfo->m_pDecompressAbortListHead)
	{
		// End job if abort flag was raised
		pRenderNode->SkipFrameFill();
		return;
	}

	const uint floorPlaybackFrame = pStreamInfo->m_wantedFloorFrame;
	const uint ceilPlaybackFrame = pStreamInfo->m_wantedCeilFrame;
	bool bFrameFilled = false;

	const char* pFloorFrameData = NULL;
	const char* pCeilFrameData = NULL;

	if (!pGeomCache->PlaybackFromMemory())
	{
		SGeomCacheFrameHeader* pFloorHeader = GetFrameDecompressHeader(pStreamInfo, floorPlaybackFrame);
		SGeomCacheFrameHeader* pCeilHeader = GetFrameDecompressHeader(pStreamInfo, ceilPlaybackFrame);

		if (pFloorHeader && pFloorHeader->m_state == SGeomCacheFrameHeader::eFHS_Decoded && pCeilHeader && pCeilHeader->m_state == SGeomCacheFrameHeader::eFHS_Decoded)
		{
			pFloorFrameData = GetFrameDecompressData(pStreamInfo, floorPlaybackFrame);
			pCeilFrameData = GetFrameDecompressData(pStreamInfo, ceilPlaybackFrame);
		}
	}
	else
	{
		pFloorFrameData = pGeomCache->GetFrameData(floorPlaybackFrame % pGeomCache->GetNumFrames());
		pCeilFrameData = pGeomCache->GetFrameData(ceilPlaybackFrame % pGeomCache->GetNumFrames());
	}

	if (pFloorFrameData && pCeilFrameData)
	{
		const float floorFrameTime = pGeomCache->GetFrameTime(floorPlaybackFrame);
		const float ceilFrameTime = pGeomCache->GetFrameTime(ceilPlaybackFrame);
		const float wantedPlaybackTime = pStreamInfo->m_wantedPlaybackTime;

		assert(wantedPlaybackTime >= floorFrameTime && wantedPlaybackTime <= ceilFrameTime);

		float lerpFactor = 0.0f;
		if (ceilFrameTime != floorFrameTime)
		{
			lerpFactor = (wantedPlaybackTime - floorFrameTime) / (ceilFrameTime - floorFrameTime);
		}

		assert(lerpFactor >= 0.0f && lerpFactor <= 1.0f);

		// m_displayedFrameTime == -1.0f means uninitialized cache. Treat as same frame, because we only want to fill twice on load.
		const bool bSameFrame = (pStreamInfo->m_displayedFrameTime == pStreamInfo->m_wantedPlaybackTime) || (pStreamInfo->m_displayedFrameTime == -1.0f);

		if (bSameFrame)
		{
			CryInterlockedIncrement(&pStreamInfo->m_sameFrameFillCount);
		}
		else
		{
			pStreamInfo->m_sameFrameFillCount = 0;
		}

		if (!pRenderNode->FillFrameAsync(pFloorFrameData, pCeilFrameData, lerpFactor))
			pRenderNode->SkipFrameFill();

		pStreamInfo->m_displayedFrameTime = pStreamInfo->m_wantedPlaybackTime;
		bFrameFilled = true;
	}
	else
	{
		pRenderNode->SkipFrameFill();
	}

	if (!bFrameFilled && pRenderNode->IsStreaming())
	{
		// If this happens, then we don't have the necessary data in the decompression
		// buffer and the geom cache render node wasn't updated
		++pStreamInfo->m_numFramesMissed;
		++m_numMissedFrames;
	}
}

template<class TBufferHandleType> TBufferHandleType* CGeomCacheManager::NewBufferHandle(const uint32 size, SGeomCacheStreamInfo& streamInfo)
{
	if (gEnv->mMainThreadId != CryGetCurrentThreadId())
	{
		CryFatalError("CGeomCacheManager::NewBufferHandle must be called from main thread");
	}

	char* pBlock = NULL;

	size_t pointerSize = (sizeof(SGeomCacheBufferHandle*) + 15) & ~15;

#if !defined(DEDICATED_SERVER)
	// Try to allocate requested size in the buffer, otherwise return NULL
	pBlock = reinterpret_cast<char*>(m_pPool->Memalign(16, pointerSize + size, "geom cache block"));
#endif

	if (!pBlock)
	{
		++m_numFailedAllocs;
		return NULL;
	}

	// Remove from free list
	TBufferHandleType* pNewRequest = new TBufferHandleType();

	// Initialize and return request handle
	*alias_cast<TBufferHandleType**>(pBlock) = pNewRequest;
	pNewRequest->m_pBuffer = pBlock + pointerSize;
	pNewRequest->m_bufferSize = size;
	pNewRequest->m_frameTime = GetTimer()->GetFrameStartTime();
	pNewRequest->m_pStream = &streamInfo;

	return pNewRequest;
}

SGeomCacheReadRequestHandle* CGeomCacheManager::NewReadRequestHandle(const uint32 size, SGeomCacheStreamInfo& streamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	SGeomCacheBufferHandle* pNewHandle = NewBufferHandle<SGeomCacheReadRequestHandle>(size, streamInfo);
	SGeomCacheReadRequestHandle* pReadRequestHandle = static_cast<SGeomCacheReadRequestHandle*>(pNewHandle);

	if (pReadRequestHandle)
	{
		pReadRequestHandle->m_state = SGeomCacheReadRequestHandle::eRRHS_Reading;
		pReadRequestHandle->m_error = 0L;
		pReadRequestHandle->m_pReadStream = NULL;
	}

	return pReadRequestHandle;
}

void CGeomCacheManager::RetireHandles(SGeomCacheStreamInfo& streamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	CGeomCacheRenderNode* pRenderNode = streamInfo.m_pRenderNode;
	const CGeomCache* pGeomCache = streamInfo.m_pGeomCache;
	const float currentCacheStreamingTime = streamInfo.m_pRenderNode->GetStreamingTime();
	const uint wantedFloorFrame = pGeomCache->GetFloorFrameIndex(currentCacheStreamingTime);

	SGeomCacheBufferHandle* pNextReadRequestHandle = streamInfo.m_pOldestReadRequestHandle;
	while (pNextReadRequestHandle)
	{
		SGeomCacheReadRequestHandle* pCurrentReadRequestHandle = static_cast<SGeomCacheReadRequestHandle*>(pNextReadRequestHandle);
		pNextReadRequestHandle = pCurrentReadRequestHandle->m_pNext;

		const uint handleEndFrame = pCurrentReadRequestHandle->m_endFrame;

		if (pCurrentReadRequestHandle->m_numJobReferences == 0 &&
		    ((handleEndFrame + 2) < wantedFloorFrame || pCurrentReadRequestHandle->m_state == SGeomCacheReadRequestHandle::eRRHS_Done))
		{
			RetireOldestReadRequestHandle(streamInfo);
		}
		else
		{
			break;
		}
	}

	SGeomCacheBufferHandle* pNextDecompressHandle = streamInfo.m_pOldestDecompressHandle;
	while (pNextDecompressHandle)
	{
		SGeomCacheBufferHandle* pCurrentDecompressHandle = pNextDecompressHandle;
		pNextDecompressHandle = pCurrentDecompressHandle->m_pNext;

		const uint handleEndFrame = pCurrentDecompressHandle->m_endFrame;

		// We also wait for the jobs of the next frame because of b frame -> index frame back references
		if (((handleEndFrame + 2) < wantedFloorFrame) && (pCurrentDecompressHandle->m_numJobReferences == 0)
		    && !(pCurrentDecompressHandle->m_pNext && pCurrentDecompressHandle->m_pNext->m_numJobReferences > 0))
		{
			RetireOldestDecompressHandle(streamInfo);
		}
		else
		{
			break;
		}
	}
}

void CGeomCacheManager::RetireOldestReadRequestHandle(SGeomCacheStreamInfo& streamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	SGeomCacheReadRequestHandle* pOldestHandle;

	pOldestHandle = streamInfo.m_pOldestReadRequestHandle;

	if (pOldestHandle == streamInfo.m_pNewestReadRequestHandle)
	{
		assert(pOldestHandle->m_pNext == NULL);
		streamInfo.m_pOldestReadRequestHandle = NULL;
		streamInfo.m_pNewestReadRequestHandle = NULL;
	}
	else
	{
		streamInfo.m_pOldestReadRequestHandle = static_cast<SGeomCacheReadRequestHandle*>(pOldestHandle->m_pNext);
	}

	assert(pOldestHandle);
	if (pOldestHandle)
	{
		if (pOldestHandle->m_numJobReferences > 0)
		{
			CryFatalError("Trying to retire handle with non zero job count");
		}

		RetireBufferHandle(pOldestHandle);
	}
}

void CGeomCacheManager::RetireOldestDecompressHandle(SGeomCacheStreamInfo& streamInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	SGeomCacheBufferHandle* pOldestHandle = NULL;

	// Unlink from stream linked list
	{
		pOldestHandle = streamInfo.m_pOldestDecompressHandle;

		assert(pOldestHandle);
		if (!pOldestHandle)
		{
			return;
		}

		if (pOldestHandle == streamInfo.m_pNewestDecompressHandle)
		{
			assert(pOldestHandle->m_pNext == NULL);
			streamInfo.m_pOldestDecompressHandle = NULL;
			streamInfo.m_pNewestDecompressHandle = NULL;
		}
		else
		{
			streamInfo.m_pOldestDecompressHandle = pOldestHandle->m_pNext;
		}
	}

	if (pOldestHandle)
	{
		RetireDecompressHandle(streamInfo, pOldestHandle);
	}
}

void CGeomCacheManager::RetireDecompressHandle(SGeomCacheStreamInfo& streamInfo, SGeomCacheBufferHandle* pHandle)
{
	CGeomCache* pGeomCache = streamInfo.m_pGeomCache;

	const uint frameDataSize = streamInfo.m_frameData.size();
	uint reinitializeStart = pGeomCache->GetPrevIFrame(pHandle->m_startFrame) % frameDataSize;
	uint reinitializeEnd = pHandle->m_endFrame % frameDataSize;

	if (!pHandle->m_pNext && (reinitializeEnd != (frameDataSize - 1)))
	{
		// If this was the last handle in stream or the next handle start frame is not right after the current handles end frame
		// we need to reinitialize until the next index frame because index frame jobs will decrement dependency
		// counters from b frames ahead of time.

		reinitializeEnd = pGeomCache->GetNextIFrame(reinitializeEnd) + 1;
	}

	ReinitializeStreamFrameData(streamInfo, reinitializeStart, reinitializeEnd);
	RetireBufferHandle(pHandle);
}

template<class TBufferHandleType> void CGeomCacheManager::RetireBufferHandle(TBufferHandleType* pHandle)
{
	FUNCTION_PROFILER_3DENGINE;

	if (gEnv->mMainThreadId != CryGetCurrentThreadId())
	{
		CryFatalError("CGeomCacheManager::RetireBufferHandle must be called from main thread");
	}

	if (pHandle->m_numJobReferences)
	{
		CryFatalError("Trying to retire handle with jobs still running");
	}
	
	size_t pointerSize = (sizeof(SGeomCacheBufferHandle*) + 15) & ~15;
	m_pPool->Free(pHandle->m_pBuffer - pointerSize);
	delete pHandle;
}

float CGeomCacheManager::GetPrecachedTime(const IGeomCacheRenderNode* pRenderNode)
{
	assert(gEnv->mMainThreadId == CryGetCurrentThreadId());

	for (uint i = 0; i < m_streamInfos.size(); ++i)
	{
		SGeomCacheStreamInfo& streamInfo = *m_streamInfos[i];
		if (streamInfo.m_pRenderNode == pRenderNode)
		{
			const float playbackTime = pRenderNode->GetPlaybackTime();

			if (streamInfo.m_pNewestDecompressHandle)
			{
				CGeomCache* pGeomCache = streamInfo.m_pGeomCache;

				const uint frame = streamInfo.m_pNewestDecompressHandle->m_startFrame;
				const float frameTime = pGeomCache->GetFrameTime(frame);

				if (playbackTime <= frameTime)
				{
					return frameTime - playbackTime;
				}
			}

			break;
		}
	}

	return 0.0f;
}

SGeomCacheBufferHandle* CGeomCacheManager::GetFrameDecompressHandle(SGeomCacheStreamInfo* pStreamInfo, const uint frameIndex)
{
	const uint frameDataSize = pStreamInfo->m_frameData.size();
	return pStreamInfo->m_frameData[frameIndex % frameDataSize].m_pDecompressHandle;
}

SGeomCacheFrameHeader* CGeomCacheManager::GetFrameDecompressHeader(SGeomCacheStreamInfo* pStreamInfo, const uint frameIndex)
{
	SGeomCacheBufferHandle* pHandle = GetFrameDecompressHandle(pStreamInfo, frameIndex);

	if (!pHandle)
	{
		return NULL;
	}

	const uint frameOffset = frameIndex - pHandle->m_startFrame;
	SGeomCacheFrameHeader* pHeader = reinterpret_cast<SGeomCacheFrameHeader*>(
	  pHandle->m_pBuffer + (frameOffset * sizeof(SGeomCacheFrameHeader)));
	return pHeader;
}

char* CGeomCacheManager::GetFrameDecompressData(SGeomCacheStreamInfo* pStreamInfo, const uint frameIndex)
{
	SGeomCacheFrameHeader* pHeader = GetFrameDecompressHeader(pStreamInfo, frameIndex);
	SGeomCacheBufferHandle* pHandle = GetFrameDecompressHandle(pStreamInfo, frameIndex);

	if (!pHandle || !pHeader)
	{
		return NULL;
	}

	return pHandle->m_pBuffer + pHeader->m_offset;
}

int* CGeomCacheManager::GetDependencyCounter(SGeomCacheStreamInfo* pStreamInfo, const uint frameIndex)
{
	const uint frameDataSize = pStreamInfo->m_frameData.size();
	return &pStreamInfo->m_frameData[frameIndex % frameDataSize].m_decodeDependencyCounter;
}

	#ifndef _RELEASE
namespace
{
void Draw2DBox(float x, float y, float width, float height, const ColorB& color, float screenHeight, float screenWidth, IRenderAuxGeom* pAuxRenderer)
{
	float position[4][2] = {
		{ x - 1.0f,         y - 1.0f          },
		{ x - 1.0f,         y + height + 1.0f },
		{ x + width + 1.0f, y + height + 1.0f },
		{ x + width + 1.0f, y - 1.0f          }
	};

	Vec3 positions[4] = {
		Vec3(position[0][0] / screenWidth, position[0][1] / screenHeight, 0.0f),
		Vec3(position[1][0] / screenWidth, position[1][1] / screenHeight, 0.0f),
		Vec3(position[2][0] / screenWidth, position[2][1] / screenHeight, 0.0f),
		Vec3(position[3][0] / screenWidth, position[3][1] / screenHeight, 0.0f)
	};

	vtx_idx const indices[6] = { 0, 1, 2, 0, 2, 3 };

	pAuxRenderer->DrawTriangles(positions, 4, indices, 6, color);
}

void Draw2DBoxOutLine(float x, float y, float width, float height, const ColorB& color, float screenHeight, float screenWidth, IRenderAuxGeom* pAuxRenderer)
{
	float position[4][2] = {
		{ x - 1.0f,         y - 1.0f          },
		{ x - 1.0f,         y + height + 1.0f },
		{ x + width + 1.0f, y + height + 1.0f },
		{ x + width + 1.0f, y - 1.0f          }
	};

	Vec3 positions[4] = {
		Vec3(position[0][0] / screenWidth, position[0][1] / screenHeight, 0.0f),
		Vec3(position[1][0] / screenWidth, position[1][1] / screenHeight, 0.0f),
		Vec3(position[2][0] / screenWidth, position[2][1] / screenHeight, 0.0f),
		Vec3(position[3][0] / screenWidth, position[3][1] / screenHeight, 0.0f)
	};

	pAuxRenderer->DrawLine(positions[0], color, positions[1], color);
	pAuxRenderer->DrawLine(positions[1], color, positions[2], color);
	pAuxRenderer->DrawLine(positions[2], color, positions[3], color);
	pAuxRenderer->DrawLine(positions[3], color, positions[0], color);
}

void DrawStream(const char* pBase, const size_t poolSize, const SGeomCacheBufferHandle* pFirstHandle, const ColorF& color, const float boxLeft, const float boxTop,
                const float boxWidth, const float boxHeight, const float screenWidth, const float screenHeight, IRenderAuxGeom* pRenderAuxGeom)
{
	const float bufferSize = (float)poolSize;
	for (const SGeomCacheBufferHandle* pCurrentHandle = pFirstHandle; pCurrentHandle; pCurrentHandle = pCurrentHandle->m_pNext)
	{
		ColorF blockColor = color;

		const float offset = (float)(pCurrentHandle->m_pBuffer - pBase);
		const float size = (float)pCurrentHandle->m_bufferSize;

		const float left = boxWidth * (offset / bufferSize);
		const float width = boxWidth * (size / bufferSize);

		Draw2DBox(boxLeft + left + 1, boxTop + 1, width - 3, boxHeight - 2, blockColor, screenHeight, screenWidth, pRenderAuxGeom);
	}
}
}

void CGeomCacheManager::DrawDebugInfo()
{
	IRenderAuxGeom* const pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	const SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();

	SAuxGeomRenderFlags flags(e_Def2DPublicRenderflags);
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetDepthWriteFlag(e_DepthWriteOff);
	flags.SetCullMode(e_CullModeNone);
	flags.SetAlphaBlendMode(e_AlphaNone);
	pRenderAuxGeom->SetRenderFlags(flags);

	const float screenWidth  = float(pRenderAuxGeom->GetCamera().GetViewSurfaceX());
	const float screenHeight = float(pRenderAuxGeom->GetCamera().GetViewSurfaceZ());

	const float topOffset = screenHeight * 0.01f;
	const float sideOffset = screenWidth * 0.01f;

	const float bufferBoxTop = 2.0f + 8.0f * topOffset;
	const float bufferBoxHeight = screenHeight * 0.05f;
	const float bufferBoxLeft = sideOffset;
	const float bufferBoxWidth = screenWidth * 0.5f;

	float streamInfosTop = bufferBoxTop + bufferBoxHeight + 2.0f * topOffset;
	const float streamInfoSpacing = 20.0f;
	const float streamInfoBoxSize = 10.0f;

	uint numActiveStreams = 0;
	uint numFramesMissed = 0;

	const uint kNumColors = 8;
	ColorF colors[kNumColors] = { Col_Red, Col_Green, Col_Yellow, Col_Blue, Col_Aquamarine, Col_Thistle, Col_Tan, Col_Salmon };

	uint colorIndex = 0;
	const uint numStreams = m_streamInfos.size();
	for (uint i = 0; i < numStreams; ++i)
	{
		SGeomCacheStreamInfo& streamInfo = *m_streamInfos[i];
		CGeomCacheRenderNode* pRenderNode = streamInfo.m_pRenderNode;
		CGeomCache* pGeomCache = streamInfo.m_pGeomCache;
		const char* pName = streamInfo.m_pRenderNode->GetName();
		const char* pFilter = GetCVars()->e_GeomCacheDebugFilter->GetString();

		const bool bDisplay = ((GetCVars()->e_GeomCacheDebug != 2) || (pRenderNode->IsStreaming() || streamInfo.m_pOldestDecompressHandle || streamInfo.m_pNewestReadRequestHandle))
		                      && strstr(pName, GetCVars()->e_GeomCacheDebugFilter->GetString()) != NULL;

		if (bDisplay)
		{
			ColorF& color = colors[colorIndex % kNumColors];

			DrawStream(reinterpret_cast<char*>(m_pPoolBaseAddress), m_poolSize, streamInfo.m_pOldestDecompressHandle, color, bufferBoxLeft, bufferBoxTop,
			           bufferBoxWidth, bufferBoxHeight, screenWidth, screenHeight, pRenderAuxGeom);
			DrawStream(reinterpret_cast<char*>(m_pPoolBaseAddress), m_poolSize, streamInfo.m_pDecompressAbortListHead, color, bufferBoxLeft, bufferBoxTop,
			           bufferBoxWidth, bufferBoxHeight, screenWidth, screenHeight, pRenderAuxGeom);
			DrawStream(reinterpret_cast<char*>(m_pPoolBaseAddress), m_poolSize, streamInfo.m_pOldestReadRequestHandle, color, bufferBoxLeft, bufferBoxTop,
			           bufferBoxWidth, bufferBoxHeight, screenWidth, screenHeight, pRenderAuxGeom);
			DrawStream(reinterpret_cast<char*>(m_pPoolBaseAddress), m_poolSize, streamInfo.m_pReadAbortListHead, color, bufferBoxLeft, bufferBoxTop,
			           bufferBoxWidth, bufferBoxHeight, screenWidth, screenHeight, pRenderAuxGeom);

			const float currentTop = streamInfosTop + streamInfoSpacing * 2.5f * numActiveStreams;
			Draw2DBox(sideOffset, currentTop, streamInfoBoxSize, streamInfoBoxSize, color, screenHeight, screenWidth, pRenderAuxGeom);

			const float wantedPlaybackTime = streamInfo.m_wantedPlaybackTime;
			const uint wantedFloorFrame = streamInfo.m_wantedFloorFrame;
			const uint wantedCeilFrame = streamInfo.m_wantedCeilFrame;
			const int oldestDiskFrame = streamInfo.m_pOldestReadRequestHandle ? streamInfo.m_pOldestReadRequestHandle->m_startFrame : -1;
			const int newestDiskFrame = streamInfo.m_pNewestReadRequestHandle ? streamInfo.m_pNewestReadRequestHandle->m_endFrame : -1;
			const int oldestDecompressFrame = streamInfo.m_pOldestDecompressHandle ? streamInfo.m_pOldestDecompressHandle->m_startFrame : -1;
			const int newestDecompressFrame = streamInfo.m_pNewestDecompressHandle ? streamInfo.m_pNewestDecompressHandle->m_endFrame : -1;

			IGeomCache::SStatistics stats = pGeomCache->GetStatistics();

			const char* pCompressionMethod = "Store";
			if (pGeomCache->GetBlockCompressionFormat() == GeomCacheFile::eBlockCompressionFormat_Deflate)
			{
				pCompressionMethod = "Deflate";
			}
			else if (pGeomCache->GetBlockCompressionFormat() == GeomCacheFile::eBlockCompressionFormat_LZ4HC)
			{
				pCompressionMethod = "LZ4 HC";
			}

			IRenderAuxText::Draw2dLabel(sideOffset + streamInfoSpacing, currentTop - 5.0f, 1.5f, Col_White, false, "%s - %.3gs %s- %.3g MiB/s - %s - %d frames missed",
			                             streamInfo.m_pRenderNode->GetName(), pGeomCache->GetDuration(), streamInfo.m_bLooping ? "looping " : "", stats.m_averageAnimationDataRate, pCompressionMethod, streamInfo.m_numFramesMissed);
			IRenderAuxText::Draw2dLabel(sideOffset + streamInfoSpacing, streamInfoSpacing + currentTop - 5.0f, 1.5f, Col_White, false,
			                             "Frame: [%04u, %04u], Disk Frames: [%04u, %04u], Decompress Frames: [%04u, %04u], Playback time: %g", wantedFloorFrame, wantedCeilFrame,
			                             oldestDiskFrame, newestDiskFrame, oldestDecompressFrame, newestDecompressFrame, wantedPlaybackTime);

			++numActiveStreams;
			++colorIndex;
		}
	}

	const uint numAbortedStreams = m_streamInfosAbortList.size();
	IRenderAuxText::Draw2dLabel(sideOffset, topOffset, 1.5f, Col_Yellow, false, "%u Geometry Cache(s) active", numActiveStreams);
	IRenderAuxText::Draw2dLabel(sideOffset, 3.5f * topOffset, 1.5f, (m_numMissedFrames > 0) ? Col_Red : Col_Green, false, "%u Frames missed", m_numMissedFrames);
	IRenderAuxText::Draw2dLabel(sideOffset + 160.0f, 3.5f * topOffset, 1.5f, (m_numStreamAborts > 0) ? Col_Red : Col_Green, false, "%u Stream aborts (err: %u, decomp: %u, read: %u)",
	                             m_numStreamAborts, m_numErrorAborts, m_numDecompressStreamAborts, m_numReadStreamAborts);
	IRenderAuxText::Draw2dLabel(sideOffset + 520.0f, 3.5f * topOffset, 1.5f, (m_numFailedAllocs > 0) ? Col_Yellow : Col_Green, false, "%u Failed alloc(s)", m_numFailedAllocs);
	IRenderAuxText::Draw2dLabel(sideOffset + 670.0f, 3.5f * topOffset, 1.5f, (numAbortedStreams > 0) ? Col_Yellow : Col_Green, false, "%u Aborted stream(s)", numAbortedStreams);

	IRenderAuxText::Draw2dLabel(sideOffset, 6.0f * topOffset, 1.25f, Col_White, false, "Geom Cache Buffer:");
	Draw2DBoxOutLine(bufferBoxLeft, bufferBoxTop, bufferBoxWidth, bufferBoxHeight, Col_White, screenHeight, screenWidth, pRenderAuxGeom);

	pRenderAuxGeom->SetRenderFlags(oldFlags);
}
	#endif

#endif
