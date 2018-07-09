// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DHDRRender.cpp : high dynamic range post-processing

    Revision history:
* Created by Honich Andrey
* 01.10.09: Started refactoring (Tiago Sousa)

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include <Cry3DEngine/I3DEngine.h>
#include "../Common/ReverseDepth.h"
#include "GraphicsPipeline/Bloom.h"

//  render targets info - first gather list of hdr targets, sort by size and create after
struct SRenderTargetInfo
{
	SRenderTargetInfo() : nWidth(0), nHeight(0), cClearColor(Clr_Empty), Format(eTF_Unknown), nFlags(0), lplpStorage(0), nPitch(0), fPriority(0.0f), nCustomID(0)
	{};

	uint32      nWidth;
	uint32      nHeight;
	ColorF      cClearColor;
	ETEX_Format Format;
	uint32      nFlags;
	CTexture**  lplpStorage;
	char        szName[64];
	uint32      nPitch;
	float       fPriority;
	int32       nCustomID;
};

struct RenderTargetSizeSort
{
	bool operator()(const SRenderTargetInfo& drtStart, const SRenderTargetInfo& drtEnd) { return (drtStart.nPitch * drtStart.fPriority) > (drtEnd.nPitch * drtEnd.fPriority); }
};

class CHDRPostProcess
{

public:

	CHDRPostProcess()
	{
	}

	~CHDRPostProcess()
	{

	}

	void AddRenderTarget(uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Format Format, float fPriority, const char* szName, CTexture** pStorage, uint32 nFlags = 0, int32 nCustomID = -1, bool bDynamicTex = 0);
	bool CreateRenderTargetList();
	void ClearRenderTargetList()
	{
		m_pRenderTargets.clear();
	}

	static CHDRPostProcess* GetInstance()
	{
		static CHDRPostProcess pInstance;
		return &pInstance;
	}

private:

	std::vector<SRenderTargetInfo> m_pRenderTargets;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHDRPostProcess::AddRenderTarget(uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Format Format, float fPriority, const char* szName, CTexture** pStorage, uint32 nFlags, int32 nCustomID, bool bDynamicTex)
{
	SRenderTargetInfo drt;

	drt.nWidth = nWidth;
	drt.nHeight = nHeight;
	drt.cClearColor = cClear;
	drt.nFlags = FT_USAGE_RENDERTARGET | FT_DONT_STREAM | nFlags;
	drt.Format = Format;
	drt.fPriority = fPriority;
	drt.lplpStorage = pStorage;
	drt.nCustomID = nCustomID;
	drt.nPitch = nWidth * CTexture::BytesPerPixel(Format);

	cry_strcpy(drt.szName, szName);
	m_pRenderTargets.push_back(drt);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CHDRPostProcess::CreateRenderTargetList()
{
	std::sort(m_pRenderTargets.begin(), m_pRenderTargets.end(), RenderTargetSizeSort());

	for (uint32 t = 0; t < m_pRenderTargets.size(); ++t)
	{
		SRenderTargetInfo& drt = m_pRenderTargets[t];
		CTexture*& pTex = (*drt.lplpStorage);

		if (!pTex && !(pTex = CTexture::GetOrCreateRenderTarget(drt.szName, drt.nWidth, drt.nHeight, drt.cClearColor, eTT_2D, drt.nFlags, drt.Format, drt.nCustomID)))
			continue;

		pTex->SetFlags(drt.nFlags);
		pTex->SetWidth(drt.nWidth);
		pTex->SetHeight(drt.nHeight);
		pTex->CreateRenderTarget(drt.Format, drt.cClearColor);

		if (!(pTex->GetFlags() & FT_FAILED))
		{
			// Clear render target surface before using it
			pTex->Clear();
		}
	}

	m_pRenderTargets.clear();

	return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CTexture::GenerateHDRMaps()
{
	int i;
	char szName[256];

	CD3D9Renderer* r = gcpRendD3D;
	CHDRPostProcess* pHDRPostProcess = CHDRPostProcess::GetInstance();

	const int width  = r->GetWidth(),  width_r2  = (width  + 1) / 2, width_r4  = (width_r2  + 1) / 2, width_r8  = (width_r4  + 1) / 2, width_r16  = (width_r8  + 1) / 2;
	const int height = r->GetHeight(), height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2, height_r16 = (height_r8 + 1) / 2;
	
	pHDRPostProcess->ClearRenderTargetList();

	ETEX_Format nHDRFormatDefaultQuality = eTF_R16G16B16A16F;
	ETEX_Format nHDRFormatLowQuality     = eTF_R11G11B10F;
	
	// If Depth of Field feature enabled cannot use low quality HDR format.
	ETEX_Format nHDRFormat = ((CRenderer::CV_r_HDRTexFormat == 0 || CRenderer::CV_r_HDRTexFormat == 2) && CRenderer::CV_r_dof==0)  ? nHDRFormatLowQuality : nHDRFormatDefaultQuality;
	// low spec only - note: for main Render Target R11G11B10 precision/range (even with rescaling) not enough for darks vs good blooming quality
	ETEX_Format nHDRFormatMainTarget = (CRenderer::CV_r_HDRTexFormat == 2) ? nHDRFormatLowQuality : nHDRFormatDefaultQuality;

	uint32 nHDRTargetFlags = FT_DONT_RELEASE | (CRenderer::CV_r_msaa ? FT_USAGE_MSAA : 0);
	uint32 nHDRTargetFlagsUAV = nHDRTargetFlags | (CRenderer::CV_r_msaa ? 0 : FT_USAGE_UNORDERED_ACCESS);  // UAV required for tiled deferred shading
	pHDRPostProcess->AddRenderTarget(r->GetWidth(), r->GetHeight(), Clr_Unknown, nHDRFormatMainTarget, 1.0f, "$HDRTarget", &s_ptexHDRTarget, nHDRTargetFlagsUAV);
	pHDRPostProcess->AddRenderTarget(r->GetWidth(), r->GetHeight(), Clr_Unknown, eTF_R11G11B10F, 1.0f, "$HDRTargetPrev", &s_ptexHDRTargetPrev);

	// Scaled versions of the HDR scene texture
	pHDRPostProcess->AddRenderTarget(width_r2, height_r2, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaled0", &s_ptexHDRTargetScaled[0], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r2, height_r2, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTmp0", &s_ptexHDRTargetScaledTmp[0], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r2, height_r2, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTempRT0", &s_ptexHDRTargetScaledTempRT[0], FT_DONT_RELEASE);

	pHDRPostProcess->AddRenderTarget(width_r4, height_r4, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaled1", &s_ptexHDRTargetScaled[1], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r4, height_r4, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTmp1", &s_ptexHDRTargetScaledTmp[1], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r4, height_r4, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTempRT1", &s_ptexHDRTargetScaledTempRT[1], 0);

	pHDRPostProcess->AddRenderTarget(width_r4, height_r4, Clr_Unknown, eTF_R11G11B10F, 0.9f, "$HDRTempBloom0", &s_ptexHDRTempBloom[0], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r4, height_r4, Clr_Unknown, eTF_R11G11B10F, 0.9f, "$HDRTempBloom1", &s_ptexHDRTempBloom[1], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r4, height_r4, Clr_Unknown, eTF_R11G11B10F, 0.9f, "$HDRFinalBloom", &s_ptexHDRFinalBloom, FT_DONT_RELEASE);

	pHDRPostProcess->AddRenderTarget(width_r8, height_r8, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaled2", &s_ptexHDRTargetScaled[2], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r8, height_r8, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTempRT2", &s_ptexHDRTargetScaledTempRT[2], FT_DONT_RELEASE);

	pHDRPostProcess->AddRenderTarget(width_r16, height_r16, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaled3", &s_ptexHDRTargetScaled[3], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r16, height_r16, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTmp3", &s_ptexHDRTargetScaledTmp[3], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r16, height_r16, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTempRT3", &s_ptexHDRTargetScaledTempRT[3], FT_DONT_RELEASE);

	for (i = 0; i < 8; i++)
	{
		sprintf(szName, "$HDRAdaptedLuminanceCur_%d", i);
		pHDRPostProcess->AddRenderTarget(1, 1, Clr_Unknown, eTF_R16G16F, 0.1f, szName, &s_ptexHDRAdaptedLuminanceCur[i], FT_DONT_RELEASE);
	}

	pHDRPostProcess->AddRenderTarget(width, height, Clr_Unknown, eTF_R11G11B10F, 1.0f, "$SceneTargetR11G11B10F_0", &s_ptexSceneTargetR11G11B10F[0], nHDRTargetFlagsUAV);
	pHDRPostProcess->AddRenderTarget(width, height, Clr_Unknown, eTF_R11G11B10F, 1.0f, "$SceneTargetR11G11B10F_1", &s_ptexSceneTargetR11G11B10F[1], nHDRTargetFlags);

	pHDRPostProcess->AddRenderTarget(width, height, Clr_Unknown, eTF_R8G8B8A8, 0.1f, "$Velocity", &s_ptexVelocity, FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(20, height, Clr_Unknown, eTF_R8G8B8A8, 0.1f, "$VelocityTilesTmp0", &s_ptexVelocityTiles[0], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(20, 20, Clr_Unknown, eTF_R8G8B8A8, 0.1f, "$VelocityTilesTmp1", &s_ptexVelocityTiles[1], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(20, 20, Clr_Unknown, eTF_R8G8B8A8, 0.1f, "$VelocityTiles", &s_ptexVelocityTiles[2], FT_DONT_RELEASE);

	pHDRPostProcess->AddRenderTarget(width_r2, height_r2, Clr_Unknown, nHDRFormat, 0.9f, "$HDRDofLayerNear", &s_ptexHDRDofLayers[0], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r2, height_r2, Clr_Unknown, nHDRFormat, 0.9f, "$HDRDofLayerFar", &s_ptexHDRDofLayers[1], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r2, height_r2, Clr_Unknown, eTF_R16G16F, 1.0f, "$MinCoC_0_Temp", &s_ptexSceneCoCTemp, FT_DONT_RELEASE);
	for (i = 0; i < MIN_DOF_COC_K; i++)
	{
		cry_sprintf(szName, "$MinCoC_%d", i);
		pHDRPostProcess->AddRenderTarget(width_r2 / (i + 1), height_r2 / (i + 1), Clr_Unknown, eTF_R16G16F, 0.1f, szName, &s_ptexSceneCoC[i], FT_DONT_RELEASE, -1, true);
	}

	// TODO: make it a MIP-mapped resource and/or use compute
	// Luminance rt
	for (i = 0; i < NUM_HDR_TONEMAP_TEXTURES; i++)
	{
		int iSampleLen = 1 << (2 * i);
		cry_sprintf(szName, "$HDRToneMap_%d", i);

		pHDRPostProcess->AddRenderTarget(iSampleLen, iSampleLen, Clr_Dark, eTF_R16G16F, 0.7f, szName, &s_ptexHDRToneMaps[i], FT_DONT_RELEASE | (i == 0 ? FT_STAGE_READBACK : 0));
	}

	s_ptexHDRMeasuredLuminanceDummy = CTexture::GetOrCreateTextureObject("$HDRMeasuredLum_Dummy", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_R16G16F, TO_HDR_MEASURED_LUMINANCE);
	for (i = 0; i < MAX_GPU_NUM; ++i)
	{
		cry_sprintf(szName, "$HDRMeasuredLum_%d", i);
		s_ptexHDRMeasuredLuminance[i] = CTexture::GetOrCreate2DTexture(szName, 1, 1, 0, FT_DONT_RELEASE | FT_DONT_STREAM, NULL, eTF_R16G16F);
	}

	pHDRPostProcess->CreateRenderTargetList();

	r->m_vSceneLuminanceInfo = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	r->m_fAdaptedSceneScaleLBuffer = r->m_fAdaptedSceneScale = r->m_fScotopicSceneScale = 1.0f;

	// Create resources if necessary - todo: refactor all this shared render targets stuff, quite cumbersome atm...
	SPostEffectsUtils::Create();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CTexture::DestroyHDRMaps()
{
	int i;

	SAFE_RELEASE(s_ptexHDRTarget);
	SAFE_RELEASE(s_ptexHDRTargetPrev);
	SAFE_RELEASE(s_ptexHDRTargetScaled[0]);
	SAFE_RELEASE(s_ptexHDRTargetScaled[1]);
	SAFE_RELEASE(s_ptexHDRTargetScaled[2]);
	SAFE_RELEASE(s_ptexHDRTargetScaled[3]);

	SAFE_RELEASE(s_ptexHDRTargetScaledTmp[0]);
	SAFE_RELEASE(s_ptexHDRTargetScaledTempRT[0]);

	SAFE_RELEASE(s_ptexHDRTargetScaledTmp[1]);
	SAFE_RELEASE(s_ptexHDRTargetScaledTmp[3]);

	SAFE_RELEASE(s_ptexHDRTargetScaledTempRT[1]);
	SAFE_RELEASE(s_ptexHDRTargetScaledTempRT[2]);
	SAFE_RELEASE(s_ptexHDRTargetScaledTempRT[3]);

	SAFE_RELEASE(s_ptexHDRTempBloom[0]);
	SAFE_RELEASE(s_ptexHDRTempBloom[1]);
	SAFE_RELEASE(s_ptexHDRFinalBloom);

	for (i = 0; i < 8; i++)
	{
		SAFE_RELEASE(s_ptexHDRAdaptedLuminanceCur[i]);
	}

	for (i = 0; i < NUM_HDR_TONEMAP_TEXTURES; i++)
	{
		SAFE_RELEASE(s_ptexHDRToneMaps[i]);
	}
	SAFE_RELEASE(s_ptexHDRMeasuredLuminanceDummy);
	for (i = 0; i < MAX_GPU_NUM; ++i)
	{
		SAFE_RELEASE(s_ptexHDRMeasuredLuminance[i]);
	}

	CTexture::s_ptexCurLumTexture = NULL;

	SAFE_RELEASE(s_ptexVelocity);
	SAFE_RELEASE(s_ptexVelocityTiles[0]);
	SAFE_RELEASE(s_ptexVelocityTiles[1]);
	SAFE_RELEASE(s_ptexVelocityTiles[2]);

	SAFE_RELEASE(s_ptexHDRDofLayers[0]);
	SAFE_RELEASE(s_ptexHDRDofLayers[1]);
	SAFE_RELEASE(s_ptexSceneCoCTemp);
	for (i = 0; i < MIN_DOF_COC_K; i++)
	{
		SAFE_RELEASE(s_ptexSceneCoC[i]);
	}
}
