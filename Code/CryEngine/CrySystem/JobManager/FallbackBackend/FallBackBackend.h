// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FallBackBackEnd.h
//  Version:     v1.00
//  Created:     07/05/2011 by Christopher Bolte
//  Compilers:   Visual Studio.NET
// -------------------------------------------------------------------------
//  History:
////////////////////////////////////////////////////////////////////////////

#ifndef FallBackBackEnd_H_
#define FallBackBackEnd_H_

#include <CryThreading/IJobManager.h>

namespace JobManager {
namespace FallBackBackEnd {

class CFallBackBackEnd : public IBackend
{
public:
	CFallBackBackEnd();
	~CFallBackBackEnd();

	bool   Init(uint32 nSysMaxWorker) { return true; }
	bool   ShutDown()                 { return true; }
	void   Update()                   {}

	void   AddJob(JobManager::CJobDelegator& crJob, const JobManager::TJobHandle cJobHandle, JobManager::SInfoBlock& rInfoBlock);

	uint32 GetNumWorkerThreads() const { return 0; }

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	virtual IWorkerBackEndProfiler* GetBackEndWorkerProfiler() const { return 0; }
#endif
};

} // namespace FallBackBackEnd
} // namespace JobManager

#endif // FallBackBackEnd_H_
