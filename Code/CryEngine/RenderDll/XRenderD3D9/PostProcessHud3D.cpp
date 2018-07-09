// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessHud3D : 3D hud post processing

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"
#include "D3DStereo.h"
#include <CrySystem/Scaleform/IFlashPlayer.h>

#pragma warning(push)
#pragma warning(disable: 4244)

enum EHud3dDebugView
{
	eHUD3D_DEBUG_VIEW_Off = 0,
	eHUD3D_DEBUG_VIEW_SolidFill,
	eHUD3D_DEBUG_VIEW_WireFrame,
	eHUD3D_DEBUG_VIEW_UnwrapOnToFlashTex
};

int16 SHudData::s_nFlashWidthMax  = 16;
int16 SHudData::s_nFlashHeightMax = 16;

void SHudData::Init()
{
	nSortVal = nFlashWidth = nFlashHeight = 0;
	pFlashPlayer = NULL;

	pDiffuse = pShaderResources->GetTexture(EFTT_DIFFUSE);
	assert(pDiffuse);

	IDynTextureSource* pDynTexSrc = pDiffuse ? pDiffuse->m_Sampler.m_pDynTexSource : NULL;
	if (pDynTexSrc)
	{
		pFlashPlayer = (IFlashPlayer*) pDynTexSrc->GetSourceTemp(IDynTextureSource::DTS_I_FLASHPLAYER);
		if (pFlashPlayer)
		{
			nFlashWidth = pFlashPlayer->GetWidth();
			nFlashHeight = pFlashPlayer->GetHeight();

			s_nFlashWidthMax = max(nFlashWidth, s_nFlashWidthMax);
			s_nFlashHeightMax = max(nFlashHeight, s_nFlashHeightMax);
		}
	}

	if (CRenderer::CV_r_PostProcessHUD3D != 2)
		nSortVal = nFlashWidth * nFlashHeight; // we sort by size
	else
	{
		int nResID = pShaderResources->m_Id;
		CShader* pShader = (CShader*)pShaderItem->m_pShader;
		nSortVal = (nResID << 18) | (pShader->mfGetID() << 6) | (pShaderItem->m_nTechnique & 0x3f);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CHud3D::CreateResources()
{
	SAFE_RELEASE(m_pNoise);

	m_pNoise = CTexture::ForName("%ENGINE%/EngineAssets/Textures/vector_noise.dds", FT_DONT_STREAM, eTF_Unknown);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::Release()
{
	SAFE_RELEASE(m_pNoise);

	for (uint32 i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		stl::free_container(m_pRenderData[i]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CHud3D::Preprocess(const SRenderViewInfo& viewInfo)
{
	// Warning: This function could get called twice per frame (NanoGlass will call it if its active)
	//					So don't do any updating here, do it in Update()

	const bool bForceRefresh = (m_pOverideCacheDelay->GetParam() > 0.5f);
	if (bForceRefresh)
	{
		return true;
	}

	const uint32 nThreadID = gRenDev->GetRenderThreadID();
	if (m_pRenderData[nThreadID].empty())
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::Update()
{
	const uint32 nThreadID = gRenDev->GetRenderThreadID();
	SHudDataVec& rRenderData = m_pRenderData[nThreadID];
	rRenderData.CoalesceMemory();
	size_t nSize = rRenderData.size();
	if (nSize != 0)
	{
		for (size_t i = 0; i < m_pRenderData[nThreadID].size(); ++i)
			m_pRenderData[nThreadID][i].Init();

		SHudData* pBegin = &rRenderData[0];
		SHudData* pEnd = pBegin + nSize;
		std::sort(pBegin, pEnd, HudDataSortCmp());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::OnBeginFrame(const SRenderingPassInfo& passInfo)
{
	const uint32 nThreadID = gRenDev->GetMainThreadID();
	if (passInfo.IsGeneralPass())
		m_pRenderData[nThreadID].resize(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::Reset(bool bOnSpecChange)
{
	// Need to reset the SHudData max width and height here, because mp has larger flash size
	// and needs to be reset when going from mp to sp
	SHudData::s_nFlashWidthMax = 16;
	SHudData::s_nFlashHeightMax = 16;

	for (uint32 i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		m_pRenderData[i].resize(0);
	m_pOpacityMul->ResetParam(1.0f);
	m_pHudColor->ResetParamVec4(Vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_pGlowMul->ResetParam(1.0f);
	m_pGlowColorMul->ResetParamVec4(Vec4(1.0f, 1.0f, 1.0f, 1.0f));
	m_pChromaShift->ResetParam(0.0f);
	m_pDofMultiplier->ResetParam(1.0f);
	m_maxParallax = 0.0f;
	m_interferenceRandTimer = 0.0f;
	m_pOverideCacheDelay->ResetParam(0);

	m_pInterference->ResetParam(0.0f);

	m_pInterferenceMinValue->ResetParam(0.0f);

	// x= hudItemOverrideStrength, y= interferenceRandFrequency, z= dofInterferenceStrength, w = free
	m_pInterferenceParams[0]->ResetParamVec4(Vec4(1.0f, 0.055f, 0.8f, 0.0f));

	// x = randomGrainStrengthScale, y= randomFadeStrengthScale, z= chromaShiftStrength, w= chromaShiftDist
	m_pInterferenceParams[1]->ResetParamVec4(Vec4(0.3f, 0.4f, 0.25f, 1.0f));

	// x= disruptScale, y= disruptMovementScale, z= noiseStrength, w = barScale
	m_pInterferenceParams[2]->ResetParamVec4(Vec4(0.3f, 1.0f, 0.2f, 0.8f));

	// xyz= barColor, w= barColorMultiplier
	m_pInterferenceParams[3]->ResetParamVec4(Vec4(0.39f, 0.6f, 1.0f, 5.0f));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::CalculateProjMatrix()
{
	const float fHUDFov = clamp_tpl<float>(m_pFOV->GetParam(), 1.0f, 180.0f);

	const auto& viewInfo = gcpRendD3D.GetGraphicsPipeline().GetCurrentViewInfo(CCamera::eEye_Left);
	// Patch projection matrix to have HUD FOV
	m_mProj = viewInfo.unjitteredProjMatrix;

	// Calc aspect ratio
	const float cameraPixelAspectRatio = viewInfo.pCamera->GetPixelAspectRatio();
	const float overscanBorderAspectRatio = GetUtils().GetOverscanBorderAspectRatio();
	const float aspectRatio = overscanBorderAspectRatio / max(cameraPixelAspectRatio, 0.0001f);

	ICVar* r_3MonHack = iConsole->GetCVar("r_3MonHack");

	if (r_3MonHack->GetIVal() == 0)
	{
		float h = 1 / tanf(DEG2RAD(fHUDFov / 2));
		m_mProj.m00 = h / aspectRatio;
		m_mProj.m11 = h;
	}
	else // 3 monitor center hack
	{
		ICVar* r_3MonHackHUDFOVX = iConsole->GetCVar("r_3MonHackHUDFOVX");
		ICVar* r_3MonHackHUDFOVY = iConsole->GetCVar("r_3MonHackHUDFOVY");

		float hX = (1 / tanf(DEG2RAD(r_3MonHackHUDFOVX->GetFVal() / 2))) / 3;
		float hY = (1 / tanf(DEG2RAD(r_3MonHackHUDFOVY->GetFVal() / 2)));
		m_mProj.m00 = hX / aspectRatio;
		m_mProj.m11 = hY;
	}

	// Patch projection matrix to do x-shift by (maxParallax * (1 - hudScreenDist/w))
	m_mProj.m20 = -m_maxParallax;
	m_mProj.m30 = -m_maxParallax * CRenderer::CV_r_StereoHudScreenDist;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::SetShaderParams(SHudData& pData)
{
	ASSERT_LEGACY_PIPELINE
/*
	CShaderResources* pShaderResources = (CShaderResources*)pData.pShaderResources;
	float fOpacity = pShaderResources->GetStrengthValue(EFTT_OPACITY) * m_pOpacityMul->GetParam();

	const CRenderObject* pRO = pData.pRO;
	Matrix44A mObjCurr, mViewProj;
	mObjCurr = pRO->m_II.m_Matrix.GetTransposed();

	// Render in camera space to remove precision bugs
	const bool bCameraSpace = (pRO->m_ObjFlags & FOB_NEAREST) ? true : false;
	Matrix44A mView;
	GetUtils().GetViewMatrix(mView, bCameraSpace);

	mViewProj = mView * m_mProj;
	mViewProj = mObjCurr * mViewProj;
	mViewProj.Transpose();

	CShaderMan::s_sh3DHUD->FXSetVSFloat(m_pMatViewProjParamName, (Vec4*) mViewProj.GetData(), 4);

	// Since we can have multiple flash files with different resolutions we need to update resolution for correct mapping
	Vec4 vHudTexCoordParams = Vec4(min(1.0f, (float) pData.nFlashWidth / (float) m_pHUD_RT->GetWidth()),
	                               min(1.0f, (float) pData.nFlashHeight / (float) m_pHUD_RT->GetHeight()),
	                               max(1.0f, (float) m_pHUD_RT->GetWidth() / (float) pData.s_nFlashWidthMax),
	                               max(1.0f, (float) m_pHUD_RT->GetHeight() / (float) pData.s_nFlashHeightMax));

	CShaderMan::s_sh3DHUD->FXSetVSFloat(m_pHudTexCoordParamName, &vHudTexCoordParams, 1);
	CShaderMan::s_sh3DHUD->FXSetPSFloat(m_pHudTexCoordParamName, &vHudTexCoordParams, 1);

	ColorF cDiffuse = pShaderResources->GetColorValue(EFTT_DIFFUSE);

	cDiffuse *= fOpacity; // pre-multiply alpha in all cases
	if (pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE)
	{
		fOpacity = 0.0f;
	}

	Vec4 vHudParams = Vec4(cDiffuse.r, cDiffuse.g, cDiffuse.b, fOpacity) * m_pHudColor->GetParamVec4();
	CShaderMan::s_sh3DHUD->FXSetPSFloat(m_pHudParamName, &vHudParams, 1);
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::SetTextures(SHudData& pData)
{
	// OLD PIPELINE
	/*
	SEfResTexture* pDiffuse = pData.pDiffuse;
	if (pDiffuse && pDiffuse->m_Sampler.m_pTex)
		GetUtils().SetTexture(pDiffuse->m_Sampler.m_pTex, 0, FILTER_LINEAR);
	else
		GetUtils().SetTexture(m_pHUD_RT, 0, FILTER_LINEAR);
		*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::RenderMesh(const CRenderElement* pRE, SShaderPass* pPass)
{
	// OLD PIPELINE
	/*
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	CREMesh* pRenderMesh = (CREMeshImpl*) pRE;

	// Create/Update vertex buffer stream
	if (pRenderMesh->m_pRenderMesh)
	{
		pRenderMesh->m_pRenderMesh->CheckUpdate(pRenderMesh->m_pRenderMesh->_GetVertexFormat(), 0);

		// set the mesh's vertex format, which hasn't been set elsewhere.
		gRenDev->m_RP.m_CurVFormat = pRenderMesh->m_pRenderMesh->_GetVertexFormat();
	}

	rd->m_RP.m_pRE = const_cast<CRenderElement*>(pRE);
	if (rd->FX_CommitStreams(pPass, true))
	{
		gRenDev->m_RP.m_FirstVertex = pRenderMesh->m_nFirstVertId;
		gRenDev->m_RP.m_FirstIndex = pRenderMesh->m_nFirstIndexId;
		gRenDev->m_RP.m_RendNumIndices = pRenderMesh->m_nNumIndices;
		gRenDev->m_RP.m_RendNumVerts = pRenderMesh->m_nNumVerts;
		gRenDev->m_RP.m_pRE->mfDraw(CShaderMan::s_sh3DHUD, pPass);
	}
	*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::DownsampleHud4x4(CTexture* pDstRT)
{
	ASSERT_LEGACY_PIPELINE
/*
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const uint32 nThreadID = gRenDev->GetRenderThreadID();

	SEfResTexture* pDiffuse = NULL;
	SShaderTechnique* pShaderTech = CShaderMan::s_sh3DHUD->mfFindTechnique(m_pDownsampleTechName);
	IF (!pShaderTech, 0)
		return;

	PROFILE_LABEL_SCOPE("3D HUD DOWNSAMPLE 4X4");

	// temporary clear/fix - try avoiding this
	rd->FX_ClearTarget(pDstRT, Clr_Transparent);
	rd->FX_PushRenderTarget(0, pDstRT, NULL);
	rd->RT_SetViewport(0, 0, pDstRT->GetWidth(), pDstRT->GetHeight());

	// Render in uv space - use unique geometry for discarding uncovered pixels for faster blur/downsampling
	uint32 nRECount = m_pRenderData[nThreadID].size();
	for (uint32 r = 0; r < nRECount; ++r)
	{
		SHudData& pData = m_pRenderData[nThreadID][r];
		CShaderResources* pShaderResources = (CShaderResources*)pData.pShaderResources;

		pDiffuse = pData.pDiffuse;
		if (!pShaderResources || !pDiffuse || !pDiffuse->m_Sampler.m_pDynTexSource)
			continue;

		GetUtils().ShBeginPass(CShaderMan::s_sh3DHUD, m_pDownsampleTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		// Rescale uv's to match viewport
		Vec4 vTCScale = Vec4(1, 1, (float) SHudData::s_nFlashWidthMax / (float) m_pHUD_RT->GetWidth(), (float) SHudData::s_nFlashHeightMax / (float) m_pHUD_RT->GetHeight());

		rd->SetCullMode(R_CULL_BACK);
		rd->FX_SetState(GS_NODEPTHTEST);

		SetShaderParams(pData);
		SetTextures(pData);
		RenderMesh(pData.pRE, &pShaderTech->m_Passes[0]);

		GetUtils().ShEndPass();
	}

	rd->FX_PopRenderTarget(0);
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::UpdateBloomRT(CTexture* pDstRT, CTexture* pBlurDst)
{
	ASSERT_LEGACY_PIPELINE
/*
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const uint32 nThreadID = gRenDev->GetRenderThreadID();

	SEfResTexture* pDiffuse = NULL;
	SShaderTechnique* pShaderTech = CShaderMan::s_sh3DHUD->mfFindTechnique(m_pUpdateBloomTechName);
	IF (!pShaderTech, 0)
		return;

	PROFILE_LABEL_SCOPE("UPDATE BLOOM RT");

	// Calculate HUD's projection matrix using fixed FOV.
	CalculateProjMatrix();

	// temporary clear/fix - try avoiding this
	rd->FX_ClearTarget(pDstRT, Clr_Transparent);
	rd->FX_PushRenderTarget(0, pDstRT, NULL);
	rd->RT_SetViewport(0, 0, pDstRT->GetWidth(), pDstRT->GetHeight());

	// Render in screen space - use unique geometry for discarding uncovered pixels for faster transfer
	uint32 nRECount = m_pRenderData[nThreadID].size();
	for (uint32 r = 0; r < nRECount; ++r)
	{
		SHudData& pData = m_pRenderData[nThreadID][r];
		CShaderResources* pShaderResources = (CShaderResources*)pData.pShaderResources;

		pDiffuse = pData.pDiffuse;
		if (!pShaderResources || !pDiffuse || !pDiffuse->m_Sampler.m_pDynTexSource)
			continue;

		SRenderObjData* pROData = pData.pRO->GetObjData();
		if (pROData)
		{
			if (pROData->m_nCustomFlags & COB_HUD_DISABLEBLOOM)
			{
				continue; // Skip the low alpha planes in bloom phase (Fixes overglow and cut off with alien hud ghosted planes)
			}
		}

		GetUtils().ShBeginPass(CShaderMan::s_sh3DHUD, m_pUpdateBloomTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		rd->SetCullMode(R_CULL_BACK);
		rd->FX_SetState(GS_NODEPTHTEST);

		SetShaderParams(pData);
		//SetTextures( pData );
		GetUtils().SetTexture(m_pHUDScaled_RT, 0, FILTER_LINEAR);
		RenderMesh(pData.pRE, &pShaderTech->m_Passes[0]);

		GetUtils().ShEndPass();
	}

	rd->FX_PopRenderTarget(0);

	if (pBlurDst)
	{
		GetUtils().TexBlurGaussian(pDstRT, 1, 1, 0.85f, false, 0, false, pBlurDst);
	}

	rd->RT_SetViewport(0, 0, rd->GetWidth(), rd->GetHeight());
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Reminder: for efficient multiple flash files to work correctly - uv's must not override
void CHud3D::FlashUpdateRT(void)
{
	ASSERT_LEGACY_PIPELINE
/*
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
#if defined(USE_VBIB_PUSH_DOWN)
	//workaround for deadlock when streaming thread wants renderthread to clean mesh pool
	if (gRenDev->m_bStartLevelLoading) CRenderMesh::Tick();
#endif
	const uint32 nThreadID = gRenDev->GetRenderThreadID();
	uint32 nRECount = m_pRenderData[nThreadID].size();

	const bool bForceRefresh = (m_pOverideCacheDelay->GetParam() > 0.5f);

	if (nRECount || bForceRefresh) //&& m_nFlashUpdateFrameID != rd->GetFrameID(false) )
	{
		// Share hud render target with scene normals
		m_pHUD_RT = CRendererResources::s_ptexCached3DHud;
		m_pHUDScaled_RT = CRendererResources::s_ptexCached3DHudScaled;

		if ((gRenDev->GetFrameID(false) % max(1, (int)CRenderer::CV_r_PostProcessHUD3DCache)) != 0)
		{
			if (!bForceRefresh)
			{
				// CV_r_PostProcessHUD3DCache>0 will skip flash asset advancing. Accumulate frame time of
				//skipped frames and add it when we're actually advancing.
				m_accumulatedFrameTime += gEnv->pTimer->GetFrameTime();

				ReleaseFlashPlayerRef(nThreadID);
				return;
			}
			else
			{
				m_pOverideCacheDelay->SetParam(0.0f);
				CryLog("reseting param");
			}
		}

		m_nFlashUpdateFrameID = rd->GetFrameID(false);

		PROFILE_LABEL_SCOPE("3D HUD FLASHPLAYER UPDATES");

		SEfResTexture* pDiffuse = NULL;
		SEfResTexture* pPrevDiffuse = NULL;

		int32 nViewportWidth = min((int32)SHudData::s_nFlashWidthMax, m_pHUD_RT->GetWidth());
		int32 nViewportHeight = min((int32)SHudData::s_nFlashHeightMax, m_pHUD_RT->GetHeight());
		RECT rect = { 0, 0, nViewportWidth, nViewportHeight };

		// Enable rendering outside backbuffer edram range
		rd->FX_ClearTarget(m_pHUD_RT, Clr_Transparent, 1, &rect, true); // temporary clear/fix - try avoiding this
		rd->FX_PushRenderTarget(0, m_pHUD_RT, &rd->m_DepthBufferOrig);
		rd->RT_SetViewport(0, 0, nViewportWidth, nViewportHeight);

		float accumulatedDeltaTime = gEnv->pTimer->GetFrameTime() + m_accumulatedFrameTime;

		for (uint32 r = 0; r < nRECount; ++r)
		{
			SHudData& pData = m_pRenderData[nThreadID][r];

			pDiffuse = pData.pDiffuse;
			if (!pDiffuse || !pDiffuse->m_Sampler.m_pDynTexSource)
				continue;

			if (pData.pFlashPlayer && pPrevDiffuse != pDiffuse)
			{
				PROFILE_LABEL_SCOPE("3D HUD FLASHFILE");

				pData.pFlashPlayer->Advance(accumulatedDeltaTime);

				pData.pFlashPlayer->SetViewport(0, 0, min(nViewportWidth, pData.pFlashPlayer->GetWidth()), min(nViewportHeight, pData.pFlashPlayer->GetHeight()));
				pData.pFlashPlayer->AvoidStencilClear(!rd->CV_r_PostProcessHUD3DStencilClear);

				// Lockless rendering playback.
				rd->FX_Commit();

				pData.pFlashPlayer->Render(false);

				rd->FX_SetState(gRenDev->m_RP.m_CurState & ~GS_BLEND_MASK);

				pData.pFlashPlayer->AvoidStencilClear(false);

				pPrevDiffuse = pDiffuse;
				break;
			}
		}

		m_accumulatedFrameTime = 0.0f;

		ReleaseFlashPlayerRef(nThreadID);

		rd->FX_PopRenderTarget(0);

		// Downsample/blur hud into half res target _1 time only_ - we'll use this for Bloom/Dof
		DownsampleHud4x4(m_pHUDScaled_RT);
	}
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHud3D::ReleaseFlashPlayerRef(const uint32 nThreadID)
{
	uint32 nRECount = m_pRenderData[nThreadID].size();
	for (uint32 r = 0; r < nRECount; ++r)
	{
		SHudData& pData = m_pRenderData[nThreadID][r];
		pData.pFlashPlayer = nullptr;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::RenderFinalPass()
{
	ASSERT_LEGACY_PIPELINE
/*
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	PROFILE_LABEL_SCOPE("3D HUD FINAL PASS");

	const uint32 nThreadID = gRenDev->GetRenderThreadID();
	SShaderTechnique* pShaderTech = CShaderMan::s_sh3DHUD->mfFindTechnique(m_pGeneralTechName);
	IF (!pShaderTech, 0)
		return;

	UpdateBloomRT(CRendererResources::s_ptexBackBufferScaled[1], CRendererResources::s_ptexBackBufferScaledTemp[1]);

	SEfResTexture* pDiffuse = NULL;
	SEfResTexture* pPrevDiffuse = NULL;

	// Calculate HUD's projection matrix using fixed FOV.
	CalculateProjMatrix();

	// Hud simple 2D dof blend
	CPostEffect* pDofPostEffect = PostEffectMgr()->GetEffect(EPostEffectID::DepthOfField);
	bool bGameDof = pDofPostEffect->IsActive();

	static float fDofBlend = 0.0f;
	fDofBlend += ((bGameDof ? .6f : 0.0f) - fDofBlend) * gEnv->pTimer->GetFrameTime() * 10.0f;
	float dofMultiplier = clamp_tpl<float>(m_pDofMultiplier->GetParam(), 0.0f, 2.0f);
	float fCurrentDofBlend = fDofBlend * dofMultiplier;

	bool bInterferenceApplied = false;
	uint32 hudEffectParamCount = 1; // Default to only pass 1 vecs

	// [0] = default params
	// [1]-[4] = interference params
	Vec4 vHudEffectParams[5];
	Vec4 vInterferenceParams;
	float interferenceStrength = clamp_tpl<float>(m_pInterference->GetParam(), m_pInterferenceMinValue->GetParam(), 1.0f);
	Vec4 vOverrideColorParams = m_pGlowColorMul->GetParamVec4(); // Apply override color multipliers (Used to change the glow flash color for Alien HUD), alpha multiplier (.w) is set per plane

	if (interferenceStrength > 0.0f)
	{
		bInterferenceApplied = true;
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1]; // Set interference combination
		hudEffectParamCount += 4;                                          // Pass interference params as well

		// x= hudItemOverrideStrength, y= interferenceRandFrequency, z= dofInterferenceStrength, w = free
		vInterferenceParams = m_pInterferenceParams[0]->GetParamVec4();
		Limit(vInterferenceParams.x, 0.0f, 1.0f);
		Limit(vInterferenceParams.y, 0.0f, 1.0f);
		Limit(vInterferenceParams.z, 0.0f, 1.0f);

		float interferenceRandFrequency = max(vInterferenceParams.y, 0.0f);
		if (m_interferenceRandTimer >= interferenceRandFrequency)
		{
			m_interferenceRandNums.x = cry_random(-1.0f, 1.0f);
			m_interferenceRandNums.y = cry_random(-1.0f, 1.0f);
			m_interferenceRandNums.z = cry_random(0.0f, 1.0f);
			m_interferenceRandNums.w = cry_random(-1.0f, 1.0f);
			m_interferenceRandTimer = 0.0f;
		}

		// x = randomGrainStrengthScale, y= randomFadeStrengthScale, z= chromaShiftStrength, w= chromaShiftDist
		const float chromaShiftMaxStrength = 2.0f;
		const float chromaShiftMaxDist = 0.007f;
		vHudEffectParams[2] = m_pInterferenceParams[1]->GetParamVec4();
		Limit(vHudEffectParams[2].x, 0.0f, 1.0f);
		Limit(vHudEffectParams[2].y, 0.0f, 1.0f);
		vHudEffectParams[2].x = ((1.0f - vHudEffectParams[2].x) + (m_interferenceRandNums.z * vHudEffectParams[2].x));
		vHudEffectParams[2].y = ((1.0f - vHudEffectParams[2].y) + (m_interferenceRandNums.z * vHudEffectParams[2].y));
		vHudEffectParams[2].z = clamp_tpl<float>(vHudEffectParams[2].z, 0.0f, 1.0f) * chromaShiftMaxStrength;
		vHudEffectParams[2].w = clamp_tpl<float>(vHudEffectParams[2].w, 0.0f, 1.0f) * chromaShiftMaxDist;

		// x= disruptScale, y= disruptMovementScale, z= noiseStrength, w = barScale
		const float disruptMaxScale = 100.0f;
		const float disruptMovementMaxScale = 0.012f;
		const float noiseMaxScale = 10.0f;
		const float barMaxScale = 10.0f;
		vHudEffectParams[3] = m_pInterferenceParams[2]->GetParamVec4();
		vHudEffectParams[3].x = clamp_tpl<float>(vHudEffectParams[3].x, 0.0f, 1.0f) * disruptMaxScale;
		vHudEffectParams[3].y = clamp_tpl<float>(vHudEffectParams[3].y, 0.0f, 1.0f) * disruptMovementMaxScale;
		vHudEffectParams[3].z = clamp_tpl<float>(vHudEffectParams[3].z, 0.0f, 1.0f) * noiseMaxScale;
		Limit(vHudEffectParams[3].w, 0.0f, 1.0f);
		vHudEffectParams[3].x *= m_interferenceRandNums.z * m_interferenceRandNums.z;
		vHudEffectParams[3].w = (1.0f - vHudEffectParams[3].w) * m_interferenceRandNums.z * barMaxScale;

		// xyz= barColor, w= bar color scale
		vHudEffectParams[4] = m_pInterferenceParams[3]->GetParamVec4();
		Limit(vHudEffectParams[4].x, 0.0f, 1.0f);
		Limit(vHudEffectParams[4].y, 0.0f, 1.0f);
		Limit(vHudEffectParams[4].z, 0.0f, 1.0f);
		Limit(vHudEffectParams[4].w, 0.0f, 20.0f);
		vHudEffectParams[4] *= vHudEffectParams[4].w;  // Scale color values by color scale
		vHudEffectParams[4].w = vInterferenceParams.z; // dofInterferenceStrength
	}

#if !defined(_RELEASE) && !defined(CONSOLE_CONST_CVAR_MODE)
	// Turn on debug view in shader
	if (m_bDebugView)
	{
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG1];
	}
	bool bDrawingSolidView = (CRenderer::CV_r_PostProcessHUD3DDebugView == eHUD3D_DEBUG_VIEW_SolidFill);
	if (m_bWireframeEnabled || bDrawingSolidView)
	{
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG2];
	}
	// Rand num generator used to color each hud item
	CMTRand_int32 rndGen;
#endif

	uint32 nRECount = m_pRenderData[nThreadID].size();
	for (uint32 r = 0; r < nRECount; ++r)
	{
		SHudData& pData = m_pRenderData[nThreadID][r];
		CShaderResources* pShaderResources = (CShaderResources*)pData.pShaderResources;

		pDiffuse = pData.pDiffuse;
		if (!pShaderResources || !pDiffuse || !pDiffuse->m_Sampler.m_pDynTexSource)
			continue;

		SRenderObjData* pROData = pData.pRO->GetObjData();

		int32 nRenderState = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA;
		if (!pROData || !(pROData->m_nCustomFlags & COB_HUD_REQUIRE_DEPTHTEST))
		{
			nRenderState |= GS_NODEPTHTEST;
		}

		GetUtils().ShBeginPass(CShaderMan::s_sh3DHUD, m_pGeneralTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		rd->SetCullMode(R_CULL_BACK);

		SetShaderParams(pData);

		// Set additional parameters
		vHudEffectParams[0].x = fCurrentDofBlend;

		// baking constant glow out of shader
		const float fGlowConstScale = CRenderer::CV_r_PostProcessHUD3DGlowAmount; // tweaked constant - before was 2.0, but barely visible due to hud shadow
		vHudEffectParams[0].y = pShaderResources->GetFinalEmittance().Luminance() * m_pGlowMul->GetParam() * fGlowConstScale;
		vHudEffectParams[0].z = interferenceStrength;

		if (pROData)
		{
			// Apply filter if ignore flag not set
			if ((pROData->m_nCustomFlags ^ COB_IGNORE_HUD_INTERFERENCE_FILTER) && interferenceStrength > 0.0f)  // without the last check it is possibel that vInterference is NaN
			{
				vHudEffectParams[0].z *= vInterferenceParams.x;
			}

			// Set per plane alpha multipler
			vOverrideColorParams.w = pData.pRO->m_fAlpha;
		}

		// baking out of shader shadow amount const multiplication
		const float fShadowAmountDotConst = 0.33f;
		vHudEffectParams[0].w = CRenderer::CV_r_PostProcessHUD3DShadowAmount * fShadowAmountDotConst;

		if (bInterferenceApplied)
		{
			float vTCScaleX = (float) SHudData::s_nFlashWidthMax / (float) m_pHUD_RT->GetWidth() * 2.0f;
			float vTCScaleY = (float) SHudData::s_nFlashHeightMax / (float) m_pHUD_RT->GetHeight() * 2.0f;

			vHudEffectParams[1].x = m_interferenceRandNums.x * vTCScaleX;
			vHudEffectParams[1].y = m_interferenceRandNums.y * vTCScaleY;
			vHudEffectParams[1].z = 0.0f;
			vHudEffectParams[1].w = m_interferenceRandNums.w;

			GetUtils().SetTexture(m_pNoise, 2, FILTER_POINT, eSamplerAddressMode_Wrap);
		}

#if !defined(_RELEASE) && !defined(CONSOLE_CONST_CVAR_MODE)
		// Debug settings
		if (m_bWireframeEnabled || bDrawingSolidView)
		{
			nRenderState = GS_BLSRC_ONE | GS_BLDST_ZERO;
			if (!pROData || !(pROData->m_nCustomFlags & COB_HUD_REQUIRE_DEPTHTEST))
			{
				nRenderState |= GS_NODEPTHTEST;
			}

			// Turn on wireframe
			if (m_bWireframeEnabled)
			{
				nRenderState |= GS_WIREFRAME;
			}
			// Set color of wire frame and solid fill debug views
			vHudEffectParams[0].x = (rndGen.GenerateFloat() * 0.75f) + 0.25f;
			vHudEffectParams[0].y = (rndGen.GenerateFloat() * 0.75f) + 0.25f;
			vHudEffectParams[0].z = (rndGen.GenerateFloat() * 0.75f) + 0.25f;
			vHudEffectParams[0].w = 1.0f;
		}
#endif

		CShaderMan::s_sh3DHUD->FXSetPSFloat(m_pHudEffectsParamName, vHudEffectParams, hudEffectParamCount);
		CShaderMan::s_sh3DHUD->FXSetPSFloat(m_pHudOverrideColorMultParamName, &vOverrideColorParams, 1);

		rd->FX_SetState(nRenderState);

		SetTextures(pData);
		GetUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[1], 1, FILTER_LINEAR);

		RenderMesh(pData.pRE, &pShaderTech->m_Passes[0]);

		GetUtils().ShEndPass();

		pPrevDiffuse = pDiffuse;
	}

	// Commit here as well, in case the HUD doesn't get drawn for some reason (e.g. hardcoded view distance for the HUD)
	gcpRendD3D->FX_Commit();

	gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE1]; // Remove interference combination
*/
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::FinalPass()
{
	ASSERT_LEGACY_PIPELINE
/*
	RenderFinalPass();

	// Debug views
#if !defined(_RELEASE) && !defined(CONSOLE_CONST_CVAR_MODE)
	if (CRenderer::CV_r_PostProcessHUD3DDebugView == eHUD3D_DEBUG_VIEW_UnwrapOnToFlashTex)
	{
		// Draw flash RT to screen for debugging
		GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, m_pTexToTexTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
		gcpRendD3D->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ZERO);
		GetUtils().SetTexture(m_pHUD_RT, 0, FILTER_LINEAR);
		GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());
		GetUtils().ShEndPass();
	}

	m_bDebugView = (CRenderer::CV_r_PostProcessHUD3DDebugView == eHUD3D_DEBUG_VIEW_UnwrapOnToFlashTex);
	m_bWireframeEnabled = (CRenderer::CV_r_PostProcessHUD3DDebugView == eHUD3D_DEBUG_VIEW_WireFrame) ||
	                      (CRenderer::CV_r_PostProcessHUD3DDebugView == eHUD3D_DEBUG_VIEW_UnwrapOnToFlashTex);

	if (m_bDebugView || m_bWireframeEnabled)
	{
		// Draw debug views
		RenderFinalPass();

		m_bDebugView = false;
		m_bWireframeEnabled = false;
	}
#endif
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHud3D::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("3D HUD");

	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] |
	                                       g_HWSR_MaskBit[HWSR_SAMPLE1] |
	                                       g_HWSR_MaskBit[HWSR_SAMPLE2] |
	                                       g_HWSR_MaskBit[HWSR_DEBUG1] |
	                                       g_HWSR_MaskBit[HWSR_DEBUG2]);

	CD3DStereoRenderer& pS3DRend = gcpRendD3D->GetS3DRend();
	bool bPostProcStereoAndSequential = pS3DRend.IsPostStereoEnabled() && pS3DRend.RequiresSequentialSubmission();
	bool bPostProcStereo = pS3DRend.IsPostStereoEnabled();

	// If post-stereo not enabled, update flash player
	// NanoGlass also updates Flash
	if (gcpRendD3D->m_nGraphicsPipeline == 0
	    && ((!bPostProcStereo && !PostEffectMgr()->GetEffect(EPostEffectID::NanoGlass)->IsActive()) || m_pHUD_RT == NULL))
	{
		FlashUpdateRT();
	}

	// Update interference rand timer
	m_interferenceRandTimer += gEnv->pTimer->GetFrameTime();

	// Render hud with projection offset or with same projection offset in MRT mode (a bit faster)
	if (bPostProcStereoAndSequential)
	{
		if (CRenderer::CV_r_StereoHudScreenDist)
		{
			float maxParallax = 0.005f * pS3DRend.GetStereoStrength();

			// Render left eye
			pS3DRend.BeginRenderingTo(CCamera::eEye_Left);
			m_maxParallax = -maxParallax;
			FinalPass();
			pS3DRend.EndRenderingTo(CCamera::eEye_Left);

			// Render right eye
			pS3DRend.BeginRenderingTo(CCamera::eEye_Right);
			m_maxParallax = maxParallax;
			FinalPass();
			pS3DRend.EndRenderingTo(CCamera::eEye_Right);

			m_maxParallax = 0;
		}
		else
		{
			pS3DRend.BeginRenderingMRT(false);
			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
			FinalPass();
			pS3DRend.EndRenderingMRT();
		}
	}
	else
	{
		FinalPass();
	}
*/
}

#pragma warning(pop)
