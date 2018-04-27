// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef LOADINGPROFILERSYSTEM_H
#define LOADINGPROFILERSYSTEM_H

#if defined(ENABLE_LOADING_PROFILER)

struct SLoadingTimeContainer;

struct SLoadingProfilerInfo
{
	string            name;
	double            selfTime;
	double            totalTime;
	uint32            callsTotal;
	double            memorySize;

	DiskOperationInfo selfInfo;
	DiskOperationInfo totalInfo;
};

class CLoadingProfilerSystem
{
public:
	static void                   Init();
	static void                   ShutDown();
	static void                   CreateNoStackList(PodArray<SLoadingTimeContainer>&);
	static void                   OutputLoadingTimeStats(ILog* pLog, int nMode);
	static SLoadingTimeContainer* StartLoadingSectionProfiling(CLoadingTimeProfiler* pProfiler, const char* szFuncName);
	static void                   EndLoadingSectionProfiling(CLoadingTimeProfiler* pProfiler);
	static const char*            GetLoadingProfilerCallstack();
	static void                   FillProfilersList(std::vector<SLoadingProfilerInfo>& profilers);
	static void                   FlushTimeContainers();
	static void                   SaveTimeContainersToFile(const char*, double fMinTotalTime, bool bClean);
	static void                   WriteTimeContainerToFile(SLoadingTimeContainer* p, FILE* f, unsigned int depth, double fMinTotalTime);
	static void                   UpdateSelfStatistics(SLoadingTimeContainer* p);
	static void                   Clean();
protected:
	static void                   AddTimeContainerFunction(PodArray<SLoadingTimeContainer>&, SLoadingTimeContainer*);
protected:
	static int                    nLoadingProfileMode;
	static int                    nLoadingProfilerNotTrackedAllocations;
	static CryCriticalSection     csLock;
	static int                    m_iMaxArraySize;
	static SLoadingTimeContainer* m_pCurrentLoadingTimeContainer;
	static SLoadingTimeContainer* m_pRoot[2];
	static int                    m_iActiveRoot;
	static ICVar*                 m_pEnableProfile;
};

#endif

#endif // LOADINGPROFILERSYSTEM_H
