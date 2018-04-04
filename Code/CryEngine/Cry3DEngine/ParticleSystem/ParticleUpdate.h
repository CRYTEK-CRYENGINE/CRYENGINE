// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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
	mutable SChaosKey          m_updateRng;
	mutable SChaosKeyV         m_updateRngv;

	SUpdateRange GetUpdateRange() const      { return m_updateRange; }
	SGroupRange GetUpdateGroupRange() const  { return SGroupRange(m_updateRange); }
	SUpdateRange GetSpawnedRange() const     { return m_container.GetSpawnedRange(); }
	SGroupRange GetSpawnedGroupRange() const { return SGroupRange(m_container.GetSpawnedRange()); }
};

#define CRY_PFX2_FOR_RANGE_PARTICLES(updateRange) \
  for (auto particleId : updateRange) {

#define CRY_PFX2_FOR_RANGE_PARTICLESGROUP(updateRange) \
  for (auto particleGroupId : SGroupRange(updateRange)) {

#define CRY_PFX2_FOR_ACTIVE_PARTICLES(updateContext) \
  for (auto particleId : (updateContext).GetUpdateRange()) {

#define CRY_PFX2_FOR_ACTIVE_PARTICLESGROUP(updateContext) \
  for (auto particleGroupId : (updateContext).GetUpdateGroupRange()) {

#define CRY_PFX2_FOR_SPAWNED_PARTICLES(updateContext) \
  for (auto particleId : (updateContext).GetSpawnedRange()) {

#define CRY_PFX2_FOR_SPAWNED_PARTICLEGROUP(updateContext) \
  for (auto particleGroupId : (updateContext).GetSpawnedGroupRange()) {

#define CRY_PFX2_FOR_END }


}

#endif // PARTICLEUPDATE_H
