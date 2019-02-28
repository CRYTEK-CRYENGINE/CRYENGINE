// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/IFlares.h>
#include "RendererResources.h"
#include "Renderer.h"
#include "Textures/Texture.h"                           // CTexture

#include "../XRenderD3D9/D3DDeferredShading.h"          // CDeferredShading
#include "../XRenderD3D9/D3DPostProcess.h"              // SD3DPostEffectsUtils

#include "RendElements/FlareSoftOcclusionQuery.h"       // CFlareSoftOcclusionQuery

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

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

class SRenderTargetPool
{

public:

	void AddRenderTarget(uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Format Format, float fPriority, const char* szName, CTexture** pStorage, uint32 nFlags = 0, int32 nCustomID = -1, bool bDynamicTex = 0);
	bool CreateRenderTargetList();
	void ClearRenderTargetList()
	{
		m_pRenderTargets.clear();
	}

	static SRenderTargetPool* GetInstance()
	{
		static SRenderTargetPool pInstance;
		return &pInstance;
	}

private:

	std::vector<SRenderTargetInfo> m_pRenderTargets;
};

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
CTexture* CRendererResources::s_ptexShadowMask;
CTexture* CRendererResources::s_ptexCachedShadowMap[MAX_GSM_LODS_NUM];
CTexture* CRendererResources::s_ptexNearestShadowMap;
CTexture* CRendererResources::s_ptexHeightMapAO[2];
CTexture* CRendererResources::s_ptexHeightMapAODepth[2];
CTexture* CRendererResources::s_ptexFromRE_FromContainer[2];
CTexture* CRendererResources::s_ptexFromObj;
CTexture* CRendererResources::s_ptexRT_2D;
CTexture* CRendererResources::s_ptexNormalsFitting;
CTexture* CRendererResources::s_ptexPerlinNoiseMap;

CTexture* CRendererResources::s_ptexSceneNormalsMap;
CTexture* CRendererResources::s_ptexSceneNormalsBent;
CTexture* CRendererResources::s_ptexAOColorBleed;
CTexture* CRendererResources::s_ptexSceneDiffuse;
CTexture* CRendererResources::s_ptexSceneSpecular;

CTexture* CRendererResources::s_ptexSceneSelectionIDs;

CTexture* CRendererResources::s_ptexWindGrid;

#if defined(DURANGO_USE_ESRAM)
CTexture* CRendererResources::s_ptexSceneSpecularESRAM;
#endif

// Post-process related textures
CTexture* CRendererResources::s_ptexBackBuffer = NULL;
CTexture* CRendererResources::s_ptexModelHudBuffer;
CTexture* CRendererResources::s_ptexCached3DHud;
CTexture* CRendererResources::s_ptexCached3DHudScaled;
CTexture* CRendererResources::s_ptexBackBufferScaled[3];
CTexture* CRendererResources::s_ptexBackBufferScaledTemp[2];
CTexture* CRendererResources::s_ptexPrevFrameScaled;

CTexture* CRendererResources::s_ptexWaterOcean;
CTexture* CRendererResources::s_ptexWaterVolumeTemp[2];
CTexture* CRendererResources::s_ptexWaterVolumeDDN;
CTexture* CRendererResources::s_ptexWaterVolumeRefl[2] = { NULL };
CTexture* CRendererResources::s_ptexWaterCaustics[2] = { NULL };
CTexture* CRendererResources::s_ptexRainOcclusion;
CTexture* CRendererResources::s_ptexRainSSOcclusion[2];

CTexture* CRendererResources::s_ptexRT_ShadowPool;
CTexture* CRendererResources::s_ptexFarPlane;
CTexture* CRendererResources::s_ptexCloudsLM;

CTexture* CRendererResources::s_ptexSceneTarget = NULL;
CTexture* CRendererResources::s_ptexSceneTargetR11G11B10F[2] = { NULL };
CTexture* CRendererResources::s_ptexSceneDiffuseTmp;
CTexture* CRendererResources::s_ptexSceneSpecularTmp;
CTexture* CRendererResources::s_ptexSceneDepthScaled[3];
CTexture* CRendererResources::s_ptexLinearDepth;
CTexture* CRendererResources::s_ptexLinearDepthReadBack[4];
CTexture* CRendererResources::s_ptexLinearDepthDownSample[4];
CTexture* CRendererResources::s_ptexLinearDepthScaled[3];
CTexture* CRendererResources::s_ptexHDRTarget;
CTexture* CRendererResources::s_ptexVelocity;
CTexture* CRendererResources::s_ptexVelocityTiles[3] = { NULL };
CTexture* CRendererResources::s_ptexVelocityObjects[2] = { NULL };
CTexture* CRendererResources::s_ptexHDRTargetPrev = NULL;
CTexture* CRendererResources::s_ptexHDRTargetScaled[4];
CTexture* CRendererResources::s_ptexHDRTargetScaledTmp[4];
CTexture* CRendererResources::s_ptexHDRTargetScaledTempRT[4];
CTexture* CRendererResources::s_ptexHDRDofLayers[2];
CTexture* CRendererResources::s_ptexSceneCoC[MIN_DOF_COC_K] = { NULL };
CTexture* CRendererResources::s_ptexSceneCoCTemp = NULL;
CTexture* CRendererResources::s_ptexHDRTempBloom[2];
CTexture* CRendererResources::s_ptexHDRFinalBloom;
CTexture* CRendererResources::s_ptexHDRAdaptedLuminanceCur[8];
int CRendererResources::s_nCurLumTextureIndex;
CTexture* CRendererResources::s_ptexCurLumTexture;
CTexture* CRendererResources::s_ptexHDRToneMaps[NUM_HDR_TONEMAP_TEXTURES];
CTexture* CRendererResources::s_ptexHDRMeasuredLuminance[MAX_GPU_NUM];
CTexture* CRendererResources::s_ptexHDRMeasuredLuminanceDummy;
CTexture* CRendererResources::s_ptexColorChart;

CTexture* CRendererResources::s_ptexFlaresOcclusionRing[MAX_OCCLUSION_READBACK_TEXTURES] = { NULL };
CTexture* CRendererResources::s_ptexFlaresGather = NULL;

CTexture* CRendererResources::s_pTexNULL = 0;

CTexture* CRendererResources::s_ptexVolumetricFog = NULL;
CTexture* CRendererResources::s_ptexVolCloudShadow = NULL;

#if defined(VOLUMETRIC_FOG_SHADOWS)
CTexture* CRendererResources::s_ptexVolFogShadowBuf[2] = { 0 };
#endif

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
	SAFE_RELEASE_FORCE(s_ptexVolCloudShadow);

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
	SAFE_RELEASE_FORCE(s_ptexRT_ShadowPool);
	SAFE_RELEASE_FORCE(s_ptexFarPlane);

	s_CustomRT_2D.Free();
	//s_ShaderTemplates.Free();

	// release targets pools
	//SDynTexture_Shadow::ShutDown();
	SDynTexture::ShutDown();
	SDynTexture2::ShutDown();

	//ReleaseSystemTargets();

	m_bLoadedSystem = false;
}

void CRendererResources::LoadDefaultSystemTextures()
{
	LOADING_TIME_PROFILE_SECTION;

	char str[256];
	int i;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Engine textures");

	if (!m_bLoadedSystem)
	{
		m_bLoadedSystem = true;

		const float  clearDepth   = Clr_FarPlane_Rev.r;
		const uint8  clearStencil = Val_Stencil;
		const ColorF clearValues  = ColorF(clearDepth, FLOAT(clearStencil), 0.f, 0.f);

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

		s_ptexRainOcclusion = CTexture::GetOrCreateTextureObject("$RainOcclusion", RAIN_OCC_MAP_SIZE, RAIN_OCC_MAP_SIZE, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_R8G8B8A8);
		s_ptexRainSSOcclusion[0] = CTexture::GetOrCreateTextureObject("$RainSSOcclusion0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		s_ptexRainSSOcclusion[1] = CTexture::GetOrCreateTextureObject("$RainSSOcclusion1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

		//s_ptexHitAreaRT[0] = CTexture::CreateTextureObject("$HitEffectAccumBuffRT_0", 128, 128, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		//s_ptexHitAreaRT[1] = CTexture::CreateTextureObject("$HitEffectAccumBuffRT_1", 128, 128, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

		s_ptexWindGrid = CTexture::GetOrCreateTextureObject("WindGrid", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown, TO_WINDGRID);
		if (!CTexture::IsTextureExist(s_ptexWindGrid))
		{
			s_ptexWindGrid->Create2DTexture(256, 256, 1, FT_DONT_RELEASE | FT_DONT_STREAM, nullptr, eTF_R16G16F);
		}

		s_ptexLinearDepthReadBack[0] = CTexture::GetOrCreateTextureObject("$ZTargetReadBack0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexLinearDepthReadBack[1] = CTexture::GetOrCreateTextureObject("$ZTargetReadBack1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexLinearDepthReadBack[2] = CTexture::GetOrCreateTextureObject("$ZTargetReadBack2", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexLinearDepthReadBack[3] = CTexture::GetOrCreateTextureObject("$ZTargetReadBack3", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);

		s_ptexLinearDepthDownSample[0] = CTexture::GetOrCreateTextureObject("$ZTargetDownSample0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexLinearDepthDownSample[1] = CTexture::GetOrCreateTextureObject("$ZTargetDownSample1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexLinearDepthDownSample[2] = CTexture::GetOrCreateTextureObject("$ZTargetDownSample2", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexLinearDepthDownSample[3] = CTexture::GetOrCreateTextureObject("$ZTargetDownSample3", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);

		s_ptexRT_ShadowPool = CTexture::GetOrCreateTextureObject("$RT_ShadowPool", 0, 0, 1, eTT_2D, FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);
		s_ptexFarPlane = CTexture::GetOrCreateTextureObject("$FarPlane", 8, 8, 1, eTT_2D, FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);

		if (!s_ptexModelHudBuffer)
			s_ptexModelHudBuffer = CTexture::GetOrCreateTextureObject("$ModelHud", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_MODELHUD);

	//	if (!s_ptexBackBuffer)
		{
			s_ptexSceneTarget = CTexture::GetOrCreateTextureObject("$SceneTarget", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SCENE_TARGET);

			s_ptexSceneTargetR11G11B10F[0] = CTexture::GetOrCreateTextureObject("$SceneTargetR11G11B10F_0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexSceneTargetR11G11B10F[1] = CTexture::GetOrCreateTextureObject("$SceneTargetR11G11B10F_1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);

			s_ptexVelocity = CTexture::GetOrCreateTextureObject("$Velocity", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexVelocityTiles[0] = CTexture::GetOrCreateTextureObject("$VelocityTilesTmp0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexVelocityTiles[1] = CTexture::GetOrCreateTextureObject("$VelocityTilesTmp1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexVelocityTiles[2] = CTexture::GetOrCreateTextureObject("$VelocityTiles", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexVelocityObjects[0] = CTexture::GetOrCreateTextureObject("$VelocityObjects", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS, eTF_Unknown, -1);
			// Only used for VR, but we need to support runtime switching
			s_ptexVelocityObjects[1] = CTexture::GetOrCreateTextureObject("$VelocityObjects_R", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS, eTF_Unknown, -1);

			s_ptexBackBuffer = CTexture::GetOrCreateTextureObject("$BackBuffer", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_BACKBUFFERMAP);

			s_ptexPrevFrameScaled = CTexture::GetOrCreateTextureObject("$PrevFrameScale", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

			s_ptexBackBufferScaled[0] = CTexture::GetOrCreateTextureObject("$BackBufferScaled_d2", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_BACKBUFFERSCALED_D2);
			s_ptexBackBufferScaled[1] = CTexture::GetOrCreateTextureObject("$BackBufferScaled_d4", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_BACKBUFFERSCALED_D4);
			s_ptexBackBufferScaled[2] = CTexture::GetOrCreateTextureObject("$BackBufferScaled_d8", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_BACKBUFFERSCALED_D8);

			s_ptexBackBufferScaledTemp[0] = CTexture::GetOrCreateTextureObject("$BackBufferScaledTemp_d2", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexBackBufferScaledTemp[1] = CTexture::GetOrCreateTextureObject("$BackBufferScaledTemp_d4", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);

			s_ptexSceneNormalsMap = CTexture::GetOrCreateTextureObject("$SceneNormalsMap", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8, TO_SCENE_NORMALMAP);
			s_ptexSceneNormalsBent = CTexture::GetOrCreateTextureObject("$SceneNormalsBent", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
			s_ptexSceneDiffuse = CTexture::GetOrCreateTextureObject("$SceneDiffuse", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
			s_ptexSceneSpecular = CTexture::GetOrCreateTextureObject("$SceneSpecular", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
#if defined(DURANGO_USE_ESRAM)
			s_ptexSceneSpecularESRAM = CTexture::GetOrCreateTextureObject("$SceneSpecularESRAM", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
#endif
			s_ptexSceneDiffuseTmp = CTexture::GetOrCreateTextureObject("$SceneDiffuseTmp", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8, TO_SCENE_DIFFUSE_ACC);
			s_ptexSceneSpecularTmp = CTexture::GetOrCreateTextureObject("$SceneSpecularTmp", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8, TO_SCENE_SPECULAR_ACC);
			s_ptexShadowMask = CTexture::GetOrCreateTextureObject("$ShadowMask", 0, 0, 1, eTT_2DArray, nRTFlags, eTF_R8, TO_SHADOWMASK);

			s_ptexFlaresGather = CTexture::GetOrCreateTextureObject("$FlaresGather", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
			for (i = 0; i < MAX_OCCLUSION_READBACK_TEXTURES; i++)
			{
				cry_sprintf(str, "$FlaresOcclusion_%d", i);
				s_ptexFlaresOcclusionRing[i] = CTexture::GetOrCreateTextureObject(str, 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
			}

			// fixme: get texture resolution from CREWaterOcean
			// TODO: make s_ptexWaterVolumeTemp an array texture
			s_ptexWaterOcean = CTexture::GetOrCreateTextureObject("$WaterOceanMap", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_NOMIPS | FT_DONT_STREAM, eTF_Unknown, TO_WATEROCEANMAP);
			s_ptexWaterVolumeTemp[0] = CTexture::GetOrCreateTextureObject("$WaterVolumeTemp_0", 64, 64, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_NOMIPS | FT_DONT_STREAM, eTF_Unknown);
			s_ptexWaterVolumeTemp[1] = CTexture::GetOrCreateTextureObject("$WaterVolumeTemp_1", 64, 64, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_NOMIPS | FT_DONT_STREAM, eTF_Unknown);
			s_ptexWaterVolumeDDN = CTexture::GetOrCreateTextureObject("$WaterVolumeDDN", 64, 64, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, eTF_Unknown, TO_WATERVOLUMEMAP);
			s_ptexWaterVolumeRefl[0] = CTexture::GetOrCreateTextureObject("$WaterVolumeRefl", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, eTF_Unknown, TO_WATERVOLUMEREFLMAP);
			s_ptexWaterVolumeRefl[1] = CTexture::GetOrCreateTextureObject("$WaterVolumeReflPrev", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, eTF_Unknown, TO_WATERVOLUMEREFLMAPPREV);
			s_ptexWaterCaustics[0] = CTexture::GetOrCreateTextureObject("$WaterVolumeCaustics", 512, 512, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_WATERVOLUMECAUSTICSMAP);
			s_ptexWaterCaustics[1] = CTexture::GetOrCreateTextureObject("$WaterVolumeCausticsTemp", 512, 512, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_WATERVOLUMECAUSTICSMAPTEMP);

			s_ptexSceneDepthScaled[0] = CTexture::GetOrCreateTextureObject("$SceneDepthScaled", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);
			s_ptexSceneDepthScaled[1] = CTexture::GetOrCreateTextureObject("$SceneDepthScaled2", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);
			s_ptexSceneDepthScaled[2] = CTexture::GetOrCreateTextureObject("$SceneDepthScaled3", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);

			s_ptexLinearDepth          = CTexture::GetOrCreateTextureObject("$ZTarget", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
			s_ptexLinearDepthScaled[0] = CTexture::GetOrCreateTextureObject("$ZTargetScaled", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_DOWNSCALED_ZTARGET_FOR_AO);
			s_ptexLinearDepthScaled[1] = CTexture::GetOrCreateTextureObject("$ZTargetScaled2", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_QUARTER_ZTARGET_FOR_AO);
			s_ptexLinearDepthScaled[2] = CTexture::GetOrCreateTextureObject("$ZTargetScaled3", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		}

		s_ptexSceneSelectionIDs = CTexture::GetOrCreateTextureObject("$SceneSelectionIDs", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_R32F);

		s_ptexHDRTarget = CTexture::GetOrCreateTextureObject("$HDRTarget", 0, 0, 1, eTT_2D, nRTFlags, eTF_Unknown);

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
		s_ptexVolCloudShadow = CTexture::GetOrCreateTextureObject("$VolCloudShadows", 0, 0, 0, eTT_3D, FT_NOMIPS | FT_USAGE_UNORDERED_ACCESS, eTF_Unknown);

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
		// Assign ESRAM offsets
		if (CRenderer::CV_r_useESRAM)
		{
			// Precomputed offsets, using xg library and aligned to 4k
			//     1600x900 RGBA16F:  11894784
			//     1600x900 RGBA8:     5955584

			if (gRenDev->GetWidth() <= 1600 && gRenDev->GetHeight() <= 900)
			{
				s_ptexHDRTarget->SetESRAMOffset(0);
				s_ptexSceneSpecularESRAM->SetESRAMOffset(0);
				s_ptexSceneNormalsMap->SetESRAMOffset(11894784 + 5955584 * 0);
				s_ptexSceneDiffuse->SetESRAMOffset(11894784 + 5955584 * 1);
				// Depth target uses: 11894784 + 5955584 * 2
			}
			else
			{
				iLog->LogError("Disabling ESRAM since resolution is larger than 1600x900");
				assert(0);
			}
		}
#endif
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

ETEX_Format CRendererResources::GetLDRFormat()
{
	ETEX_Format candidate = eTF_R8G8B8A8;

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
		ETEX_Format nHDRFormat = CRenderer::CV_r_HDRTexFormat == 0 ? eTF_R11G11B10F : eTF_R16G16B16A16F;

		// Create ZTarget
		CreateDepthMaps(resourceWidth, resourceHeight);

		// Create scene targets
		CreateSceneMaps(nHDRFormat, resourceWidth, resourceHeight);

		// Create HDR targets
		CreateHDRMaps(resourceWidth, resourceHeight);

		// Allocate cached shadow maps if required
		CreateCachedShadowMaps();

		// Allocate the nearest shadow map if required
		CreateNearestShadowMap();

		// Create post effects targets
		CreatePostFXMaps(resourceWidth, resourceHeight);

		// Create deferred lighting targets
		CreateDeferredMaps(resourceWidth, resourceHeight);

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

		ETEX_Format nHDRFormat = CRenderer::CV_r_HDRTexFormat == 0 ? eTF_R11G11B10F : eTF_R16G16B16A16F;

		// Resize ZTarget
		CreateDepthMaps(resourceWidth, resourceHeight);

		// Resize scene targets
		CreateSceneMaps(nHDRFormat, resourceWidth, resourceHeight);

		// Resize HDR targets
		CreateHDRMaps(resourceWidth, resourceHeight);

		// Resize post effects targets
		CreatePostFXMaps(resourceWidth, resourceHeight);

		// Resize deferred lighting targets
		CreateDeferredMaps(resourceWidth, resourceHeight);

		s_resourceWidth  = resourceWidth;
		s_resourceHeight = resourceHeight;
	}
}

void CRendererResources::DestroySystemTargets()
{
	if (gcpRendD3D->m_bSystemTargetsInit)
	{
		DestroyDepthMaps();
		DestroySceneMaps();
		DestroyHDRMaps();
		DestroyCachedShadowMaps();
		DestroyNearestShadowMap();
		DestroyDeferredMaps();
		DestroyPostFXMaps();

		SAFE_RELEASE(s_ptexWaterVolumeTemp[0]);
		SAFE_RELEASE(s_ptexWaterVolumeTemp[1]);
		SAFE_RELEASE_FORCE(s_ptexWaterOcean);

		SAFE_RELEASE_FORCE(s_ptexSceneNormalsMap);
		SAFE_RELEASE_FORCE(s_ptexSceneNormalsBent);
		SAFE_RELEASE_FORCE(s_ptexSceneDiffuse);
		SAFE_RELEASE_FORCE(s_ptexSceneSpecular);
		SAFE_RELEASE_FORCE(s_ptexSceneSelectionIDs);
#if defined(DURANGO_USE_ESRAM)
		SAFE_RELEASE(s_ptexSceneSpecularESRAM);
#endif
		SAFE_RELEASE_FORCE(s_ptexSceneDiffuseTmp);
		SAFE_RELEASE_FORCE(s_ptexSceneSpecularTmp);

		s_resourceWidth  = 0;
		s_resourceHeight = 0;

		gcpRendD3D->m_bSystemTargetsInit = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CRendererResources::CreateDepthMaps(int resourceWidth, int resourceHeight)
{
	const ETEX_Format preferredDepthFormat =
		gRenDev->GetDepthBpp() == 32 ? eTF_D32FS8 :
		gRenDev->GetDepthBpp() == 24 ? eTF_D24S8  :
		gRenDev->GetDepthBpp() ==  8 ? eTF_D16S8  : eTF_D16;
	const ETEX_Format eTFZ = s_eTFZ =
		preferredDepthFormat == eTF_D32FS8 ? eTF_R32F :
		preferredDepthFormat == eTF_D24S8  ? eTF_R32F :
		preferredDepthFormat == eTF_D16S8  ? eTF_R16  : eTF_R16;

	const uint32 nDSFlags = FT_DONT_STREAM | FT_DONT_RELEASE | FT_USAGE_DEPTHSTENCIL;
	const uint32 nRTFlags = FT_DONT_STREAM | FT_DONT_RELEASE | FT_USAGE_RENDERTARGET;
	
	s_ptexLinearDepth->SetFlags(nRTFlags);
	s_ptexLinearDepth->SetWidth(resourceWidth);
	s_ptexLinearDepth->SetHeight(resourceHeight);
	s_ptexLinearDepth->CreateRenderTarget(eTFZ, ColorF(1.0f, 1.0f, 1.0f, 1.0f));
	
}

void CRendererResources::DestroyDepthMaps()
{
	SAFE_RELEASE_FORCE(s_ptexLinearDepth);
}

//==================================================================================================

void CRendererResources::CreateSceneMaps(ETEX_Format eTF, int resourceWidth, int resourceHeight)
{
	const int32 nWidth  = resourceWidth;
	const int32 nHeight = resourceHeight;
	uint32 nFlags = FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS;

	if (!s_ptexSceneTarget)
		s_ptexSceneTarget = CTexture::GetOrCreateRenderTarget("$SceneTarget", nWidth, nHeight, Clr_Empty, eTT_2D, nFlags, eTF, TO_SCENE_TARGET);
	else
	{
		s_ptexSceneTarget->SetFlags(nFlags);
		s_ptexSceneTarget->SetWidth(nWidth);
		s_ptexSceneTarget->SetHeight(nHeight);
		s_ptexSceneTarget->CreateRenderTarget(eTF, Clr_Empty);
	}

	nFlags &= ~(FT_USAGE_MSAA | FT_USAGE_UNORDERED_ACCESS);

	// This RT used for all post processes passes and shadow mask (group 0) as well
	if (!CTexture::IsTextureExist(s_ptexBackBuffer))
		s_ptexBackBuffer = CTexture::GetOrCreateRenderTarget("$BackBuffer", nWidth, nHeight, Clr_Empty, eTT_2D, nFlags, eTF_R8G8B8A8, TO_BACKBUFFERMAP);
	else
	{
		s_ptexBackBuffer->SetFlags(nFlags);
		s_ptexBackBuffer->SetWidth(nWidth);
		s_ptexBackBuffer->SetHeight(nHeight);
		s_ptexBackBuffer->CreateRenderTarget(eTF, Clr_Empty);
	}

	nFlags &= ~(FT_USAGE_MSAA | FT_USAGE_UNORDERED_ACCESS);

	// This RT can be used by the Render3DModelMgr if the buffer needs to be persistent
	if (CRenderer::CV_r_UsePersistentRTForModelHUD > 0)
	{
		if (!CTexture::IsTextureExist(s_ptexModelHudBuffer))
			s_ptexModelHudBuffer = CTexture::GetOrCreateRenderTarget("$ModelHUD", nWidth, nHeight, Clr_Transparent, eTT_2D, nFlags, eTF_R8G8B8A8);
		else
		{
			s_ptexModelHudBuffer->SetFlags(nFlags);
			s_ptexModelHudBuffer->SetWidth(nWidth);
			s_ptexModelHudBuffer->SetHeight(nHeight);
			s_ptexModelHudBuffer->CreateRenderTarget(eTF_R8G8B8A8, Clr_Transparent);
		}
	}

	if (gEnv->IsEditor())
	{
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneSelectionIDs", s_ptexSceneSelectionIDs, nWidth, nHeight, Clr_Transparent, false, false, eTF_R32F, -1, nFlags);
	}
}

void CRendererResources::DestroySceneMaps()
{
	SAFE_RELEASE(s_ptexSceneTarget);
	SAFE_RELEASE(s_ptexBackBuffer);

	if (CRenderer::CV_r_UsePersistentRTForModelHUD > 0)
	{
		SAFE_RELEASE(s_ptexModelHudBuffer);
	}

	if (gEnv->IsEditor())
	{
		SAFE_RELEASE(s_ptexSceneSelectionIDs);
	}
}

//==================================================================================================

void CRendererResources::CreateDeferredMaps(int resourceWidth, int resourceHeight)
{
	Vec2i resolution = Vec2i(resourceWidth, resourceHeight);

	static uint32 nPrevLBuffersFmt = CRenderer::CV_r_DeferredShadingLBuffersFmt;
//	if (!s_ptexSceneNormalsMap ||
//		!s_ptexSceneNormalsMap->GetDevTexture() ||
//		s_ptexSceneNormalsMap->GetDevTexture()->IsMSAAChanged()
//		|| s_ptexSceneNormalsMap->GetWidth() != resolution.x
//		|| s_ptexSceneNormalsMap->GetHeight() != resolution.y
//		|| nPrevLBuffersFmt != CRenderer::CV_r_DeferredShadingLBuffersFmt
//		|| (gRenDev->IsStereoEnabled() && !s_ptexVelocityObjects[1]->GetDevTexture()))
	{
		nPrevLBuffersFmt = CRenderer::CV_r_DeferredShadingLBuffersFmt;

		const int width  = resolution.x, width_r2  = (width  + 1) / 2, width_r4  = (width_r2  + 1) / 2, width_r8  = (width_r4 +  1) / 2;
		const int height = resolution.y, height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2;

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneNormalsMap", s_ptexSceneNormalsMap, width, height, Clr_Unknown, true, false, eTF_R8G8B8A8, TO_SCENE_NORMALMAP, FT_USAGE_ALLOWREADSRGB);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneNormalsBent", s_ptexSceneNormalsBent, width, height, Clr_Median, true, false, eTF_R8G8B8A8);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$AOColorBleed", s_ptexAOColorBleed, width_r8, height_r8, Clr_Unknown, true, false, eTF_R8G8B8A8);
		
		ETEX_Format preferredDepthFormat =
			gRenDev->GetDepthBpp() == 32 ? eTF_D32FS8 :
			gRenDev->GetDepthBpp() == 24 ? eTF_D24S8 :
			gRenDev->GetDepthBpp() ==  8 ? eTF_D16S8 : eTF_D16;
		ETEX_Format fmtZScaled = eTF_R16G16B16A16F;
		ETEX_Format nTexFormat = eTF_R16G16B16A16F;
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		if (CRenderer::CV_r_DeferredShadingLBuffersFmt == 1)
			nTexFormat = eTF_R11G11B10F;
#endif

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneDiffuseTmp", s_ptexSceneDiffuseTmp, width, height, Clr_Empty, true, false, eTF_R8G8B8A8);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneSpecularTmp", s_ptexSceneSpecularTmp, width, height, Clr_Empty, true, false, eTF_R8G8B8A8);

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneDiffuse", s_ptexSceneDiffuse, width, height, Clr_Empty, true, false, eTF_R8G8B8A8, -1, FT_USAGE_ALLOWREADSRGB);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneSpecular", s_ptexSceneSpecular, width, height, Clr_Empty, true, false, eTF_R8G8B8A8, -1, FT_USAGE_ALLOWREADSRGB);
#if defined(DURANGO_USE_ESRAM)
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneSpecularESRAM", s_ptexSceneSpecularESRAM, width, height, Clr_Empty, true, false, eTF_R8G8B8A8, -1);
#endif

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$VelocityObjects", s_ptexVelocityObjects[0], width, height, Clr_Transparent, true, false, eTF_R16G16F, -1, FT_USAGE_UNORDERED_ACCESS);
		if (gRenDev->IsStereoEnabled())
		{
			SD3DPostEffectsUtils::GetOrCreateRenderTarget("$VelocityObject_R", s_ptexVelocityObjects[1], width, height, Clr_Transparent, true, false, eTF_R16G16F, -1, FT_USAGE_UNORDERED_ACCESS);
		}

		SD3DPostEffectsUtils::GetOrCreateDepthStencil("$SceneDepthScaled" , s_ptexSceneDepthScaled[0], width_r2, height_r2, Clr_FarPlane_Rev, false, false, preferredDepthFormat, -1, FT_USAGE_DEPTHSTENCIL);
		SD3DPostEffectsUtils::GetOrCreateDepthStencil("$SceneDepthScaled2", s_ptexSceneDepthScaled[1], width_r4, height_r4, Clr_FarPlane_Rev, false, false, preferredDepthFormat, -1, FT_USAGE_DEPTHSTENCIL);
		SD3DPostEffectsUtils::GetOrCreateDepthStencil("$SceneDepthScaled3", s_ptexSceneDepthScaled[2], width_r8, height_r8, Clr_FarPlane_Rev, false, false, preferredDepthFormat, -1, FT_USAGE_DEPTHSTENCIL);

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$ZTargetScaled" , s_ptexLinearDepthScaled[0], width_r2, height_r2, ColorF(1.0f, 1.0f, 1.0f, 1.0f), 1, 0, fmtZScaled, TO_DOWNSCALED_ZTARGET_FOR_AO);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$ZTargetScaled2", s_ptexLinearDepthScaled[1], width_r4, height_r4, ColorF(1.0f, 1.0f, 1.0f, 1.0f), 1, 0, fmtZScaled, TO_QUARTER_ZTARGET_FOR_AO);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$ZTargetScaled3", s_ptexLinearDepthScaled[2], width_r8, height_r8, ColorF(1.0f, 1.0f, 1.0f, 1.0f), 1, 0, fmtZScaled);
	}

	// Pre-create shadow pool
	IF(gcpRendD3D->m_pRT->IsRenderThread() && gEnv->p3DEngine, 1)
	{
		//init shadow pool size
		static ICVar* p_e_ShadowsPoolSize = iConsole->GetCVar("e_ShadowsPoolSize");
		gcpRendD3D->m_nShadowPoolHeight = p_e_ShadowsPoolSize->GetIVal();
		gcpRendD3D->m_nShadowPoolWidth = gcpRendD3D->m_nShadowPoolHeight; //square atlas

		ETEX_Format eShadTF = gcpRendD3D->CV_r_shadowtexformat == 1 ? eTF_D16 : eTF_D32F;
		s_ptexRT_ShadowPool->Invalidate(gcpRendD3D->m_nShadowPoolWidth, gcpRendD3D->m_nShadowPoolHeight, eShadTF);
		if (!CTexture::IsTextureExist(s_ptexRT_ShadowPool))
		{
			s_ptexRT_ShadowPool->CreateDepthStencil(eTF_Unknown, ColorF(1.0f, 5, 0.f, 0.f));
		}
	}

	// shadow mask
	{
		if (s_ptexShadowMask)
			s_ptexShadowMask->Invalidate(resolution.x, resolution.y, eTF_R8);

		if (!CTexture::IsTextureExist(s_ptexShadowMask))
		{
			const int nArraySize = gcpRendD3D->CV_r_ShadowCastingLightsMaxCount;
			s_ptexShadowMask = CTexture::GetOrCreateTextureArray("$ShadowMask", resolution.x, resolution.y, nArraySize, 1, eTT_2DArray, FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_R8, TO_SHADOWMASK);
		}
	}

	// height map AO mask
	if (CRenderer::CV_r_HeightMapAO > 0)
	{
		const int shift = clamp_tpl(3 - CRenderer::CV_r_HeightMapAO, 0, 2);

		int hmaoWidth = resolution.x;
		int hmaoHeight = resolution.y;
		for (int i = 0; i < shift; i++)
		{
			hmaoWidth = (hmaoWidth + 1) / 2;
			hmaoHeight = (hmaoHeight + 1) / 2;
		}

		for (int i = 0; i < 2; ++i)
		{
			if (s_ptexHeightMapAO[i])
				s_ptexHeightMapAO[i]->Invalidate(hmaoWidth, hmaoHeight, eTF_R8G8);

			if (!CTexture::IsTextureExist(s_ptexHeightMapAO[i]))
			{
				char buf[128];
				cry_sprintf(buf, "$HeightMapAO_%d", i);

				SD3DPostEffectsUtils::GetOrCreateRenderTarget(buf, s_ptexHeightMapAO[i], hmaoWidth, hmaoHeight, Clr_Neutral, true, false, eTF_R8G8);
			}
		}
	}
}

void CRendererResources::DestroyDeferredMaps()
{
	SAFE_RELEASE(s_ptexSceneNormalsMap);
	SAFE_RELEASE(s_ptexSceneNormalsBent);
	SAFE_RELEASE(s_ptexAOColorBleed);
	SAFE_RELEASE(s_ptexSceneDiffuseTmp);
	SAFE_RELEASE(s_ptexSceneSpecularTmp);
	SAFE_RELEASE(s_ptexSceneDiffuse);
	SAFE_RELEASE(s_ptexSceneSpecular);
#if defined(DURANGO_USE_ESRAM)
	SAFE_RELEASE(s_ptexSceneSpecularESRAM);
#endif
	SAFE_RELEASE(s_ptexVelocityObjects[0]);
	SAFE_RELEASE(s_ptexVelocityObjects[1]);
	SAFE_RELEASE(s_ptexLinearDepthScaled[0]);
	SAFE_RELEASE(s_ptexLinearDepthScaled[1]);
	SAFE_RELEASE(s_ptexLinearDepthScaled[2]);
	SAFE_RELEASE(s_ptexSceneDepthScaled[0]);
	SAFE_RELEASE(s_ptexSceneDepthScaled[1]);
	SAFE_RELEASE(s_ptexSceneDepthScaled[2]);

	// shadow pool
	// shadow mask
	SAFE_RELEASE(s_ptexRT_ShadowPool);
	SAFE_RELEASE(s_ptexShadowMask);

	// height map AO mask
	SAFE_RELEASE(s_ptexHeightMapAO[0]);
	SAFE_RELEASE(s_ptexHeightMapAO[1]);
}

//==================================================================================================

void CRendererResources::CreateHDRMaps(int resourceWidth, int resourceHeight)
{
	int i;
	char szName[256];

	CD3D9Renderer* r = gcpRendD3D;
	SRenderTargetPool* pHDRPostProcess = SRenderTargetPool::GetInstance();

	const int width = resourceWidth, width_r2 = (width + 1) / 2, width_r4 = (width_r2 + 1) / 2, width_r8 = (width_r4 + 1) / 2, width_r16 = (width_r8 + 1) / 2;
	const int height = resourceHeight, height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2, height_r16 = (height_r8 + 1) / 2;

	pHDRPostProcess->ClearRenderTargetList();

	ETEX_Format nHDRFormat = CRenderer::CV_r_HDRTexFormat == 0 ? eTF_R11G11B10F : eTF_R16G16B16A16F;

	uint32 nHDRTargetFlags = FT_DONT_RELEASE;
	uint32 nHDRTargetFlagsUAV = nHDRTargetFlags | (FT_USAGE_UNORDERED_ACCESS);  // UAV required for tiled deferred shading
	pHDRPostProcess->AddRenderTarget(width, height, Clr_Unknown, nHDRFormat, 1.0f, "$HDRTarget", &s_ptexHDRTarget, nHDRTargetFlagsUAV);
	pHDRPostProcess->AddRenderTarget(width, height, Clr_Unknown, eTF_R11G11B10F, 1.0f, "$HDRTargetPrev", &s_ptexHDRTargetPrev);

	// Scaled versions of the HDR scene texture
	pHDRPostProcess->AddRenderTarget(width_r2, height_r2, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaled0", &s_ptexHDRTargetScaled[0], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r2, height_r2, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTmp0", &s_ptexHDRTargetScaledTmp[0], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r2, height_r2, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTempRT0", &s_ptexHDRTargetScaledTempRT[0], FT_DONT_RELEASE);

	pHDRPostProcess->AddRenderTarget(width_r4, height_r4, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaled1", &s_ptexHDRTargetScaled[1], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r4, height_r4, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTmp1", &s_ptexHDRTargetScaledTmp[1], FT_DONT_RELEASE);
	pHDRPostProcess->AddRenderTarget(width_r4, height_r4, Clr_Unknown, nHDRFormat, 0.9f, "$HDRTargetScaledTempRT1", &s_ptexHDRTargetScaledTempRT[1], FT_DONT_RELEASE);

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
		pHDRPostProcess->AddRenderTarget(1, 1, Clr_White, eTF_R16G16F, 0.1f, szName, &s_ptexHDRAdaptedLuminanceCur[i], FT_DONT_RELEASE);
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
}

void CRendererResources::DestroyHDRMaps()
{
	int i;

	SAFE_RELEASE_FORCE(s_ptexHDRTarget);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetPrev);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaled[0]);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaled[1]);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaled[2]);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaled[3]);

	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaledTmp[0]);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaledTmp[1]);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaledTmp[3]);

	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaledTempRT[0]);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaledTempRT[1]);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaledTempRT[2]);
	SAFE_RELEASE_FORCE(s_ptexHDRTargetScaledTempRT[3]);

	SAFE_RELEASE_FORCE(s_ptexHDRTempBloom[0]);
	SAFE_RELEASE_FORCE(s_ptexHDRTempBloom[1]);
	SAFE_RELEASE_FORCE(s_ptexHDRFinalBloom);

	for (i = 0; i < 8; i++)
	{
		SAFE_RELEASE_FORCE(s_ptexHDRAdaptedLuminanceCur[i]);
	}

	for (i = 0; i < NUM_HDR_TONEMAP_TEXTURES; i++)
	{
		SAFE_RELEASE_FORCE(s_ptexHDRToneMaps[i]);
	}
	SAFE_RELEASE_FORCE(s_ptexHDRMeasuredLuminanceDummy);
	for (i = 0; i < MAX_GPU_NUM; ++i)
	{
		SAFE_RELEASE_FORCE(s_ptexHDRMeasuredLuminance[i]);
	}

	s_ptexCurLumTexture = NULL;

	SAFE_RELEASE_FORCE(s_ptexVelocity);
	SAFE_RELEASE_FORCE(s_ptexVelocityTiles[0]);
	SAFE_RELEASE_FORCE(s_ptexVelocityTiles[1]);
	SAFE_RELEASE_FORCE(s_ptexVelocityTiles[2]);

	SAFE_RELEASE_FORCE(s_ptexHDRDofLayers[0]);
	SAFE_RELEASE_FORCE(s_ptexHDRDofLayers[1]);
	SAFE_RELEASE_FORCE(s_ptexSceneCoCTemp);
	for (i = 0; i < MIN_DOF_COC_K; i++)
	{
		SAFE_RELEASE_FORCE(s_ptexSceneCoC[i]);
	}
}

//==================================================================================================

bool CRendererResources::CreatePostFXMaps(int resourceWidth, int resourceHeight)
{
	const bool bCreateCaustics = (CRenderer::CV_r_watervolumecaustics && CRenderer::CV_r_watercaustics && CRenderer::CV_r_watercausticsdeferred) && !CTexture::IsTextureExist(s_ptexWaterCaustics[0]);

	const int width = resourceWidth, width_r2 = (width + 1) / 2, width_r4 = (width_r2 + 1) / 2, width_r8 = (width_r4 + 1) / 2;
	const int height = resourceHeight, height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2;

	if (!s_ptexBackBufferScaled[0] ||
		s_ptexBackBufferScaled[0]->GetWidth() != width_r2 ||
		s_ptexBackBufferScaled[0]->GetHeight() != height_r2 || 
		bCreateCaustics)
	{
		assert(gRenDev);

		SPostEffectsUtils::GetOrCreateRenderTarget("$Cached3DHud", s_ptexCached3DHud, width, height, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);
		SPostEffectsUtils::GetOrCreateRenderTarget("$Cached3DHudDownsampled", s_ptexCached3DHudScaled, width_r4, height_r4, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

		// Scaled versions of the scene target
		SPostEffectsUtils::GetOrCreateRenderTarget("$BackBufferScaled_d2", s_ptexBackBufferScaled[0], width_r2, height_r2, Clr_Unknown, 1, 0, eTF_R8G8B8A8, TO_BACKBUFFERSCALED_D2, FT_DONT_RELEASE);

		// Ghosting requires data overframes, need to handle for each GPU in MGPU mode
		SPostEffectsUtils::GetOrCreateRenderTarget("$PrevFrameScaled", s_ptexPrevFrameScaled, width_r2, height_r2, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

		SPostEffectsUtils::GetOrCreateRenderTarget("$BackBufferScaledTemp_d2", s_ptexBackBufferScaledTemp[0], width_r2, height_r2, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);
		SPostEffectsUtils::GetOrCreateRenderTarget("$WaterVolumeRefl", s_ptexWaterVolumeRefl[0], width_r2, height_r2, Clr_Unknown, 1, true, eTF_R11G11B10F, TO_WATERVOLUMEREFLMAP, FT_DONT_RELEASE);
		SPostEffectsUtils::GetOrCreateRenderTarget("$WaterVolumeReflPrev", s_ptexWaterVolumeRefl[1], width_r2, height_r2, Clr_Unknown, 1, true, eTF_R11G11B10F, TO_WATERVOLUMEREFLMAPPREV, FT_DONT_RELEASE);

		//	s_ptexWaterVolumeRefl[0]->DisableMgpuSync();
		//	s_ptexWaterVolumeRefl[1]->DisableMgpuSync();

		SPostEffectsUtils::GetOrCreateRenderTarget("$BackBufferScaled_d4", s_ptexBackBufferScaled[1], width_r4, height_r4, Clr_Unknown, 1, 0, eTF_R8G8B8A8, TO_BACKBUFFERSCALED_D4, FT_DONT_RELEASE);
		SPostEffectsUtils::GetOrCreateRenderTarget("$BackBufferScaledTemp_d4", s_ptexBackBufferScaledTemp[1], width_r4, height_r4, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

		SPostEffectsUtils::GetOrCreateRenderTarget("$BackBufferScaled_d8", s_ptexBackBufferScaled[2], width_r8, height_r8, Clr_Unknown, 1, 0, eTF_R8G8B8A8, TO_BACKBUFFERSCALED_D8, FT_DONT_RELEASE);

		SPostEffectsUtils::GetOrCreateRenderTarget("$RainSSOcclusion0", s_ptexRainSSOcclusion[0], width_r8, height_r8, Clr_Unknown, 1, false, eTF_R8G8B8A8);
		SPostEffectsUtils::GetOrCreateRenderTarget("$RainSSOcclusion1", s_ptexRainSSOcclusion[1], width_r8, height_r8, Clr_Unknown, 1, false, eTF_R8G8B8A8);

		if (CRenderer::CV_r_watervolumecaustics && CRenderer::CV_r_watercaustics && CRenderer::CV_r_watercausticsdeferred)
		{
			const int nCausticRes = clamp_tpl(CRenderer::CV_r_watervolumecausticsresolution, 256, 4096);
			SPostEffectsUtils::GetOrCreateRenderTarget("$WaterVolumeCaustics", s_ptexWaterCaustics[0], nCausticRes, nCausticRes, Clr_Unknown, 1, false, eTF_R8G8B8A8, TO_WATERVOLUMECAUSTICSMAP);
			SPostEffectsUtils::GetOrCreateRenderTarget("$WaterVolumeCausticsTemp", s_ptexWaterCaustics[1], nCausticRes, nCausticRes, Clr_Unknown, 1, false, eTF_R8G8B8A8, TO_WATERVOLUMECAUSTICSMAPTEMP);
		}
		else
		{
			SAFE_RELEASE(s_ptexWaterCaustics[0]);
			SAFE_RELEASE(s_ptexWaterCaustics[1]);
		}

#if defined(VOLUMETRIC_FOG_SHADOWS)
		int fogShadowBufDiv = (CRenderer::CV_r_FogShadows == 2) ? 4 : 2;
		SPostEffectsUtils::GetOrCreateRenderTarget("$VolFogShadowBuf0", s_ptexVolFogShadowBuf[0], width / fogShadowBufDiv, height / fogShadowBufDiv, Clr_Unknown, 1, 0, eTF_R8G8B8A8, TO_VOLFOGSHADOW_BUF);
		SPostEffectsUtils::GetOrCreateRenderTarget("$VolFogShadowBuf1", s_ptexVolFogShadowBuf[1], width / fogShadowBufDiv, height / fogShadowBufDiv, Clr_Unknown, 1, 0, eTF_R8G8B8A8);
#endif

		// TODO: Only create necessary RTs for minimal ring?
		char str[256];
		for (int i = 0, end = gcpRendD3D->GetActiveGPUCount() * MAX_FRAMES_IN_FLIGHT; i < end; i++)
		{
			sprintf(str, "$FlaresOcclusion_%d", i);
			SPostEffectsUtils::GetOrCreateRenderTarget(str, s_ptexFlaresOcclusionRing[i], CFlareSoftOcclusionQuery::s_nIDColMax, CFlareSoftOcclusionQuery::s_nIDRowMax, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE | FT_STAGE_READBACK);
		}

		SPostEffectsUtils::GetOrCreateRenderTarget("$FlaresGather", s_ptexFlaresGather, CFlareSoftOcclusionQuery::s_nGatherTextureWidth, CFlareSoftOcclusionQuery::s_nGatherTextureHeight, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);
	}

	/*
	 *	The following textures do not need to be recreated on resize
	 */

	if (!CTexture::IsTextureExist(s_ptexRainOcclusion))
		SPostEffectsUtils::GetOrCreateRenderTarget("$RainOcclusion", s_ptexRainOcclusion, RAIN_OCC_MAP_SIZE, RAIN_OCC_MAP_SIZE, Clr_Neutral, false, false, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

	if (!CTexture::IsTextureExist(s_ptexWaterVolumeDDN))
	{
		SPostEffectsUtils::GetOrCreateRenderTarget("$WaterVolumeDDN", s_ptexWaterVolumeDDN, 64, 64, Clr_Unknown, 1, true, eTF_R16G16B16A16F, TO_WATERVOLUMEMAP);
		//s_ptexWaterVolumeDDN->DisableMgpuSync();
	}


	return 1;
}

void CRendererResources::DestroyPostFXMaps()
{
	SAFE_RELEASE_FORCE(s_ptexBackBufferScaled[0]);
	SAFE_RELEASE_FORCE(s_ptexBackBufferScaled[1]);
	SAFE_RELEASE_FORCE(s_ptexBackBufferScaled[2]);

	SAFE_RELEASE_FORCE(s_ptexBackBufferScaledTemp[0]);
	SAFE_RELEASE_FORCE(s_ptexBackBufferScaledTemp[1]);

	SAFE_RELEASE(s_ptexWaterVolumeDDN);
	SAFE_RELEASE_FORCE(s_ptexWaterVolumeRefl[0]);
	SAFE_RELEASE_FORCE(s_ptexWaterVolumeRefl[1]);
	SAFE_RELEASE(s_ptexWaterCaustics[0]);
	SAFE_RELEASE(s_ptexWaterCaustics[1]);

	SAFE_RELEASE_FORCE(s_ptexCached3DHud);
	SAFE_RELEASE_FORCE(s_ptexCached3DHudScaled);

	SAFE_RELEASE_FORCE(s_ptexPrevFrameScaled);

	SAFE_RELEASE(s_ptexRainSSOcclusion[0]);
	SAFE_RELEASE(s_ptexRainSSOcclusion[1]);
	SAFE_RELEASE_FORCE(s_ptexRainOcclusion);

#if defined(VOLUMETRIC_FOG_SHADOWS)
	SAFE_RELEASE(s_ptexVolFogShadowBuf[0]);
	SAFE_RELEASE(s_ptexVolFogShadowBuf[1]);
#endif

	for (int i = 0; i < MAX_OCCLUSION_READBACK_TEXTURES; i++)
		SAFE_RELEASE_FORCE(s_ptexFlaresOcclusionRing[i]);

	SAFE_RELEASE_FORCE(s_ptexFlaresGather);
}

//==================================================================================================

void CRendererResources::CreateCachedShadowMaps()
{
	StaticArray<int, MAX_GSM_LODS_NUM> nResolutions = gRenDev->GetCachedShadowsResolution();

	// parse shadow resolutions from cvar
	{
		int nCurPos = 0;
		int nCurRes = 0;

		string strResolutions = gEnv->pConsole->GetCVar("r_ShadowsCacheResolutions")->GetString();
		string strCurRes = strResolutions.Tokenize(" ,;-\t", nCurPos);

		if (!strCurRes.empty())
		{
			nResolutions.fill(0);

			while (!strCurRes.empty())
			{
				int nRes = atoi(strCurRes.c_str());
				nResolutions[nCurRes] = clamp_tpl(nRes, 0, 16384);

				strCurRes = strResolutions.Tokenize(" ,;-\t", nCurPos);
				++nCurRes;
			}

			gRenDev->SetCachedShadowsResolution(nResolutions);
		}
	}

	const ETEX_Format texFormat = CRendererCVars::CV_r_ShadowsCacheFormat == 0 ? eTF_D32F : eTF_D16;
	const int cachedShadowsStart = clamp_tpl(CRendererCVars::CV_r_ShadowsCache, 0, MAX_GSM_LODS_NUM - 1);

	int gsmCascadeCount = gEnv->pSystem->GetConfigSpec() == CONFIG_LOW_SPEC ? 4 : 5;
	if (ICVar* pGsmLodsVar = gEnv->pConsole->GetCVar("e_GsmLodsNum"))
		gsmCascadeCount = pGsmLodsVar->GetIVal();
	const int cachedCascadesCount = cachedShadowsStart > 0 ? clamp_tpl(gsmCascadeCount - cachedShadowsStart + 1, 0, MAX_GSM_LODS_NUM) : 0;

	for (int i = 0; i < MAX_GSM_LODS_NUM; ++i)
	{
		CTexture*& pTx = s_ptexCachedShadowMap[i];

		if (!pTx)
		{
			char szName[32];
			cry_sprintf(szName, "CachedShadowMap_%d", i);

			pTx = CTexture::GetOrCreateTextureObject(szName, nResolutions[i], nResolutions[i], 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, texFormat);
		}

		pTx->Invalidate(nResolutions[i], nResolutions[i], texFormat);

		// delete existing texture in case it's not needed anymore
		if (CTexture::IsTextureExist(pTx) && nResolutions[i] == 0)
			pTx->ReleaseDeviceTexture(false);

		// allocate texture directly for all cached cascades
		if (!CTexture::IsTextureExist(pTx) && nResolutions[i] > 0 && i < cachedCascadesCount)
		{
			CryLog("Allocating shadow map cache %d x %d: %.2f MB", nResolutions[i], nResolutions[i], sqr(nResolutions[i]) * CTexture::BitsPerPixel(texFormat) / (1024.f * 1024.f * 8.f));
			pTx->CreateDepthStencil(texFormat, ColorF(1.0f, 1.0f, 1.0f, 1.0f));
		}
	}

	// height map AO
	if (CRendererCVars::CV_r_HeightMapAO)
	{
		const int nTexRes = (int)clamp_tpl(CRendererCVars::CV_r_HeightMapAOResolution, 0.f, 16384.f);
		ETEX_Format texFormatMips = texFormat == eTF_D32F ? eTF_R32F : eTF_R16;
		// Allow non-supported SNORM/UNORM to fall back to a FLOAT format with slightly less precision
		texFormatMips = CRendererResources::s_hwTexFormatSupport.GetLessPreciseFormatSupported(texFormatMips);

		if (!s_ptexHeightMapAODepth[0])
		{
			s_ptexHeightMapAODepth[0] = CTexture::GetOrCreateTextureObject("HeightMapAO_Depth_0", nTexRes, nTexRes, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, texFormat);
			s_ptexHeightMapAODepth[1] = CTexture::GetOrCreateTextureObject("HeightMapAO_Depth_1", nTexRes, nTexRes, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, texFormatMips);
		}

		s_ptexHeightMapAODepth[0]->Invalidate(nTexRes, nTexRes, texFormat);
		s_ptexHeightMapAODepth[1]->Invalidate(nTexRes, nTexRes, texFormatMips);

		if (!CTexture::IsTextureExist(s_ptexHeightMapAODepth[0]) && nTexRes > 0)
		{
			s_ptexHeightMapAODepth[0]->CreateDepthStencil(texFormat    , ColorF(1.0f, 1.0f, 1.0f, 1.0f));
			s_ptexHeightMapAODepth[1]->CreateRenderTarget(texFormatMips, ColorF(1.0f, 1.0f, 1.0f, 1.0f));
		}
	}

	if (ShadowFrustumMGPUCache* pShadowMGPUCache = gRenDev->GetShadowFrustumMGPUCache())
	{
		pShadowMGPUCache->nUpdateMaskRT = 0;
		pShadowMGPUCache->nUpdateMaskMT = 0;
	}
}

void CRendererResources::DestroyCachedShadowMaps()
{
	for (int i = 0; i < MAX_GSM_LODS_NUM; ++i)
	{
		SAFE_RELEASE_FORCE(s_ptexCachedShadowMap[i]);
	}

	SAFE_RELEASE_FORCE(s_ptexHeightMapAODepth[0]);
	SAFE_RELEASE_FORCE(s_ptexHeightMapAODepth[1]);
}

//==================================================================================================

void CRendererResources::CreateNearestShadowMap()
{
	const int texResolution = CRendererCVars::CV_r_ShadowsNearestMapResolution;
	const ETEX_Format texFormat = CRendererCVars::CV_r_shadowtexformat == 0 ? eTF_D32F : eTF_D16;

	s_ptexNearestShadowMap = CTexture::GetOrCreateTextureObject("NearestShadowMap", texResolution, texResolution, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, texFormat);
}

void CRendererResources::DestroyNearestShadowMap()
{
	SAFE_RELEASE_FORCE(s_ptexNearestShadowMap);
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

void CRendererResources::Clear()
{
	// Clear core render targets on shut down, just to be sure
	// they are in a sane state for next render and prevent flicks
	CTexture* clearTextures[] =
	{
		s_ptexSceneNormalsMap,
		s_ptexSceneDiffuse,
		s_ptexSceneSpecular,
		s_ptexSceneDiffuseTmp,
		s_ptexSceneSpecularTmp,
		s_ptexBackBuffer,
		s_ptexSceneTarget,
		s_ptexLinearDepth,
		s_ptexHDRTarget
	};

	for (auto pTex : clearTextures)
	{
		if (CTexture::IsTextureExist(pTex))
		{
			CClearSurfacePass::Execute(pTex, pTex->GetClearColor());
		}
	}
}

void CRendererResources::ShutDown()
{
	UnloadDefaultSystemTextures(true);

	if (CRenderer::CV_r_releaseallresourcesonexit)
	{
		// To avoid crash on ShutDown
		s_ptexSceneNormalsMap = NULL;
		s_ptexSceneDiffuse = NULL;
		s_ptexSceneSpecular = NULL;
		s_ptexSceneDiffuseTmp = NULL;
		s_ptexSceneSpecularTmp = NULL;
		s_ptexBackBuffer = NULL;
		s_ptexSceneTarget = NULL;
		s_ptexLinearDepth = NULL;
		s_ptexHDRTarget = NULL;
	}

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
	cry_sprintf(pName, "$DynDepthStencil_%8_x%d", m_DTallocs + 1, m_DTallocs + 2); ++m_DTallocs;
	return CTexture::GetOrCreateDepthStencil(pName, nWidth, nHeight, cClear, eTT_2D, FT_USAGE_TEMPORARY | FT_NOMIPS, eTF == eTF_Unknown ? GetDepthFormat() : eTF);
}

CTexture* CRendererResources::CreateRenderTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF)
{
	char pName[128]; // Create unique names for every allocation, otherwise name-matches would occur in GetOrCreateRenderTarget()
	cry_sprintf(pName, "$RenderTarget%8x", ++m_RTallocs);
	auto pTarget = CTexture::GetOrCreateRenderTarget(pName, nWidth, nHeight, cClear, eTT_2D, FT_USAGE_TEMPORARY | FT_NOMIPS, eTF);

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	pTarget->GetDevTexture()->Get2DTexture()->SetPrivateData(WKPDID_D3DDebugObjectName, strlen("Dynamically requested Color-Target"), "Dynamically requested Color-Target");
#endif

	return pTarget;
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
	SEnvTexture* cm = NULL;
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
			objPos = pObj->GetMatrix(*pPassInfo).GetTranslation();
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
