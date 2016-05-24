// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessWater : water related post processing

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"

#pragma warning(disable: 4244)

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CUnderwaterGodRays::Render()
{
	PROFILE_LABEL_SCOPE("GODRAYS");

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	float fAmount = m_pAmount->GetParam();
	float fWatLevel = SPostEffectsUtils::m_fWaterLevel;

	static CCryNameTSCRC pTechName("UnderwaterGodRays");
	static CCryNameR pParam0Name("PI_GodRaysParamsVS");
	static CCryNameR pParam1Name("PI_GodRaysParamsPS");

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];

	if (gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Render god-rays into low-res render target for less fillrate hit

	gcpRendD3D->FX_ClearTarget(CTexture::s_ptexBackBufferScaled[1], Clr_Transparent);
	gcpRendD3D->FX_PushRenderTarget(0, CTexture::s_ptexBackBufferScaled[1], 0);
	gcpRendD3D->RT_SetViewport(0, 0, CTexture::s_ptexBackBufferScaled[1]->GetWidth(), CTexture::s_ptexBackBufferScaled[1]->GetHeight());

	//  PostProcessUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES);
	uint32 nPasses;
	CShaderMan::s_shPostEffects->FXSetTechnique(pTechName);
	CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETSTATES);

	int nSlicesCount = 10;
	bool bInVisarea = (gRenDev->m_p3DEngineCommon.m_pCamVisAreaInfo.nFlags & S3DEngineCommon::VAF_EXISTS_FOR_POSITION);
	Vec3 vSunDir = bInVisarea ? Vec3(0.5f, 0.5f, 1.0f) : -gEnv->p3DEngine->GetSunDirNormalized();

	for (int r(0); r < nSlicesCount; ++r)
	{
		// !force updating constants per-pass!
		CShaderMan::s_shPostEffects->FXBeginPass(0);

		// Set per instance params
		Vec4 pParams = Vec4(fWatLevel, fAmount, r, 1.0f / (float) nSlicesCount);
		CShaderMan::s_shPostEffects->FXSetVSFloat(pParam0Name, &pParams, 1);

		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams, 1);

		gcpRendD3D->SetCullMode(R_CULL_NONE);
		gcpRendD3D->FX_SetState(GS_BLSRC_ONE | GS_BLDST_ONE | GS_NODEPTHTEST);

		PostProcessUtils().DrawFullScreenTri(CTexture::s_ptexBackBuffer->GetWidth(), CTexture::s_ptexBackBuffer->GetHeight(), 0);

		CShaderMan::s_shPostEffects->FXEndPass();
	}

	CShaderMan::s_shPostEffects->FXEnd();
	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;

	// Restore previous viewport
	gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Display god-rays

	CCryNameTSCRC pTechName0("UnderwaterGodRaysFinal");

	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechName0, FEF_DONTSETSTATES);
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
	Vec4 pParams = Vec4(CRenderer::CV_r_water_godrays_distortion, 0, 0, 0);
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams, 1);
	PostProcessUtils().DrawFullScreenTri(CTexture::s_ptexBackBuffer->GetWidth(), CTexture::s_ptexBackBuffer->GetHeight());
	PostProcessUtils().ShEndPass();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterDroplets::Render()
{
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	static CCryNameTSCRC pTechName("WaterDroplets");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	const float fUserAmount = m_pAmount->GetParam();

	const float fAtten = 1.0f; //clamp_tpl<float>(fabs( gRenDev->GetRCamera().vOrigin.z - SPostEffectsUtils::m_fWaterLevel ), 0.0f, 1.0f);
	Vec4 vParams = Vec4(1, 1, 1, min(fUserAmount, 1.0f) * fAtten);
	static CCryNameR pParamName("waterDropletsParams");
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

	PostProcessUtils().DrawFullScreenTri(CTexture::s_ptexBackBuffer->GetWidth(), CTexture::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterFlow::Render()
{
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	float fAmount = m_pAmount->GetParam();

	static CCryNameTSCRC pTechName("WaterFlow");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	Vec4 vParams = Vec4(1, 1, 1, fAmount);
	static CCryNameR pParamName("waterFlowParams");
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

	PostProcessUtils().DrawFullScreenTri(CTexture::s_ptexBackBuffer->GetWidth(), CTexture::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Enabled/Disabled if no hits on list to process - call from render thread
bool CWaterRipples::RT_SimulationStatus()
{
	return !s_pWaterHits[gcpRendD3D->m_RP.m_nProcessThreadID].empty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CWaterRipples::Preprocess()
{
	const float fTimeOut = 10.0f; // 10 seconds
	bool bSimTimeOut = (gEnv->pTimer->GetCurrTime() - m_fLastSpawnTime) > fTimeOut;
	if (s_nUpdateMask || CRenderer::CV_r_PostProcessGameFx && m_pAmount->GetParam() > 0.005f && (RT_SimulationStatus() || !bSimTimeOut))
		return true;

	m_bInitializeSim = true;
	m_pAmount->ResetParam(0.0f);

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterRipples::CreatePhysCallbacks()
{
	if (gEnv->pPhysicalWorld)
		gEnv->pPhysicalWorld->AddEventClient(EventPhysCollision::id, &OnEventPhysCollision, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterRipples::ReleasePhysCallbacks()
{
	if (gEnv->pPhysicalWorld)
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysCollision::id, &OnEventPhysCollision, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterRipples::AddHit(const Vec3& vPos, const float scale, const float strength)
{
	const uint32 nThreadID = gcpRendD3D->m_RP.m_nFillThreadID;
	if (s_pWaterHits[nThreadID].size() < MAX_HITS)
	{
		s_pWaterHits[nThreadID].push_back(SWaterHit(vPos, scale, strength));

#if !defined(_RELEASE)
		const int nDisplayFrames = 60;

		SWaterHitRecord hit;
		hit.mHit = s_pWaterHits[nThreadID].back();
		hit.fHeight = vPos.z;
		hit.nCounter = nDisplayFrames;

		std::vector<SWaterHitRecord>::iterator itHits = m_DebugWaterHits.begin();
		for (; itHits != m_DebugWaterHits.end(); ++itHits)
		{
			if (itHits->nCounter <= 0)
			{
				*itHits = hit;
				break;
			}
		}

		if (itHits == m_DebugWaterHits.end())
			m_DebugWaterHits.push_back(hit);
#endif

	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterRipples::Reset(bool bOnSpecChange)
{
#ifndef _RELEASE
	if (!gRenDev->m_pRT->IsRenderThread(true))
	{
		if (gEnv && gEnv->pSystem && (gEnv->pSystem->IsQuitting() == false))
		{
			__debugbreak();
		}
	}
#endif

	m_fLastSpawnTime = 0.0f;
	m_fLastUpdateTime = 0.0f;
	m_bInitializeSim = true;
	s_SimOrigin = Vec2(ZERO);

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
		stl::free_container(s_pWaterHits[i]);
	stl::free_container(s_pWaterHitsMGPU);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CWaterRipples::OnEventPhysCollision(const EventPhys* pEvent)
{
	EventPhysCollision* pEventPhysArea = (EventPhysCollision*)pEvent;
	if (!pEventPhysArea)
		return 1;

	// only add nearby hits
	if (pEventPhysArea->idmat[1] == gEnv->pPhysicalWorld->GetWaterMat())
	{
		// Compute the momentum of the object.
		// Clamp the mass so that particles and other "massless" objects still cause ripples.
		float fMomentum = pEventPhysArea->vloc->GetLength() * max(pEventPhysArea->mass[0], 0.025f);
		AddHit(pEventPhysArea->pt, 1.0f, fMomentum);
	}

	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterRipples::RenderHits()
{
	const uint32 nThreadID = gcpRendD3D->m_RP.m_nProcessThreadID;

	const uint32 nWidth = CTexture::s_ptexWaterRipplesDDN->GetWidth();
	const uint32 nHeight = CTexture::s_ptexWaterRipplesDDN->GetHeight();
	const float fWidth = (float)nWidth;
	const float fHeight = (float)nHeight;
	const float fWidthRcp = 1.0f / fWidth;
	const float fHeightRcp = 1.0f / fHeight;
	const float fRatio = fHeightRcp / fWidthRcp;

	const float fWidthHit = 2.0f * fWidthRcp * fRatio;
	const float fHeightHit = 2.0f * fHeightRcp;

	gcpRendD3D->Set2DMode(true, nWidth, nHeight);

	// Add hits to simulation
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, m_pRipplesHitTechName, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);

	// only update blue channel: current frame
	gcpRendD3D->FX_SetState(GS_BLSRC_ONE | GS_BLDST_ONE | GS_NODEPTHTEST | GS_NOCOLMASK_R | GS_NOCOLMASK_G | GS_NOCOLMASK_A);

	// Simulation origin point is snapped camera position to nearest m_fSimGridSnapRange
	Vec2 vSimOrig = Vec2(s_CameraPos.x - fabs(fmodf(s_CameraPos.x, m_fSimGridSnapRange)),
	                     s_CameraPos.y - fabs(fmodf(s_CameraPos.y, m_fSimGridSnapRange)));

	uint32 nHitCount = s_pWaterHitsMGPU.size();
	uint32 nCurrHit = 0;

	for (; nCurrHit < nHitCount; ++nCurrHit)
	{
		SWaterHit& currentHit = s_pWaterHitsMGPU[nCurrHit];

		// Map hit world space to simulation space
		float xmapped = currentHit.worldPos.x * s_vLookupParams.x + s_vLookupParams.z;
		float ymapped = currentHit.worldPos.y * s_vLookupParams.x + s_vLookupParams.w;

		// Render a sprite at hit location
		float x0 = (xmapped - 0.5f * (fWidthHit * currentHit.scale)) * fWidth;
		float y0 = (ymapped - 0.5f * (fHeightHit * currentHit.scale)) * fHeight;

		float x1 = (xmapped + 0.5f * (fWidthHit * currentHit.scale)) * fWidth;
		float y1 = (ymapped + 0.5f * (fHeightHit * currentHit.scale)) * fHeight;

		// Pass height scale to shader.
		s_vParams.w = currentHit.strength;
		CShaderMan::s_shPostEffects->FXSetPSFloat(m_pRipplesParamName, &s_vParams, 1);

		PostProcessUtils().DrawScreenQuad(nWidth, nHeight, x0, y0, x1, y1);
	}

	PostProcessUtils().ShEndPass();

	gcpRendD3D->Set2DMode(false, nWidth, nHeight);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterRipples::Render()
{
	if (!CTexture::s_ptexWaterRipplesDDN || !CTexture::IsTextureExist(CTexture::s_ptexBackBufferScaled[0]))
		return;

	const uint32 nThreadID = gRenDev->m_RP.m_nProcessThreadID;
	int32 nGpuID = 0;//gRenDev->RT_GetCurrGpuID();

	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	// always snap by entire pixels to avoid errors when displacing the simulation
	const float fPixelSizeWS = CTexture::s_ptexWaterRipplesDDN->GetWidth() > 0 ? 2.f * m_fSimGridSize / CTexture::s_ptexWaterRipplesDDN->GetWidth() : 1.0f;
	m_fSimGridSnapRange = max(ceilf(m_fSimGridSnapRange / fPixelSizeWS), 1.f) * fPixelSizeWS;

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	float fTime = gRenDev->m_RP.m_TI[nThreadID].m_RealTime;

	// only allow updates every 25ms - for lower frames, would need to iterate multiple times
	if (gRenDev->GetActiveGPUCount() == 1)
	{
		if (fTime - m_fLastUpdateTime < 0.025f)
			return;
		m_fLastUpdateTime = fTime - fmodf(fTime - m_fLastUpdateTime, 0.025f);
	}
	else
	{
		if (fTime - m_fLastUpdateTime <= 0.0f)
			return;
		m_fLastUpdateTime = gRenDev->m_RP.m_TI[nThreadID].m_RealTime;
	}

	Vec4 vParams = Vec4(0, 0, 0, 0);
	if (!s_pWaterHits[nThreadID].empty())
	{
		vParams = Vec4(s_pWaterHits[nThreadID][0].worldPos.x, s_pWaterHits[nThreadID][0].worldPos.y, 0, 1.0f);
		m_fLastSpawnTime = fTime;
	}

	if (!s_nUpdateMask)
	{
		s_CameraPos = gRenDev->GetRCamera().vOrigin;
		float xsnap = ceilf(s_CameraPos.x / m_fSimGridSnapRange) * m_fSimGridSnapRange;
		float ysnap = ceilf(s_CameraPos.y / m_fSimGridSnapRange) * m_fSimGridSnapRange;

		m_bSnapToCenter = false;
		if (s_SimOrigin.x != xsnap || s_SimOrigin.y != ysnap)
		{
			m_bSnapToCenter = true;
			vParams.x = (xsnap - s_SimOrigin.x) * s_vLookupParams.x;
			vParams.y = (ysnap - s_SimOrigin.y) * s_vLookupParams.x;

			s_SimOrigin = Vec2(xsnap, ysnap);
		}

		s_bInitializeSim = m_bInitializeSim;
		s_vParams = vParams;
		s_pWaterHitsMGPU = s_pWaterHits[nThreadID];

		// update world space -> sim space transform
		s_vLookupParams.x = 1.f / (2.f * m_fSimGridSize);
		s_vLookupParams.y = m_fSimGridSize - m_fSimGridSnapRange;
		s_vLookupParams.z = -s_SimOrigin.x * s_vLookupParams.x + 0.5f;
		s_vLookupParams.w = -s_SimOrigin.y * s_vLookupParams.x + 0.5f;

		if (gRenDev->GetActiveGPUCount() > 1)
			s_nUpdateMask = (1 << gRenDev->GetActiveGPUCount()) - 1;
	}

	PROFILE_LABEL_SCOPE("WATER RIPPLES GEN");

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1]);

	// Initialize sim on first frame
	if (s_bInitializeSim)
	{
		m_bInitializeSim = false;

		RECT rect = { 0, 0, CTexture::s_ptexWaterRipplesDDN->GetWidth(), CTexture::s_ptexWaterRipplesDDN->GetHeight() };
		gcpRendD3D->FX_ClearTarget(CTexture::s_ptexBackBufferScaled[0], Clr_Transparent, 1, &rect, true);
	}

	// spawn particles into effects accumulation buffer
	gcpRendD3D->FX_PushRenderTarget(0, CTexture::s_ptexBackBufferScaled[0], 0);
	gcpRendD3D->RT_SetViewport(0, 0, CTexture::s_ptexWaterRipplesDDN->GetWidth(), CTexture::s_ptexWaterRipplesDDN->GetHeight());

	// Snapping occurred - update simulation to new offset
	if (m_bSnapToCenter)
	{
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

		PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, m_pRipplesGenTechName, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);
		gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

		CShaderMan::s_shPostEffects->FXSetPSFloat(m_pRipplesParamName, &s_vParams, 1);

		PostProcessUtils().SetTexture(CTexture::s_ptexWaterRipplesDDN, 0, FILTER_LINEAR, 1);
		PostProcessUtils().DrawFullScreenTri(CTexture::s_ptexWaterRipplesDDN->GetWidth(), CTexture::s_ptexWaterRipplesDDN->GetHeight());

		PostProcessUtils().ShEndPass();

		PostProcessUtils().CopyScreenToTexture(CTexture::s_ptexWaterRipplesDDN);

		gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];
	}

	// Compute wave propagation
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, m_pRipplesGenTechName, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	CShaderMan::s_shPostEffects->FXSetPSFloat(m_pRipplesParamName, &s_vParams, 1);

	PostProcessUtils().SetTexture(CTexture::s_ptexWaterRipplesDDN, 0, FILTER_LINEAR, 1);
	PostProcessUtils().DrawFullScreenTri(CTexture::s_ptexWaterRipplesDDN->GetWidth(), CTexture::s_ptexWaterRipplesDDN->GetHeight());
	PostProcessUtils().ShEndPass();

	// Add current frame hits
	RenderHits();

	PostProcessUtils().CopyScreenToTexture(CTexture::s_ptexWaterRipplesDDN);

	CTexture::s_ptexBackBufferScaled[0]->SetResolved(true);
	gcpRendD3D->FX_PopRenderTarget(0);

	CTexture::s_ptexWaterRipplesDDN->GenerateMipMaps();

	gcpRendD3D->RT_SetViewport(0, 0, iWidth, iHeight);

	// disable processing
	m_pAmount->SetParam(0.0f);
	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;

	s_nUpdateMask &= ~(1 << gRenDev->RT_GetCurrGpuID());
	s_pWaterHits[nThreadID].clear();
}

void CWaterRipples::DEBUG_DrawWaterHits()
{
	gRenDev->GetIRenderAuxGeom()->SetRenderFlags(SAuxGeomRenderFlags());
	for (std::vector<SWaterHitRecord>::iterator itHits = m_DebugWaterHits.begin();
	     itHits != m_DebugWaterHits.end(); ++itHits)
	{
		if (itHits->nCounter > 0)
		{
			--itHits->nCounter;

			Vec3 vHitPos(itHits->mHit.worldPos.x, itHits->mHit.worldPos.y, itHits->fHeight);
			gRenDev->GetIRenderAuxGeom()->DrawSphere(vHitPos, 0.15f, ColorB(255, 0, 0, 255));
		}
	}

	Vec3 simCenter = Vec3(s_SimOrigin.x, s_SimOrigin.y, gRenDev->GetRCamera().vOrigin.z - 1.5f);
	gRenDev->GetIRenderAuxGeom()->DrawSphere(simCenter, 0.15f, ColorB(0, 255, 0, 255));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

namespace WaterVolumeStaticData
{
CWater* pWaterSim = 0;
void GetMemoryUsage(ICrySizer* pSizer)
{
	if (pWaterSim)
		pWaterSim->GetMemoryUsage(pSizer);
}
}

void CWaterVolume::Render()
{
	PROFILE_LABEL_SCOPE("WATERVOLUME_TEXGEN");

	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	const int frameID = SPostEffectsUtils::m_iFrameCounter % 2;

	{
		static int nFrameID = 0;

		// remember ptr of WaterSim to access it with CrySizer
		WaterVolumeStaticData::pWaterSim = WaterSimMgr();

		const int nGridSize = 64;

		int nCurFrameID = gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_nFrameID;
		if (nFrameID != nCurFrameID)
		{
			static Vec4 pParams0(0, 0, 0, 0), pParams1(0, 0, 0, 0);
			Vec4 pCurrParams0, pCurrParams1;
			gEnv->p3DEngine->GetOceanAnimationParams(pCurrParams0, pCurrParams1);

			// Update sim settings
			if (WaterSimMgr()->NeedInit() || pCurrParams0.x != pParams0.x || pCurrParams0.y != pParams0.y ||
			    pCurrParams0.z != pParams0.z || pCurrParams0.w != pParams0.w || pCurrParams1.x != pParams1.x ||
			    pCurrParams1.y != pParams1.y || pCurrParams1.z != pParams1.z || pCurrParams1.w != pParams1.w)
			{
				pParams0 = pCurrParams0;
				pParams1 = pCurrParams1;
				WaterSimMgr()->Create(1.0, pParams0.x, pParams0.z, 1.0f, 1.0f);
			}

			// Create texture if required
			if (!CTexture::IsTextureExist(CTexture::s_ptexWaterVolumeTemp[frameID]))
			{
				if (!CTexture::s_ptexWaterVolumeTemp[frameID]->Create2DTexture(64, 64, 1,
				                                                               FT_DONT_RELEASE | FT_NOMIPS | FT_STAGE_UPLOAD,
				                                                               0, eTF_R32G32B32A32F, eTF_R32G32B32A32F))
					return;
			}

			CTexture* pTexture = CTexture::s_ptexWaterVolumeTemp[frameID];

			// Copy data..
			if (CTexture::IsTextureExist(pTexture))
			{
				//const float fUpdateTime = 2.f*0.125f*gEnv->pTimer->GetCurrTime();
				const float fUpdateTime = 0.125 * gEnv->pTimer->GetCurrTime();// / clamp_tpl<float>(pParams1.x, 0.55f, 1.0f);

				void* pRawPtr = NULL;
				WaterSimMgr()->Update(nCurFrameID, fUpdateTime, true, pRawPtr);

				Vec4* pDispGrid = WaterSimMgr()->GetDisplaceGrid();

				const uint32 pitch = 4 * sizeof(f32) * nGridSize;
				const uint32 width = nGridSize;
				const uint32 height = nGridSize;

				STALL_PROFILER("update subresource")

				CDeviceTexture * pDevTex = pTexture->GetDevTexture();
				pDevTex->UploadFromStagingResource(0, [=](void* pData, uint32 rowPitch, uint32 slicePitch)
				{
					cryMemcpy(pData, pDispGrid, 4 * width * height * sizeof(f32));
					return true;
				});
			}
			nFrameID = nCurFrameID;
		}
	}

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	// make final normal map
	gcpRendD3D->FX_PushRenderTarget(0, CTexture::s_ptexWaterVolumeDDN, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, CTexture::s_ptexWaterVolumeDDN->GetWidth(), CTexture::s_ptexWaterVolumeDDN->GetHeight());

	static CCryNameTSCRC pTechName("WaterVolumesNormalGen");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	static CCryNameR pParamName("waterVolumesParams");
	Vec4 vParams = Vec4(64.0f, 64.0f, 64.0f, 64.0f);
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

	int32 nFilter = FILTER_LINEAR;

	PostProcessUtils().SetTexture(CTexture::s_ptexWaterVolumeTemp[frameID], 0, nFilter, 0);
	PostProcessUtils().DrawFullScreenTri(CTexture::s_ptexBackBuffer->GetWidth(), CTexture::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();

	gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(0, 0, iWidth, iHeight);

	CTexture::s_ptexWaterVolumeDDN->GenerateMipMaps();

	// HACK (re-set back-buffer): due to lazy RT updates/setting there's strong possibility we run into problems on x360 when we try to resolve from edram with no RT set
	gcpRendD3D->FX_SetActiveRenderTargets();
	gcpRendD3D->FX_ResetPipe();

	// disable processing
	m_pAmount->SetParam(0.0f);
}
