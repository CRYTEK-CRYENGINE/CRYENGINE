// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   StreamIOThread.cpp
//  Created:     22/07/2010 by Timur.
//  Description: Streaming Thread for IO
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "StreamIOThread.h"
#include "StreamEngine.h"
#include "../System.h"
#include <CryCore/Platform/IPlatformOS.h>

extern SSystemCVars g_cvars;

//#pragma("control %push O=0")             // to disable optimization

//////////////////////////////////////////////////////////////////////////
CStreamingIOThread::CStreamingIOThread(CStreamEngine* pStreamEngine, EStreamSourceMediaType mediaType, const char* name)
{
	m_pStreamEngine = pStreamEngine;
	m_bCancelThreadRequest = false;
	m_bNeedSorting = false;
	m_bNeedReset = false;
	m_bNewRequests = false;
	m_name = name;
	m_eMediaType = mediaType;
	m_nFallbackMTs = 0;

	m_iUrgentRequests = 0;

	m_bPaused = false;
	m_bAbortReads = false;

	m_nReadCounter = 0;

	if (!gEnv->pThreadManager->SpawnThread(this, name))
	{
		CryFatalError("Error spawning \"%s\" thread.", name);
	}
}

CStreamingIOThread::~CStreamingIOThread()
{
	SignalStopWork();
	gEnv->pThreadManager->JoinThread(this, eJM_Join);
}

//////////////////////////////////////////////////////////////////////////
void CStreamingIOThread::AddRequest(CAsyncIOFileRequest* pRequest, bool bStartImmidietly)
{
	pRequest->AddRef(); // Acquire ownership on file request.
	pRequest->m_status = CAsyncIOFileRequest::eStatusInFileQueue;
	if (pRequest->m_eMediaType != eStreamSourceTypeMemory)
		pRequest->m_eMediaType = m_eMediaType;
	// does this ignore the tmp out of memory
	if (pRequest->IgnoreOutofTmpMem())
	{
		CryInterlockedIncrement(&m_iUrgentRequests);
	}
	m_newFileRequests.push_back(pRequest);

	if (bStartImmidietly)
	{
		READ_WRITE_BARRIER
		  m_bNewRequests = true;
		m_awakeEvent.Set();
	}
}

//////////////////////////////////////////////////////////////////////////
void CStreamingIOThread::SignalStartWork(bool bForce)
{
	if (!m_newFileRequests.empty() || bForce)
	{
		READ_WRITE_BARRIER
		  m_bNewRequests = true;

		m_awakeEvent.Set();
	}
}

//////////////////////////////////////////////////////////////////////////

void CStreamingIOThread::Pause(bool bPause)
{
	m_bPaused = bPause;
}

//////////////////////////////////////////////////////////////////////////
void CStreamingIOThread::ThreadEntry()
{
	CTimeValue t0 = gEnv->pTimer->GetAsyncTime();

	m_nLastReadDiskOffset = 0;

	//
	// Main thread loop
	while (!m_bCancelThreadRequest)
	{
		if (m_bNewRequests || !m_newFileRequests.empty())
		{
			CRY_PROFILE_REGION(PROFILE_SYSTEM, "StreamIO - Process New Request");

			READ_WRITE_BARRIER
			  ProcessNewRequests();
		}
		else
		{
			CRY_PROFILE_REGION_WAITING(PROFILE_SYSTEM, "Wait - StreamIO New Request");

			if (m_eMediaType == eStreamSourceTypeDisc && gEnv->pSystem->GetPlatformOS())
			{
				gEnv->pSystem->GetPlatformOS()->SetOpticalDriveIdle(true);
			}

#if defined(_RELEASE)
			m_awakeEvent.Wait();
#elif defined(STREAMENGINE_ENABLE_STATS)
			// compute max time to wait - revive thread every second at least once to update stats
			bool bWaiting = true;
			while (bWaiting)
			{
				CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
				CTimeValue deltaT = t1 - t0;
				uint64 msec = deltaT.GetMilliSecondsAsInt64();
				if (msec < 1000)
				{
					bWaiting = !m_awakeEvent.Wait(1000 - (uint32)msec);
				}

				if (bWaiting)
				{
					// update the delta time again
					t1 = gEnv->pTimer->GetAsyncTime();
					deltaT = t1 - t0;

					m_InMemoryStats.Update(deltaT);
					m_NotInMemoryStats.Update(deltaT);

					t0 = t1;
				}
			}
#endif

			if (m_eMediaType == eStreamSourceTypeDisc && gEnv->pSystem->GetPlatformOS())
			{
				gEnv->pSystem->GetPlatformOS()->SetOpticalDriveIdle(false);
			}
		}

		if (m_bNeedReset)
		{
			CRY_PROFILE_REGION(PROFILE_SYSTEM, "StreamIO Process Reset");

			ProcessReset();
		}

		bool bIsOOM = false;

		while (!m_bCancelThreadRequest && !m_fileRequestQueue.empty())
		{
			CAsyncIOFileRequest_TransferPtr pFileRequest(m_fileRequestQueue.back());
			m_fileRequestQueue.pop_back();

			assert(&*pFileRequest);

			if (pFileRequest->HasFailed())
			{
				CRY_PROFILE_REGION(PROFILE_SYSTEM, "StreamIO Process High Prio Task");

				// check if request was high prio, then decrease open count
				if (pFileRequest->IgnoreOutofTmpMem())
					CryInterlockedDecrement(&m_iUrgentRequests);

				CAsyncIOFileRequest::JobFinalize_Read(pFileRequest, m_pStreamEngine->GetJobEngineState());

				continue;
			}

			//////////////////////////////////////////////////////////////////////////
			// When temporary memory goes out of budget we must loop here and wait until previous file requests are finished and free up memory.
			// Only allow processing of requests which are flagged for processing when out of tmp memory
			//////////////////////////////////////////////////////////////////////////
			if (bIsOOM && !m_bCancelThreadRequest)
			{
				CRY_PROFILE_REGION_WAITING(PROFILE_SYSTEM, "Wait - StreamIO Memory out of budget");

				m_pStreamEngine->FlagTempMemOutOfBudget();
				if (m_iUrgentRequests > 0)
				{
					if (m_bNewRequests || !m_newFileRequests.empty())
					{
						READ_WRITE_BARRIER
						  ProcessNewRequests();
					}

					// read the current request
					m_fileRequestQueue.push_back(pFileRequest.Relinquish());

					// search for the first request which ignores the current out of mem state
					// Search for next highest priority request
					std::vector<CAsyncIOFileRequest*>::reverse_iterator rit;
					for (rit = m_fileRequestQueue.rbegin(); rit != m_fileRequestQueue.rend(); ++rit)
					{
						if ((*rit)->IgnoreOutofTmpMem())
						{
							pFileRequest = *rit;
							std::vector<CAsyncIOFileRequest*>::iterator it(rit.base());
							--it;
							m_fileRequestQueue.erase(it);
							break;
						}
					}
				}
				else
				{
					// read the current request
					m_fileRequestQueue.push_back(pFileRequest.Relinquish());
				}
			}

			// Simply let the io thread sleep when paused before doing any actual IO
			{
				CRY_PROFILE_REGION_WAITING(PROFILE_SYSTEM, "Wait - StreamIO Paused Sleep");

				while (m_bPaused)
				{
					CrySleep(10);
				}
			}

			// If at this point, the file request is zero, the above prioritization of
			// urgent requests couldn't find a new task to displace the current
			// one. As the current one had been pushed back previously, we can safely
			// assume that restarting the loop will grab it again (eventually).
			if (!pFileRequest)
			{
				break;
			}

			uint32 nSizeOnMedia = 0;

			{
				CRY_PROFILE_REGION(PROFILE_SYSTEM, "StreamIO Process High Prio Task Handling");

				// check if request was high prio, then decrease open count
				if (pFileRequest->IgnoreOutofTmpMem())
				{
					CryInterlockedDecrement(&m_iUrgentRequests);
				}

				bIsOOM = false;

				nSizeOnMedia = pFileRequest->m_nSizeOnMedia;
				uint32 nError = 0;

				// Handle file request.
				if (m_bAbortReads)
					nError = ERROR_ABORTED_ON_SHUTDOWN;
				else if (pFileRequest->m_bReadBegun)
					nError = pFileRequest->ReadFileResume(this);
				else
					nError = pFileRequest->ReadFile(this);

#ifdef STREAMENGINE_ENABLE_STATS
				pFileRequest->m_nReadCounter = m_nReadCounter++;
#endif

				if (nError == 0)
				{
					if (pFileRequest->m_eMediaType != eStreamSourceTypeMemory)
					{
						pFileRequest->m_nReadHeadOffsetKB = (int32)(((int64)pFileRequest->m_nDiskOffset - m_nLastReadDiskOffset) >> 10); // in KB
						m_nLastReadDiskOffset = pFileRequest->m_nDiskOffset + pFileRequest->m_nSizeOnMedia;

#ifdef STREAMENGINE_ENABLE_STATS
						m_NotInMemoryStats.m_nTempReadOffset += abs(pFileRequest->m_nReadHeadOffsetKB);
						m_NotInMemoryStats.m_nTotalReadOffset += abs(pFileRequest->m_nReadHeadOffsetKB);

						m_NotInMemoryStats.m_nTempRequestCount++;

						// Calc IO bandwidth only for non memory files.
						m_NotInMemoryStats.m_nTempBytesRead += pFileRequest->m_nSizeOnMedia;
						m_NotInMemoryStats.m_TempReadTime += pFileRequest->m_readTime;
#endif
					}
					else
					{
#ifdef STREAMENGINE_ENABLE_STATS
						m_InMemoryStats.m_nTempRequestCount++;

						// Calc IO bandwidth only for in memory files.
						m_InMemoryStats.m_nTempBytesRead += pFileRequest->m_nSizeOnMedia;
						m_InMemoryStats.m_TempReadTime += pFileRequest->m_readTime;
#endif
					}

					CAsyncIOFileRequest::JobFinalize_Read(pFileRequest, m_pStreamEngine->GetJobEngineState());
				}
				else
				{
					switch (nError)
					{
					case ERROR_OUT_OF_MEMORY:
						bIsOOM = true;

						pFileRequest->SetPriority(estpPreempted);

						if (pFileRequest->IgnoreOutofTmpMem())
							CryInterlockedIncrement(&m_iUrgentRequests);

						m_fileRequestQueue.push_back(pFileRequest.Relinquish());
						m_bNewRequests = true;
						break;

					case ERROR_PREEMPTED:
						pFileRequest->SetPriority(estpPreempted);

						if (pFileRequest->IgnoreOutofTmpMem())
							CryInterlockedIncrement(&m_iUrgentRequests);

						m_fileRequestQueue.push_back(pFileRequest.Relinquish());
						m_bNewRequests = true;
						break;

					case ERROR_MISSCHEDULED:
						// Request tried to read a file that has changed media type. Reset the sort key
						// and reschedule.
						pFileRequest->m_bSortKeyComputed = 0;
						AddRequest(&*pFileRequest, false);
						break;

					default:
						pFileRequest->SyncWithDecrypt();
						pFileRequest->SyncWithDecompress();
						pFileRequest->Failed(nError);

						CAsyncIOFileRequest::JobFinalize_Read(pFileRequest, m_pStreamEngine->GetJobEngineState());
						break;
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			if (m_bNewRequests)
			{
				READ_WRITE_BARRIER
				  CRY_PROFILE_REGION(PROFILE_SYSTEM, "StreamIO Process New Request");

				ProcessNewRequests();
			}
			if (m_bNeedReset)
			{
				CRY_PROFILE_REGION(PROFILE_SYSTEM, "StreamIO Process Reset");

				ProcessReset();
			}
			if (m_bNeedSorting)
			{
				CRY_PROFILE_REGION(PROFILE_SYSTEM, "StreamIO Sort Requests");

				SortRequests();
			}

			//////////////////////////////////////////////////////////////////////////
#ifdef STREAMENGINE_ENABLE_STATS
			{
				CRY_PROFILE_REGION(PROFILE_SYSTEM, "StreamIO Updating Stats");

				if (g_cvars.sys_streaming_max_bandwidth != 0)
				{
					CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
					CTimeValue deltaT = t1 - t0;

					// Sleep in case we are streaming too fast.
					const float fTheoreticalReadTime = float(nSizeOnMedia) / g_cvars.sys_streaming_max_bandwidth * 0.00000095367431640625f; // / (1024*1024)

					if (fTheoreticalReadTime - deltaT.GetSeconds() > FLT_EPSILON)
					{
						uint32 nSleepTime = uint32(1000.f * (fTheoreticalReadTime - deltaT.GetSeconds()));
						CrySleep(nSleepTime);
					}
				}

				CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
				CTimeValue deltaT = t1 - t0;

				// update the stats every second
				if (deltaT.GetMilliSecondsAsInt64() > 1000)
				{
					m_InMemoryStats.Update(deltaT);
					m_NotInMemoryStats.Update(deltaT);

					t0 = t1;
				}
			}
#endif
		}
	}
}

#ifdef STREAMENGINE_ENABLE_STATS
void CStreamingIOThread::SStats::Update(const CTimeValue& deltaT)
{
	m_nReadBytesInLastSecond = (uint32)m_nTempBytesRead;
	m_nRequestCountInLastSecond = m_nTempRequestCount;
	m_nTotalReadBytes += (uint32)m_nTempBytesRead;
	m_nTotalRequestCount += m_nTempRequestCount;
	m_TotalReadTime += m_TempReadTime;

	if (m_TempReadTime.GetValue() != 0)
		m_nActualReadBandwith = (uint32)(m_nTempBytesRead / m_TempReadTime.GetSeconds());
	else
		m_nActualReadBandwith = 0;
	m_nCurrentReadBandwith = (uint32)(m_nTempBytesRead / deltaT.GetSeconds());
	m_fReadingDuringLastSecond = m_TempReadTime.GetSeconds() / deltaT.GetSeconds() * 100;

	if (m_nTempRequestCount > 0)
		m_nReadOffsetInLastSecond = m_nTempReadOffset / m_nTempRequestCount;
	else
		m_nReadOffsetInLastSecond = 0;

	m_TempReadTime.SetValue(0);
	m_nTempBytesRead = 0;
	m_nTempReadOffset = 0;
	m_nTempRequestCount = 0;
}
#endif

//////////////////////////////////////////////////////////////////////////
void CStreamingIOThread::SignalStopWork()
{
	m_bCancelThreadRequest = true;
	m_awakeEvent.Set();
}

//////////////////////////////////////////////////////////////////////////
struct SCompareAsyncFileRequest
{
	bool operator()(CAsyncIOFileRequest* pFile1, CAsyncIOFileRequest* pFile2) const
	{
		return pFile1->m_nSortKey > pFile2->m_nSortKey;
	}
};

//////////////////////////////////////////////////////////////////////////
void CStreamingIOThread::SortRequests()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	std::sort(m_fileRequestQueue.begin(), m_fileRequestQueue.end(), SCompareAsyncFileRequest());

	/*
	   int nStartOfQueue = 0;
	   int64 nDiskOffsetLimit = m_nLastReadDiskOffset - 32*1024; // 32KB less only

	   int nCount = (int)m_fileRequestQueue.size();
	   for (int i = nCount-1; i >= 0; i--)
	   {
	    if (m_fileRequestQueue[i]->m_nDiskOffset > nDiskOffsetLimit)
	    {
	      nStartOfQueue = i+1;
	      break;
	    }
	   }
	   if (nStartOfQueue < nCount && nStartOfQueue > 0)
	   {
	    int nElements = nCount - nStartOfQueue;
	    // Move all elements up to nStartOfQueue, from begining of the request array to the end.
	    m_temporaryArray.resize(0);
	    // Copy to temp array elements up to nStartOfQueue
	    m_temporaryArray.insert( m_temporaryArray.end(),m_fileRequestQueue.begin()+nStartOfQueue,m_fileRequestQueue.end() );
	    // Remove elements up to nStartOfQueue from request list
	    m_fileRequestQueue.erase( m_fileRequestQueue.begin()+nStartOfQueue,m_fileRequestQueue.end() );
	    // Add elemenets at the end from temp array.
	    m_fileRequestQueue.insert( m_fileRequestQueue.begin(),m_temporaryArray.begin(),m_temporaryArray.end() );
	   }
	 */
	m_bNeedSorting = false;
}

void CStreamingIOThread::NeedSorting()
{
	m_bNeedSorting = true;
}

//////////////////////////////////////////////////////////////////////////
void CStreamingIOThread::ProcessNewRequests()
{
	m_bNewRequests = false;

	std::vector<CAsyncIOFileRequest*> temporaryArray;
	temporaryArray.reserve(m_newFileRequests.size());
	m_newFileRequests.swap(temporaryArray);

	std::vector<CAsyncIOFileRequest*>& newFiles = temporaryArray;

	if (!newFiles.empty())
	{
		uint64 nCurrentKeyInProgress = m_fileRequestQueue.size() ? m_fileRequestQueue.back()->m_nSortKey : 0;

		// Compute sorting key for new file entries.
		int iWakeFallback(0);
		const TFallbackIOVecConstIt itEnd = m_FallbackIOThreads.end();
		const size_t fallbackNum = m_FallbackIOThreads.size();
		PREFAST_SUPPRESS_WARNING(6255)
		uint8 * pFallbackSignals = fallbackNum ? (uint8*)alloca(fallbackNum) : NULL;
		for (uint32 fb = 0; fb < fallbackNum; ++fb)
			pFallbackSignals[fb] = 0;

		for (size_t i = 0, num = newFiles.size(); i < num; i++)
		{
			CAsyncIOFileRequest* pFilepRequest = newFiles[i];

			pFilepRequest->ComputeSortKey(nCurrentKeyInProgress);
			static_cast<CReadStream*>(&*pFilepRequest->m_pReadStream)->ComputedMediaType(pFilepRequest->m_eMediaType);

#ifdef STREAMENGINE_ENABLE_LISTENER
			IStreamEngineListener* pListener = m_pStreamEngine->GetListener();
			if (pListener)
				pListener->OnStreamComputedSortKey(pFilepRequest, pFilepRequest->m_nSortKey);
#endif

			bool bFallback = false;
			int idx = -1;
			for (TFallbackIOVecConstIt it = m_FallbackIOThreads.begin(); it != itEnd && !bFallback; ++it)
			{
				++idx;
				if (it->second == pFilepRequest->GetMediaType())
				{
					if (pFilepRequest->IgnoreOutofTmpMem())
					{
						CryInterlockedDecrement(&m_iUrgentRequests);
					}
					(it->first)->AddRequest(pFilepRequest, true);
					pFilepRequest->Release(); // Release local ownership of request (moved to fallback IO thread)
					iWakeFallback++;
					bFallback = true;
					pFallbackSignals[idx] = 1;
				}
			}
			if (!bFallback)
				m_fileRequestQueue.push_back(pFilepRequest);
		}

		for (uint32 fb = 0; fb < fallbackNum; ++fb)
		{
			if (pFallbackSignals[fb] != 0)
				(m_FallbackIOThreads[fb].first)->SignalStartWork(false);
		}

		SortRequests();
		/*
		   if (m_fileRequestQueue.back() != pRequest && pRequest != 0)
		   {
		   // Highest priority changed.
		   if (m_fileRequestQueue.back()->m_nDiskOffset < (m_nLastReadDiskOffset-32*1024))
		   {
		    //CryLog( "Bad Offset in Queue" );
		   }
		   }
		 */
	}
}

void CStreamingIOThread::ProcessReset()
{
	if (!m_fileRequestQueue.empty())
	{
		for (std::vector<CAsyncIOFileRequest*>::iterator it = m_fileRequestQueue.begin(), itEnd = m_fileRequestQueue.end(); it != itEnd; ++it)
			(*it)->Release();
	}

	stl::free_container(m_fileRequestQueue);

	if (!m_temporaryArray.empty())
	{
		for (std::vector<CAsyncIOFileRequest*>::iterator it = m_temporaryArray.begin(), itEnd = m_temporaryArray.end(); it != itEnd; ++it)
			(*it)->Release();
	}

	stl::free_container(m_temporaryArray);

	m_bNeedReset = false;
	m_resetDoneEvent.Set();
}

//////////////////////////////////////////////////////////////////////////
void CStreamingIOThread::CancelAll()
{
	{
		CryMT::vector<CAsyncIOFileRequest*>::AutoLock lock(m_newFileRequests.get_lock());

		if (!m_newFileRequests.empty())
		{
			CAsyncIOFileRequest* const* it = &m_newFileRequests.front();
			CAsyncIOFileRequest* const* itEnd = it + m_newFileRequests.size();
			for (; it != itEnd; ++it)
				(*it)->Release();
		}
	}

	m_newFileRequests.free_memory();
	m_iUrgentRequests = 0;
}

void CStreamingIOThread::AbortAll(bool bAbort)
{
	m_bAbortReads = bAbort;
}

void CStreamingIOThread::BeginReset()
{
	CancelAll();

	m_resetDoneEvent.Reset();
	m_bNeedReset = true;
	m_awakeEvent.Set();
}

void CStreamingIOThread::EndReset()
{
	m_resetDoneEvent.Wait();
}
//////////////////////////////////////////////////////////////////////////
void CStreamingIOThread::RegisterFallbackIOThread(EStreamSourceMediaType mediaType, CStreamingIOThread* pIOThread)
{
	//check if media has not yet been registered
	if (!pIOThread)
		return;//no need for NULL register anymore
	const TFallbackIOVecConstIt itEnd = m_FallbackIOThreads.end();
	for (TFallbackIOVecConstIt it = m_FallbackIOThreads.begin(); it != itEnd; ++it)
	{
		if (it->second == mediaType)
			return;
	}
	m_FallbackIOThreads.push_back(std::make_pair(pIOThread, mediaType));
	m_nFallbackMTs |= 1 << mediaType;
}

bool CStreamingIOThread::HasUrgentRequests()
{
	bool ret = false;

	if (m_iUrgentRequests > 0)
	{
		//lock to prevent list modification whilst traversing
		m_newFileRequests.get_lock().Lock();

		int nRequests = m_newFileRequests.size();

		if (nRequests)
		{
			for (int i = 0; i < nRequests; i++)
			{
				if (m_newFileRequests[i]->m_ePriority == estpUrgent)
				{
					//printf("Urgent task pending: %s\n", m_newFileRequests[i]->m_strFileName.c_str());
					ret = true;
					break;
				}
			}
		}
		m_newFileRequests.get_lock().Unlock();
	}
	return ret;
}

bool CStreamingIOThread::IsMisscheduled(EStreamSourceMediaType mt) const
{
	if (mt == m_eMediaType)
		return false;

	if (m_nFallbackMTs & (1 << mt))
		return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CStreamingWorkerThread::CStreamingWorkerThread(CStreamEngine* pStreamEngine, const char* name, EWorkerType type, SStreamRequestQueue* pQueue)
{
	m_type = type;
	m_name = name;
	m_pStreamEngine = pStreamEngine;
	m_pQueue = pQueue;
	m_bCancelThreadRequest = false;
	m_bNeedsReset = false;

	if (!gEnv->pThreadManager->SpawnThread(this, name))
	{
		CryFatalError("Error spawning \"%s\" thread.", name);
	}
}

CStreamingWorkerThread::~CStreamingWorkerThread()
{
	SignalStopWork();
	gEnv->pThreadManager->JoinThread(this, eJM_Join);
}

//////////////////////////////////////////////////////////////////////////
void CStreamingWorkerThread::ThreadEntry()
{
	// Main thread loop
	while (!m_bCancelThreadRequest)
	{
		m_pQueue->m_awakeEvent.Wait();
		m_pQueue->m_awakeEvent.Reset();

		CAsyncIOFileRequest_AutoPtr pFileRequest;
		while (!m_bCancelThreadRequest && !m_bNeedsReset && m_pQueue->TryPopRequest(pFileRequest))
		{
			switch (m_type)
			{
			case eWorkerAsyncCallback:
				{
					float fTime = gEnv->pTimer->GetAsyncCurTime();
					m_pStreamEngine->ReportAsyncFileRequestComplete(pFileRequest);
					float fTime1 = gEnv->pTimer->GetAsyncCurTime();

#ifdef STREAMENGINE_ENABLE_STATS
					CryInterlockedDecrement(&m_pStreamEngine->GetStreamingStatistics().nCurrentAsyncCount);
#endif

#ifndef _RELEASE
					if ((fTime1 - fTime) > 1.f && !pFileRequest->m_strFileName.empty())
					{
						string str;
						str.Format("[ACALL] %s time=%.5f\n", pFileRequest->m_strFileName.c_str(), (fTime1 - fTime));
						OutputDebugString(str.c_str());
					}
#endif
				}
				break;
			}
		}

		if (m_bNeedsReset)
		{
			m_pQueue->Reset();
			m_bNeedsReset = false;
			m_resetDoneEvent.Set();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CStreamingWorkerThread::SignalStopWork()
{
	m_bCancelThreadRequest = true;
	m_pQueue->m_awakeEvent.Set();
}

//////////////////////////////////////////////////////////////////////////
void CStreamingWorkerThread::CancelAll()
{
	m_pQueue->Reset();
}

void CStreamingWorkerThread::BeginReset()
{
	CancelAll();

	m_resetDoneEvent.Reset();
	m_bNeedsReset = true;
	m_pQueue->m_awakeEvent.Set();
}

void CStreamingWorkerThread::EndReset()
{
	m_resetDoneEvent.Wait();
}
