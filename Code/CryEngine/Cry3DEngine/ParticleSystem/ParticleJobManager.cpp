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

CParticleJobManager::CParticleJobManager()
{
	REGISTER_CVAR(e_ParticlesJobsPerThread, 16, 0, "Maximum particle jobs to assign per worker thread");
}

void CParticleJobManager::AddDeferredRender(CParticleComponentRuntime* pRuntime, const SRenderContext& renderContext)
{
	SDeferredRender render(pRuntime, renderContext);
	m_deferredRenders.push_back(render);
}

void CParticleJobManager::ScheduleComputeVertices(CParticleComponentRuntime* pComponentRuntime, CRenderObject* pRenderObject, const SRenderContext& renderContext)
{
	CParticleManager* pPartManager = static_cast<CParticleManager*>(gEnv->pParticleManager);

	SAddParticlesToSceneJob& job = pPartManager->GetParticlesToSceneJob(renderContext.m_passInfo);
	auto pGpuRuntime = pComponentRuntime->GetGpuRuntime();
	auto pCpuRuntime = pComponentRuntime->GetCpuRuntime();
	if (pGpuRuntime)
		job.pGpuRuntime = pGpuRuntime;
	else if (pCpuRuntime)
		job.pVertexCreator = pCpuRuntime;
	job.pRenderObject = pRenderObject;
	job.pShaderItem = &pRenderObject->m_pCurrMaterial->GetShaderItem();
	job.nCustomTexId = renderContext.m_renderParams.nTextureID;
}

void CParticleJobManager::ScheduleUpdates()
{
	if (m_emitterRefs.empty())
		return;

	CRY_PFX2_ASSERT(!m_updateState.IsRunning());
	if (ThreadMode() == 0)
	{
		// Update synchronously in main thread
		for (auto pEmitter : m_emitterRefs)
			pEmitter->UpdateAll();
		m_emitterRefs.clear();
		return;
	}

	auto job = [this]()
	{
		Job_ScheduleUpdates();
	};
	gEnv->pJobManager->AddLambdaJob("job:pfx2:ScheduleUpdates", job, JobManager::eRegularPriority, &m_updateState);
}

void CParticleJobManager::Job_ScheduleUpdates()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	JobManager::TPriorityLevel priority = JobManager::eRegularPriority;

	if (ThreadMode() >= 3)
	{
		// Schedule emitters rendered last frame first
		TDynArray<CParticleEmitter*> visible, hidden;
		visible.reserve(m_emitterRefs.size());
		hidden.reserve(m_emitterRefs.size());

		for (auto pEmitter : m_emitterRefs)
		{
			if (pEmitter->WasRenderedLastFrame())
				visible.push_back(pEmitter);
			else
				hidden.push_back(pEmitter);
		}

		// Sort by camera Z
		const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
		Vec3 sortDir = -camera.GetViewdir();
		stl::sort(visible, [sortDir](const CParticleEmitter* pe)
		{
			return pe->GetLocation().t | sortDir;
		});

		for (auto pEmitter : visible)
		{
			auto job = [pEmitter]()
			{
				pEmitter->UpdateAll();
			};
			gEnv->pJobManager->AddLambdaJob("job:pfx2:UpdateEmitter", job, priority, &m_updateState);
		}

		std::swap(m_emitterRefs, hidden);
		priority = JobManager::eStreamPriority;
	}

	// Split emitter list into jobs
	const uint maxJobs = gEnv->pJobManager->GetNumWorkerThreads() * e_ParticlesJobsPerThread;
	const uint numJobs = min(m_emitterRefs.size(), maxJobs);

	uint e = 0;
	for (uint j = 0; j < numJobs; ++j)
	{
		uint e2 = (j + 1) * m_emitterRefs.size() / numJobs;
		auto emitters = m_emitterRefs(e, e2 - e);
		e = e2;

		auto job = [emitters]()
		{
			for (auto pEmitter : emitters)
				pEmitter->UpdateAll();
		};
		gEnv->pJobManager->AddLambdaJob("job:pfx2:UpdateEmitters", job, priority, &m_updateState);
	}
	CRY_PFX2_ASSERT(e == m_emitterRefs.size());
}

void CParticleJobManager::SynchronizeUpdates()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	m_updateState.Wait();
	m_emitterRefs.clear();
}

void CParticleJobManager::DeferredRender()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	for (const SDeferredRender& render : m_deferredRenders)
	{
		CParticleComponentRuntime* pRuntime = render.m_pRuntime;
		CParticleComponent* pComponent = pRuntime->GetComponent();
		CParticleEmitter* pEmitter = pRuntime->GetEmitter();
		pEmitter->SyncUpdate();
		SRenderContext renderContext(render.m_rParam, render.m_passInfo);
		renderContext.m_distance = render.m_distance;
		renderContext.m_lightVolumeId = render.m_lightVolumeId;
		renderContext.m_fogVolumeId = render.m_fogVolumeId;
		pRuntime->GetComponent()->RenderDeferred(pEmitter, pRuntime, pComponent, renderContext);
	}
	m_deferredRenders.clear();
}


}
