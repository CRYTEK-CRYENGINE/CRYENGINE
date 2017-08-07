// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessUtils.cpp : Post processing common utilities

   Revision history:
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include "PostProcessUtils.h"
#include "../RendElements/FlareSoftOcclusionQuery.h"

// class CMipmapGenPass;
#include "../../XRenderD3D9/GraphicsPipeline/Common/UtilityPasses.h"

RECT SPostEffectsUtils::m_pScreenRect;
ITimer* SPostEffectsUtils::m_pTimer;
int SPostEffectsUtils::m_iFrameCounter = 0;
SDepthTexture* SPostEffectsUtils::m_pCurDepthSurface;
CShader* SPostEffectsUtils::m_pCurrShader;
int SPostEffectsUtils::m_nColorMatrixFrameID;
float SPostEffectsUtils::m_fWaterLevel;
float SPostEffectsUtils::m_fOverscanBorderAspectRatio = 1.0f;
Matrix44 SPostEffectsUtils::m_pScaleBias = Matrix44(0.5f, 0, 0, 0,
                                                    0, -0.5f, 0, 0,
                                                    0, 0, 1.0f, 0,
                                                    0.5f, 0.5f, 0, 1.0f);

Vec3 SPostEffectsUtils::m_vRT = Vec3(0, 0, 0);
Vec3 SPostEffectsUtils::m_vLT = Vec3(0, 0, 0);
Vec3 SPostEffectsUtils::m_vLB = Vec3(0, 0, 0);
Vec3 SPostEffectsUtils::m_vRB = Vec3(0, 0, 0);
int SPostEffectsUtils::m_nFrustrumFrameID = 0;
CRenderCamera SPostEffectsUtils::m_cachedRenderCamera;

namespace
{
bool CompareRenderCamera(const CRenderCamera& lrc, const CRenderCamera& rrc)
{
	if (lrc.vX != rrc.vX) return false;
	if (lrc.vY != rrc.vY) return false;
	if (lrc.vZ != rrc.vZ) return false;
	if (lrc.vOrigin != rrc.vOrigin) return false;
	if (lrc.fWL != rrc.fWL) return false;
	if (lrc.fWR != rrc.fWR) return false;
	if (lrc.fWB != rrc.fWB) return false;
	if (lrc.fWT != rrc.fWT) return false;
	if (lrc.fNear != rrc.fNear) return false;
	if (lrc.fFar != rrc.fFar) return false;

	return true;
}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SPostEffectsUtils::Create()
{
	const SViewport& MainVp = gRenDev->m_MainViewport;
	const bool bCreatePostAA = CRenderer::CV_r_AntialiasingMode && (!CTexture::IsTextureExist(CTexture::s_ptexPrevBackBuffer[0][0])) || (gRenDev->IsStereoEnabled() && !CTexture::s_ptexPrevBackBuffer[0][1]);
	const bool bCreateCaustics = (CRenderer::CV_r_watervolumecaustics && CRenderer::CV_r_watercaustics && CRenderer::CV_r_watercausticsdeferred) && !CTexture::IsTextureExist(CTexture::s_ptexWaterCaustics[0]);
	if (!CTexture::s_ptexBackBufferScaled[0] || m_pScreenRect.right != MainVp.nWidth || m_pScreenRect.bottom != MainVp.nHeight || bCreatePostAA || bCreateCaustics)
	{
		assert(gRenDev);

		const int width = gRenDev->GetWidth(), width_r2 = (width + 1) / 2, width_r4 = (width_r2 + 1) / 2, width_r8 = (width_r4 + 1) / 2;
		const int height = gRenDev->GetHeight(), height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2;

		// Update viewport info
		m_pScreenRect.left = 0;
		m_pScreenRect.top = 0;

		m_pScreenRect.right = width;
		m_pScreenRect.bottom = height;

		if (CRenderer::CV_r_AntialiasingMode)
		{
			GetOrCreateRenderTarget("$PrevBackBuffer0", CTexture::s_ptexPrevBackBuffer[0][0], width, height, Clr_Unknown, 1, 0, eTF_R16G16B16A16, TO_PREVBACKBUFFERMAP0, FT_DONT_RELEASE);
			GetOrCreateRenderTarget("$PrevBackBuffer1", CTexture::s_ptexPrevBackBuffer[1][0], width, height, Clr_Unknown, 1, 0, eTF_R16G16B16A16, TO_PREVBACKBUFFERMAP1, FT_DONT_RELEASE);
			if (gRenDev->IsStereoEnabled())
			{
				GetOrCreateRenderTarget("$PrevBackBuffer0_R", CTexture::s_ptexPrevBackBuffer[0][1], width, height, Clr_Unknown, 1, 0, eTF_R16G16B16A16, -1, FT_DONT_RELEASE);
				GetOrCreateRenderTarget("$PrevBackBuffer1_R", CTexture::s_ptexPrevBackBuffer[1][1], width, height, Clr_Unknown, 1, 0, eTF_R16G16B16A16, -1, FT_DONT_RELEASE);
			}
		}
		else
		{
			SAFE_RELEASE(CTexture::s_ptexPrevBackBuffer[0][0]);
			SAFE_RELEASE(CTexture::s_ptexPrevBackBuffer[1][0]);
			SAFE_RELEASE(CTexture::s_ptexPrevBackBuffer[0][1]);
			SAFE_RELEASE(CTexture::s_ptexPrevBackBuffer[1][1]);
		}

		GetOrCreateRenderTarget("$Cached3DHud", CTexture::s_ptexCached3DHud, width, height, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);
		GetOrCreateRenderTarget("$Cached3DHudDownsampled", CTexture::s_ptexCached3DHudScaled, width_r4, height_r4, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

		// Scaled versions of the scene target
		GetOrCreateRenderTarget("$BackBufferScaled_d2", CTexture::s_ptexBackBufferScaled[0], width_r2, height_r2, Clr_Unknown, 1, 0, eTF_R8G8B8A8, TO_BACKBUFFERSCALED_D2, FT_DONT_RELEASE);

		// Ghosting requires data overframes, need to handle for each GPU in MGPU mode
		GetOrCreateRenderTarget("$PrevFrameScaled", CTexture::s_ptexPrevFrameScaled, width_r2, height_r2, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

		GetOrCreateRenderTarget("$BackBufferScaledTemp_d2", CTexture::s_ptexBackBufferScaledTemp[0], width_r2, height_r2, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);
		GetOrCreateRenderTarget("$WaterVolumeRefl", CTexture::s_ptexWaterVolumeRefl[0], width_r2, height_r2, Clr_Unknown, 1, true, eTF_R11G11B10F, TO_WATERVOLUMEREFLMAP, FT_DONT_RELEASE);
		GetOrCreateRenderTarget("$WaterVolumeReflPrev", CTexture::s_ptexWaterVolumeRefl[1], width_r2, height_r2, Clr_Unknown, 1, true, eTF_R11G11B10F, TO_WATERVOLUMEREFLMAPPREV, FT_DONT_RELEASE);

		//	CTexture::s_ptexWaterVolumeRefl[0]->DisableMgpuSync();
		//	CTexture::s_ptexWaterVolumeRefl[1]->DisableMgpuSync();

		GetOrCreateRenderTarget("$BackBufferScaled_d4", CTexture::s_ptexBackBufferScaled[1], width_r4, height_r4, Clr_Unknown, 1, 0, eTF_R8G8B8A8, TO_BACKBUFFERSCALED_D4, FT_DONT_RELEASE);
		GetOrCreateRenderTarget("$BackBufferScaledTemp_d4", CTexture::s_ptexBackBufferScaledTemp[1], width_r4, height_r4, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

		GetOrCreateRenderTarget("$BackBufferScaled_d8", CTexture::s_ptexBackBufferScaled[2], width_r8, height_r8, Clr_Unknown, 1, 0, eTF_R8G8B8A8, TO_BACKBUFFERSCALED_D8, FT_DONT_RELEASE);

		GetOrCreateRenderTarget("$RainDropsAccumRT_0", CTexture::s_ptexRainDropsRT[0], width_r4, height_r4, Clr_Unknown, 1, false, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);
		GetOrCreateRenderTarget("$RainDropsAccumRT_1", CTexture::s_ptexRainDropsRT[1], width_r4, height_r4, Clr_Unknown, 1, false, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

		GetOrCreateRenderTarget("$RainSSOcclusion0", CTexture::s_ptexRainSSOcclusion[0], width_r8, height_r8, Clr_Unknown, 1, false, eTF_R8G8B8A8);
		GetOrCreateRenderTarget("$RainSSOcclusion1", CTexture::s_ptexRainSSOcclusion[1], width_r8, height_r8, Clr_Unknown, 1, false, eTF_R8G8B8A8);

		GetOrCreateRenderTarget("$RainOcclusion", CTexture::s_ptexRainOcclusion, RAIN_OCC_MAP_SIZE, RAIN_OCC_MAP_SIZE, Clr_Unknown, false, false, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

		GetOrCreateRenderTarget("$WaterVolumeDDN", CTexture::s_ptexWaterVolumeDDN, 64, 64, Clr_Unknown, 1, true, eTF_R16G16B16A16F, TO_WATERVOLUMEMAP);
		//CTexture::s_ptexWaterVolumeDDN->DisableMgpuSync();

		if (CRenderer::CV_r_watervolumecaustics && CRenderer::CV_r_watercaustics && CRenderer::CV_r_watercausticsdeferred)
		{
			const int nCausticRes = clamp_tpl(CRenderer::CV_r_watervolumecausticsresolution, 256, 4096);
			GetOrCreateRenderTarget("$WaterVolumeCaustics", CTexture::s_ptexWaterCaustics[0], nCausticRes, nCausticRes, Clr_Unknown, 1, false, eTF_R8G8B8A8, TO_WATERVOLUMECAUSTICSMAP);
			GetOrCreateRenderTarget("$WaterVolumeCausticsTemp", CTexture::s_ptexWaterCaustics[1], nCausticRes, nCausticRes, Clr_Unknown, 1, false, eTF_R8G8B8A8, TO_WATERVOLUMECAUSTICSMAPTEMP);
		}
		else
		{
			SAFE_RELEASE(CTexture::s_ptexWaterCaustics[0]);
			SAFE_RELEASE(CTexture::s_ptexWaterCaustics[1]);
		}

#if defined(VOLUMETRIC_FOG_SHADOWS)
		int fogShadowBufDiv = (CRenderer::CV_r_FogShadows == 2) ? 4 : 2;
		GetOrCreateRenderTarget("$VolFogShadowBuf0", CTexture::s_ptexVolFogShadowBuf[0], width / fogShadowBufDiv, height / fogShadowBufDiv, Clr_Unknown, 1, 0, eTF_R8G8B8A8, TO_VOLFOGSHADOW_BUF);
		GetOrCreateRenderTarget("$VolFogShadowBuf1", CTexture::s_ptexVolFogShadowBuf[1], width / fogShadowBufDiv, height / fogShadowBufDiv, Clr_Unknown, 1, 0, eTF_R8G8B8A8);
#endif

		// TODO: Only create necessary RTs for minimal ring?
		char str[256];
		for (int i = 0; i < MAX_OCCLUSION_READBACK_TEXTURES; i++)
		{
			sprintf(str, "$FlaresOcclusion_%d", i);
			GetOrCreateRenderTarget(str, CTexture::s_ptexFlaresOcclusionRing[i], CFlareSoftOcclusionQuery::s_nIDColMax, CFlareSoftOcclusionQuery::s_nIDRowMax, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE | FT_STAGE_READBACK);
		}

		GetOrCreateRenderTarget("$FlaresGather", CTexture::s_ptexFlaresGather, CFlareSoftOcclusionQuery::s_nGatherTextureWidth, CFlareSoftOcclusionQuery::s_nGatherTextureHeight, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);
	}

	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::Release()
{
	SAFE_RELEASE(CTexture::s_ptexPrevBackBuffer[0][0]);
	SAFE_RELEASE(CTexture::s_ptexPrevBackBuffer[1][0]);
	SAFE_RELEASE(CTexture::s_ptexPrevBackBuffer[0][1]);
	SAFE_RELEASE(CTexture::s_ptexPrevBackBuffer[1][1]);

	SAFE_RELEASE(CTexture::s_ptexBackBufferScaled[0]);
	SAFE_RELEASE(CTexture::s_ptexBackBufferScaled[1]);
	SAFE_RELEASE(CTexture::s_ptexBackBufferScaled[2]);

	SAFE_RELEASE(CTexture::s_ptexBackBufferScaledTemp[0]);
	SAFE_RELEASE(CTexture::s_ptexBackBufferScaledTemp[1]);

	SAFE_RELEASE(CTexture::s_ptexWaterVolumeDDN);
	SAFE_RELEASE(CTexture::s_ptexWaterVolumeRefl[0]);
	SAFE_RELEASE(CTexture::s_ptexWaterVolumeRefl[1]);
	SAFE_RELEASE(CTexture::s_ptexWaterCaustics[0]);
	SAFE_RELEASE(CTexture::s_ptexWaterCaustics[1]);

	SAFE_RELEASE(CTexture::s_ptexCached3DHud);
	SAFE_RELEASE(CTexture::s_ptexCached3DHudScaled);

	SAFE_RELEASE(CTexture::s_ptexPrevFrameScaled);

	SAFE_RELEASE(CTexture::s_ptexRainDropsRT[0]);
	SAFE_RELEASE(CTexture::s_ptexRainDropsRT[1]);

	SAFE_RELEASE(CTexture::s_ptexRainSSOcclusion[0]);
	SAFE_RELEASE(CTexture::s_ptexRainSSOcclusion[1]);
	SAFE_RELEASE(CTexture::s_ptexRainOcclusion);

#if defined(VOLUMETRIC_FOG_SHADOWS)
	SAFE_RELEASE(CTexture::s_ptexVolFogShadowBuf[0]);
	SAFE_RELEASE(CTexture::s_ptexVolFogShadowBuf[1]);
#endif

	for (int i = 0; i < MAX_OCCLUSION_READBACK_TEXTURES; i++)
		SAFE_RELEASE(CTexture::s_ptexFlaresOcclusionRing[i]);
	SAFE_RELEASE(CTexture::s_ptexFlaresGather);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void SPostEffectsUtils::GetFullScreenTri(SVF_P3F_C4B_T2F pResult[3], int nTexWidth, int nTexHeight, float z, const RECT* pSrcRegion)
{
	if (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
		z = 1.0f - z;

	pResult[0].xyz = Vec3(-0.0f, -0.0f, z);
	pResult[0].color.dcolor = ~0U;
	pResult[0].st = Vec2(0, 0);
	pResult[1].xyz = Vec3(-0.0f, 2.0f, z);
	pResult[1].color.dcolor = ~0U;
	pResult[1].st = Vec2(0, 2);
	pResult[2].xyz = Vec3(2.0f, -0.0f, z);
	pResult[2].color.dcolor = ~0U;
	pResult[2].st = Vec2(2, 0);

	if (pSrcRegion)
	{
		const Vec4 vTexCoordsRegion(2.0f * float(pSrcRegion->left) / nTexWidth,
		                            2.0f * float(pSrcRegion->right) / nTexWidth,
		                            2.0f * float(pSrcRegion->top) / nTexHeight,
		                            2.0f * float(pSrcRegion->bottom) / nTexHeight);
		pResult[0].st = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.z);
		pResult[1].st = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.w);
		pResult[2].st = Vec2(vTexCoordsRegion.y, vTexCoordsRegion.z);
	}
}

void SPostEffectsUtils::GetFullScreenQuad(SVF_P3F_C4B_T2F pResult[4], int nTexWidth, int nTexHeight, float z, const RECT* pSrcRegion)
{
	if (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
		z = 1.0f - z;

	pResult[0].xyz = Vec3(-0.0f, -0.0f, z);
	pResult[0].color.dcolor = ~0U;
	pResult[0].st = Vec2(0, 0);
	pResult[1].xyz = Vec3(-0.0f, 1.0f, z);
	pResult[1].color.dcolor = ~0U;
	pResult[1].st = Vec2(0, 1);
	pResult[2].xyz = Vec3(1.0f, -0.0f, z);
	pResult[2].color.dcolor = ~0U;
	pResult[2].st = Vec2(1, 0);
	pResult[3].xyz = Vec3(1.0f, 1.0f, z);
	pResult[3].color.dcolor = ~0U;
	pResult[3].st = Vec2(1, 1);

	if (pSrcRegion)
	{
		const Vec4 vTexCoordsRegion(float(pSrcRegion->left) / nTexWidth,
		                            float(pSrcRegion->right) / nTexWidth,
		                            float(pSrcRegion->top) / nTexHeight,
		                            float(pSrcRegion->bottom) / nTexHeight);
		pResult[0].st = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.z);
		pResult[1].st = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.w);
		pResult[2].st = Vec2(vTexCoordsRegion.y, vTexCoordsRegion.z);
		pResult[3].st = Vec2(vTexCoordsRegion.y, vTexCoordsRegion.w);
	}
}

void SPostEffectsUtils::DrawFullScreenTri(int nTexWidth, int nTexHeight, float z, const RECT* pSrcRegion)
{

	CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 4, screenQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);
	GetFullScreenQuad(screenQuad, nTexWidth, nTexHeight, z, pSrcRegion);

	CVertexBuffer strip(screenQuad, EDefaultInputLayouts::P3F_C4B_T2F);
	gRenDev->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::DrawScreenQuad(int nTexWidth, int nTexHeight, float x0, float y0, float x1, float y1)
{
	const float z = (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) ? 1.0f : 0.0f;
	CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 4, pScreenQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);

	pScreenQuad[0] = {
		Vec3(x0, y0,     z), {
			{ 0 }
		},       Vec2(0, 0)
	};
	pScreenQuad[1] = {
		Vec3(x0, y1,     z), {
			{ 0 }
		},       Vec2(0, 1)
	};
	pScreenQuad[2] = {
		Vec3(x1, y0,     z), {
			{ 0 }
		},       Vec2(1, 0)
	};
	pScreenQuad[3] = {
		Vec3(x1, y1,     z), {
			{ 0 }
		},       Vec2(1, 1)
	};

	gRenDev->m_RP.m_PersFlags2 &= ~(RBPF2_COMMIT_PF);

	CVertexBuffer strip(pScreenQuad, EDefaultInputLayouts::P3F_C4B_T2F);
	gRenDev->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::DrawQuad(int nTexWidth, int nTexHeight,
                                 const Vec2& vxA, const Vec2& vxB, const Vec2& vxC, const Vec2& vxD,
                                 const Vec2& uvA, const Vec2& uvB, const Vec2& uvC, const Vec2& uvD)
{
	const float z = (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) ? 1.0f : 0.0f;
	CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 4, pScreenQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);

	pScreenQuad[0] = {
		Vec3(vxA.x, vxA.y, z), {
			{ 0 }
		},          uvA
	};
	pScreenQuad[1] = {
		Vec3(vxB.x, vxB.y, z), {
			{ 0 }
		},          uvB
	};
	pScreenQuad[2] = {
		Vec3(vxD.x, vxD.y, z), {
			{ 0 }
		},          uvD
	};
	pScreenQuad[3] = {
		Vec3(vxC.x, vxC.y, z), {
			{ 0 }
		},          uvC
	};

	gRenDev->m_RP.m_PersFlags2 &= ~(RBPF2_COMMIT_PF);

	CVertexBuffer strip(pScreenQuad, EDefaultInputLayouts::P3F_C4B_T2F);
	gRenDev->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::GetFullScreenTriWPOS(SVF_P3F_T2F_T3F pResult[3], int nTexWidth, int nTexHeight, float z, const RECT* pSrcRegion)
{
	UpdateFrustumCorners();

	if (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
		z = 1.0f - z;

	pResult[0].p = Vec3(-0.0f, -0.0f, z);
	pResult[0].st0 = Vec2(0, 0);
	pResult[0].st1 = m_vLT;
	pResult[1].p = Vec3(-0.0f, 2.0f, z);
	pResult[1].st0 = Vec2(0, 2);
	pResult[1].st1 = m_vLB * 2.0f - m_vLT;
	pResult[2].p = Vec3(2.0f, -0.0f, z);
	pResult[2].st0 = Vec2(2, 0);
	pResult[2].st1 = m_vRT * 2.0f - m_vLT;

	if (pSrcRegion)
	{
		const Vec4 vTexCoordsRegion(2.0f * float(pSrcRegion->left) / nTexWidth,
		                            2.0f * float(pSrcRegion->right) / nTexWidth,
		                            2.0f * float(pSrcRegion->top) / nTexHeight,
		                            2.0f * float(pSrcRegion->bottom) / nTexHeight);
		pResult[0].st0 = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.z);
		pResult[1].st0 = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.w);
		pResult[2].st0 = Vec2(vTexCoordsRegion.y, vTexCoordsRegion.z);
	}
}

void SPostEffectsUtils::GetFullScreenQuadWPOS(SVF_P3F_T2F_T3F pResult[4], int nTexWidth, int nTexHeight, float z, const RECT* pSrcRegion)
{
	UpdateFrustumCorners();

	if (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
		z = 1.0f - z;

	pResult[0].p = Vec3(-0.0f, -0.0f, z);
	pResult[0].st0 = Vec2(0, 0);
	pResult[0].st1 = m_vLT;
	pResult[1].p = Vec3(-0.0f, 1.0f, z);
	pResult[1].st0 = Vec2(0, 1);
	pResult[1].st1 = m_vLB;
	pResult[2].p = Vec3(1.0f, -0.0f, z);
	pResult[2].st0 = Vec2(1, 0);
	pResult[2].st1 = m_vRT;
	pResult[3].p = Vec3(1.0f, 1.0f, z);
	pResult[3].st0 = Vec2(1, 1);
	pResult[3].st1 = m_vRB;

	if (pSrcRegion)
	{
		const Vec4 vTexCoordsRegion(float(pSrcRegion->left) / nTexWidth,
		                            float(pSrcRegion->right) / nTexWidth,
		                            float(pSrcRegion->top) / nTexHeight,
		                            float(pSrcRegion->bottom) / nTexHeight);
		pResult[0].st0 = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.z);
		pResult[1].st0 = Vec2(vTexCoordsRegion.x, vTexCoordsRegion.w);
		pResult[2].st0 = Vec2(vTexCoordsRegion.y, vTexCoordsRegion.z);
		pResult[3].st0 = Vec2(vTexCoordsRegion.y, vTexCoordsRegion.w);
	}
}

void SPostEffectsUtils::DrawFullScreenTriWPOS(int nTexWidth, int nTexHeight, float z, const RECT* pSrcRegion)
{
	CryStackAllocWithSizeVector(SVF_P3F_T2F_T3F, 4, screenQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);
	GetFullScreenQuadWPOS(screenQuad, nTexWidth, nTexHeight, z, pSrcRegion);

	CVertexBuffer strip(screenQuad, EDefaultInputLayouts::P3F_T2F_T3F);
	gRenDev->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::SetTexture(CTexture* pTex, int nStage, int nFilter, ESamplerAddressMode eMode, bool bSRGBLookup, DWORD dwBorderColor)
{
	if (pTex)
	{
		SSamplerState TS;
		TS.SetFilterMode(nFilter);
		TS.SetClampMode(eMode, eMode, eMode);
		if (eMode == eSamplerAddressMode_Border)
			TS.SetBorderColor(dwBorderColor);
		TS.m_bSRGBLookup = bSRGBLookup;
		SamplerStateHandle nTexState = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(TS);
		pTex->Apply(nStage, nTexState);
	}
	else
	{
		CTexture::ApplyForID(nStage, 0, EDefaultSamplerStates::Unspecified, -1);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SPostEffectsUtils::GetOrCreateRenderTarget(const char* szTexName, CTexture*& pTex, int nWidth, int nHeight, const ColorF& cClear, bool bUseAlpha, bool bMipMaps, ETEX_Format eTF, int nCustomID, int nFlags)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "PostEffects CreateRenderTarget: %s", szTexName);

	// check if parameters are valid
	if (!nWidth || !nHeight)
	{
		return 0;
	}

	uint32 flags = nFlags & ~(FT_USAGE_DEPTHSTENCIL);
	flags |= FT_DONT_STREAM | FT_USAGE_RENDERTARGET | (bMipMaps ? FT_FORCE_MIPS : FT_NOMIPS);

	// if texture doesn't exist yet, create it
	if (!CTexture::IsTextureExist(pTex))
	{
		pTex = CTexture::GetOrCreateRenderTarget(szTexName, nWidth, nHeight, cClear, eTT_2D, flags, eTF, nCustomID);
		if (pTex && !(pTex->GetFlags() & FT_FAILED))
		{
			// Clear render target surface before using it
			pTex->Clear();
		}
	}
	else
	{
		pTex->SetFlags(flags);
		pTex->SetWidth(nWidth);
		pTex->SetHeight(nHeight);
		pTex->CreateRenderTarget(eTF, cClear);
	}

	return CTexture::IsTextureExist(pTex) ? 1 : 0;
}

bool SPostEffectsUtils::GetOrCreateDepthStencil(const char* szTexName, CTexture*& pTex, int nWidth, int nHeight, const ColorF& cClear, bool bUseAlpha, bool bMipMaps, ETEX_Format eTF, int nCustomID, int nFlags)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "PostEffects CreateRenderTarget: %s", szTexName);

	// check if parameters are valid
	if (!nWidth || !nHeight)
	{
		return 0;
	}

	uint32 flags = nFlags & ~(FT_USAGE_RENDERTARGET);
	flags |= FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL | (bMipMaps ? FT_FORCE_MIPS : FT_NOMIPS);

	// if texture doesn't exist yet, create it
	if (!CTexture::IsTextureExist(pTex))
	{
		pTex = CTexture::GetOrCreateDepthStencil(szTexName, nWidth, nHeight, cClear, eTT_2D, flags, eTF, nCustomID);
		if (pTex && !(pTex->GetFlags() & FT_FAILED))
		{
			// Clear render target surface before using it
			pTex->Clear();
		}
	}
	else
	{
		pTex->SetFlags(flags);
		pTex->SetWidth(nWidth);
		pTex->SetHeight(nHeight);
		pTex->CreateDepthStencil(eTF, cClear);
	}

	return CTexture::IsTextureExist(pTex) ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SPostEffectsUtils::ShBeginPass(CShader* pShader, const CCryNameTSCRC& TechName, uint32 nFlags)
{
	assert(pShader);

	m_pCurrShader = pShader;

	uint32 nPasses;
	m_pCurrShader->FXSetTechnique(TechName);
	m_pCurrShader->FXBegin(&nPasses, nFlags);
	return m_pCurrShader->FXBeginPass(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::ShEndPass()
{
	assert(m_pCurrShader);

	m_pCurrShader->FXEndPass();
	m_pCurrShader->FXEnd();

	m_pCurrShader = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::ShSetParamVS(const CCryNameR& pParamName, const Vec4& pParam)
{
	assert(m_pCurrShader);
	m_pCurrShader->FXSetVSFloat(pParamName, &pParam, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::ShSetParamPS(const CCryNameR& pParamName, const Vec4& pParam)
{
	assert(m_pCurrShader);
	m_pCurrShader->FXSetPSFloat(pParamName, &pParam, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::ClearScreen(float r, float g, float b, float a)
{
	static CCryNameTSCRC pTechName("ClearScreen");
	ShBeginPass(CShaderMan::s_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	Vec4 pClrScrParms = Vec4(r, g, b, a);
	static CCryNameR pParamName("clrScrParams");
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParamName, &pClrScrParms, 1);

	DrawFullScreenTri(iWidth, iHeight);

	ShEndPass();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::GetFrustumCorners(Vec3& vRT, Vec3& vLT, Vec3& vLB, Vec3& vRB, const CRenderCamera& rc, bool bMirrorCull)
{
	Vec3 vCoords[8];
	rc.CalcVerts(vCoords);

	// Swap order when mirrored culling enabled
	if (bMirrorCull)
	{
		vLT = vCoords[4] - vCoords[0];
		vRT = vCoords[5] - vCoords[1];
		vRB = vCoords[6] - vCoords[2];
		vLB = vCoords[7] - vCoords[3];
	}
	else
	{
		vRT = vCoords[4] - vCoords[0];
		vLT = vCoords[5] - vCoords[1];
		vLB = vCoords[6] - vCoords[2];
		vRB = vCoords[7] - vCoords[3];
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::UpdateFrustumCorners()
{
	int nFrameID = gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_nFrameID;
	const auto& rc = gRenDev->GetRCamera();

	if (m_nFrustrumFrameID != nFrameID || CRenderer::CV_r_StereoMode == 1 || !CompareRenderCamera(rc, m_cachedRenderCamera))
	{
		bool bMirrorCull = (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_MIRRORCULL) != 0;
		GetFrustumCorners(m_vRT, m_vLT, m_vLB, m_vRB, rc, bMirrorCull);

		m_nFrustrumFrameID = nFrameID;
		m_cachedRenderCamera.Copy(rc);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void SPostEffectsUtils::UpdateOverscanBorderAspectRatio()
{
	if (gRenDev)
	{
		const float screenWidth = (float)gRenDev->GetWidth();
		const float screenHeight = (float)gRenDev->GetHeight();
		Vec2 overscanBorders = Vec2(0.0f, 0.0f);
		gRenDev->EF_Query(EFQ_OverscanBorders, overscanBorders);

		const float aspectX = (screenWidth * (1.0f - (overscanBorders.y * 2.0f)));
		const float aspectY = (screenHeight * (1.0f - (overscanBorders.x * 2.0f)));
		m_fOverscanBorderAspectRatio = aspectX / max(aspectY, 0.001f);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

Matrix44& SPostEffectsUtils::GetColorMatrix()
{
	CPostEffectsMgr* pPostMgr = PostEffectMgr();
	int nFrameID = gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_nFrameID;
	if (m_nColorMatrixFrameID != nFrameID)
	{
		// Create color transformation matrices

		// Note: These parameters do not affect the result
		// float fBrightness = pPostMgr->GetByNameF("Global_Brightness");
		// float fContrast = pPostMgr->GetByNameF("Global_Contrast");
		// float fSaturation = pPostMgr->GetByNameF("Global_Saturation");
		// float fColorC = pPostMgr->GetByNameF("Global_ColorC");
		// float fColorM = pPostMgr->GetByNameF("Global_ColorM");
		// float fColorY = pPostMgr->GetByNameF("Global_ColorY");
		// float fColorK = pPostMgr->GetByNameF("Global_ColorK");
		// float fColorHue = pPostMgr->GetByNameF("Global_ColorHue");

		float fUserCyan = pPostMgr->GetByNameF("Global_User_ColorC");
		float fColorC = fUserCyan;

		float fUserMagenta = pPostMgr->GetByNameF("Global_User_ColorM");
		float fColorM = fUserMagenta;

		float fUserYellow = pPostMgr->GetByNameF("Global_User_ColorY");
		float fColorY = fUserYellow;

		float fUserLuminance = pPostMgr->GetByNameF("Global_User_ColorK");
		float fColorK = fUserLuminance;

		float fUserHue = pPostMgr->GetByNameF("Global_User_ColorHue");
		float fColorHue = fUserHue;

		float fUserBrightness = pPostMgr->GetByNameF("Global_User_Brightness");
		float fBrightness = fUserBrightness;

		float fUserContrast = pPostMgr->GetByNameF("Global_User_Contrast");
		float fContrast = fUserContrast;

		float fUserSaturation = pPostMgr->GetByNameF("Global_User_Saturation"); // translate to 0
		float fSaturation = fUserSaturation;

		// Saturation matrix
		Matrix44 pSaturationMat;

		{
			float y = 0.3086f, u = 0.6094f, v = 0.0820f, s = clamp_tpl<float>(fSaturation, -1.0f, 100.0f);

			float a = (1.0f - s) * y + s;
			float b = (1.0f - s) * y;
			float c = (1.0f - s) * y;
			float d = (1.0f - s) * u;
			float e = (1.0f - s) * u + s;
			float f = (1.0f - s) * u;
			float g = (1.0f - s) * v;
			float h = (1.0f - s) * v;
			float i = (1.0f - s) * v + s;

			pSaturationMat.SetIdentity();
			pSaturationMat.SetRow(0, Vec3(a, d, g));
			pSaturationMat.SetRow(1, Vec3(b, e, h));
			pSaturationMat.SetRow(2, Vec3(c, f, i));
		}

		// Create Brightness matrix
		Matrix44 pBrightMat;
		fBrightness = clamp_tpl<float>(fBrightness, 0.0f, 100.0f);

		pBrightMat.SetIdentity();
		pBrightMat.SetRow(0, Vec3(fBrightness, 0, 0));
		pBrightMat.SetRow(1, Vec3(0, fBrightness, 0));
		pBrightMat.SetRow(2, Vec3(0, 0, fBrightness));

		// Create Contrast matrix
		Matrix44 pContrastMat;
		{
			float c = clamp_tpl<float>(fContrast, -1.0f, 100.0f);

			pContrastMat.SetIdentity();
			pContrastMat.SetRow(0, Vec3(c, 0, 0));
			pContrastMat.SetRow(1, Vec3(0, c, 0));
			pContrastMat.SetRow(2, Vec3(0, 0, c));
			pContrastMat.SetColumn(3, 0.5f * Vec3(1.0f - c, 1.0f - c, 1.0f - c));
		}

		// Create CMKY matrix
		Matrix44 pCMKYMat;
		{
			Vec4 pCMYKParams = Vec4(fColorC + fColorK, fColorM + fColorK, fColorY + fColorK, 1.0f);

			pCMKYMat.SetIdentity();
			pCMKYMat.SetColumn(3, -Vec3(pCMYKParams.x, pCMYKParams.y, pCMYKParams.z));
		}

		// Create Hue rotation matrix
		Matrix44 pHueMat;
		{
			pHueMat.SetIdentity();

			const Vec3 pHueVec = Vec3(0.57735026f, 0.57735026f, 0.57735026f); // (normalized(1,1,1)
			pHueMat = Matrix34::CreateRotationAA(fColorHue * PI, pHueVec);
			pHueMat.SetColumn(3, Vec3(0, 0, 0));
		}

		// Compose final color matrix and set fragment program constants
		m_pColorMat = pSaturationMat * (pBrightMat * pContrastMat * pCMKYMat * pHueMat);

		m_nColorMatrixFrameID = nFrameID;
	}

	return m_pColorMat;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
