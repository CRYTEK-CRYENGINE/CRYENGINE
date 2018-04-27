// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleCommon.h"

class CRenderObject;

namespace pfx2
{

class CParticleEmitter;
class CParticleComponentRuntime;
struct SRenderContext;

// Schedules particle update and render jobs
class CParticleJobManager
{
public:
	struct SDeferredRender
	{
		SDeferredRender(CParticleComponentRuntime* pRuntime, const SRenderContext& renderContext)
			: m_pRuntime(pRuntime)
			, m_rParam(renderContext.m_renderParams)
			, m_passInfo(renderContext.m_passInfo)
			, m_distance(renderContext.m_distance)
			, m_lightVolumeId(renderContext.m_lightVolumeId)
			, m_fogVolumeId(renderContext.m_fogVolumeId) {}
		CParticleComponentRuntime* m_pRuntime;
		SRendParams                m_rParam;
		SRenderingPassInfo         m_passInfo;
		float                      m_distance;
		uint16                     m_lightVolumeId;
		uint16                     m_fogVolumeId;
	};

public:
	CParticleJobManager();
	void AddEmitter(CParticleEmitter* pEmitter) { m_emitterRefs.push_back(pEmitter); }
	void AddDeferredRender(CParticleComponentRuntime* pRuntime, const SRenderContext& renderContext);
	void ScheduleComputeVertices(CParticleComponentRuntime* pComponentRuntime, CRenderObject* pRenderObject, const SRenderContext& renderContext);
	void ScheduleUpdates();
	void SynchronizeUpdates();
	void DeferredRender();

	static int ThreadMode() { return Cry3DEngineBase::GetCVars()->e_ParticlesThread; }

private:

	void Job_ScheduleUpdates();

	TDynArray<CParticleEmitter*> m_emitterRefs;
	TDynArray<SDeferredRender>   m_deferredRenders;
	JobManager::SJobState        m_updateState;
};

}

