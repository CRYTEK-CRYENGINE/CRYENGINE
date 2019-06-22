// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/IFlares.h>
#include "RendererResources.h"
#include "RendererCVars.h"
#include "Renderer.h"
#include "Textures/Texture.h"

#include "../XRenderD3D9/D3DDeferredShading.h"
#include "../XRenderD3D9/D3DPostProcess.h"

#include "RendElements/FlareSoftOcclusionQuery.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SRenderTargetPool::AddRenderTarget(uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Format Format, float fPriority, const char* szName, CTexture** pStorage, uint32 nFlags, int32 nCustomID, bool bDynamicTex)
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

bool SRenderTargetPool::CreateRenderTargetList()
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

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

SPixFormatSupport CRendererResources::s_hwTexFormatSupport;

CTexture* CRendererResources::s_ptexNoTexture;
CTexture* CRendererResources::s_ptexNoTextureCM;
CTexture* CRendererResources::s_ptexWhite;
CTexture* CRendererResources::s_ptexGray;
CTexture* CRendererResources::s_ptexMedian;
CTexture* CRendererResources::s_ptexBlack;
CTexture* CRendererResources::s_ptexBlackAlpha;
CTexture* CRendererResources::s_ptexBlackCM;
CTexture* CRendererResources::s_ptexDefaultProbeCM;
CTexture* CRendererResources::s_ptexDefaultMergedDetail;
CTexture* CRendererResources::s_ptexFlatBump;
#if !defined(_RELEASE)
CTexture* CRendererResources::s_ptexMipMapDebug;
CTexture* CRendererResources::s_ptexColorBlue;
CTexture* CRendererResources::s_ptexColorCyan;
CTexture* CRendererResources::s_ptexColorGreen;
CTexture* CRendererResources::s_ptexColorPurple;
CTexture* CRendererResources::s_ptexColorRed;
CTexture* CRendererResources::s_ptexColorWhite;
CTexture* CRendererResources::s_ptexColorYellow;
CTexture* CRendererResources::s_ptexColorOrange;
CTexture* CRendererResources::s_ptexColorMagenta;
#endif
CTexture* CRendererResources::s_ptexPaletteDebug;
CTexture* CRendererResources::s_ptexPaletteTexelsPerMeter;
CTexture* CRendererResources::s_ptexIconShaderCompiling;
CTexture* CRendererResources::s_ptexIconStreaming;
CTexture* CRendererResources::s_ptexIconStreamingTerrainTexture;
CTexture* CRendererResources::s_ptexIconNavigationProcessing;
CTexture* CRendererResources::s_ptexMipColors_Diffuse;
CTexture* CRendererResources::s_ptexMipColors_Bump;
CTexture* CRendererResources::s_ptexShadowJitterMap;
CTexture* CRendererResources::s_ptexEnvironmentBRDF;
CTexture* CRendererResources::s_ptexScreenNoiseMap;
CTexture* CRendererResources::s_ptexDissolveNoiseMap;
CTexture* CRendererResources::s_ptexNoise3D;
CTexture* CRendererResources::s_ptexGrainFilterMap;
CTexture* CRendererResources::s_ptexFilmGrainMap;
CTexture* CRendererResources::s_ptexVignettingMap;
CTexture* CRendererResources::s_ptexAOJitter;
CTexture* CRendererResources::s_ptexAOVOJitter;
CTexture* CRendererResources::s_ptexFromRE[8];
CTexture* CRendererResources::s_ptexShadowID[8];
CTexture* CRendererResources::s_ptexFromRE_FromContainer[2];
CTexture* CRendererResources::s_ptexFromObj;
CTexture* CRendererResources::s_ptexRT_2D;
CTexture* CRendererResources::s_ptexNormalsFitting;
CTexture* CRendererResources::s_ptexPerlinNoiseMap;
CTexture* CRendererResources::s_ptexLTC1;
CTexture* CRendererResources::s_ptexLTC2;
CTexture* CRendererResources::s_ptexWindGrid;


// Post-process related textures
CTexture* CRendererResources::s_ptexDisplayTargetScaledPrev;

CTexture* CRendererResources::s_ptexWaterOcean;
CTexture* CRendererResources::s_ptexWaterVolumeTemp[2];
CTexture* CRendererResources::s_ptexWaterVolumeDDN;

CTexture* CRendererResources::s_ptexFarPlane;
CTexture* CRendererResources::s_ptexCloudsLM;

CTexture* CRendererResources::s_ptexHDRAdaptedLuminanceCur[8];
int CRendererResources::s_nCurLumTextureIndex;
CTexture* CRendererResources::s_ptexCurLumTexture;
CTexture* CRendererResources::s_ptexHDRToneMaps[NUM_HDR_TONEMAP_TEXTURES];
CTexture* CRendererResources::s_ptexHDRMeasuredLuminanceDummy;
CTexture* CRendererResources::s_ptexColorChart;

CTexture* CRendererResources::s_ptexFlaresOcclusionRing[MAX_OCCLUSION_READBACK_TEXTURES] = { NULL };
CTexture* CRendererResources::s_ptexFlaresGather = NULL;

CTexture* CRendererResources::s_pTexNULL = 0;

CTexture* CRendererResources::s_ptexVolumetricFog = NULL;

SEnvTexture CRendererResources::s_EnvTexts[MAX_ENVTEXTURES];

TArray<SEnvTexture> CRendererResources::s_CustomRT_2D;

TArray<CTexture> CRendererResources::s_ShaderTemplates(EFTT_MAX);
bool CRendererResources::s_ShaderTemplatesInitialized = false;

ETEX_Format CRendererResources::s_eTFZ = eTF_R32F;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CRendererResources::m_bLoadedSystem = false;

void CRendererResources::UnloadDefaultSystemTextures(bool bFinalRelease)
{
	// Keep these valid - if any textures failed to load and also leaked across the shutdown,
	// they'll be left with a dangling dev texture pointer otherwise.
	//SAFE_RELEASE_FORCE(s_ptexNoTexture);
	//SAFE_RELEASE_FORCE(s_ptexNoTextureCM);
	//SAFE_RELEASE_FORCE(s_ptexWhite);
	//SAFE_RELEASE_FORCE(s_ptexGray);
	//SAFE_RELEASE_FORCE(s_ptexBlack);
	//SAFE_RELEASE_FORCE(s_ptexBlackAlpha);
	//SAFE_RELEASE_FORCE(s_ptexBlackCM);
	//SAFE_RELEASE_FORCE(s_ptexFlatBump);
#if !defined(_RELEASE)
	//SAFE_RELEASE_FORCE(s_ptexDefaultMergedDetail);
	//SAFE_RELEASE_FORCE(s_ptexMipMapDebug);
	//SAFE_RELEASE_FORCE(s_ptexColorBlue);
	//SAFE_RELEASE_FORCE(s_ptexColorCyan);
	//SAFE_RELEASE_FORCE(s_ptexColorGreen);
	//SAFE_RELEASE_FORCE(s_ptexColorPurple);
	//SAFE_RELEASE_FORCE(s_ptexColorRed);
	//SAFE_RELEASE_FORCE(s_ptexColorWhite);
	//SAFE_RELEASE_FORCE(s_ptexColorYellow);
	//SAFE_RELEASE_FORCE(s_ptexColorMagenta);
	//SAFE_RELEASE_FORCE(s_ptexColorOrange);
#endif
	//SAFE_RELEASE_FORCE(s_ptexPaletteDebug);
	//SAFE_RELEASE_FORCE(s_ptexShadowJitterMap);
	//SAFE_RELEASE_FORCE(s_ptexEnvironmentBRDF);
	//SAFE_RELEASE_FORCE(s_ptexScreenNoiseMap);
	//SAFE_RELEASE_FORCE(s_ptexDissolveNoiseMap);
	//SAFE_RELEASE_FORCE(s_ptexNoise3D);
	//SAFE_RELEASE_FORCE(s_ptexGrainFilterMap);
	//SAFE_RELEASE_FORCE(s_ptexFilmGrainMap);
	//SAFE_RELEASE_FORCE(s_ptexVignettingMap);
	//SAFE_RELEASE_FORCE(s_ptexAOJitter);
	//SAFE_RELEASE_FORCE(s_ptexAOVOJitter);
	SAFE_RELEASE_FORCE(s_ptexRT_2D);
	SAFE_RELEASE_FORCE(s_ptexCloudsLM);

	//SAFE_RELEASE_FORCE(s_ptexHitAreaRT[0]);
	//SAFE_RELEASE_FORCE(s_ptexHitAreaRT[1]);

	SAFE_RELEASE_FORCE(s_ptexVolumetricFog);

	uint32 i;
	for (i = 0; i < 8; i++)
	{
		//SAFE_RELEASE_FORCE(s_ptexFromRE[i]);
	}
	for (i = 0; i < 8; i++)
	{
		SAFE_RELEASE_FORCE(s_ptexShadowID[i]);
	}

	for (i = 0; i < 2; i++)
	{
		//SAFE_RELEASE_FORCE(s_ptexFromRE_FromContainer[i]);
	}

	SAFE_RELEASE_FORCE(s_ptexColorChart);

	SAFE_RELEASE_FORCE(s_ptexWindGrid);

	for (i = 0; i < MAX_ENVTEXTURES; i++)
	{
		s_EnvTexts[i].Release();
	}
	//m_EnvTexTemp.Release();

	SAFE_RELEASE_FORCE(s_ptexMipColors_Diffuse);
	SAFE_RELEASE_FORCE(s_ptexMipColors_Bump);
	SAFE_RELEASE_FORCE(s_ptexFarPlane);

	s_CustomRT_2D.Free();
	//s_ShaderTemplates.Free();

	// release targets pools
	SDynTexture::ShutDown();

	//ReleaseSystemTargets();

	m_bLoadedSystem = false;
}

void CRendererResources::LoadDefaultSystemTextures()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	char str[256];
	int i;

	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Engine textures");

	if (!m_bLoadedSystem)
	{
		m_bLoadedSystem = true;

		// Textures loaded directly from file
		struct
		{
			CTexture*&  pTexture;
			const char* szFileName;
			uint32      flags;
		}
		texturesFromFile[] =
		{
			{ s_ptexWhite,                       "%ENGINE%/EngineAssets/Textures/White.dds",                         FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexBlack,                       "%ENGINE%/EngineAssets/Textures/Black.dds",                         FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexBlackAlpha,                  "%ENGINE%/EngineAssets/Textures/BlackAlpha.dds",                    FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexBlackCM,                     "%ENGINE%/EngineAssets/Textures/BlackCM.dds",                       FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexGray,                        "%ENGINE%/EngineAssets/Textures/Grey.dds",                          FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexDefaultMergedDetail,         "%ENGINE%/EngineAssets/Textures/GreyAlpha.dds",                     FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexDefaultProbeCM,              "%ENGINE%/EngineAssets/Shading/defaultProbe_cm.tif",                FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexFlatBump,                    "%ENGINE%/EngineAssets/Textures/White_ddn.dds",                     FT_DONT_RELEASE | FT_DONT_STREAM | FT_TEX_NORMAL_MAP },
			{ s_ptexPaletteDebug,                "%ENGINE%/EngineAssets/Textures/palletteInst.dds",                  FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexPaletteTexelsPerMeter,       "%ENGINE%/EngineAssets/Textures/TexelsPerMeterGrad.dds",            FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexIconShaderCompiling,         "%ENGINE%/EngineAssets/Icons/ShaderCompiling.tif",                  FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexIconStreaming,               "%ENGINE%/EngineAssets/Icons/Streaming.tif",                        FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexIconStreamingTerrainTexture, "%ENGINE%/EngineAssets/Icons/StreamingTerrain.tif",                 FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexIconNavigationProcessing,    "%ENGINE%/EngineAssets/Icons/NavigationProcessing.tif",             FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexShadowJitterMap,             "%ENGINE%/EngineAssets/Textures/rotrandom.dds",                     FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexEnvironmentBRDF,             "%ENGINE%/EngineAssets/Shading/environmentBRDF.tif",                FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexScreenNoiseMap,              "%ENGINE%/EngineAssets/Textures/JumpNoiseHighFrequency_x27y19.dds", FT_DONT_RELEASE | FT_DONT_STREAM | FT_NOMIPS },
			{ s_ptexDissolveNoiseMap,            "%ENGINE%/EngineAssets/Textures/noise.dds",                         FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexNoise3D,                     "%ENGINE%/EngineAssets/Textures/noise3d.dds",                       FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexGrainFilterMap,              "%ENGINE%/EngineAssets/ScreenSpace/grain_bayer_mul.tif",            FT_DONT_RELEASE | FT_DONT_STREAM | FT_NOMIPS },
			{ s_ptexFilmGrainMap,                "%ENGINE%/EngineAssets/ScreenSpace/film_grain.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM | FT_NOMIPS },
			{ s_ptexVignettingMap,               "%ENGINE%/EngineAssets/Shading/vignetting.tif",                     FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexAOJitter,                    "%ENGINE%/EngineAssets/ScreenSpace/PointsOnSphere4x4.tif",          FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexAOVOJitter,                  "%ENGINE%/EngineAssets/ScreenSpace/PointsOnSphereVO4x4.tif",        FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexNormalsFitting,              "%ENGINE%/EngineAssets/ScreenSpace/NormalsFitting.dds",             FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexPerlinNoiseMap,              "%ENGINE%/EngineAssets/Textures/perlinNoise2D.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexLTC1,                        "%ENGINE%/EngineAssets/Shading/ltc_1.dds",                          FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexLTC2,                        "%ENGINE%/EngineAssets/Shading/ltc_2.dds",                          FT_DONT_RELEASE | FT_DONT_STREAM },

#if !defined(_RELEASE)
			{ s_ptexNoTexture,                   "%ENGINE%/EngineAssets/TextureMsg/ReplaceMe.tif",                   FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexNoTextureCM,                 "%ENGINE%/EngineAssets/TextureMsg/ReplaceMeCM.tif",                 FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexMipMapDebug,                 "%ENGINE%/EngineAssets/TextureMsg/MipMapDebug.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexColorBlue,                   "%ENGINE%/EngineAssets/TextureMsg/color_Blue.dds",                  FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexColorCyan,                   "%ENGINE%/EngineAssets/TextureMsg/color_Cyan.dds",                  FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexColorGreen,                  "%ENGINE%/EngineAssets/TextureMsg/color_Green.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexColorPurple,                 "%ENGINE%/EngineAssets/TextureMsg/color_Purple.dds",                FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexColorRed,                    "%ENGINE%/EngineAssets/TextureMsg/color_Red.dds",                   FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexColorWhite,                  "%ENGINE%/EngineAssets/TextureMsg/color_White.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexColorYellow,                 "%ENGINE%/EngineAssets/TextureMsg/color_Yellow.dds",                FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexColorOrange,                 "%ENGINE%/EngineAssets/TextureMsg/color_Orange.dds",                FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexColorMagenta,                "%ENGINE%/EngineAssets/TextureMsg/color_Magenta.dds",               FT_DONT_RELEASE | FT_DONT_STREAM },
#else
			{ s_ptexNoTexture,                   "%ENGINE%/EngineAssets/Textures/Grey.dds",                          FT_DONT_RELEASE | FT_DONT_STREAM },
			{ s_ptexNoTextureCM,                 "%ENGINE%/EngineAssets/Shading/defaultProbe_cm.tif",                FT_DONT_RELEASE | FT_DONT_STREAM },
#endif
		};

		for (int t = 0; t < CRY_ARRAY_COUNT(texturesFromFile); ++t)
		{
			if (!texturesFromFile[t].pTexture)
			{
				texturesFromFile[t].pTexture = CTexture::ForName(texturesFromFile[t].szFileName, texturesFromFile[t].flags, eTF_Unknown);

				if (texturesFromFile[t].pTexture->GetFlags() & FT_FAILED)
				{
					CryFatalError("Default system texture \"%s\" failed to load, please make sure the engine.pak or the engine folder is available.", texturesFromFile[t].szFileName);
				}

				if (!texturesFromFile[t].pTexture || !texturesFromFile[t].pTexture->IsLoaded() || !texturesFromFile[t].pTexture->GetDevTexture())
				{
					CryFatalError("Can't open %s texture file.", texturesFromFile[t].szFileName);
				}
			}
		}

		// Associate dummy NULL-resource with s_pTexNULL
		s_pTexNULL = new CTexture(FT_DONT_RELEASE | FT_DONT_STREAM, Clr_Transparent, CDeviceTexture::Associate(s_ptexBlackAlpha->GetLayout(), nullptr));

		// Default Template textures
		int nRTFlags = FT_DONT_RELEASE | FT_DONT_STREAM | FT_STATE_CLAMP | FT_USAGE_RENDERTARGET;
		s_ptexMipColors_Diffuse = CTexture::GetOrCreateTextureObject("$MipColors_Diffuse", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_MIPCOLORS_DIFFUSE);
		s_ptexMipColors_Bump = CTexture::GetOrCreateTextureObject("$MipColors_Bump", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_MIPCOLORS_BUMP);

		s_ptexRT_2D = CTexture::GetOrCreateTextureObject("$RT_2D", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_RT_2D);

		//s_ptexHitAreaRT[0] = CTexture::CreateTextureObject("$HitEffectAccumBuffRT_0", 128, 128, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		//s_ptexHitAreaRT[1] = CTexture::CreateTextureObject("$HitEffectAccumBuffRT_1", 128, 128, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

		s_ptexWindGrid = CTexture::GetOrCreateTextureObject("WindGrid", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_WINDGRID);
		if (!CTexture::IsTextureExist(s_ptexWindGrid))
		{
			s_ptexWindGrid->Create2DTexture(256, 256, 1, FT_DONT_RELEASE | FT_DONT_STREAM, nullptr, eTF_R16G16F);
		}

		s_ptexMedian = CTexture::GetOrCreateTextureObject("$MedianColor", 8, 8, 1, eTT_2D, FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		if (!CTexture::IsTextureExist(s_ptexMedian))
		{
			s_ptexMedian->CreateRenderTarget(eTF_R8G8B8A8, Clr_Median);
			CClearSurfacePass::Execute(s_ptexMedian, Clr_Median);
		}

		s_ptexFarPlane = CTexture::GetOrCreateTextureObject("$FarPlane", 8, 8, 1, eTT_2D, FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);

		//	if (!s_ptexBackBuffer)
		{
			s_ptexDisplayTargetScaledPrev = CTexture::GetOrCreateTextureObject("$DisplayTarget 1 / 2p", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

			s_ptexFlaresGather = CTexture::GetOrCreateTextureObject("$FlaresGather", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8 /* 1-bit used only */);
			for (i = 0; i < MAX_OCCLUSION_READBACK_TEXTURES; i++)
			{
				cry_sprintf(str, "$FlaresOcclusion_%d", i);
				s_ptexFlaresOcclusionRing[i] = CTexture::GetOrCreateTextureObject(str, 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8);
			}

			// fixme: get texture resolution from CREWaterOcean
			// TODO: make s_ptexWaterVolumeTemp an array texture
			s_ptexWaterOcean = CTexture::GetOrCreateTextureObject("$WaterOceanMap", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_NOMIPS | FT_DONT_STREAM, eTF_Unknown, TO_WATEROCEANMAP);
			s_ptexWaterVolumeTemp[0] = CTexture::GetOrCreateTextureObject("$WaterVolumeTemp_0", 64, 64, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_NOMIPS | FT_DONT_STREAM, eTF_Unknown);
			s_ptexWaterVolumeTemp[1] = CTexture::GetOrCreateTextureObject("$WaterVolumeTemp_1", 64, 64, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_NOMIPS | FT_DONT_STREAM, eTF_Unknown);
			s_ptexWaterVolumeDDN = CTexture::GetOrCreateTextureObject("$WaterVolumeDDN", 64, 64, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, eTF_Unknown, TO_WATERVOLUMEMAP);
		}

		// Create dummy texture object for terrain and clouds lightmap
		s_ptexCloudsLM = CTexture::GetOrCreateTextureObject("$CloudsLM", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_CLOUDS_LM);

		for (i = 0; i < 8; i++)
		{
			cry_sprintf(str, "$FromRE_%d", i);
			if (!s_ptexFromRE[i])
				s_ptexFromRE[i] = CTexture::GetOrCreateTextureObject(str, 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_FROMRE0 + i);
		}

		for (i = 0; i < 8; i++)
		{
			cry_sprintf(str, "$ShadowID_%d", i);
			s_ptexShadowID[i] = CTexture::GetOrCreateTextureObject(str, 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SHADOWID0 + i);
		}

		for (i = 0; i < 2; i++)
		{
			cry_sprintf(str, "$FromRE%d_FromContainer", i);
			if (!s_ptexFromRE_FromContainer[i])
				s_ptexFromRE_FromContainer[i] = CTexture::GetOrCreateTextureObject(str, 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_FROMRE0_FROM_CONTAINER + i);
		}

		s_ptexColorChart = CTexture::GetOrCreateTextureObject("$ColorChart", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_COLORCHART);

		for (i = 0; i < EFTT_MAX; i++)
		{
			new(&s_ShaderTemplates[i])CTexture(FT_DONT_RELEASE);
			s_ShaderTemplates[i].SetCustomID(EFTT_DIFFUSE + i);
			s_ShaderTemplates[i].SetFlags(FT_DONT_RELEASE);
		}
		s_ShaderTemplatesInitialized = true;

		s_ptexVolumetricFog = CTexture::GetOrCreateTextureObject("$VolFogInscattering", 0, 0, 0, eTT_3D, FT_NOMIPS | FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_UNORDERED_ACCESS, eTF_Unknown);
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

ETEX_Format CRendererResources::GetHDRFormat(bool withAlpha, bool lowQuality)
{
	ETEX_Format candidate = eTF_R16G16B16A16F;

	if (!withAlpha && s_hwTexFormatSupport.IsFormatSupported(eTF_R11G11B10F))
	{
		if (CRenderer::CV_r_HDRTexFormat <= 1 && lowQuality)
			candidate = eTF_R11G11B10F;
		if (CRenderer::CV_r_HDRTexFormat <= 0)
			candidate = eTF_R11G11B10F;
	}

	return candidate;
}

ETEX_Format CRendererResources::GetLDRFormat(bool precisionBits)
{
	ETEX_Format candidate = eTF_R8G8B8A8;

	if (precisionBits)
		candidate = eTF_R10G10B10A2;

	return candidate;
}

ETEX_Format CRendererResources::GetDisplayFormat()
{
	ETEX_Format candidate = eTF_R8G8B8A8;

	return candidate;
}

ETEX_Format CRendererResources::GetDepthFormat()
{
	return
		gRenDev->GetDepthBpp() == 32 ? eTF_D32FS8 :
		gRenDev->GetDepthBpp() == 24 ? eTF_D24S8 :
		gRenDev->GetDepthBpp() ==  8 ? eTF_D16S8 : eTF_D16;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CRendererResources::CreateSystemTargets(int resourceWidth, int resourceHeight)
{
	if (!resourceWidth ) resourceWidth =  s_resourceWidth;
	if (!resourceHeight) resourceHeight = s_resourceHeight;
	
	if (!resourceWidth ) resourceWidth  = s_renderWidth;
	if (!resourceHeight) resourceHeight = s_renderHeight;

	CRY_ASSERT(resourceWidth);
	CRY_ASSERT(resourceHeight);

	if (!gcpRendD3D->m_bSystemTargetsInit)
	{
		// Create HDR targets
		CreateHDRMaps(resourceWidth, resourceHeight);

		// Create post effects targets
		CreatePostFXMaps(resourceWidth, resourceHeight);

		s_resourceWidth  = resourceWidth;
		s_resourceHeight = resourceHeight;

		gcpRendD3D->m_bSystemTargetsInit = 1;
	}
}

void CRendererResources::ResizeSystemTargets(int renderWidth, int renderHeight)
{
	int resourceWidth  = s_resourceWidth;
	int resourceHeight = s_resourceHeight;

#if 0
	if (resourceWidth  < renderWidth ||
		resourceHeight < renderHeight)
	{
		resourceWidth  = std::max(resourceWidth , renderWidth);
		resourceHeight = std::max(resourceHeight, renderHeight);
#else
	if (resourceWidth  != renderWidth ||
		resourceHeight != renderHeight)
	{
		resourceWidth  = renderWidth;
		resourceHeight = renderHeight;
#endif

		// Resize HDR targets
		CreateHDRMaps(resourceWidth, resourceHeight);

		// Resize post effects targets
		CreatePostFXMaps(resourceWidth, resourceHeight);

		s_resourceWidth  = resourceWidth;
		s_resourceHeight = resourceHeight;
	}
}

void CRendererResources::DestroySystemTargets()
{
	if (gcpRendD3D->m_bSystemTargetsInit)
	{
		DestroyHDRMaps();
		DestroyPostFXMaps();

		SAFE_RELEASE(s_ptexWaterVolumeTemp[0]);
		SAFE_RELEASE(s_ptexWaterVolumeTemp[1]);
		SAFE_RELEASE_FORCE(s_ptexWaterOcean);

		s_resourceWidth  = 0;
		s_resourceHeight = 0;

		gcpRendD3D->m_bSystemTargetsInit = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CRendererResources::CreateHDRMaps(int resourceWidth, int resourceHeight)
{
	CD3D9Renderer* r = gcpRendD3D;
	SRenderTargetPool* pHDRPostProcess = SRenderTargetPool::GetInstance();

	pHDRPostProcess->ClearRenderTargetList();

	CRY_DISABLE_WARN_UNUSED_VARIABLES();
	const int width = resourceWidth, width_r2 = (width + 1) / 2, width_r4 = (width_r2 + 1) / 2, width_r8 = (width_r4 + 1) / 2;
	const int height = resourceHeight, height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2, height_r16 = (height_r8 + 1) / 2;

	const ETEX_Format nHDRFormat  = GetHDRFormat(false, false); // No alpha, default is HiQ, can be downgraded
	const ETEX_Format nHDRQFormat = GetHDRFormat(false, true ); // No alpha, default is LoQ, can be upgraded
	const ETEX_Format nHDRAFormat = GetHDRFormat(true , false); // With alpha

	uint32 nHDRTargetFlags = FT_DONT_RELEASE;
	uint32 nHDRTargetFlagsUAV = nHDRTargetFlags | (FT_USAGE_UNORDERED_ACCESS);  // UAV required for tiled deferred shading
	CRY_RESTORE_WARN_UNUSED_VARIABLES();

	for (int i = 0; i < 8; i++)
	{
		char szName[256];
		sprintf(szName, "$HDRAdaptedLuminanceCur_%d", i);
		pHDRPostProcess->AddRenderTarget(1, 1, Clr_White, eTF_R16G16F, 0.1f, szName, &s_ptexHDRAdaptedLuminanceCur[i], FT_DONT_RELEASE);
	}

#if RENDERER_ENABLE_FULL_PIPELINE
	// TODO: make it a MIP-mapped resource and/or use compute
	// Luminance rt
	for (int i = 0; i < NUM_HDR_TONEMAP_TEXTURES; i++)
	{
		char szName[256];
		int iSampleLen = 1 << (2 * i);
		cry_sprintf(szName, "$HDRToneMap_%d", i);

		pHDRPostProcess->AddRenderTarget(iSampleLen, iSampleLen, Clr_Dark, eTF_R16G16F, 0.7f, szName, &s_ptexHDRToneMaps[i], FT_DONT_RELEASE | (i == 0 ? FT_STAGE_READBACK : 0));
	}

	s_ptexHDRMeasuredLuminanceDummy = CTexture::GetOrCreateTextureObject("$HDRMeasuredLum_Dummy", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_R16G16F, TO_HDR_MEASURED_LUMINANCE);
#endif

	pHDRPostProcess->CreateRenderTargetList();

	r->m_vSceneLuminanceInfo = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	r->m_fAdaptedSceneScaleLBuffer = r->m_fAdaptedSceneScale = r->m_fScotopicSceneScale = 1.0f;
}

void CRendererResources::DestroyHDRMaps()
{
	int i;

	for (i = 0; i < 8; i++)
	{
		SAFE_RELEASE_FORCE(s_ptexHDRAdaptedLuminanceCur[i]);
	}

	for (i = 0; i < NUM_HDR_TONEMAP_TEXTURES; i++)
	{
		SAFE_RELEASE_FORCE(s_ptexHDRToneMaps[i]);
	}
	SAFE_RELEASE_FORCE(s_ptexHDRMeasuredLuminanceDummy);

	s_ptexCurLumTexture = NULL;
}

//==================================================================================================

bool CRendererResources::CreatePostFXMaps(int resourceWidth, int resourceHeight)
{
#if RENDERER_ENABLE_FULL_PIPELINE
	
	CRY_DISABLE_WARN_UNUSED_VARIABLES();
	const int width = resourceWidth, width_r2 = (width + 1) / 2, width_r4 = (width_r2 + 1) / 2, width_r8 = (width_r4 + 1) / 2;
	const int height = resourceHeight, height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2;

	const ETEX_Format nHDRFormat  = GetHDRFormat(false, false); // No alpha, default is HiQ, can be downgraded
	const ETEX_Format nHDRAFormat = GetHDRFormat(true , false); // With alpha
	const ETEX_Format nHDRQFormat = GetHDRFormat(false, true ); // No alpha, default is LoQ, can be upgraded
	const ETEX_Format nLDRPFormat = GetLDRFormat(true);         // With more than 8 mantissa bits for calculations
	CRY_RESTORE_WARN_UNUSED_VARIABLES();

	assert(gRenDev);

	// Ghosting requires data overframes, need to handle for each GPU in MGPU mode
	SPostEffectsUtils::GetOrCreateRenderTarget("$DisplayTarget 1/2p", s_ptexDisplayTargetScaledPrev, width_r2, height_r2, Clr_Unknown, 1, 0, nLDRPFormat, -1, FT_DONT_RELEASE);

	//	s_ptexWaterVolumeRefl[0]->DisableMgpuSync();
	//	s_ptexWaterVolumeRefl[1]->DisableMgpuSync();

	// TODO: Only create necessary RTs for minimal ring?
	char str[256];
	for (int i = 0, end = gcpRendD3D->GetActiveGPUCount() * MAX_FRAMES_IN_FLIGHT; i < end; i++)
	{
		sprintf(str, "$FlaresOcclusion_%d", i);
		SPostEffectsUtils::GetOrCreateRenderTarget(str, s_ptexFlaresOcclusionRing[i], CFlareSoftOcclusionQuery::s_nIDColMax, CFlareSoftOcclusionQuery::s_nIDRowMax, Clr_Unknown, 1, 0, eTF_R8G8, -1, FT_DONT_RELEASE | FT_STAGE_READBACK);
	}

	SPostEffectsUtils::GetOrCreateRenderTarget("$FlaresGather", s_ptexFlaresGather, CFlareSoftOcclusionQuery::s_nGatherTextureWidth, CFlareSoftOcclusionQuery::s_nGatherTextureHeight, Clr_Unknown, 1, 0, eTF_R8, -1, FT_DONT_RELEASE);

	/*
	 *	The following textures do not need to be recreated on resize
	 */

	if (!CTexture::IsTextureExist(s_ptexWaterVolumeDDN))
	{
		SPostEffectsUtils::GetOrCreateRenderTarget("$WaterVolumeDDN", s_ptexWaterVolumeDDN, 64, 64, Clr_Unknown, 1, true, eTF_R16G16B16A16F, TO_WATERVOLUMEMAP);
		//s_ptexWaterVolumeDDN->DisableMgpuSync();
	}
#endif

	return 1;
}

void CRendererResources::DestroyPostFXMaps()
{
	SAFE_RELEASE_FORCE(s_ptexDisplayTargetScaledPrev);
	SAFE_RELEASE(s_ptexWaterVolumeDDN);

	for (int i = 0; i < MAX_OCCLUSION_READBACK_TEXTURES; i++)
		SAFE_RELEASE_FORCE(s_ptexFlaresOcclusionRing[i]);

	SAFE_RELEASE_FORCE(s_ptexFlaresGather);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// The rules:
//
// #renderResolution can be whole multiples of #outputResolution (super-sampling)
// #outputResolution can be [1 to 2] factor of #displayResolution (dynamic downscale)
//
// See @CD3D9Renderer::CalculateResolutions

int CRendererResources::s_resourceWidth = 0, CRendererResources::s_resourceHeight = 0;
int CRendererResources::s_renderWidth   = 0, CRendererResources::s_renderHeight   = 0;
int CRendererResources::s_renderMinDim  = 0, CRendererResources::s_renderArea     = 0;
int CRendererResources::s_outputWidth   = 0, CRendererResources::s_outputHeight   = 0;
int CRendererResources::s_displayWidth  = 0, CRendererResources::s_displayHeight  = 0;

void CRendererResources::OnRenderResolutionChanged(int renderWidth, int renderHeight)
{
	if (s_renderWidth  != renderWidth ||
		s_renderHeight != renderHeight)
	{
		if (gcpRendD3D->m_bSystemTargetsInit)
			ResizeSystemTargets(renderWidth, renderHeight);

		// Stereo resources are used as temporary targets for deferred shaded viewports
		gcpRendD3D->GetS3DRend().OnResolutionChanged(renderWidth, renderHeight);

		s_renderWidth  = renderWidth;
		s_renderHeight = renderHeight;
		s_renderMinDim = std::min(renderWidth, renderHeight);
		s_renderArea   = renderWidth * renderHeight;
	}
}

void CRendererResources::OnOutputResolutionChanged(int outputWidth, int outputHeight)
{
	if (s_outputWidth  != outputWidth ||
		s_outputHeight != outputHeight)
	{
		s_outputWidth  = outputWidth;
		s_outputHeight = outputHeight;
	}
}

void CRendererResources::OnDisplayResolutionChanged(int displayWidth, int displayHeight)
{
	if (s_displayWidth  != displayWidth ||
		s_displayHeight != displayHeight)
	{
		s_displayWidth  = displayWidth;
		s_displayHeight = displayHeight;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CRendererResources::ShutDown()
{
	UnloadDefaultSystemTextures(true);

	if (s_ShaderTemplatesInitialized)
	{
		for (int i = 0; i < EFTT_MAX; i++)
		{
			s_ShaderTemplates[i].~CTexture();
		}
	}
	s_ShaderTemplates.Free();

	SAFE_DELETE(s_pTexNULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

size_t CRendererResources::m_RTallocs = 0;
size_t CRendererResources::m_DTallocs = 0;

CTexture* CRendererResources::CreateDepthTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF)
{
	char pName[128]; // Create unique names for every allocation, otherwise name-matches would occur in GetOrCreateDepthStencil()
	cry_sprintf(pName, "$DynDepthStencil_%8_x%d", m_DTallocs + 1, m_DTallocs + 2);
	++m_DTallocs;
	return CTexture::GetOrCreateDepthStencil(pName, nWidth, nHeight, cClear, eTT_2D, FT_USAGE_TEMPORARY | FT_NOMIPS, eTF == eTF_Unknown ? GetDepthFormat() : eTF);
}

CTexture* CRendererResources::CreateRenderTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF)
{
	char pName[128]; // Create unique names for every allocation, otherwise name-matches would occur in GetOrCreateRenderTarget()
	cry_sprintf(pName, "$DynRenderTarget_%8_x%d", m_RTallocs + 1, m_RTallocs + 2);
	++m_RTallocs;
	return CTexture::GetOrCreateRenderTarget(pName, nWidth, nHeight, cClear, eTT_2D, FT_USAGE_TEMPORARY | FT_NOMIPS, eTF);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

Ang3 sDeltAngles(Ang3& Ang0, Ang3& Ang1)
{
	Ang3 out;
	for (int i = 0; i < 3; i++)
	{
		float a0 = Ang0[i];
		a0 = (float)((360.0 / 65536) * ((int)(a0 * (65536 / 360.0)) & 65535)); // angmod
		float a1 = Ang1[i];
		a1 = (float)((360.0 / 65536) * ((int)(a1 * (65536 / 360.0)) & 65535));
		out[i] = a0 - a1;
	}
	return out;
}

SEnvTexture* CRendererResources::FindSuitableEnvTex(Vec3& Pos, Ang3& Angs, bool bMustExist, int RendFlags, bool bUseExistingREs, CShader* pSH, CShaderResources* pRes, CRenderObject* pObj, bool bReflect, CRenderElement* pRE, bool* bMustUpdate, const SRenderingPassInfo* pPassInfo)
{
	float time0 = iTimer->GetAsyncCurTime();

	int i;
	float distO = 999999;
	float adist = 999999;
	int firstForUse = -1;
	int firstFree = -1;
	Vec3 objPos;
	if (bMustUpdate)
		*bMustUpdate = false;
	if (!pObj)
		bReflect = false;
	else
	{
		if (bReflect)
		{
			Plane pl;
			pRE->mfGetPlane(pl);
			objPos = pl.MirrorPosition(Vec3(0, 0, 0));
		}
		else if (pRE)
			pRE->mfCenter(objPos, pObj, *pPassInfo);
		else
		{
			CRY_ASSERT(!pPassInfo);
			CRY_ASSERT(!gcpRendD3D->m_pRT->IsRenderThread());
			objPos = pObj->GetMatrix().GetTranslation();
		}
	}
	float dist = 999999;
	for (i = 0; i < MAX_ENVTEXTURES; i++)
	{
		SEnvTexture* cur = &s_EnvTexts[i];
		if (cur->m_bReflected != bReflect)
			continue;
		float s = (cur->m_CamPos - Pos).GetLengthSquared();
		Ang3 angDelta = sDeltAngles(Angs, cur->m_Angle);
		float a = angDelta.x * angDelta.x + angDelta.y * angDelta.y + angDelta.z * angDelta.z;
		float so = 0;
		if (bReflect)
			so = (cur->m_ObjPos - objPos).GetLengthSquared();
		if (s <= dist && a <= adist && so <= distO)
		{
			dist = s;
			adist = a;
			distO = so;
			firstForUse = i;
			if (!so && !s && !a)
				break;
		}
		if (cur->m_pTex && !cur->m_pTex->m_pTexture && firstFree < 0)
			firstFree = i;
	}
	if (bMustExist && firstForUse >= 0)
		return &s_EnvTexts[firstForUse];
	if (bReflect)
		dist = distO;

	float curTime = iTimer->GetCurrTime();
	int nUpdate = -2;
	float fTimeInterval = dist * CRenderer::CV_r_envtexupdateinterval + CRenderer::CV_r_envtexupdateinterval * 0.5f;
	float fDelta = curTime - s_EnvTexts[firstForUse].m_TimeLastUpdated;
	if (bMustExist)
		nUpdate = -2;
	else if (dist > MAX_ENVTEXSCANDIST)
	{
		if (firstFree >= 0)
			nUpdate = firstFree;
		else
			nUpdate = -1;
	}
	else if (fDelta > fTimeInterval)
		nUpdate = firstForUse;
	if (nUpdate == -2 && firstForUse >= 0)
	{
		// No need to update (Up to date)
		return &s_EnvTexts[firstForUse];
	}
	if (nUpdate >= 0 && !s_EnvTexts[nUpdate].m_pTex)
		return NULL;
	if (nUpdate >= 0)
	{
		if (!s_EnvTexts[nUpdate].m_pTex->m_pTexture || SRenderStatistics::Write().m_fEnvTextUpdateTime < 0.1f)
		{
			int n = nUpdate;
			s_EnvTexts[n].m_TimeLastUpdated = curTime;
			s_EnvTexts[n].m_CamPos = Pos;
			s_EnvTexts[n].m_Angle = Angs;
			s_EnvTexts[n].m_ObjPos = objPos;
			s_EnvTexts[n].m_bReflected = bReflect;
			if (bMustUpdate)
				*bMustUpdate = true;
		}
		SRenderStatistics::Write().m_fEnvTextUpdateTime += iTimer->GetAsyncCurTime() - time0;
		return &s_EnvTexts[nUpdate];
	}

	dist = 0;
	firstForUse = -1;
	for (i = 0; i < MAX_ENVTEXTURES; i++)
	{
		SEnvTexture* cur = &s_EnvTexts[i];
		if (dist < curTime - cur->m_TimeLastUpdated && !cur->m_bInprogress)
		{
			dist = curTime - cur->m_TimeLastUpdated;
			firstForUse = i;
		}
	}
	if (firstForUse < 0)
	{
		return NULL;
	}
	int n = firstForUse;
	s_EnvTexts[n].m_TimeLastUpdated = curTime;
	s_EnvTexts[n].m_CamPos = Pos;
	s_EnvTexts[n].m_ObjPos = objPos;
	s_EnvTexts[n].m_Angle = Angs;
	s_EnvTexts[n].m_bReflected = bReflect;
	if (bMustUpdate)
		*bMustUpdate = true;

	SRenderStatistics::Write().m_fEnvTextUpdateTime += iTimer->GetAsyncCurTime() - time0;
	return &s_EnvTexts[n];

}
