// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     13/03/2015 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef PARTICLEKERNEL_H
#define PARTICLEKERNEL_H

#pragma once

#include <CryThreading/IJobManager_JobDelegator.h>
#include "ParticleContainer.h"

class CRenderObject;
namespace JobManager {
namespace Detail {
class SGenericJobTPostUpdateParticlesJob;
}
}

namespace pfx2
{

class CParticleComponent;
class CParticleComponentRuntime;
class CParticleEffect;
struct SRenderContext;

class CParticleJobManager
{
public:
	struct SComponentRef
	{
		SComponentRef()
			: m_pComponentRuntime(0)
			, m_pPostSubUpdates(0)
			, m_firstChild(0)
			, m_numChildren(0) {}
		SComponentRef(CParticleComponentRuntime* pComponentRuntime)
			: m_pComponentRuntime(pComponentRuntime)
			, m_pPostSubUpdates(0)
			, m_firstChild(0)
			, m_numChildren(0) {}

		CParticleComponentRuntime* m_pComponentRuntime;
		JobManager::SJobState      m_subUpdateState;
		JobManager::Detail::SGenericJobTPostUpdateParticlesJob* m_pPostSubUpdates;
		size_t                     m_firstChild;
		size_t                     m_numChildren;
	};

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
	void AddEmitter(CParticleEmitter* pEmitter);
	void AddDeferredRender(CParticleComponentRuntime* pRuntime, const SRenderContext& renderContext);
	void ScheduleComputeVertices(IParticleComponentRuntime* pComponentRuntime, CRenderObject* pRenderObject, const SRenderContext& renderContext);
	void KernelUpdateAll();
	void SynchronizeUpdate();
	void DeferredRender();

	// job entry points
	void Job_UpdateEmitter(uint componentRefIdx);
	void Job_UpdateComponent(uint componentRefIdx);
	void Job_AddRemoveParticles(uint componentRefIdx);
	void Job_UpdateParticles(uint componentRefIdx, SUpdateRange updateRange);
	void Job_PostUpdateParticles(uint componentRefIdx);
	void Job_CalculateBounds(uint componentRefIdx);
	// ~job entry points

private:
	void AddComponentRecursive(CParticleEmitter* pEmitter, size_t parentRefIdx);

	void ScheduleUpdateParticles(uint componentRefIdx);

	void ClearAll();

	std::vector<SComponentRef>     m_componentRefs;
	std::vector<size_t>            m_firstGenComponentsRef;
	std::vector<CParticleEmitter*> m_emitterRefs;
	std::vector<SDeferredRender>   m_deferredRenders;
	JobManager::SJobState          m_updateState;
};

}

#endif // PARTICLEKERNEL_H
