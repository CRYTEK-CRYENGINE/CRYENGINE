// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"
#include "D3DVolumetricClouds.h"

#include "Common/RenderView.h"
#include "GraphicsPipeline/StandardGraphicsPipeline.h"
#include "GraphicsPipeline/ShadowMap.h"

#pragma warning(disable: 4244)

#if CRY_PLATFORM_DURANGO
	#pragma warning(push)
	#pragma warning(disable : 4273)
BOOL InflateRect(LPRECT lprc, int dx, int dy);
	#pragma warning(pop)
#endif

bool CD3D9Renderer::FX_DeferredShadowPassSetupBlend(const Matrix44& mShadowTexGen, int nFrustumNum, float maskRTWidth, float maskRTHeight)
{
	//set ScreenToWorld Expansion Basis
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
	CShadowUtils::ProjectScreenToWorldExpansionBasis(mShadowTexGen, GetCamera(), m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, true, &m_RenderTileInfo);

	Matrix44A* mat = &gRenDev->m_TempMatrices[nFrustumNum][2];
	mat->SetRow4(0, vWBasisX);
	mat->SetRow4(1, vWBasisY);
	mat->SetRow4(2, vWBasisZ);
	mat->SetRow4(3, vCamPos);

	return true;
}

bool CD3D9Renderer::FX_DeferredShadowPassSetup(const Matrix44& mShadowTexGen, ShadowMapFrustum* pShadowFrustum, float maskRTWidth, float maskRTHeight, Matrix44& mScreenToShadow, bool bNearest)
{
	//set ScreenToWorld Expansion Basis
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
	bool bVPosSM30 = (GetFeatures() & (RFT_HW_SM30 | RFT_HW_SM40)) != 0;

	CCamera Cam = GetCamera();
	if (bNearest && m_drawNearFov > 1.0f && m_drawNearFov < 179.0f)
		Cam.SetFrustum(Cam.GetViewSurfaceX(), Cam.GetViewSurfaceZ(), DEG2RAD(m_drawNearFov), Cam.GetNearPlane(), Cam.GetFarPlane(), Cam.GetPixelAspectRatio());

	CShadowUtils::ProjectScreenToWorldExpansionBasis(mShadowTexGen, Cam, m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, bVPosSM30, &m_RenderTileInfo);

	//TOFIX: create PB components for these params
	//creating common projection matrix for depth reconstruction

	mScreenToShadow = Matrix44(vWBasisX.x, vWBasisX.y, vWBasisX.z, vWBasisX.w,
	                           vWBasisY.x, vWBasisY.y, vWBasisY.z, vWBasisY.w,
	                           vWBasisZ.x, vWBasisZ.y, vWBasisZ.z, vWBasisZ.w,
	                           vCamPos.x, vCamPos.y, vCamPos.z, vCamPos.w);

	//save magnitudes separately to inrease precision
	m_cEF.m_TempVecs[14].x = vWBasisX.GetLength();
	m_cEF.m_TempVecs[14].y = vWBasisY.GetLength();
	m_cEF.m_TempVecs[14].z = vWBasisZ.GetLength();
	m_cEF.m_TempVecs[14].w = 1.0f;

	//Vec4r normalization in doubles
	vWBasisX /= vWBasisX.GetLength();
	vWBasisY /= vWBasisY.GetLength();
	vWBasisZ /= vWBasisZ.GetLength();

	m_cEF.m_TempVecs[10].x = vWBasisX.x;
	m_cEF.m_TempVecs[10].y = vWBasisX.y;
	m_cEF.m_TempVecs[10].z = vWBasisX.z;
	m_cEF.m_TempVecs[10].w = vWBasisX.w;

	m_cEF.m_TempVecs[11].x = vWBasisY.x;
	m_cEF.m_TempVecs[11].y = vWBasisY.y;
	m_cEF.m_TempVecs[11].z = vWBasisY.z;
	m_cEF.m_TempVecs[11].w = vWBasisY.w;

	m_cEF.m_TempVecs[12].x = vWBasisZ.x;
	m_cEF.m_TempVecs[12].y = vWBasisZ.y;
	m_cEF.m_TempVecs[12].z = vWBasisZ.z;
	m_cEF.m_TempVecs[12].w = vWBasisZ.w;

	m_cEF.m_TempVecs[13].x = CV_r_ShadowsAdaptionRangeClamp;
	m_cEF.m_TempVecs[13].y = CV_r_ShadowsAdaptionSize * 250.f;//to prevent akwardy high number in cvar
	m_cEF.m_TempVecs[13].z = CV_r_ShadowsAdaptionMin;

	// Particles shadow constants
	if (m_RP.m_nPassGroupID == EFSLIST_TRANSP || m_RP.m_nPassGroupID == EFSLIST_HALFRES_PARTICLES)
	{
		m_cEF.m_TempVecs[13].x = CV_r_ShadowsParticleKernelSize;
		m_cEF.m_TempVecs[13].y = CV_r_ShadowsParticleJitterAmount;
		m_cEF.m_TempVecs[13].z = CV_r_ShadowsParticleAnimJitterAmount * 0.05f;
		m_cEF.m_TempVecs[13].w = CV_r_ShadowsParticleNormalEffect;
	}

	m_cEF.m_TempVecs[0].x = vCamPos.x;
	m_cEF.m_TempVecs[0].y = vCamPos.y;
	m_cEF.m_TempVecs[0].z = vCamPos.z;
	m_cEF.m_TempVecs[0].w = vCamPos.w;

	return true;
}

HRESULT GetSampleOffsetsGaussBlur5x5Bilinear(DWORD dwD3DTexWidth, DWORD dwD3DTexHeight, Vec4* avTexCoordOffset, Vec4* avSampleWeight, FLOAT fMultiplier)
{
	float tu = 1.0f / (float)dwD3DTexWidth;
	float tv = 1.0f / (float)dwD3DTexHeight;
	float totalWeight = 0.0f;
	Vec4 vWhite(1.f, 1.f, 1.f, 1.f);
	float fWeights[6];

	int index = 0;
	for (int x = -2; x <= 2; x++, index++)
	{
		fWeights[index] = PostProcessUtils().GaussianDistribution2D((float)x, 0.f, 4);
	}

	//  compute weights for the 2x2 taps.  only 9 bilinear taps are required to sample the entire area.
	index = 0;
	for (int y = -2; y <= 2; y += 2)
	{
		float tScale = (y == 2) ? fWeights[4] : (fWeights[y + 2] + fWeights[y + 3]);
		float tFrac = fWeights[y + 2] / tScale;
		float tOfs = ((float)y + (1.f - tFrac)) * tv;
		for (int x = -2; x <= 2; x += 2, index++)
		{
			float sScale = (x == 2) ? fWeights[4] : (fWeights[x + 2] + fWeights[x + 3]);
			float sFrac = fWeights[x + 2] / sScale;
			float sOfs = ((float)x + (1.f - sFrac)) * tu;
			avTexCoordOffset[index] = Vec4(sOfs, tOfs, 0, 1);
			avSampleWeight[index] = vWhite * sScale * tScale;
			totalWeight += sScale * tScale;
		}
	}

	for (int i = 0; i < index; i++)
	{
		avSampleWeight[i] *= (fMultiplier / totalWeight);
	}

	return S_OK;
}

int CRenderer::FX_ApplyShadowQuality()
{
	SShaderProfile* pSP = &m_cEF.m_ShaderProfiles[eST_Shadow];
	const uint64 quality = g_HWSR_MaskBit[HWSR_QUALITY];
	const uint64 quality1 = g_HWSR_MaskBit[HWSR_QUALITY1];
	m_RP.m_FlagsShader_RT &= ~(quality | quality1);

	int nQuality = (int)pSP->GetShaderQuality();
	m_RP.m_nShaderQuality = nQuality;
	switch (nQuality)
	{
	case eSQ_Medium:
		m_RP.m_FlagsShader_RT |= quality;
		break;
	case eSQ_High:
		m_RP.m_FlagsShader_RT |= quality1;
		break;
	case eSQ_VeryHigh:
		m_RP.m_FlagsShader_RT |= quality;
		m_RP.m_FlagsShader_RT |= quality1;
		break;
	}
	return nQuality;
}

void CD3D9Renderer::FX_StateRestore(int prevState)
{
}

void CD3D9Renderer::FX_ShadowBlur(float fShadowBluriness, SDynTexture* tpSrcTemp, CTexture* tpDst, int iShadowMode, bool bScreenVP, CTexture* tpDst2, CTexture* tpSrc2)
{
	if (m_LogFile)
		Logv("   Blur shadow map...\n");

	gRenDev->m_cEF.mfRefreshSystemShader("ShadowBlur", CShaderMan::s_ShaderShadowBlur);

	assert(tpDst);
	PREFAST_ASSUME(tpDst);

	if (iShadowMode != 4)
	{
		assert(tpSrcTemp);
		PREFAST_ASSUME(tpSrcTemp);
	}

	uint32 nP;
	m_RP.m_FlagsStreams_Decl = 0;
	m_RP.m_FlagsStreams_Stream = 0;
	m_RP.m_FlagsPerFlush = 0;
	m_RP.m_pCurObject = m_RP.m_pIdendityRenderObject;
	m_RP.m_pPrevObject = NULL;
	EF_Scissor(false, 0, 0, 0, 0);
	D3DSetCull(eCULL_None);
	int nSizeX = tpDst->GetWidth();
	int nSizeY = tpDst->GetHeight();
	bool bCreateBlured = true;
	uint64 nMaskRT = m_RP.m_FlagsShader_RT;

	STexState sTexState = STexState(FILTER_LINEAR, true);

	if (tpDst && tpSrcTemp)
		fShadowBluriness *= (tpDst->GetWidth() / tpSrcTemp->GetWidth());

	float fVertDepth = 0.f;

	Set2DMode(true, 1, 1);

	// setup screen aligned quad
	SVF_P3F_C4B_T2F pScreenBlur[] =
	{
		{ Vec3(0, 0, fVertDepth), {
						{ ~0U }
			    }, Vec2(0, 0) },
		{ Vec3(0, 1, fVertDepth), {
						{ ~0U }
			    }, Vec2(0, m_RP.m_CurDownscaleFactor.y) },
		{ Vec3(1, 0, fVertDepth), {
						{ ~0U }
			    }, Vec2(m_RP.m_CurDownscaleFactor.x, 0) },
		{ Vec3(1, 1, fVertDepth), {
						{ ~0U }
			    }, Vec2(m_RP.m_CurDownscaleFactor.x, m_RP.m_CurDownscaleFactor.y) },
	};

	CShader* pSH = m_cEF.s_ShaderShadowBlur;
	if (!pSH)
	{
		Set2DMode(false, 1, 1);
		return;
	}

	if (iShadowMode < 0)
	{
		iShadowMode = CV_r_shadowblur;

		if (CV_r_shadowblur == 3)
		{
			if (m_RP.m_nPassGroupID == EFSLIST_TRANSP || !CTexture::s_ptexZTarget)
				iShadowMode = 2;  // blur transparent object in the standard way
		}
	}

	//////////////////////////////////////////////////////////////////////////
	if (iShadowMode == 9)
	{
	}
	else if (iShadowMode == 8)
	{
		uint32 nPasses = 0;
		static CCryNameTSCRC TechName("ShadowBlurGen");
		pSH->FXSetTechnique(TechName);
		pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		tpSrcTemp->SetRT(0, true, &m_DepthBufferOrig, bScreenVP);
		bool bSwap = false;

		int m_GenerateSATRDSamples = 4;
		for (int iPass = 1; iPass < nSizeX; iPass *= m_GenerateSATRDSamples)
		{
			int nDone = iPass / m_GenerateSATRDSamples;

			EF_Scissor(true, nDone, 0, nSizeX, nSizeY);

			STexState sPointFilterState = STexState(FILTER_POINT, true);
			if (bSwap)
			{
				FX_SetRenderTarget(0, tpDst, &m_DepthBufferOrig, false, -1, bScreenVP);
				tpSrcTemp->m_pTexture->Apply(0, CTexture::GetTexState(sPointFilterState));
			}
			else
			{
				FX_SetRenderTarget(0, tpSrcTemp->m_pTexture, &m_DepthBufferOrig, false, -1, bScreenVP);
				tpDst->Apply(0, CTexture::GetTexState(sPointFilterState));
			}
			bSwap = !bSwap;

			FX_SetState(GS_NODEPTHTEST);

			pSH->FXBeginPass(0);
			Vec4 v;
			v[0] = 0;
			v[1] = 0;
			v[2] = 0;
			v[3] = 0;
			static CCryNameR Param1Name("PixelOffset");
			pSH->FXSetVSFloat(Param1Name, &v, 1);

			//setup pass offset
			Vec4 PassOffset;

			PassOffset[0] = float(iPass) / float(nSizeX);
			PassOffset[1] = 0;
			PassOffset[2] = 0;
			PassOffset[3] = 0;

			static CCryNameR ParamName("BlurOffset");
			pSH->FXSetPSFloat(ParamName, &PassOffset, 1);

			// Draw a fullscreen quad to sample the RT
			{
				CVertexBuffer pVertexBuffer(pScreenBlur, eVF_P3F_C4B_T2F);
				//EF_Commit() is called here
				DrawPrimitivesInternal(&pVertexBuffer, 4, eptTriangleStrip);
			}
			pSH->FXEndPass();

		}
		EF_Scissor(false, 0, 0, 0, 0);
		SetTexture(0);
		pSH->FXEnd();
		FX_PopRenderTarget(0);
		FX_Commit();
	}
	//////////////////////////////////////////////////////////////////////////
	else if (iShadowMode == 3 && CTexture::s_ptexZTarget)   // with depth lookup to avoid shadow leaking - s_ptexZTarget might be 0 in wireframe mode
	{
		CTexture* tpDepthSrc = CTexture::s_ptexZTarget;

		tpDepthSrc->Apply(1, CTexture::GetTexState(sTexState));

		uint32 nPasses = 0;

		FX_SetState(GS_NODEPTHTEST);

		FX_PushRenderTarget(0, tpDst, NULL, -1, bScreenVP);
		tpSrcTemp->Apply(0, CTexture::GetTexState(sTexState));

		static CCryNameTSCRC TechName("ShadowBlurScreenOpaque");
		pSH->FXSetTechnique(TechName);
		pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		for (nP = 0; nP < nPasses; nP++)
		{
			pSH->FXBeginPass(nP);

			float sW[9] = { 0.2813f, 0.2137f, 0.1185f, 0.0821f, 0.0461f, 0.0262f, 0.0162f, 0.0102f, 0.0052f };

			Vec4 v;
			v[0] = 0;
			v[1] = 0;
			v[2] = 0;
			v[3] = 0;
			static CCryNameR Param1Name("PixelOffset");
			pSH->FXSetVSFloat(Param1Name, &v, 1);

			// X Blur
			v[0] = 1.0f / (float)nSizeX * fShadowBluriness;
			v[1] = 1.0f / (float)nSizeY * fShadowBluriness;
			static CCryNameR Param2Name("BlurOffset");
			pSH->FXSetPSFloat(Param2Name, &v, 1);

			Vec4 vWeight[9];
			for (uint32 i = 0; i < 9; i++)
			{
				vWeight[i].x = sW[i];
				vWeight[i].y = sW[i];
				vWeight[i].z = sW[i];
				vWeight[i].w = sW[i];
			}

			// Draw a fullscreen quad to sample the RT
			CVertexBuffer pVertexBuffer(pScreenBlur, eVF_P3F_C4B_T2F);
			DrawPrimitivesInternal(&pVertexBuffer, 4, eptTriangleStrip);

			pSH->FXEndPass();
		}
		SetTexture(0);
		pSH->FXEnd();
		FX_PopRenderTarget(0);
	}
	else if (iShadowMode == 1)
	{
		tpDst->Apply(0, CTexture::GetTexState(sTexState));
		tpSrcTemp->SetRT(0, true, &m_DepthBufferOrig, bScreenVP);
		uint32 nPasses = 0;
		static CCryNameTSCRC TechName("ShadowBlurScreen");
		pSH->FXSetTechnique(TechName);
		pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		FX_SetState(GS_NODEPTHTEST);

		for (nP = 0; nP < nPasses; nP++)
		{
			pSH->FXBeginPass(nP);

			float sW[9] = { 0.2813f, 0.2137f, 0.1185f, 0.0821f, 0.0461f, 0.0262f, 0.0162f, 0.0102f, 0.0052f };

			Vec4 v;
			v[0] = 0;
			v[1] = 0;
			v[2] = 0;
			v[3] = 0;
			static CCryNameR Param1Name("PixelOffset");
			pSH->FXSetVSFloat(Param1Name, &v, 1);

			// X Blur
			v[0] = 1.0f / (float)nSizeX * fShadowBluriness * 2.f;
			v[1] = 0;
			static CCryNameR Param2Name("BlurOffset");
			pSH->FXSetPSFloat(Param2Name, &v, 1);

			Vec4 vWeight[9];
			float fSumm = 0;
			for (uint32 i = 0; i < 9; i++)
				fSumm += sW[i];

			for (uint32 i = 0; i < 9; i++)
			{
				vWeight[i].x = sW[i] / fSumm;
				vWeight[i].y = sW[i] / fSumm;
				vWeight[i].z = sW[i] / fSumm;
				vWeight[i].w = sW[i] / fSumm;
			}
			static CCryNameR Param3Name("SampleWeights");
			pSH->FXSetPSFloat(Param3Name, vWeight, 9);

			// Draw a fullscreen quad to sample the RT
			{
				CVertexBuffer pVertexBuffer(pScreenBlur, eVF_P3F_C4B_T2F);
				DrawPrimitivesInternal(&pVertexBuffer, 4, eptTriangleStrip);
			}

			FX_SetRenderTarget(0, tpDst, &m_DepthBufferOrig, false, -1, bScreenVP);

			// Y Blur
			v[0] = 0;
			v[1] = 1.0f / (float)nSizeY * fShadowBluriness * 2.f;
			pSH->FXSetPSFloat(Param2Name, &v, 1);

			tpSrcTemp->m_pTexture->Apply(0, CTexture::GetTexState(sTexState));

			// Draw a fullscreen quad to sample the RT
			{
				CVertexBuffer pVertexBuffer(pScreenBlur, eVF_P3F_C4B_T2F);
				DrawPrimitivesInternal(&pVertexBuffer, 4, eptTriangleStrip);
			}
			pSH->FXEndPass();
		}
		SetTexture(0);
		pSH->FXEnd();
		FX_PopRenderTarget(0);
		FX_Commit();
	}
	else if (iShadowMode == 0 || iShadowMode == 2)
	{
		FX_PushRenderTarget(0, tpDst, NULL, -1, bScreenVP);
		tpSrcTemp->Apply(0, CTexture::GetTexState(sTexState));

		uint32 nPasses = 0;
		static CCryNameTSCRC TechName("ShadowGaussBlur5x5");
		pSH->FXSetTechnique(TechName);
		pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		Vec4 avSampleOffsets[10];
		Vec4 avSampleWeights[10];

		FX_SetState(GS_NODEPTHTEST);

		RECT rectSrc;
		PostProcessUtils().GetTextureRect(tpSrcTemp->m_pTexture, &rectSrc);
		InflateRect(&rectSrc, -1, -1);

		RECT rectDest;
		PostProcessUtils().GetTextureRect(tpDst, &rectDest);
		InflateRect(&rectDest, -1, -1);

		CoordRect coords;
		GetTextureCoords(tpSrcTemp->m_pTexture, &rectSrc, tpDst, &rectDest, &coords);

		for (nP = 0; nP < nPasses; nP++)
		{
			pSH->FXBeginPass(nP);

			Vec4 v;
			v[0] = 0;
			v[1] = 0;
			v[2] = 0;
			v[3] = 0;
			static CCryNameR Param1Name("PixelOffset");
			pSH->FXSetVSFloat(Param1Name, &v, 1);

			uint32 n = 9;
			float fBluriness = CLAMP(fShadowBluriness, 0.01f, 16.0f);
			HRESULT hr = GetSampleOffsetsGaussBlur5x5Bilinear((int)(nSizeX / fBluriness), (int)(nSizeY / fBluriness), avSampleOffsets, avSampleWeights, 1.0f);
			static CCryNameR Param2Name("SampleOffsets");
			static CCryNameR Param3Name("SampleWeights");
			pSH->FXSetPSFloat(Param2Name, avSampleOffsets, n);
			pSH->FXSetPSFloat(Param3Name, avSampleWeights, n);

			// Draw a fullscreen quad to sample the RT
			::DrawFullScreenQuad(coords);

			pSH->FXEndPass();
		}
		pSH->FXEnd();
		FX_PopRenderTarget(0);
	}
	Set2DMode(false, 1, 1);
	m_RP.m_FlagsShader_RT = nMaskRT;

	if (m_LogFile)
		Logv("   End bluring of shadow map...\n");
}

void CD3D9Renderer::FX_StencilTestCurRef(bool bEnable, bool bNoStencilClear, bool bStFuncEqual)
{
	if (bEnable)
	{
		int nStencilState =
		  STENC_FUNC(bStFuncEqual ? FSS_STENCFUNC_EQUAL : FSS_STENCFUNC_NOTEQUAL) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
		  STENCOP_PASS(FSS_STENCOP_KEEP);

		FX_SetStencilState(nStencilState, m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFFFFFF);
		FX_SetState(m_RP.m_CurState | GS_STENCIL);
	}
	else
	{
	}
}

void CD3D9Renderer::FX_DeferredShadowPass(const SRenderLight* pLight, ShadowMapFrustum* pShadowFrustum, bool bShadowPass, bool bCloudShadowPass, bool bStencilPrepass, int nLod)
{
	uint32 nPassCount = 0;
	CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;
	static CCryNameTSCRC DeferredShadowTechName = "DeferredShadowPass";

	D3DSetCull(eCULL_Back, true); //fs quads should not revert test..

	if (pShadowFrustum->m_eFrustumType != ShadowMapFrustum::e_Nearest && !bCloudShadowPass)
	{
		if (pShadowFrustum->bUseShadowsPool || pShadowFrustum->pDepthTex == NULL)
			return;
	}

	if (pShadowFrustum->m_eFrustumType == ShadowMapFrustum::e_HeightMapAO)
		return;

	int nShadowQuality = FX_ApplyShadowQuality();

	//////////////////////////////////////////////////////////////////////////
	// set global shader RT flags
	//////////////////////////////////////////////////////////////////////////

	// set pass dependent RT flags
	m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE4] |
	                           g_HWSR_MaskBit[HWSR_CUBEMAP0] | g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE] | g_HWSR_MaskBit[HWSR_POINT_LIGHT] |
	                           g_HWSR_MaskBit[HWSR_SHADOW_MIXED_MAP_G16R16] | g_HWSR_MaskBit[HWSR_SHADOW_JITTERING] | g_HWSR_MaskBit[HWSR_NEAREST]);

	if (!pShadowFrustum->bBlendFrustum)
		m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE3];

	if (m_shadowJittering > 0.0f)
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SHADOW_JITTERING];

	//enable depth precision shift for sun's FP shadow RTs
	if (!(pShadowFrustum->bNormalizedDepth) && !(pShadowFrustum->bHWPCFCompare))
	{
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SHADOW_MIXED_MAP_G16R16];
	}

	//enable hw-pcf per frustum
	if (pShadowFrustum->bHWPCFCompare)
	{
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE];
	}

	if (pShadowFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest)
	{
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_NEAREST];
	}

	if (bCloudShadowPass || (CV_r_ShadowsScreenSpace && bShadowPass && pShadowFrustum->nShadowMapLod == 0))
	{
		if (bCloudShadowPass && GetVolumetricCloud().IsEnabledVolumetricCloudsShadow())
		{
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3];
		}
		else
		{
			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
		}
	}

	ConfigShadowTexgen(0, pShadowFrustum);
	if (nShadowQuality == eSQ_VeryHigh) //DX10 only
	{
		ConfigShadowTexgen(1, pShadowFrustum, -1, false, false);
		m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
	}

	int newState = m_RP.m_CurState;
	newState &= ~(GS_DEPTHWRITE | GS_BLSRC_ONE | GS_BLDST_ONE | GS_BLOP_MAX);
	newState |= GS_NODEPTHTEST;

	if (pShadowFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest)
	{
		newState &= ~(GS_NODEPTHTEST | GS_DEPTHFUNC_MASK);
		newState |= GS_DEPTHFUNC_GREAT;
	}

	if (pShadowFrustum->bUseAdditiveBlending)
	{
		newState |= GS_BLSRC_ONE | GS_BLDST_ONE | GS_BLOP_MAX;
	}
	else if (bShadowPass && pShadowFrustum->bBlendFrustum)
	{
		newState |= GS_BLSRC_ONE | GS_BLDST_ONE;
	}

	pShader->FXSetTechnique(DeferredShadowTechName);
	pShader->FXBegin(&nPassCount, FEF_DONTSETSTATES);

	//////////////////////////////////////////////////////////////////////////
	//Stencil cull pre-pass for GSM
	//////////////////////////////////////////////////////////////////////////
	if (bStencilPrepass)
	{
		newState |= GS_STENCIL;
		//Disable color writes
		newState |= GS_COLMASK_NONE;

		FX_SetState(newState);
		//////////////////////////////////////////////////////////////////////////
		if (!CV_r_ShadowsUseClipVolume)
		{
			FX_SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
			  STENCOP_PASS(FSS_STENCOP_REPLACE),
			  nLod, 0x7F, 0x7F
			  );
			pShader->FXBeginPass(DS_STENCIL_PASS);
			if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_T2F_T3F)))
			{
				FX_Commit();
				FX_DrawPrimitive(eptTriangleStrip, 0, 4);
			}
		}
		else
		{
			//render clip volume
			Matrix44 mViewProj = pShadowFrustum->mLightViewMatrix;
			Matrix44 mViewProjInv = mViewProj.GetInverted();
			gRenDev->m_TempMatrices[0][0] = mViewProjInv.GetTransposed();

			pShader->FXBeginPass(DS_STENCIL_VOLUME_CLIP);

			if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
			{
				FX_SetVStream(0, m_pUnitFrustumVB[SHAPE_SIMPLE_PROJECTOR], 0, sizeof(SVF_P3F_C4B_T2F));
				FX_SetIStream(m_pUnitFrustumIB[SHAPE_SIMPLE_PROJECTOR], 0, (kUnitObjectIndexSizeof == 2 ? Index16 : Index32));
				FX_StencilCullPass(nLod, m_UnitFrustVBSize[SHAPE_SIMPLE_PROJECTOR], m_UnitFrustIBSize[SHAPE_SIMPLE_PROJECTOR]);

				// camera might be outside cached frustum => do front facing pass as well
				if (pShadowFrustum->IsCached())
				{
					Vec4 vCamPosShadowSpace = Vec4(GetRCamera().vOrigin, 1.f) * mViewProj;
					vCamPosShadowSpace /= vCamPosShadowSpace.w;

					if (abs(vCamPosShadowSpace.x) > 1.0f || abs(vCamPosShadowSpace.y) > 1.0f || vCamPosShadowSpace.z < 0 || vCamPosShadowSpace.z > 1)
					{
						D3DSetCull(eCULL_Back);
						FX_SetStencilState(
						  STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
						  STENCOP_FAIL(FSS_STENCOP_KEEP) |
						  STENCOP_ZFAIL(FSS_STENCOP_ZERO) |
						  STENCOP_PASS(FSS_STENCOP_KEEP),
						  nLod, 0xFFFFFFFF, 0xFFFF
						  );

						FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, m_UnitFrustVBSize[SHAPE_SIMPLE_PROJECTOR], 0, m_UnitFrustIBSize[SHAPE_SIMPLE_PROJECTOR]);
					}
				}
			}
		}

		pShader->FXEndPass();
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Shadow Pass
	//////////////////////////////////////////////////////////////////////////

	if (bShadowPass)
	{
		newState &= ~(GS_COLMASK_NONE | GS_STENCIL);
		newState |= GS_NOCOLMASK_G | GS_NOCOLMASK_B | GS_NOCOLMASK_A;

		if (nLod != 0 && !bCloudShadowPass)
		{
			newState |= GS_STENCIL;

			FX_SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
			  STENCOP_PASS(FSS_STENCOP_KEEP),
			  nLod, 0xFFFFFFFF, 0xFFFFFFFF
			  );
		}

		FX_SetState(newState);

		pShader->FXBeginPass(bCloudShadowPass ? DS_CLOUDS_SEPARATE : DS_SHADOW_PASS);

		const float fCustomZ = pShadowFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest ? CV_r_DrawNearZRange - 0.001f : 0.f;
		PostProcessUtils().DrawFullScreenTriWPOS(0, 0, fCustomZ);

		pShader->FXEndPass();

	}
	pShader->FXEnd();
}

bool CD3D9Renderer::CreateAuxiliaryMeshes()
{
	t_arrDeferredMeshIndBuff arrDeferredInds;
	t_arrDeferredMeshVertBuff arrDeferredVerts;

	uint nProjectorMeshStep = 10;

	//projector frustum mesh
	for (int i = 0; i < 3; i++)
	{
		uint nFrustTess = 11 + nProjectorMeshStep * i;
		CDeferredRenderUtils::CreateUnitFrustumMesh(nFrustTess, nFrustTess, arrDeferredInds, arrDeferredVerts);
		SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_PROJECTOR + i]);
		SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_PROJECTOR + i]);
		COMPILE_TIME_ASSERT(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]));
		CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_PROJECTOR + i], m_pUnitFrustumVB[SHAPE_PROJECTOR + i]);
		m_UnitFrustVBSize[SHAPE_PROJECTOR + i] = arrDeferredVerts.size();
		m_UnitFrustIBSize[SHAPE_PROJECTOR + i] = arrDeferredInds.size();
	}

	//clip-projector frustum mesh
	for (int i = 0; i < 3; i++)
	{
		uint nClipFrustTess = 41 + nProjectorMeshStep * i;
		CDeferredRenderUtils::CreateUnitFrustumMesh(nClipFrustTess, nClipFrustTess, arrDeferredInds, arrDeferredVerts);
		SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_CLIP_PROJECTOR + i]);
		SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_CLIP_PROJECTOR + i]);
		COMPILE_TIME_ASSERT(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]));
		CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_CLIP_PROJECTOR + i], m_pUnitFrustumVB[SHAPE_CLIP_PROJECTOR + i]);
		m_UnitFrustVBSize[SHAPE_CLIP_PROJECTOR + i] = arrDeferredVerts.size();
		m_UnitFrustIBSize[SHAPE_CLIP_PROJECTOR + i] = arrDeferredInds.size();
	}

	//omni-light mesh
	//Use tess3 for big lights
	CDeferredRenderUtils::CreateUnitSphere(2, arrDeferredInds, arrDeferredVerts);
	SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_SPHERE]);
	SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_SPHERE]);
	COMPILE_TIME_ASSERT(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]));
	CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_SPHERE], m_pUnitFrustumVB[SHAPE_SPHERE]);
	m_UnitFrustVBSize[SHAPE_SPHERE] = arrDeferredVerts.size();
	m_UnitFrustIBSize[SHAPE_SPHERE] = arrDeferredInds.size();

	//unit box
	CDeferredRenderUtils::CreateUnitBox(arrDeferredInds, arrDeferredVerts);
	SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_BOX]);
	SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_BOX]);
	COMPILE_TIME_ASSERT(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]));
	CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_BOX], m_pUnitFrustumVB[SHAPE_BOX]);
	m_UnitFrustVBSize[SHAPE_BOX] = arrDeferredVerts.size();
	m_UnitFrustIBSize[SHAPE_BOX] = arrDeferredInds.size();

	//frustum approximated with 8 vertices
	CDeferredRenderUtils::CreateSimpleLightFrustumMesh(arrDeferredInds, arrDeferredVerts);
	SAFE_RELEASE(m_pUnitFrustumVB[SHAPE_SIMPLE_PROJECTOR]);
	SAFE_RELEASE(m_pUnitFrustumIB[SHAPE_SIMPLE_PROJECTOR]);
	COMPILE_TIME_ASSERT(kUnitObjectIndexSizeof == sizeof(arrDeferredInds[0]));
	CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, m_pUnitFrustumIB[SHAPE_SIMPLE_PROJECTOR], m_pUnitFrustumVB[SHAPE_SIMPLE_PROJECTOR]);
	m_UnitFrustVBSize[SHAPE_SIMPLE_PROJECTOR] = arrDeferredVerts.size();
	m_UnitFrustIBSize[SHAPE_SIMPLE_PROJECTOR] = arrDeferredInds.size();

	// FS quad
	CDeferredRenderUtils::CreateQuad(arrDeferredInds, arrDeferredVerts);
	SAFE_RELEASE(m_pQuadVB);
	D3DIndexBuffer* pDummyQuadIB = 0; // reusing CreateUnitVolumeMesh.
	CreateUnitVolumeMesh(arrDeferredInds, arrDeferredVerts, pDummyQuadIB, m_pQuadVB);
	m_nQuadVBSize = int16(arrDeferredVerts.size());

	return true;
}

bool CD3D9Renderer::ReleaseAuxiliaryMeshes()
{

	for (int i = 0; i < SHAPE_MAX; i++)
	{
		SAFE_RELEASE(m_pUnitFrustumVB[i]);
		SAFE_RELEASE(m_pUnitFrustumIB[i]);
	}

	SAFE_RELEASE(m_pQuadVB);
	m_nQuadVBSize = 0;

	return true;
}

bool CD3D9Renderer::CreateUnitVolumeMesh(t_arrDeferredMeshIndBuff& arrDeferredInds, t_arrDeferredMeshVertBuff& arrDeferredVerts, D3DIndexBuffer*& pUnitFrustumIB, D3DVertexBuffer*& pUnitFrustumVB)
{
	HRESULT hr = S_OK;

	//FIX: try default pools

	D3D11_BUFFER_DESC BufDesc;
	ZeroStruct(BufDesc);
	D3D11_SUBRESOURCE_DATA SubResData;
	ZeroStruct(SubResData);

	if (!arrDeferredVerts.empty())
	{
		BufDesc.ByteWidth = arrDeferredVerts.size() * sizeof(SDeferMeshVert);
		BufDesc.Usage = D3D11_USAGE_IMMUTABLE;
		BufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufDesc.CPUAccessFlags = 0;
		BufDesc.MiscFlags = 0;

		SubResData.pSysMem = &arrDeferredVerts[0];
		SubResData.SysMemPitch = 0;
		SubResData.SysMemSlicePitch = 0;

		hr = GetDevice().CreateBuffer(&BufDesc, &SubResData, (ID3D11Buffer**)&pUnitFrustumVB);
		assert(SUCCEEDED(hr));

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
		pUnitFrustumVB->SetPrivateData(WKPDID_D3DDebugObjectName, strlen("Unit Frustum"), "Unit Frustum");
#endif
	}

	if (!arrDeferredInds.empty())
	{
		ZeroStruct(BufDesc);
		BufDesc.ByteWidth = arrDeferredInds.size() * sizeof(arrDeferredInds[0]);
		BufDesc.Usage = D3D11_USAGE_IMMUTABLE;
		BufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		BufDesc.CPUAccessFlags = 0;
		BufDesc.MiscFlags = 0;

		ZeroStruct(SubResData);
		SubResData.pSysMem = &arrDeferredInds[0];
		SubResData.SysMemPitch = 0;
		SubResData.SysMemSlicePitch = 0;

		hr = GetDevice().CreateBuffer(&BufDesc, &SubResData, &pUnitFrustumIB);
		assert(SUCCEEDED(hr));

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
		pUnitFrustumVB->SetPrivateData(WKPDID_D3DDebugObjectName, strlen("Unit Frustum"), "Unit Frustum");
#endif
	}

	return SUCCEEDED(hr);
}

void CD3D9Renderer::FX_StencilCullPass(int nStencilID, int nNumVers, int nNumInds)
{
	int newState = m_RP.m_CurState;

	// Set LS colormask
	// debug states
	if (CV_r_DebugLightVolumes /*&& m_RP.m_TI.m_PersFlags2 & d3dRBPF2_ENABLESTENCILPB*/)
	{
		newState &= ~GS_COLMASK_NONE;
		newState &= ~GS_NODEPTHTEST;
		//	newState |= GS_NODEPTHTEST;
		newState |= GS_DEPTHWRITE;
		newState |= ((~(0xF)) << GS_COLMASK_SHIFT) & GS_COLMASK_MASK;

		if (CV_r_DebugLightVolumes > 1)
		{
			newState |= GS_WIREFRAME;
		}
	}
	else
	{
		//	// Disable color writes
		//	if (CV_r_ShadowsStencilPrePass == 2)
		//	{
		//	  newState &= ~GS_COLMASK_NONE;
		//	  newState |= GS_COLMASK_A;
		//	}
		//	else
		{
			newState |= GS_COLMASK_NONE;
		}

		//setup depth test and enable stencil
		newState &= ~(GS_NODEPTHTEST | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK);
		newState |= GS_DEPTHFUNC_LEQUAL | GS_STENCIL;
	}

	//////////////////////////////////////////////////////////////////////////
	// draw back faces - inc when depth fail
	//////////////////////////////////////////////////////////////////////////
	int stencilFunc = FSS_STENCFUNC_ALWAYS;
	uint32 nCurrRef = 0;
	if (nStencilID >= 0)
	{
		D3DSetCull(eCULL_Front);

		//	if (nStencilID > 0)
		stencilFunc = m_RP.m_CurStencilCullFunc;

		FX_SetStencilState(
		  STENC_FUNC(stencilFunc) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		  STENCOP_PASS(FSS_STENCOP_KEEP),
		  nStencilID, 0xFFFFFFFF, 0xFFFF
		  );

		//	uint32 nStencilWriteMask = 1 << nStencilID; //0..7
		//	FX_SetStencilState(
		//		STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
		//		STENCOP_FAIL(FSS_STENCOP_KEEP) |
		//		STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		//		STENCOP_PASS(FSS_STENCOP_KEEP),
		//		0xFF, 0xFFFFFFFF, nStencilWriteMask
		//	);
	}
	else if (nStencilID == -4) // set all pixels with nCurRef within clip volume to nCurRef-1
	{
		stencilFunc = FSS_STENCFUNC_EQUAL;
		nCurrRef = m_nStencilMaskRef;

		FX_SetStencilState(
		  STENC_FUNC(stencilFunc) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_DECR) |
		  STENCOP_PASS(FSS_STENCOP_KEEP),
		  nCurrRef, 0xFFFFFFFF, 0xFFFF
		  );

		m_nStencilMaskRef--;
		D3DSetCull(eCULL_Front);
	}
	else
	{
		if (nStencilID == -3) // TD: Fill stencil by values=1 for drawn volumes in order to avoid overdraw
		{
			stencilFunc = FSS_STENCFUNC_LEQUAL;
			m_nStencilMaskRef--;
		}
		else if (nStencilID == -2)
		{
			stencilFunc = FSS_STENCFUNC_GEQUAL;
			m_nStencilMaskRef--;
		}
		else
		{
			stencilFunc = FSS_STENCFUNC_GEQUAL;
			m_nStencilMaskRef++;
			//	m_nStencilMaskRef = m_nStencilMaskRef % STENC_MAX_REF + m_nStencilMaskRef / STENC_MAX_REF;
			if (m_nStencilMaskRef > STENC_MAX_REF)
			{
				EF_ClearTargetsImmediately(FRT_CLEAR_STENCIL, Clr_Unused.r, 1);
				m_nStencilMaskRef = 2;
			}
		}

		nCurrRef = m_nStencilMaskRef;
		assert(m_nStencilMaskRef > 0 && m_nStencilMaskRef <= STENC_MAX_REF);

		D3DSetCull(eCULL_Front);
		FX_SetStencilState(
		  STENC_FUNC(stencilFunc) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		  STENCOP_PASS(FSS_STENCOP_KEEP),
		  nCurrRef, 0xFFFFFFFF, 0xFFFF
		  );
	}

	FX_SetState(newState);
	FX_Commit();

	// Don't clip pixels beyond far clip plane
	SStateRaster PreviousRS = m_StatesRS[m_nCurStateRS];
	SStateRaster NoDepthClipRS = PreviousRS;
	NoDepthClipRS.Desc.DepthClipEnable = false;

	SetRasterState(&NoDepthClipRS);
	FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, nNumVers, 0, nNumInds);
	SetRasterState(&PreviousRS);

	//////////////////////////////////////////////////////////////////////////
	// draw front faces - decr when depth fail
	//////////////////////////////////////////////////////////////////////////
	if (nStencilID >= 0)
	{
		//	uint32 nStencilWriteMask = 1 << nStencilID; //0..7
		//	D3DSetCull(eCULL_Back);
		//	FX_SetStencilState(
		//		STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
		//		STENCOP_FAIL(FSS_STENCOP_KEEP) |
		//		STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		//		STENCOP_PASS(FSS_STENCOP_KEEP),
		//		0x0, 0xFFFFFFFF, nStencilWriteMask
		//	);
	}
	else
	{
		D3DSetCull(eCULL_Back);
		// TD: deferred meshed should have proper front facing on dx10
		FX_SetStencilState(
		  STENC_FUNC(stencilFunc) |
		  STENCOP_FAIL(FSS_STENCOP_KEEP) |
		  STENCOP_ZFAIL(FSS_STENCOP_ZERO) |
		  STENCOP_PASS(FSS_STENCOP_KEEP),
		  m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFF
		  );
	}

	// skip front faces when nStencilID is specified
	if (nStencilID <= 0)
	{
		FX_SetState(newState);
		FX_Commit();

		FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, nNumVers, 0, nNumInds);
	}

	return;
}

void CD3D9Renderer::FX_StencilFrustumCull(int nStencilID, const SRenderLight* pLight, ShadowMapFrustum* pFrustum, int nAxis)
{
	EShapeMeshType nPrimitiveID = m_RP.m_nDeferredPrimitiveID; //SHAPE_PROJECTOR;
	uint32 nPassCount = 0;
	CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;
	static CCryNameTSCRC StencilCullTechName = "DeferredShadowPass";

	float Z = 1.0f;

	Matrix44A mProjection = m_IdentityMatrix;
	Matrix44A mView = m_IdentityMatrix;

	CRY_ASSERT(pLight != NULL);
	bool bAreaLight = (pLight->m_Flags & DLF_AREA_LIGHT) && pLight->m_fAreaWidth && pLight->m_fAreaHeight && pLight->m_fLightFrustumAngle && CRenderer::CV_r_DeferredShadingAreaLights;

	Vec3 vOffsetDir(0, 0, 0);

	//un-projection matrix calc
	if (pFrustum == NULL)
	{
		ITexture* pLightTexture = pLight->GetLightTexture();
		if (pLight->m_fProjectorNearPlane < 0)
		{
			SRenderLight instLight = *pLight;
			vOffsetDir = (-pLight->m_fProjectorNearPlane) * (pLight->m_ObjMatrix.GetColumn0().GetNormalized());
			instLight.SetPosition(instLight.m_Origin - vOffsetDir);
			instLight.m_fRadius -= pLight->m_fProjectorNearPlane;
			CShadowUtils::GetCubemapFrustumForLight(&instLight, nAxis, 160.0f, &mProjection, &mView, false); // 3.0f -  offset to make sure that frustums are intersected
		}
		else if ((pLight->m_Flags & DLF_PROJECT) && pLightTexture && !(pLightTexture->GetFlags() & FT_REPLICATE_TO_ALL_SIDES))
		{
			//projective light
			//refactor projective light

			//for light source
			CShadowUtils::GetCubemapFrustumForLight(pLight, nAxis, pLight->m_fLightFrustumAngle * 2.f /*CShadowUtils::g_fOmniLightFov+3.0f*/, &mProjection, &mView, false); // 3.0f -  offset to make sure that frustums are intersected
		}
		else //omni/area light
		{
			//////////////// light sphere/box processing /////////////////////////////////
			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Push();

			float fExpensionRadius = pLight->m_fRadius * 1.08f;

			Vec3 vScale(fExpensionRadius, fExpensionRadius, fExpensionRadius);

			Matrix34 mLocal;
			if (bAreaLight)
			{
				mLocal = CShadowUtils::GetAreaLightMatrix(pLight, vScale);
			}
			else if (pLight->m_Flags & DLF_DEFERRED_CUBEMAPS)
			{
				Matrix33 rotMat(pLight->m_ObjMatrix.GetColumn0().GetNormalized() * pLight->m_ProbeExtents.x,
				                pLight->m_ObjMatrix.GetColumn1().GetNormalized() * pLight->m_ProbeExtents.y,
				                pLight->m_ObjMatrix.GetColumn2().GetNormalized() * pLight->m_ProbeExtents.z);
				mLocal = Matrix34::CreateTranslationMat(pLight->m_Origin) * rotMat * Matrix34::CreateScale(Vec3(2, 2, 2), Vec3(-1, -1, -1));
			}
			else
			{
				mLocal.SetIdentity();
				mLocal.SetScale(vScale, pLight->m_Origin);

				mLocal.m00 *= -1.0f;
			}

			Matrix44 mLocalTransposed = mLocal.GetTransposed();
			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->MultMatrixLocal(&mLocalTransposed);

			pShader->FXSetTechnique(StencilCullTechName);
			pShader->FXBegin(&nPassCount, FEF_DONTSETSTATES);

			// Vertex/index buffer
			const EShapeMeshType meshType = (bAreaLight || (pLight->m_Flags & DLF_DEFERRED_CUBEMAPS)) ? SHAPE_BOX : SHAPE_SPHERE;

			FX_SetVStream(0, m_pUnitFrustumVB[meshType], 0, sizeof(SVF_P3F_C4B_T2F));
			FX_SetIStream(m_pUnitFrustumIB[meshType], 0, (kUnitObjectIndexSizeof == 2 ? Index16 : Index32));

			//  shader pass
			pShader->FXBeginPass(DS_SHADOW_CULL_PASS);

			if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
				FX_StencilCullPass(nStencilID == -4 ? -4 : -1, m_UnitFrustVBSize[meshType], m_UnitFrustIBSize[meshType]);

			pShader->FXEndPass();

			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Pop();

			pShader->FXEnd();

			return;
			//////////////////////////////////////////////////////////////////////////
		}
	}
	else
	{
		if (!pFrustum->bOmniDirectionalShadow)
		{
			//temporarily disabled since mLightProjMatrix contains pre-multiplied matrix already
			//pmProjection = &(pFrustum->mLightProjMatrix);
			mProjection = gRenDev->m_IdentityMatrix;
			mView = pFrustum->mLightViewMatrix;
		}
		else
		{
			//calc one of cubemap's frustums
			Matrix33 mRot = (Matrix33(pLight->m_ObjMatrix));
			//rotation for shadow frustums is disabled
			CShadowUtils::GetCubemapFrustum(FTYP_OMNILIGHTVOLUME, pFrustum, nAxis, &mProjection, &mView /*, &mRot*/);
		}
	}

	//matrix concanation and inversion should be computed in doubles otherwise we have precision problems with big coords on big levels
	//which results to the incident frustum's discontinues for omni-lights
	Matrix44r mViewProj = Matrix44r(mView) * Matrix44r(mProjection);
	Matrix44A mViewProjInv = mViewProj.GetInverted();

	gRenDev->m_TempMatrices[0][0] = mViewProjInv.GetTransposed();

	//setup light source pos/radius
	m_cEF.m_TempVecs[5] = Vec4(pLight->m_Origin, pLight->m_fRadius * 1.1f); //increase radius slightly
	if (pLight->m_fProjectorNearPlane < 0)
	{
		m_cEF.m_TempVecs[5].x -= vOffsetDir.x;
		m_cEF.m_TempVecs[5].y -= vOffsetDir.y;
		m_cEF.m_TempVecs[5].z -= vOffsetDir.z;
		nPrimitiveID = SHAPE_CLIP_PROJECTOR;
	}

	//if (m_pUnitFrustumVB==NULL || m_pUnitFrustumIB==NULL)
	//  CreateUnitFrustumMesh();

	FX_SetVStream(0, m_pUnitFrustumVB[nPrimitiveID], 0, sizeof(SVF_P3F_C4B_T2F));
	FX_SetIStream(m_pUnitFrustumIB[nPrimitiveID], 0, (kUnitObjectIndexSizeof == 2 ? Index16 : Index32));

	pShader->FXSetTechnique(StencilCullTechName);
	pShader->FXBegin(&nPassCount, FEF_DONTSETSTATES);

	//  shader pass
	pShader->FXBeginPass(DS_SHADOW_FRUSTUM_CULL_PASS);

	if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
		FX_StencilCullPass(nStencilID, m_UnitFrustVBSize[nPrimitiveID], m_UnitFrustIBSize[nPrimitiveID]);

	pShader->FXEndPass();
	pShader->FXEnd();

	return;
}

void CD3D9Renderer::FX_StencilCull(int nStencilID, t_arrDeferredMeshIndBuff& arrDeferredInds, t_arrDeferredMeshVertBuff& arrDeferredVerts, CShader* pShader)
{
	PROFILE_FRAME(FX_StencilCull);

	TempDynVB<SVF_P3F_C4B_T2F>::CreateFillAndBind(&arrDeferredVerts[0], arrDeferredVerts.size(), 0);
	TempDynIB16::CreateFillAndBind(&arrDeferredInds[0], arrDeferredInds.size());

	uint32 nPasses = 0;
	pShader->FXBeginPass(DS_SHADOW_CULL_PASS);

	if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
		FX_StencilCullPass(nStencilID, arrDeferredVerts.size(), arrDeferredInds.size());

	pShader->FXEndPass();
}

void CD3D9Renderer::FX_StencilRefreshCustomVolume(int StencilFunc, uint32 nStencRef, uint32 nStencMask,
                                                  Vec3* pVerts, uint32 nNumVerts, uint16* pInds, uint32 nNumInds)
{
	// Use the backfaces of a specified volume to refresh hi-stencil
	if (!m_bDeviceSupports_NVDBT) return;  // For DBT capable hw only

	TempDynVB<SVF_P3F_C4B_T2F> vb;
	vb.Allocate(nNumVerts);
	SVF_P3F_C4B_T2F* Verts = vb.Lock();

	for (uint32 i = 0; i < nNumVerts; ++i)
		Verts[i].xyz = pVerts[i];

	vb.Unlock();
	vb.Bind(0);
	vb.Release();

	TempDynIB16::CreateFillAndBind(pInds, nNumInds);

	uint32 nPasses = 0;
	CShader* pSH(CShaderMan::s_ShaderShadowMaskGen);
	static CCryNameTSCRC TechName = "StencilVolume";
	pSH->FXSetTechnique(TechName);
	pSH->FXBegin(&nPasses, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);

	pSH->FXBeginPass(0);

	FX_SetStencilState(
	  StencilFunc |
	  STENCOP_FAIL(FSS_STENCOP_KEEP) |
	  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
	  STENCOP_PASS(FSS_STENCOP_KEEP),
	  nStencRef, nStencMask, 0xFFFFFFFF
	  );
	D3DSetCull(eCULL_Front);

	FX_Commit();

	D3DSetCull(eCULL_None);
	pSH->FXEndPass();
	pSH->FXEnd();
}

void       CD3D9Renderer::FX_StencilCullNonConvex(int nStencilID, IRenderMesh* pWaterTightMesh, const Matrix34& mWorldTM)
{
	CShader* pShader(CShaderMan::s_ShaderShadowMaskGen);
	static CCryNameTSCRC TechName0 = "DeferredShadowPass";

	CRenderMesh* pRenderMesh = static_cast<CRenderMesh*>(pWaterTightMesh);
	pRenderMesh->CheckUpdate(pRenderMesh->_GetVertexFormat(), 0);

	const buffer_handle_t hVertexStream = pRenderMesh->_GetVBStream(VSF_GENERAL);
	const buffer_handle_t hIndexStream = pRenderMesh->_GetIBStream();

	if (hVertexStream != ~0u && hIndexStream != ~0u)
	{
		size_t nOffsI = 0;
		size_t nOffsV = 0;

		D3DVertexBuffer* pVB = gRenDev->m_DevBufMan.GetD3DVB(hVertexStream, &nOffsV);
		D3DIndexBuffer* pIB = gRenDev->m_DevBufMan.GetD3DIB(hIndexStream, &nOffsI);

		FX_SetVStream(0, pVB, nOffsV, pRenderMesh->GetStreamStride(VSF_GENERAL));
		FX_SetIStream(pIB, nOffsI, (sizeof(vtx_idx) == 2 ? Index16 : Index32));

		if (!FAILED(FX_SetVertexDeclaration(0, pRenderMesh->_GetVertexFormat())))
		{
			ECull nPrevCullMode = m_RP.m_eCull;
			int nPrevState = m_RP.m_CurState;

			int newState = nPrevState;
			newState &= ~(GS_NODEPTHTEST | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK);
			newState |= GS_DEPTHFUNC_LEQUAL | GS_STENCIL | GS_COLMASK_NONE;

			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Push();
			Matrix44 mLocalTransposed;
			mLocalTransposed = mWorldTM.GetTransposed();
			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->MultMatrixLocal(&mLocalTransposed);

			uint32 nPasses = 0;
			pShader->FXSetTechnique(TechName0);
			pShader->FXBegin(&nPasses, FEF_DONTSETSTATES);
			pShader->FXBeginPass(CD3D9Renderer::DS_SHADOW_CULL_PASS);

			// Mark all pixels that might be inside volume first (z-fail on back-faces)
			{
				D3DSetCull(eCULL_Front);
				FX_SetStencilState(
				  STENC_FUNC(FSS_STENCFUNC_GEQUAL) |
				  STENCOP_FAIL(FSS_STENCOP_KEEP) |
				  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
				  STENCOP_PASS(FSS_STENCOP_KEEP),
				  nStencilID, 0xFFFFFFFF, 0xFFFFFFFF
				  );

				FX_SetState(newState);
				FX_Commit();
				FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, pRenderMesh->_GetNumVerts(), 0, pRenderMesh->_GetNumInds());
			}

			// Flip bits for each face
			{
				D3DSetCull(eCULL_None);
				FX_SetStencilState(STENC_FUNC(FSS_STENCFUNC_GEQUAL) |
				                   STENCOP_FAIL(FSS_STENCOP_KEEP) |
				                   STENCOP_ZFAIL(FSS_STENCOP_INVERT) |
				                   STENCOP_PASS(FSS_STENCOP_KEEP),
				                   ~nStencilID, 0xFFFFFFFF, 0xFFFFFFFF);

				FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, pRenderMesh->_GetNumVerts(), 0, pRenderMesh->_GetNumInds());
			}

			D3DSetCull(nPrevCullMode);
			FX_SetState(nPrevState);

			m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Pop();

			pShader->FXEndPass();
			pShader->FXEnd();
		}
	}
}

void CD3D9Renderer::FX_CreateDeferredQuad(const SRenderLight* pLight, float maskRTWidth, float maskRTHeight, Matrix44A* pmCurView, Matrix44A* pmCurComposite, float fCustomZ)
{
	//////////////////////////////////////////////////////////////////////////
	// Create FS quad
	//////////////////////////////////////////////////////////////////////////

	TempDynVB<SVF_P3F_T2F_T3F> vb;
	vb.Allocate(4);
	SVF_P3F_T2F_T3F* vQuad = vb.Lock();

	Vec2 vBoundRectMin(0.0f, 0.0f), vBoundRectMax(1.0f, 1.0f);
	Vec3 vCoords[8];
	Vec3 vRT, vLT, vLB, vRB;

	//calc screen quad
	if (!(pLight->m_Flags & DLF_DIRECTIONAL))
	{
		if (CV_r_ShowLightBounds)
			CShadowUtils::CalcLightBoundRect(pLight, GetRCamera(), *pmCurView, *pmCurComposite, &vBoundRectMin, &vBoundRectMax, gRenDev->GetIRenderAuxGeom());
		else
			CShadowUtils::CalcLightBoundRect(pLight, GetRCamera(), *pmCurView, *pmCurComposite, &vBoundRectMin, &vBoundRectMax, NULL);

		if (m_RenderTileInfo.nGridSizeX > 1.f || m_RenderTileInfo.nGridSizeY > 1.f)
		{

			//always render full-screen quad for hi-res screenshots
			gcpRendD3D->GetRCamera().CalcTileVerts(vCoords,
			                                       m_RenderTileInfo.nGridSizeX - 1 - m_RenderTileInfo.nPosX,
			                                       m_RenderTileInfo.nPosY,
			                                       m_RenderTileInfo.nGridSizeX,
			                                       m_RenderTileInfo.nGridSizeY);
			vBoundRectMin = Vec2(0.0f, 0.0f);
			vBoundRectMax = Vec2(1.0f, 1.0f);

			vRT = vCoords[4] - vCoords[0];
			vLT = vCoords[5] - vCoords[1];
			vLB = vCoords[6] - vCoords[2];
			vRB = vCoords[7] - vCoords[3];

			// Swap order when mirrored culling enabled
			if (gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_MIRRORCULL)
			{
				vLT = vCoords[4] - vCoords[0];
				vRT = vCoords[5] - vCoords[1];
				vRB = vCoords[6] - vCoords[2];
				vLB = vCoords[7] - vCoords[3];
			}

		}
		else
		{
			GetRCamera().CalcRegionVerts(vCoords, vBoundRectMin, vBoundRectMax);
			vRT = vCoords[4] - vCoords[0];
			vLT = vCoords[5] - vCoords[1];
			vLB = vCoords[6] - vCoords[2];
			vRB = vCoords[7] - vCoords[3];
		}
	}
	else //full screen case for directional light
	{
		if (m_RenderTileInfo.nGridSizeX > 1.f || m_RenderTileInfo.nGridSizeY > 1.f)
			gcpRendD3D->GetRCamera().CalcTileVerts(vCoords,
			                                       m_RenderTileInfo.nGridSizeX - 1 - m_RenderTileInfo.nPosX,
			                                       m_RenderTileInfo.nPosY,
			                                       m_RenderTileInfo.nGridSizeX,
			                                       m_RenderTileInfo.nGridSizeY);
		else
			gcpRendD3D->GetRCamera().CalcVerts(vCoords);

		vRT = vCoords[4] - vCoords[0];
		vLT = vCoords[5] - vCoords[1];
		vLB = vCoords[6] - vCoords[2];
		vRB = vCoords[7] - vCoords[3];

	}

	vQuad[0].p.x = vBoundRectMin.x;
	vQuad[0].p.y = vBoundRectMin.y;
	vQuad[0].p.z = fCustomZ;
	vQuad[0].st0[0] = vBoundRectMin.x;
	vQuad[0].st0[1] = 1 - vBoundRectMin.y;
	vQuad[0].st1 = vLB;

	vQuad[1].p.x = vBoundRectMax.x;
	vQuad[1].p.y = vBoundRectMin.y;
	vQuad[1].p.z = fCustomZ;
	vQuad[1].st0[0] = vBoundRectMax.x;
	vQuad[1].st0[1] = 1 - vBoundRectMin.y;
	vQuad[1].st1 = vRB;

	vQuad[3].p.x = vBoundRectMax.x;
	vQuad[3].p.y = vBoundRectMax.y;
	vQuad[3].p.z = fCustomZ;
	vQuad[3].st0[0] = vBoundRectMax.x;
	vQuad[3].st0[1] = 1 - vBoundRectMax.y;
	vQuad[3].st1 = vRT;

	vQuad[2].p.x = vBoundRectMin.x;
	vQuad[2].p.y = vBoundRectMax.y;
	vQuad[2].p.z = fCustomZ;
	vQuad[2].st0[0] = vBoundRectMin.x;
	vQuad[2].st0[1] = 1 - vBoundRectMax.y;
	vQuad[2].st1 = vLT;

	vb.Unlock();
	vb.Bind(0);
	vb.Release();
}

void CD3D9Renderer::FX_DeferredShadowMaskGen(CRenderView* pRenderView, const TArray<uint32>& shadowPoolLights)
{
	FUNCTION_PROFILER_RENDERER

	const uint64 nPrevFlagsShaderRT = m_RP.m_FlagsShader_RT;

	const int nThreadID = m_RP.m_nProcessThreadID;
	const int nPreviousState = m_RP.m_CurState;

	const bool isShadowPassEnabled = IsShadowPassEnabled();
	const int nMaskWidth = GetWidth();
	const int nMaskHeight = GetHeight();

	// reset render element and current render object in pipeline
	m_RP.m_pRE = 0;
	m_RP.m_pCurObject = m_RP.m_pIdendityRenderObject;
	m_RP.m_ObjFlags = 0;

	m_RP.m_FlagsShader_RT = 0;
	m_RP.m_FlagsShader_LT = 0;
	m_RP.m_FlagsShader_MD = 0;
	m_RP.m_FlagsShader_MDV = 0;

	FX_ResetPipe();
	FX_Commit();

	CTexture* pShadowMask = CTexture::s_ptexShadowMask;
	SResourceView curSliceRVDesc = SResourceView::RenderTargetView(pShadowMask->GetTextureDstFormat(), 0, 1);
	SResourceView& firstSliceRV = pShadowMask->GetResourceView(curSliceRVDesc);

	// set shadow mask RT and clear stencil
	FX_ClearTarget(static_cast<D3DSurface*>(firstSliceRV.m_pDeviceResourceView), Clr_Transparent, 0, nullptr);
	FX_ClearTarget(&m_DepthBufferOrig, CLEAR_STENCIL, Clr_Unused.r, 0);
	FX_PushRenderTarget(0, static_cast<D3DSurface*>(firstSliceRV.m_pDeviceResourceView), &m_DepthBufferOrig);
	RT_SetViewport(0, 0, nMaskWidth, nMaskHeight);

	int nFirstChannel = 0;
	int nChannelsInUse = 0;

	// sun
	if (m_RP.m_pSunLight && isShadowPassEnabled)
	{
		PROFILE_LABEL_SCOPE("SHADOWMASK_SUN");
		FX_DeferredShadows(pRenderView, m_RP.m_pSunLight, nMaskWidth, nMaskHeight);
		++nFirstChannel;
		++nChannelsInUse;
	}

	// point lights
	if (!shadowPoolLights.empty() && isShadowPassEnabled)
	{
		PROFILE_LABEL_SCOPE("SHADOWMASK_DEFERRED_LIGHTS");

		const int nMaxChannelCount = pShadowMask->StreamGetNumSlices() * 4;
		std::vector<std::vector<std::pair<int, Vec4>>> lightsPerChannel;
		lightsPerChannel.resize(nMaxChannelCount);

		// sort lights into layers first in order to minimize the number of required render targets
		for (int i = 0; i < shadowPoolLights.size(); ++i)
		{
			const int nLightID = shadowPoolLights[i];
			SRenderLight* pLight = &pRenderView->GetLight(eDLT_DeferredLight, nLightID);
			const int nFrustumIdx = pRenderView->GetDynamicLightsCount() + nLightID;

			if (!pLight || !(pLight->m_Flags & DLF_CASTSHADOW_MAPS))
				__debugbreak();

			auto& SMFrustums = pRenderView->GetShadowFrustumsForLight(nFrustumIdx);

			//no single frustum was allocated for this light
			if (SMFrustums.empty())
				continue;

			// get light scissor rect
			Vec4 pLightRect = Vec4(static_cast<float>(pLight->m_sX), static_cast<float>(pLight->m_sY), static_cast<float>(pLight->m_sWidth), static_cast<float>(pLight->m_sHeight));

			int nChannelIndex = nFirstChannel;
			while (nChannelIndex < nMaxChannelCount)
			{
				bool bHasOverlappingLight = false;

				float minX = pLightRect.x, maxX = pLightRect.x + pLightRect.z;
				float minY = pLightRect.y, maxY = pLightRect.y + pLightRect.w;

				for (int j = 0; j < lightsPerChannel[nChannelIndex].size(); ++j)
				{
					const Vec4& lightRect = lightsPerChannel[nChannelIndex][j].second;

					if (maxX >= lightRect.x && minX <= lightRect.x + lightRect.z &&
					    maxY >= lightRect.y && minY <= lightRect.y + lightRect.w)
					{
						bHasOverlappingLight = true;
						break;
					}
				}

				if (!bHasOverlappingLight)
				{
					lightsPerChannel[nChannelIndex].push_back(std::make_pair(nLightID, pLightRect));
					nChannelsInUse = max(nChannelIndex + 1, nChannelsInUse);
					break;
				}

				++nChannelIndex;
			}

			if (nChannelIndex >= nMaxChannelCount)
			{
				SMFrustums[0]->pFrustum->nShadowGenMask = 0;
				++nChannelsInUse;
			}
		}

		// now render each layer
		for (int nChannel = nFirstChannel; nChannel < min(nChannelsInUse, nMaxChannelCount); ++nChannel)
		{
			const int nMaskIndex = nChannel / 4;
			const int nMaskChannel = nChannel % 4;

			if (nChannel > 0 && nMaskChannel == 0)
			{
				curSliceRVDesc.m_Desc.nFirstSlice = nMaskIndex;
				SResourceView& curSliceRV = pShadowMask->GetResourceView(curSliceRVDesc);

				FX_PopRenderTarget(0);
				FX_PushRenderTarget(0, static_cast<D3DSurface*>(curSliceRV.m_pDeviceResourceView), &m_DepthBufferOrig);
			}

			for (int i = 0; i < lightsPerChannel[nChannel].size(); ++i)
			{
				const int nLightIndex = lightsPerChannel[nChannel][i].first;
				const Vec4 lightRect = lightsPerChannel[nChannel][i].second;

				SRenderLight* pLight = &pRenderView->GetLight(eDLT_DeferredLight, nLightIndex);
				assert(pLight);

				m_RP.m_nDeferredPrimitiveID = SHAPE_PROJECTOR;

				//////////////////////////////////////////////////////////////////////////
				const int nFrustumIdx = pRenderView->GetDynamicLightsCount() + nLightIndex;

				auto& SMFrustums = pRenderView->GetShadowFrustumsForLight(nFrustumIdx);
				assert(!SMFrustums.empty());

				ShadowMapFrustum& firstFrustum = *SMFrustums[0]->pFrustum;

				const int nSides = firstFrustum.bOmniDirectionalShadow ? 6 : 1;
				const bool bAreaLight = (pLight->m_Flags & DLF_AREA_LIGHT) && pLight->m_fAreaWidth && pLight->m_fAreaHeight && pLight->m_fLightFrustumAngle;

				//enable hw-pcf light
				if (firstFrustum.bHWPCFCompare)
					m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE];

				// determine what's more beneficial: full screen quad or light volume
				bool bStencilMask = true;
				bool bUseLightVolumes = false;
				CDeferredShading::Instance().GetLightRenderSettings(pLight, bStencilMask, bUseLightVolumes, m_RP.m_nDeferredPrimitiveID);

				// reserve stencil values
				m_nStencilMaskRef += (nSides + 1);
				if (m_nStencilMaskRef > STENC_MAX_REF)
				{
					FX_ClearTarget(&m_DepthBufferOrig, CLEAR_STENCIL, Clr_Unused.r, 0);
					m_nStencilMaskRef = nSides + 1;
				}

				if (CRenderer::CV_r_DeferredShadingScissor)
				{
					EF_Scissor(true,
					           static_cast<int>(lightRect.x * m_RP.m_CurDownscaleFactor.x),
					           static_cast<int>(lightRect.y * m_RP.m_CurDownscaleFactor.y),
					           static_cast<int>(lightRect.z * m_RP.m_CurDownscaleFactor.x + 1),
					           static_cast<int>(lightRect.w * m_RP.m_CurDownscaleFactor.y + 1));
				}

				uint32 nPersFlagsPrev = m_RP.m_TI[nThreadID].m_PersFlags;

				for (int nS = 0; nS < nSides; nS++)
				{
					// render light volume to stencil
					{
						m_RP.m_TI[nThreadID].m_PersFlags |= RBPF_MIRRORCULL;
						FX_StencilFrustumCull(-2, pLight, bAreaLight ? NULL : &firstFrustum, nS);
					}

					FX_StencilTestCurRef(true, true);

					if (firstFrustum.nShadowGenMask & (1 << nS))
					{
						FX_ApplyShadowQuality();
						CShader* pShader = CShaderMan::s_shDeferredShading;
						static CCryNameTSCRC techName = "ShadowMaskGen";
						static CCryNameTSCRC techNameVolume = "ShadowMaskGenVolume";

						ConfigShadowTexgen(0, &firstFrustum, nS);

						if (CV_r_ShadowsScreenSpace)
						{
							m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
						}

						if (bUseLightVolumes)
						{
							m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
							SD3DPostEffectsUtils::ShBeginPass(pShader, techNameVolume, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
						}
						else
							SD3DPostEffectsUtils::ShBeginPass(pShader, techName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

						// comparison filtering for shadow map
						STexState TS(firstFrustum.bHWPCFCompare ? FILTER_LINEAR : FILTER_POINT, true);
						TS.m_bSRGBLookup = false;
						TS.SetComparisonFilter(true);

						CTexture* pShadowMap = firstFrustum.bUseShadowsPool ? CTexture::s_ptexRT_ShadowPool : firstFrustum.pDepthTex;
						pShadowMap->Apply(1, CTexture::GetTexState(TS));

						SD3DPostEffectsUtils::SetTexture(CTexture::s_ptexShadowJitterMap, 7, FILTER_POINT, 0);

						static ICVar* pVar = iConsole->GetCVar("e_ShadowsPoolSize");
						int nShadowAtlasRes = pVar->GetIVal();

						//LRad
						float kernelSize = firstFrustum.bOmniDirectionalShadow ? 2.5f : 1.5f;
						const Vec4 vShadowParams(kernelSize * (float(firstFrustum.nTexSize) / nShadowAtlasRes), firstFrustum.nTexSize, 0.0f, firstFrustum.fDepthConstBias);
						static CCryNameR generalParamsName("g_GeneralParams");
						pShader->FXSetPSFloat(generalParamsName, &vShadowParams, 1);

						// set up shadow matrix
						static CCryNameR lightProjParamName("g_mLightShadowProj");
						Matrix44A shadowMat = gRenDev->m_TempMatrices[0][0];
						const Vec4 vEye(gRenDev->GetRCamera().vOrigin, 0.f);
						Vec4 vecTranslation(vEye.Dot((Vec4&)shadowMat.m00), vEye.Dot((Vec4&)shadowMat.m10), vEye.Dot((Vec4&)shadowMat.m20), vEye.Dot((Vec4&)shadowMat.m30));
						shadowMat.m03 += vecTranslation.x;
						shadowMat.m13 += vecTranslation.y;
						shadowMat.m23 += vecTranslation.z;
						shadowMat.m33 += vecTranslation.w;

						// pre-multiply by 1/frustrum_far_plane
						(Vec4&)shadowMat.m20 *= gRenDev->m_cEF.m_TempVecs[2].x;

						//camera matrix
						pShader->FXSetPSFloat(lightProjParamName, alias_cast<Vec4*>(&shadowMat), 4);

						Vec4 vScreenScale(1.0f / nMaskWidth, 1.0f / nMaskHeight, 0.0f, 0.0f);
						static CCryNameR screenScaleParamName("g_ScreenScale");
						pShader->FXSetPSFloat(screenScaleParamName, &vScreenScale, 1);

						Vec4 vLightPos(pLight->m_Origin, 0.0f);
						static CCryNameR vLightPosName("g_vLightPos");
						pShader->FXSetPSFloat(vLightPosName, &vLightPos, 1);

						CTexture::s_ptexZTarget->Apply(0, CTexture::GetTexState(STexState(FILTER_POINT, true)));

						// color mask
						uint32 newState = m_RP.m_CurState & ~(GS_COLMASK_NONE | GS_BLEND_MASK);
						newState |= ~((1 << nMaskChannel % 4) << GS_COLMASK_SHIFT) & GS_COLMASK_MASK;

						if (bUseLightVolumes)
						{
							// shadow clip space to world space transform
							Matrix44A mUnitVolumeToWorld(IDENTITY);
							Vec4 vSphereAdjust(ZERO);

							if (bAreaLight)
							{
								const float fExpensionRadius = pLight->m_fRadius * 1.08f;
								mUnitVolumeToWorld = CShadowUtils::GetAreaLightMatrix(pLight, Vec3(fExpensionRadius)).GetTransposed();
								m_RP.m_nDeferredPrimitiveID = SHAPE_BOX;
							}
							else
							{
								Matrix44A mProjection, mView;
								if (firstFrustum.bOmniDirectionalShadow)
								{
									CShadowUtils::GetCubemapFrustum(FTYP_OMNILIGHTVOLUME, &firstFrustum, nS, &mProjection, &mView);
								}
								else
								{
									mProjection = gRenDev->m_IdentityMatrix;
									mView = firstFrustum.mLightViewMatrix;
								}

								Matrix44r mViewProj = Matrix44r(mView) * Matrix44r(mProjection);
								mUnitVolumeToWorld = mViewProj.GetInverted();
								vSphereAdjust = Vec4(pLight->m_Origin, pLight->m_fRadius * 1.1f);
							}

							newState &= ~(GS_NODEPTHTEST | GS_DEPTHWRITE);
							newState |= GS_DEPTHFUNC_LEQUAL;
							FX_SetState(newState);

							CDeferredShading::Instance().DrawLightVolume(m_RP.m_nDeferredPrimitiveID, mUnitVolumeToWorld, vSphereAdjust);
						}
						else
						{
							// depth state
							newState &= ~GS_DEPTHWRITE;
							newState |= GS_NODEPTHTEST;
							FX_SetState(newState);

							m_RP.m_TI[nThreadID].m_PersFlags &= ~RBPF_MIRRORCULL;
							gcpRendD3D->D3DSetCull(eCULL_Back, true); //fs quads should not revert test..
							SD3DPostEffectsUtils::DrawFullScreenTriWPOS(nMaskWidth, nMaskHeight);
						}

						SD3DPostEffectsUtils::ShEndPass();
					}

					// restore PersFlags
					m_RP.m_TI[nThreadID].m_PersFlags = nPersFlagsPrev;

				} // for each side

				pLight->m_ShadowChanMask = nMaskChannel;
				pLight->m_ShadowMaskIndex = nMaskIndex;

				EF_Scissor(false, 0, 0, 0, 0);

				m_nStencilMaskRef += nSides;

				m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE] | g_HWSR_MaskBit[HWSR_CUBEMAP0] | g_HWSR_MaskBit[HWSR_SAMPLE2]);
			} // for each light
		}
	}

#ifndef _RELEASE
	m_RP.m_PS[nThreadID].m_NumShadowMaskChannels = (pShadowMask->StreamGetNumSlices() * 4 << 16) | (nChannelsInUse & 0xFFFF);
#endif

	gcpRendD3D->D3DSetCull(eCULL_Back, true);

	FX_SetState(nPreviousState);
	FX_PopRenderTarget(0);

	m_RP.m_FlagsShader_RT = nPrevFlagsShaderRT;
}

bool CD3D9Renderer::FX_DeferredShadows(CRenderView* pRenderView, SRenderLight* pLight, int maskRTWidth, int maskRTHeight)
{
	if (!pLight || (pLight->m_Flags & DLF_CASTSHADOW_MAPS) == 0)
		return false;

	const int nThreadID = m_RP.m_nProcessThreadID;

	//set ScreenToWorld Expansion Basis
	Vec3 vWBasisX, vWBasisY, vWBasisZ;
	CShadowUtils::CalcScreenToWorldExpansionBasis(GetCamera(), (float)maskRTWidth, (float)maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, true);
	m_cEF.m_TempVecs[10] = Vec4(vWBasisX, 1.0f);
	m_cEF.m_TempVecs[11] = Vec4(vWBasisY, 1.0f);
	m_cEF.m_TempVecs[12] = Vec4(vWBasisZ, 1.0f);

	const bool bCloudShadows = m_bCloudShadowsEnabled && (m_cloudShadowTexId > 0 || GetVolumetricCloud().IsEnabledVolumetricCloudsShadow());
	const bool bCustomShadows = !pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_Custom).empty();

	//////////////////////////////////////////////////////////////////////////
	//check for valid gsm frustums
	//////////////////////////////////////////////////////////////////////////
	CRY_ASSERT(pLight->m_Id >= 0);

	auto& arrFrustums = pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_SunDynamic);
	if (arrFrustums.empty() && !bCloudShadows && !bCustomShadows)
		return false;

	// set shader
	CShader* pSH(CShaderMan::s_ShaderShadowMaskGen);
	uint32 nPasses = 0;
	static CCryNameR TechName("DeferredShadowPass");

	static ICVar* pCascadesDebugVar = iConsole->GetCVar("e_ShadowsCascadesDebug");
	const bool bDebugShadowCascades = pCascadesDebugVar && pCascadesDebugVar->GetIVal() > 0;

	const int nCasterCount = arrFrustums.size();
	const bool bCascadeBlending = CV_r_ShadowsStencilPrePass == 1 && nCasterCount > 0 && arrFrustums.front()->pFrustum && arrFrustums.front()->pFrustum->bBlendFrustum && !bDebugShadowCascades;
	if (bCascadeBlending)
	{
		for (int nCaster = 1; nCaster < nCasterCount; nCaster++)
		{
			arrFrustums[nCaster]->pFrustum->pPrevFrustum = arrFrustums[nCaster - 1]->pFrustum;
		}

		for (int nCaster = 0; nCaster < arrFrustums.size(); nCaster++)
		{
			const bool bFirstCaster = (nCaster == 0);
			const bool bLastCaster = (nCaster == nCasterCount - 1);

			const uint32 nStencilID = 2 * (nCaster) + 1;
			const uint32 nMaxStencilID = 2 * nCasterCount + 1;

			m_RP.m_CurStencilCullFunc = FSS_STENCFUNC_GEQUAL;

			m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE3];
			FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, false, false, true, nStencilID);    // This frustum

			if (!bLastCaster)
			{
				m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE3];
				FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, false, false, true, nStencilID + 1);  // This frustum, not including blend region

				if (!bFirstCaster)
				{
					m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE3];
					FX_DeferredShadowPass(pLight, arrFrustums[nCaster - 1]->pFrustum, false, false, true, nStencilID + 1); // Mask whole prior frustum ( allow blending region )
				}
			}

			m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE3];
			FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, true, false, false, !bLastCaster ? (nStencilID + 1) : nStencilID); // non-blending

			if (!bLastCaster)
			{
				m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE3];
				FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, true, false, false, nStencilID);    // blending
			}

			m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE3];

			m_RP.m_CurStencilCullFunc = FSS_STENCFUNC_ALWAYS;

			if (!bLastCaster)
			{
				FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, false, false, true, nMaxStencilID);    // Invalidate interior region for future rendering
			}

			m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE3];

			if (!bFirstCaster && !bLastCaster)
			{
				FX_DeferredShadowPass(pLight, arrFrustums[nCaster - 1]->pFrustum, false, false, true, nMaxStencilID);    // Remove prior region
			}
		}

		for (int nCaster = 0; nCaster < arrFrustums.size(); nCaster++)
		{
			arrFrustums[nCaster]->pFrustum->pPrevFrustum = nullptr;
		}
	}
	else if (CV_r_ShadowsStencilPrePass == 1)
	{
		m_RP.m_CurStencilCullFunc = FSS_STENCFUNC_GEQUAL;
		for (int nCaster = 0; nCaster < nCasterCount; nCaster++)
			FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, false, false, true, nCasterCount - nCaster);

#ifdef SUPPORTS_MSAA
		// Todo: bilateral upscale pass. Measure performance vs rendering shadows to multisampled target+sample freq passes
		FX_MSAASampleFreqStencilSetup();
#endif

		m_RP.m_CurStencilCullFunc = FSS_STENCFUNC_ALWAYS;
		for (int nCaster = 0; nCaster < nCasterCount; nCaster++)
			FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, true, false, false, nCasterCount - nCaster);
	}
	else if (CV_r_ShadowsStencilPrePass == 0) // @Nick: Can we remove this mode?
	{
		//////////////////////////////////////////////////////////////////////////
		//shadows passes
		for (int nCaster = 0; nCaster < nCasterCount; nCaster++ /*, m_RP.m_PS[rd->m_RP.m_nProcessThreadID]. ++ */) // for non-conservative
		{
			m_RP.m_CurStencilCullFunc = FSS_STENCFUNC_GEQUAL;
			FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, false, false, true, 10 - (nCaster + 1));

			m_RP.m_CurStencilCullFunc = FSS_STENCFUNC_ALWAYS;
			FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, true, false, false, 10 - (nCaster + 1));

		}
	}
	else if (CV_r_ShadowsStencilPrePass == 2) // @Nick: Can we remove this mode?
	{
		for (int nCaster = 0; nCaster < nCasterCount; nCaster++)
		{
			m_RP.m_CurStencilCullFunc = FSS_STENCFUNC_GEQUAL;
			int nLod = (10 - (nCaster + 1));

			//stencil mask
			FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, false, false, true, nLod);

			//shadow pass
			FX_DeferredShadowPass(pLight, arrFrustums[nCaster]->pFrustum, true, false, false, nLod);
		}

		m_RP.m_CurStencilCullFunc = FSS_STENCFUNC_ALWAYS;
	}
	else
	{
		assert(0);
	}

	// update stencil ref value, so subsequent passes will not use the same stencil values
	m_nStencilMaskRef = bCascadeBlending ? 2 * nCasterCount + 1 : nCasterCount;

	///////////////// Cascades debug mode /////////////////////////////////
	if ((pLight->m_Flags & DLF_SUN) && bDebugShadowCascades)
	{
		PROFILE_LABEL_SCOPE("DEBUG_SHADOWCASCADES");

		static CCryNameTSCRC TechName = "DebugShadowCascades";
		static CCryNameR CascadeColorParam("DebugCascadeColor");

		const Vec4 cascadeColors[] =
		{
			Vec4(1, 0, 0, 1),
			Vec4(0, 1, 0, 1),
			Vec4(0, 0, 1, 1),
			Vec4(1, 1, 0, 1),
			Vec4(1, 0, 1, 1),
			Vec4(0, 1, 1, 1),
			Vec4(1, 0, 0, 1),
		};

		// Draw information text for Cascade colors
		float yellow[4] = { 1.f, 1.f, 0.f, 1.f };
		Draw2dLabel(10.f, 30.f, 2.0f, yellow, false,
		            "e_ShadowsCascadesDebug");
		Draw2dLabel(40.f, 60.f, 1.5f, yellow, false,
		            "Cascade0: Red\n"
		            "Cascade1: Green\n"
		            "Cascade2: Blue\n"
		            "Cascade3: Yellow\n"
		            "Cascade4: Pink"
		            );

		const int cascadeColorCount = CRY_ARRAY_COUNT(cascadeColors);

		// render into diffuse color target
		CTexture* colorTarget = CTexture::s_ptexSceneDiffuse;
		SDepthTexture* depthStencilTarget = &m_DepthBufferOrig;

		FX_PushRenderTarget(0, colorTarget, depthStencilTarget);

		const int oldState = m_RP.m_CurState;
		int newState = oldState;
		newState |= GS_STENCIL;
		newState &= ~GS_COLMASK_NONE;

		FX_SetState(newState);

		for (int nCaster = 0; nCaster < nCasterCount; ++nCaster)
		{
			ShadowMapFrustum* pFr = arrFrustums[nCaster]->pFrustum;
			const Vec4& cascadeColor = cascadeColors[pFr->nShadowMapLod % cascadeColorCount];

			FX_SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
			  STENCOP_PASS(FSS_STENCOP_KEEP),
			  nCasterCount - nCaster, 0xFFFFFFFF, 0xFFFFFFFF
			  );

			// set shader
			SD3DPostEffectsUtils::ShBeginPass(pSH, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
			pSH->FXSetPSFloat(CascadeColorParam, &cascadeColor, 1);

			SD3DPostEffectsUtils::DrawFullScreenTriWPOS(colorTarget->GetWidth(), colorTarget->GetHeight());
			SD3DPostEffectsUtils::ShEndPass();
		}

		FX_SetState(oldState);
		FX_PopRenderTarget(0);
	}

	//////////////////////////////////////////////////////////////////////////
	//draw clouds shadow
	if (bCloudShadows)
	{
		ShadowMapFrustum cloudsFrustum;
		cloudsFrustum.bUseAdditiveBlending = true;
		FX_DeferredShadowPass(pLight, &cloudsFrustum, true, true, false, 0);
	}

	//////////////////////////////////////////////////////////////////////////
	{
		PROFILE_LABEL_SCOPE("CUSTOM SHADOW MAPS")

		for (auto& frustumToRender : pRenderView->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_Custom))
		{
			ShadowMapFrustum& curFrustum = *frustumToRender->pFrustum;
			const bool bIsNearestFrustum = curFrustum.m_eFrustumType == ShadowMapFrustum::e_Nearest;

			// stencil prepass for per rendernode frustums. front AND back faces here
			if (!bIsNearestFrustum)
				FX_DeferredShadowPass(pLight, &curFrustum, false, false, true, -1);

			// shadow pass
			FX_DeferredShadowPass(pLight, &curFrustum, true, false, false, bIsNearestFrustum ? 0 : m_nStencilMaskRef);
		}
	}
	////////////////////////////////////////////////////////////////////////////////////////////////

	EF_DirtyMatrix();

	return true;
}
