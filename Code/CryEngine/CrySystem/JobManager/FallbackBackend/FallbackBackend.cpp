// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FallbackBackend.h
//  Version:     v1.00
//  Created:     07/05/2011 by Christopher Bolte
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FallBackBackend.h"
#include "../JobManager.h"

JobManager::FallBackBackEnd::CFallBackBackEnd::CFallBackBackEnd()
{

}

JobManager::FallBackBackEnd::CFallBackBackEnd::~CFallBackBackEnd()
{

}

void JobManager::FallBackBackEnd::CFallBackBackEnd::AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock)
{
	CJobManager* __restrict pJobManager = CJobManager::Instance();

	// just execute the job in the calling context
	IF (rInfoBlock.HasQueue(), 0)
	{
		JobManager::SProdConsQueueBase* pQueue = (JobManager::SProdConsQueueBase*)rInfoBlock.GetQueue();
		JobManager::SJobSyncVariable* pQueueState = &pQueue->m_QueueRunningState;

		volatile INT_PTR* pQueuePull = alias_cast<volatile INT_PTR*>(&pQueue->m_pPull);
		volatile INT_PTR* pQueuePush = alias_cast<volatile INT_PTR*>(&pQueue->m_pPush);

		uint32 queueIncr = pQueue->m_PullIncrement;
		INT_PTR queueStart = pQueue->m_RingBufferStart;
		INT_PTR queueEnd = pQueue->m_RingBufferEnd;
		INT_PTR curPullPtr = *pQueuePull;

		// == process job packet == //
		void* pParamMem = (void*)curPullPtr;
		const uint32 cParamSize = (rInfoBlock.paramSize << 4);
		SAddPacketData* const __restrict pAddPacketData = (SAddPacketData*)((unsigned char*)curPullPtr + cParamSize);

		Invoker pInvoker = pJobManager->GetJobInvoker(pAddPacketData->nInvokerIndex);

		// call delegator function to invoke job entry
		(*pInvoker)(pParamMem);

		// mark job as finished
		IF (pAddPacketData->pJobState, 1)
		{
			pAddPacketData->pJobState->SetStopped();
		}

		// == update queue state == //
		const INT_PTR cNextPull = curPullPtr + queueIncr;
		curPullPtr = (cNextPull >= queueEnd) ? queueStart : cNextPull;

		// update pull ptr (safe since only change by a single job worker)
		*pQueuePull = curPullPtr;

		// mark queue as finished(safe since the fallback is not threaded)
		pQueueState->SetStopped();
	}
	else
	{
		Invoker delegator = crJob.GetGenericDelegator();
		const void* pParamMem = crJob.GetJobParamData();

		// execute job function
		if (crJob.GetLambda())
		{
			crJob.GetLambda()();
		}
		else
		{
			(*delegator)((void*)pParamMem);
		}

		IF (rInfoBlock.GetJobState(), 1)
		{
			SJobState* pJobState = rInfoBlock.GetJobState();
			pJobState->SetStopped();
		}
	}
}
