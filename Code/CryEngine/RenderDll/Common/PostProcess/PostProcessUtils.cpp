// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

ITimer* SPostEffectsUtils::m_pTimer;
int SPostEffectsUtils::m_iFrameCounter = 0;
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
int64 SPostEffectsUtils::m_nFrustrumFrameID = 0;
CCamera SPostEffectsUtils::m_cachedRenderCamera;

static bool CompareRenderCamera(const CCamera& lrc, const CCamera& rrc)
{
	return lrc.GetMatrix().IsEqual(rrc.GetMatrix());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void SPostEffectsUtils::GetFullScreenTri(SVF_P3F_C4B_T2F pResult[3], int nTexWidth, int nTexHeight, float z, const RECT* pSrcRegion)
{
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
	//OLD PIPELINE
	ASSERT_LEGACY_PIPELINE

	//CryStackAllocWithSizeVector(SVF_P3F_C4B_T2F, 4, screenQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);
	//GetFullScreenQuad(screenQuad, nTexWidth, nTexHeight, z, pSrcRegion);

	//CVertexBuffer strip(screenQuad, EDefaultInputLayouts::P3F_C4B_T2F);
	//gRenDev->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::DrawScreenQuad(int nTexWidth, int nTexHeight, float x0, float y0, float x1, float y1)
{
	//OLD PIPELINE
	ASSERT_LEGACY_PIPELINE

	const float z = 1.0f;
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

	//gRenDev->m_RP.m_PersFlags2 &= ~(RBPF2_COMMIT_PF);
	//CVertexBuffer strip(pScreenQuad, EDefaultInputLayouts::P3F_C4B_T2F);
	//gRenDev->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::DrawQuad(int nTexWidth, int nTexHeight,
                                 const Vec2& vxA, const Vec2& vxB, const Vec2& vxC, const Vec2& vxD,
                                 const Vec2& uvA, const Vec2& uvB, const Vec2& uvC, const Vec2& uvD)
{
	//OLD PIPELINE
	ASSERT_LEGACY_PIPELINE
	
	const float z = 1.0f;
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

	//gRenDev->m_RP.m_PersFlags2 &= ~(RBPF2_COMMIT_PF);
	//CVertexBuffer strip(pScreenQuad, EDefaultInputLayouts::P3F_C4B_T2F);
	//gRenDev->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::GetFullScreenTriWPOS(SVF_P3F_T2F_T3F pResult[3], int nTexWidth, int nTexHeight, float z, const RECT* pSrcRegion)
{
	UpdateFrustumCorners();

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
	//OLD PIPELINE
	ASSERT_LEGACY_PIPELINE

	//CryStackAllocWithSizeVector(SVF_P3F_T2F_T3F, 4, screenQuad, CDeviceBufferManager::AlignBufferSizeForStreaming);
	//GetFullScreenQuadWPOS(screenQuad, nTexWidth, nTexHeight, z, pSrcRegion);

	//CVertexBuffer strip(screenQuad, EDefaultInputLayouts::P3F_T2F_T3F);
	//gRenDev->DrawPrimitivesInternal(&strip, 4, eptTriangleStrip);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::SetTexture(CTexture* pTex, int nStage, int nFilter, ESamplerAddressMode eMode, bool bSRGBLookup, DWORD dwBorderColor)
{
	ASSERT_LEGACY_PIPELINE;
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
	ASSERT_LEGACY_PIPELINE
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::ShEndPass()
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::ShSetParamVS(const CCryNameR& pParamName, const Vec4& pParam)
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::ShSetParamPS(const CCryNameR& pParamName, const Vec4& pParam)
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::ClearScreen(float r, float g, float b, float a)
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SPostEffectsUtils::GetFrustumCorners(Vec3& vRT, Vec3& vLT, Vec3& vLB, Vec3& vRB, const CCamera& camera, bool bMirrorCull)
{
	Vec3 vCoords[8];
	camera.CalcAsymmetricFrustumVertices(vCoords);

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
	int64 nFrameID = gRenDev->GetRenderFrameID();
	const CCamera& camera = gcpRendD3D.GetGraphicsPipeline().GetCurrentRenderView()->GetCamera(CCamera::eEye_Left);

	if (m_nFrustrumFrameID != nFrameID || gcpRendD3D->GetS3DRend().GetStereoMode() == EStereoMode::STEREO_MODE_DUAL_RENDERING || !CompareRenderCamera(camera, m_cachedRenderCamera))
	{
		bool bMirrorCull = gcpRendD3D.GetGraphicsPipeline().GetCurrentRenderView()->IsViewFlag(SRenderViewInfo::eFlags_MirrorCull);
		GetFrustumCorners(m_vRT, m_vLT, m_vLB, m_vRB, camera, bMirrorCull);

		m_nFrustrumFrameID = nFrameID;
		m_cachedRenderCamera = camera;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void SPostEffectsUtils::UpdateOverscanBorderAspectRatio()
{
	if (gRenDev)
	{
		const float screenWidth  = (float)CRendererResources::s_renderWidth;
		const float screenHeight = (float)CRendererResources::s_renderHeight;
		Vec2 overscanBorders = Vec2(0.0f, 0.0f);
		gRenDev->EF_Query(EFQ_OverscanBorders, overscanBorders);

		const float aspectX = (screenWidth  * (1.0f - (overscanBorders.y * 2.0f)));
		const float aspectY = (screenHeight * (1.0f - (overscanBorders.x * 2.0f)));
		m_fOverscanBorderAspectRatio = aspectX / max(aspectY, 0.001f);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

Matrix44& SPostEffectsUtils::GetColorMatrix()
{
	CPostEffectsMgr* pPostMgr = PostEffectMgr();
	int nFrameID = gRenDev->GetRenderFrameID();
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
