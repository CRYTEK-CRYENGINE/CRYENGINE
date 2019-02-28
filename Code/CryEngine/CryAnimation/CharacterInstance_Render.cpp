// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IShader.h>
#include "ModelMesh.h"
#include "CharacterInstance.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "CharacterManager.h"

CLodValue CCharInstance::ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo)
{
	// If the character instance has software vertex animation and ca_vaSkipVertexAnimationLod is set
	// we skip the first LOD to disable software skinning
	if (m_bHasVertexAnimation && wantedLod == 0 && Console::GetInst().ca_vaSkipVertexAnimationLOD == 1)
	{
		return CLodValue(1);
	}

	return CLodValue(wantedLod);
}

//! Render object ( register render elements into renderer )
void CCharInstance::Render(const struct SRendParams& RendParams, const SRenderingPassInfo& passInfo)
{
	if (GetFlags() & CS_FLAG_COMPOUND_BASE)
	{
		if (Console::GetInst().ca_DrawCC == 0)
			return;
	}

	if (m_pDefaultSkeleton->m_ObjectType == CGA)
	{
		if (Console::GetInst().ca_DrawCGA == 0)
			return;
	}

	if (m_pDefaultSkeleton->m_ObjectType == CHR)
	{
		if (Console::GetInst().ca_DrawCHR == 0)
			return;
	}

	if (!(m_rpFlags & CS_FLAG_DRAW_MODEL))
		return;

	assert(RendParams.pMatrix);

	const Matrix34& RenderMat34 = *RendParams.pMatrix;

	//f32 axisX = RenderMat34.GetColumn0().GetLength();
	//f32 axisY = RenderMat34.GetColumn1().GetLength();
	//f32 axisZ = RenderMat34.GetColumn2().GetLength();
	//f32 fScaling = 0.333333333f*(axisX+axisY+axisZ);
	//RenderMat34.OrthonormalizeFast();

	uint32 nFrameID = g_pCharacterManager->m_nUpdateCounter;
	if (m_LastRenderedFrameID != nFrameID)
	{
		m_fZoomDistanceSq = std::numeric_limits<float>::max();
		m_LastRenderedFrameID = nFrameID;
	}
		
	g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);

	if (!passInfo.IsShadowPass())
	{
		m_nAnimationLOD = RendParams.lodValue.LodA();
		if (m_nAnimationLOD == -1)
			m_nAnimationLOD = RendParams.lodValue.LodB();
	}

	//	float fColor[4] = {1,0,1,1};
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.2f, fColor, false,"fDistance: %f m_nAnimationLOD: %d   m_nRenderLOD: %d   numLODs: %d  m_pDefaultSkeleton->m_nBaseLOD: %d  Model: %s",RendParams.fDistance,m_nAnimationLOD,m_nRenderLOD,numLODs,m_pDefaultSkeleton->m_nBaseLOD,m_pDefaultSkeleton->GetFilePath().c_str() );
	//	g_YLine+=16.0f;

	//------------------------------------------------------------------------
	//------------   Debug-Draw of the final Render Location     -------------
	//------------------------------------------------------------------------
	if (Console::GetInst().ca_DrawPositionPost)
	{
		Vec3 wpos = RenderMat34.GetTranslation();
		g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);
		static Ang3 angle(0, 0, 0);
		angle += Ang3(0.01f, 0.02f, 0.03f);
		AABB aabb = AABB(Vec3(-0.055f, -0.055f, -0.055f), Vec3(+0.055f, +0.055f, +0.055f));
		OBB obb = OBB::CreateOBBfromAABB(Matrix33::CreateRotationXYZ(angle), aabb);
		g_pAuxGeom->DrawOBB(obb, wpos, 0, RGBA8(0x00, 0xff, 0x00, 0xff), eBBD_Extremes_Color_Encoded);

		Vec3 axisX = RenderMat34.GetColumn0();
		Vec3 axisY = RenderMat34.GetColumn1();
		Vec3 axisZ = RenderMat34.GetColumn2();
		g_pAuxGeom->DrawLine(wpos, RGBA8(0x7f, 0x00, 0x00, 0x00), wpos + axisX, RGBA8(0xff, 0x00, 0x00, 0x00));
		g_pAuxGeom->DrawLine(wpos, RGBA8(0x00, 0x7f, 0x00, 0x00), wpos + axisY, RGBA8(0x00, 0xff, 0x00, 0x00));
		g_pAuxGeom->DrawLine(wpos, RGBA8(0x00, 0x00, 0x7f, 0x00), wpos + axisZ, RGBA8(0x00, 0x00, 0xff, 0x00));
	}

	//------------------------------------------------------------------------

	SRendParams attachmentRendParams(RendParams);
	{
		uint64 uAdditionalObjFlags = 0;
		if (m_rpFlags & CS_FLAG_DRAW_NEAR)
			uAdditionalObjFlags |= FOB_NEAREST;

		attachmentRendParams.pMaterial = NULL;    // this is required to avoid the attachments using the parent character material (this is the material that overrides the default material in the attachment)
		attachmentRendParams.dwFObjFlags |= uAdditionalObjFlags;
	}

	const f32 fFOV = passInfo.GetCamera().GetFov();
	const f32 fZoomFactor = 0.0f + 1.0f * (RAD2DEG(fFOV) / 60.f);
	const f32 attachmentCullingRation = (gEnv->bMultiplayer) ? Console::GetInst().ca_AttachmentCullingRationMP : Console::GetInst().ca_AttachmentCullingRation;
	m_fZoomDistanceSq = std::fminf(m_fZoomDistanceSq, sqr(RendParams.fDistance * fZoomFactor / attachmentCullingRation));
	const auto& FinalMat = (attachmentRendParams.dwFObjFlags & FOB_NEAREST) != 0 ? *RendParams.pNearestMatrix : RenderMat34;

	m_AttachmentManager.DrawMergedAttachments(attachmentRendParams, FinalMat, passInfo, fZoomFactor, m_fZoomDistanceSq);
	
	if (m_pDefaultSkeleton->m_ObjectType == CGA)
		RenderCGA(RendParams, FinalMat, passInfo);
	else
		RenderCHR(RendParams, FinalMat, passInfo);

	// draw weapon and binded objects
	m_AttachmentManager.DrawAttachments(attachmentRendParams, FinalMat, passInfo, fZoomFactor, m_fZoomDistanceSq);

#ifndef _RELEASE
	// in-game debug rendering of characters attachments proxies 
	const uint32 uiProxies = (uint32)Console::GetInst().ca_DebugAttachmentsProxies;
	m_AttachmentManager.DrawProxies(uiProxies ? uiProxies : 0x80);
#endif
}

void CCharInstance::RenderCGA(const struct SRendParams& RendParams, const Matrix34& RenderMat34, const SRenderingPassInfo& passInfo)
{
	int nList = (int)CharacterManager::GetRendererMainThreadId();

	if (GetSkinningTransformationCount())
		CryFatalError("CryAnimation: CGA should not have Dual-Quaternions for Skinning");

	SRendParams nodeRP = RendParams;

	Matrix34 orthoTm34 = RenderMat34;
	IMaterial* pMaterial = nodeRP.pMaterial;
	uint32 numJoints = m_SkeletonPose.GetPoseData().GetJointCount();
	assert(numJoints > 0);
	if (Console::GetInst().ca_DrawBaseMesh)
	{
		for (uint32 i = 0; i < numJoints; i++)
		{
			if (m_SkeletonPose.m_arrCGAJoints.size() && m_SkeletonPose.m_arrCGAJoints[i].m_CGAObjectInstance)
			{
				const Skeleton::CPoseData& poseData = m_SkeletonPose.GetPoseData();

				Matrix34 tm34 = orthoTm34 * Matrix34(poseData.GetJointAbsolute(i));
				tm34 *= poseData.GetJointAbsoluteS(i);

				nodeRP.pMatrix = &tm34;
				nodeRP.dwFObjFlags |= FOB_TRANS_MASK;
				nodeRP.pInstance = &m_SkeletonPose.m_arrCGAJoints[i];

				// apply additional depth sort offset, if set
				const Vec3 depthSortOffset = (orthoTm34 * Matrix34(poseData.GetJointAbsolute(i))).TransformVector(m_SkeletonPose.m_arrCGAJoints[i].m_CGAObjectInstance->GetDepthSortOffset());
				// TODO: ^ Dot me against the camera's forward vector. Is orthoTm34 already operating in the camera-space?

				// apply custom joint material, if set
				nodeRP.pMaterial = m_SkeletonPose.m_arrCGAJoints[i].m_pMaterial ? m_SkeletonPose.m_arrCGAJoints[i].m_pMaterial.get() : pMaterial;

				if (tm34.IsValid())
				{
					m_SkeletonPose.m_arrCGAJoints[i].m_CGAObjectInstance->Render(nodeRP, passInfo);
				}
				else
				{
					gEnv->pLog->LogError("CCharInstance::RenderCGA: object has invalid matrix: %s", m_pDefaultSkeleton->GetModelFilePath());
				}
			}
		}
	}

	if (Console::GetInst().ca_DrawSkeleton || Console::GetInst().ca_DrawBBox)
	{
		Matrix34 wsRenderMat34(RenderMat34);

		// Convert "Camera Space" to "World Space"
		if (RendParams.dwFObjFlags & FOB_NEAREST)
		{
			wsRenderMat34.AddTranslation(passInfo.GetCamera().GetPosition());
		}

		if (Console::GetInst().ca_DrawSkeleton)
			m_SkeletonPose.DrawSkeleton(wsRenderMat34);

		if (Console::GetInst().ca_DrawBBox)
			m_SkeletonPose.DrawBBox(wsRenderMat34);
	}
}

void CCharInstance::RenderCHR(const SRendParams& inputRendParams, const Matrix34& rRenderMat34, const SRenderingPassInfo& passInfo)
{
	SRendParams RendParams = inputRendParams;

	CRenderObject* pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	if (!pObj)
		return;

	//#TODO Extracted from RenderProxy needed for WrinkleMap, should be implemented during character instance rendering
	/*
	{
		// Make a local copy of render params as some of the parameters will be modified.
		SRendParams rParams(inRenderParams);

		if (m_Callbacks.size() > 0)
		{
			// if we have callbacks be sure the shader params are properly initialized
			IShaderPublicParams* pParams = GetShaderPublicParams(true);

			// get the current active material
			IMaterial* pMaterial = m_pEntity->GetMaterial();
			if (pMaterial == NULL)
				pMaterial = GetSlotMaterial(0);

			TCallbackVector::iterator itEnd = m_Callbacks.end();
			for (TCallbackVector::iterator it = m_Callbacks.begin(); it != itEnd; ++it)
			{
				(*it)->SetupShaderParams(pParams, pMaterial);
			}
		}

		if (m_pShaderPublicParams)
			m_pShaderPublicParams->AssignToRenderParams(rParams);
	}
	*/

	pObj->m_pRenderNode = RendParams.pRenderNode;
	pObj->m_ObjFlags |= FOB_TRANS_MASK;
	pObj->m_editorSelectionID = RendParams.nEditorSelectionID;

	//check if it should be drawn close to the player
	// For nearest geometry (weapons/arms) - make sure its rendered really at beginning (before water passes)
	if ((RendParams.dwFObjFlags & FOB_NEAREST) || (m_rpFlags & CS_FLAG_DRAW_NEAR))
	{
		pObj->m_ObjFlags |= FOB_NEAREST;
		((SRendParams&)RendParams).nAfterWater = 1;
	}
	else
		pObj->m_ObjFlags &= ~FOB_NEAREST;

	pObj->m_fAlpha = RendParams.fAlpha;
	pObj->m_fDistance = RendParams.fDistance;

	pObj->SetAmbientColor(RendParams.AmbientColor, passInfo);

	pObj->m_ObjFlags |= RendParams.dwFObjFlags;
	SRenderObjData* pD = pObj->GetObjData();

	// copy the shaderparams into the render object data from the render params
	if (RendParams.pShaderParams && RendParams.pShaderParams->size() > 0)
	{
		pD->SetShaderParams(RendParams.pShaderParams);
	}

	pObj->SetMatrix(rRenderMat34, passInfo);

	pObj->m_nClipVolumeStencilRef = RendParams.nClipVolumeStencilRef;
	pObj->m_nTextureID = RendParams.nTextureID;

	RendParams.pInstance = this;

	bool bCheckMotion = MotionBlurMotionCheck(pObj->m_ObjFlags);
	pD->m_uniqueObjectId = reinterpret_cast<uintptr_t>(RendParams.pInstance);
	if (bCheckMotion)
		pObj->m_ObjFlags |= FOB_MOTION_BLUR;

	pD->m_nVisionParams = RendParams.nVisionParams;
	pD->m_nHUDSilhouetteParams = RendParams.nHUDSilhouettesParams;

	pObj->m_nMaterialLayers = RendParams.nMaterialLayersBlend;

	pD->m_nCustomData = RendParams.nCustomData;
	pD->m_nCustomFlags = RendParams.nCustomFlags;
	if (RendParams.nCustomFlags & COB_CLOAK_HIGHLIGHT)
	{
		pD->m_fTempVars[5] = RendParams.fCustomData[0];
	}
	else if (RendParams.nCustomFlags & COB_POST_3D_RENDER)
	{
		memcpy(&pD->m_fTempVars[5], &RendParams.fCustomData[0], sizeof(float) * 4);
		pObj->m_fAlpha = 1.0f;     // Use the alpha in the post effect instead of here
		pD->m_fTempVars[9] = RendParams.fAlpha;
	}
	pObj->m_DissolveRef = RendParams.nDissolveRef;

	pD->m_pSkinningData = GetSkinningData(passInfo);
	pObj->m_ObjFlags |= FOB_SKINNED | FOB_INSHADOW;

	Vec3 skinOffset(ZERO);
	if (m_pDefaultSkeleton->GetModelMesh())
		skinOffset = m_pDefaultSkeleton->GetModelMesh()->m_vRenderMeshOffset;
	pD->m_pSkinningData->vecAdditionalOffset = skinOffset;

	if (g_pI3DEngine->IsTessellationAllowed(pObj, passInfo))
	{
		// Allow this RO to be tessellated, however actual tessellation will be applied if enabled in material
		pObj->m_ObjFlags |= FOB_ALLOW_TESSELLATION;
	}

	pObj->m_nSort = fastround_positive(RendParams.fDistance * 2.0f);

	if (!m_bHideMaster)
	{
		if (Console::GetInst().ca_DrawSkeleton)
			m_SkeletonPose.DrawSkeleton(rRenderMat34);
		if (Console::GetInst().ca_DrawBBox)
			m_SkeletonPose.DrawBBox(rRenderMat34);

		if (CModelMesh* pModelMesh = m_pDefaultSkeleton->GetModelMesh())
		{
			pModelMesh->m_stream.nFrameId = passInfo.GetMainFrameID();

			if (IRenderMesh* pIRenderMesh = pModelMesh->m_pIRenderMesh)
			{
				// MichaelS - use the instance's material if there is one, and if no override material has
				// already been specified.
				IMaterial* pMaterial = RendParams.pMaterial;
				if (pMaterial == 0)
					pMaterial = this->GetIMaterial_Instance();
				if (pMaterial == 0)
					pMaterial = m_pDefaultSkeleton->GetIMaterial();
				pObj->m_pCurrMaterial = pMaterial;
#ifndef _RELEASE
				static ICVar* p_e_debug_draw = gEnv->pConsole->GetCVar("e_DebugDraw");
				if (p_e_debug_draw && p_e_debug_draw->GetIVal() != 0)
					pModelMesh->DrawDebugInfo(this->m_pDefaultSkeleton, m_nAnimationLOD, rRenderMat34, p_e_debug_draw->GetIVal(), pMaterial, pObj, RendParams, passInfo.IsGeneralPass(), (IRenderNode*)RendParams.pRenderNode, m_SkeletonPose.GetAABB(),passInfo);
#endif
				//	float fColor[4] = {1,0,1,1};
				//	extern f32 g_YLine;
				//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_nRenderLOD: %d  %d",RendParams.nLodLevel, pObj->m_nLod  ); g_YLine+=0x10;
				if (Console::GetInst().ca_DrawBaseMesh)
				{
					pIRenderMesh->Render(pObj, passInfo);
				}

				if (Console::GetInst().ca_DrawDecalsBBoxes)
				{
					Matrix34 wsRenderMat34(rRenderMat34);
					// Convert "Camera Space" to "World Space"
					if (pObj->m_ObjFlags & FOB_NEAREST)
						wsRenderMat34.AddTranslation(passInfo.GetCamera().GetPosition());
					g_pAuxGeom->SetRenderFlags(e_Def3DPublicRenderflags);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

bool CCharInstance::MotionBlurMotionCheck(uint64 nObjFlags) const
{
	if (m_skinningTransformationsMovement > Console::GetInst().ca_MotionBlurMovementThreshold)
		return true;

	return false;
}

SSkinningData* CCharInstance::GetSkinningData(const SRenderingPassInfo& passInfo)
{
	DEFINE_PROFILER_FUNCTION();

	CAttachmentManager* pAttachmentManager = static_cast<CAttachmentManager*>(GetIAttachmentManager());
	uint32 numSkinningBones = GetSkinningTransformationCount() + pAttachmentManager->GetExtraBonesCount();

	bool bNeedJobSyncVar = true;

	// get data to fill
	int nFrameID = gEnv->pRenderer->EF_GetSkinningPoolID();
	int nList = nFrameID % 3;
	int nPrevList = (nFrameID - 1) % 3;

	// before allocating new skinning date, check if we already have for this frame
	if (arrSkinningRendererData[nList].nFrameID == nFrameID && arrSkinningRendererData[nList].pSkinningData)
	{
		return arrSkinningRendererData[nList].pSkinningData;
	}

	SSkinningData* pSkinningData = gEnv->pRenderer->EF_CreateSkinningData(passInfo.GetIRenderView(), numSkinningBones, bNeedJobSyncVar);
	pSkinningData->pCustomTag = this;
	arrSkinningRendererData[nList].pSkinningData = pSkinningData;
	arrSkinningRendererData[nList].nFrameID = nFrameID;

	assert(pSkinningData);
	PREFAST_ASSUME(pSkinningData);

	// set data for motion blur
	if (arrSkinningRendererData[nPrevList].nFrameID == (nFrameID - 1) && arrSkinningRendererData[nPrevList].pSkinningData)
	{
		pSkinningData->nHWSkinningFlags |= eHWS_MotionBlured;
		pSkinningData->pPreviousSkinningRenderData = arrSkinningRendererData[nPrevList].pSkinningData;
		if (pSkinningData->pPreviousSkinningRenderData->pAsyncJobs)
			gEnv->pJobManager->WaitForJob(*pSkinningData->pPreviousSkinningRenderData->pAsyncJobs);
	}
	else
	{
		// if we don't have motion blur data, use the same as for the current frame
		pSkinningData->pPreviousSkinningRenderData = pSkinningData;
	}

	BeginSkinningTransformationsComputation(pSkinningData);
	pSkinningData->pRenderMesh = GetRenderMesh();
	return pSkinningData;
}
