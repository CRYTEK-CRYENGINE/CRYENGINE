// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "ComputeSkinning.h"
#include "../../Common/ComputeSkinningStorage.h"
#include "../../Common/RenderPipeline.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

CComputeSkinningStage::CComputeSkinningStage()
{

}

void CComputeSkinningStage::Init()
{

}

void CComputeSkinningStage::Prepare(CRenderView* pRenderView)
{

}

void CComputeSkinningStage::Execute(CRenderView* pRenderView)
{
	if (pRenderView->IsRecursive())
		return;

	PROFILE_LABEL_SCOPE("CHARACTER_DEFORMATION");

	DispatchComputeShaders(pRenderView);

#ifndef _RELEASE
	ICVar* cvar_gdDebugDraw = gEnv->pConsole->GetCVar("r_ComputeSkinningDebugDraw");
	if (cvar_gdDebugDraw && cvar_gdDebugDraw->GetIVal())
		gcpRendD3D->GetComputeSkinningStorage()->DebugDraw();
#endif
}

void CComputeSkinningStage::DispatchComputeShaders(CRenderView* pRenderView)
{
	CShader* pShader = CShaderMan::s_shaderComputeSkinning;
	if (!pShader)
		return;

	ICVar* cvar_gdMorphs = gEnv->pConsole->GetCVar("r_ComputeSkinningMorphs");
	ICVar* cvar_gdTangents = gEnv->pConsole->GetCVar("r_ComputeSkinningTangents");

	bool bDoPreMorphs = cvar_gdMorphs && cvar_gdMorphs->GetIVal();
	bool bDoTangents = cvar_gdTangents && cvar_gdTangents->GetIVal();

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	D3DDeviceContext* ctx = gcpRendD3D->GetDeviceContext().GetRealDeviceContext();

	auto& list = gcpRendD3D.GetComputeSkinningDataListRT();
	for (auto iter = list.begin(); iter != list.end(); ++iter)
	{
		SSkinningData* pSD = *iter;
		CRenderMesh* pRenderMesh = static_cast<CRenderMesh*>(pSD->pRenderMesh);

		CGpuBuffer* bindPoseVerticesSRV = rd->GetComputeSkinningStorage()->GetResource(pRenderMesh->m_skinBindPoseVerticesSRVHandle);
		CGpuBuffer* boneInfSRV = rd->GetComputeSkinningStorage()->GetResource(pRenderMesh->m_skinBoneInfluencesSRVHandle);
		CGpuBuffer* boneInfMapSRV = rd->GetComputeSkinningStorage()->GetResource(pRenderMesh->m_skinBoneInfluencesMapSRVHandle);
		CGpuBuffer* indicesSRV = rd->GetComputeSkinningStorage()->GetResource(pRenderMesh->m_skinIndicesSRVHandle);
		CGpuBuffer* triangleAdjacencySRV = rd->GetComputeSkinningStorage()->GetResource(pRenderMesh->m_skinTriangleAdjacencySRVHandle);

		if (!(bindPoseVerticesSRV &&
		      boneInfSRV &&
		      boneInfMapSRV &&
		      indicesSRV &&
		      triangleAdjacencySRV &&
		      pRenderMesh->m_outVerticesUAV.size())
		    )
		{
			continue;
		}

		// bind output skinning
		CRenderMesh::CSSkinOutputMap::iterator it = pRenderMesh->m_outVerticesUAV.find(pSD->pCustomTag);
		CGpuBuffer* skinningUAVBuffer = NULL;
		CGpuBuffer* tangentsUAVBuffer = NULL;
		if (it != pRenderMesh->m_outVerticesUAV.end())
		{
			skinningUAVBuffer = gRenDev->GetComputeSkinningStorage()->GetResource((*it).second.skinnedUAVHandle);
			tangentsUAVBuffer = gRenDev->GetComputeSkinningStorage()->GetResource((*it).second.tangentsUAVHandle);
		}
		else
			continue;

		if (!(skinningUAVBuffer && tangentsUAVBuffer))
			continue;

		rd->m_DevMan.BindUAV(CDeviceManager::TYPE_CS, skinningUAVBuffer->GetUAV(), 1, 0);

		// bind input skinning buffers
		ID3D11ShaderResourceView* pSkinningSRVs[3] = {
			bindPoseVerticesSRV->GetSRV(),
			boneInfSRV->GetSRV(),
			boneInfMapSRV->GetSRV()
		};
		rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pSkinningSRVs, 0, 3);

		// bind morphs if needed
		if (bDoPreMorphs && (pSD->nHWSkinningFlags & eHWS_DC_Deformation_PreMorphs))
		{
			CGpuBuffer* morphDeltasSRV = gRenDev->GetComputeSkinningStorage()->GetResource(pRenderMesh->m_vertexDeltasSRVHandle);
			CGpuBuffer* morphDeltasBitfieldSRV = gRenDev->GetComputeSkinningStorage()->GetResource(pRenderMesh->m_vertexMorphsBitfieldSRVHandle);

			CD3D9Renderer::SCharacterInstanceCB* cicb = alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSD->pCharInstCB);
			if (morphDeltasSRV && morphDeltasBitfieldSRV && cicb->activeMorphsBuffer.m_numElements)
			{
				ID3D11ShaderResourceView* pMorphsSRVs[3] = {
					morphDeltasSRV->GetSRV(),
					morphDeltasBitfieldSRV->GetSRV(),
					cicb->activeMorphsBuffer.GetSRV()
				};
				rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pMorphsSRVs, 3, 3);
			}
		}

		// bind transform bones
		CConstantBuffer* pCBBuffer[2] = { NULL, NULL };
		pCBBuffer[0] = alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSD->pCharInstCB)->boneTransformsBuffer;
		if (pSD->pPreviousSkinningRenderData)
		{
			pCBBuffer[1] = alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSD->pPreviousSkinningRenderData->pCharInstCB)->boneTransformsBuffer;
		}
		rd->m_DevMan.BindCBs(CDeviceManager::TYPE_CS, pCBBuffer, 9, 2);

		// dispatch skinning and morphs
		const bool shaderAvailable = SD3DPostEffectsUtils::ShBeginPass(pShader, CCryNameTSCRC("DeformTechnique"), FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
		if (shaderAvailable)
		{
			uint numMorphMaskBits = (pRenderMesh->m_nMorphs + 31) & ~31;
			uint numMorphMasks = numMorphMaskBits >> 5;

			// morphs are applied pre-skinning so for post-skinning morphs, one has to send a trigger
			// to the GPU and replace the #define PRE_MORPH inside the shader with an if()
			Vec4 params;
			params[0] = (f32) pRenderMesh->GetVerticesCount();
			params[1] = cvar_gdMorphs->GetIVal() ? (f32) pSD->nNumActiveMorphs : 0;
			params[2] = (f32) pRenderMesh->m_nMorphs;
			params[3] = (f32) numMorphMasks;

			pShader->FXSetCSFloat(CCryNameR("g_DeformDispatchParams"), alias_cast<Vec4*>(&params), 1);
			rd->FX_Commit();
			rd->m_DevMan.Dispatch((pRenderMesh->GetVerticesCount() + 63) >> 6, 1, 1);

			SD3DPostEffectsUtils::ShEndPass();

#ifndef _RELEASE
			rRP.m_PS[rRP.m_nProcessThreadID].m_NumRendSkinnedObjects++;
#endif
		}

		// dispatch tangents
		if (bDoTangents && (pSD->nHWSkinningFlags & eHWS_DC_Deformation_Tangents))
		{
			// input
			ID3D11ShaderResourceView* pTriangleTangentsSRVs[1] = { indicesSRV->GetSRV() };
			rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pTriangleTangentsSRVs, 6, 1);

			// output
			rd->m_DevMan.BindUAV(CDeviceManager::TYPE_CS, skinningUAVBuffer->GetUAV(), 1, 0);
			rd->m_DevMan.BindUAV(CDeviceManager::TYPE_CS, tangentsUAVBuffer->GetUAV(), 1, 1);

			// calculate triangles TN
			if (SD3DPostEffectsUtils::ShBeginPass(pShader, CCryNameTSCRC("TangentsTechnique"), FEF_DONTSETTEXTURES | FEF_DONTSETSTATES, 0))
			{
				uint32 numTriangles = pRenderMesh->GetIndicesCount() / 3;
				Vec4 params;
				params[0] = (f32) numTriangles;
				params[1] = 0;
				params[2] = 0;
				params[3] = 0;

				pShader->FXSetCSFloat(CCryNameR("g_DeformDispatchParams"), alias_cast<Vec4*>(&params), 1);

				rd->FX_Commit();
				rd->m_DevMan.Dispatch((numTriangles + 63) >> 6, 1, 1);

				SD3DPostEffectsUtils::ShEndPass();
			}

			ID3D11ShaderResourceView* pVertexTangentsSRVs[1] = { triangleAdjacencySRV->GetSRV() };
			rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pVertexTangentsSRVs, 7, 1);
			rd->m_DevMan.BindUAV(CDeviceManager::TYPE_CS, tangentsUAVBuffer->GetUAV(), 1, 1);

			// calculate vertices TN
			if (SD3DPostEffectsUtils::ShBeginPass(pShader, CCryNameTSCRC("TangentsTechnique"), FEF_DONTSETTEXTURES | FEF_DONTSETSTATES, 1))
			{
				Vec4 params;
				params[0] = (f32) pRenderMesh->GetVerticesCount();
				params[1] = 0;
				params[2] = 0;
				params[3] = 0;
				pShader->FXSetCSFloat(CCryNameR("g_DeformDispatchParams"), alias_cast<Vec4*>(&params), 1);

				rd->FX_Commit();
				rd->m_DevMan.Dispatch((pRenderMesh->GetVerticesCount() + 63) >> 6, 1, 1);

				SD3DPostEffectsUtils::ShEndPass();
			}
		}
	}

	// clean up
	CConstantBuffer* pCBBuffer[2] = { NULL, NULL };
	ID3D11UnorderedAccessView* pUAVNull[1] = { NULL };
	ID3D11ShaderResourceView* pSRVNull[7] = { NULL };
	rd->m_DevMan.BindUAV(CDeviceManager::TYPE_CS, pUAVNull, NULL, 0, 1);
	rd->m_DevMan.BindUAV(CDeviceManager::TYPE_CS, pUAVNull, NULL, 1, 1);

	rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pSRVNull, 0, 7);
	rd->m_DevMan.BindCBs(CDeviceManager::TYPE_CS, pCBBuffer, 9, 2);
	CHWShader_D3D::mfBindCS(NULL, NULL);

	rd->FX_Commit();
	rd->m_DevMan.CommitDeviceStates();
}
