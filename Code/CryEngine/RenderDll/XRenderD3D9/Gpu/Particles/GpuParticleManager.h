// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryRenderer/IGpuParticles.h>
#include "GpuParticleFeatureFactory.h"
#include "Gpu/GpuBitonicSort.h"
#include "GpuParticleComponentRuntime.h"
#include <map>

namespace gpu_pfx2
{

class CGpuInterfaceFactory;

// interface of GPU particle system
class CManager : public IManager
{
public:
	struct SRuntimeRef
	{
		SRuntimeRef() : m_pComponentRuntime(0), m_firstChild(0), m_numChildren(0) {}
		SRuntimeRef(gpu_pfx2::IParticleComponentRuntime* pComponentRuntime)
			: m_pComponentRuntime((CParticleComponentRuntime*)pComponentRuntime)
			, m_firstChild(0)
			, m_numChildren(0)
		{
		}

		CParticleComponentRuntime* m_pComponentRuntime;
		size_t                     m_firstChild;
		size_t                     m_numChildren;
	};

	CManager();

	virtual void BeginFrame() override;

	virtual _smart_ptr<IParticleComponentRuntime>
	CreateParticleComponentRuntime(
		IParticleEmitter* pEmitter,
		pfx2::IParticleComponent* pComponent,
		const pfx2::SRuntimeInitializationParameters& params) override;

	void RenderThreadUpdate();
	void RenderThreadPreUpdate();
	void RenderThreadPostUpdate();

	// gets initialized the first time it is called and will allocate buffers
	// (so make sure its only called from the render thread)
	gpu::CBitonicSort* GetBitonicSort();

	virtual _smart_ptr<IParticleFeatureGpuInterface>
	CreateParticleFeatureGpuInterface(EGpuFeatureType) override;

	// this needs to be called from the render thread when the renderer
	// is teared down to make sure the runtimes are not still persistent when the
	// render thread is gone
	void CleanupResources();
private:
	std::vector<_smart_ptr<CParticleComponentRuntime>>& GetWriteRuntimes()
	{
		return m_particleComponentRuntimes[0];
	}
	std::vector<_smart_ptr<CParticleComponentRuntime>>& GetReadRuntimes()
	{
		return m_particleComponentRuntimes[1];
	}

	void Initialize();

	// in here, gpu resources might be freed
	void ProcessResources();

	// this keeps a reference to runtimes so we can control the lifetime and
	// release resources on the render thread if refcount = 1
	std::vector<_smart_ptr<CParticleComponentRuntime>>                    m_particleComponentRuntimes[2];
	std::vector<_smart_ptr<CFeature>>                                     m_particleFeatureGpuInterfaces;
	std::vector<_smart_ptr<CFeature>>                                     m_particleFeatureGpuInterfacesInitialization;

	gpu::CTypedResource<uint, gpu::BufferFlagsReadWriteReadback>          m_counter;
	gpu::CTypedResource<uint, gpu::BufferFlagsReadWriteReadback>          m_scratch;
	gpu::CTypedResource<SReadbackData, gpu::BufferFlagsReadWriteReadback> m_readback;

	int m_numRuntimesReadback;

	CryCriticalSection                 m_cs;

	std::unique_ptr<gpu::CBitonicSort> m_pBitonicSort;
	CGpuInterfaceFactory               m_gpuInterfaceFactory;

	volatile EState                    m_state;
};
}
