// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleSystem/ParticleFeature.h"

namespace pfx2
{

const uint vertexBufferSize = 16 * 1024;
const uint vertexChunckSize = 4 * 1024;

template<class T, uint bufferSize, uint chunckSize>
class CWriteCombinedBuffer
{
public:
	CWriteCombinedBuffer(FixedDynArray<T>& destMem);
	~CWriteCombinedBuffer();
	FixedDynArray<T>& Array();
	T*                CheckAvailable(int elems);

private:
	uint UnflushedBytes() const;
	void FlushData(uint flushBytes);
	void WrapBuffer();

	FixedDynArray<T>* m_pDestMem;                                             // Destination array in VMEM.
	byte*             m_pDestMemBegin;                                        // Cached pointer to start of VMEM.
	uint              m_flushedBytes;                                         // How much written from src buffer.
	uint              m_writtenDestBytes;                                     // Total written to dest buffer.
	FixedDynArray<T>  m_srcArray;                                             // Array for writing, references following buffer:
	byte*             m_pSrcMemBegin;                                         // Aligned pointer into SrcBuffer;
	byte              m_srcBuffer[bufferSize + CRY_PFX2_PARTICLES_ALIGNMENT]; // Raw buffer for writing.
};

class CParticleRenderBase : public CParticleFeature, public Cry3DEngineBase
{
public:
	virtual EFeatureType GetFeatureType() override;
	virtual void         AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void         Render(CParticleEmitter* pEmitter, CParticleComponentRuntime* pComponentRuntime, CParticleComponent* pComponent, const SRenderContext& renderContext) override;

protected:
	virtual bool     SupportsWaterCulling() const { return false; }
	void             PrepareRenderObject(CParticleEmitter* pEmitter, CParticleComponent* pComponent, uint renderObjectId, uint threadId, uint64 objFlags);
	void             AddRenderObject(CParticleEmitter* pEmitter, CParticleComponentRuntime* pComponentRuntime, CParticleComponent* pComponent, const SRenderContext& renderContext, uint renderObjectId, uint threadId, uint64 objFlags);
	ILINE C3DEngine* Get3DEngine() const          { return static_cast<C3DEngine*>(gEnv->p3DEngine); }

private:
	uint m_renderObjectBeforeWaterId = -1;
	uint m_renderObjectAfterWaterId  = -1;
	bool m_waterCulling              = false;
};

}

#include "ParticleRenderImpl.h"
