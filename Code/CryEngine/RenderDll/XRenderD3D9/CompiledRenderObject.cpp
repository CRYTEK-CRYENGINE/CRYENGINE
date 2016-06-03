// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompiledRenderObject.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"  // CMotionBlur::GetPrevObjToWorldMat
#include "Common/Include_HLSL_CPP_Shared.h"
#include "Common/ComputeSkinningStorage.h"

//////////////////////////////////////////////////////////////////////////
#include "GraphicsPipeline/ShadowMap.h"
#include "GraphicsPipeline/SceneGBuffer.h"

//////////////////////////////////////////////////////////////////////////

CRenderObjectsPools* CCompiledRenderObject::s_pPools = 0;

//////////////////////////////////////////////////////////////////////////
CRenderObjectsPools::CRenderObjectsPools()
{
}

//////////////////////////////////////////////////////////////////////////
CRenderObjectsPools::~CRenderObjectsPools()
{
}
//////////////////////////////////////////////////////////////////////////
CConstantBufferPtr CRenderObjectsPools::AllocatePerInstanceConstantBuffer()
{
	if (!m_freeConstantBuffers.empty())
	{
		CConstantBufferPtr ptr = m_freeConstantBuffers.back();
		m_freeConstantBuffers.pop_back();
		return ptr;
	}
	// returns refcount==1
	CConstantBufferPtr pBuf = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerInstanceConstantBuffer_Skin)); // largest buffer type in use
	pBuf->Release();                                                                                                     // now refcount==1 again
	return pBuf;
}

//////////////////////////////////////////////////////////////////////////
void CRenderObjectsPools::FreePerInstanceConstantBuffer(const CConstantBufferPtr& buffer)
{
	if (buffer)
	{
		m_freeConstantBuffers.push_back(buffer);
	}
}

//////////////////////////////////////////////////////////////////////////
CCompiledRenderObject::~CCompiledRenderObject()
{
	m_pRenderElement = 0;
	//SAFE_RELEASE(m_shaderItem.m_pShader);
	//SAFE_RELEASE(m_shaderItem.m_pShaderResources);
	m_shaderItem.m_pShader = 0;
	m_shaderItem.m_pShaderResources = 0;

	m_StencilRef = 0;
	m_nInstances = 0;

	if (m_perInstanceCB && m_bOwnPerInstanceCB)
	{
		s_pPools->FreePerInstanceConstantBuffer(m_perInstanceCB);
	}
	m_perInstanceCB.reset();

	m_materialResourceSet.reset();

	m_InstancingCBs.clear();

	for (int i = 0; i < MAX_PIPELINE_SCENE_STAGES; i++)
	{
		m_pso[i] = DevicePipelineStatesArray();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCompiledRenderObject::Init(const SShaderItem& shaderItem, CRendElementBase* pRE)
{
	m_pRenderElement = pRE;
	m_shaderItem = shaderItem;
	//if (m_shaderItem.m_pShader)
	//m_shaderItem.m_pShader->AddRef();
	//if (m_shaderItem.m_pShaderResources)
	//m_shaderItem.m_pShaderResources->AddRef();
}

//////////////////////////////////////////////////////////////////////////
// allocate and fill buffer
void CCompiledRenderObject::UpdatePerInstanceCB(void* pData, size_t size)
{
	if (!m_perInstanceCB)
	{
		m_perInstanceCB = s_pPools->AllocatePerInstanceConstantBuffer();
		m_bOwnPerInstanceCB = true;
	}
	m_perInstanceCB->UpdateBuffer(pData, size);
}

//////////////////////////////////////////////////////////////////////////
void CCompiledRenderObject::CompilePerInstanceConstantBuffer(CRenderObject* pRenderObject, float realTime)
{
	CCompiledRenderObject* pRootCompiled = pRenderObject->m_pCompiledObject;
	if (pRootCompiled && pRootCompiled->m_perInstanceCB && gcpRendD3D->m_nGraphicsPipeline >= 2)
	{
		// If root object have per instance constant buffer, share ours with root compiled object.
		m_bOwnPerInstanceCB = false;
		m_perInstanceCB = pRootCompiled->m_perInstanceCB;
		m_rootConstants = pRootCompiled->m_rootConstants;
		return;
	}

	// Alpha Test
	float dissolve = 0;
	if (pRenderObject->m_ObjFlags & (FOB_DISSOLVE_OUT | FOB_DISSOLVE))
	{
		dissolve = float(pRenderObject->m_DissolveRef) * (1.0f / 255.0f);
	}
	float dissolveOut = (pRenderObject->m_ObjFlags & FOB_DISSOLVE_OUT) ? 1.0f : -1.0f;
	float tessellationPatchIDOffset = alias_cast<float>(m_TessellationPatchIDOffset);

	Vec4 bendInfo = Vec4(ZERO);
	if (pRenderObject->m_ObjFlags & FOB_BENDED)
	{
		SVegetationBending* pBend = &pRenderObject->m_data.m_bending;

		bendInfo.x = pBend->m_fRadiusVert;
		bendInfo.y = 0;
		bendInfo.z = 0;
		bendInfo.w = pBend->m_fMainBendingScale;
	}

	m_rootConstants.dissolve = dissolve;
	m_rootConstants.dissolveOut = dissolveOut;

	// wrinkle blending
	bool bHasWrinkleBending = false;
	if (m_shaderItem.m_pShader)
	{
		bHasWrinkleBending = (m_shaderItem.m_pShader->GetFlags2() & EF2_WRINKLE_BLENDING) != 0;
	}

	Matrix44A objPrevMatr;
	CMotionBlur::GetPrevObjToWorldMat(pRenderObject, objPrevMatr);

	if (pRenderObject->m_data.m_pTerrainSectorTextureInfo) // (%TEMP_TERRAIN || %TEMP_VEGETATION)
	{
		HLSL_PerInstanceConstantBuffer_TerrainVegetation cb;
		cb.SPIObjWorldMat = pRenderObject->m_II.m_Matrix;
		cb.SPIPrevObjWorldMat = Matrix34(objPrevMatr);
		cb.SPIBendInfo = bendInfo;
		cb.SPIAlphaTest = Vec4(dissolve, dissolveOut, tessellationPatchIDOffset, 0);

		// Fill terrain texture info if present
		cb.BlendTerrainColInfo[0] = pRenderObject->m_data.m_pTerrainSectorTextureInfo->fTexOffsetX;
		cb.BlendTerrainColInfo[1] = pRenderObject->m_data.m_pTerrainSectorTextureInfo->fTexOffsetY;
		cb.BlendTerrainColInfo[2] = pRenderObject->m_data.m_pTerrainSectorTextureInfo->fTexScale;
		cb.BlendTerrainColInfo[3] = pRenderObject->m_data.m_fMaxViewDistance; // Obj view max distance

		// Fill terrain layer info if present
		if (float* pData = (float*)m_pRenderElement->m_CustomData)
		{
			UFloat4* pOut = (UFloat4*)cb.TerrainLayerInfo.GetData();
			pOut[0].f[0] = pData[0];
			pOut[0].f[1] = pData[1];
			pOut[0].f[2] = pData[2];
			pOut[0].f[3] = pData[3];
			pOut[1].f[0] = pData[4];
			pOut[1].f[1] = pData[5];
			pOut[1].f[2] = pData[6];
			pOut[1].f[3] = pData[7];
			pOut[2].f[0] = pData[8];
			pOut[2].f[1] = pData[9];
			pOut[2].f[2] = pData[10];
			pOut[2].f[3] = pData[11];
			pOut[3].f[0] = pData[12];
			pOut[3].f[1] = pData[13];
			pOut[3].f[2] = pData[14];
			pOut[3].f[3] = pData[15];
		}

		UpdatePerInstanceCB(&cb, sizeof(cb));
	}
	else if ((pRenderObject->m_ObjFlags & FOB_SKINNED) || bHasWrinkleBending) // (%_RT_SKELETON_SSD || %_RT_SKELETON_SSD_LINEAR || %WRINKLE_BLENDING)
	{
		HLSL_PerInstanceConstantBuffer_Skin cb;
		ZeroStruct(cb);

		cb.SPIObjWorldMat = pRenderObject->m_II.m_Matrix;
		cb.SPIPrevObjWorldMat = Matrix34(objPrevMatr);
		cb.SPIBendInfo = bendInfo;
		cb.SPIAlphaTest = Vec4(dissolve, dissolveOut, tessellationPatchIDOffset, 0);

		if (SRenderObjData* pOD = pRenderObject->GetObjData())
		{
			// Skinning precision offset
			if (pOD->m_pSkinningData)
			{
				cb.SkinningInfo[0] = pOD->m_pSkinningData->vecPrecisionOffset[0];
				cb.SkinningInfo[1] = pOD->m_pSkinningData->vecPrecisionOffset[1];
				cb.SkinningInfo[2] = pOD->m_pSkinningData->vecPrecisionOffset[2];
			}
			// wrinkles mask
			if (pOD->m_pShaderParams)
			{
				SShaderParam::GetValue(ECGP_PI_WrinklesMask0, const_cast<DynArray<SShaderParam>*>(pOD->m_pShaderParams), &cb.WrinklesMask0[0], 0);
				SShaderParam::GetValue(ECGP_PI_WrinklesMask1, const_cast<DynArray<SShaderParam>*>(pOD->m_pShaderParams), &cb.WrinklesMask1[0], 0);
				SShaderParam::GetValue(ECGP_PI_WrinklesMask2, const_cast<DynArray<SShaderParam>*>(pOD->m_pShaderParams), &cb.WrinklesMask2[0], 0);
			}
		}

		if (m_pRenderElement->mfGetType() == eDATA_Mesh)
		{
			// Skinning extra weights
			cb.SkinningInfo[3] = ((CREMeshImpl*)m_pRenderElement)->m_pRenderMesh->m_extraBonesBuffer.m_numElements > 0 ? 1.0f : 0.0f;
		}

		UpdatePerInstanceCB(&cb, sizeof(cb));
	}
	else // default base per instance buffer
	{
		HLSL_PerInstanceConstantBuffer_Base cb;
		cb.SPIObjWorldMat = pRenderObject->m_II.m_Matrix;
		cb.SPIPrevObjWorldMat = Matrix34(objPrevMatr);
		cb.SPIAlphaTest = Vec4(dissolve, dissolveOut, tessellationPatchIDOffset, 0);
		cb.SPIBendInfo = bendInfo;

		UpdatePerInstanceCB(&cb, sizeof(cb));
	}
}

void CCompiledRenderObject::CompileInstancingData(CRenderObject* pRenderObject, bool bForce)
{
	int nSrcInsts = pRenderObject->m_Instances.size();
	if (m_nInstances == nSrcInsts && !bForce)
		return;

	assert(nSrcInsts);
	m_nInstances = nSrcInsts;

	//m_InstancingCBs.reserve(nSrcInsts);

	m_InstancingCBs.clear();

	int nBeginOffs = 0;
	while (nSrcInsts)
	{
		int nInstsPerCB = MIN(nSrcInsts, 800); // 4096 Vec4 entries max in DX11

		int nSize = nInstsPerCB * sizeof(CRenderObject::SInstanceData);
		CConstantBuffer* pCB = gcpRendD3D.m_DevBufMan.CreateConstantBuffer(nSize); // returns refcount==1
		if (pCB)
		{
			pCB->UpdateBuffer(&pRenderObject->m_Instances[nBeginOffs], nSize);

			CCompiledRenderObject::SInstancingData ID;
			ID.m_nInstances = nInstsPerCB;
			ID.m_pConstBuffer = pCB;
			pCB->Release(); // back to refcount==1
			m_InstancingCBs.push_back(ID);
		}

		nBeginOffs += nInstsPerCB;
		nSrcInsts -= nInstsPerCB;
	}
	//pRenderObject->m_Instances.clear();
}

//////////////////////////////////////////////////////////////////////////
void CCompiledRenderObject::CompilePerInstanceExtraResources(CRenderObject* pRenderObject)
{
	if (!m_bHasTessellation && !pRenderObject->m_data.m_pSkinningData) // only needed for skinning and tessellation at the moment
	{
		m_perInstanceExtraResources = gcpRendD3D->GetGraphicsPipeline().GetDefaultInstanceExtraResources();
		assert(m_perInstanceExtraResources && m_perInstanceExtraResources->IsValid() && "Bad shared default resources");
		return;
	}

	m_perInstanceExtraResources = CCryDeviceWrapper::GetObjectFactory().CloneResourceSet(gcpRendD3D->GetGraphicsPipeline().GetDefaultInstanceExtraResources());

	// TODO: only bind to hs and ds stages when tessellation is enabled
	const EShaderStage shaderStages = EShaderStage_Vertex | EShaderStage_Hull | EShaderStage_Domain;

	if (SSkinningData* pSkinningData = pRenderObject->m_data.m_pSkinningData)
	{
		CD3D9Renderer::SCharacterInstanceCB* const pCurSkinningData = alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSkinningData->pCharInstCB);

		ICVar* cvar_gd = gEnv->pConsole->GetCVar("r_ComputeSkinning");
		bool bDoComputeDeformation = (cvar_gd && cvar_gd->GetIVal()) && (pSkinningData->nHWSkinningFlags & eHWS_DC_deformation_Skinning);
		if (bDoComputeDeformation)
		{
			CRenderMesh* pRenderMesh = static_cast<CRenderMesh*>(pSkinningData->pRenderMesh);
			CRenderMesh::CSSkinOutputMap::iterator it = pRenderMesh->m_outVerticesUAV.find(pSkinningData->pCustomTag);
			if (it != pRenderMesh->m_outVerticesUAV.end())
				m_perInstanceExtraResources->SetBuffer(EReservedTextureSlot_ComputeSkinVerts, *gcpRendD3D->GetComputeSkinningStorage()->GetResource(it->second.skinnedUAVHandle), shaderStages);
		}
		else
		{
			m_perInstanceExtraResources->SetConstantBuffer(eConstantBufferShaderSlot_SkinQuat, pCurSkinningData->boneTransformsBuffer, shaderStages);
			if (pSkinningData->pPreviousSkinningRenderData)
			{
				CD3D9Renderer::SCharacterInstanceCB* const pPrevSkinningData = alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSkinningData->pPreviousSkinningRenderData->pCharInstCB);
				m_perInstanceExtraResources->SetConstantBuffer(eConstantBufferShaderSlot_SkinQuatPrev, pPrevSkinningData->boneTransformsBuffer, shaderStages);
			}

			if (m_pExtraSkinWeights && (m_pExtraSkinWeights->m_flags & DX11BUF_BIND_SRV) != 0) // TODO: m_pExtraSkinWeights is passed a pointer-to-default-constructed-buffer when it should be passed nullptr. Need to check for BIND_SRV flag to check that Create() was called on the buffer.
			{
				m_perInstanceExtraResources->SetBuffer(EReservedTextureSlot_SkinExtraWeights, *m_pExtraSkinWeights, shaderStages);
			}
		}
	}

	if (m_bHasTessellation && m_pTessellationAdjacencyBuffer)
	{
		m_perInstanceExtraResources->SetBuffer(EReservedTextureSlot_AdjacencyInfo, *m_pTessellationAdjacencyBuffer, shaderStages);
	}

	m_perInstanceExtraResources->Build();
}

//////////////////////////////////////////////////////////////////////////
bool CCompiledRenderObject::Compile(CRenderObject* pRenderObject, float realTime)
{
	//int nFrameId = gEnv->pRenderer->GetFrameID(false);
	//{ char buf[1024]; cry_sprintf(buf,"compiled: %p : frame(%d) \r\n", pRenderObject, nFrameId); OutputDebugString(buf); }

	m_pRO = pRenderObject;
	const bool bMuteWarnings = gcpRendD3D->m_nGraphicsPipeline >= 1;  // @TODO: Remove later

	m_bIncomplete = true;

	bool bInstanceDataUpdateOnly = pRenderObject->m_bInstanceDataDirty;

	// Only objects with RenderElements can be compiled
	if (!m_pRenderElement)
	{
		if (!bMuteWarnings) Warning("[CCompiledRenderObject] Compile failed, no render element");
		return true;
	}

	EDataType reType = m_pRenderElement->mfGetType();
	if (reType != eDATA_Mesh && reType != eDATA_Terrain && reType != eDATA_GeomCache)
	{
		// Do not compile non mesh render elements
		if (!bMuteWarnings) Warning("[CCompiledRenderObject] Compile failed, unsupported render element type");
		return true;
	}

	CRendElementBase::SGeometryInfo geomInfo;
	ZeroStruct(geomInfo);

	if (!bInstanceDataUpdateOnly) // first update only: needed for per instance buffers
	{
		const bool bSupportTessellation = (pRenderObject->m_ObjFlags & FOB_ALLOW_TESSELLATION) && SDeviceObjectHelpers::CheckTessellationSupport(m_shaderItem);
		geomInfo.bonesRemapGUID = (pRenderObject->m_data.m_pSkinningData) ? pRenderObject->m_data.m_pSkinningData->remapGUID : 0;
		if (!m_pRenderElement->GetGeometryInfo(geomInfo, bSupportTessellation))
		{
			if (!bMuteWarnings) Warning("[CCompiledRenderObject] Compile failed, GetGeometryInfo failed");
			return true;
		}

		m_bHasTessellation = bSupportTessellation;
		m_TessellationPatchIDOffset = geomInfo.nTessellationPatchIDOffset;

		m_pExtraSkinWeights = reinterpret_cast<CGpuBuffer*>(geomInfo.pSkinningExtraBonesBuffer);
		m_pTessellationAdjacencyBuffer = reinterpret_cast<CGpuBuffer*>(geomInfo.pTessellationAdjacencyBuffer);
	}

	if (pRenderObject->m_Instances.size())
	{
		CompileInstancingData(pRenderObject, false);
	}

	CompilePerInstanceConstantBuffer(pRenderObject, realTime);
	CompilePerInstanceExtraResources(pRenderObject);

	if (bInstanceDataUpdateOnly)
	{
		// Issue the barriers on the core command-list, which executes directly before the Draw()s in multi-threaded jobs
		PrepareForUse(*CCryDeviceWrapper::GetObjectFactory().GetCoreGraphicsCommandList(), true);

		return true;
	}

	CRendElementBase* pRenderElement = m_pRenderElement;

	CShaderResources* RESTRICT_POINTER pResources = static_cast<CShaderResources*>(m_shaderItem.m_pShaderResources);
	assert(pResources);

	if (!pResources || !pResources->m_pCompiledResourceSet)
	{
		if (!bMuteWarnings) Warning("[CCompiledRenderObject] Compile failed, invalid resource set");
		return true;
	}
	m_materialResourceSet = pResources->m_pCompiledResourceSet;

	// Fill stream pointers.
	m_indexStreamSet = CCryDeviceWrapper::GetObjectFactory().CreateIndexStreamSet(&geomInfo.indexStream);
	m_vertexStreamSet = CCryDeviceWrapper::GetObjectFactory().CreateVertexStreamSet(CRY_ARRAY_COUNT(geomInfo.vertexStream), &geomInfo.vertexStream[0]);

	m_nVertexStreamSetSize = geomInfo.nNumVertexStreams;

	m_drawParams[0].m_nNumIndices = m_drawParams[1].m_nNumIndices = geomInfo.nNumIndices;
	m_drawParams[0].m_nStartIndex = m_drawParams[1].m_nStartIndex = geomInfo.nFirstIndex;
	m_drawParams[0].m_nVerticesCount = m_drawParams[1].m_nVerticesCount = geomInfo.nNumVertices;

	if (pRenderObject->m_bPermanent && m_pRenderElement->mfGetType() == eDATA_Mesh)
	{
		CREMeshImpl* pRE = ((CREMeshImpl*)m_pRenderElement);
		pRE->m_pRenderMesh->AddShadowPassMergedChunkIndicesAndVertices(pRE->m_pChunk, pRenderObject->m_pCurrMaterial, m_drawParams[1].m_nVerticesCount, m_drawParams[1].m_nNumIndices);
	}

	// THIS IS DANGEROUS!
	//pRenderObject->m_Instances.clear();

	// Stencil ref value
	uint8 stencilRef = 0; // @TODO: get from CRNTmpData::SRNUserData::m_pClipVolume::GetStencilRef
	m_StencilRef = CRenderer::CV_r_VisAreaClipLightsPerPixel ? 0 : (stencilRef | BIT_STENCIL_INSIDE_CLIPVOLUME);
	m_StencilRef |= (!(pRenderObject->m_ObjFlags & FOB_DYNAMIC_OBJECT) ? BIT_STENCIL_RESERVED : 0);

	m_bRenderNearest = (pRenderObject->m_ObjFlags & FOB_NEAREST) != 0;

	// Create Pipeline States
	SGraphicsPipelineStateDescription psoDescription(pRenderObject, m_shaderItem, TTYPE_GENERAL, geomInfo.eVertFormat, geomInfo.streamMask, geomInfo.primitiveType);
	psoDescription.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_PER_INSTANCE_CB_TEMP];  // Enable flag to use special per instance constant buffer
	if (m_InstancingCBs.size() != 0)
	{
		psoDescription.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_ENVIRONMENT_CUBEMAP]; // Enable flag to use static instancing
	}

	// Issue the barriers on the core command-list, which executes directly before the Draw()s in multi-threaded jobs
	PrepareForUse(*CCryDeviceWrapper::GetObjectFactory().GetCoreGraphicsCommandList(), false);

	if (!gcpRendD3D->GetGraphicsPipeline().CreatePipelineStates(m_pso, psoDescription, pResources->m_pipelineStateCache.get()))
	{
		if (!CRenderer::CV_r_shadersAllowCompilation)
		{
			if (!bMuteWarnings) Warning("[CCompiledRenderObject] Compile failed, PSO creation failed");
			return true;
		}
		else
		{
			return false;  // Shaders might still compile; try recompiling object later
		}
	}

	m_bIncomplete = false;
	return true;
}

void CCompiledRenderObject::PrepareForUse(CDeviceGraphicsCommandListRef RESTRICT_REFERENCE commandList, bool bInstanceOnly) const
{
	if (!bInstanceOnly)
	{
		commandList.PrepareResourcesForUse(EResourceLayoutSlot_PerMaterialRS, m_materialResourceSet.get());
	}

	commandList.PrepareResourcesForUse(EResourceLayoutSlot_PerInstanceExtraRS, m_perInstanceExtraResources.get());

	EShaderStage perInstanceCBShaderStages = m_bHasTessellation ? EShaderStage_Hull | EShaderStage_Vertex | EShaderStage_Pixel : EShaderStage_Vertex | EShaderStage_Pixel;
	commandList.PrepareInlineConstantBufferForUse(EResourceLayoutSlot_PerInstanceCB, m_perInstanceCB, eConstantBufferShaderSlot_PerInstance, perInstanceCBShaderStages);

	{
		if (!bInstanceOnly)
		{
			commandList.PrepareVertexBuffersForUse(m_nVertexStreamSetSize, m_vertexStreamSet);

			if (m_indexStreamSet == nullptr)
			{
				return;
			}

			commandList.PrepareIndexBufferForUse(m_indexStreamSet);
		}

		{
			if (!m_InstancingCBs.empty())
			{
				// Render instanced draw calls.
				for (auto& ID : m_InstancingCBs)
				{
					commandList.PrepareInlineConstantBufferForUse(EResourceLayoutSlot_PerInstanceCB, ID.m_pConstBuffer, eConstantBufferShaderSlot_PerInstance, EShaderStage_Vertex | EShaderStage_Pixel);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CCompiledRenderObject::DrawVerification(const SGraphicsPipelinePassContext& passContext) const
{
#if defined(ENABLE_PROFILING_CODE)
	if (m_bIncomplete || !m_materialResourceSet->IsValid())
	{
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nIncompleteCompiledObjects);
	}
#endif

	const CDeviceGraphicsPSOPtr& pPso = m_pso[passContext.stageID][passContext.passID];

	if (!pPso || !pPso->IsValid() || !m_materialResourceSet->IsValid())
		return false;

	return true;
}

void CCompiledRenderObject::DrawToCommandList(CDeviceGraphicsCommandListRef RESTRICT_REFERENCE commandList, const CDeviceGraphicsPSOPtr& pPso, uint32 drawParamsIndex) const
{
	assert(pPso != nullptr);
	assert(pPso->IsValid());
	assert(m_materialResourceSet->IsValid());
	assert(m_perInstanceExtraResources->IsValid());

	//int nFrameId = gEnv->pRenderer->GetFrameID(false);
	//assert(0 == (m_pRenderElement->m_Flags & FCEF_DIRTY));

	// Set states
	commandList.SetPipelineState(pPso);
	commandList.SetStencilRef(m_StencilRef);
	commandList.SetResources(EResourceLayoutSlot_PerMaterialRS, m_materialResourceSet.get());
	commandList.SetResources(EResourceLayoutSlot_PerInstanceExtraRS, m_perInstanceExtraResources.get());

	EShaderStage perInstanceCBShaderStages = m_bHasTessellation ? EShaderStage_Hull | EShaderStage_Vertex | EShaderStage_Pixel : EShaderStage_Vertex | EShaderStage_Pixel;
	commandList.SetInlineConstantBuffer(EResourceLayoutSlot_PerInstanceCB, m_perInstanceCB, eConstantBufferShaderSlot_PerInstance, perInstanceCBShaderStages);

	{
#ifndef _RELEASE
		if (m_vertexStreamSet)
		{
			if (!!m_vertexStreamSet[VSF_HWSKIN_INFO])
			{
				CD3D9Renderer* pRenderer = gcpRendD3D;
				SRenderPipeline& rp = pRenderer->m_RP;

				CryInterlockedIncrement(&(rp.m_PS[rp.m_nProcessThreadID].m_NumRendSkinnedObjects));
			}
		}
#endif

		commandList.SetVertexBuffers(m_nVertexStreamSetSize, m_vertexStreamSet);

		if (m_indexStreamSet == nullptr)
		{
			if (CRenderer::CV_r_NoDraw != 3)
			{
				commandList.Draw(m_drawParams[drawParamsIndex].m_nVerticesCount, 1, 0, 0);
			}

			return;
		}

		commandList.SetIndexBuffer(m_indexStreamSet);

		if (CRenderer::CV_r_NoDraw != 3)
		{
			if (!m_InstancingCBs.empty())
			{
				// Render instanced draw calls.
				for (auto& ID : m_InstancingCBs)
				{
					commandList.SetInlineConstantBuffer(EResourceLayoutSlot_PerInstanceCB, ID.m_pConstBuffer, eConstantBufferShaderSlot_PerInstance, EShaderStage_Vertex | EShaderStage_Pixel);
					commandList.DrawIndexed(m_drawParams[drawParamsIndex].m_nNumIndices, ID.m_nInstances, m_drawParams[drawParamsIndex].m_nStartIndex, 0, 0);
				}
				return;
			}

			{
				commandList.DrawIndexed(m_drawParams[drawParamsIndex].m_nNumIndices, 1, m_drawParams[drawParamsIndex].m_nStartIndex, 0, 0);
				return;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPermanentRenderObject::~CPermanentRenderObject()
{
	// Free compiled objects
	for (int i = 0, num = m_permanentRenderItems[eRenderPass_Shadows].size(); i < num; i++)
	{
		const SPermanentRendItem& pri = m_permanentRenderItems[eRenderPass_Shadows][i];
		if (pri.m_pCompiledObject && !pri.m_pCompiledObject->m_bSharedWithShadow) // Shared object will be released in the general items
		{
			CCompiledRenderObject::FreeToPool(pri.m_pCompiledObject);
		}
	}
	for (int i = 0, num = m_permanentRenderItems[eRenderPass_General].size(); i < num; i++)
	{
		const SPermanentRendItem& pri = m_permanentRenderItems[eRenderPass_General][i];
		if (pri.m_pCompiledObject)
		{
			CCompiledRenderObject::FreeToPool(pri.m_pCompiledObject);
		}
	}

	if (m_pNextPermanent)
	{
		FreeToPool(m_pNextPermanent);
		m_pNextPermanent = nullptr;
	}
}

CPermanentRenderObject* CPermanentRenderObject::AllocateFromPool()
{
	CPermanentRenderObject* pObject = s_pPools->m_permanentRenderObjectsPool.New();

	return pObject;
}

void CPermanentRenderObject::FreeToPool(CPermanentRenderObject* pObj)
{
	s_pPools->m_permanentRenderObjectsPool.Delete(static_cast<CPermanentRenderObject*>(pObj));
}
