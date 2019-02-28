// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleJobManager.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "ParticleComponentRuntime.h"
#include "ParticleSystem.h"
#include "ParticleProfiler.h"
#include <CryRenderer/IGpuParticles.h>

namespace pfx2
{

int e_ParticlesJobsPerThread = 16;

ILINE bool EmitterHasDeferred(CParticleEmitter* pEmitter)
{
	return ThreadMode() >= 4 && !pEmitter->GetCEffect()->RenderDeferred.empty();
}

CParticleJobManager::CParticleJobManager()
{
	REGISTER_CVAR(e_ParticlesJobsPerThread, 16, 0, "Maximum particle jobs to assign per worker thread");
}

void CParticleJobManager::AddDeferredRender(CParticleEmitter* pEmitter, const SRenderContext& renderContext)
{
	SDeferredRender render(pEmitter, renderContext);
	m_deferredRenders.push_back(render);
}

void CParticleJobManager::ScheduleComputeVertices(CParticleComponentRuntime& runtime, CRenderObject* pRenderObject, const SRenderContext& renderContext)
{
	CParticleManager* pPartManager = static_cast<CParticleManager*>(gEnv->pParticleManager);

	SAddParticlesToSceneJob& job = pPartManager->GetParticlesToSceneJob(renderContext.m_passInfo);
	if (auto pGpuRuntime = runtime.GetGpuRuntime())
		job.pGpuRuntime = pGpuRuntime;
	else
		job.pVertexCreator = &runtime;
	job.pRenderObject = pRenderObject;
	job.pShaderItem = &pRenderObject->m_pCurrMaterial->GetShaderItem();
	job.nCustomTexId = renderContext.m_renderParams.nTextureID;
	job.aabb = runtime.GetBounds();
}

void CParticleJobManager::AddUpdateEmitter(CParticleEmitter* pEmitter)
{
	CRY_PFX2_ASSERT(!m_updateState.IsRunning());
	if (ThreadMode() == 0)
	{
		// Update synchronously in main thread
		pEmitter->UpdateParticles();
	}
	else
	{
		// Schedule emitters rendered last frame first
		if (ThreadMode() >= 2 && pEmitter->WasRenderedLastFrame())
		{
			if (EmitterHasDeferred(pEmitter))
				m_emittersDeferred.push_back(pEmitter);
			else
				m_emittersVisible.push_back(pEmitter);
		}
		else if (ThreadMode() < 3 || !pEmitter->IsStable())
			m_emittersInvisible.push_back(pEmitter);
	}
}

void CParticleJobManager::ScheduleUpdateEmitter(CParticleEmitter* pEmitter)
{
	auto job = [pEmitter]()
	{
		pEmitter->UpdateParticles();
	};
	auto priority = EmitterHasDeferred(pEmitter) ? JobManager::eHighPriority : JobManager::eRegularPriority;
	gEnv->pJobManager->AddLambdaJob("job:pfx2:UpdateEmitter", job, priority, &m_updateState);
}

void CParticleJobManager::ScheduleUpdates()
{
	// Schedule jobs in a high-priority job
	CRY_PFX2_ASSERT(!m_updateState.IsRunning());
	auto job = [this]()
	{
		Job_ScheduleUpdates();
	};
	gEnv->pJobManager->AddLambdaJob("job:pfx2:ScheduleUpdates", job, JobManager::eHighPriority, &m_updateState);
}

void CParticleJobManager::Job_ScheduleUpdates()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	// Schedule deferred emitters in individual high-priority jobs
	for (auto pEmitter : m_emittersDeferred)
		ScheduleUpdateEmitter(pEmitter);
	m_emittersDeferred.resize(0);

	if (m_emittersVisible.size())
	{
		// Sort fast (visible) emitters by camera Z
		const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
		Vec3 sortDir = -camera.GetViewdir();
		stl::sort(m_emittersVisible, [sortDir](const CParticleEmitter* pe)
		{
			return pe->GetLocation().t | sortDir;
		});
		ScheduleUpdateEmitters(m_emittersVisible, JobManager::eRegularPriority);
	}
	ScheduleUpdateEmitters(m_emittersInvisible, JobManager::eStreamPriority);
}

void CParticleJobManager::ScheduleUpdateEmitters(TDynArray<CParticleEmitter*>& emitters, JobManager::TPriorityLevel priority)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	// Batch updates into jobs
	const uint maxJobs = gEnv->pJobManager->GetNumWorkerThreads() * e_ParticlesJobsPerThread;
	const uint numJobs = min(emitters.size(), maxJobs);

	uint e = 0;
	for (uint j = 0; j < numJobs; ++j)
	{
		uint e2 = (j + 1) * emitters.size() / numJobs;
		auto emitterGroup = emitters(e, e2 - e);
		e = e2;

		auto job = [emitterGroup]()
		{
			for (auto pEmitter : emitterGroup)
				pEmitter->UpdateParticles();
		};
		gEnv->pJobManager->AddLambdaJob("job:pfx2:UpdateEmitters", job, priority, &m_updateState);
	}
	CRY_PFX2_ASSERT(e == emitters.size());
}

void CParticleJobManager::SynchronizeUpdates()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	m_updateState.Wait();
	m_emittersInvisible.resize(0);
	m_emittersVisible.resize(0);
}

void CParticleJobManager::DeferredRender()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	for (const SDeferredRender& render : m_deferredRenders)
	{
		SRenderContext renderContext(render.m_rParam, render.m_passInfo);
		renderContext.m_distance = render.m_distance;
		renderContext.m_lightVolumeId = render.m_lightVolumeId;
		renderContext.m_fogVolumeId = render.m_fogVolumeId;
		render.m_pEmitter->RenderDeferred(renderContext);
	}
	m_deferredRenders.clear();
}


}
