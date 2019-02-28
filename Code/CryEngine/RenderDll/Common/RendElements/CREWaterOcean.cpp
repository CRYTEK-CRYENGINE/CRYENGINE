// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/RenderElements/CREWaterOcean.h>

#include <Cry3DEngine/I3DEngine.h>
#include "XRenderD3D9/DriverD3D.h"

#include "GraphicsPipeline/Water.h"
#include "Common/ReverseDepth.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////

namespace water
{
const buffer_handle_t invalidBufferHandle = ~0u;

const uint32 passIdUnderWater = CWaterStage::ePass_WaterSurface;
const uint32 passIdAboveWater = CWaterStage::ePass_CausticsGen;

// per instance texture slot for water ocean
enum EPerInstanceTexture
{
	ePerInstanceTexture_OceanFoam         = CWaterStage::ePerInstanceTexture_Foam,
	ePerInstanceTexture_OceanDisplacement = CWaterStage::ePerInstanceTexture_Displacement,
	ePerInstanceTexture_OceanReflection   = CWaterStage::ePerInstanceTexture_RainRipple,

	ePerInstanceTexture_Count
};
static_assert(int32(ePerInstanceTexture_Count) == int32(CWaterStage::ePerInstanceTexture_Count), "Per instance texture count must be same in water stage to ensure using same resource layout.");

struct SPerDrawConstantBuffer
{
	Matrix44 mReflProj;

	Vec4     OceanParams0;
	Vec4     OceanParams1;
	Vec4     cOceanFogColorDensity;
};

struct SCompiledWaterOcean : NoCopy
{
	SCompiledWaterOcean()
		: m_pPerDrawCB(nullptr)
		, m_vertexStreamSet(nullptr)
		, m_indexStreamSet(nullptr)
		, m_nVerticesCount(0)
		, m_nStartIndex(0)
		, m_nNumIndices(0)
		, m_bValid(0)
		, m_bHasTessellation(0)
		, m_bAboveWater(0)
		, m_reserved(0)
	{}

	CDeviceResourceSetPtr     m_pMaterialResourceSet;
	CDeviceResourceSetPtr     m_pPerDrawRS;
	CConstantBufferPtr        m_pPerDrawCB;

	const CDeviceInputStream* m_vertexStreamSet;
	const CDeviceInputStream* m_indexStreamSet;

	int32                     m_nVerticesCount;
	int32                     m_nStartIndex;
	int32                     m_nNumIndices;

	DevicePipelineStatesArray m_psoArray;

	uint32                    m_bValid           : 1; //!< True if compilation succeeded.
	uint32                    m_bHasTessellation : 1; //!< True if tessellation is enabled
	uint32                    m_bAboveWater      : 1; //!< True if camera is above the water ocean.
	uint32                    m_reserved         : 29;
};

bool CreatePipelineStates(
  DevicePipelineStatesArray& stateArray,
  CGraphicsPipelineStateLocalCache* pStateCache,
  const SGraphicsPipelineStateDescription& stateDesc,
  CWaterStage& waterStage)
{
	if (pStateCache->Find(stateDesc, stateArray))
		return true;

	bool bFullyCompiled = true;

	SGraphicsPipelineStateDescription _stateDesc = stateDesc;

	_stateDesc.technique = TTYPE_GENERAL;
	bFullyCompiled &= waterStage.CreatePipelineState(stateArray[passIdUnderWater], _stateDesc, CWaterStage::ePass_WaterSurface, nullptr);

	_stateDesc.technique = TTYPE_GENERAL;
	auto modifyRenderState = [](CDeviceGraphicsPSODesc& psoDesc)
	{
		psoDesc.m_RenderState &= ~GS_DEPTHFUNC_MASK;
		psoDesc.m_RenderState |= GS_DEPTHWRITE | GS_DEPTHFUNC_LEQUAL;
	};
	bFullyCompiled &= waterStage.CreatePipelineState(stateArray[passIdAboveWater], _stateDesc, CWaterStage::ePass_WaterSurface, modifyRenderState);

	_stateDesc.technique = TTYPE_GENERAL;
	auto modifyRenderStateMaskGen = [](CDeviceGraphicsPSODesc& psoDesc)
	{
		psoDesc.m_RenderState &= ~GS_DEPTHFUNC_MASK;
		psoDesc.m_RenderState |= GS_DEPTHFUNC_GREAT;
	};
	bFullyCompiled &= waterStage.CreatePipelineState(stateArray[CWaterStage::ePass_OceanMaskGen], _stateDesc, CWaterStage::ePass_OceanMaskGen, modifyRenderStateMaskGen);

	if (bFullyCompiled)
	{
		pStateCache->Put(stateDesc, stateArray);
	}

	return bFullyCompiled;
}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

CREWaterOcean::CREWaterOcean()
	: CRenderElement()
	, m_pCompiledObject(new water::SCompiledWaterOcean())
	, m_pRenderTarget(new SHRenderTarget())
	, m_vertexBufferHandle(water::invalidBufferHandle)
	, m_indexBufferHandle(water::invalidBufferHandle)
	, m_nVerticesCount(0)
	, m_nIndicesCount(0)
	, m_nIndexSizeof(0)
{
	mfSetType(eDATA_WaterOcean);
	mfUpdateFlags(FCEF_TRANSFORM);

	m_pRenderTarget->m_eOrder = eRO_PreProcess;
	m_pRenderTarget->m_nProcessFlags = FSPR_SCANTEXWATER;
	m_pRenderTarget->m_nFlags |= FRT_CAMERA_REFLECTED_WATERPLANE;
	m_pRenderTarget->m_bTempDepth = true;
	m_pRenderTarget->m_nIDInPool = 0; // _RT2D_WATER_ID=0 is defined in Common.cfi.
	CRY_ASSERT(m_pRenderTarget->m_nIDInPool >= 0 && m_pRenderTarget->m_nIDInPool < 64);
	m_pRenderTarget->m_nWidth = -1;
	m_pRenderTarget->m_nHeight = -1;
	m_pRenderTarget->m_eUpdateType = eRTUpdate_WaterReflect;
	m_pRenderTarget->m_ClearColor = ColorF(0.0f);
	m_pRenderTarget->m_fClearDepth = 1.0f;
	m_pRenderTarget->m_nFlags |= FRT_CLEAR_COLOR | FRT_CLEAR_DEPTH;

	if (m_pRenderTarget->m_nIDInPool >= 0)
	{
		if ((int)CRendererResources::s_CustomRT_2D.Num() <= m_pRenderTarget->m_nIDInPool)
			CRendererResources::s_CustomRT_2D.Expand(m_pRenderTarget->m_nIDInPool + 1);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

CREWaterOcean::~CREWaterOcean()
{
	ReleaseOcean();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::mfGetPlane(Plane& pl)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

Vec4* CREWaterOcean::GetDisplaceGrid() const
{
	//assert( m_pWaterSim );
	if (WaterSimMgr())
		return WaterSimMgr()->GetDisplaceGrid();

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

SHRenderTarget* CREWaterOcean::GetReflectionRenderTarget()
{
	return m_pRenderTarget.get();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CREWaterOcean::RequestVerticesBuffer(SVF_P3F_C4B_T2F** pOutputVertices, uint8** pOutputIndices, uint32 nVerticesCount, uint32 nIndicesCount, uint32 nIndexSizeof)
{
	if (!pOutputVertices || !pOutputIndices)
	{
		return false;
	}

	if (!nVerticesCount || !nIndicesCount || (nIndexSizeof != 2 && nIndexSizeof != 4))
	{
		pOutputVertices = nullptr;
		pOutputIndices = nullptr;

		return false;
	}

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	auto nThreadID = gRenDev->GetMainThreadID();
	CRY_ASSERT(rd->m_pRT->IsMainThread());

	*pOutputVertices = new SVF_P3F_C4B_T2F[nVerticesCount];
	*pOutputIndices = new uint8[nIndicesCount * nIndexSizeof];

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CREWaterOcean::SubmitVerticesBuffer(uint32 nVerticesCount, uint32 nIndicesCount, uint32 nIndexSizeof, SVF_P3F_C4B_T2F* pVertices, uint8* pIndices)
{
	if (!pVertices || !pIndices)
	{
		SAFE_DELETE_ARRAY(pVertices);
		SAFE_DELETE_ARRAY(pIndices);

		return false;
	}

	if (!nVerticesCount || !nIndicesCount || (nIndexSizeof != 2 && nIndexSizeof != 4))
	{
		SAFE_DELETE_ARRAY(pVertices);
		SAFE_DELETE_ARRAY(pIndices);

		return false;
	}

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	auto nThreadID = gRenDev->GetMainThreadID();
	CRY_ASSERT(rd->m_pRT->IsMainThread());

	auto& requests = m_verticesUpdateRequests[nThreadID];

	requests.emplace_back();
	auto& req = requests.back();

	req.nVerticesCount = nVerticesCount;
	req.pVertices = pVertices;
	req.nIndicesCount = nIndicesCount;
	req.pIndices = pIndices;
	req.nIndexSizeof = nIndexSizeof;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::Create(uint32 nVerticesCount, SVF_P3F_C4B_T2F* pVertices, uint32 nIndicesCount, const void* pIndices, uint32 nIndexSizeof)
{
	if (!nVerticesCount || !pVertices || !nIndicesCount || !pIndices || (nIndexSizeof != 2 && nIndexSizeof != 4))
		return;

	CD3D9Renderer* rd(gcpRendD3D);
	ReleaseOcean();

	m_nVerticesCount = nVerticesCount;
	m_nIndicesCount = nIndicesCount;
	m_nIndexSizeof = nIndexSizeof;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Create vertex buffer
	//////////////////////////////////////////////////////////////////////////////////////////////////
	{
		uint32 size = nVerticesCount * sizeof(SVF_P3F_C4B_T2F);

		m_vertexBufferHandle = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, size);

		if (m_vertexBufferHandle != water::invalidBufferHandle)
		{
			gRenDev->m_DevBufMan.UpdateBuffer(m_vertexBufferHandle, pVertices, size);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Create index buffer
	//////////////////////////////////////////////////////////////////////////////////////////////////
	{
		const uint32 size = nIndicesCount * m_nIndexSizeof;

		m_indexBufferHandle = gRenDev->m_DevBufMan.Create(BBT_INDEX_BUFFER, BU_STATIC, size);

		if (m_indexBufferHandle != water::invalidBufferHandle)
		{
			gRenDev->m_DevBufMan.UpdateBuffer(m_indexBufferHandle, pIndices, size);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::CreateVertexAndIndexBuffer(threadID threadId)
{
	auto& requests = m_verticesUpdateRequests[threadId];

	if (!requests.empty())
	{
		auto& req = requests.back();

		// create vertex and index buffer if update requests exist.
		Create(req.nVerticesCount, req.pVertices, req.nIndicesCount, req.pIndices, req.nIndexSizeof);

		for (auto& req : requests)
		{
			SAFE_DELETE_ARRAY(req.pVertices);
			SAFE_DELETE_ARRAY(req.pIndices);
		}

		requests.clear();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::FrameUpdate()
{
	static bool bInitialize = true;
	static Vec4 pParams0(0, 0, 0, 0), pParams1(0, 0, 0, 0);

	Vec4 pCurrParams0, pCurrParams1;
	gEnv->p3DEngine->GetOceanAnimationParams(pCurrParams0, pCurrParams1);

	// TODO: this water sim is shared with CREWaterOcean and CWaterStage. the sim result also could be shared.
	// why no comparison operator on Vec4 ??
	if (bInitialize || pCurrParams0.x != pParams0.x || pCurrParams0.y != pParams0.y ||
	    pCurrParams0.z != pParams0.z || pCurrParams0.w != pParams0.w || pCurrParams1.x != pParams1.x ||
	    pCurrParams1.y != pParams1.y || pCurrParams1.z != pParams1.z || pCurrParams1.w != pParams1.w)
	{
		pParams0 = pCurrParams0;
		pParams1 = pCurrParams1;
		WaterSimMgr()->Create(1.0, pParams0.x, 1.0f, 1.0f);
		bInitialize = false;
	}

	const int nGridSize = 64;

	// Update Vertex Texture
	if (!CTexture::IsTextureExist(CRendererResources::s_ptexWaterOcean))
	{
		CRendererResources::s_ptexWaterOcean->Create2DTexture(nGridSize, nGridSize, 1, FT_DONT_RELEASE | FT_NOMIPS, 0, eTF_R32G32B32A32F);
	}

	CTexture* pTexture = CRendererResources::s_ptexWaterOcean;

	// Copy data..
	if (CTexture::IsTextureExist(pTexture))
	{
		const float fUpdateTime = 0.125f * gEnv->pTimer->GetCurrTime();// / clamp_tpl<float>(pParams1.x, 0.55f, 1.0f);
		int nFrameID = gRenDev->GetRenderFrameID();
		void* pRawPtr = NULL;
		WaterSimMgr()->Update(nFrameID, fUpdateTime, false, pRawPtr);

		Vec4* pDispGrid = WaterSimMgr()->GetDisplaceGrid();
		if (pDispGrid == NULL)
			return;

		const uint32 pitch = 4 * sizeof(f32) * nGridSize;
		const uint32 width = nGridSize;
		const uint32 height = nGridSize;

		CRY_PROFILE_REGION_WAITING(PROFILE_RENDERER, "update subresource");

		CDeviceTexture * pDevTex = pTexture->GetDevTexture();
		pDevTex->UploadFromStagingResource(0, [=](void* pData, uint32 rowPitch, uint32 slicePitch)
		{
			cryMemcpy(pData, pDispGrid, 4 * width * height * sizeof(f32));
			return true;
		});
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::ReleaseOcean()
{
	if (m_vertexBufferHandle != water::invalidBufferHandle)
	{
		gRenDev->m_DevBufMan.Destroy(m_vertexBufferHandle);
		m_vertexBufferHandle = water::invalidBufferHandle;
	}
	if (m_indexBufferHandle != water::invalidBufferHandle)
	{
		gRenDev->m_DevBufMan.Destroy(m_indexBufferHandle);
		m_indexBufferHandle = water::invalidBufferHandle;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CREWaterOcean::Compile(CRenderObject* pObj,CRenderView *pRenderView, bool updateInstanceDataOnly)
{
	if (!m_pCompiledObject)
	{
		return false;
	}

	auto& compiledObj = *(m_pCompiledObject);
	compiledObj.m_bValid = 0;

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	auto nThreadID = gRenDev->GetRenderThreadID();
	CRY_ASSERT(rd->m_pRT->IsRenderThread());
	auto* pWaterStage = rd->GetGraphicsPipeline().GetWaterStage();

	if (!pWaterStage
	    || !pObj
	    || !pObj->m_pCurrMaterial)
	{
		return false;
	}

	auto& shaderItem = pObj->m_pCurrMaterial->GetShaderItem(0);
	CShader* pShader = (CShader*)shaderItem.m_pShader;
	CShaderResources* RESTRICT_POINTER pResources = static_cast<CShaderResources*>(shaderItem.m_pShaderResources);

	if (!pShader
	    || pShader->m_eShaderType != eST_Water
	    || (shaderItem.m_nPreprocessFlags & (FB_WATER_REFL | FB_WATER_CAUSTIC)) != 0
	    || !pResources
	    || !pResources->m_pCompiledResourceSet
	    || !pResources->m_pipelineStateCache)
	{
		return false;
	}

	CreateVertexAndIndexBuffer(nThreadID);

	if (!m_nVerticesCount
	    || !m_nIndicesCount
	    || m_vertexBufferHandle == water::invalidBufferHandle
	    || m_indexBufferHandle == water::invalidBufferHandle)
	{
		return false;
	}

	// update ocean's displacement texture.
	if (m_oceanParam[nThreadID].bWaterOceanFFT)
	{
		FrameUpdate();
	}

	N3DEngineCommon::SOceanInfo& OceanInfo = gRenDev->m_p3DEngineCommon.m_OceanInfo;
	const bool bAboveWater = pRenderView->GetCamera(CCamera::eEye_Left).GetPosition().z > OceanInfo.m_fWaterLevel;
	compiledObj.m_bAboveWater = bAboveWater ? 1 : 0;

	const InputLayoutHandle vertexFormat =  EDefaultInputLayouts::P3F_C4B_T2F;

	// need to check mesh is ready for tessellation because m_bUseWaterTessHW is enabled but CREWaterOcean::Create() isn't called yet.
	const bool bTessellationMesh = ((m_nIndicesCount % 3) == 0);

	const bool bUseWaterTess = rd->m_bUseWaterTessHW && bTessellationMesh;
	ERenderPrimitiveType primType = bUseWaterTess ? eptTriangleList : eptTriangleStrip;
	compiledObj.m_bHasTessellation = 0;
#ifdef WATER_TESSELLATION_RENDERER
	// Enable tessellation for water geometry
	if (bUseWaterTess && SDeviceObjectHelpers::CheckTessellationSupport(shaderItem, TTYPE_GENERAL))
	{
		compiledObj.m_bHasTessellation = 1;
	}
#endif

	// NOTE: workaround for tessellation for water.
	//       FOB_ALLOW_TESSELLATION is forcibly added in CRenderer::EF_AddEf_NotVirtual() even if shader doesn't have domain and hull shaders.
	pObj->m_ObjFlags &= ~FOB_ALLOW_TESSELLATION;

	// create PSOs which match to specific material.
	SGraphicsPipelineStateDescription psoDescription(
	  pObj,
	  this,
	  shaderItem,
	  TTYPE_GENERAL, // set as default, this may be overwritten in CreatePipelineStates().
	  vertexFormat,
	  0 /*geomInfo.CalcStreamMask()*/,
	  primType // tessellation is handled in CreatePipelineStates(). ept3ControlPointPatchList is used in that case.
	  );

	// apply shader quality
	{
		const uint64 quality = g_HWSR_MaskBit[HWSR_QUALITY];
		const uint64 quality1 = g_HWSR_MaskBit[HWSR_QUALITY1];
		switch ((rd->GetShaderProfile(pShader->m_eShaderType)).GetShaderQuality())
		{
		case eSQ_Medium:
			psoDescription.objectRuntimeMask |= quality;
			break;
		case eSQ_High:
			psoDescription.objectRuntimeMask |= quality1;
			break;
		case eSQ_VeryHigh:
			psoDescription.objectRuntimeMask |= (quality | quality1);
			break;
		}
	}

	psoDescription.objectFlags |= compiledObj.m_bHasTessellation ? FOB_ALLOW_TESSELLATION : 0;

	// TODO: remove this if old graphics pipeline and material preview is removed.
	// NOTE: this is to use a typed constant buffer instead of per batch constant buffer.
	psoDescription.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_COMPUTE_SKINNING];

	// fog related runtime mask, this changes eventual PSOs.
	const bool bFog = pRenderView->IsGlobalFogEnabled();
	const bool bVolumetricFog = (rd->m_bVolumetricFogEnabled != 0);
	psoDescription.objectRuntimeMask |= bFog ? g_HWSR_MaskBit[HWSR_FOG] : 0;
	psoDescription.objectRuntimeMask |= bVolumetricFog ? g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG] : 0;

	// TODO: replace this shader flag with shader constant to avoid shader permutation after removing old graphics pipeline.
	const bool bDeferredRain = rd->m_bDeferredRainEnabled;
	psoDescription.objectRuntimeMask |= bDeferredRain ? g_HWSR_MaskBit[HWSR_OCEAN_PARTICLE] : 0;

	auto* pStateCache = pResources->m_pipelineStateCache.get();
	if (!water::CreatePipelineStates(compiledObj.m_psoArray, pStateCache, psoDescription, *pWaterStage))
	{
		if (!CRenderer::CV_r_shadersAllowCompilation)
		{
			return true;
		}
		else
		{
			return false;  // Shaders might still compile; try recompiling object later
		}
	}

	compiledObj.m_pMaterialResourceSet = pResources->m_pCompiledResourceSet;

	// UpdatePerInstanceCB uses not thread safe functions like CreateConstantBuffer(),
	// so this needs to be called here instead of DrawToCommandList().
	UpdatePerDrawCB(compiledObj, *pObj);
	UpdatePerDrawRS(compiledObj, m_oceanParam[nThreadID], *pWaterStage);

	UpdateVertex(compiledObj, primType);

	CRY_ASSERT(compiledObj.m_pMaterialResourceSet);
	CRY_ASSERT(compiledObj.m_pPerDrawCB);
	CRY_ASSERT(compiledObj.m_pPerDrawRS && compiledObj.m_pPerDrawRS->IsValid());
	CRY_ASSERT(compiledObj.m_vertexStreamSet);
	CRY_ASSERT(compiledObj.m_indexStreamSet);

	// Issue the barriers on the core command-list, which executes directly before the Draw()s in multi-threaded jobs
	PrepareForUse(compiledObj, false, GetDeviceObjectFactory().GetCoreCommandList());

	compiledObj.m_bValid = 1;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx, CDeviceCommandList* commandList)
{
	if (!m_pCompiledObject || !(m_pCompiledObject->m_bValid))
		return;

	auto& RESTRICT_REFERENCE compiledObj = *m_pCompiledObject;

#if defined(ENABLE_PROFILING_CODE)
	if (!compiledObj.m_bValid || !compiledObj.m_pMaterialResourceSet->IsValid())
	{
		CryInterlockedIncrement(&SRenderStatistics::Write().m_nIncompleteCompiledObjects);
	}
#endif

	CRY_ASSERT(ctx.stageID == eStage_Water);
	CRY_ASSERT(ctx.passID == CWaterStage::ePass_WaterSurface || ctx.passID == CWaterStage::ePass_OceanMaskGen);
	const auto passId = (ctx.passID == CWaterStage::ePass_WaterSurface)
	                    ? (compiledObj.m_bAboveWater ? water::passIdAboveWater : water::passIdUnderWater)
	                    : ctx.passID;
	const CDeviceGraphicsPSOPtr& pPso = compiledObj.m_psoArray[passId];

	if (!pPso || !pPso->IsValid() || !compiledObj.m_pMaterialResourceSet->IsValid())
		return;

	CRY_ASSERT(compiledObj.m_pPerDrawCB);
	CRY_ASSERT(compiledObj.m_pPerDrawRS && compiledObj.m_pPerDrawRS->IsValid());

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	CDeviceGraphicsCommandInterface& RESTRICT_REFERENCE commandInterface = *(commandList->GetGraphicsInterface());

	// Set states
	commandInterface.SetPipelineState(pPso.get());
	commandInterface.SetResources(EResourceLayoutSlot_PerMaterialRS, compiledObj.m_pMaterialResourceSet.get());
	commandInterface.SetResources(EResourceLayoutSlot_PerDrawExtraRS, compiledObj.m_pPerDrawRS.get());

	EShaderStage perDrawInlineShaderStages = compiledObj.m_bHasTessellation
		? (EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain)
		: (EShaderStage_Vertex | EShaderStage_Pixel);

	commandInterface.SetInlineConstantBuffer(EResourceLayoutSlot_PerDrawCB, compiledObj.m_pPerDrawCB, eConstantBufferShaderSlot_PerDraw, perDrawInlineShaderStages);

	if (CRenderer::CV_r_NoDraw != 3)
	{
		CRY_ASSERT(compiledObj.m_vertexStreamSet);
		commandInterface.SetVertexBuffers(1, 0, compiledObj.m_vertexStreamSet);

		if (compiledObj.m_indexStreamSet == nullptr)
		{
			commandInterface.Draw(compiledObj.m_nVerticesCount, 1, 0, 0);
		}
		else
		{
			commandInterface.SetIndexBuffer(compiledObj.m_indexStreamSet);
			commandInterface.DrawIndexed(compiledObj.m_nNumIndices, 1, compiledObj.m_nStartIndex, 0, 0);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::PrepareForUse(water::SCompiledWaterOcean& compiledObj, bool bInstanceOnly, CDeviceCommandList& RESTRICT_REFERENCE commandList) const
{
	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();

	if (!bInstanceOnly)
	{
		pCommandInterface->PrepareResourcesForUse(EResourceLayoutSlot_PerMaterialRS, compiledObj.m_pMaterialResourceSet.get());
	}

	pCommandInterface->PrepareResourcesForUse(EResourceLayoutSlot_PerDrawExtraRS, compiledObj.m_pPerDrawRS.get());

	EShaderStage perDrawInlineShaderStages = compiledObj.m_bHasTessellation
		? (EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain)
		: (EShaderStage_Vertex | EShaderStage_Pixel);

	pCommandInterface->PrepareInlineConstantBufferForUse(EResourceLayoutSlot_PerDrawCB, compiledObj.m_pPerDrawCB, eConstantBufferShaderSlot_PerDraw, perDrawInlineShaderStages);

	{
		if (!bInstanceOnly)
		{
			pCommandInterface->PrepareVertexBuffersForUse(1, 0, compiledObj.m_vertexStreamSet);

			if (compiledObj.m_indexStreamSet == nullptr)
			{
				return;
			}

			pCommandInterface->PrepareIndexBufferForUse(compiledObj.m_indexStreamSet);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::UpdatePerDrawRS(water::SCompiledWaterOcean& RESTRICT_REFERENCE compiledObj, const SWaterOceanParam& oceanParam, const CWaterStage& waterStage)
{
	CDeviceResourceSetDesc perInstanceResources(waterStage.GetDefaultPerInstanceResources(), nullptr, nullptr);

	auto* pDisplacementTex = (oceanParam.bWaterOceanFFT) ? CRendererResources::s_ptexWaterOcean : CRendererResources::s_ptexBlack;

//	perInstanceResources.SetTexture(water::ePerInstanceTexture_OceanFoam, CWaterStage::m_pFoamTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
	perInstanceResources.SetTexture(water::ePerInstanceTexture_OceanDisplacement, pDisplacementTex, EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Domain);

	// get ocean reflection texture from render target.
	auto* pReflectionTex = CRendererResources::s_ptexBlack;
	if (m_pRenderTarget)
	{
		SEnvTexture* pEnvTex = m_pRenderTarget->GetEnv2D();
		if (pEnvTex && pEnvTex->m_pTex && pEnvTex->m_pTex->GetTexture())
		{
			pReflectionTex = (CTexture*)pEnvTex->m_pTex->GetTexture();
		}
	}
	perInstanceResources.SetTexture(water::ePerInstanceTexture_OceanReflection, pReflectionTex, EDefaultResourceViews::Default, EShaderStage_Pixel);

	compiledObj.m_pPerDrawRS = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	compiledObj.m_pPerDrawRS->Update(perInstanceResources);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::UpdatePerDrawCB(water::SCompiledWaterOcean& RESTRICT_REFERENCE compiledObj, const CRenderObject& renderObj) const
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	if (!compiledObj.m_pPerDrawCB)
	{
		compiledObj.m_pPerDrawCB = rd->m_DevBufMan.CreateConstantBuffer(sizeof(water::SPerDrawConstantBuffer));
	}

	if (!compiledObj.m_pPerDrawCB)
	{
		return;
	}

	CryStackAllocWithSize(water::SPerDrawConstantBuffer, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

	// set reflection matrix from render target.
	cb->mReflProj.SetIdentity();
	if (m_pRenderTarget)
	{
		SEnvTexture* pEnvTex = m_pRenderTarget->GetEnv2D();
		if (pEnvTex && pEnvTex->m_pTex)
		{
			cb->mReflProj = pEnvTex->m_Matrix;
			cb->mReflProj.Transpose();
		}
	}

	if (m_CustomData)
	{
		// passed from COcean::Render()
		float* pData = static_cast<float*>(m_CustomData);
		cb->OceanParams0 = Vec4(pData[0], pData[1], pData[2], pData[3]);
		cb->OceanParams1 = Vec4(pData[4], pData[5], pData[6], pData[7]);
		cb->cOceanFogColorDensity = Vec4(pData[8], pData[9], pData[10], pData[11]);
	}
	else
	{
		cb->OceanParams0 = Vec4(0.0f);
		cb->OceanParams1 = Vec4(0.0f);
		cb->cOceanFogColorDensity = Vec4(0.0f);
	}

	compiledObj.m_pPerDrawCB->UpdateBuffer(cb, cbSize);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CREWaterOcean::UpdateVertex(water::SCompiledWaterOcean& compiledObj, int32 primType)
{
	CRenderElement::SGeometryInfo geomInfo;
	ZeroStruct(geomInfo);

	{
		const size_t vertexBufferStride = sizeof(SVF_P3F_C4B_T2F);

		const bool bTessellation = compiledObj.m_bHasTessellation;

		// fill geomInfo.
		geomInfo.primitiveType = bTessellation ? ept3ControlPointPatchList : primType;
		geomInfo.eVertFormat = EDefaultInputLayouts::P3F_C4B_T2F;
		geomInfo.nFirstIndex = 0;
		geomInfo.nNumIndices = m_nIndicesCount;
		geomInfo.nFirstVertex = 0;
		geomInfo.nNumVertices = m_nVerticesCount;
		geomInfo.nNumVertexStreams = 1;
		geomInfo.indexStream.hStream = m_indexBufferHandle;
		geomInfo.indexStream.nStride = (m_nIndexSizeof == 2) ? RenderIndexType::Index16 : RenderIndexType::Index32;
		geomInfo.vertexStreams[0].hStream = m_vertexBufferHandle;
		geomInfo.vertexStreams[0].nStride = vertexBufferStride;
		geomInfo.vertexStreams[0].nSlot = 0;
		geomInfo.nTessellationPatchIDOffset = 0;
	}

	// Fill stream pointers.
	if (geomInfo.indexStream.hStream != 0)
	{
		compiledObj.m_indexStreamSet = GetDeviceObjectFactory().CreateIndexStreamSet(&geomInfo.indexStream);
	}
	else
	{
		compiledObj.m_indexStreamSet = nullptr;
	}
	compiledObj.m_vertexStreamSet = GetDeviceObjectFactory().CreateVertexStreamSet(geomInfo.nNumVertexStreams, &geomInfo.vertexStreams[0]);
	compiledObj.m_nNumIndices = geomInfo.nNumIndices;
	compiledObj.m_nStartIndex = geomInfo.nFirstIndex;
	compiledObj.m_nVerticesCount = geomInfo.nNumVertices;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
