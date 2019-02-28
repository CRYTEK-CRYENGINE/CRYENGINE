// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/RenderElements/CREParticle.h>
#include <CryCore/Containers/HeapContainer.h>
#include "../ParticleBuffer.h"

#include <iterator>
#include <Cry3DEngine/I3DEngine.h>
#include <CryParticleSystem/IParticles.h>
#include <CryThreading/IJobManager_JobDelegator.h>
#include "Common/RenderView.h"
#include "CompiledRenderObject.h"
#include "XRenderD3D9/DriverD3D.h"
#include "GraphicsPipeline/SceneForward.h"
#include "GraphicsPipeline/SceneCustom.h"
#include "GraphicsPipeline/VolumetricFog.h"
#include "GraphicsPipeline/TiledLightVolumes.h"
#include "Gpu/Particles/GpuParticleComponentRuntime.h"

//////////////////////////////////////////////////////////////////////////
// CFillRateManager implementation

void CFillRateManager::AddPixelCount(float fPixels)
{
	if (fPixels > 0.f)
	{
		Lock(); // TODO: Lockless
		m_afPixels.push_back(fPixels);
		m_fTotalPixels += fPixels;
		Unlock();
	}
}

void CFillRateManager::ComputeMaxPixels()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	// Find per-container maximum which will not exceed total.
	// don't use static here, this function can be called before particle cvars are registered
	ICVar* pVar = gEnv->pConsole->GetCVar("e_ParticlesMaxScreenFill");
	if (!pVar)
		return;
	float fMaxTotalPixels = pVar->GetFVal() * CRendererResources::s_renderArea;
	float fNewMax = fMaxTotalPixels;

	Lock();

	if (m_fTotalPixels > fMaxTotalPixels)
	{
		// Compute max pixels we can have per emitter before total exceeded,
		// from previous frame's data.
		std::sort(m_afPixels.begin(), m_afPixels.end());
		float fUnclampedTotal = 0.f;
		int nRemaining = m_afPixels.size();
		for (auto& pixels : m_afPixels)
		{
			float fTotal = fUnclampedTotal + nRemaining * pixels;
			if (fTotal > fMaxTotalPixels)
			{
				fNewMax = (fMaxTotalPixels - fUnclampedTotal) / nRemaining;
				break;
			}
			fUnclampedTotal += pixels;
			nRemaining--;
		}
	}

	// Update current value gradually.
	float fLastMax = m_fMaxPixels;
	float fMaxChange = max(fLastMax, fNewMax) * 0.5f;
	m_fMaxPixels = clamp_tpl(fNewMax, fLastMax - fMaxChange, fLastMax + fMaxChange);

	Reset();

	Unlock();
}

//////////////////////////////////////////////////////////////////////////
//
// CREParticle::SCompiledParticle implementation.
//

struct SParticleInstanceCB
{
	SParticleInstanceCB()
		: m_avgFogVolumeContrib(0.0f, 0.0f, 0.0f, 1.0f)
		, m_glowParams(ZERO)
		, m_vertexOffset(0)
		, m_lightVolumeId(-1)
		, m_envCubemapIndex(-1) {}

	Vec4   m_avgFogVolumeContrib;
	Vec4   m_glowParams;
	uint32 m_vertexOffset;
	uint32 m_lightVolumeId;
	uint32 m_envCubemapIndex;
};

enum class EParticlePSOMode
{
	NoLigthing,
	WithLighting,
	NoLigthingRecursive,
	WithLightingRecursive,
	DebugSolid,
	DebugWireframe,
	VolumetricFog,
	Count
};

class CCompiledParticle
{
public:
	CCompiledParticle()
	{
		auto& graphicsPipeline = gcpRendD3D->GetGraphicsPipeline();
		m_pPerDrawCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SParticleInstanceCB), true);
		m_pShaderDataCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SParticleShaderData));
		m_pPerDrawExtraRS = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	}

// private:
	CDeviceGraphicsPSOPtr m_pGraphicsPSOs[uint(EParticlePSOMode::Count)];
	CDeviceResourceSetPtr m_pPerDrawExtraRS;
	CConstantBufferPtr    m_pPerDrawCB;
	CConstantBufferPtr    m_pShaderDataCB;
	Vec4                  m_glowParams = Vec4(ZERO);
};

namespace
{

class CCompileParticlePool
{
public:
	~CCompileParticlePool()
	{
		// CREParticle::ResetPool should habe been
		// called before application tear down
		assert(m_pool.empty());
	}

	void ReturnCompiledObject(CCompiledParticle* pObject)
	{
		SREC_AUTO_LOCK(m_lock);
		m_pool.push_back(pObject);
	}

	TCompiledParticlePtr MakeCompiledObject()
	{
		SREC_AUTO_LOCK(m_lock);
		const auto ReturnFn = [this](CCompiledParticle* pObject) { ReturnCompiledObject(pObject); };
		CCompiledParticle* pObject;
		if (m_pool.empty())
		{
			pObject = new CCompiledParticle();
			++m_objectCount;
		}
		else
		{
			pObject = m_pool.back();
			m_pool.pop_back();
		}
		return TCompiledParticlePtr(pObject, ReturnFn);
	}

	void Reset()
	{
		CRY_ASSERT(m_objectCount == m_pool.size()); // All objects should have been returned to pool

		SREC_AUTO_LOCK(m_lock);
		for (auto* pObject : m_pool)
			delete pObject;
		m_pool.clear();
		m_objectCount = 0;
	}

private:
	std::vector<CCompiledParticle*> m_pool;
	SRecursiveSpinLock              m_lock;
	uint32                          m_objectCount = 0;
};

CCompileParticlePool& GetCompiledObjectPool()
{
	static CCompileParticlePool pool;
	return pool;
}

}

//////////////////////////////////////////////////////////////////////////
//
// CREParticle implementation.
//

CREParticle::CREParticle()
	: m_pCompiledParticle(nullptr)
	, m_pVertexCreator(nullptr)
	, m_pGpuRuntime(nullptr)
	, m_nFirstVertex(0)
	, m_nFirstIndex(0)
	, m_addedToView(0)
{
	mfSetType(eDATA_Particle);
}

void CREParticle::ResetPool()
{
	GetCompiledObjectPool().Reset();
}

void CREParticle::Reset(IParticleVertexCreator* pVC, int nThreadId, uint allocId)
{
	if (m_pVertexCreator)
	{
		assert(m_pVertexCreator == pVC);
		assert(m_nThreadId == nThreadId);
	}
	m_pVertexCreator = pVC;
	m_pGpuRuntime = nullptr;
	m_nThreadId = nThreadId;
	m_allocId = allocId;
	Construct(m_RenderVerts);
	m_nFirstVertex = 0;
	m_nFirstIndex = 0;
}

void CREParticle::SetRuntime(gpu_pfx2::CParticleComponentRuntime* pRuntime)
{
	m_pVertexCreator = nullptr;
	m_pGpuRuntime = pRuntime;
}

SRenderVertices* CREParticle::AllocVertices(int nAllocVerts, int nAllocInds)
{
	auto& particleBuffer = gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet();

	CParticleBufferSet::SAlloc vertAlloc = particleBuffer.AllocVertices(m_allocId, nAllocVerts);
	SVF_Particle* pVertexBuffer = alias_cast<SVF_Particle*>(vertAlloc.m_pBase) + vertAlloc.m_firstElem;
	m_RenderVerts.aVertices.set(ArrayT(pVertexBuffer, int(vertAlloc.m_numElemns))); 
	m_nFirstVertex = vertAlloc.m_firstElem;

	CParticleBufferSet::SAlloc indAlloc = particleBuffer.AllocIndices(m_allocId, nAllocInds);
	uint16* pIndexBuffer = alias_cast<uint16*>(indAlloc.m_pBase) + indAlloc.m_firstElem;
	m_RenderVerts.aIndices.set(ArrayT(pIndexBuffer, int(indAlloc.m_numElemns)));
	m_nFirstIndex = indAlloc.m_firstElem;

	m_RenderVerts.fPixels = 0.f;

	return &m_RenderVerts;
}

SRenderVertices* CREParticle::AllocPullVertices(int nPulledVerts)
{
	auto& particleBuffer = gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet();

	CParticleBufferSet::SAllocStreams streams = particleBuffer.AllocStreams(m_allocId, nPulledVerts);
	m_RenderVerts.aPositions.set(ArrayT(streams.m_pPositions, int(streams.m_numElemns)));
	m_RenderVerts.aAxes.set(ArrayT(streams.m_pAxes, int(streams.m_numElemns)));
	m_RenderVerts.aColorSTs.set(ArrayT(streams.m_pColorSTs, int(streams.m_numElemns)));
	m_nFirstVertex = streams.m_firstElem;

	m_RenderVerts.fPixels = 0.f;

	return &m_RenderVerts;
}

void CREParticle::ComputeVertices(SCameraInfo camInfo, uint64 uRenderFlags)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	m_pVertexCreator->ComputeVertices(camInfo, this, uRenderFlags, gRenDev->m_FillRateManager.GetMaxPixels());
}

//////////////////////////////////////////////////////////////////////////
//
// CRenderer particle functions implementation.
//

void CRenderer::EF_AddParticle(CREParticle* pParticle, SShaderItem& shaderItem, CRenderObject* pRO, const SRenderingPassInfo& passInfo)
{
	if (pRO)
	{
		uint32 nBatchFlags;
		int nList;
		auto nThreadID = gRenDev->GetMainThreadID();
		EF_GetParticleListAndBatchFlags(nBatchFlags, nList, pRO, shaderItem, passInfo);
		passInfo.GetRenderView()->AddRenderItem(pParticle, pRO, shaderItem, nList, nBatchFlags, passInfo, passInfo.GetRendItemSorter(), passInfo.IsShadowPass(), passInfo.IsAuxWindow());
	}
}

void CRenderer::EF_RemoveParticlesFromScene()
{
	m_FillRateManager.ComputeMaxPixels();
}

void CRenderer::SyncComputeVerticesJobs()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	gEnv->pJobManager->WaitForJob(m_ComputeVerticesJobState);
}

void CRenderer::EF_AddMultipleParticlesToScene(const SAddParticlesToSceneJob* jobs, size_t numJobs, const SRenderingPassInfo& passInfo) PREFAST_SUPPRESS_WARNING(6262)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	ASSERT_IS_MAIN_THREAD(m_pRT)

	// update fill thread id for particle jobs
	const CCamera& camera = passInfo.GetCamera();
	int threadList = passInfo.ThreadID();

	auto& particleBuffer = gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet();

	// skip particle rendering in rare cases (like after a resolution change)
	if (!particleBuffer.IsValid(passInfo.GetFrameID()))
		return;

	// if we have jobs, set our sync variables to running before starting the jobs
	if (numJobs)
	{
		PrepareParticleRenderObjects(Array<const SAddParticlesToSceneJob>(jobs, numJobs), 0, passInfo);
	}
}

///////////////////////////////////////////////////////////////////////////////
void CRenderer::PrepareParticleRenderObjects(Array<const SAddParticlesToSceneJob> aJobs, int nREStart, const SRenderingPassInfo& passInfo) PREFAST_SUPPRESS_WARNING(6262)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	const auto& particleBuffer = gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet();

	// == create list of non empty container to submit to the renderer == //
	threadID threadList = passInfo.ThreadID();
	const uint allocId = particleBuffer.GetAllocId(passInfo.GetFrameID());

	// make sure the GPU doesn't use the VB/IB Buffer we are going to fill anymore
	WaitForParticleBuffer(passInfo.GetFrameID());

	// == now create the render elements and start processing those == //
	ICVar* pVar = gEnv->pConsole->GetCVar("e_ParticlesDebug");
	const bool computeSync = pVar && (pVar->GetIVal() & AlphaBit('v'));
	const bool useComputeVerticesJob = !computeSync && passInfo.IsGeneralPass();
	if (useComputeVerticesJob)
	{
		m_ComputeVerticesJobState.SetRunning();
	}

	SCameraInfo camInfo(passInfo);
	const bool bParticleTessellation = m_bDeviceSupportsTessellation && CV_r_ParticlesTessellation != 0;

	for (auto& job : aJobs)
	{
		// Generate the RenderItem for this particle container
		int nList;
		uint32 nBatchFlags;
		SShaderItem shaderItem = *job.pShaderItem;
		CRenderObject* pRenderObject = job.pRenderObject;

		const bool isRefractive = shaderItem.m_pShader && (((CShader*)shaderItem.m_pShader)->m_Flags & EF_REFRACTIVE);
		const bool refractionEnabled = CV_r_Refraction && CV_r_ParticlesRefraction;
		if (isRefractive && !refractionEnabled)
			continue;

		// When changing sys_spec from Flash GUI, clearing cached render objects delays one frame from the change of sys_spec.
		// Because it's executed in UpdateEngineData() via CParticleManager::OnFrameStart(), and OnFrameStart() is called before CFlashUI::UpdateFG().
		// If this branch doesn't exist, assertion is triggered due to invalid render object and CCompiledParticle.
		const bool bAllowTessellation = (pRenderObject->m_ObjFlags & FOB_ALLOW_TESSELLATION) != 0;
		const bool bReadyToRender = ((bParticleTessellation && bAllowTessellation) || (!bAllowTessellation));
		if (!bReadyToRender)
			continue;

		size_t ij = &job - aJobs.data();
		CREParticle* pRE = static_cast<CREParticle*>(pRenderObject->m_pRE);

		// Clamp AABB
		auto aabb = job.aabb;
		if (aabb.IsReset())
			aabb = AABB{ .0f };
		if (pRenderObject->m_pCompiledObject)
			pRenderObject->m_pCompiledObject->m_aabb = aabb;

		// generate the RenderItem entries for this Particle Element
		assert(pRenderObject->m_bPermanent);
		EF_GetParticleListAndBatchFlags(
			nBatchFlags, nList, pRenderObject,
			shaderItem, passInfo);
		if (!pRE->AddedToView())
		{
			// Update particle AABB
			pRE->SetBBox(aabb.min, aabb.max);

			passInfo.GetRenderView()->AddRenderItem(
				pRE, pRenderObject, shaderItem, nList, nBatchFlags, 
				passInfo, passInfo.GetRendItemSorter(), passInfo.IsShadowPass(), 
				passInfo.IsAuxWindow());

			pRE->SetAddedToView();
		}

		if (job.nCustomTexId > 0)
		{
			pRE->m_CustomTexBind[0] = job.nCustomTexId;
		}
		else
		{
			if (passInfo.IsAuxWindow())
				pRE->m_CustomTexBind[0] = CRendererResources::s_ptexDefaultProbeCM->GetID();
			else
				pRE->m_CustomTexBind[0] = CRendererResources::s_ptexBlackCM->GetID();
		}

		if (job.pVertexCreator)
		{
			pRE->Reset(job.pVertexCreator, threadList, allocId);
			if (useComputeVerticesJob)
			{
				// Start new job to compute the vertices
				auto job = [pRE, camInfo, pRenderObject]()
				{
					pRE->ComputeVertices(camInfo, pRenderObject->m_ObjFlags);
				};
				gEnv->pJobManager->AddLambdaJob("job:pfx2:UpdateEmitter", job, JobManager::eLowPriority, &m_ComputeVerticesJobState);
			}
			else
			{
				// Perform it in same thread.
				pRE->ComputeVertices(camInfo, pRenderObject->m_ObjFlags);
			}
		}
		else if (job.pGpuRuntime)
		{
			pRE->SetRuntime(static_cast<gpu_pfx2::CParticleComponentRuntime*>(job.pGpuRuntime));
		}

		passInfo.GetRendItemSorter().IncreaseParticleCounter();
	}

	if (useComputeVerticesJob)
	{
		m_ComputeVerticesJobState.SetStopped();
	}
}

///////////////////////////////////////////////////////////////////////////////
void CRenderer::EF_GetParticleListAndBatchFlags(uint32& nBatchFlags, int& nList, CRenderObject* pRenderObject, const SShaderItem& shaderItem, const SRenderingPassInfo& passInfo)
{
	nBatchFlags = FB_GENERAL;
	nBatchFlags |= (pRenderObject->m_ObjFlags & FOB_AFTER_WATER) ? 0 : FB_BELOW_WATER;

	const uint8 uHalfResBlend = CV_r_ParticlesHalfResBlendMode ? OS_ADD_BLEND : OS_ALPHA_BLEND;
	bool bHalfRes = (CV_r_ParticlesHalfRes + (pRenderObject->m_ParticleObjFlags & CREParticle::ePOF_HALF_RES)) >= 2   // cvar allows or forces half-res
	                && pRenderObject->m_RState & uHalfResBlend
	                && pRenderObject->m_ObjFlags & FOB_AFTER_WATER;

	const bool bVolumeFog = (pRenderObject->m_ParticleObjFlags & CREParticle::ePOF_VOLUME_FOG) != 0;
	const bool bPulledVertices = (pRenderObject->m_ParticleObjFlags & CREParticle::ePOF_USE_VERTEX_PULL_MODEL) != 0;
	const bool bUseTessShader = !bVolumeFog && (pRenderObject->m_ObjFlags & FOB_ALLOW_TESSELLATION) != 0;
	const bool bNearest = (pRenderObject->m_ObjFlags & FOB_NEAREST) != 0;

	// Adjust shader and flags.
	if (bUseTessShader)
		pRenderObject->m_ObjFlags &= ~FOB_OCTAGONAL;

	SShaderTechnique* pTech = shaderItem.GetTechnique();
	assert(!pTech || (bUseTessShader && pTech->m_Flags & FHF_USE_HULL_SHADER) || !bUseTessShader);

	// Disable vertex instancing on unsupported hardware, or from cvar.
	const bool useTessellation = (pRenderObject->m_ObjFlags & FOB_ALLOW_TESSELLATION) != 0;
#if CRY_PLATFORM_ORBIS
	const bool canUsePointSprites = useTessellation || bPulledVertices;
#else
	const bool canUsePointSprites = useTessellation || CV_r_ParticlesInstanceVertices;
#endif
	if (!canUsePointSprites)
		pRenderObject->m_ObjFlags &= ~FOB_POINT_SPRITE;

	if (shaderItem.m_pShader && (((CShader*)shaderItem.m_pShader)->m_Flags & EF_REFRACTIVE))
	{
		nBatchFlags |= FB_TRANSPARENT;
		if (CRenderer::CV_r_Refraction)
			pRenderObject->m_ObjFlags |= FOB_REQUIRES_RESOLVE;

		bHalfRes = false;
	}

	if (pTech)
	{
		int nThreadID = passInfo.ThreadID();
		SRenderObjData* pRenderObjectData = pRenderObject->GetObjData();
		if (pTech->m_nTechnique[TTYPE_CUSTOMRENDERPASS] > 0)
		{
			if (m_nThermalVisionMode && pRenderObjectData->m_nVisionParams)
			{
				nBatchFlags |= FB_CUSTOM_RENDER;
				bHalfRes = false;
			}
		}

		pRenderObject->m_ObjFlags |= (pRenderObjectData && pRenderObjectData->m_LightVolumeId) ? FOB_LIGHTVOLUME : 0;
	}

	if (bHalfRes)
		nList = EFSLIST_HALFRES_PARTICLES;
	else if (bVolumeFog)
		nList = EFSLIST_FOG_VOLUME;
	else if (pRenderObject->m_RState & OS_TRANSPARENT)
		nList = bNearest ? EFSLIST_TRANSP_NEAREST : 
			(!!(nBatchFlags & FB_BELOW_WATER) ? EFSLIST_TRANSP_BW : EFSLIST_TRANSP_AW);
	else
		nList = EFSLIST_GENERAL;
}

bool CREParticle::Compile(CRenderObject* pRenderObject, CRenderView *pRenderView, bool updateInstanceDataOnly)
{
	if (updateInstanceDataOnly)
	{
		// Fast path
		PrepareDataToRender(pRenderView, pRenderObject);
		// NOTE: Works only because CB's resource state doesn't change (is reverted or UPLOAD-heap)
		return true;
	}

	const bool isPulledVertices = (pRenderObject->m_ParticleObjFlags & CREParticle::ePOF_USE_VERTEX_PULL_MODEL) != 0;
	const bool isPointSprites = (pRenderObject->m_ObjFlags & FOB_POINT_SPRITE) != 0;
	const bool bVolumeFog = (pRenderObject->m_ParticleObjFlags & CREParticle::ePOF_VOLUME_FOG) != 0;
	const bool bUseTessShader = !bVolumeFog && (pRenderObject->m_ObjFlags & FOB_ALLOW_TESSELLATION) != 0;
	const bool isGpuParticles = m_pGpuRuntime != nullptr;

	auto& graphicsPipeline = gcpRendD3D->GetGraphicsPipeline();
	auto& coreCommandList = GetDeviceObjectFactory().GetCoreCommandList();
	auto& commandInterface = *coreCommandList.GetGraphicsInterface();
	const SShaderItem& shaderItem = pRenderObject->m_pCurrMaterial->GetShaderItem();
	CShader* pShader = static_cast<CShader*>(shaderItem.m_pShader);
	CShaderResources* pShaderResources = static_cast<CShaderResources*>(shaderItem.m_pShaderResources);

	const int TECHNIQUE_TESS = 0;
	const int TECHNIQUE_NO_TESS = 1;
	const int TECHNIQUE_PULLED = 2;
	const int TECHNIQUE_GPU = 3;
	const int TECHNIQUE_VOL_FOG = 4;

	int techniqueId = -1;
	if (isGpuParticles)
		techniqueId = TECHNIQUE_GPU;
	else if (bUseTessShader)
		techniqueId = TECHNIQUE_TESS;
	else if (isPulledVertices)
		techniqueId = TECHNIQUE_PULLED;
	else
		techniqueId = TECHNIQUE_NO_TESS;
	assert(techniqueId != -1);

	SGraphicsPipelineStateDescription stateDesc;
	stateDesc.shaderItem = shaderItem;
	stateDesc.shaderItem.m_nTechnique = techniqueId;
	stateDesc.technique = TTYPE_GENERAL;
	stateDesc.objectFlags = pRenderObject->m_ObjFlags;
	stateDesc.renderState = pRenderObject->m_RState;
	if (isGpuParticles)
	{
		stateDesc.vertexFormat = EDefaultInputLayouts::Empty;
		stateDesc.primitiveType = eptTriangleList;
	}
	else if (isPulledVertices)
	{
		stateDesc.vertexFormat = EDefaultInputLayouts::Empty;
		if (isPointSprites)
		{
			stateDesc.primitiveType = eptTriangleList;
			stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_SPRITE];
		}
		else
		{
			stateDesc.primitiveType = eptTriangleStrip;
		}
	}
	else
	{
		stateDesc.vertexFormat = EDefaultInputLayouts::P3F_C4B_T4B_N3F2;
		if (isPointSprites && !bUseTessShader)
			stateDesc.streamMask |= VSM_INSTANCED;
		if (bUseTessShader && isPointSprites)
			stateDesc.primitiveType = ept1ControlPointPatchList;
		else if (bUseTessShader)
			stateDesc.primitiveType = ept4ControlPointPatchList;
		else if (isPointSprites)
			stateDesc.primitiveType = eptTriangleStrip;
		else
			stateDesc.primitiveType = eptTriangleList;
		if (isPointSprites)
			stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_SPRITE];
	}
	if (!bUseTessShader)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_NO_TESSELLATION];
	if (pRenderObject->m_ObjFlags & FOB_INSHADOW)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW];
	if (pRenderObject->m_ObjFlags & FOB_SOFT_PARTICLE)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_SOFT_PARTICLE];
	if (pRenderObject->m_RState & OS_ALPHA_BLEND)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_ALPHABLEND];
	if (pRenderObject->m_RState & OS_ANIM_BLEND)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_ANIM_BLEND];
	if (pRenderObject->m_RState & OS_ENVIRONMENT_CUBEMAP)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_ENVIRONMENT_CUBEMAP];
	if (!(pRenderObject->m_ObjFlags & FOB_NO_FOG))
	{
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_FOG];
		if (gcpRendD3D->m_bVolumetricFogEnabled)
			stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG];
	}

	if (!m_pCompiledParticle)
		m_pCompiledParticle = GetCompiledObjectPool().MakeCompiledObject();

	CDeviceGraphicsPSOPtr pGraphicsPSO;

	auto customForwardState = [](CDeviceGraphicsPSODesc& psoDesc, const SGraphicsPipelineStateDescription& desc)
	{
		psoDesc.m_CullMode = eCULL_None;
		psoDesc.m_bAllowTesselation = true;

		const bool bDepthFixup = (((CShader*)desc.shaderItem.m_pShader)->GetFlags2() & EF2_DEPTH_FIXUP) != 0;

		switch (desc.renderState & OS_TRANSPARENT)
		{
		case OS_ALPHA_BLEND:
			if (bDepthFixup)
				psoDesc.m_RenderState |= GS_BLALPHA_MIN | GS_BLSRC_SRC1ALPHA | GS_BLDST_ONEMINUSSRC1ALPHA;
			else
				psoDesc.m_RenderState |= GS_BLALPHA_MIN | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
			break;
		case OS_ADD_BLEND:
			psoDesc.m_RenderState |= GS_BLALPHA_MIN | GS_BLSRC_ONE | GS_BLDST_ONE;
			break;
		case OS_MULTIPLY_BLEND:
			psoDesc.m_RenderState |= GS_BLALPHA_MIN | GS_BLSRC_DSTCOL | GS_BLDST_SRCCOL;
			break;
		}

		if (desc.renderState & OS_NODEPTH_TEST)
			psoDesc.m_RenderState |= GS_NODEPTHTEST;
	};

	bool bCompiled = true;
	bCompiled &= graphicsPipeline.GetSceneForwardStage()->CreatePipelineState(stateDesc, pGraphicsPSO, CSceneForwardStage::ePass_Forward, customForwardState);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::NoLigthing)] = pGraphicsPSO;

	stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	bCompiled &= graphicsPipeline.GetSceneForwardStage()->CreatePipelineState(stateDesc, pGraphicsPSO, CSceneForwardStage::ePass_Forward, customForwardState);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::WithLighting)] = pGraphicsPSO;

	stateDesc.objectRuntimeMask &= ~g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG]; // volumetric fog supports only general pass currently.
	stateDesc.objectRuntimeMask &= ~g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	bCompiled &= graphicsPipeline.GetSceneForwardStage()->CreatePipelineState(stateDesc, pGraphicsPSO, CSceneForwardStage::ePass_ForwardRecursive, customForwardState);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::NoLigthingRecursive)] = pGraphicsPSO;

	stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	bCompiled &= graphicsPipeline.GetSceneForwardStage()->CreatePipelineState(stateDesc, pGraphicsPSO, CSceneForwardStage::ePass_ForwardRecursive, customForwardState);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::WithLightingRecursive)] = pGraphicsPSO;

	stateDesc.objectRuntimeMask &= ~g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_DEBUG0];
	bCompiled &= graphicsPipeline.GetSceneCustomStage()->CreatePipelineState(stateDesc, CSceneCustomStage::ePass_DebugViewSolid, pGraphicsPSO);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::DebugSolid)] = pGraphicsPSO;

	bCompiled &= graphicsPipeline.GetSceneCustomStage()->CreatePipelineState(stateDesc, CSceneCustomStage::ePass_DebugViewWireframe, pGraphicsPSO);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::DebugWireframe)] = pGraphicsPSO;

	if (!bCompiled)
		return false;

	if (bVolumeFog)
	{
		stateDesc.objectRuntimeMask &= ~(g_HWSR_MaskBit[HWSR_LIGHTVOLUME0] | g_HWSR_MaskBit[HWSR_DEBUG0]); // remove unneeded flags.

		stateDesc.shaderItem.m_nTechnique = TECHNIQUE_VOL_FOG; // particles are always rendered with vol fog technique in vol fog pass.

		bCompiled &= graphicsPipeline.GetVolumetricFogStage()->CreatePipelineState(stateDesc, pGraphicsPSO);
		m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::VolumetricFog)] = pGraphicsPSO;
	}

	const ColorF glowParam = pShaderResources->GetFinalEmittance();
	const SRenderObjData& objectData = *pRenderObject->GetObjData();
	m_pCompiledParticle->m_glowParams = Vec4(glowParam.r, glowParam.g, glowParam.b, 0.0f);

	m_pCompiledParticle->m_pShaderDataCB->UpdateBuffer(objectData.m_pParticleShaderData, sizeof(*objectData.m_pParticleShaderData));

	CDeviceResourceSetDesc perInstanceExtraResources(graphicsPipeline.GetDefaultDrawExtraResourceLayout(), nullptr, nullptr);

	perInstanceExtraResources.SetConstantBuffer(
		eConstantBufferShaderSlot_PerGroup,
		m_pCompiledParticle->m_pShaderDataCB,
		EShaderStage_Vertex | EShaderStage_Hull | EShaderStage_Pixel);

	if (isGpuParticles)
	{
		perInstanceExtraResources.SetBuffer(
			EReservedTextureSlot_GpuParticleStream,
			&m_pGpuRuntime->GetContainer()->GetDefaultParticleDataBuffer(),
			EDefaultResourceViews::Default,
			EShaderStage_Vertex);
	}

	m_pCompiledParticle->m_pPerDrawExtraRS->Update(perInstanceExtraResources);

	PrepareDataToRender(pRenderView, pRenderObject);

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	const EShaderStage perDrawInlineShaderStages = (EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain);

	commandInterface.PrepareResourcesForUse(EResourceLayoutSlot_PerMaterialRS, pShaderResources->m_pCompiledResourceSet.get());
	commandInterface.PrepareResourcesForUse(EResourceLayoutSlot_PerDrawExtraRS, m_pCompiledParticle->m_pPerDrawExtraRS.get());

	commandInterface.PrepareInlineConstantBufferForUse(EResourceLayoutSlot_PerDrawCB, m_pCompiledParticle->m_pPerDrawCB, eConstantBufferShaderSlot_PerDraw, perDrawInlineShaderStages);

	return true;
}

void CREParticle::DrawToCommandList(CRenderObject* pRenderObject, const struct SGraphicsPipelinePassContext& context, CDeviceCommandList* commandList)
{
	auto pGraphicsPSO = GetGraphicsPSO(pRenderObject, context);
	if (!pGraphicsPSO || !pGraphicsPSO->IsValid())
		return;

	gRenDev->m_FillRateManager.AddPixelCount(m_RenderVerts.fPixels);

	if (m_pGpuRuntime == nullptr && m_RenderVerts.aPositions.empty() && m_RenderVerts.aVertices.empty())
		return;

	const bool isLegacy = m_pGpuRuntime == nullptr && (pRenderObject->m_ParticleObjFlags & CREParticle::ePOF_USE_VERTEX_PULL_MODEL) == 0;

	CDeviceGraphicsCommandInterface& commandInterface = *commandList->GetGraphicsInterface();
	BindPipeline(pRenderObject, commandInterface, pGraphicsPSO);
	if (isLegacy)
		DrawParticlesLegacy(pRenderObject, commandInterface, context.pRenderView->GetFrameId());
	else
		DrawParticles(pRenderObject, commandInterface, context.pRenderView->GetFrameId());
}

CDeviceGraphicsPSOPtr CREParticle::GetGraphicsPSO(CRenderObject* pRenderObject, const struct SGraphicsPipelinePassContext& context) const
{
	assert(pRenderObject->GetObjData());
	const CLightVolumeBuffer& lightVolumes = gcpRendD3D.GetGraphicsPipeline().GetLightVolumeBuffer();
	const SRenderObjData& objectData = *pRenderObject->GetObjData();
	const uint lightVolumeId = objectData.m_LightVolumeId - 1;
	const bool isDebug = context.stageID == eStage_SceneCustom;
	const bool isWireframe = isDebug && context.passID == CSceneCustomStage::ePass_DebugViewWireframe;
	const bool isVolFog = context.stageID == eStage_VolumetricFog;
	const bool isRecursive = context.stageID == eStage_SceneForward && context.passID == CSceneForwardStage::ePass_ForwardRecursive;

	EParticlePSOMode mode = isRecursive ? EParticlePSOMode::NoLigthingRecursive : EParticlePSOMode::NoLigthing;
	if (isWireframe)
	{
		mode = EParticlePSOMode::DebugWireframe;
	}
	else if (isDebug)
	{
		mode = EParticlePSOMode::DebugSolid;
	}
	else if (isVolFog)
	{
		mode = EParticlePSOMode::VolumetricFog;
	}
	else if (lightVolumes.HasVolumes() && (pRenderObject->m_ObjFlags & FOB_LIGHTVOLUME) && lightVolumes.HasLights(lightVolumeId))
	{
#ifndef _RELEASE
		const uint numVolumes = lightVolumes.GetNumVolumes();
		if (lightVolumeId >= numVolumes)
			CRY_ASSERT_MESSAGE(0, "Bad LightVolumeId");
#endif
		mode = isRecursive ? EParticlePSOMode::WithLightingRecursive : EParticlePSOMode::WithLighting;
	}
	auto pGraphicsPSO = m_pCompiledParticle->m_pGraphicsPSOs[uint(mode)];

	return pGraphicsPSO;
}

void CREParticle::PrepareDataToRender(CRenderView *pRenderView, CRenderObject* pRenderObject)
{
	const auto* tiledLights = gcpRendD3D.GetGraphicsPipeline().GetTiledLightVolumesStage();

	assert(pRenderObject->GetObjData());
	const SRenderObjData& objectData = *pRenderObject->GetObjData();

	SParticleInstanceCB instanceCB;
	const uint lightVolumeId = objectData.m_LightVolumeId - 1;
	const uint fogVolumeIdx = objectData.m_FogVolumeContribIdx;
	if (fogVolumeIdx != (uint16)-1)
	{
		ColorF contrib;
		pRenderView->GetFogVolumeContribution(fogVolumeIdx, contrib);
		instanceCB.m_avgFogVolumeContrib.x = contrib.r * (1 - contrib.a);
		instanceCB.m_avgFogVolumeContrib.y = contrib.g * (1 - contrib.a);
		instanceCB.m_avgFogVolumeContrib.z = contrib.b * (1 - contrib.a);
		instanceCB.m_avgFogVolumeContrib.w = contrib.a;
	}
	instanceCB.m_vertexOffset = m_nFirstVertex;
	instanceCB.m_envCubemapIndex = tiledLights->GetLightShadeIndexBySpecularTextureId(m_CustomTexBind[0]); // #PFX2_TODO these ids are going back and thorth, clean this up
	instanceCB.m_lightVolumeId = lightVolumeId;
	instanceCB.m_glowParams = m_pCompiledParticle->m_glowParams;

	m_pCompiledParticle->m_pPerDrawCB->UpdateBuffer(&instanceCB, sizeof(instanceCB));
}

void CREParticle::BindPipeline(CRenderObject* pRenderObject, CDeviceGraphicsCommandInterface& commandInterface, CDeviceGraphicsPSOPtr pGraphicsPSO)
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	const SShaderItem& shaderItem = pRenderObject->m_pCurrMaterial->GetShaderItem();
	const CShaderResources* pShaderResources = static_cast<CShaderResources*>(shaderItem.m_pShaderResources);
	const EShaderStage perDrawInlineShaderStages = (EShaderStage_Vertex | EShaderStage_Domain | EShaderStage_Pixel);

	commandInterface.SetPipelineState(pGraphicsPSO.get());
	commandInterface.SetResources(EResourceLayoutSlot_PerMaterialRS, pShaderResources->m_pCompiledResourceSet.get());
	commandInterface.SetResources(EResourceLayoutSlot_PerDrawExtraRS, m_pCompiledParticle->m_pPerDrawExtraRS.get());

	commandInterface.SetInlineConstantBuffer(EResourceLayoutSlot_PerDrawCB, m_pCompiledParticle->m_pPerDrawCB, eConstantBufferShaderSlot_PerDraw, perDrawInlineShaderStages);
}

void CREParticle::DrawParticles(CRenderObject* pRenderObject, CDeviceGraphicsCommandInterface& commandInterface, int frameId)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	const auto& particleBuffer = gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet();

	const bool isPointSprites = (pRenderObject->m_ObjFlags & FOB_POINT_SPRITE) != 0;
	const bool isGpuParticles = m_pGpuRuntime != nullptr;
	
	if (isGpuParticles)
	{
		const uint numSprites = m_pGpuRuntime->GetNumParticles();
		commandInterface.Draw(numSprites * 6, 1, 0, 0);
	}
	else if (isPointSprites)
	{
		const uint maxNumSprites = particleBuffer.GetMaxNumSprites();
		const uint numVertices = min(uint(m_RenderVerts.aPositions.size()), maxNumSprites);
		commandInterface.SetIndexBuffer(particleBuffer.GetSpriteIndexBuffer());
		commandInterface.DrawIndexed(numVertices * 6, 1, 0, 0, 0);
	}
	else
	{
		const uint numVertices = m_RenderVerts.aPositions.size();
		commandInterface.Draw(numVertices * 2, 1, 0, 0);
	}
}

void CREParticle::DrawParticlesLegacy(CRenderObject* pRenderObject, CDeviceGraphicsCommandInterface& commandInterface, int frameId)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	const auto& particleBuffer = gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet();

	const bool isPointSprites = (pRenderObject->m_ObjFlags & FOB_POINT_SPRITE) != 0;
	const bool isOctagonal = (pRenderObject->m_ObjFlags & FOB_OCTAGONAL) != 0;
	const bool isVolumeFog = (pRenderObject->m_ParticleObjFlags & CREParticle::ePOF_VOLUME_FOG) != 0;
	const bool isTessellated = !isVolumeFog && (pRenderObject->m_ObjFlags & FOB_ALLOW_TESSELLATION) != 0;
	
	commandInterface.SetVertexBuffers(1, 0, particleBuffer.GetVertexStream(frameId));
	commandInterface.SetIndexBuffer(particleBuffer.GetIndexStream(frameId));

	const uint numVertices = m_RenderVerts.aVertices.size();
	const uint numIndices = m_RenderVerts.aIndices.size();
	if (isTessellated && isPointSprites)
	{
		commandInterface.Draw(numVertices, 1, m_nFirstVertex, 0);
	}
	else if (isPointSprites)
	{
		const uint verticesPerInstance = isOctagonal ? 8 : 4;
		commandInterface.Draw(verticesPerInstance, numVertices, 0, m_nFirstVertex);
	}
	else
	{
		commandInterface.DrawIndexed(numIndices, 1, m_nFirstIndex, m_nFirstVertex, 0);
	}
}
