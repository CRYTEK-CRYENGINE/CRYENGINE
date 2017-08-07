// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     13/03/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "ParticleComponentRuntime.h"
#include "ParticleSystem.h"
#include "ParticleProfiler.h"
#include "ParticleJobManager.h"
#include <CryRenderer/IGpuParticles.h>

DECLARE_JOB("Particles : UpdateEmitter", TUpdateEmitterJob, pfx2::CParticleJobManager::Job_UpdateEmitter);
DECLARE_JOB("Particles : UpdateComponent", TUpdateComponentJob, pfx2::CParticleJobManager::Job_UpdateComponent);
DECLARE_JOB("Particles : AddRemoveParticles", TAddRemoveJob, pfx2::CParticleJobManager::Job_AddRemoveParticles);
DECLARE_JOB("Particles : UpdateParticles", TUpdateParticlesJob_, pfx2::CParticleJobManager::Job_UpdateParticles);
DECLARE_JOB("Particles : PostUpdateParticles", TPostUpdateParticlesJob, pfx2::CParticleJobManager::Job_PostUpdateParticles);
DECLARE_JOB("Particles : CalculateBounds", TCalculateBoundsJob, pfx2::CParticleJobManager::Job_CalculateBounds);

namespace pfx2
{

stl::TPoolAllocator<TPostUpdateParticlesJob>& GetPostJobPool()
{
	static stl::TPoolAllocator<TPostUpdateParticlesJob> pool;
	return pool;
}

void CParticleJobManager::AddEmitter(CParticleEmitter* pEmitter)
{
	CRY_PFX2_ASSERT(!m_updateState.IsRunning());

	uint numComponents = 0;
	for (auto& ref : pEmitter->GetRuntimes())
	{
		auto pCpuRuntime = ref.pRuntime->GetCpuRuntime();
		if (!pCpuRuntime)
			continue;
		const bool isActive = pCpuRuntime->IsActive();
		const bool isChild = pCpuRuntime->IsChild();
		if (isActive && !isChild)
		{
			size_t refIdx = m_componentRefs.size();
			m_firstGenComponentsRef.push_back(refIdx);
			m_componentRefs.push_back(SComponentRef(pCpuRuntime));
			SComponentRef& componentRef = m_componentRefs.back();
			componentRef.m_firstChild = m_componentRefs.size();
			componentRef.m_pPostSubUpdates = GetPostJobPool().New(m_componentRefs.size() - 1);
			componentRef.m_pPostSubUpdates->SetClassInstance(this);
			AddComponentRecursive(pEmitter, refIdx);
			numComponents++;
		}
	}
	if (numComponents)
		m_emitterRefs.push_back(pEmitter);
}

void CParticleJobManager::AddComponentRecursive(CParticleEmitter* pEmitter, size_t parentRefIdx)
{
	const CParticleComponentRuntime* pParentComponentRuntime = m_componentRefs[parentRefIdx].m_pComponentRuntime;

	for (const auto& pChild : pParentComponentRuntime->GetComponent()->GetChildComponents())
	{
		auto pChildRuntime = pEmitter->GetRuntimeFor(pChild)->GetCpuRuntime();
		if (!pChildRuntime)
			continue;
		const bool isActive = pChildRuntime->IsActive();
		if (isActive)
		{
			const SComponentParams& childParams = pChildRuntime->GetComponentParams();
			m_componentRefs.push_back(SComponentRef(pChildRuntime));
			SComponentRef& componentRef = m_componentRefs.back();
			componentRef.m_pPostSubUpdates = GetPostJobPool().New(m_componentRefs.size() - 1);
			componentRef.m_pPostSubUpdates->SetClassInstance(this);
			++m_componentRefs[parentRefIdx].m_numChildren;
		}
	}

	size_t numChildrend = m_componentRefs[parentRefIdx].m_numChildren;
	size_t firstChild = m_componentRefs[parentRefIdx].m_firstChild;
	for (size_t i = 0; i < numChildrend; ++i)
	{
		size_t childRefIdx = firstChild + i;
		m_componentRefs[childRefIdx].m_firstChild = m_componentRefs.size();
		AddComponentRecursive(pEmitter, childRefIdx);
	}
}

void CParticleJobManager::AddDeferredRender(CParticleComponentRuntime* pRuntime, const SRenderContext& renderContext)
{
	SDeferredRender render(pRuntime, renderContext);
	m_deferredRenders.push_back(render);
}

void CParticleJobManager::ScheduleComputeVertices(IParticleComponentRuntime* pComponentRuntime, CRenderObject* pRenderObject, const SRenderContext& renderContext)
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

void CParticleJobManager::KernelUpdateAll()
{
	if (m_firstGenComponentsRef.empty())
		return;

	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	CRY_PFX2_ASSERT(!m_updateState.IsRunning());

	CVars* pCVars = static_cast<C3DEngine*>(gEnv->p3DEngine)->GetCVars();

	if (pCVars->e_ParticlesThread == 3)
	{
		// One job per component task
		for (size_t i = 0; i < m_componentRefs.size(); ++i)
			m_updateState.SetRunning();
		for (size_t idx : m_firstGenComponentsRef)
		{
			TAddRemoveJob job(idx);
			job.SetClassInstance(this);
			job.Run();
		}
	}
	else if (pCVars->e_ParticlesThread == 2)
	{
		// One job per component
		for (size_t i = 0; i < m_componentRefs.size(); ++i)
			m_updateState.SetRunning();
		for (size_t idx : m_firstGenComponentsRef)
		{
			TUpdateComponentJob job(idx);
			job.SetClassInstance(this);
			job.Run();
		}
	}
	else if (pCVars->e_ParticlesThread == 1)
	{
		// One job per emitter
		for (size_t i = 0; i < m_emitterRefs.size(); ++i)
			m_updateState.SetRunning();
		for (size_t i = 0; i < m_emitterRefs.size(); ++i)
		{
			TUpdateEmitterJob job(i);
			job.SetClassInstance(this);
			job.Run();
		}
	}
	else
	{
		// No threading
		for (auto& componentRef : m_componentRefs)
		{
			componentRef.m_pComponentRuntime->UpdateAll();
		}
	}
}

void CParticleJobManager::SynchronizeUpdate()
{
	if (m_firstGenComponentsRef.empty())
		return;
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	gEnv->pJobManager->WaitForJob(m_updateState);
}

void CParticleJobManager::DeferredRender()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	for (const SDeferredRender& render : m_deferredRenders)
	{
		CParticleComponentRuntime* pRuntime = render.m_pRuntime;
		CParticleComponent* pComponent = pRuntime->GetComponent();
		CParticleEmitter* pEmitter = pRuntime->GetEmitter();
		SRenderContext renderContext(render.m_rParam, render.m_passInfo);
		renderContext.m_distance = render.m_distance;
		renderContext.m_lightVolumeId = render.m_lightVolumeId;
		renderContext.m_fogVolumeId = render.m_fogVolumeId;
		pComponent->RenderDeferred(pEmitter, pRuntime, renderContext);
	}

	ClearAll();
}

void CParticleJobManager::Job_UpdateEmitter(uint emitterRefIdx)
{
	auto& profiler = GetPSystem()->GetProfiler();
	CParticleEmitter* pEmitter = m_emitterRefs[emitterRefIdx];

	for (auto ref : pEmitter->GetRuntimes())
	{
		if (auto pCpuRuntime = ref.pRuntime->GetCpuRuntime())
		{
			if (pCpuRuntime->IsActive())
			{
				profiler.AddEntry(pCpuRuntime, EPS_Jobs);
				pCpuRuntime->UpdateAll();
			}
		}
	}
	m_updateState.SetStopped();
}

void CParticleJobManager::Job_UpdateComponent(uint componentRefIdx)
{
	SComponentRef& componentRef = m_componentRefs[componentRefIdx];
	CParticleComponentRuntime* pRuntime = componentRef.m_pComponentRuntime;
	GetPSystem()->GetProfiler().AddEntry(pRuntime, EPS_Jobs);

	pRuntime->UpdateAll();
	m_updateState.SetStopped();

	// Schedule child components
	for (size_t i = 0; i < componentRef.m_numChildren; ++i)
	{
		TUpdateComponentJob job(componentRef.m_firstChild + i);
		job.SetClassInstance(this);
		job.Run();
	}
}

void CParticleJobManager::Job_AddRemoveParticles(uint componentRefIdx)
{
	CParticleComponentRuntime* pRuntime = m_componentRefs[componentRefIdx].m_pComponentRuntime;
	GetPSystem()->GetProfiler().AddEntry(pRuntime, EPS_Jobs);

	SUpdateContext context(pRuntime);
	pRuntime->AddRemoveNewBornsParticles(context);
	ScheduleUpdateParticles(componentRefIdx);
}

void CParticleJobManager::Job_UpdateParticles(uint componentRefIdx, SUpdateRange updateRange)
{
	CParticleComponentRuntime* pRuntime = m_componentRefs[componentRefIdx].m_pComponentRuntime;
	GetPSystem()->GetProfiler().AddEntry(pRuntime, EPS_Jobs);

	SUpdateContext context(pRuntime, updateRange);
	pRuntime->UpdateParticles(context);
}

void CParticleJobManager::Job_PostUpdateParticles(uint componentRefIdx)
{
	SComponentRef& componentRef = m_componentRefs[componentRefIdx];
	CParticleComponentRuntime* pRuntime = componentRef.m_pComponentRuntime;
	GetPSystem()->GetProfiler().AddEntry(pRuntime, EPS_Jobs);

	// Schedule child components
	for (size_t i = 0; i < componentRef.m_numChildren; ++i)
	{
		TAddRemoveJob job(componentRef.m_firstChild + i);
		job.SetClassInstance(this);
		job.Run();
	}

	// Schedule calculate bounds
	TCalculateBoundsJob calculateBoundsJob(componentRefIdx);
	calculateBoundsJob.SetClassInstance(this);
	calculateBoundsJob.Run();
}

void CParticleJobManager::Job_CalculateBounds(uint componentRefIdx)
{
	CParticleComponentRuntime* pRuntime = m_componentRefs[componentRefIdx].m_pComponentRuntime;
	GetPSystem()->GetProfiler().AddEntry(pRuntime, EPS_Jobs);

	pRuntime->CalculateBounds();
	m_updateState.SetStopped();
}

void CParticleJobManager::ScheduleUpdateParticles(uint componentRefIdx)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	SComponentRef& componentRef = m_componentRefs[componentRefIdx];
	componentRef.m_subUpdateState.SetRunning();

	componentRef.m_subUpdateState.RegisterPostJob(componentRef.m_pPostSubUpdates);

	const size_t particleCountThreshold = 1024 * 8;
	const CParticleContainer& container = componentRef.m_pComponentRuntime->GetContainer();
	const TParticleId lastParticleId = container.GetLastParticleId();

	for (TParticleId pId = 0; pId < lastParticleId; pId += particleCountThreshold)
	{
		SUpdateRange range(pId, min<TParticleId>(pId + particleCountThreshold, lastParticleId));

		TUpdateParticlesJob_ job(componentRefIdx, range);
		job.RegisterJobState(&componentRef.m_subUpdateState);
		job.SetClassInstance(this);
		job.Run();
	}

	componentRef.m_subUpdateState.SetStopped();
}

void CParticleJobManager::ClearAll()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	CRY_PFX2_ASSERT(!m_updateState.IsRunning());
	for (auto& componentRef : m_componentRefs)
		GetPostJobPool().Delete(componentRef.m_pPostSubUpdates);
	m_deferredRenders.clear();
	m_firstGenComponentsRef.clear();
	m_componentRefs.clear();
	m_emitterRefs.clear();
}

}
