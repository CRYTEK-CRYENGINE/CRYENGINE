// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include <CryRenderer/RenderElements/CREFogVolume.h>

#include "XRenderD3D9/DriverD3D.h"
#include "GraphicsPipeline/SceneForward.h"

//////////////////////////////////////////////////////////////////////////

namespace render_element
{
namespace fogvolume
{
const buffer_handle_t invalidBufferHandle = ~0u;

enum class EPsoId : uint32
{
	ViewerOustideVolume = 0,
	ViewerInsideVolume,
	ViewerInsideVolumeNearCutoff,
};

struct SPerInstanceConstantBuffer
{
	Matrix44 objMatrix;
	Matrix34 invObjSpaceMatrix;
	Vec4     globalDensity;
	Vec4     densityOffset;
	Vec4     nearCutoff;
	Vec4     outsideSoftEdgesLerp;
	Vec4     fogColor;
	Vec4     heightFallOffBasePoint;
	Vec4     heightFallOffDirScaled;
};

struct SCompiledFogVolume : NoCopy
{
	SCompiledFogVolume() {}

	~SCompiledFogVolume()
	{
		ReleaseDeviceResources();
	}

	void ReleaseDeviceResources()
	{
		if (m_pMaterialResourceSet)
		{
			gRenDev->m_pRT->RC_ReleaseRS(m_pMaterialResourceSet);
		}

		if (m_pPerInstanceResourceSet)
		{
			gRenDev->m_pRT->RC_ReleaseRS(m_pPerInstanceResourceSet);
		}

		if (m_pPerInstanceCB)
		{
			gRenDev->m_pRT->RC_ReleaseCB(m_pPerInstanceCB);
			m_pPerInstanceCB = nullptr;
		}
	}

	CDeviceResourceSetPtr     m_pMaterialResourceSet = nullptr;
	CDeviceResourceSetPtr     m_pPerInstanceResourceSet = nullptr;
	CConstantBuffer*          m_pPerInstanceCB = nullptr;

	const CDeviceInputStream* m_vertexStreamSet = nullptr;
	const CDeviceInputStream* m_indexStreamSet = nullptr;

	int32                     m_nVerticesCount = 0;
	int32                     m_nStartIndex = 0;
	int32                     m_nNumIndices = 0;

	DevicePipelineStatesArray m_psoArray;

	bool                      m_bValid = false; //!< True if compilation succeeded.
	bool                      m_bViewerInsideVolume = false;
	bool                      m_bNearCutoff = false;
};

bool CreatePipelineStates(CSceneForwardStage& forwardStage,
                          DevicePipelineStatesArray& stateArray,
                          const SGraphicsPipelineStateDescription& stateDesc,
                          CGraphicsPipelineStateLocalCache* pStateCache)
{
	if (pStateCache->Find(stateDesc, stateArray))
		return true;

	bool bFullyCompiled = true;

	SGraphicsPipelineStateDescription _stateDesc = stateDesc;

	_stateDesc.technique = TTYPE_GENERAL;
	auto customRenderStateOutside = [](CDeviceGraphicsPSODesc& psoDesc, const SGraphicsPipelineStateDescription& desc)
	{
		psoDesc.m_RenderState &= ~(GS_NODEPTHTEST|GS_DEPTHFUNC_MASK);
		psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL;
		psoDesc.m_CullMode = eCULL_Back;
	};
	bFullyCompiled &= forwardStage.CreatePipelineState(_stateDesc, stateArray[uint32(EPsoId::ViewerOustideVolume)], CSceneForwardStage::ePass_Forward, customRenderStateOutside);

	// rendered with no depth test, cull front.
	_stateDesc.technique = TTYPE_GENERAL;
	bFullyCompiled &= forwardStage.CreatePipelineState(_stateDesc, stateArray[uint32(EPsoId::ViewerInsideVolume)], CSceneForwardStage::ePass_Forward, nullptr);

	_stateDesc.technique = TTYPE_GENERAL;
	auto customRenderStateNearCutoff = [](CDeviceGraphicsPSODesc& psoDesc, const SGraphicsPipelineStateDescription& desc)
	{
		psoDesc.m_RenderState &= ~(GS_NODEPTHTEST|GS_DEPTHFUNC_MASK);
		psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL;
	};
	bFullyCompiled &= forwardStage.CreatePipelineState(_stateDesc, stateArray[uint32(EPsoId::ViewerInsideVolumeNearCutoff)], CSceneForwardStage::ePass_Forward, customRenderStateNearCutoff);

	if (bFullyCompiled)
	{
		pStateCache->Put(stateDesc, stateArray);
	}

	return bFullyCompiled;
}
}
}

//////////////////////////////////////////////////////////////////////////

CREFogVolume::CREFogVolume()
	: CRenderElement()
	, m_pCompiledObject(new render_element::fogvolume::SCompiledFogVolume())
	, m_center(0.0f, 0.0f, 0.0f)
	, m_viewerInsideVolume(0)
	, m_stencilRef(0)
	, m_reserved(0)
	, m_localAABB(Vec3(-1, -1, -1), Vec3(1, 1, 1))
	, m_matWSInv()
	, m_fogColor(1, 1, 1, 1)
	, m_globalDensity(1)
	, m_softEdgesLerp(1, 0)
	, m_heightFallOffDirScaled(0, 0, 1)
	, m_heightFallOffBasePoint(0, 0, 0)
	, m_eyePosInWS(0, 0, 0)
	, m_eyePosInOS(0, 0, 0)
	, m_rampParams(0, 0, 0)
	, m_windOffset(0, 0, 0)
	, m_noiseScale(0)
	, m_noiseFreq(1, 1, 1)
	, m_noiseOffset(0)
	, m_noiseElapsedTime(0)
	, m_emission(0, 0, 0)
	, m_handleVertexBuffer(render_element::fogvolume::invalidBufferHandle)
	, m_handleIndexBuffer(render_element::fogvolume::invalidBufferHandle)
	, m_cachedLocalAABB(AABB::RESET)
{
	mfSetType(eDATA_FogVolume);
	mfUpdateFlags(FCEF_TRANSFORM);

	m_matWSInv.SetIdentity();
}

CREFogVolume::~CREFogVolume()
{
	if (m_handleVertexBuffer != render_element::fogvolume::invalidBufferHandle)
	{
		gRenDev->m_DevBufMan.Destroy(m_handleVertexBuffer);
	}
	if (m_handleIndexBuffer != render_element::fogvolume::invalidBufferHandle)
	{
		gRenDev->m_DevBufMan.Destroy(m_handleIndexBuffer);
	}
}

void CREFogVolume::mfPrepare(bool bCheckOverflow)
{
	if (bCheckOverflow)
		gRenDev->FX_CheckOverflow(0, 0, this);
	gRenDev->m_RP.m_pRE = this;
	gRenDev->m_RP.m_RendNumIndices = 0;
	gRenDev->m_RP.m_RendNumVerts = 0;
}

bool CREFogVolume::mfDraw(CShader* ef, SShaderPass* sfm)
{
	CD3D9Renderer* rd(gcpRendD3D);

#if !defined(_RELEASE)
	if (ef->m_eShaderType != eST_PostProcess)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Incorrect shader set for fog volume");
		return false;
	}
#endif

	PROFILE_LABEL_SCOPE("FOG_VOLUME");

	rd->m_RP.m_PersFlags2 |= RBPF2_MSAA_SAMPLEFREQ_PASS;
	rd->FX_SetMSAAFlagsRT();

	// render
	uint32 nPasses(0);
	ef->FXBegin(&nPasses, 0);
	if (0 == nPasses)
	{
		assert(0);
		rd->m_RP.m_PersFlags2 &= ~RBPF2_MSAA_SAMPLEFREQ_PASS;
		return(false);
	}
	ef->FXBeginPass(0);

	if (m_viewerInsideVolume)
	{
		rd->SetCullMode(R_CULL_FRONT);
		int nState = GS_COLMASK_RGB | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
		nState |= m_nearCutoff ? 0 : GS_NODEPTHTEST;
		rd->FX_SetState(nState);
	}
	else
	{
		rd->SetCullMode(R_CULL_BACK);
		rd->FX_SetState(GS_COLMASK_RGB | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
	}

	// set vs constants
	static CCryNameR invObjSpaceMatrixName("invObjSpaceMatrix");
	ef->FXSetVSFloat(invObjSpaceMatrixName, (const Vec4*)&m_matWSInv.m00, 3);

	const Vec4 cNearCutoffVec(m_nearCutoff, (!m_viewerInsideVolume ? 1.0f : 0.0f), 0.0f, 0.0f);
	static CCryNameR nearCutoffName("nearCutoff");
	ef->FXSetVSFloat(nearCutoffName, &cNearCutoffVec, 1);

	// set ps constants
	const Vec4 cFogColVec(m_fogColor.r, m_fogColor.g, m_fogColor.b, 0);
	static CCryNameR fogColorName("fogColor");
	ef->FXSetPSFloat(fogColorName, &cFogColVec, 1);

	const Vec4 cGlobalDensityVec(m_globalDensity, 1.44269502f * m_globalDensity, 0, 0);
	static CCryNameR globalDensityName("globalDensity");
	ef->FXSetPSFloat(globalDensityName, &cGlobalDensityVec, 1);

	const Vec4 cDensityOffsetVec(m_densityOffset, m_densityOffset, m_densityOffset, m_densityOffset);
	static CCryNameR densityOffsetName("densityOffset");
	ef->FXSetPSFloat(densityOffsetName, &cDensityOffsetVec, 1);

	const Vec4 cHeigthFallOffBasePointVec(m_heightFallOffBasePoint, 0);
	static CCryNameR heightFallOffBasePointName("heightFallOffBasePoint");
	ef->FXSetPSFloat(heightFallOffBasePointName, &cHeigthFallOffBasePointVec, 1);

	const Vec4 cHeightFallOffDirScaledVec(m_heightFallOffDirScaled, 0);
	static CCryNameR heightFallOffDirScaledName("heightFallOffDirScaled");
	ef->FXSetPSFloat(heightFallOffDirScaledName, &cHeightFallOffDirScaledVec, 1);

	const Vec4 cOutsideSoftEdgesLerpVec(m_softEdgesLerp.x, m_softEdgesLerp.y, 0, 0);
	static CCryNameR outsideSoftEdgesLerpName("outsideSoftEdgesLerp");
	ef->FXSetPSFloat(outsideSoftEdgesLerpName, &cOutsideSoftEdgesLerpVec, 1);

	// commit all render changes
	rd->FX_Commit();

	// set vertex declaration and streams of skydome
	if (!FAILED(rd->FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
	{
		// define bounding box geometry
		const uint32 c_numBBVertices(8);
		SVF_P3F_C4B_T2F bbVertices[c_numBBVertices] =
		{
			{ Vec3(m_localAABB.min.x, m_localAABB.min.y, m_localAABB.min.z) },
			{ Vec3(m_localAABB.min.x, m_localAABB.max.y, m_localAABB.min.z) },
			{ Vec3(m_localAABB.max.x, m_localAABB.max.y, m_localAABB.min.z) },
			{ Vec3(m_localAABB.max.x, m_localAABB.min.y, m_localAABB.min.z) },
			{ Vec3(m_localAABB.min.x, m_localAABB.min.y, m_localAABB.max.z) },
			{ Vec3(m_localAABB.min.x, m_localAABB.max.y, m_localAABB.max.z) },
			{ Vec3(m_localAABB.max.x, m_localAABB.max.y, m_localAABB.max.z) },
			{ Vec3(m_localAABB.max.x, m_localAABB.min.y, m_localAABB.max.z) }
		};

		const uint32 c_numBBIndices(36);
		static const uint16 bbIndices[c_numBBIndices] =
		{
			0, 1, 2, 0, 2, 3,
			7, 6, 5, 7, 5, 4,
			3, 2, 6, 3, 6, 7,
			4, 5, 1, 4, 1, 0,
			1, 5, 6, 1, 6, 2,
			4, 0, 3, 4, 3, 7
		};

		// copy vertices into dynamic VB
		TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(bbVertices, c_numBBVertices, 0);

		// copy indices into dynamic IB
		TempDynIB16::CreateFillAndBind(bbIndices, c_numBBIndices);
		// draw skydome
		rd->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, c_numBBVertices, 0, c_numBBIndices);
	}

	ef->FXEndPass();
	ef->FXEnd();

	rd->m_RP.m_PersFlags2 &= ~RBPF2_MSAA_SAMPLEFREQ_PASS;

	return(true);
}

bool CREFogVolume::Compile(CRenderObject* pObj)
{
	if (!m_pCompiledObject)
	{
		return false;
	}

	auto& cro = *(m_pCompiledObject);
	cro.m_bValid = false;

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	auto* pForwardStage = rd->GetGraphicsPipeline().GetSceneForwardStage();

	if (!pObj
	    || !pObj->m_pCurrMaterial
	    || !pForwardStage)
	{
		return false;
	}

	auto& shaderItem = pObj->m_pCurrMaterial->GetShaderItem(0);
	CShader* pShader = (CShader*)shaderItem.m_pShader;
	CShaderResources* RESTRICT_POINTER pResources = static_cast<CShaderResources*>(shaderItem.m_pShaderResources);

	if (!pShader
	    || pShader->m_eShaderType != eST_PostProcess
	    || !pResources)
	{
		return false;
	}

	InputLayoutHandle vertexFormat = EDefaultInputLayouts::P3F_C4B_T2F;

	// create PSOs which match to specific material.
	SGraphicsPipelineStateDescription psoDescription(
	  pObj,
	  this,
	  shaderItem,
	  TTYPE_GENERAL, // set as default, this may be overwritten in CreatePipelineStates().
	  vertexFormat,
	  0 /*geomInfo.CalcStreamMask()*/,
	  eptTriangleList
	  );

	cro.m_bViewerInsideVolume = m_viewerInsideVolume ? true : false;
	cro.m_bNearCutoff = m_nearCutoff ? true : false;

	if (!render_element::fogvolume::CreatePipelineStates(*pForwardStage, cro.m_psoArray, psoDescription, pResources->m_pipelineStateCache.get()))
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

	UpdateVertex(cro);

	// UpdatePerInstanceCB uses not thread safe functions like CreateConstantBuffer(),
	// so this needs to be called here instead of DrawToCommandList().
	UpdatePerInstanceCB(cro, *pObj);

	// this is dummy resource to match this render element with the resource layout of forward pass.
	{
		if (!cro.m_pMaterialResourceSet)
		{
			CDeviceResourceSetDesc materialResources;
			for (const auto& res : rd->GetGraphicsPipeline().GetDefaultMaterialBindPoints().GetResources())
			{
				const SResourceBindPoint& bindPoint = res.first;
				const SResourceBinding&   binding   = res.second;

				switch (binding.type)
				{
				case SResourceBinding::EResourceType::ConstantBuffer:
					materialResources.SetConstantBuffer(bindPoint.slotNumber, rd->m_DevBufMan.GetNullConstantBuffer(), bindPoint.stages);
					break;
				case SResourceBinding::EResourceType::Texture:
					materialResources.SetTexture(bindPoint.slotNumber, CTexture::s_ptexBlack, binding.view, bindPoint.stages);
					break;
				default:
					CRY_ASSERT("Not implemented!");
				}
			}

			cro.m_pMaterialResourceSet = GetDeviceObjectFactory().CreateResourceSet();
			cro.m_pMaterialResourceSet->Update(materialResources);
		}

		if (!cro.m_pPerInstanceResourceSet)
		{
			cro.m_pPerInstanceResourceSet = rd->GetGraphicsPipeline().GetDefaultInstanceExtraResourceSet();
		}
	}

	CRY_ASSERT(cro.m_pMaterialResourceSet);
	CRY_ASSERT(cro.m_pPerInstanceResourceSet);
	CRY_ASSERT(cro.m_pPerInstanceCB);
	CRY_ASSERT(cro.m_vertexStreamSet);
	CRY_ASSERT(cro.m_indexStreamSet);

	// Issue the barriers on the core command-list, which executes directly before the Draw()s in multi-threaded jobs
	PrepareForUse(cro, false, GetDeviceObjectFactory().GetCoreCommandList());

	cro.m_bValid = true;
	return true;
}

void CREFogVolume::DrawToCommandList(CRenderObject* pObj, const struct SGraphicsPipelinePassContext& ctx)
{
	if (!m_pCompiledObject)
	{
		return;
	}

	auto& RESTRICT_REFERENCE cobj = *m_pCompiledObject;

#if defined(ENABLE_PROFILING_CODE)
	if (!cobj.m_bValid)
	{
		CryInterlockedIncrement(&SPipeStat::Out()->m_nIncompleteCompiledObjects);
	}
#endif

	if (!cobj.m_bValid)
	{
		return;
	}

	if (ctx.stageID != eStage_SceneForward || ctx.passID != CSceneForwardStage::ePass_Forward)
	{
		return;
	}

	uint32 psoId = cobj.m_bViewerInsideVolume
	               ? (cobj.m_bNearCutoff ? uint32(render_element::fogvolume::EPsoId::ViewerInsideVolumeNearCutoff) : uint32(render_element::fogvolume::EPsoId::ViewerInsideVolume))
	               : uint32(render_element::fogvolume::EPsoId::ViewerOustideVolume);

	const CDeviceGraphicsPSOPtr& pPso = cobj.m_psoArray[psoId];

	if (!pPso || !pPso->IsValid())
	{
		return;
	}

	CRY_ASSERT(cobj.m_pPerInstanceCB);

	CDeviceGraphicsCommandInterface& RESTRICT_REFERENCE commandInterface = *(ctx.pCommandList->GetGraphicsInterface());

	// Set states
	commandInterface.SetPipelineState(pPso.get());
	commandInterface.SetResources(EResourceLayoutSlot_PerMaterialRS, cobj.m_pMaterialResourceSet.get());
	commandInterface.SetResources(EResourceLayoutSlot_PerInstanceExtraRS, cobj.m_pPerInstanceResourceSet.get());

	EShaderStage perInstanceCBShaderStages = (EShaderStage_Vertex | EShaderStage_Pixel);
	commandInterface.SetInlineConstantBuffer(EResourceLayoutSlot_PerInstanceCB, cobj.m_pPerInstanceCB, eConstantBufferShaderSlot_PerInstance, perInstanceCBShaderStages);

	if (CRenderer::CV_r_NoDraw != 3)
	{
		commandInterface.SetVertexBuffers(1, 0, cobj.m_vertexStreamSet);
		commandInterface.SetIndexBuffer(cobj.m_indexStreamSet);
		commandInterface.DrawIndexed(cobj.m_nNumIndices, 1, cobj.m_nStartIndex, 0, 0);
	}
}

void CREFogVolume::PrepareForUse(render_element::fogvolume::SCompiledFogVolume& RESTRICT_REFERENCE compiledObj, bool bInstanceOnly, CDeviceCommandList& RESTRICT_REFERENCE commandList) const
{
	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();

	pCommandInterface->PrepareResourcesForUse(EResourceLayoutSlot_PerMaterialRS, compiledObj.m_pMaterialResourceSet.get());
	pCommandInterface->PrepareResourcesForUse(EResourceLayoutSlot_PerInstanceExtraRS, compiledObj.m_pPerInstanceResourceSet.get());

	EShaderStage perInstanceCBShaderStages = (EShaderStage_Vertex | EShaderStage_Pixel);
	pCommandInterface->PrepareInlineConstantBufferForUse(EResourceLayoutSlot_PerInstanceCB, compiledObj.m_pPerInstanceCB, eConstantBufferShaderSlot_PerInstance, perInstanceCBShaderStages);

	pCommandInterface->PrepareVertexBuffersForUse(1, 0, compiledObj.m_vertexStreamSet);
	pCommandInterface->PrepareIndexBufferForUse(compiledObj.m_indexStreamSet);
}

void CREFogVolume::UpdatePerInstanceCB(render_element::fogvolume::SCompiledFogVolume& RESTRICT_REFERENCE compiledObj, const CRenderObject& renderObj) const
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	if (!compiledObj.m_pPerInstanceCB)
	{
		compiledObj.m_pPerInstanceCB = rd->m_DevBufMan.CreateConstantBufferRaw(sizeof(render_element::fogvolume::SPerInstanceConstantBuffer));
	}

	if (!compiledObj.m_pPerInstanceCB)
	{
		return;
	}

	CryStackAllocWithSize(render_element::fogvolume::SPerInstanceConstantBuffer, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

	cb->objMatrix = renderObj.m_II.m_Matrix;
	cb->invObjSpaceMatrix = m_matWSInv;
	cb->globalDensity = Vec4(m_globalDensity, 1.44269502f * m_globalDensity, 0, 0);
	cb->densityOffset = Vec4(m_densityOffset, m_densityOffset, m_densityOffset, m_densityOffset);
	cb->nearCutoff = Vec4(m_nearCutoff, (!m_viewerInsideVolume ? 1.0f : 0.0f), 0.0f, 0.0f);
	cb->outsideSoftEdgesLerp = Vec4(m_softEdgesLerp.x, m_softEdgesLerp.y, 0, 0);
	cb->fogColor = Vec4(m_fogColor.r, m_fogColor.g, m_fogColor.b, 0);
	cb->heightFallOffBasePoint = Vec4(m_heightFallOffBasePoint, 0);
	cb->heightFallOffDirScaled = Vec4(m_heightFallOffDirScaled, 0);

	compiledObj.m_pPerInstanceCB->UpdateBuffer(cb, cbSize);
}

bool CREFogVolume::UpdateVertex(render_element::fogvolume::SCompiledFogVolume& RESTRICT_REFERENCE compiledObj)
{
	const size_t numIndices = 36;
	const size_t indexBufferStride = 2;
	const size_t indexBufferSize = numIndices * indexBufferStride;

	const size_t numVertices = 8;
	const size_t vertexBufferStride = sizeof(SVF_P3F_C4B_T2F);
	const size_t vertexBufferSize = numVertices * vertexBufferStride;

	if (m_handleIndexBuffer == render_element::fogvolume::invalidBufferHandle)
	{
		m_handleIndexBuffer = gRenDev->m_DevBufMan.Create(BBT_INDEX_BUFFER, BU_STATIC, indexBufferSize);

		if (m_handleIndexBuffer != render_element::fogvolume::invalidBufferHandle)
		{
			CRY_ASSERT(gRenDev->m_DevBufMan.Size(m_handleIndexBuffer) >= indexBufferSize);

			// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
			CryStackAllocWithSizeVector(uint16, numIndices, volumeIndices, CDeviceBufferManager::AlignBufferSizeForStreaming);

			static const uint16 bbIndices[numIndices] =
			{
				0, 1, 2, 0, 2, 3,
				7, 6, 5, 7, 5, 4,
				3, 2, 6, 3, 6, 7,
				4, 5, 1, 4, 1, 0,
				1, 5, 6, 1, 6, 2,
				4, 0, 3, 4, 3, 7
			};
			memcpy(volumeIndices.address, bbIndices, sizeof(bbIndices));

			gRenDev->m_DevBufMan.UpdateBuffer(m_handleIndexBuffer, volumeIndices, indexBufferSize);
		}
	}

	if (m_handleVertexBuffer == render_element::fogvolume::invalidBufferHandle)
	{
		m_handleVertexBuffer = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, vertexBufferSize);
	}

	if (m_handleVertexBuffer != render_element::fogvolume::invalidBufferHandle
	    && !IsEquivalent(m_cachedLocalAABB, m_localAABB))
	{
		CRY_ASSERT(gRenDev->m_DevBufMan.Size(m_handleVertexBuffer) >= vertexBufferSize);

		// update vertex buffer once and keep it.
		{
			// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
			CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, numVertices, volumeVertices, CDeviceBufferManager::AlignBufferSizeForStreaming);

			volumeVertices[0].xyz = Vec3(m_localAABB.min.x, m_localAABB.min.y, m_localAABB.min.z);
			volumeVertices[1].xyz = Vec3(m_localAABB.min.x, m_localAABB.max.y, m_localAABB.min.z);
			volumeVertices[2].xyz = Vec3(m_localAABB.max.x, m_localAABB.max.y, m_localAABB.min.z);
			volumeVertices[3].xyz = Vec3(m_localAABB.max.x, m_localAABB.min.y, m_localAABB.min.z);
			volumeVertices[4].xyz = Vec3(m_localAABB.min.x, m_localAABB.min.y, m_localAABB.max.z);
			volumeVertices[5].xyz = Vec3(m_localAABB.min.x, m_localAABB.max.y, m_localAABB.max.z);
			volumeVertices[6].xyz = Vec3(m_localAABB.max.x, m_localAABB.max.y, m_localAABB.max.z);
			volumeVertices[7].xyz = Vec3(m_localAABB.max.x, m_localAABB.min.y, m_localAABB.max.z);

			gRenDev->m_DevBufMan.UpdateBuffer(m_handleVertexBuffer, volumeVertices, vertexBufferSize);

			m_cachedLocalAABB = m_localAABB;
		}
	}

	if (m_handleIndexBuffer != render_element::fogvolume::invalidBufferHandle
	    && m_handleVertexBuffer != render_element::fogvolume::invalidBufferHandle)
	{
		// fill geomInfo.
		CRenderElement::SGeometryInfo geomInfo = {};
		geomInfo.primitiveType = eptTriangleList;
		geomInfo.eVertFormat = EDefaultInputLayouts::P3F_C4B_T2F;
		geomInfo.nFirstVertex = 0;
		geomInfo.nNumIndices = numIndices;
		geomInfo.nFirstVertex = 0;
		geomInfo.nNumVertices = numVertices;
		geomInfo.nNumVertexStreams = 1;
		geomInfo.indexStream.hStream = m_handleIndexBuffer;
		geomInfo.indexStream.nStride = (indexBufferStride == 2) ? RenderIndexType::Index16 : RenderIndexType::Index32;
		geomInfo.vertexStreams[0].hStream = m_handleVertexBuffer;
		geomInfo.vertexStreams[0].nStride = vertexBufferStride;
		geomInfo.vertexStreams[0].nSlot = 0;
		geomInfo.nTessellationPatchIDOffset = 0;

		// Fill stream pointers.
		compiledObj.m_indexStreamSet = GetDeviceObjectFactory().CreateIndexStreamSet(&geomInfo.indexStream);
		compiledObj.m_vertexStreamSet = GetDeviceObjectFactory().CreateVertexStreamSet(geomInfo.nNumVertexStreams, &geomInfo.vertexStreams[0]);
		compiledObj.m_nNumIndices = geomInfo.nNumIndices;
		compiledObj.m_nStartIndex = geomInfo.nFirstIndex;
		compiledObj.m_nVerticesCount = geomInfo.nNumVertices;

		return true;
	}

	return false;
}
