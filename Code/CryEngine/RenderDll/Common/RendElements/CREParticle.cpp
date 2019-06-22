// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/RenderElements/CREParticle.h>
#include <CryParticleSystem/IParticles.h>
#include "CompiledRenderObject.h"
#include "GraphicsPipeline/SceneForward.h"
#include "GraphicsPipeline/ShadowMap.h"
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
	NoLighting,
	WithLighting,
	NoLightingRecursive,
	WithLightingRecursive,
	DebugSolid,
	DebugWireframe,
	VolumetricFog,
	Mobile,
	Count
};

class CCompiledParticle
{
public:
	CCompiledParticle()
	{
		m_pPerDrawCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SParticleInstanceCB), true);
		m_pShaderDataCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SParticleShaderData));
		m_pPerDrawExtraRS = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);

		if (m_pPerDrawCB) m_pPerDrawCB->SetDebugName("Particle Per-Draw CB");
		if (m_pShaderDataCB) m_pShaderDataCB->SetDebugName("Particle Per-Draw Extra CB");
	}

// private:
	DevicePipelineStatesArray m_StandardGraphicsPSOs[eStage_SCENE_NUM];
	CDeviceGraphicsPSOPtr     m_pGraphicsPSOs[uint(EParticlePSOMode::Count)];
	CDeviceResourceSetPtr     m_pPerDrawExtraRS;
	CConstantBufferPtr        m_pPerDrawCB;
	CConstantBufferPtr        m_pShaderDataCB;
	Vec4                      m_glowParams = Vec4(ZERO);
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
	, m_bCompiled(false)
{
	mfSetType(eDATA_Particle);
	mfUpdateFlags(FCEF_KEEP_DISTANCE);
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
	auto& particleBuffer = gcpRendD3D.GetParticleBufferSet();

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
	auto& particleBuffer = gcpRendD3D.GetParticleBufferSet();

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
	auto& particleBuffer = gcpRendD3D.GetParticleBufferSet();

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

	const auto& particleBuffer = gcpRendD3D.GetParticleBufferSet();

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

	for (const auto& job : aJobs)
	{
		// Generate the RenderItem for this particle container
		SShaderItem shaderItem = *job.pShaderItem;
		CRenderObject* pRenderObject = job.pRenderObject;

		const bool isRefractive = shaderItem.m_pShader && (((CShader*)shaderItem.m_pShader)->m_Flags & EF_REFRACTIVE);
		const bool refractionEnabled = CV_r_Refraction && CV_r_ParticlesRefraction;
		if (isRefractive && !refractionEnabled)
			continue;

		CREParticle* pRE = static_cast<CREParticle*>(pRenderObject->m_pRE);

		// Clamp AABB
		AABB aabb;
		pRE->mfGetBBox(aabb);
		if (aabb.IsReset())
			aabb = AABB{ .0f };
		if (pRenderObject->m_pCompiledObject)
			pRenderObject->m_pCompiledObject->m_aabb = aabb;

		if (job.pCamera)
			camInfo.pCamera = job.pCamera;
		camInfo.bShadowPass = job.pCamera != &passInfo.GetCamera();

		// generate the RenderItem entries for this Particle Element
		assert(pRenderObject->m_bPermanent);
		CPermanentRenderObject* RESTRICT_POINTER pPermanentRendObj = static_cast<CPermanentRenderObject*>(pRenderObject);
		auto renderPassType = camInfo.bShadowPass ? CPermanentRenderObject::eRenderPass_Shadows : CPermanentRenderObject::eRenderPass_General;
		if (!pPermanentRendObj->m_permanentRenderItems[renderPassType].size())
		{
			uint32 nBatchFlags;
			ERenderListID nList;
			EF_GetParticleListAndBatchFlags(nBatchFlags, nList, pRenderObject, shaderItem);
			pPermanentRendObj->AddRenderItem(pRE, shaderItem, aabb, pRenderObject->m_fDistance, nBatchFlags, nList, renderPassType);
		}

		if (!pRE->m_CustomTexBind[0])
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
				gEnv->pJobManager->AddLambdaJob("job:Particles:ComputeVertices", job, JobManager::eLowPriority, &m_ComputeVerticesJobState);
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
void CRenderer::EF_GetParticleListAndBatchFlags(uint32& nBatchFlags, ERenderListID& nList, const CRenderObject* pRenderObject, const SShaderItem& shaderItem)
{
	nBatchFlags = FB_GENERAL;
	nBatchFlags |= (pRenderObject->m_ObjFlags & FOB_AFTER_WATER) ? 0 : FB_BELOW_WATER;

	const uint8 uHalfResBlend = CV_r_ParticlesHalfResBlendMode ? OS_ADD_BLEND : OS_ALPHA_BLEND;
	bool bHalfRes = (CV_r_ParticlesHalfRes + (pRenderObject->m_ObjFlags & FOB_HALF_RES)) >= 2   // cvar allows or forces half-res
	                && pRenderObject->m_RState & uHalfResBlend
	                && pRenderObject->m_ObjFlags & FOB_AFTER_WATER;

	const bool isVolumeFog = (pRenderObject->m_ObjFlags & FOB_VOLUME_FOG) != 0;
	const bool isNearest = (pRenderObject->m_ObjFlags & FOB_NEAREST) != 0;

	if (shaderItem.m_pShader && (((CShader*)shaderItem.m_pShader)->m_Flags & EF_REFRACTIVE))
	{
		nBatchFlags |= FB_TRANSPARENT;
		if (CRenderer::CV_r_Refraction)
			nBatchFlags |= FB_REFRACTION;

		bHalfRes = false;
	}

	if (SShaderTechnique* pTech = shaderItem.GetTechnique())
	{
		if (pTech->m_nTechnique[TTYPE_CUSTOMRENDERPASS] > 0)
		{
			if (m_nThermalVisionMode && pRenderObject->GetObjData()->m_nVisionParams)
			{
				nBatchFlags |= FB_CUSTOM_RENDER;
				bHalfRes = false;
			}
		}
	}

	if (bHalfRes)
		nList = EFSLIST_HALFRES_PARTICLES;
	else if (isVolumeFog)
		nList = EFSLIST_FOG_VOLUME;
	else if (pRenderObject->m_ObjFlags & FOB_ZPREPASS)
	{
		nList = isNearest ? EFSLIST_FORWARD_OPAQUE_NEAREST : EFSLIST_FORWARD_OPAQUE;
		nBatchFlags |= FB_TILED_FORWARD | FB_ZPREPASS;
	}
	else
	{
		nList = isNearest ? EFSLIST_TRANSP_NEAREST :
			nBatchFlags & FB_BELOW_WATER ? EFSLIST_TRANSP_BW : EFSLIST_TRANSP_AW;
	}
}

bool CREParticle::Compile(CRenderObject* pRenderObject, uint64 objFlags, ERenderElementFlags elmFlags, const AABB& localAABB, CRenderView* pRenderView, bool updateInstanceDataOnly)
{
	if (m_bCompiled && updateInstanceDataOnly)
	{
		// Fast path
		PrepareDataToRender(pRenderView, pRenderObject);
		// NOTE: Works only because CB's resource state doesn't change (is reverted or UPLOAD-heap)
		return true;
	}

	const bool isPulledVertices = (pRenderObject->m_ObjFlags & FOB_VERTEX_PULL_MODEL) != 0;
	const bool isPointSprites = (objFlags & FOB_POINT_SPRITE) != 0;
	const bool isVolumeFog = (pRenderObject->m_ObjFlags & FOB_VOLUME_FOG) != 0;
	const bool isTessellated = (pRenderObject->m_ObjFlags & FOB_ALLOW_TESSELLATION) != 0;
	const bool isGpuParticles = m_pGpuRuntime != nullptr;

	CGraphicsPipeline* pGraphicsPipeline = pRenderView->GetGraphicsPipeline().get();
	auto& coreCommandList = GetDeviceObjectFactory().GetCoreCommandList();
	auto& commandInterface = *coreCommandList.GetGraphicsInterface();
	const SShaderItem& shaderItem = pRenderObject->m_pCurrMaterial->GetShaderItem();
	CShaderResources* pShaderResources = static_cast<CShaderResources*>(shaderItem.m_pShaderResources);

	enum class ETechnique
	{
		None = -1,
		Vertex,
		VertexTess,
		Pulled,
		PulledTess,
		GPU,
		VolFog,
		ZPass,
		ZPassTess,
	};

	ETechnique techniqueId = ETechnique::None;

	SGraphicsPipelineStateDescription stateDesc;
	stateDesc.shaderItem = shaderItem;
	stateDesc.technique = TTYPE_GENERAL;
	stateDesc.objectFlags = objFlags;
	stateDesc.renderState = pRenderObject->m_RState;

	if (isGpuParticles)
	{
		techniqueId = ETechnique::GPU;
		stateDesc.vertexFormat = EDefaultInputLayouts::Empty;
		stateDesc.primitiveType = eptTriangleList;
	}
	else if (isPulledVertices)
	{
		techniqueId = ETechnique::Pulled;
		stateDesc.vertexFormat = EDefaultInputLayouts::Empty;
		if (isPointSprites)
		{
			stateDesc.primitiveType = eptTriangleList;
		}
		else if (isTessellated)
		{
			techniqueId = ETechnique::PulledTess;
			stateDesc.primitiveType = ept4ControlPointPatchList;
		}
		else
		{
			stateDesc.primitiveType = eptTriangleStrip;
		}
	}
	else
	{
		stateDesc.vertexFormat = EDefaultInputLayouts::P3F_C4B_T4B_N3F2;
		if (isTessellated)
		{
			techniqueId = ETechnique::VertexTess;
			if (isPointSprites)
				stateDesc.primitiveType = ept1ControlPointPatchList;
			else
				stateDesc.primitiveType = ept4ControlPointPatchList;
		}
		else
		{
			techniqueId = ETechnique::Vertex;
			if (isPointSprites)
			{
				stateDesc.primitiveType = eptTriangleStrip;
				stateDesc.streamMask |= VSM_INSTANCED;
			}
			else
				stateDesc.primitiveType = eptTriangleList;
		}
	}
	assert(techniqueId != ETechnique::None);
	stateDesc.shaderItem.m_nTechnique = (int)techniqueId;

	if (isPointSprites)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_SPRITE];
	if (!isTessellated)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_NO_TESSELLATION];
	if (objFlags & FOB_INSHADOW)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW];
	if (objFlags & FOB_NEAREST)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_NEAREST];
	if (objFlags & FOB_SOFT_PARTICLE)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_SOFT_PARTICLE];
	if (objFlags & FOB_ALPHATEST)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_ALPHATEST];
	if (pRenderObject->m_RState & OS_ALPHA_BLEND)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_ALPHABLEND];
	if (pRenderObject->m_RState & OS_ANIM_BLEND)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_ANIM_BLEND];
	if (pRenderObject->m_RState & OS_ENVIRONMENT_CUBEMAP)
		stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_ENVIRONMENT_CUBEMAP];
	if (!(objFlags & FOB_NO_FOG))
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
			if (bDepthFixup && CRendererCVars::CV_r_HDRTexFormat)
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
		case 0:
			psoDesc.m_RenderState |= GS_BLSRC_ONE | GS_BLDST_ZERO;
			break;
		}

		if (desc.renderState & OS_NODEPTH_TEST)
			psoDesc.m_RenderState |= GS_NODEPTHTEST;

		if (bDepthFixup && !CRendererCVars::CV_r_HDRTexFormat)
			psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
	};

	bool bCompiled = true;

#if RENDERER_ENABLE_FULL_PIPELINE
	bCompiled &= pGraphicsPipeline->GetStage<CSceneForwardStage>()->CreatePipelineState(stateDesc, pGraphicsPSO, CSceneForwardStage::ePass_Forward, customForwardState);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::NoLighting)] = pGraphicsPSO;

	stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	bCompiled &= pGraphicsPipeline->GetStage<CSceneForwardStage>()->CreatePipelineState(stateDesc, pGraphicsPSO, CSceneForwardStage::ePass_Forward, customForwardState);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::WithLighting)] = pGraphicsPSO;

	stateDesc.objectRuntimeMask &= ~g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG]; // volumetric fog supports only general pass currently.
	stateDesc.objectRuntimeMask &= ~g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	bCompiled &= pGraphicsPipeline->GetStage<CSceneForwardStage>()->CreatePipelineState(stateDesc, pGraphicsPSO, CSceneForwardStage::ePass_ForwardRecursive, customForwardState);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::NoLightingRecursive)] = pGraphicsPSO;

	stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	bCompiled &= pGraphicsPipeline->GetStage<CSceneForwardStage>()->CreatePipelineState(stateDesc, pGraphicsPSO, CSceneForwardStage::ePass_ForwardRecursive, customForwardState);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::WithLightingRecursive)] = pGraphicsPSO;

	stateDesc.objectRuntimeMask &= ~g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	stateDesc.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_DEBUG0];
	stateDesc.objectRuntimeMask &= ~g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW];
	bCompiled &= pGraphicsPipeline->GetStage<CSceneCustomStage>()->CreatePipelineState(stateDesc, CSceneCustomStage::ePass_DebugViewSolid, pGraphicsPSO);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::DebugSolid)] = pGraphicsPSO;

	bCompiled &= pGraphicsPipeline->GetStage<CSceneCustomStage>()->CreatePipelineState(stateDesc, CSceneCustomStage::ePass_DebugViewWireframe, pGraphicsPSO);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::DebugWireframe)] = pGraphicsPSO;

	if (pRenderObject->m_ObjFlags & FOB_ZPREPASS)
	{
		stateDesc.shaderItem.m_nTechnique = (int)(isTessellated ? ETechnique::ZPassTess : ETechnique::ZPass);
		bCompiled &= pGraphicsPipeline->GetStage<CSceneGBufferStage>()->CreatePipelineState(stateDesc, CSceneGBufferStage::ePass_DepthBufferFill, pGraphicsPSO);
		m_pCompiledParticle->m_StandardGraphicsPSOs[eStage_SceneGBuffer][CSceneGBufferStage::ePass_DepthBufferFill] = pGraphicsPSO;

		CShaderResources* RESTRICT_POINTER pResources = static_cast<CShaderResources*>(shaderItem.m_pShaderResources);
 		bCompiled &= pGraphicsPipeline->GetStage<CShadowMapStage>()->CreatePipelineStates(m_pCompiledParticle->m_StandardGraphicsPSOs, stateDesc, pResources->m_pipelineStateCache.get());
	}
#endif

#if RENDERER_ENABLE_MOBILE_PIPELINE
	stateDesc.objectRuntimeMask &= ~(g_HWSR_MaskBit[HWSR_LIGHTVOLUME0] | g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG] | g_HWSR_MaskBit[HWSR_DEBUG0]);
	bCompiled &= pGraphicsPipeline->GetStage<CSceneForwardStage>()->CreatePipelineState(stateDesc, pGraphicsPSO, CSceneForwardStage::ePass_ForwardMobile, customForwardState);
	m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::Mobile)] = pGraphicsPSO;
#endif

	if (isVolumeFog)
	{
		stateDesc.objectRuntimeMask &= ~(g_HWSR_MaskBit[HWSR_LIGHTVOLUME0] | g_HWSR_MaskBit[HWSR_DEBUG0]); // remove unneeded flags.
		stateDesc.shaderItem.m_nTechnique = (int)ETechnique::VolFog; // particles are always rendered with vol fog technique in vol fog pass.
		bCompiled &= pGraphicsPipeline->GetStage<CVolumetricFogStage>()->CreatePipelineState(stateDesc, pGraphicsPSO);
		m_pCompiledParticle->m_pGraphicsPSOs[uint(EParticlePSOMode::VolumetricFog)] = pGraphicsPSO;
	}

	m_bCompiled = bCompiled;
	if (!bCompiled)
		return false;

	const ColorF glowParam = pShaderResources->GetFinalEmittance();
	const SRenderObjData& objectData = *pRenderObject->GetObjData();
	m_pCompiledParticle->m_glowParams = Vec4(glowParam.r, glowParam.g, glowParam.b, 0.0f);

	m_pCompiledParticle->m_pShaderDataCB->UpdateBuffer(objectData.m_pParticleShaderData, sizeof(*objectData.m_pParticleShaderData));

	CDeviceResourceSetDesc perInstanceExtraResources(CSceneRenderPass::GetDefaultDrawExtraResourceLayout(), nullptr, nullptr);

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

	const EShaderStage perDrawInlineShaderStages = (EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain);

	commandInterface.PrepareResourcesForUse(EResourceLayoutSlot_PerMaterialRS, pShaderResources->m_pCompiledResourceSet.get());
	commandInterface.PrepareResourcesForUse(EResourceLayoutSlot_PerDrawExtraRS, m_pCompiledParticle->m_pPerDrawExtraRS.get());

	commandInterface.PrepareInlineConstantBufferForUse(EResourceLayoutSlot_PerDrawCB, m_pCompiledParticle->m_pPerDrawCB, eConstantBufferShaderSlot_PerDraw, perDrawInlineShaderStages);

	return true;
}

void CREParticle::DrawToCommandList(CRenderObject* pRenderObject, const struct SGraphicsPipelinePassContext& context, CDeviceCommandList* commandList)
{
	const SShaderItem& shaderItem = pRenderObject->m_pCurrMaterial->GetShaderItem();
	const CShaderResources* pShaderResources = static_cast<CShaderResources*>(shaderItem.m_pShaderResources);

	const bool bIncompleteResourceSets =
		!pShaderResources->m_pCompiledResourceSet || !pShaderResources->m_pCompiledResourceSet->IsValid() ||
		!m_pCompiledParticle->m_pPerDrawExtraRS || !m_pCompiledParticle->m_pPerDrawExtraRS->IsValid();

#if defined(ENABLE_PROFILING_CODE)
	if (!m_bCompiled || bIncompleteResourceSets)
	{
		CryInterlockedIncrement(&SRenderStatistics::Write().m_nIncompleteCompiledObjects);
	}
#endif

	auto pGraphicsPSO = GetGraphicsPSO(pRenderObject, context);
	if (!pGraphicsPSO || !pGraphicsPSO->IsValid() || bIncompleteResourceSets)
		return;

	gRenDev->m_FillRateManager.AddPixelCount(m_RenderVerts.fPixels);

	if (m_pGpuRuntime == nullptr && m_RenderVerts.aPositions.empty() && m_RenderVerts.aVertices.empty())
		return;

	const bool isLegacy = m_pGpuRuntime == nullptr && (pRenderObject->m_ObjFlags & FOB_VERTEX_PULL_MODEL) == 0;

	CDeviceGraphicsCommandInterface& commandInterface = *commandList->GetGraphicsInterface();
	BindPipeline(pRenderObject, commandInterface, pGraphicsPSO);
	if (isLegacy)
		DrawParticlesLegacy(pRenderObject, commandInterface, context.pRenderView->GetFrameId());
	else
		DrawParticles(pRenderObject, commandInterface, context.pRenderView->GetFrameId());
}

CDeviceGraphicsPSOPtr CREParticle::GetGraphicsPSO(CRenderObject* pRenderObject, const struct SGraphicsPipelinePassContext& context) const
{
	EParticlePSOMode mode;
	if (context.stageID == eStage_SceneGBuffer && context.passID == CSceneGBufferStage::ePass_DepthBufferFill)
	{
		return m_pCompiledParticle->m_StandardGraphicsPSOs[eStage_SceneGBuffer][CSceneForwardStage::ePass_ForwardPrepassed];
	}
	else if (context.stageID == eStage_ShadowMap)
	{
		return m_pCompiledParticle->m_StandardGraphicsPSOs[eStage_ShadowMap][context.passID];
	}
	else if (context.stageID == eStage_SceneForward)
	{
		if (context.passID == CSceneForwardStage::ePass_ForwardMobile)
		{
			mode = EParticlePSOMode::Mobile;
		}
		else
		{
			assert(pRenderObject->GetObjData());
			const bool isRecursive = context.passID == CSceneForwardStage::ePass_ForwardRecursive;
			CGraphicsPipeline* pGraphicsPipeline = context.pRenderView->GetGraphicsPipeline().get();
			CTiledLightVolumesStage* pLightVolumeStage = pGraphicsPipeline->GetStage<CTiledLightVolumesStage>();
			CRY_ASSERT(pLightVolumeStage);
			
			const CLightVolumeBuffer& lightVolumes = pLightVolumeStage->GetLightVolumeBuffer();
			const int lightVolumeId = pRenderObject->GetObjData()->m_LightVolumeId - 1;
			if ((pRenderObject->m_ObjFlags & FOB_LIGHTVOLUME) && lightVolumes.HasVolumes() && lightVolumeId >= 0 && lightVolumes.HasLights(lightVolumeId))
			{
#ifndef _RELEASE
				CRY_ASSERT(lightVolumeId < lightVolumes.GetNumVolumes());
#endif
				mode = isRecursive ? EParticlePSOMode::WithLightingRecursive : EParticlePSOMode::WithLighting;
			}
			else
			{
				mode = isRecursive ? EParticlePSOMode::NoLightingRecursive : EParticlePSOMode::NoLighting;
			}	
		}
	}
	else if (context.stageID == eStage_VolumetricFog)
	{
		mode = EParticlePSOMode::VolumetricFog;
	}
	else if (context.stageID == eStage_SceneCustom)
	{
		if (context.passID == CSceneCustomStage::ePass_DebugViewWireframe)
			mode = EParticlePSOMode::DebugWireframe;
		else
			mode = EParticlePSOMode::DebugSolid;
	}
	else
		return nullptr;

	return m_pCompiledParticle->m_pGraphicsPSOs[uint(mode)];
}

void CREParticle::PrepareDataToRender(CRenderView* pRenderView, CRenderObject* pRenderObject)
{
	const auto* tiledLights = pRenderView->GetGraphicsPipeline()->GetStage<CTiledLightVolumesStage>();

	assert(pRenderObject->GetObjData());
	const SRenderObjData& objectData = *pRenderObject->GetObjData();

	SParticleInstanceCB instanceCB;
	if (objectData.m_FogVolumeContribIdx)
	{
		ColorF contrib;
		pRenderView->GetFogVolumeContribution(objectData.m_FogVolumeContribIdx, contrib);
		instanceCB.m_avgFogVolumeContrib.x = contrib.r * (1 - contrib.a);
		instanceCB.m_avgFogVolumeContrib.y = contrib.g * (1 - contrib.a);
		instanceCB.m_avgFogVolumeContrib.z = contrib.b * (1 - contrib.a);
		instanceCB.m_avgFogVolumeContrib.w = contrib.a;
	}
	instanceCB.m_vertexOffset = m_nFirstVertex;
	instanceCB.m_envCubemapIndex = tiledLights->GetLightShadeIndexBySpecularTextureId(m_CustomTexBind[0]); // #PFX2_TODO these ids are going back and thorth, clean this up
	instanceCB.m_lightVolumeId = objectData.m_LightVolumeId - 1;
	instanceCB.m_glowParams = m_pCompiledParticle->m_glowParams;

	m_pCompiledParticle->m_pPerDrawCB->UpdateBuffer(&instanceCB, sizeof(instanceCB));
}

void CREParticle::BindPipeline(CRenderObject* pRenderObject, CDeviceGraphicsCommandInterface& commandInterface, CDeviceGraphicsPSOPtr pGraphicsPSO)
{
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
	const auto& particleBuffer = gcpRendD3D.GetParticleBufferSet();

	const bool isPointSprites = (pRenderObject->m_ObjFlags & FOB_POINT_SPRITE) != 0;
	const bool isTessellated = (pRenderObject->m_ObjFlags & FOB_ALLOW_TESSELLATION) != 0;
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
		if (numVertices > 1)
		{
			if (isTessellated)
				commandInterface.Draw((numVertices - 1) * 4, 1, 0, 0);
			else
				commandInterface.Draw(numVertices * 2, 1, 0, 0);
		}
	}
}

void CREParticle::DrawParticlesLegacy(CRenderObject* pRenderObject, CDeviceGraphicsCommandInterface& commandInterface, int frameId)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	const auto& particleBuffer = gcpRendD3D.GetParticleBufferSet();

	// Disable vertex instancing on unsupported hardware, or from cvar.
#if CRY_PLATFORM_ORBIS
	const bool canInstance = false;
#else
	const bool canInstance = (CRendererCVars::CV_r_ParticlesInstanceVertices != 0);
#endif
	const bool isPointSprites = (pRenderObject->m_ObjFlags & FOB_POINT_SPRITE) != 0;
	const bool isTessellated = (pRenderObject->m_ObjFlags & FOB_ALLOW_TESSELLATION) != 0;

	const auto vertexStream = particleBuffer.GetVertexStream(frameId);
	const auto indexStream = particleBuffer.GetIndexStream(frameId);
	if (!indexStream || !vertexStream)
		return;

	commandInterface.SetVertexBuffers(1, 0, vertexStream);
	commandInterface.SetIndexBuffer(indexStream);

	const uint numVertices = m_RenderVerts.aVertices.size();
	const uint numIndices = m_RenderVerts.aIndices.size();
	if (isTessellated && isPointSprites)
	{
		commandInterface.Draw(numVertices, 1, m_nFirstVertex, 0);
	}
	else if (isPointSprites && canInstance)
	{
		const bool isOctagonal = (pRenderObject->m_ObjFlags & FOB_OCTAGONAL) != 0;
		const uint verticesPerInstance = isOctagonal ? 8 : 4;
		commandInterface.Draw(verticesPerInstance, numVertices, 0, m_nFirstVertex);
	}
	else
	{
		commandInterface.DrawIndexed(numIndices, 1, m_nFirstIndex, m_nFirstVertex, 0);
	}
}
