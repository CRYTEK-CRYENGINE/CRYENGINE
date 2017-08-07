// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessMisc : misc post processing

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"

#include "Common/RenderView.h"

#pragma warning(push)
#pragma warning(disable: 4244)

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CVolumetricScattering::Render()
{
	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Render god-rays into low-res render target for less fillrate hit

	gcpRendD3D->FX_ClearTarget(CTexture::s_ptexBackBufferScaled[1], Clr_Transparent);
	gcpRendD3D->FX_PushRenderTarget(0, CTexture::s_ptexBackBufferScaled[1], NULL);
	gcpRendD3D->RT_SetViewport(0, 0, CTexture::s_ptexBackBufferScaled[1]->GetWidth(), CTexture::s_ptexBackBufferScaled[1]->GetHeight());

	float fAmount = m_pAmount->GetParam();
	float fTilling = m_pTilling->GetParam();
	float fSpeed = m_pSpeed->GetParam();
	Vec4 pColor = m_pColor->GetParamVec4();

	static CCryNameTSCRC pTechName("VolumetricScattering");
	uint32 nPasses;
	CShaderMan::s_shPostEffects->FXSetTechnique(pTechName);
	CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETSTATES);

	gcpRendD3D->SetCullMode(R_CULL_NONE);
	gcpRendD3D->FX_SetState(GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL | GS_NODEPTHTEST);

	int nSlicesCount = 10;

	Vec4 pParams;
	pParams = Vec4(fTilling, fSpeed, fTilling, fSpeed);

	static CCryNameR pParam0Name("VolumetricScattering");
	static CCryNameR pParam1Name("VolumetricScatteringColor");

	static CCryNameR pParam2Name("PI_volScatterParamsVS");
	static CCryNameR pParam3Name("PI_volScatterParamsPS");

	for (int r(0); r < nSlicesCount; ++r)
	{
		// !force updating constants per-pass! (dx10..)
		CShaderMan::s_shPostEffects->FXBeginPass(0);

		// Set PS default params
		Vec4 pParamsPI = Vec4(1.0f, fAmount, r, 1.0f / (float) nSlicesCount);
		CShaderMan::s_shPostEffects->FXSetVSFloat(pParam0Name, &pParams, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pColor, 1);
		CShaderMan::s_shPostEffects->FXSetVSFloat(pParam2Name, &pParamsPI, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam3Name, &pParamsPI, 1);

		GetUtils().DrawFullScreenTri(CTexture::s_ptexBackBufferScaled[1]->GetWidth(), CTexture::s_ptexBackBufferScaled[1]->GetHeight());

		CShaderMan::s_shPostEffects->FXEndPass();
	}

	CShaderMan::s_shPostEffects->FXEnd();

	// Restore previous viewport
	gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Display volumetric scattering effect

	CCryNameTSCRC pTechName0("VolumetricScatteringFinal");

	GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechName0, FEF_DONTSETSTATES);
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	GetUtils().DrawFullScreenTri(CTexture::s_ptexBackBuffer->GetWidth(), CTexture::s_ptexBackBuffer->GetHeight());
	GetUtils().ShEndPass();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPost3DRenderer::Render()
{
	PROFILE_LABEL_SCOPE("POST_3D_RENDERER");

#ifdef SUPPORTS_MSAA
	CTexture::s_ptexZTarget->SetUseMultisampledRTV(false);
	CTexture::s_ptexSceneNormalsMap->SetUseMultisampledRTV(false);
	CTexture::s_ptexSceneDiffuse->SetUseMultisampledRTV(false);
	CTexture::s_ptexZTarget->SetResolved(true);
	CTexture::s_ptexSceneNormalsMap->SetResolved(true);
	CTexture::s_ptexSceneDiffuse->SetResolved(true);
#endif

	// Must update the RT pointers here, otherwise they can get out-of-date
	if (CRenderer::CV_r_UsePersistentRTForModelHUD > 0)
	{
		m_pFlashRT = CTexture::s_ptexModelHudBuffer;
	}
	else
	{
		m_pFlashRT = CTexture::s_ptexBackBuffer;
	}

	m_pTempRT = CTexture::s_ptexSceneDiffuse;
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	m_pTempRT = CTexture::s_ptexSceneNormalsBent; // non-msaaed target
#endif

	if (HasModelsToRender() && IsActive())
	{
		// Render model groups
		m_edgeFadeScale = (1.0f / clamp_tpl<float>(m_pEdgeFadeScale->GetParam(), 0.001f, 1.0f));
		m_post3DRendererflags |= eP3DR_DirtyFlashRT;
		m_groupCount = 1; // There must be at least 1 group, this will then get set the correct amount when processing the models
		for (uint8 groupId = 0; groupId < m_groupCount; groupId++)
		{
			RenderGroup(groupId);
		}
	}
	else
	{
		// Nothing to render, so clear Flash RT so that we don't render rubbish on the flash objects
		ClearFlashRT();
	}

#ifdef SUPPORTS_MSAA
	CTexture::s_ptexZTarget->SetUseMultisampledRTV(true);
	CTexture::s_ptexSceneNormalsMap->SetUseMultisampledRTV(true);
	CTexture::s_ptexSceneDiffuse->SetUseMultisampledRTV(true);
	CTexture::s_ptexZTarget->SetResolved(true);
	CTexture::s_ptexSceneNormalsMap->SetResolved(true);
	CTexture::s_ptexSceneDiffuse->SetResolved(true);
#endif
}

void CPost3DRenderer::ClearFlashRT()
{
	PROFILE_LABEL_SCOPE("CLEAR_RT");

	const uint8 zbufferRenderTile = 2; // Use zbuffer render tile, because in stereo the 1st 2 are used for the eyes

	gcpRendD3D->FX_ClearTarget(m_pFlashRT, Clr_Transparent);
}

void CPost3DRenderer::RenderGroup(uint8 groupId)
{
	PROFILE_LABEL_SCOPE("RENDER_GROUP");

	SRenderPipeline& renderPipeline = gcpRendD3D->m_RP;

	// Reset group vars
	m_post3DRendererflags &= ~eP3DR_HasSilhouettes;
	m_alpha = 1.0f;
	float screenRect[4];
	memset(screenRect, 0, sizeof(screenRect));

	{
		PROFILE_LABEL_SCOPE("RENDER_DEPTH");
		// On PC we render depth separately
		renderPipeline.m_pRenderFunc = gcpRendD3D->FX_FlushShader_ZPass;
		RenderMeshes(groupId, screenRect, eRMM_DepthOnly);
	}

	renderPipeline.m_pRenderFunc = gcpRendD3D->FX_FlushShader_General;

	RenderMeshes(groupId, screenRect);
	AlphaCorrection();
	GammaCorrection(screenRect);

	if (m_post3DRendererflags & eP3DR_HasSilhouettes)
	{
		RenderSilhouettes(groupId, screenRect);
	}
}

void CPost3DRenderer::RenderMeshes(uint8 groupId, float screenRect[4], ERenderMeshMode renderMeshMode)
{
	PROFILE_LABEL_SCOPE("RENDER_MESHES");

	const bool bCustomRender = (renderMeshMode == eRMM_Custom) ? true : false;
	const bool bDefaultRender = (renderMeshMode == eRMM_Default) ? true : false;
	const bool bDepthOnlyRender = (renderMeshMode == eRMM_DepthOnly) ? true : false;
	const bool bDoStencil = bDefaultRender;
	const bool bReverseDepth = (gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;

	gcpRendD3D->FX_ClearTarget(&gcpRendD3D->m_DepthBufferOrig, (bDepthOnlyRender * CLEAR_ZBUFFER) | CLEAR_STENCIL, Clr_FarPlane_R.r, (bDepthOnlyRender * 1));

	if (!bDepthOnlyRender)
	{
		gcpRendD3D->FX_ClearTarget(m_pTempRT, Clr_Transparent);
		gcpRendD3D->FX_PushRenderTarget(0, m_pTempRT, &gcpRendD3D->m_DepthBufferOrig);
		gcpRendD3D->RT_SetViewport(0, 0, m_pTempRT->GetWidth(), m_pTempRT->GetHeight());
	}
	else
	{
		// Setup depth render
		const bool bClearOnResolve = false;
		const int nCMSide = -1;
		const bool bScreenVP = true;

		gcpRendD3D->FX_ClearTarget(CTexture::s_ptexZTarget, Clr_Transparent);
		gcpRendD3D->FX_ClearTarget(CTexture::s_ptexSceneNormalsMap, Clr_Transparent);

		gcpRendD3D->FX_PushRenderTarget(0, CTexture::s_ptexZTarget, &gcpRendD3D->m_DepthBufferOrig, bClearOnResolve, nCMSide, bScreenVP);
		gcpRendD3D->FX_PushRenderTarget(1, CTexture::s_ptexSceneNormalsMap, NULL);

		gcpRendD3D->FX_SetState(GS_DEPTHWRITE);
		gcpRendD3D->RT_SetViewport(0, 0, gcpRendD3D->m_MainViewport.nWidth, gcpRendD3D->m_MainViewport.nHeight);

		// Stencil initialized to 1 - 0 is reserved for MSAAed samples
		gcpRendD3D->m_nStencilMaskRef = 1;

		gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_PersFlags |= RBPF_ZPASS;
		if (CTexture::s_eTFZ == eTF_R32F || CTexture::s_eTFZ == eTF_R16G16F || CTexture::s_eTFZ == eTF_R16G16B16A16F || CTexture::s_eTFZ == eTF_D24S8 || CTexture::s_eTFZ == eTF_D32FS8)
		{
			gcpRendD3D->m_RP.m_PersFlags2 |= (RBPF2_NOALPHABLEND | RBPF2_NOALPHATEST);
			gcpRendD3D->m_RP.m_StateAnd &= ~(GS_BLEND_MASK | GS_ALPHATEST);
		}
	}

	// Set scissor
	gcpRendD3D->EF_Scissor(true, 0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());

	// Update ambient
	gRenDev->m_cEF.m_PF[gcpRendD3D->m_RP.m_nProcessThreadID].post3DRendererAmbient = m_pAmbient->GetParamVec4();

	SRenderPipeline& RESTRICT_REFERENCE renderPipeline = gcpRendD3D->m_RP;

	if (bDoStencil)
	{
		// Setup stencil for alpha correction pass
		renderPipeline.m_ForceStateOr |= GS_STENCIL;

		const int32 nStencilState = STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_REPLACE);
		gcpRendD3D->FX_SetStencilState(nStencilState, 1, 0xFF, 0xFF);

		GetUtils().SetupStencilStates(FSS_STENCFUNC_EQUAL);

		gcpRendD3D->m_nStencilMaskRef = 1;
		GetUtils().BeginStencilPrePass(true);
	}

	// Create custom camera so FOV is the same when ever we render
	CCamera prevCamera = gcpRendD3D.GetCamera();
	CCamera postRenderCamera = prevCamera;

	Matrix34 cameraMatrix;
	cameraMatrix.SetIdentity(); // Camera is at origin
	postRenderCamera.SetMatrix(cameraMatrix);

	const float fov = DEFAULT_FOV * clamp_tpl<float>(m_pFOVScale->GetParam(), 0.05f, 1.0f);
	const float pixelAspectRatio = m_pPixelAspectRatio->GetParam();
	postRenderCamera.SetFrustum(prevCamera.GetViewSurfaceX(), prevCamera.GetViewSurfaceZ(), fov, DEFAULT_NEAR, DEFAULT_FAR, pixelAspectRatio);
	gcpRendD3D.SetCamera(postRenderCamera);

	// Set flags
	const uint32 prevAnd = renderPipeline.m_ForceStateAnd;
	const uint32 prevOr = renderPipeline.m_ForceStateOr;
	if (!bDepthOnlyRender)
	{
		renderPipeline.m_ForceStateAnd |= GS_DEPTHFUNC_EQUAL;
		renderPipeline.m_ForceStateOr |= GS_DEPTHWRITE;
	}

	renderPipeline.m_PersFlags2 |= RBPF2_POST_3D_RENDERER_PASS | RBPF2_CUSTOM_RENDER_PASS;
	if (bCustomRender)
	{
		renderPipeline.m_PersFlags2 |= RBPF2_SINGLE_FORWARD_LIGHT_PASS;
	}

	// Set fog
	I3DEngine* p3DEngine = gEnv->p3DEngine;
	const ColorF fogColor(0.0f, 0.0f, 0.0f, 0.0f);
	gcpRendD3D->SetFogColor(fogColor);

	// Draw custom objects
	{
		PROFILE_LABEL_SCOPE("FB_POST_3D_RENDER");
		ProcessRenderList(EFSLIST_GENERAL, FB_POST_3D_RENDER, groupId, screenRect, bCustomRender);
		ProcessRenderList(EFSLIST_SKIN, FB_POST_3D_RENDER, groupId, screenRect, bCustomRender);
		if (!bCustomRender)
		{
			ProcessRenderList(EFSLIST_DECAL, FB_POST_3D_RENDER, groupId, screenRect, bCustomRender);
		}
		if (!bDepthOnlyRender)
		{
			ProcessRenderList(EFSLIST_TRANSP, FB_POST_3D_RENDER, groupId, screenRect, bCustomRender);
		}
	}

	// Pop render targets
	if (!bDepthOnlyRender)
	{
		gcpRendD3D->FX_PopRenderTarget(0);
	}
	else
	{
		gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_PersFlags &= ~RBPF_ZPASS;
		if (CTexture::s_eTFZ == eTF_R16G16F || CTexture::s_eTFZ == eTF_R32F || CTexture::s_eTFZ == eTF_R16G16B16A16F || CTexture::s_eTFZ == eTF_D24S8 || CTexture::s_eTFZ == eTF_D32FS8)
		{
			gcpRendD3D->m_RP.m_PersFlags2 &= ~(RBPF2_NOALPHABLEND | RBPF2_NOALPHATEST);
			gcpRendD3D->m_RP.m_StateAnd |= (GS_BLEND_MASK | GS_ALPHATEST);
		}

		gcpRendD3D->FX_PopRenderTarget(0);
		gcpRendD3D->FX_PopRenderTarget(1);
	}

	gcpRendD3D->FX_ResetPipe();
	gcpRendD3D->RT_SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());

#ifdef SUPPORTS_MSAA
	CTexture::s_ptexZTarget->SetResolved(true);
	CTexture::s_ptexSceneNormalsMap->SetResolved(true);
#endif

	// Set everything back again
	renderPipeline.m_ForceStateAnd = prevAnd;
	renderPipeline.m_ForceStateOr = prevOr;
	renderPipeline.m_PersFlags2 &= ~(RBPF2_POST_3D_RENDERER_PASS | RBPF2_CUSTOM_RENDER_PASS);
	if (bCustomRender)
	{
		renderPipeline.m_PersFlags2 &= ~RBPF2_SINGLE_FORWARD_LIGHT_PASS;
	}
	gcpRendD3D.SetCamera(prevCamera);
	p3DEngine->SetupDistanceFog();

	if (bDoStencil)
	{
		GetUtils().EndStencilPrePass();
		renderPipeline.m_ForceStateOr &= ~GS_STENCIL;
	}
}

void CPost3DRenderer::AlphaCorrection()
{
	// Alpha correction - Override alpha using stencil, otherwise the alpha from the diffuse map will get copied
	// into render target, which then will get used when drawing the 3D objects to screen
	PROFILE_LABEL_SCOPE("ALPHA_CORRECTION");

	const ColorF clearColor(0.0f, 0.0f, 0.0f, 0.0f);
	gcpRendD3D->FX_PushRenderTarget(0, m_pTempRT, &gcpRendD3D->m_DepthBufferOrig);

	GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, m_alphaCorrectionTechName, FEF_DONTSETSTATES);

	GetUtils().SetupStencilStates(FSS_STENCFUNC_EQUAL);
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST | GS_STENCIL | GS_BLSRC_ONE | GS_BLDST_ONE_A_ZERO);

	GetUtils().DrawFullScreenTri(m_pTempRT->GetWidth(), m_pTempRT->GetHeight());

	GetUtils().ShEndPass();
	GetUtils().SetupStencilStates(-1);

	gcpRendD3D->FX_PopRenderTarget(0);
#ifdef SUPPORTS_MSAA
	m_pTempRT->SetResolved(true);
#endif
}

void CPost3DRenderer::GammaCorrection(float screenRect[4])
{
	// Gamma correction and move to correct location on RT
	PROFILE_LABEL_SCOPE("GAMMA_CORRECTION");

	const int flashRTWidth = m_pFlashRT->GetWidth();
	const int flashRTHeight = m_pFlashRT->GetHeight();

	// Clear buffer for first group
	if (m_post3DRendererflags & eP3DR_DirtyFlashRT)
	{
		m_post3DRendererflags &= ~eP3DR_DirtyFlashRT;

		gcpRendD3D->FX_ClearTarget(m_pFlashRT, Clr_Transparent);
	}

	gcpRendD3D->FX_PushRenderTarget(0, m_pFlashRT, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, flashRTWidth, flashRTHeight);

	const float rectWidth = max(screenRect[2] - screenRect[0], 0.0001f);
	const float rectHeight = max(screenRect[3] - screenRect[1], 0.0001f);

	// Super sample when we copy so double the size of the source texture uv's
	float sourceRectWidth = rectWidth * 2.0f;
	float sourceRectHeight = rectHeight * 2.0f;

	// Clamp the size if larger than 1.0
	if (sourceRectWidth >= sourceRectHeight)
	{
		if (sourceRectWidth > 1.0f)
		{
			sourceRectHeight *= (1.0f / sourceRectWidth);
			sourceRectWidth = 1.0f;
		}
	}
	else if (sourceRectHeight > 1.0f)
	{
		sourceRectWidth *= (1.0f / sourceRectHeight);
		sourceRectHeight = 1.0f;
	}

	float halfSourceRectWidth = sourceRectWidth * 0.5f;
	float halfSourceRectHeight = sourceRectHeight * 0.5f;

	const float sourceLeft = max(0.5f - halfSourceRectWidth, 0.0f);
	const float sourceRight = min(0.5f + halfSourceRectWidth, 1.0f);
	const float sourceTop = max(0.5f - halfSourceRectHeight, 0.0f);
	const float sourceBottom = min(0.5f + halfSourceRectHeight, 1.0f);
	Vec2 pos[4];
	Vec2 uv[4];

	pos[0] = Vec2(screenRect[0], screenRect[1]);
	pos[1] = Vec2(screenRect[0], screenRect[3]);
	pos[2] = Vec2(screenRect[2], screenRect[3]);
	pos[3] = Vec2(screenRect[2], screenRect[1]);

	uv[0] = Vec2(sourceLeft, sourceTop);
	uv[1] = Vec2(sourceLeft, sourceBottom);
	uv[2] = Vec2(sourceRight, sourceBottom);
	uv[3] = Vec2(sourceRight, sourceTop);

	GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, m_gammaCorrectionTechName, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);
	GetUtils().SetTexture(m_pTempRT, 0, FILTER_LINEAR);

	// VS params
	Vec4 vsParam;
	vsParam.x = sourceLeft;              // Top
	vsParam.y = sourceTop;               // Left
	vsParam.z = 1.0f / sourceRectWidth;  // inv scale x
	vsParam.w = 1.0f / sourceRectHeight; // inv scale y
	CShaderMan::s_shPostEffects->FXSetVSFloat(m_vsParamName, &vsParam, 1);

	// PS params
	const Vec4 psParams(m_alpha, m_edgeFadeScale, 0.0f, 0.0f);
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(m_psParamName, &psParams, 1);

	const int blendState = GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE;
	gcpRendD3D->FX_SetState(blendState);

	GetUtils().DrawQuad(flashRTWidth, flashRTHeight,
	                    pos[0], pos[1], pos[2], pos[3],
	                    uv[0], uv[1], uv[2], uv[3]);

	GetUtils().ShEndPass();

	gcpRendD3D->FX_PopRenderTarget(0);
}

void CPost3DRenderer::ProcessRenderList(int list, uint32 batchFilter, uint8 groupId, float screenRect[4], bool bCustomRender)
{
	const int stage = 3;
	gcpRendD3D->FX_PreRender(stage);

	SRenderPipeline& renderPipeline = gcpRendD3D->m_RP;
	renderPipeline.m_nPassGroupID = list;
	renderPipeline.m_nPassGroupDIP = list;

	size_t numItems = renderPipeline.m_pCurrentRenderView->GetRenderItems(list).size();

	ProcessBatchesList(0, numItems, batchFilter, groupId, screenRect, bCustomRender);

	gcpRendD3D->FX_PostRender();
}

void CPost3DRenderer::ProcessBatchesList(int listStart, int listEnd, uint32 batchFilter, uint8 groupId, float screenRect[4], bool bCustomRender)
{
	// Basis for this function taken from FX_ProcessBatchesList

	if ((listEnd - listStart) > 0)
	{
		gcpRendD3D->FX_StartBatching();

		SRenderPipeline& renderPipeline = gcpRendD3D->m_RP;
		const int threadID = renderPipeline.m_nProcessThreadID;
		const int passGroupID = renderPipeline.m_nPassGroupID;

		auto& renderItems = renderPipeline.m_pCurrentRenderView->GetRenderItems(passGroupID);

		renderPipeline.m_nBatchFilter = batchFilter;

		CShader* pShader = NULL;
		CShaderResources* pCurShaderResources = NULL;
		CRenderObject* pCurObject = NULL;
		CShader* pCurShader = NULL;
		int tech = 0;

		for (int i = listStart; i < listEnd; i++)
		{
			SRendItem& renderItem = renderItems[i];
			if (!(renderItem.nBatchFlags & batchFilter))
				continue;

			assert(renderItem.pObj);
			PREFAST_ASSUME(renderItem.pObj);
			CRenderObject* pRenderObject = renderItem.pObj;

			// Apply group filter and update group count
			SRenderObjData* pRenderObjData = pRenderObject->GetObjData();
			uint8 currentObjGroupId = pRenderObjData->m_nCustomData;
			m_groupCount = max((uint8)(currentObjGroupId + 1), m_groupCount); // update group count
			if (currentObjGroupId != groupId)
				continue;

			// Detect if shader has changed
			CRenderElement* pRenderElement = renderItem.pElem;
			bool bChangedShader = false;
			CShaderResources* pShaderResources = NULL;
			SRendItem::mfGet(renderItem.SortVal, tech, pShader, pShaderResources);

			// Set custom shader
			if (bCustomRender && pShader)
			{
				if (!pShader->FXSetTechnique(m_customRenderTechName))
				{
					continue;
				}
				tech = renderPipeline.m_nShaderTechnique;
			}

			if ((pShader != pCurShader) ||
			    (!pShaderResources) ||
			    (!pCurShaderResources) ||
			    (pShaderResources->m_IdGroup != pCurShaderResources->m_IdGroup) ||
			    (pRenderObject->m_ObjFlags & (FOB_SKINNED | FOB_DECAL)))
			{
				bChangedShader = true;
			}
			pCurShaderResources = pShaderResources;

			if (pRenderObject != pCurObject)
			{
				if (pCurShader)
				{
					renderPipeline.m_pRenderFunc();
					pCurShader = NULL;
					bChangedShader = true;
				}
				if (!gcpRendD3D->FX_ObjectChange(pShader, pCurShaderResources, pRenderObject, pRenderElement))
				{
					continue;
				}
				pCurObject = pRenderObject;
			}

			if (bChangedShader)
			{
				if (pCurShader)
				{
					renderPipeline.m_pRenderFunc();
				}
				pCurShader = pShader;
				gcpRendD3D->FX_Start(pShader, tech, pCurShaderResources, pRenderElement);
			}

			pRenderElement->mfPrepare(true);

			if (renderPipeline.m_RIs[0].size() == 0)
			{
				// Add item to batch
				renderPipeline.m_RIs[0].AddElem(&renderItem);

				// Update screen rect
				memcpy(screenRect, &pRenderObjData->m_fTempVars[5], sizeof(float) * 4);
				if (pRenderObjData->m_nHUDSilhouetteParams)
				{
					m_post3DRendererflags |= eP3DR_HasSilhouettes;
				}

				// Set current alpha to be the minimum object within group
				const float objAlpha = pRenderObjData->m_fTempVars[9];
				m_alpha = min(objAlpha, m_alpha);
			}
		}
		if (pCurShader)
		{
			renderPipeline.m_pRenderFunc();
		}
	}
}

void CPost3DRenderer::RenderSilhouettes(uint8 groupId, float screenRect[4])
{
	PROFILE_LABEL_SCOPE("SILHOUTTES");

	RenderMeshes(groupId, screenRect, eRMM_Custom);

	CTexture* pOutlineTex = CTexture::s_ptexBackBufferScaled[0];
	CTexture* pGlowTex = CTexture::s_ptexBackBufferScaled[1];

	ApplyShaderQuality();
	SilhouetteOutlines(pOutlineTex, pGlowTex);
	SilhouetteGlow(pOutlineTex, pGlowTex);
	SilhouetteCombineBlurAndOutline(pOutlineTex, pGlowTex);
	GammaCorrection(screenRect);
}

void CPost3DRenderer::SilhouetteCombineBlurAndOutline(CTexture* pOutlineTex, CTexture* pGlowTex)
{
	PROFILE_LABEL_SCOPE("COMBINE_BLUR_AND_OUTLINE");

	// Combine blur and outline
	gcpRendD3D->FX_ClearTarget(m_pTempRT, Clr_Transparent); // TODO: necessary?
	gcpRendD3D->FX_PushRenderTarget(0, m_pTempRT, NULL);

	GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, m_combineSilhouettesTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gRenDev->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);

	GetUtils().SetTexture(pOutlineTex, 0, FILTER_LINEAR);
	GetUtils().SetTexture(pGlowTex, 1, FILTER_LINEAR);

	// Set PS default params
	const float silhouetteStrength = max(m_pSilhouetteStrength->GetParam(), 0.0f);
	const float fillStrength = 0.02f;
	const float glowStrength = 0.5f;

	const Vec4 psParams(silhouetteStrength, fillStrength, glowStrength, 0.0f);
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(m_psParamName, &psParams, 1);

	GetUtils().DrawFullScreenTri(m_pTempRT->GetWidth(), m_pTempRT->GetHeight());
	GetUtils().ShEndPass();

	gcpRendD3D->FX_PopRenderTarget(0);
}

void CPost3DRenderer::SilhouetteGlow(CTexture* pOutlineTex, CTexture* pGlowTex)
{
	PROFILE_LABEL_SCOPE("GLOW");

	GetUtils().StretchRect(pOutlineTex, pGlowTex);
	GetUtils().TexBlurIterative(pGlowTex, 1, false);
	gcpRendD3D->RT_SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());
}

void CPost3DRenderer::SilhouetteOutlines(CTexture* pOutlineTex, CTexture* pGlowTex)
{
	PROFILE_LABEL_SCOPE("OUTLINES");

	// Set improved (expensive) edge detection flag
	CD3D9Renderer* const __restrict pRD = gcpRendD3D;
	const uint64 prevRTFlags = pRD->m_RP.m_FlagsShader_RT;
	pRD->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	gcpRendD3D->FX_PushRenderTarget(0, pOutlineTex, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, pOutlineTex->GetWidth(), pOutlineTex->GetHeight());

	GetUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, m_silhouetteTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gRenDev->FX_SetState(GS_NODEPTHTEST);

	// Set shader params
	const float edgeScale = 1.0f;
	const float fillSrength = 0.1f;

	const Vec4 vsParams(edgeScale, 0.0f, 0.0f, 0.0f);
	const Vec4 psParams(0.0f, 0.0f, 0.0f, fillSrength);
	CShaderMan::s_shPostEffectsGame->FXSetVSFloat(m_vsParamName, &vsParams, 1);
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(m_psParamName, &psParams, 1);

	GetUtils().SetTexture(m_pTempRT, 0, FILTER_LINEAR);
	GetUtils().SetTexture(CTexture::s_ptexZTarget, 1, FILTER_POINT);
	GetUtils().DrawFullScreenTri(CTexture::s_ptexSceneTarget->GetWidth(), CTexture::s_ptexSceneTarget->GetHeight());

	GetUtils().ShEndPass();
	gcpRendD3D->FX_PopRenderTarget(0);

	// Revert shader flags
	pRD->m_RP.m_FlagsShader_RT = prevRTFlags;
}

void CPost3DRenderer::ApplyShaderQuality(EShaderType shaderType)
{
	CD3D9Renderer* const __restrict pRD = gcpRendD3D;

	// Retrieve quality for shader type
	SShaderProfile* pSP = &pRD->m_cEF.m_ShaderProfiles[shaderType];
	int nQuality = (int)pSP->GetShaderQuality();

	// Apply correct flag set
	pRD->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1]);
	pRD->m_RP.m_nShaderQuality = nQuality;

	switch (nQuality)
	{
	case eSQ_Medium:
		pRD->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY];
		break;
	case eSQ_High:
		pRD->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY1];
		break;
	case eSQ_VeryHigh:
		pRD->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY];
		pRD->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_QUALITY1];
		break;
	}
}

#pragma warning(pop)
