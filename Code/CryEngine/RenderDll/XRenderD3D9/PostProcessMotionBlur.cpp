// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessMotionBlur : camera/object motion blur post processing

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"
#include "D3DStereo.h"

#pragma warning(push)
#pragma warning(disable: 4244)

CMotionBlur::OMBParamsMap CMotionBlur::m_pOMBData[3];
CThreadSafeRendererContainer<CMotionBlur::OMBParamsMap::value_type> CMotionBlur::m_FillData[RT_COMMAND_BUF_COUNT];

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CMotionBlur::GetPrevObjToWorldMat(CRenderObject* pObj, Matrix44A& res)
{
	assert(pObj);

	if (pObj->m_ObjFlags & FOB_HAS_PREVMATRIX)
	{
		uint32 nThreadID = gRenDev->GetRenderThreadID();
		SRenderObjData* const __restrict pOD = pObj->GetObjData();

		const uintptr_t ObjID = pOD ? pOD->m_uniqueObjectId : 0;
		const uint32 nFrameID = gRenDev->GetRenderFrameID();
		const uint32 nObjFrameReadID = (nFrameID - 1) % 3;

		OMBParamsMapItor it = m_pOMBData[nObjFrameReadID].find(ObjID);
		if (it != m_pOMBData[nObjFrameReadID].end())
		{
			res = it->second.mObjToWorld;
			return true;
		}
	}

	res = pObj->GetMatrix(gcpRendD3D->GetObjectAccessorThreadConfig());
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMotionBlurDiscardCheck
{
	SMotionBlurDiscardCheck(uint32 nDiscardThreshold, uint32 nFrameID) : m_nDiscardThreshold(nDiscardThreshold), m_nFrameID(nFrameID){}

	bool operator()(const VectorMap<uintptr_t, SObjMotionBlurParams>::value_type& rObj) const
	{
		return (m_nFrameID - rObj.second.nFrameUpdateID) > m_nDiscardThreshold;
	}

private:
	uint32 m_nDiscardThreshold;
	uint32 m_nFrameID;
};

void CMotionBlur::OnBeginFrame(const SRenderingPassInfo& passInfo)
{
	assert(!gRenDev->m_pRT || gRenDev->m_pRT->IsMainThread());

	const uint32 nDiscardThreshold = 60;
	const uint32 nFrameID = gRenDev->GetMainFrameID();
	const uint32 nObjFrameReadID = nFrameID % 3;

	m_pOMBData[nObjFrameReadID].erase_if(SMotionBlurDiscardCheck(nDiscardThreshold, nFrameID));
}

void CMotionBlur::InsertNewElements()
{
	uint32 nThreadID = gRenDev->GetRenderThreadID();
	if (m_FillData[nThreadID].empty())
		return;

	const uint32 nFrameID = gRenDev->GetRenderFrameID();
	const uint32 nObjFrameWriteID = (nFrameID - 1) % 3;

	m_FillData[nThreadID].CoalesceMemory();
	m_pOMBData[nObjFrameWriteID].insert(&m_FillData[nThreadID][0], &m_FillData[nThreadID][0] + m_FillData[nThreadID].size());
	m_FillData[nThreadID].resize(0);
}

void CMotionBlur::FreeData()
{
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		m_FillData[i].clear();

	for (size_t i = 0; i < CRY_ARRAY_COUNT(m_pOMBData); ++i)
	{
		stl::reconstruct(m_pOMBData[i]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CMotionBlur::Preprocess(const SRenderViewInfo& viewInfo)
{
	// skip LDR processing, this is done during HDR
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CMotionBlur::RenderObjectsVelocity()
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

float ComputeMotionScale()
{
	// The length of the generated motion vectors is proportional to the current time step, so we need
	// to rescale the motion vectors to simulate a constant camera exposure time

	float exposureTime = 1.0f / std::max(CRenderer::CV_r_MotionBlurShutterSpeed, 1e-6f);
	float timeStep = std::max(gEnv->pTimer->GetFrameTime(), 1e-6f);

	exposureTime *= gEnv->pTimer->GetTimeScale();

	return exposureTime / timeStep;
}

void CMotionBlur::Render()
{
	// OLD PIPELINE
	ASSERT_LEGACY_PIPELINE
	/*
	CPostEffectsMgr* pPostMgr = PostEffectMgr();
	CShader* pSH = CShaderMan::s_shPostMotionBlur;

	const int nThreadID = gRenDev->GetRenderThreadID();
	gRenDev->m_cEF.mfRefreshSystemShader("MotionBlur", CShaderMan::s_shPostMotionBlur);

	gcpRendD3D->FX_SetActiveRenderTargets();

	CTexture* pSceneHalfRes = CRendererResources::s_ptexHDRTargetScaled[0];

	const bool bAllowObjMotionBlur = CRenderer::CV_r_MotionBlur >= 2;

	int iTempX, iTempY, iWidth, iHeight;
	gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2]);

	Matrix44A mViewProjPrev = CMotionBlur::GetPrevView();
	Matrix44 mViewProj = GetUtils().m_pView;
	mViewProjPrev = mViewProjPrev * GetUtils().m_pProj * GetUtils().m_pScaleBias;
	mViewProjPrev.Transpose();

	CDepthOfField* pDofRenderTech = (CDepthOfField*)pPostMgr->GetEffect(EPostEffectID::DepthOfField);
	SDepthOfFieldParams pDofParams = pDofRenderTech->GetParams();
	pDofParams.vFocus.w *= 2.0f;

	const bool bSpriteDofEnabled = CRenderer::CV_r_dof > 0 && CRenderer::CV_r_DofMode == 0 && pDofParams.vFocus.w > 0.0001f;
	const bool bGatherDofEnabled = CRenderer::CV_r_dof > 0 && CRenderer::CV_r_DofMode == 1 && pDofParams.vFocus.w > 0.0001f;

	// prebake parameters in cpu side
	Vec4 vFocus = pDofParams.vFocus;

	if (pDofParams.bGameMode)
	{
		pDofParams.vFocus.x = 1.0f / (vFocus.z + 1e-6f);
		pDofParams.vFocus.y = -vFocus.y / (vFocus.z + 1e-6f);

		pDofParams.vFocus.z = -1.0f / (vFocus.w + 1e-6f);
		pDofParams.vFocus.w = vFocus.x / (vFocus.w + 1e-6f);
	}
	else
	{
		pDofParams.vFocus.x = 1.0f / (vFocus.y + 1e-6f);
		pDofParams.vFocus.y = -vFocus.z / (vFocus.y + 1e-6f);

		pDofParams.vFocus.z = 1.0f / (vFocus.x + 1e-6f);
		pDofParams.vFocus.w = -vFocus.z / (vFocus.x + 1e-6f);
	}

	const Vec4 vDofParams0 = pDofParams.vFocus;
	const float fNearestDofScaleBoost = (pDofParams.bGameMode) ? CRenderer::CV_r_dofMinZBlendMult : 1.0f;
	const Vec4 vDofParams1 = Vec4(CRenderer::CV_r_dofMinZ + pDofParams.vMinZParams.x, CRenderer::CV_r_dofMinZScale + pDofParams.vMinZParams.y, fNearestDofScaleBoost, vFocus.w);
	const EShaderQuality nShaderQuality = gRenDev->EF_GetShaderQuality(eST_PostProcess);

	if (CRenderer::CV_r_MotionBlur && !gRenDev->m_nDisableTemporalEffects)
	{
		PROFILE_LABEL_SCOPE("MB");

		CTexture* pVelocityRT = CRendererResources::s_ptexVelocity;
		float tileCountX = (float)CRendererResources::s_ptexVelocityTiles[1]->GetWidth();
		float tileCountY = (float)CRendererResources::s_ptexVelocityTiles[1]->GetHeight();

		const float fMaxRange = 32.0f;
		const float fAmount = clamp_tpl<float>(m_pRadBlurAmount->GetParam() / fMaxRange, 0.0f, 1.0f);
		const float fRadius = 1.0f / clamp_tpl<float>(m_pRadBlurRadius->GetParam(), 1e-6f, 2.0f);
		const Vec4 vDirBlur = m_pDirectionalBlurVec->GetParamVec4();
		const Vec4 vDirBlurParam = Vec4(vDirBlur.x * (fMaxRange / (float)iWidth), vDirBlur.y * (fMaxRange / (float)iHeight), (float) iWidth / (float) iHeight, 1.0f);
		const Vec4 vRadBlurParam = Vec4(m_pRadBlurScreenPosX->GetParam() * vDirBlurParam.z, m_pRadBlurScreenPosY->GetParam(), fRadius * fAmount, fAmount);
		if (fAmount + (vDirBlur.x * vDirBlur.x) + (vDirBlur.y * vDirBlur.y) > 1.0f / (float) iWidth)
			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

		{
			PROFILE_LABEL_SCOPE("VELOCITY BUFFER");

			{
				PROFILE_LABEL_SCOPE("PACK VELOCITY");
				gcpRendD3D->FX_PushRenderTarget(0, pVelocityRT, NULL);  // xy = v, z = ||v|| , w = 8 bit z

				GetUtils().ShBeginPass(pSH, m_pPackVelocities, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
				gRenDev->FX_SetState(GS_NODEPTHTEST);
				mViewProjPrev.Transpose();
				pSH->FXSetPSFloat(m_pViewProjPrevParam, (Vec4*)mViewProjPrev.GetData(), 4);
				pSH->FXSetPSFloat(m_pDirBlurParamName, &vDirBlurParam, 1);
				pSH->FXSetPSFloat(m_pRadBlurParamName, &vRadBlurParam, 1);

				const Vec4 vMotionBlurParams = Vec4(ComputeMotionScale(), 1.0f / tileCountX, 1.0f / tileCountX * CRenderer::CV_r_MotionBlurCameraMotionScale, 0);
				pSH->FXSetPSFloat(m_pMotionBlurParamName, &vMotionBlurParams, 1);

				GetUtils().SetTexture(CRendererResources::s_ptexZTarget, 0, FILTER_POINT);
				GetUtils().SetTexture(CRendererResources::s_ptexHDRTarget, 1, FILTER_POINT);
				GetUtils().SetTexture(GetUtils().GetVelocityObjectRT(), 2, FILTER_POINT);

				SD3DPostEffectsUtils::DrawFullScreenTriWPOS(CRendererResources::s_ptexSceneTarget->GetWidth(), CRendererResources::s_ptexSceneTarget->GetHeight());
				GetUtils().ShEndPass();

				RenderObjectsVelocity();
				gcpRendD3D->FX_PopRenderTarget(0);
			}

			if (CRenderer::CV_r_MotionBlur && !gRenDev->m_nDisableTemporalEffects)
			{
				PROFILE_LABEL_SCOPE("VELOCITY TILES");

				static CCryNameTSCRC techVelocityTileGen("VelocityTileGen");
				static CCryNameTSCRC techTileNeighborhood("VelocityTileNeighborhood");

				gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

				// Tile generation first pass
				{
					gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexVelocityTiles[0], 0);
					GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, techVelocityTileGen, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

					Vec4 vParams = Vec4(pVelocityRT->GetWidth(), pVelocityRT->GetHeight(), ceilf((float)gcpRendD3D->GetWidth() / tileCountX), 0);
					CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pMotionBlurParamName, &vParams, 1);
					GetUtils().SetTexture(pVelocityRT, 0, FILTER_POINT);

					SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexVelocityTiles[0]->GetWidth(), CRendererResources::s_ptexVelocityTiles[0]->GetHeight(), 0);

					GetUtils().ShEndPass();
					gcpRendD3D->FX_PopRenderTarget(0);
				}

				// Tile generation second pass
				{
					gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexVelocityTiles[1], 0);
					GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, techVelocityTileGen, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

					Vec4 vParams = Vec4(CRendererResources::s_ptexVelocityTiles[0]->GetWidth(), CRendererResources::s_ptexVelocityTiles[0]->GetHeight(), ceilf((float)gcpRendD3D->GetHeight() / tileCountY), 1);
					CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pMotionBlurParamName, &vParams, 1);
					GetUtils().SetTexture(CRendererResources::s_ptexVelocityTiles[0], 0, FILTER_POINT);

					SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexVelocityTiles[1]->GetWidth(), CRendererResources::s_ptexVelocityTiles[1]->GetHeight(), 0);

					GetUtils().ShEndPass();
					gcpRendD3D->FX_PopRenderTarget(0);
				}

				// Neighborhood max
				{
					gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexVelocityTiles[2], 0);
					GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, techTileNeighborhood, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

					Vec4 vParams = Vec4(1.0f / tileCountX, 1.0f / tileCountY, 0, 0);
					CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pMotionBlurParamName, &vParams, 1);
					GetUtils().SetTexture(CRendererResources::s_ptexVelocityTiles[1], 0, FILTER_POINT);

					SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexVelocityTiles[2]->GetWidth(), CRendererResources::s_ptexVelocityTiles[2]->GetHeight(), 0);

					GetUtils().ShEndPass();
					gcpRendD3D->FX_PopRenderTarget(0);
				}
			}
		}

		////////////////////////////////////////////////////////////////////////////////////////////////////////////

		gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3] | g_HWSR_MaskBit[HWSR_SAMPLE4]);

		{
			PROFILE_LABEL_SCOPE("MOTION VECTOR APPLY");

			gRenDev->m_RP.m_FlagsShader_RT |= (CRenderer::CV_r_MotionBlurQuality >= 2) ? g_HWSR_MaskBit[HWSR_SAMPLE2] : 0;
			gRenDev->m_RP.m_FlagsShader_RT |= (CRenderer::CV_r_MotionBlurQuality == 1) ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;

			if (bGatherDofEnabled)
				GetUtils().StretchRect(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexSceneTargetR11G11B10F[0]);

			gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexHDRTarget, 0);
			GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, m_pMotionBlurTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

			Vec4 vParams = Vec4(1.0f / tileCountX, 1.0f / tileCountY, 0, 0);
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pMotionBlurParamName, &vParams, 1);

			GetUtils().SetTexture(bGatherDofEnabled ? CRendererResources::s_ptexSceneTargetR11G11B10F[0] : CRendererResources::s_ptexHDRTargetPrev, 0, FILTER_LINEAR);
			GetUtils().SetTexture(pVelocityRT, 1, FILTER_POINT);
			GetUtils().SetTexture(CRendererResources::s_ptexVelocityTiles[2], 2, FILTER_POINT);

			gcpRendD3D->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA);
			SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexSceneTarget->GetWidth(), CRendererResources::s_ptexSceneTarget->GetHeight(), 0);

			GetUtils().ShEndPass();
			gcpRendD3D->FX_PopRenderTarget(0);
			gcpRendD3D->FX_SetActiveRenderTargets();
		}
	}

	// restore previous frame data
	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;

	if (bSpriteDofEnabled)
	{
		///////////////////////////////////////////////////////////////////////////////////////////////////
		// Apply depth of field / masked blur

		{
			PROFILE_LABEL_SCOPE("SPRITE DOF");

			// TODO: Avoid this copy
			GetUtils().StretchRect(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexSceneTarget);

			GetUtils().StretchRect(CRendererResources::s_ptexHDRTarget, pSceneHalfRes, false, false, false, true); // 0.25
			{
				PROFILE_LABEL_SCOPE("QUAD BOKEH DOF");
				const uint32 nWidth = CRendererResources::s_ptexHDRDofLayers[0]->GetWidth();
				const uint32 nHeight = CRendererResources::s_ptexHDRDofLayers[0]->GetHeight();
				const uint32 nBokehQuadsCount = nWidth * nHeight * 3 * 2;
				const float fBokehNormFactor = (float)(640 * 360 * 3 * 2) / (float)nBokehQuadsCount;  // Assume tweaked for 1600x900 with render target scale of 0.8

				gcpRendD3D->FX_ClearTarget(CRendererResources::s_ptexHDRDofLayers[0], Clr_Transparent);
				gcpRendD3D->FX_ClearTarget(CRendererResources::s_ptexHDRDofLayers[1], Clr_Transparent);
				gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexHDRDofLayers[0], NULL);
				gcpRendD3D->FX_PushRenderTarget(1, CRendererResources::s_ptexHDRDofLayers[1], NULL);

				gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1]);
				if (CRenderer::CV_r_dofDilation > 0.001f)
					gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

				static CCryNameTSCRC pszTech("DofBokehRef");
				CShaderMan::s_shPostMotionBlur->FXSetTechnique(pszTech);
				uint32 nPasses(0);
				CShaderMan::s_shPostMotionBlur->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
				CShaderMan::s_shPostMotionBlur->FXBeginPass(0);

				int32 nRenderState = GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE;
				gcpRendD3D->SetCullMode(R_CULL_BACK);
				gcpRendD3D->FX_SetState(nRenderState);

				// bind vertex textures for dof splats
				pSceneHalfRes->SetVertexTexture(true);
				GetUtils().SetTexture(pSceneHalfRes, 0, FILTER_POINT);
				CRendererResources::s_ptexZTarget->SetVertexTexture(true);
				GetUtils().SetTexture(CRendererResources::s_ptexZTarget, 1, FILTER_POINT);

				pSceneHalfRes->SetVertexTexture(false);
				GetUtils().SetTexture(pSceneHalfRes, 0, FILTER_POINT);
				CRendererResources::s_ptexZTarget->SetVertexTexture(false);
				GetUtils().SetTexture(CRendererResources::s_ptexZTarget, 1, FILTER_POINT);
				GetUtils().SetTexture(CRendererResources::s_ptexGrainFilterMap, 2, FILTER_POINT);
				if (m_pBokehShape)
					GetUtils().SetTexture(m_pBokehShape, 3, FILTER_TRILINEAR);

				static CCryNameR pAspectRatioName("vAspectRatio");
				Vec4 vAspectRatioMul = Vec4(CRendererResources::s_ptexHDRTarget->GetWidth(), CRendererResources::s_ptexHDRTarget->GetHeight(), 1.0f, 1.0f);
				const float fAspectRatio = (1920.0f / 1080.0f) * (vAspectRatioMul.y / vAspectRatioMul.x);
				vAspectRatioMul = Vec4(fAspectRatio * (vAspectRatioMul.x / 1920.0f), vAspectRatioMul.y / 1080.0f, fBokehNormFactor / 80.0f, CRenderer::CV_r_dofDilation);
				CShaderMan::s_shPostMotionBlur->FXSetVSFloat(pAspectRatioName, &vAspectRatioMul, 1);
				CShaderMan::s_shPostMotionBlur->FXSetPSFloat(pAspectRatioName, &vAspectRatioMul, 1);

				CShaderMan::s_shPostMotionBlur->FXSetVSFloat(m_pDofFocusParam0Name, &vDofParams0, 1);
				CShaderMan::s_shPostMotionBlur->FXSetVSFloat(m_pDofFocusParam1Name, &vDofParams1, 1);
				CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam0Name, &vDofParams0, 1);
				CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam1Name, &vDofParams1, 1);

				gcpRendD3D->FX_Commit();
				if (!FAILED(gcpRendD3D->FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3S_C4B_T2S)))
				{
					gcpRendD3D->FX_SetVStream(0, NULL, 0, 0);
					gcpRendD3D->FX_SetIStream(0, 0, Index16);
					gcpRendD3D->FX_DrawPrimitive(eptTriangleList, 0, nBokehQuadsCount);
				}

				CShaderMan::s_shPostMotionBlur->FXEndPass();
				CShaderMan::s_shPostMotionBlur->FXEnd();

				pSceneHalfRes->SetVertexTexture(false);
				CRendererResources::s_ptexZTarget->SetVertexTexture(false);

				gcpRendD3D->FX_PopRenderTarget(1);
				gcpRendD3D->FX_PopRenderTarget(0);
			}

			{
				PROFILE_LABEL_SCOPE("BOKEH DOF COMPOSE");
				gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexHDRTarget, NULL);  // Apply normalization+composition pass

				GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, m_pRefDofMBNormalize, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
				gcpRendD3D->SetCullMode(R_CULL_BACK);
				gcpRendD3D->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
				CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam0Name, &vDofParams0, 1);
				CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam1Name, &vDofParams1, 1);

				GetUtils().SetTexture(CRendererResources::s_ptexHDRDofLayers[0], 0, FILTER_LINEAR);
				GetUtils().SetTexture(CRendererResources::s_ptexHDRDofLayers[1], 1, FILTER_LINEAR);
				GetUtils().SetTexture(CRendererResources::s_ptexSceneTarget, 2, FILTER_POINT);
				GetUtils().SetTexture(CRendererResources::s_ptexZTarget, 3, FILTER_POINT);

				SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexHDRTarget->GetWidth(), CRendererResources::s_ptexHDRTarget->GetHeight(), 0, &gcpRendD3D->m_FullResRect);

				GetUtils().ShEndPass();
				gcpRendD3D->FX_PopRenderTarget(0);


			}
		}

		CRendererResources::s_ptexHDRTarget->SetResolved(true);
	}

	gcpRendD3D->FX_SetActiveRenderTargets();
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);
	*/
}

#pragma warning(pop)
