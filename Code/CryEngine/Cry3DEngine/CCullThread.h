// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryThreading/IJobManager.h>

struct SCheckOcclusionJobData;

namespace NAsyncCull
{
class CCullThread : public Cry3DEngineBase
{
	bool m_Enabled;
	bool m_Active;                                      // used to verify that the cull job is running and no new jobs are added after the job has finished

public:
	enum PrepareStateT { IDLE, PREPARE_STARTED, PREPARE_DONE, CHECK_REQUESTED, CHECK_STARTED };
	PrepareStateT         m_nPrepareState;
	CryCriticalSection    m_FollowUpLock;
	SRenderingPassInfo    m_passInfoForCheckOcclusion;
	uint32                m_nRunningReprojJobs;
	uint32                m_nRunningReprojJobsAfterMerge;
	int                   m_bCheckOcclusionRequested;
private:
	JobManager::SJobState m_JobStatePrepareOcclusionBuffer;
	JobManager::SJobState m_PrepareBufferSync;
	JobManager::SJobState m_checkOcclusion;
	Matrix44A             m_MatScreenViewProj;
	Matrix44A             m_MatScreenViewProjTransposed;
	Vec3                  m_ViewDir;
	Vec3                  m_Position;
	float                 m_NearPlane;
	float                 m_FarPlane;
	float                 m_NearestMax;

	PodArray<uint8>       m_OCMBuffer;
	uint8*                m_pOCMBufferAligned;
	uint32                m_OCMMeshCount;
	uint32                m_OCMInstCount;
	uint32                m_OCMOffsetInstances;

	template<class T>
	T Swap(T& rData)
	{
		// #if IS_LOCAL_MACHINE_BIG_ENDIAN
		PREFAST_SUPPRESS_WARNING(6326)
		switch (sizeof(T))
		{
		case 1:
			break;
		case 2:
			SwapEndianBase(reinterpret_cast<uint16*>(&rData));
			break;
		case 4:
			SwapEndianBase(reinterpret_cast<uint32*>(&rData));
			break;
		case 8:
			SwapEndianBase(reinterpret_cast<uint64*>(&rData));
			break;
		default:
			UNREACHABLE();
		}
		//#endif
		return rData;
	}

	void RasterizeZBuffer(uint32 PolyLimit);
	void OutputMeshList();

public:

	void CreateOcclusionJob(const SCheckOcclusionJobData& rCheckOcclusionData);
	void CheckOcclusion_JobEntry(const SCheckOcclusionJobData checkOcclusionData);
	void PrepareOcclusion();

	void PrepareOcclusion_RasterizeZBuffer();
	void PrepareOcclusion_ReprojectZBuffer();
	void PrepareOcclusion_ReprojectZBufferLine(int nStartLine, int nNumLines);
	void PrepareOcclusion_ReprojectZBufferLineAfterMerge(int nStartLine, int nNumLines);
	void Init();

	bool LoadLevel(const char* pFolderName);
	void UnloadLevel();

	bool TestAABB(const AABB& rAABB, float fEntDistance, float fVerticalExpand = 0.0f);
	bool TestQuad(const Vec3& vCenter, const Vec3& vAxisX, const Vec3& vAxisY);

	void WaitOnCheckOcclusionJobs();

	CCullThread();
	~CCullThread();

#ifndef _RELEASE
	void CoverageBufferDebugDraw();
#endif

	void PrepareCullbufferAsync(const CCamera& rCamera, const SGraphicsPipelineKey& cullGraphicsContextKey);
	void CullStart(const SRenderingPassInfo& passInfo);
	void CullEnd();

	void SetActive(bool bActive);

	Vec3 GetViewDir() { return m_ViewDir; }

};

}
