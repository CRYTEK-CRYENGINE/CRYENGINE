// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     25/03/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLEUPDATE_H
#define PARTICLEUPDATE_H

#pragma once

#include "ParticleContainer.h"

namespace pfx2
{

class CParticleComponentRuntime;
class CParticleContainer;

struct SUpdateContext
{
	explicit SUpdateContext(CParticleComponentRuntime* pRuntime);
	SUpdateContext(CParticleComponentRuntime* pRuntime, const SUpdateRange& updateRange);
	CParticleSystem*           m_pSystem;
	CParticleComponentRuntime& m_runtime;
	CParticleContainer&        m_container;
	CParticleContainer&        m_parentContainer;
	const SComponentParams&    m_params;
	const SUpdateRange         m_updateRange;
	TParticleHeap*             m_pMemHeap;
	const float                m_deltaTime;
	const float                m_time;
	mutable SChaosKey          m_spawnRng;
	mutable SChaosKeyV         m_spawnRngv;

	SUpdateRange GetUpdateRange() const      { return m_updateRange; }
	SGroupRange GetUpdateGroupRange() const  { return SGroupRange(m_updateRange); }
	SUpdateRange GetSpawnedRange() const     { return m_container.GetSpawnedRange(); }
	SGroupRange GetSpawnedGroupRange() const { return SGroupRange(m_container.GetSpawnedRange()); }
};


}

#endif // PARTICLEUPDATE_H
