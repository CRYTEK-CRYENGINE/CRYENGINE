// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/RenderElements/CREWaterVolume.h>

#include "XRenderD3D9/DriverD3D.h"

#include "GraphicsPipeline/Water.h"
#include "Common/ReverseDepth.h"
#include "Common/PostProcess/PostProcessUtils.h"

//////////////////////////////////////////////////////////////////////////

namespace watervolume
{
const buffer_handle_t invalidBufferHandle = ~0u;

struct SPerDrawConstantBuffer
{
	Matrix44 WorldMatrix;

	// ocean related parameters
	Vec4 OceanParams0;
	Vec4 OceanParams1;

	// water surface and reflection parameters
	Vec4 vBBoxMin;
	Vec4 vBBoxMax;

	// Fog parameters
	Vec4 cFogColorDensity;
	Vec4 cViewerColorToWaterPlane;

	// water fog volume parameters
	Vec4 cFogPlane;
	Vec4 cPerpDist;
	Vec4 cFogColorShallowWaterLevel;
	Vec4 cUnderWaterInScatterConst;
	Vec4 volFogShadowRange;

	// water caustics parameters
	Vec4 vCausticParams;
};

struct SCompiledWaterVolume : NoCopy
{
	SCompiledWaterVolume()
		: m_pPerDrawCB(nullptr)
		, m_vertexStreamSet(nullptr)
		, m_indexStreamSet(nullptr)
		, m_nVerticesCount(0)
		, m_nStartIndex(0)
		, m_nNumIndices(0)
		, m_bValid(0)
		, m_bHasTessellation(0)
		, m_bCaustics(0)
		, m_reserved(0)
	{}

	~SCompiledWaterVolume()
	{
		ReleaseDeviceResources();
	}

	void ReleaseDeviceResources()
	{
		m_pMaterialResourceSet.reset();
		m_pPerDrawRS.reset();
		m_pPerDrawCB.reset();
	}

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
	uint32                    m_bCaustics        : 1; //!< True if this object can be rendered in caustics gen pass.
	uint32                    m_reserved         : 29;
};
}

//////////////////////////////////////////////////////////////////////////

CREWaterVolume::CREWaterVolume()
	: CRenderElement()
	, m_pParams(nullptr)
	, m_pOceanParams(nullptr)
	, m_drawWaterSurface(false)
	, m_drawFastPath(false)
	, m_vertexBuffer()
	, m_indexBuffer()
	, m_vertexBufferQuad()
	, m_pCompiledObject(new watervolume::SCompiledWaterVolume())
{
	mfSetType(eDATA_WaterVolume);
	mfUpdateFlags(FCEF_TRANSFORM);
}

CREWaterVolume::~CREWaterVolume()
{
	if (m_vertexBuffer.handle != watervolume::invalidBufferHandle)
	{
		gRenDev->m_DevBufMan.Destroy(m_vertexBuffer.handle);
	}
	if (m_indexBuffer.handle != watervolume::invalidBufferHandle)
	{
		gRenDev->m_DevBufMan.Destroy(m_indexBuffer.handle);
	}
	if (m_vertexBufferQuad.handle != watervolume::invalidBufferHandle)
	{
		gRenDev->m_DevBufMan.Destroy(m_vertexBufferQuad.handle);
	}
}

void CREWaterVolume::mfGetPlane(Plane& pl)
{
	pl = m_pParams->m_fogPlane;
	pl.d = -pl.d;
}

void CREWaterVolume::mfCenter(Vec3& vCenter, CRenderObject* pObj, const SRenderingPassInfo& passInfo)
{
	vCenter = m_pParams->m_center;
	if (pObj)
		vCenter += pObj->GetMatrix(passInfo).GetTranslation();
}

bool CREWaterVolume::Compile(CRenderObject* pObj,CRenderView *pRenderView, bool updateInstanceDataOnly)
{
	if (!m_pCompiledObject)
	{
		return false;
	}

	auto& compiledObj = *(m_pCompiledObject);
	compiledObj.m_bValid = 0;

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

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
	    || !pResources
	    || !pResources->m_pCompiledResourceSet)
	{
		return false;
	}

	// NOTE: workaround for tessellation for water.
	//       FOB_ALLOW_TESSELLATION is forcibly added in CRenderer::EF_AddEf_NotVirtual() even if shader doesn't have domain and hull shaders.
	pObj->m_ObjFlags &= ~FOB_ALLOW_TESSELLATION;

	// create PSOs which match to specific material.
	const InputLayoutHandle vertexFormat = EDefaultInputLayouts::P3F_C4B_T2F;
	SGraphicsPipelineStateDescription psoDescription(
	  pObj,
	  this,
	  shaderItem,
	  TTYPE_GENERAL, // set as default, this may be overwritten in CreatePipelineStates().
	  vertexFormat,
	  0 /*geomInfo.CalcStreamMask()*/,
	  eptTriangleList // tessellation is handled in CreatePipelineStates(). ept3ControlPointPatchList is used in that case.
	  );

	// apply shader quality
	{
		const uint64 quality = g_HWSR_MaskBit[HWSR_QUALITY];
		const uint64 quality1 = g_HWSR_MaskBit[HWSR_QUALITY1];
		switch((rd->GetShaderProfile(pShader->m_eShaderType)).GetShaderQuality())
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

	// TODO: remove this if old graphics pipeline and material preview is removed.
	// NOTE: this is to use a typed constant buffer instead of per batch constant buffer.
	psoDescription.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_COMPUTE_SKINNING];

	// fog related runtime mask, this changes eventual PSOs.
	const bool bFog = pRenderView->IsGlobalFogEnabled();
	const bool bVolumetricFog = (rd->m_bVolumetricFogEnabled != 0);
#if defined(VOLUMETRIC_FOG_SHADOWS)
	const bool bVolFogShadow = (rd->m_bVolFogShadowsEnabled != 0);
#else
	const bool bVolFogShadow = false;
#endif
	psoDescription.objectRuntimeMask |= bFog ? g_HWSR_MaskBit[HWSR_FOG] : 0;
	psoDescription.objectRuntimeMask |= bVolumetricFog ? g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG] : 0;
	psoDescription.objectRuntimeMask |= bVolFogShadow ? g_HWSR_MaskBit[HWSR_SAMPLE5] : 0;

	uint32 passMask = 0;
	bool bFullscreen = false;
	bool bCaustics = false;
	bool bRenderFogShadowWater = false;
	bool bAllowTessellation = false;
	CRY_ASSERT(m_pParams);

	if (m_drawWaterSurface)
	{
		passMask |= CWaterStage::ePassMask_ReflectionGen;
		passMask |= CWaterStage::ePassMask_WaterSurface;
		passMask |= CWaterStage::ePassMask_CausticsGen;

		bFullscreen = false;

		bCaustics = CRenderer::CV_r_watercaustics &&
		            CRenderer::CV_r_watercausticsdeferred &&
		            CRenderer::CV_r_watervolumecaustics &&
		            m_pParams->m_caustics &&
		            (-m_pParams->m_fogPlane.d >= 1.0f); // unfortunately packing to RG8 limits us to heights over 1 meter, so we just disable if volume goes below.

		bAllowTessellation = true;

		// TODO: replace this shader flag with shader constant to avoid shader permutation after removing old graphics pipeline.
		const bool bDeferredRain = rd->m_bDeferredRainEnabled;
		psoDescription.objectRuntimeMask |= bDeferredRain ? g_HWSR_MaskBit[HWSR_OCEAN_PARTICLE] : 0;
	}
	else
	{
		// TODO: replace this shader flag with shader constant to avoid shader permutation after removing old graphics pipeline.
		// forward shadow runtime mask. this changes eventual PSOs.
		// only water fog volume is affected by shadow.
		bRenderFogShadowWater = (CRenderer::CV_r_FogShadowsWater > 0) && (m_pParams->m_fogShadowing > 0.01f);
		psoDescription.objectRuntimeMask |= bRenderFogShadowWater ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;

		// NOTE: PSO caches belong to a material so different material can use different PSO if pass id is same.
		//       For example, different material is used to render inside water fog volume or outside water fog volume.
		passMask |= CWaterStage::ePassMask_FogVolume;

		if (m_pParams->m_viewerInsideVolume)
		{
			bFullscreen = true;
		}
		else // !(m_pParams->m_viewerInsideVolume)
		{
			bFullscreen = false;
		}
	}
	CRY_ASSERT(passMask != 0);

	compiledObj.m_bHasTessellation = 0;
#ifdef WATER_TESSELLATION_RENDERER
	// Enable tessellation for water geometry
	if (bAllowTessellation && SDeviceObjectHelpers::CheckTessellationSupport(shaderItem, TTYPE_GENERAL))
	{
		compiledObj.m_bHasTessellation = 1;
	}
#endif
	psoDescription.objectFlags |= compiledObj.m_bHasTessellation ? FOB_ALLOW_TESSELLATION : 0;

	if (!pWaterStage->CreatePipelineStates(passMask, compiledObj.m_psoArray, psoDescription, pResources->m_pipelineStateCache.get()))
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
	if (m_pParams->m_numVertices == 0)
	{
		return false;
	}

	compiledObj.m_pMaterialResourceSet = pResources->m_pCompiledResourceSet;
	compiledObj.m_bCaustics = bCaustics ? 1 : 0;

	compiledObj.m_pPerDrawRS = pWaterStage->GetDefaultPerInstanceResourceSet();

	UpdateVertex(compiledObj, bFullscreen);

	// UpdatePerInstanceCB uses not thread safe functions like CreateConstantBuffer(),
	// so this needs to be called here instead of DrawToCommandList().
	UpdatePerDrawCB(compiledObj, *pObj, bRenderFogShadowWater, bCaustics, pRenderView);

	CRY_ASSERT(compiledObj.m_pMaterialResourceSet);
	CRY_ASSERT(compiledObj.m_pPerDrawCB);
	CRY_ASSERT(compiledObj.m_pPerDrawRS);
	CRY_ASSERT(compiledObj.m_vertexStreamSet);
	CRY_ASSERT((!bFullscreen && compiledObj.m_indexStreamSet != nullptr) || (bFullscreen && compiledObj.m_indexStreamSet == nullptr));

	// Issue the barriers on the core command-list, which executes directly before the Draw()s in multi-threaded jobs
	PrepareForUse(compiledObj, false, GetDeviceObjectFactory().GetCoreCommandList());

	compiledObj.m_bValid = 1;
	return true;
}

void CREWaterVolume::DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx, CDeviceCommandList* commandList)
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
	const CDeviceGraphicsPSOPtr& pPso = compiledObj.m_psoArray[ctx.passID];

	if (!pPso || !pPso->IsValid() || !compiledObj.m_pMaterialResourceSet->IsValid())
		return;

	// Don't render in caustics gen pass unless needed.
	if ((ctx.passID == CWaterStage::ePass_CausticsGen) && !compiledObj.m_bCaustics)
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

void CREWaterVolume::PrepareForUse(watervolume::SCompiledWaterVolume& compiledObj, bool bInstanceOnly, CDeviceCommandList& RESTRICT_REFERENCE commandList) const
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	CDeviceGraphicsCommandInterface* RESTRICT_POINTER pCommandInterface = commandList.GetGraphicsInterface();

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

void CREWaterVolume::UpdatePerDrawCB(
  watervolume::SCompiledWaterVolume& RESTRICT_REFERENCE compiledObj,
  const CRenderObject& renderObj,
  bool bRenderFogShadowWater,
  bool bCaustics,
  CRenderView *pRenderView) const
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	SRenderViewShaderConstants& PF = pRenderView->GetShaderConstants();
	const auto cameraPos = PF.pCameraPos;

	if (!compiledObj.m_pPerDrawCB)
	{
		compiledObj.m_pPerDrawCB = rd->m_DevBufMan.CreateConstantBuffer(sizeof(watervolume::SPerDrawConstantBuffer));
	}

	if (!compiledObj.m_pPerDrawCB)
	{
		return;
	}

	CryStackAllocWithSize(watervolume::SPerDrawConstantBuffer, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

	if (!m_drawWaterSurface && m_pParams->m_viewerInsideVolume)
	{
		Matrix44 m;
		m.SetIdentity();
		cb->WorldMatrix = m;
	}
	else
	{
		cb->WorldMatrix = renderObj.GetMatrix(gcpRendD3D->GetObjectAccessorThreadConfig());
	}

	if (m_CustomData)
	{
		// passed from COcean::Render() and CWaterWaveRenderNode::Render().
		float* pData = static_cast<float*>(m_CustomData);
		cb->OceanParams0 = Vec4(pData[0], pData[1], pData[2], pData[3]);
		cb->OceanParams1 = Vec4(pData[4], pData[5], pData[6], pData[7]);
	}
	else
	{
		cb->OceanParams0 = Vec4(0.0f);
		cb->OceanParams1 = Vec4(0.0f);
	}

	cb->cFogColorDensity = Vec4(0.0f);

	if (!m_drawWaterSurface || m_drawFastPath && !m_pParams->m_viewerInsideVolume)
	{
		const float log2e = 1.44269502f; // log2(e) = 1.44269502

		if (!m_pOceanParams)
		{
			// fog color & density
			const Vec3 col = m_pParams->m_fogColorAffectedBySun ? m_pParams->m_fogColor.CompMul(PF.pSunColor) : m_pParams->m_fogColor;
			cb->cFogColorDensity = Vec4(col, log2e * m_pParams->m_fogDensity);
		}
		else
		{
			// fog color & density
			cb->cFogColorDensity = Vec4(m_pOceanParams->m_fogColor.CompMul(PF.pSunColor), log2e * m_pOceanParams->m_fogDensity);

			// fog color shallow & water level
			cb->cFogColorShallowWaterLevel = Vec4(m_pOceanParams->m_fogColorShallow.CompMul(PF.pSunColor), -m_pParams->m_fogPlane.d);
			if (m_pParams->m_viewerInsideVolume)
			{
				// under water in-scattering constant term = exp2( -fogDensity * ( waterLevel - cameraPos.z) )
				float c(expf(-m_pOceanParams->m_fogDensity * (-m_pParams->m_fogPlane.d - cameraPos.z)));
				cb->cUnderWaterInScatterConst = Vec4(c, 0.0f, 0.0f, 0.0f);
			}
		}

		cb->cFogPlane = Vec4(m_pParams->m_fogPlane.n.x, m_pParams->m_fogPlane.n.y, m_pParams->m_fogPlane.n.z, m_pParams->m_fogPlane.d);
		if (m_pParams->m_viewerInsideVolume)
		{
			cb->cPerpDist = Vec4(m_pParams->m_fogPlane | cameraPos, 0.0f, 0.0f, 0.0f);
		}
	}

	// Disable fog when inside volume or when not using fast path - could assign custom RT flag for this instead
	if (m_drawFastPath && m_pParams->m_viewerInsideVolume || !m_drawFastPath && m_drawWaterSurface)
	{
		// fog color & density
		cb->cFogColorDensity = Vec4(0.0f);
	}

	if (m_drawWaterSurface)
	{
		cb->vBBoxMin = Vec4(m_pParams->m_WSBBox.min, 1.0f);
		cb->vBBoxMax = Vec4(m_pParams->m_WSBBox.max, 1.0f);
	}
	else
	{
		cb->vBBoxMin = Vec4(0.0f);
		cb->vBBoxMax = Vec4(0.0f);
	}

	Vec4 viewerColorToWaterPlane(m_pParams->m_viewerCloseToWaterPlane && m_pParams->m_viewerCloseToWaterVolume ? 0.0f : 1.0f,
	                             m_pParams->m_viewerCloseToWaterVolume ? 2.0f * 2.0f : 0.0f,
	                             0.0f,
	                             0.0f);
	cb->cViewerColorToWaterPlane = viewerColorToWaterPlane;

	if (bRenderFogShadowWater)
	{
		Vec3 volFogShadowRangeP;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_RANGE, volFogShadowRangeP);
		volFogShadowRangeP.x = clamp_tpl(volFogShadowRangeP.x, 0.01f, 1.0f);
		cb->volFogShadowRange.x = volFogShadowRangeP.x;
		cb->volFogShadowRange.y = volFogShadowRangeP.y;
		cb->volFogShadowRange.z = clamp_tpl(m_pParams->m_fogShadowing, 0.0f, 1.0f);
		cb->volFogShadowRange.w = 1.0f - clamp_tpl(m_pParams->m_fogShadowing, 0.0f, 1.0f);
	}
	else
	{
		cb->volFogShadowRange = Vec4(0.0f);
	}

	if (bCaustics)
	{
		cb->vCausticParams = Vec4(CRenderer::CV_r_watercausticsdistance, m_pParams->m_causticIntensity, m_pParams->m_causticTiling, m_pParams->m_causticHeight);
	}
	else
	{
		cb->vCausticParams = Vec4(0.0f);
	}

	compiledObj.m_pPerDrawCB->UpdateBuffer(cb, cbSize);
}

void CREWaterVolume::UpdateVertex(watervolume::SCompiledWaterVolume& compiledObj, bool bFullscreen)
{
	CRenderElement::SGeometryInfo geomInfo;
	ZeroStruct(geomInfo);

	const bool bDrawSurface = (m_drawWaterSurface || !m_pParams->m_viewerInsideVolume);
	CRY_ASSERT((bFullscreen && !bDrawSurface) || (!bFullscreen && bDrawSurface));

	if (!bFullscreen)
	{
		const size_t vertexBufferStride = sizeof(SVF_P3F_C4B_T2F);
		const size_t vertexBufferSize = m_pParams->m_numVertices * vertexBufferStride;
		const size_t indexBufferStride = 2;
		const size_t indexBufferSize = m_pParams->m_numIndices * indexBufferStride;

		// create buffers if needed.
		if (m_vertexBuffer.handle == watervolume::invalidBufferHandle
		    || m_vertexBuffer.size != vertexBufferSize)
		{
			if (m_vertexBuffer.handle != watervolume::invalidBufferHandle)
			{
				gRenDev->m_DevBufMan.Destroy(m_vertexBuffer.handle);
			}
			m_vertexBuffer.handle = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_DYNAMIC, vertexBufferSize);
			m_vertexBuffer.size = vertexBufferSize;
		}
		if (m_indexBuffer.handle == watervolume::invalidBufferHandle
		    || m_indexBuffer.size != indexBufferSize)
		{
			if (m_indexBuffer.handle != watervolume::invalidBufferHandle)
			{
				gRenDev->m_DevBufMan.Destroy(m_indexBuffer.handle);
			}
			m_indexBuffer.handle = gRenDev->m_DevBufMan.Create(BBT_INDEX_BUFFER, BU_DYNAMIC, indexBufferSize);
			m_indexBuffer.size = indexBufferSize;
		}

		// TODO: update only when water volume surface changes.
		CRY_ASSERT(m_vertexBuffer.handle != watervolume::invalidBufferHandle);
		CRY_ASSERT(gRenDev->m_DevBufMan.Size(m_vertexBuffer.handle) >= vertexBufferSize);
		CRY_ASSERT(m_pParams->m_pVertices!=nullptr);
		if ((m_vertexBuffer.handle != watervolume::invalidBufferHandle)  && (m_pParams->m_pVertices != nullptr))
		{
			gRenDev->m_DevBufMan.UpdateBuffer(m_vertexBuffer.handle, m_pParams->m_pVertices, vertexBufferSize);
		}

		CRY_ASSERT(m_indexBuffer.handle != watervolume::invalidBufferHandle);
		CRY_ASSERT(gRenDev->m_DevBufMan.Size(m_indexBuffer.handle) >= indexBufferSize);
		CRY_ASSERT(m_pParams->m_pIndices != nullptr);
		if ((m_indexBuffer.handle != watervolume::invalidBufferHandle) && (m_pParams->m_pIndices != nullptr))
		{
			gRenDev->m_DevBufMan.UpdateBuffer(m_indexBuffer.handle, m_pParams->m_pIndices, indexBufferSize);
		}

		const bool bTessellation = compiledObj.m_bHasTessellation;

		// fill geomInfo.
		geomInfo.primitiveType = bTessellation ? ept3ControlPointPatchList : eptTriangleList;
		geomInfo.eVertFormat = EDefaultInputLayouts::P3F_C4B_T2F;
		geomInfo.nFirstIndex = 0;
		geomInfo.nNumIndices = m_pParams->m_numIndices;
		geomInfo.nFirstVertex = 0;
		geomInfo.nNumVertices = m_pParams->m_numVertices;
		geomInfo.nNumVertexStreams = 1;
		geomInfo.indexStream.hStream = m_indexBuffer.handle;
		geomInfo.indexStream.nStride = (indexBufferStride == 2) ? RenderIndexType::Index16 : RenderIndexType::Index32;
		geomInfo.vertexStreams[0].hStream = m_vertexBuffer.handle;
		geomInfo.vertexStreams[0].nStride = vertexBufferStride;
		geomInfo.vertexStreams[0].nSlot = 0;
		geomInfo.nTessellationPatchIDOffset = 0;
	}
	else
	{
		const size_t vertexNum = 3;
		const size_t vertexBufferStride = sizeof(SVF_P3F_C4B_T2F);
		const size_t vertexBufferSize = vertexNum * vertexBufferStride;

		// create buffers if needed.
		if (m_vertexBuffer.handle == watervolume::invalidBufferHandle
		    || m_vertexBuffer.size != vertexBufferSize)
		{
			if (m_vertexBuffer.handle != watervolume::invalidBufferHandle)
			{
				gRenDev->m_DevBufMan.Destroy(m_vertexBuffer.handle);
			}
			m_vertexBuffer.handle = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_DYNAMIC, vertexBufferSize);
			m_vertexBuffer.size = vertexBufferSize;

			// update vertex buffer once and keep it.
			{
				// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
				CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, vertexNum, fullscreenTriVertices, CDeviceBufferManager::AlignBufferSizeForStreaming);

				SPostEffectsUtils::GetFullScreenTri(fullscreenTriVertices, 0, 0, 0.0f);

				if (m_vertexBuffer.handle != watervolume::invalidBufferHandle)
				{
					gRenDev->m_DevBufMan.UpdateBuffer(m_vertexBuffer.handle, fullscreenTriVertices, vertexBufferSize);
				}
			}
		}

		CRY_ASSERT(m_vertexBuffer.handle != watervolume::invalidBufferHandle);
		CRY_ASSERT(gRenDev->m_DevBufMan.Size(m_vertexBuffer.handle) >= vertexBufferSize);

		// fill geomInfo.
		geomInfo.primitiveType = eptTriangleStrip;
		geomInfo.eVertFormat = EDefaultInputLayouts::P3F_C4B_T2F;
		geomInfo.nFirstVertex = 0;
		geomInfo.nNumVertices = vertexNum;
		geomInfo.nNumVertexStreams = 1;
		geomInfo.indexStream.hStream = watervolume::invalidBufferHandle;
		geomInfo.vertexStreams[0].hStream = m_vertexBuffer.handle;
		geomInfo.vertexStreams[0].nStride = vertexBufferStride;
		geomInfo.vertexStreams[0].nSlot = 0;
		geomInfo.nTessellationPatchIDOffset = 0;
	}

	// Fill stream pointers.
	compiledObj.m_indexStreamSet = nullptr;
	if (geomInfo.indexStream.hStream != watervolume::invalidBufferHandle)
		compiledObj.m_indexStreamSet = GetDeviceObjectFactory().CreateIndexStreamSet(&geomInfo.indexStream);

	compiledObj.m_vertexStreamSet = GetDeviceObjectFactory().CreateVertexStreamSet(geomInfo.nNumVertexStreams, &geomInfo.vertexStreams[0]);
	compiledObj.m_nNumIndices = geomInfo.nNumIndices;
	compiledObj.m_nStartIndex = geomInfo.nFirstIndex;
	compiledObj.m_nVerticesCount = geomInfo.nNumVertices;
}
