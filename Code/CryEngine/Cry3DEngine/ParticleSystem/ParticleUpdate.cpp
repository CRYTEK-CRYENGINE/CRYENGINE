// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     27/10/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleSystem.h"
#include "ParticleUpdate.h"
#include "ParticleComponentRuntime.h"
#include "ParticleEmitter.h"
#include "ParticleEmitter.h"

CRY_PFX2_DBG

namespace
{

pfx2::SChaosKey MakeSpawnSeed(pfx2::CParticleComponentRuntime* pRuntime)
{
	uint32 emitterSeed = pRuntime->GetEmitter()->GetInitialSeed();
	uint32 containerSeed = pRuntime->GetContainer().GetNextSpawnId();
	return pfx2::SChaosKey(
		pfx2::SChaosKey(emitterSeed),
		pfx2::SChaosKey(containerSeed));
}

pfx2::SChaosKey MakeUpdateSeed(pfx2::CParticleComponentRuntime* pRuntime, uint firstParticleId)
{
	uint32 emitterSeed = pRuntime->GetEmitter()->GetCurrentSeed();
	uint32 containerSeed = pRuntime->GetContainer().GetNextSpawnId();
	return pfx2::SChaosKey(
		pfx2::SChaosKey(emitterSeed),
		pfx2::SChaosKey(containerSeed),
		pfx2::SChaosKey(firstParticleId));
}

}

namespace pfx2
{

SUpdateContext::SUpdateContext(CParticleComponentRuntime* pRuntime)
	: SUpdateContext(pRuntime, pRuntime->GetContainer().GetFullRange())
{
}

SUpdateContext::SUpdateContext(CParticleComponentRuntime* pRuntime, const SUpdateRange& updateRange)
	: m_pSystem(GetPSystem())
	, m_runtime(*pRuntime)
	, m_container(pRuntime->GetContainer())
	, m_parentContainer(pRuntime->GetParentContainer())
	, m_params(pRuntime->GetComponentParams())
	, m_updateRange(updateRange)
	, m_deltaTime(pRuntime->GetEmitter()->GetDeltaTime())
	, m_time(pRuntime->GetEmitter()->GetTime())
	, m_spawnRng(MakeSpawnSeed(pRuntime))
	, m_spawnRngv(MakeSpawnSeed(pRuntime))
	, m_updateRng(MakeUpdateSeed(pRuntime, updateRange.m_begin))
	, m_updateRngv(MakeUpdateSeed(pRuntime, updateRange.m_begin))
{
	const uint32 threadId = JobManager::GetWorkerThreadId();
	m_pMemHeap = &m_pSystem->GetMemHeap(threadId);
}

}
