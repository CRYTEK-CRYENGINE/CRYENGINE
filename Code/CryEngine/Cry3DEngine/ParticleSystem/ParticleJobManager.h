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
		SDeferredRender(CParticleEmitter* pEmitter, const SRenderContext& renderContext)
			: m_pEmitter(pEmitter)
			, m_rParam(renderContext.m_renderParams)
			, m_passInfo(renderContext.m_passInfo)
			, m_distance(renderContext.m_distance)
			, m_lightVolumeId(renderContext.m_lightVolumeId)
			, m_fogVolumeId(renderContext.m_fogVolumeId) {}
		CParticleEmitter*  m_pEmitter;
		SRendParams        m_rParam;
		SRenderingPassInfo m_passInfo;
		float              m_distance;
		uint16             m_lightVolumeId;
		uint16             m_fogVolumeId;
	};

public:
	CParticleJobManager();
	void AddUpdateEmitter(CParticleEmitter* pEmitter);
	void ScheduleUpdateEmitter(CParticleEmitter* pEmitter);
	void AddDeferredRender(CParticleEmitter* pEmitter, const SRenderContext& renderContext);
	void ScheduleComputeVertices(CParticleComponentRuntime& runtime, CRenderObject* pRenderObject, const SRenderContext& renderContext);
	void ScheduleUpdates();
	void SynchronizeUpdates();
	void DeferredRender();

private:

	void Job_ScheduleUpdates();
	void ScheduleUpdateEmitters(TDynArray<CParticleEmitter*>& emitters, JobManager::TPriorityLevel priority);

	TDynArray<CParticleEmitter*> m_emittersDeferred;
	TDynArray<CParticleEmitter*> m_emittersVisible;
	TDynArray<CParticleEmitter*> m_emittersInvisible;
	TDynArray<SDeferredRender>   m_deferredRenders;
	JobManager::SJobState        m_updateState;
};

}

