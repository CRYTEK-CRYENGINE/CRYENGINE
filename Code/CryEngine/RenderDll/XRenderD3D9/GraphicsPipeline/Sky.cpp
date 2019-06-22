// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Sky.h"

#include <CrySystem/File/CryBufferedFileReader.h>

CSkyStage::CSkyStage(CGraphicsPipeline& graphicsPipeline)
	: CGraphicsPipelineStage(graphicsPipeline)
	, m_skyDomeTextureLastTimeStamp(-1)
	, m_pSkyDomeTextureMie(NULL)
	, m_pSkyDomeTextureRayleigh(NULL)
	, m_numStars(0)
	, m_pStarMesh(NULL)
	, m_skyPass(&graphicsPipeline)
{}

void CSkyStage::Init()
{
	// Create HDR Sky Dome textures
	CreateSkyDomeTextures(SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight);
	if (!LoadStarsData())
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "Failed to load stars data");
	}
	m_skyDomeTextureLastTimeStamp = -1;
}

void CSkyStage::Update()
{
}

void CSkyStage::CreateSkyDomeTextures(int32 width, int32 height)
{
	SAFE_RELEASE(m_pSkyDomeTextureMie);
	SAFE_RELEASE(m_pSkyDomeTextureRayleigh);

	const uint32 creationFlags = FT_STATE_CLAMP | FT_NOMIPS | FT_DONT_STREAM;

	std::string mieTextureName = "$SkyDomeTextureMie" + m_graphicsPipeline.GetUniqueIdentifierName();
	std::string rayleighTextureName = "$SkyDomeTextureRayleigh" + m_graphicsPipeline.GetUniqueIdentifierName();
	m_pSkyDomeTextureMie = CTexture::GetOrCreateTextureObject(mieTextureName.c_str(), width, height, 1, eTT_2D, creationFlags, eTF_R16G16B16A16F);
	m_pSkyDomeTextureRayleigh = CTexture::GetOrCreateTextureObject(rayleighTextureName.c_str(), width, height, 1, eTT_2D, creationFlags, eTF_R16G16B16A16F);

	m_pSkyDomeTextureMie->Create2DTexture(width, height, 1, creationFlags, nullptr, eTF_R16G16B16A16F);
	m_pSkyDomeTextureRayleigh->Create2DTexture(width, height, 1, creationFlags, nullptr, eTF_R16G16B16A16F);
}

bool CSkyStage::LoadStarsData()
{
	const uint32 c_fileTag(0x52415453);       // "STAR"
	const uint32 c_fileVersion(0x00010001);
	const char c_fileName[] = "%ENGINE%/engineassets/sky/stars.dat";
	
	CCryBufferedFileReader file;
	if (file.Open(c_fileName, "rb"))
	{
		// read and validate header
		size_t itemsRead(0);
		uint32 fileTag(0);
		itemsRead = file.ReadType(&fileTag);
		if (itemsRead != 1 || fileTag != c_fileTag)
		{
			return false;
		}

		uint32 fileVersion(0);
		itemsRead = file.ReadType(&fileVersion);
		if (itemsRead != 1 || fileVersion != c_fileVersion)
		{
			return false;
		}

		// read in stars
		file.ReadType(&m_numStars);

		SVF_P3S_C4B_T2S* pData(new SVF_P3S_C4B_T2S[6 * m_numStars]);

		for (unsigned int i(0); i < m_numStars; ++i)
		{
			float ra(0);
			file.ReadType(&ra);

			float dec(0);
			file.ReadType(&dec);

			uint8 r(0);
			file.ReadType(&r);

			uint8 g(0);
			file.ReadType(&g);

			uint8 b(0);
			file.ReadType(&b);

			uint8 mag(0);
			file.ReadType(&mag);

			Vec3 v;
			v.x = -cosf(DEG2RAD(dec)) * sinf(DEG2RAD(ra * 15.0f));
			v.y = cosf(DEG2RAD(dec)) * cosf(DEG2RAD(ra * 15.0f));
			v.z = sinf(DEG2RAD(dec));

			for (int k = 0; k < 6; k++)
			{
				pData[6 * i + k].xyz = v;
				pData[6 * i + k].color.dcolor = (mag << 24) + (b << 16) + (g << 8) + r;
			}
		}

		m_pStarMesh = gRenDev->CreateRenderMeshInitialized(pData, 6 * m_numStars, EDefaultInputLayouts::P3S_C4B_T2S, 0, 0, prtTriangleList, "Stars", "Stars");

		delete[] pData;

		// check if we read entire file
		const size_t curPos(file.GetPosition());
		file.Seek(0, SEEK_END);
		const size_t endPos(file.GetPosition());
		
		return (curPos == endPos);
	}
	return false;
}

void CSkyStage::SetSkyParameters()
{
	auto threadID = gRenDev->GetRenderThreadID();
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	// SkyBox
	{
		static CCryNameR skyBoxParamName("SkyDome_SkyBoxParams");
		const float skyBoxAngle = DEG2RAD(rd->m_p3DEngineCommon[threadID].m_SkyInfo.m_fSkyBoxAngle);
		const float skyBoxScaling = rd->m_p3DEngineCommon[threadID].m_SkyInfo.m_fSkyBoxStretching;
		const float skyBoxMultiplier = rd->m_p3DEngineCommon[threadID].m_SkyInfo.m_fSkyBoxMultiplier; // Deprecated: E3DPARAM_SKYBOX_MULTIPLIER
		const Vec4 skyBoxParams(skyBoxAngle, skyBoxScaling, skyBoxMultiplier, 0.f);
		m_skyPass.SetConstant(skyBoxParamName, skyBoxParams, eHWSC_Pixel);

		static CCryNameR skyBoxExposureName("SkyDome_SkyBoxExposure");
		const Vec3& skyBoxExposure = rd->m_p3DEngineCommon[threadID].m_SkyInfo.m_vSkyBoxEmittance;
		m_skyPass.SetConstant(skyBoxExposureName, Vec4(skyBoxExposure, 1.0f), eHWSC_Pixel);

		static CCryNameR skyBoxOpacityName("SkyDome_SkyBoxOpacity");
		const Vec3& skyBoxOpacity = rd->m_p3DEngineCommon[threadID].m_SkyInfo.m_vSkyBoxFilter;
		m_skyPass.SetConstant(skyBoxOpacityName, Vec4(skyBoxOpacity, 1.0f), eHWSC_Pixel);
	}

	if (rd->m_p3DEngineCommon[threadID].m_SkyInfo.m_bApplySkyDome)
	{
		SetHDRSkyParameters();
	}
}

void CSkyStage::SetHDRSkyParameters()
{
	I3DEngine* const p3DEngine = gEnv->p3DEngine;

	// Day sky
	{
		static CCryNameR mieName("SkyDome_PartialMieInScatteringConst");
		static CCryNameR rayleighName("SkyDome_PartialRayleighInScatteringConst");
		static CCryNameR sunDirectionName("SkyDome_SunDirection");
		static CCryNameR phaseName("SkyDome_PhaseFunctionConstants");

		auto renderParams = p3DEngine->GetSkyLightRenderParams();
		m_skyPass.SetConstant(mieName, renderParams->m_partialMieInScatteringConst, eHWSC_Pixel);
		m_skyPass.SetConstant(rayleighName, renderParams->m_partialRayleighInScatteringConst, eHWSC_Pixel);
		m_skyPass.SetConstant(sunDirectionName, renderParams->m_sunDirection, eHWSC_Pixel);
		m_skyPass.SetConstant(phaseName, renderParams->m_phaseFunctionConsts, eHWSC_Pixel);
	}

	// Night sky
	{
		static CCryNameR nightColBaseName("SkyDome_NightSkyColBase");
		static CCryNameR nightColDeltaName("SkyDome_NightSkyColDelta");
		static CCryNameR nightZenithColShiftName("SkyDome_NightSkyZenithColShift");

		Vec3 nightSkyHorizonCol;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_HORIZON_COLOR, nightSkyHorizonCol);
		Vec3 nightSkyZenithCol;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_COLOR, nightSkyZenithCol);
		float nightSkyZenithColShift(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_SHIFT));
		const float minNightSkyZenithGradient(-0.1f);

		Vec4 colBase(nightSkyHorizonCol, 0);
		Vec4 colDelta(nightSkyZenithCol - nightSkyHorizonCol, 0);
		Vec4 zenithColShift(1.0f / (nightSkyZenithColShift - minNightSkyZenithGradient), -minNightSkyZenithGradient / (nightSkyZenithColShift - minNightSkyZenithGradient), 0, 0);

		m_skyPass.SetConstant(nightColBaseName, colBase, eHWSC_Pixel);
		m_skyPass.SetConstant(nightColDeltaName, colDelta, eHWSC_Pixel);
		m_skyPass.SetConstant(nightZenithColShiftName, zenithColShift, eHWSC_Pixel);
	}

	// Moon
	{
		static CCryNameR moonTexGenRightName("SkyDome_NightMoonTexGenRight");
		static CCryNameR moonTexGenUpName("SkyDome_NightMoonTexGenUp");
		static CCryNameR moonDirSizeName("SkyDome_NightMoonDirSize");
		static CCryNameR moonColorName("SkyDome_NightMoonColor");
		static CCryNameR moonInnerScaleName("SkyDome_NightMoonInnerCoronaColorScale");
		static CCryNameR moonOuterScaleName("SkyDome_NightMoonOuterCoronaColorScale");

		Vec3 nightMoonColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_COLOR, nightMoonColor);
		Vec4 moonColor(nightMoonColor, 0);

		Vec3 nightMoonInnerCoronaColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR, nightMoonInnerCoronaColor);
		float nightMoonInnerCoronaScale(1.0f + 1000.0f * p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE));
		Vec4 moonInnerCoronaColorScale(nightMoonInnerCoronaColor, nightMoonInnerCoronaScale);

		Vec3 nightMoonOuterCoronaColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR, nightMoonOuterCoronaColor);
		float nightMoonOuterCoronaScale(1.0f + 1000.0f * p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE));
		Vec4 moonOuterCoronaColorScale(nightMoonOuterCoronaColor, nightMoonOuterCoronaScale);

		m_skyPass.SetConstant(moonColorName, moonColor, eHWSC_Pixel);
		m_skyPass.SetConstant(moonInnerScaleName, moonInnerCoronaColorScale, eHWSC_Pixel);
		m_skyPass.SetConstant(moonOuterScaleName, moonOuterCoronaColorScale, eHWSC_Pixel);

		Vec3 mr;
		p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, mr);
		float moonLati = -gf_PI + gf_PI * mr.x / 180.0f;
		float moonLong = 0.5f * gf_PI - gf_PI * mr.y / 180.0f;

		float sinLonR = sinf(-0.5f * gf_PI);
		float cosLonR = cosf(-0.5f * gf_PI);
		float sinLatR = sinf(moonLati + 0.5f * gf_PI);
		float cosLatR = cosf(moonLati + 0.5f * gf_PI);
		Vec3 moonTexGenRight(sinLonR* cosLatR, sinLonR* sinLatR, cosLonR);
		m_paramMoonTexGenRight = Vec4(moonTexGenRight, 0);
		m_skyPass.SetConstant(moonTexGenRightName, m_paramMoonTexGenRight, eHWSC_Pixel);

		float sinLonU = sinf(moonLong + 0.5f * gf_PI);
		float cosLonU = cosf(moonLong + 0.5f * gf_PI);
		float sinLatU = sinf(moonLati);
		float cosLatU = cosf(moonLati);
		Vec3 moonTexGenUp(sinLonU* cosLatU, sinLonU* sinLatU, cosLonU);
		m_paramMoonTexGenUp = Vec4(moonTexGenUp, 0);
		m_skyPass.SetConstant(moonTexGenUpName, m_paramMoonTexGenUp, eHWSC_Pixel);

		Vec3 nightMoonDirection;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, nightMoonDirection);
		float nightMoonSize(25.0f - 24.0f * clamp_tpl(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_SIZE), 0.0f, 1.0f));
		m_paramMoonDirSize = Vec4(nightMoonDirection, true ? nightMoonSize : 9999.0f);
		m_skyPass.SetConstant(moonDirSizeName, m_paramMoonDirSize, eHWSC_Pixel);
	}
}

static void FillSkyTextureData(CTexture* pTexture, const void* pData, const uint32 width, const uint32 height, const uint32 pitch)
{
	assert(pTexture && pTexture->GetWidth() == width && pTexture->GetHeight() == height && pTexture->GetDevTexture());
	const SResourceMemoryAlignment layout = SResourceMemoryAlignment::Linear<CryHalf4>(width, height);
	CRY_ASSERT(pitch == layout.rowStride);

	GPUPIN_DEVICE_TEXTURE(gcpRendD3D->GetPerformanceDeviceContext(), pTexture->GetDevTexture());
	GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pData, pTexture->GetDevTexture(), layout);
}

void CSkyStage::Execute(CTexture* pColorTex, CTexture* pDepthTex)
{
	FUNCTION_PROFILER_RENDERER();
	PROFILE_LABEL_SCOPE("SKY_PASS");

	I3DEngine* const p3DEngine = gEnv->p3DEngine;
	auto threadID = gRenDev->GetRenderThreadID();
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	const bool isProcedualSky = rd->m_p3DEngineCommon[threadID].m_SkyInfo.m_bApplySkyDome;
	const bool isOverlayedSky = rd->m_p3DEngineCommon[threadID].m_SkyInfo.m_bApplySkyBox;

	CRenderView* const pRenderView = RenderView();
	const bool applyFog = pRenderView->IsGlobalFogEnabled() && !(m_graphicsPipeline.IsPipelineFlag(CGraphicsPipeline::EPipelineFlags::NO_SHADER_FOG));

	CTexture* pSkyDomeTextureMie = CRendererResources::s_ptexBlack;
	CTexture* pSkyDomeTextureRayleigh = CRendererResources::s_ptexBlack;
	CTexture* pSkyDomeTex = CRendererResources::s_ptexBlack;

	if (isProcedualSky)
	{
		const SSkyLightRenderParams* const pRenderParams = p3DEngine->GetSkyLightRenderParams();

		if (m_skyDomeTextureLastTimeStamp != pRenderParams->m_skyDomeTextureTimeStamp)
		{
			FillSkyTextureData(m_pSkyDomeTextureMie, pRenderParams->m_pSkyDomeTextureDataMie,
			                   SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight, pRenderParams->m_skyDomeTexturePitch);
			FillSkyTextureData(m_pSkyDomeTextureRayleigh, pRenderParams->m_pSkyDomeTextureDataRayleigh,
			                   SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight, pRenderParams->m_skyDomeTexturePitch);

			// Update time stamp of last update
			m_skyDomeTextureLastTimeStamp = pRenderParams->m_skyDomeTextureTimeStamp;
		}

		pSkyDomeTextureMie = m_pSkyDomeTextureMie;
		pSkyDomeTextureRayleigh = m_pSkyDomeTextureRayleigh;
	}

	if (isOverlayedSky)
	{
		pSkyDomeTex = rd->m_p3DEngineCommon[threadID].m_SkyInfo.m_pSkyBoxTexture.get();
	}

	const string moonTextureName = p3DEngine->GetMoonTextureName();
	const bool hasMoonTexture = !moonTextureName.empty();
	CTexture* pSkyMoonTex = hasMoonTexture
		? CTexture::ForName(moonTextureName, FT_DONT_STREAM, eTF_Unknown)
		: CRendererResources::s_ptexBlack;

	uint64 rtMask = 0;
	rtMask |= isProcedualSky ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
	rtMask |= isOverlayedSky ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;
	if (rtMask == 0) // in case no sky texture is provided, draw black sky
		rtMask = g_HWSR_MaskBit[HWSR_SAMPLE1];
	rtMask |= applyFog ? g_HWSR_MaskBit[HWSR_FOG] : 0;

	// TODO: streaming invalidation
	if (m_skyPass.IsDirty(rtMask, m_skyDomeTextureLastTimeStamp,
	                      pSkyMoonTex->GetTextureID(), pSkyDomeTex->GetTextureID(), pDepthTex->GetTextureID()))
	{
		const SSamplerState samplerDescLinearWrapU(FILTER_LINEAR, eSamplerAddressMode_Wrap, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0);
		const SamplerStateHandle samplerStateLinearWrapU = GetDeviceObjectFactory().GetOrCreateSamplerStateHandle(samplerDescLinearWrapU);

		static CCryNameTSCRC techSkyPass("SkyPass");
		m_skyPass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_skyPass.SetTechnique(CShaderMan::s_ShaderStars, techSkyPass, rtMask);
		m_skyPass.SetRequirePerViewConstantBuffer(true);
		m_skyPass.SetRenderTarget(0, pColorTex);
		m_skyPass.SetDepthTarget(pDepthTex);
		m_skyPass.SetState(GS_DEPTHFUNC_EQUAL);
		m_skyPass.SetTexture(0, pSkyDomeTex);
		m_skyPass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

		m_skyPass.SetTexture(1, pSkyDomeTextureMie);
		m_skyPass.SetTexture(2, pSkyDomeTextureRayleigh);
		m_skyPass.SetTexture(3, pSkyMoonTex);
		m_skyPass.SetSampler(1, samplerStateLinearWrapU);
	}

	m_skyPass.BeginConstantUpdate();
	SetSkyParameters();
	m_skyPass.Execute();

	// Stars
	if (isProcedualSky)
	{
		const float starIntensity = gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_STAR_INTENSITY);

		if (starIntensity > 1e-3f)
		{
			D3DViewPort viewport = RenderViewportToD3D11Viewport(RenderView()->GetViewport());
			if (pRenderView->IsRecursive())
			{
				viewport = { 0.f, 0.f, float(pColorTex->GetWidth()), float(pColorTex->GetHeight()), 0.0f, 1.0f };
			}
			CRY_ASSERT(pColorTex->GetWidth() == viewport.Width && pColorTex->GetHeight() == viewport.Height);

			m_starsPass.SetRenderTarget(0, pColorTex);
			m_starsPass.SetDepthTarget(pDepthTex);
			m_starsPass.SetViewport(viewport);
			m_starsPass.BeginAddingPrimitives();

			const bool bReverseDepth = true;
			const uint64 rtMask = bReverseDepth ? g_HWSR_MaskBit[HWSR_REVERSE_DEPTH] : 0;
			const int32 depthState = bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL;

			static CCryNameTSCRC techStars("Stars");
			m_starsPrimitive.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants);
			m_starsPrimitive.SetTechnique(CShaderMan::s_ShaderStars, techStars, rtMask);
			m_starsPrimitive.SetRenderState(depthState | GS_BLSRC_ONE | GS_BLDST_ONE);
			m_starsPrimitive.SetCullMode(eCULL_None);

			CRenderMesh* pStarMesh = static_cast<CRenderMesh*>(m_pStarMesh.get());
			buffer_handle_t hVertexStream = pStarMesh->_GetVBStream(VSF_GENERAL);

			m_starsPrimitive.SetCustomVertexStream(hVertexStream, pStarMesh->_GetVertexFormat(), pStarMesh->GetStreamStride(VSF_GENERAL));
			m_starsPrimitive.SetDrawInfo(eptTriangleList, 0, 0, 6 * m_numStars);
			m_starsPrimitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_graphicsPipeline.GetMainViewConstantBuffer(), EShaderStage_Vertex);
			m_starsPrimitive.Compile(m_starsPass);

			{
				static CCryNameR nameMoonTexGenRight("SkyDome_NightMoonTexGenRight");
				static CCryNameR nameMoonTexGenUp("SkyDome_NightMoonTexGenUp");
				static CCryNameR nameMoonDirSize("SkyDome_NightMoonDirSize");
				static CCryNameR nameStarSize("SkyDome_StarSize");
				static CCryNameR nameStarIntensity("SkyDome_StarIntensity");

				m_starsPrimitive.GetConstantManager().BeginNamedConstantUpdate();

				m_starsPrimitive.GetConstantManager().SetNamedConstant(nameMoonTexGenRight, m_paramMoonTexGenRight, eHWSC_Vertex);
				m_starsPrimitive.GetConstantManager().SetNamedConstant(nameMoonTexGenUp, m_paramMoonTexGenUp, eHWSC_Vertex);
				m_starsPrimitive.GetConstantManager().SetNamedConstant(nameMoonDirSize, m_paramMoonDirSize, eHWSC_Vertex);

				const float size = 5.0f * min(1.f, min(viewport.Width / 1280.f, viewport.Height / 720.f));
				float flickerTime(gEnv->pTimer->GetCurrTime());
				Vec4 paramStarSize(size / (float)viewport.Width, size / (float)viewport.Height, 0, flickerTime * 0.5f);
				m_starsPrimitive.GetConstantManager().SetNamedConstant(nameStarSize, paramStarSize, eHWSC_Vertex);
				Vec4 paramStarIntensity(starIntensity* min(1.0f, size), 0, 0, 0);
				m_starsPrimitive.GetConstantManager().SetNamedConstant(nameStarIntensity, paramStarIntensity, eHWSC_Pixel);

				m_starsPrimitive.GetConstantManager().EndNamedConstantUpdate(&m_starsPass.GetViewport(), pRenderView);

				m_starsPass.AddPrimitive(&m_starsPrimitive);
				m_starsPass.Execute();
			}
		}
	}
}

void CSkyStage::ExecuteMinimum(CTexture* pColorTex, CTexture* pDepthTex)
{
	FUNCTION_PROFILER_RENDERER();
	PROFILE_LABEL_SCOPE("SKY_PASS_MINIMUM");

	CRenderView* const pRenderView = RenderView();
	const bool applyFog = pRenderView->IsGlobalFogEnabled() && !(m_graphicsPipeline.IsPipelineFlag(CGraphicsPipeline::EPipelineFlags::NO_SHADER_FOG));

	CTexture* pSkyDomeTextureMie = CRendererResources::s_ptexBlack;
	CTexture* pSkyDomeTextureRayleigh = CRendererResources::s_ptexBlack;

	CTexture* pSkyDomeTex = CRendererResources::s_ptexBlack;
	CTexture* pSkyMoonTex = CRendererResources::s_ptexBlack;

	uint64 rtMask = 0;
	rtMask = g_HWSR_MaskBit[HWSR_SAMPLE1];
	rtMask |= applyFog ? g_HWSR_MaskBit[HWSR_FOG] : 0;

	if (m_skyPass.IsDirty(rtMask, m_skyDomeTextureLastTimeStamp,
		pSkyMoonTex->GetTextureID(), pSkyDomeTex->GetTextureID(), pDepthTex->GetTextureID()))
	{
		const SSamplerState samplerDescLinearWrapU(FILTER_LINEAR, eSamplerAddressMode_Wrap, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0);
		const SamplerStateHandle samplerStateLinearWrapU = GetDeviceObjectFactory().GetOrCreateSamplerStateHandle(samplerDescLinearWrapU);

		static CCryNameTSCRC techSkyPass("SkyPass");
		m_skyPass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_skyPass.SetTechnique(CShaderMan::s_ShaderStars, techSkyPass, rtMask);
		m_skyPass.SetRequirePerViewConstantBuffer(true);
		m_skyPass.SetRenderTarget(0, pColorTex);
		m_skyPass.SetDepthTarget(pDepthTex);
		m_skyPass.SetState(GS_DEPTHFUNC_EQUAL);
		m_skyPass.SetTexture(0, pSkyDomeTex);
		m_skyPass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

		m_skyPass.SetTexture(1, pSkyDomeTextureMie);
		m_skyPass.SetTexture(2, pSkyDomeTextureRayleigh);
		m_skyPass.SetTexture(3, pSkyMoonTex);
		m_skyPass.SetSampler(1, samplerStateLinearWrapU);
	}

	m_skyPass.BeginConstantUpdate();
	m_skyPass.Execute();
}
