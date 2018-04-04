// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*=============================================================================
   Texture.cpp : Common texture manager implementation.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include <Cry3DEngine/ImageExtensionHelper.h>
#include "Image/CImage.h"
#include "Image/DDSImage.h"
#include "TextureManager.h"
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CrySystem/File/IResourceManager.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryString/StringUtils.h>                // stristr()
#include "TextureStreamPool.h"
#include "TextureHelpers.h"
#include <CrySystem/Scaleform/IUIFramework.h>
#include <CrySystem/Scaleform/IScaleformHelper.h>

// class CMipmapGenPass;
#include "../../XRenderD3D9/GraphicsPipeline/Common/UtilityPasses.h"

#define TEXTURE_LEVEL_CACHE_PAK "dds0.pak"

SSamplerState SSamplerState::s_sDefState;

STexStageInfo CTexture::s_TexStages[MAX_TMU];
SamplerStateHandle CTexture::s_TexStateIDs[eHWSC_Num][MAX_TMU];
int CTexture::s_nStreamingMode;
int CTexture::s_nStreamingUpdateMode;
bool CTexture::s_bPrecachePhase;
bool CTexture::s_bInLevelPhase = false;
bool CTexture::s_bPrestreamPhase;
int CTexture::s_nStreamingThroughput = 0;
float CTexture::s_nStreamingTotalTime = 0;
CTextureStreamPoolMgr* CTexture::s_pPoolMgr;
std::set<string> CTexture::s_vTexReloadRequests;
CryCriticalSection CTexture::s_xTexReloadLock;
#ifdef TEXTURE_GET_SYSTEM_COPY_SUPPORT
CTexture::LowResSystemCopyType CTexture::s_LowResSystemCopy;
#endif

bool CTexture::m_bLoadedSystem;

// ==============================================================================
CTexture* CTexture::s_ptexNoTexture;
CTexture* CTexture::s_ptexNoTextureCM;
CTexture* CTexture::s_ptexWhite;
CTexture* CTexture::s_ptexGray;
CTexture* CTexture::s_ptexBlack;
CTexture* CTexture::s_ptexBlackAlpha;
CTexture* CTexture::s_ptexBlackCM;
CTexture* CTexture::s_ptexDefaultProbeCM;
CTexture* CTexture::s_ptexDefaultMergedDetail;
CTexture* CTexture::s_ptexFlatBump;
#if !defined(_RELEASE)
CTexture* CTexture::s_ptexMipMapDebug;
CTexture* CTexture::s_ptexColorBlue;
CTexture* CTexture::s_ptexColorCyan;
CTexture* CTexture::s_ptexColorGreen;
CTexture* CTexture::s_ptexColorPurple;
CTexture* CTexture::s_ptexColorRed;
CTexture* CTexture::s_ptexColorWhite;
CTexture* CTexture::s_ptexColorYellow;
CTexture* CTexture::s_ptexColorOrange;
CTexture* CTexture::s_ptexColorMagenta;
#endif
CTexture* CTexture::s_ptexPaletteDebug;
CTexture* CTexture::s_ptexPaletteTexelsPerMeter;
CTexture* CTexture::s_ptexIconShaderCompiling;
CTexture* CTexture::s_ptexIconStreaming;
CTexture* CTexture::s_ptexIconStreamingTerrainTexture;
CTexture* CTexture::s_ptexIconNavigationProcessing;
CTexture* CTexture::s_ptexMipColors_Diffuse;
CTexture* CTexture::s_ptexMipColors_Bump;
CTexture* CTexture::s_ptexShadowJitterMap;
CTexture* CTexture::s_ptexEnvironmentBRDF;
CTexture* CTexture::s_ptexScreenNoiseMap;
CTexture* CTexture::s_ptexDissolveNoiseMap;
CTexture* CTexture::s_ptexNoise3D;
CTexture* CTexture::s_ptexGrainFilterMap;
CTexture* CTexture::s_ptexFilmGrainMap;
CTexture* CTexture::s_ptexVignettingMap;
CTexture* CTexture::s_ptexAOJitter;
CTexture* CTexture::s_ptexAOVOJitter;
CTexture* CTexture::s_ptexFromRE[8];
CTexture* CTexture::s_ptexShadowID[8];
CTexture* CTexture::s_ptexShadowMask;
CTexture* CTexture::s_ptexCachedShadowMap[MAX_GSM_LODS_NUM];
CTexture* CTexture::s_ptexNearestShadowMap;
CTexture* CTexture::s_ptexHeightMapAO[2];
CTexture* CTexture::s_ptexHeightMapAODepth[2];
CTexture* CTexture::s_ptexFromRE_FromContainer[2];
CTexture* CTexture::s_ptexFromObj;
CTexture* CTexture::s_ptexSvoTree;
CTexture* CTexture::s_ptexSvoTris;
CTexture* CTexture::s_ptexSvoGlobalCM;
CTexture* CTexture::s_ptexSvoRgbs;
CTexture* CTexture::s_ptexSvoNorm;
CTexture* CTexture::s_ptexSvoOpac;
CTexture* CTexture::s_ptexRT_2D;
CTexture* CTexture::s_ptexNormalsFitting;
CTexture* CTexture::s_ptexPerlinNoiseMap;

CTexture* CTexture::s_ptexSceneNormalsMap;
CTexture* CTexture::s_ptexSceneNormalsMapMS;
CTexture* CTexture::s_ptexSceneNormalsBent;
CTexture* CTexture::s_ptexAOColorBleed;
CTexture* CTexture::s_ptexSceneDiffuse;
CTexture* CTexture::s_ptexSceneSpecular;

CTexture* CTexture::s_ptexSceneSelectionIDs;
CTexture* CTexture::s_ptexSceneHalfDepthStencil;

CTexture* CTexture::s_ptexWindGrid;

#if defined(DURANGO_USE_ESRAM)
CTexture* CTexture::s_ptexSceneSpecularESRAM;
#endif

// Post-process related textures
CTexture* CTexture::s_ptexBackBuffer = NULL;
CTexture* CTexture::s_ptexModelHudBuffer;
CTexture* CTexture::s_ptexPrevBackBuffer[2][2] = {
	{ NULL }
};
CTexture* CTexture::s_ptexCached3DHud;
CTexture* CTexture::s_ptexCached3DHudScaled;
CTexture* CTexture::s_ptexBackBufferScaled[3];
CTexture* CTexture::s_ptexBackBufferScaledTemp[2];
CTexture* CTexture::s_ptexPrevFrameScaled;

CTexture* CTexture::s_ptexDepthBufferQuarter;
CTexture* CTexture::s_ptexDepthBufferHalfQuarter;

CTexture* CTexture::s_ptexWaterOcean;
CTexture* CTexture::s_ptexWaterVolumeTemp[2];
CTexture* CTexture::s_ptexWaterVolumeDDN;
CTexture* CTexture::s_ptexWaterVolumeRefl[2] = { NULL };
CTexture* CTexture::s_ptexWaterCaustics[2] = { NULL };
CTexture* CTexture::s_ptexRainOcclusion;
CTexture* CTexture::s_ptexRainSSOcclusion[2];

CTexture* CTexture::s_ptexRainDropsRT[2];

CTexture* CTexture::s_ptexRT_ShadowPool;
CTexture* CTexture::s_ptexFarPlane;
CTexture* CTexture::s_ptexCloudsLM;

CTexture* CTexture::s_ptexSceneTarget = NULL;
CTexture* CTexture::s_ptexSceneTargetR11G11B10F[2] = { NULL };
CTexture* CTexture::s_ptexSceneTargetScaledR11G11B10F[4] = { NULL };
CTexture* CTexture::s_ptexCurrSceneTarget;
CTexture* CTexture::s_ptexCurrentSceneDiffuseAccMap;
CTexture* CTexture::s_ptexSceneDiffuseAccMap;
CTexture* CTexture::s_ptexSceneSpecularAccMap;
CTexture* CTexture::s_ptexSceneDiffuseAccMapMS;
CTexture* CTexture::s_ptexSceneSpecularAccMapMS;
CTexture* CTexture::s_ptexZTarget;
CTexture* CTexture::s_ptexZOcclusion[2];
CTexture* CTexture::s_ptexZTargetReadBack[4];
CTexture* CTexture::s_ptexZTargetDownSample[4];
CTexture* CTexture::s_ptexZTargetScaled;
CTexture* CTexture::s_ptexZTargetScaled2;
CTexture* CTexture::s_ptexZTargetScaled3;
CTexture* CTexture::s_ptexHDRTarget;
CTexture* CTexture::s_ptexVelocity;
CTexture* CTexture::s_ptexVelocityTiles[3] = { NULL };
CTexture* CTexture::s_ptexVelocityObjects[2] = { NULL };
CTexture* CTexture::s_ptexHDRTargetPrev = NULL;
CTexture* CTexture::s_ptexHDRTargetScaled[4];
CTexture* CTexture::s_ptexHDRTargetScaledTmp[4];
CTexture* CTexture::s_ptexHDRTargetScaledTempRT[4];
CTexture* CTexture::s_ptexHDRDofLayers[2];
CTexture* CTexture::s_ptexSceneCoC[MIN_DOF_COC_K] = { NULL };
CTexture* CTexture::s_ptexSceneCoCTemp = NULL;
CTexture* CTexture::s_ptexHDRTempBloom[2];
CTexture* CTexture::s_ptexHDRFinalBloom;
CTexture* CTexture::s_ptexHDRAdaptedLuminanceCur[8];
int CTexture::s_nCurLumTextureIndex;
CTexture* CTexture::s_ptexCurLumTexture;
CTexture* CTexture::s_ptexHDRToneMaps[NUM_HDR_TONEMAP_TEXTURES];
CTexture* CTexture::s_ptexHDRMeasuredLuminance[MAX_GPU_NUM];
CTexture* CTexture::s_ptexHDRMeasuredLuminanceDummy;
CTexture* CTexture::s_ptexSkyDomeMie;
CTexture* CTexture::s_ptexSkyDomeRayleigh;
CTexture* CTexture::s_ptexSkyDomeMoon;
CTexture* CTexture::s_ptexColorChart;
CTexture* CTexture::s_ptexSceneTargetScaled;
CTexture* CTexture::s_ptexSceneTargetScaledBlurred;
CTexture* CTexture::s_ptexStereoL = NULL;
CTexture* CTexture::s_ptexStereoR = NULL;
CTexture* CTexture::s_ptexQuadLayers[2] = { NULL };

CTexture* CTexture::s_ptexFlaresOcclusionRing[MAX_OCCLUSION_READBACK_TEXTURES] = { NULL };
CTexture* CTexture::s_ptexFlaresGather = NULL;

SEnvTexture CTexture::s_EnvTexts[MAX_ENVTEXTURES];

TArray<SEnvTexture> CTexture::s_CustomRT_2D;

TArray<CTexture> CTexture::s_ShaderTemplates(EFTT_MAX);
bool CTexture::s_ShaderTemplatesInitialized = false;

CTexture* CTexture::s_pTexNULL = 0;

CTexture* CTexture::s_pBackBuffer;
CTexture* CTexture::s_FrontBufferTextures[2] = { NULL };

CTexture* CTexture::s_ptexVolumetricFog = NULL;
CTexture* CTexture::s_ptexVolumetricClipVolumeStencil = NULL;

CTexture* CTexture::s_ptexVolCloudShadow = NULL;

#if defined(TEXSTRM_DEFERRED_UPLOAD)
ID3D11DeviceContext* CTexture::s_pStreamDeferredCtx = nullptr;
#endif

#if defined(VOLUMETRIC_FOG_SHADOWS)
CTexture* CTexture::s_ptexVolFogShadowBuf[2] = { 0 };
#endif

ETEX_Format CTexture::s_eTFZ = eTF_R32F;

//============================================================

SResourceView SResourceView::ShaderResourceView(DXGI_FORMAT nFormat, int nFirstSlice, int nSliceCount, int nMostDetailedMip, int nMipCount, bool bSrgbRead, bool bMultisample, int nFlags)
{
	SResourceView result(0);

	result.m_Desc.eViewType = eShaderResourceView;
	result.m_Desc.nFormat = nFormat;
	result.m_Desc.nFirstSlice = nFirstSlice;
	result.m_Desc.nSliceCount = nSliceCount;
	result.m_Desc.nMostDetailedMip = nMostDetailedMip;
	result.m_Desc.nMipCount = nMipCount;
	result.m_Desc.bSrgbRead = bSrgbRead ? 1 : 0;
	result.m_Desc.nFlags = nFlags;
	result.m_Desc.bMultisample = bMultisample ? 1 : 0;

	return result;
}

SResourceView SResourceView::RenderTargetView(DXGI_FORMAT nFormat, int nFirstSlice, int nSliceCount, int nMipLevel, bool bMultisample)
{
	SResourceView result(0);

	result.m_Desc.eViewType = eRenderTargetView;
	result.m_Desc.nFormat = nFormat;
	result.m_Desc.nFirstSlice = nFirstSlice;
	result.m_Desc.nSliceCount = nSliceCount;
	result.m_Desc.nMostDetailedMip = nMipLevel;
	result.m_Desc.nMipCount = 1;
	result.m_Desc.bMultisample = bMultisample ? 1 : 0;

	return result;
}

SResourceView SResourceView::DepthStencilView(DXGI_FORMAT nFormat, int nFirstSlice, int nSliceCount, int nMipLevel, bool bMultisample, int nFlags)
{
	SResourceView result(0);

	result.m_Desc.eViewType = eDepthStencilView;
	result.m_Desc.nFormat = nFormat;
	result.m_Desc.nFirstSlice = nFirstSlice;
	result.m_Desc.nSliceCount = nSliceCount;
	result.m_Desc.nMostDetailedMip = nMipLevel;
	result.m_Desc.nMipCount = 1;
	result.m_Desc.nFlags = nFlags;
	result.m_Desc.bMultisample = bMultisample ? 1 : 0;

	return result;
}

SResourceView SResourceView::UnorderedAccessView(DXGI_FORMAT nFormat, int nFirstSlice, int nSliceCount, int nMipLevel, int nFlags)
{
	SResourceView result(0);

	result.m_Desc.eViewType = eUnorderedAccessView;
	result.m_Desc.nFormat = nFormat;
	result.m_Desc.nFirstSlice = nFirstSlice;
	result.m_Desc.nSliceCount = nSliceCount;
	result.m_Desc.nMostDetailedMip = nMipLevel;
	result.m_Desc.nMipCount = 1;
	result.m_Desc.nFlags = nFlags;

	return result;
}

//============================================================

template<typename T>
static inline uint32 ConvertFromTextureFlags(ETextureFlags eFlags)
{
	// NOTE  Currently without correspondence:
	//
	//  FT_DONT_RELEASE
	//  FT_USAGE_MSAA
	//  FT_FROMIMAGE
	//  FT_USAGE_ALLOWREADSRGB

	// *INDENT-OFF*
	return
		(!(eFlags & FT_DONT_READ             ) ?  CDeviceObjectFactory::BIND_SHADER_RESOURCE                                        : 0) |
		( (eFlags & FT_USAGE_RENDERTARGET    ) ?  CDeviceObjectFactory::BIND_RENDER_TARGET                                          : 0) |
		( (eFlags & FT_USAGE_DEPTHSTENCIL    ) ?  CDeviceObjectFactory::BIND_DEPTH_STENCIL                                          : 0) |
		( (eFlags & FT_USAGE_UNORDERED_ACCESS) ?  CDeviceObjectFactory::BIND_UNORDERED_ACCESS                                       : 0) |
		( (eFlags & FT_USAGE_DYNAMIC         ) ?  CDeviceObjectFactory::USAGE_CPU_WRITE                                             : 0) |
		(!(eFlags & FT_DONT_STREAM           ) ?  CDeviceObjectFactory::USAGE_STREAMING                                             : 0) |
		( (eFlags & FT_STAGE_READBACK        ) ? (CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_READ ) : 0) |
		( (eFlags & FT_STAGE_UPLOAD          ) ? (CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_WRITE) : 0) |
		( (eFlags & FT_FORCE_MIPS            ) ?  CDeviceObjectFactory::USAGE_AUTOGENMIPS                                           : 0) |
		( (eFlags & FT_USAGE_UAV_RWTEXTURE   ) ?  CDeviceObjectFactory::USAGE_UAV_READWRITE                                         : 0);
	// *INDENT-ON*
}

template<typename T>
static inline ETextureFlags ConvertToTextureFlags(uint32 eFlags)
{
	// NOTE  Currently without correspondence:
	//
	//  FT_DONT_RELEASE
	//  FT_USAGE_MSAA
	//  FT_FROMIMAGE
	//  FT_USAGE_ALLOWREADSRGB

	// *INDENT-OFF*
	return ETextureFlags(
		(!(eFlags &  CDeviceObjectFactory::BIND_SHADER_RESOURCE                                       ) ? FT_DONT_READ              : 0) |
		( (eFlags &  CDeviceObjectFactory::BIND_RENDER_TARGET                                         ) ? FT_USAGE_RENDERTARGET     : 0) |
		( (eFlags &  CDeviceObjectFactory::BIND_DEPTH_STENCIL                                         ) ? FT_USAGE_DEPTHSTENCIL     : 0) |
		( (eFlags &  CDeviceObjectFactory::BIND_UNORDERED_ACCESS                                      ) ? FT_USAGE_UNORDERED_ACCESS : 0) |
		(((eFlags & (CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_WRITE ))
		                                                     == CDeviceObjectFactory::USAGE_CPU_WRITE ) ? FT_USAGE_DYNAMIC          : 0) |
		(!(eFlags &  CDeviceObjectFactory::USAGE_STREAMING                                            ) ? FT_DONT_STREAM            : 0) |
		( (eFlags & (CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_READ )) ? FT_STAGE_READBACK         : 0) |
		( (eFlags & (CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_WRITE)) ? FT_STAGE_UPLOAD           : 0) |
		( (eFlags &  CDeviceObjectFactory::USAGE_AUTOGENMIPS                                          ) ? FT_FORCE_MIPS             : 0) |
		( (eFlags &  CDeviceObjectFactory::USAGE_UAV_READWRITE                                        ) ? FT_USAGE_UAV_RWTEXTURE    : 0));
	// *INDENT-ON*
}

//============================================================
CTexture::CTexture(const uint32 nFlags, const ColorF& clearColor /*= ColorF(Clr_Empty)*/, CDeviceTexture* devTexToOwn /*= nullptr*/)
{
	m_eFlags = nFlags;
	m_eDstFormat = eTF_Unknown;
	m_eSrcFormat = eTF_Unknown;
	m_nMips = 1;
	m_nWidth = 0;
	m_nHeight = 0;
	m_eTT = eTT_2D;
	m_nDepth = 1;
	m_nArraySize = 1;
	m_nDevTextureSize = 0;
	m_fAvgBrightness = 1.0f;
	m_cMinColor = 0.0f;
	m_cMaxColor = 1.0f;
	m_cClearColor = clearColor;
	m_nPersistentSize = 0;
	m_fAvgBrightness = 0.0f;

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	m_nDeviceAddressInvalidated = 0;
#endif
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	m_nESRAMOffset = SKIP_ESRAM;
#endif

	m_nUpdateFrameID = -1;
	m_nAccessFrameID = -1;
	m_nCustomID = -1;
	m_pPixelFormat = NULL;
	m_pDevTexture = NULL;

	m_bAsyncDevTexCreation = false;

	m_bIsLocked = false;
	m_bNeedRestoring = false;
	m_bNoTexture = false;
	m_bResolved = true;
	m_bUseMultisampledRTV = true;
	m_bHighQualityFiltering = false;
	m_bCustomFormat = false;
	m_eSrcTileMode = eTM_Unspecified;

	m_bPostponed = false;
	m_bForceStreamHighRes = false;
	m_bWasUnloaded = false;
	m_bStreamed = false;
	m_bStreamPrepared = false;
	m_bStreamRequested = false;
	m_bVertexTexture = false;
	m_bUseDecalBorderCol = false;
	m_bIsSRGB = false;
	m_bNoDevTexture = false;
	m_bInDistanceSortedList = false;
	m_bCreatedInLevel = gRenDev->m_bInLevel;
	m_bUsedRecently = 0;
	m_bStatTracked = 0;
	m_bStreamHighPriority = 0;
	m_nStreamingPriority = 0;
	m_nMinMipVidUploaded = MAX_MIP_LEVELS;
	m_nMinMipVidActive = MAX_MIP_LEVELS;
	m_nStreamSlot = InvalidStreamSlot;
	m_fpMinMipCur = MAX_MIP_LEVELS << 8;
	m_nStreamFormatCode = 0;

	m_nDefState = EDefaultSamplerStates::PointClamp;
	m_pFileTexMips = NULL;
	m_fCurrentMipBias = 0.f;

	static_assert(MaxStreamTasks < 32767, "Too many stream tasks!");

	if (devTexToOwn)
	{
		OwnDevTexture(devTexToOwn);
	}
}

//============================================================

CTexture::~CTexture()
{
	InvalidateDeviceResource(this, eResourceDestroyed);

	// sizes of these structures should NOT exceed L2 cache line!
#if CRY_PLATFORM_64BIT
	static_assert((offsetof(CTexture, m_composition) - offsetof(CTexture, m_pFileTexMips)) <= 64, "Invalid offset!");
	//static_assert((offsetof(CTexture, m_pFileTexMips) % 64) == 0, "Invalid offset!");
#endif

#ifndef _RELEASE
	if (!gRenDev->m_pRT->IsRenderThread() || gRenDev->m_pRT->IsRenderLoadingThread())
		__debugbreak();
#endif

#ifndef _RELEASE
	if (IsStreaming())
		__debugbreak();
#endif

	if (gRenDev && gRenDev->m_pRT)
		gRenDev->m_pRT->RC_ReleaseDeviceTexture(this);

	if (m_pFileTexMips)
	{
		Unlink();
		StreamState_ReleaseInfo(this, m_pFileTexMips);
		m_pFileTexMips = NULL;
	}

#ifdef ENABLE_TEXTURE_STREAM_LISTENER
	if (s_pStreamListener)
		s_pStreamListener->OnDestroyedStreamedTexture(this);
#endif

#ifndef _RELEASE
	if (m_bInDistanceSortedList)
		__debugbreak();
#endif

#ifdef TEXTURE_GET_SYSTEM_COPY_SUPPORT
	s_LowResSystemCopy.erase(this);
#endif
}

void CTexture::RT_ReleaseDevice()
{
	ReleaseDeviceTexture(false);
}

const CCryNameTSCRC& CTexture::mfGetClassName()
{
	return s_sClassName;
}

CCryNameTSCRC CTexture::GenName(const char* name, uint32 nFlags)
{
	stack_string strName = name;
	strName.MakeLower();

	//'\\' in texture names causing duplication
	PathUtil::ToUnixPath(strName);

	if (nFlags & FT_ALPHA)
		strName += "_a";

	return CCryNameTSCRC(strName.c_str());
}

class StrComp
{
public:
	bool operator()(const char* s1, const char* s2) const { return strcmp(s1, s2) < 0; }
};

CTexture* CTexture::GetByID(int nID)
{
	CTexture* pTex = s_ptexNoTexture;

	const CCryNameTSCRC& className = mfGetClassName();
	CBaseResource* pBR = CBaseResource::GetResource(className, nID, false);
	if (pBR)
		pTex = (CTexture*)pBR;
	return pTex;
}

CTexture* CTexture::GetByName(const char* szName, uint32 flags)
{
	CTexture* pTex = nullptr;

	CCryNameTSCRC Name = GenName(szName, flags);
	CBaseResource* pBR = CBaseResource::GetResource(mfGetClassName(), Name, false);
	if (pBR)
		pTex = (CTexture*)pBR;
	return pTex;
}

CTexture* CTexture::GetByNameCRC(CCryNameTSCRC Name)
{
	CTexture* pTex = nullptr;

	CBaseResource* pBR = CBaseResource::GetResource(mfGetClassName(), Name, false);
	if (pBR)
		pTex = (CTexture*)pBR;
	return pTex;
}

CTexture* CTexture::FindOrRegisterTextureObject(const char* name, uint32 nFlags, ETEX_Format eTFDst, bool& bFound)
{
	CTexture* pTex = nullptr;

	CCryNameTSCRC Name = GenName(name, nFlags);
	CBaseResource* pBR = CBaseResource::GetResource(mfGetClassName(), Name, false);
	if (!pBR)
	{
		pTex = new CTexture(nFlags);
		pTex->Register(mfGetClassName(), Name);
		pTex->m_eFlags = nFlags;
		pTex->m_eDstFormat = eTFDst;
		pTex->m_SrcName = name;
		bFound = false;
	}
	else
	{
		pTex = (CTexture*)pBR;
		pTex->AddRef();
		bFound = true;
	}

	return pTex;
}

void CTexture::SetDevTexture(CDeviceTexture* pDeviceTex)
{
	if (m_pDevTexture) 
		m_pDevTexture->SetOwner(NULL);
	SAFE_RELEASE(m_pDevTexture);

	m_pDevTexture = pDeviceTex;
	if (m_pDevTexture)
	{
		m_pDevTexture->SetNoDelete(!!(m_eFlags & FT_DONT_RELEASE));
		m_pDevTexture->SetOwner(this);
	}

	InvalidateDeviceResource(this, eDeviceResourceDirty);
}

void CTexture::OwnDevTexture(CDeviceTexture* pDeviceTex)
{
	SAFE_RELEASE(m_pDevTexture);

	m_pDevTexture = pDeviceTex;
	if (m_pDevTexture)
	{
		const STextureLayout Layput = m_pDevTexture->GetLayout();

		m_nWidth       = Layput.m_nWidth;
		m_nHeight      = Layput.m_nHeight;
		m_nDepth       = Layput.m_nDepth;
		m_nArraySize   = Layput.m_nArraySize;
		m_nMips        = Layput.m_nMips;
		m_eSrcFormat   = Layput.m_eSrcFormat;
		m_eDstFormat   = Layput.m_eDstFormat;
		m_pPixelFormat = Layput.m_pPixelFormat;
		m_eTT          = Layput.m_eTT;
		m_eFlags       = Layput.m_eFlags; /* TODO: change FT_... to CDeviceObjectFactory::... */
		m_bIsSRGB      = Layput.m_bIsSRGB;

		m_nDevTextureSize = m_nPersistentSize = m_pDevTexture->GetDeviceSize();
		CryInterlockedAdd(&CTexture::s_nStatsCurManagedNonStreamedTexMem, m_nDevTextureSize);
	}

	InvalidateDeviceResource(this, eDeviceResourceDirty);
}

void CTexture::PostCreate()
{
	m_nUpdateFrameID = gRenDev->GetFrameID(false);
	m_bPostponed = false;
}

void CTexture::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddObject(m_SrcName);

#ifdef TEXTURE_GET_SYSTEM_COPY_SUPPORT
	const LowResSystemCopyType::iterator& it = s_LowResSystemCopy.find(this);
	if (it != CTexture::s_LowResSystemCopy.end())
		pSizer->AddObject((*it).second.m_lowResSystemCopy);
#endif

	if (m_pFileTexMips)
		m_pFileTexMips->GetMemoryUsage(pSizer, m_nMips, m_CacheFileHeader.m_nSides);
}

//=======================================================
// Low-level functions calling CreateDeviceTexture()

bool CTexture::CreateRenderTarget(ETEX_Format eFormat, const ColorF& cClear)
{
	if (m_eSrcFormat == eTF_Unknown)
		m_eSrcFormat = eFormat;
	if (m_eSrcFormat == eTF_Unknown)
		return false;
	const void** pData = nullptr;

	SetClosestFormatSupported();
	m_eFlags |= FT_USAGE_RENDERTARGET;
	m_cClearColor = cClear;
	m_nMips = m_eFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(m_nWidth, m_nHeight) : m_nMips;
	bool bRes = CreateDeviceTexture(pData);
	PostCreate();

	return bRes;
}

bool CTexture::CreateDepthStencil(ETEX_Format eFormat, const ColorF& cClear)
{
	if (m_eSrcFormat == eTF_Unknown)
		m_eSrcFormat = eFormat;
	if (m_eSrcFormat == eTF_Unknown)
		return false;
	const void** pData = nullptr;

	SetClosestFormatSupported();
	m_eFlags |= FT_USAGE_DEPTHSTENCIL;
	m_cClearColor = cClear;
	m_nMips = m_eFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(m_nWidth, m_nHeight) : m_nMips;
	bool bRes = CreateDeviceTexture(pData);
	PostCreate();

	return bRes;
}

bool CTexture::CreateShaderResource(STexData& td)
{
	m_nWidth = td.m_nWidth;
	m_nHeight = td.m_nHeight;
	m_nDepth = td.m_nDepth;
	m_eSrcFormat = td.m_eFormat;
	m_nMips = td.m_nMips;
	m_fAvgBrightness = td.m_fAvgBrightness;
	m_cMinColor = td.m_cMinColor;
	m_cMaxColor = td.m_cMaxColor;
	m_cClearColor = ColorF(0.0f, 0.0f, 0.0f, 1.0f);
	m_bUseDecalBorderCol = (td.m_nFlags & FIM_DECAL) != 0;
	m_bIsSRGB = (td.m_nFlags & FIM_SRGB_READ) != 0;

	assert((m_nDepth == 1) || (m_eTT == eTT_3D));
	assert((m_nArraySize == 1) || (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray || m_eTT == eTT_2DArray));
	assert((m_nArraySize % 6) || (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray));
	assert(!td.m_pData[1] || !(m_eFlags & FT_REPLICATE_TO_ALL_SIDES) || (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray));
	assert(m_nWidth && m_nHeight && m_nMips);

	SetClosestFormatSupported();
	if (!ImagePreprocessing(td))
		return false;

	assert(m_nWidth && m_nHeight && m_nMips);

	const int nMaxTextureSize = gRenDev->GetMaxTextureSize();
	if (nMaxTextureSize > 0)
	{
		if (m_nWidth > nMaxTextureSize || m_nHeight > nMaxTextureSize)
			return false;
	}

	// Semi-consecutive data: {{slice,0},{slice,0},{0,0}}
	const void* pData[6 * 2 + 2];
	pData[6 * 2 + 0] = nullptr;
	pData[6 * 2 + 1] = nullptr;
	for (uint32 i = 0; i < 6; i++)
		pData[i * 2 + 0] = td.m_pData[i],
		pData[i * 2 + 1] = nullptr;

	bool bRes = CreateDeviceTexture(pData);

	return bRes;
}

//=======================================================
// Mid-level functions calling Create...()

bool CTexture::Create2DTexture(int nWidth, int nHeight, int nMips, int nFlags, byte* pSrcData, ETEX_Format eSrcFormat)
{
	if (nMips <= 0)
		nMips = CTexture::CalcNumMips(nWidth, nHeight);
	m_eSrcTileMode = pSrcData ? eTM_None : eTM_Unspecified;
	m_eSrcFormat = eSrcFormat;
	m_nMips = nMips;

	STexData td;
	td.m_eFormat = eSrcFormat;
	td.m_nWidth = nWidth;
	td.m_nHeight = nHeight;
	td.m_nDepth = 1;
	td.m_nMips = nMips;
	td.m_pData[0] = pSrcData;

	bool bRes = CreateShaderResource(td);
	if (!bRes)
		m_eFlags |= FT_FAILED;

	PostCreate();

	return bRes;
}

bool CTexture::Create3DTexture(int nWidth, int nHeight, int nDepth, int nMips, int nFlags, byte* pSrcData, ETEX_Format eSrcFormat)
{
	//if (nMips <= 0)
	//  nMips = CTexture::CalcNumMips(nWidth, nHeight);
	m_eSrcTileMode = pSrcData ? eTM_None : eTM_Unspecified;
	m_eSrcFormat = eSrcFormat;
	m_nMips = nMips;

	STexData td;
	td.m_eFormat = eSrcFormat;
	td.m_nWidth = nWidth;
	td.m_nHeight = nHeight;
	td.m_nDepth = nDepth;
	td.m_nMips = nMips;
	td.m_pData[0] = pSrcData;

	bool bRes = CreateShaderResource(td);
	if (!bRes)
		m_eFlags |= FT_FAILED;

	PostCreate();

	return bRes;
}

//=======================================================
// High-level functions calling Create...()

CTexture* CTexture::GetOrCreateTextureObject(const char* name, uint32 nWidth, uint32 nHeight, int nDepth, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID)
{
	SYNCHRONOUS_LOADING_TICK();

	bool bFound = false;

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "%s", name);

	CTexture* pTex = FindOrRegisterTextureObject(name, nFlags, eFormat, bFound);
	if (bFound)
	{
		if (pTex->m_nWidth == 0)
			pTex->SetWidth(nWidth);
		if (pTex->m_nHeight == 0)
			pTex->SetHeight(nHeight);

		pTex->m_nMips = nFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(pTex->m_nWidth, pTex->m_nHeight) : pTex->m_nMips;
		pTex->m_eFlags |= nFlags & (FT_DONT_RELEASE | FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL);

		return pTex;
	}

	pTex->m_nDepth = nDepth;
	pTex->SetWidth(nWidth);
	pTex->SetHeight(nHeight);
	pTex->m_nMips = nFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(pTex->m_nWidth, pTex->m_nHeight) : pTex->m_nMips;
	pTex->m_eTT = eTT;
	pTex->m_eSrcFormat = eFormat;
	pTex->m_nCustomID = nCustomID;
	pTex->m_SrcName = name;
	pTex->SetClosestFormatSupported();

	return pTex;
}

_smart_ptr<CTexture> CTexture::GetOrCreateTextureObjectPtr(const char* name, uint32 nWidth, uint32 nHeight, int nDepth, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID)
{
	CTexture* pTex = GetOrCreateTextureObject(name, nWidth, nHeight, nDepth, eTT, nFlags, eFormat, nCustomID);
	_smart_ptr<CTexture> result;
	result.Assign_NoAddRef(pTex);

	return result;
}

CTexture* CTexture::GetOrCreateTextureArray(const char* name, uint32 nWidth, uint32 nHeight, uint32 nArraySize, int nMips, ETEX_Type eType, uint32 nFlags, ETEX_Format eFormat, int nCustomID)
{
	assert(eType == eTT_2DArray || eType == eTT_CubeArray);

	if (nArraySize > 2048 /*D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION*/)
	{
		assert(0);
		return NULL;
	}

	if (nMips <= 0)
		nMips = CTexture::CalcNumMips(nWidth, nHeight);

	bool sRGB = (nFlags & FT_USAGE_ALLOWREADSRGB) != 0;
	nFlags &= ~FT_USAGE_ALLOWREADSRGB;

	CTexture* pTex = GetOrCreateTextureObject(name, nWidth, nHeight, 1, eType, nFlags, eFormat, nCustomID);
	pTex->SetWidth(nWidth);
	pTex->SetHeight(nHeight);
	pTex->m_nMips = nFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(pTex->m_nWidth, pTex->m_nHeight) : pTex->m_nMips;
	pTex->m_nArraySize = nArraySize; assert((eType != eTT_CubeArray) || !(nArraySize % 6));
	pTex->m_nDepth = 1;

	bool bRes;
	if (nFlags & FT_USAGE_RENDERTARGET)
	{
		bRes = pTex->CreateRenderTarget(eFormat, Clr_Unknown);
	}
	else if (nFlags & FT_USAGE_DEPTHSTENCIL)
	{
		bRes = pTex->CreateDepthStencil(eFormat, Clr_Unknown);
	}
	else
	{
		STexData td;

		td.m_eFormat = eFormat;
		td.m_nDepth = 1;
		td.m_nWidth = nWidth;
		td.m_nHeight = nHeight;
		td.m_nMips = nMips;
		td.m_nFlags = sRGB ? FIM_SRGB_READ : 0;

		bRes = pTex->CreateShaderResource(td);
	}

	if (!bRes)
		pTex->m_eFlags |= FT_FAILED;
	pTex->PostCreate();

	return pTex;
}

CTexture* CTexture::GetOrCreateRenderTarget(const char* name, uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID)
{
	CTexture* pTex = GetOrCreateTextureObject(name, nWidth, nHeight, 1, eTT, nFlags | FT_USAGE_RENDERTARGET, eFormat, nCustomID);
	pTex->SetWidth(nWidth);
	pTex->SetHeight(nHeight);
	pTex->m_nMips = nFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(pTex->m_nWidth, pTex->m_nHeight) : pTex->m_nMips;
	pTex->m_eFlags |= nFlags;

	bool bRes = pTex->CreateRenderTarget(eFormat, cClear);
	if (!bRes)
		pTex->m_eFlags |= FT_FAILED;
	pTex->PostCreate();

	return pTex;
}

CTexture* CTexture::GetOrCreateDepthStencil(const char* name, uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID)
{
	CTexture* pTex = GetOrCreateTextureObject(name, nWidth, nHeight, 1, eTT, nFlags | FT_USAGE_DEPTHSTENCIL, eFormat, nCustomID);
	pTex->SetWidth(nWidth);
	pTex->SetHeight(nHeight);
	pTex->m_nMips = nFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(pTex->m_nWidth, pTex->m_nHeight) : pTex->m_nMips;
	pTex->m_eFlags |= nFlags;

	bool bRes = pTex->CreateDepthStencil(eFormat, cClear);
	if (!bRes)
		pTex->m_eFlags |= FT_FAILED;
	pTex->PostCreate();

	return pTex;
}

CTexture* CTexture::GetOrCreate2DTexture(const char* szName, int nWidth, int nHeight, int nMips, int nFlags, byte* pSrcData, ETEX_Format eSrcFormat, bool bAsyncDevTexCreation)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

	CTexture* pTex = GetOrCreateTextureObject(szName, nWidth, nHeight, 1, eTT_2D, nFlags, eSrcFormat, -1);
	pTex->m_bAsyncDevTexCreation = bAsyncDevTexCreation;
	bool bFound = false;

	pTex->Create2DTexture(nWidth, nHeight, nMips, nFlags, pSrcData, eSrcFormat);

	return pTex;
}

CTexture* CTexture::GetOrCreate3DTexture(const char* szName, int nWidth, int nHeight, int nDepth, int nMips, int nFlags, byte* pSrcData, ETEX_Format eSrcFormat)
{
	CTexture* pTex = GetOrCreateTextureObject(szName, nWidth, nHeight, nDepth, eTT_3D, nFlags, eSrcFormat, -1);
	bool bFound = false;

	pTex->Create3DTexture(nWidth, nHeight, nDepth, nMips, nFlags, pSrcData, eSrcFormat);

	return pTex;
}

CTexture* CTexture::GetOrCreate2DCompositeTexture(const char* szName, int nWidth, int nHeight, int nMips, int nFlags, ETEX_Format eFormat, const STexComposition* pCompositions, size_t nCompositions)
{
	nFlags |= FT_COMPOSITE;
	nFlags &= ~FT_DONT_STREAM;

	if (nMips <= 0)
		nMips = CTexture::CalcNumMips(nWidth, nHeight);

	bool bFound = false;
	CTexture* pTex = FindOrRegisterTextureObject(szName, nFlags, eFormat, bFound);
	if (!bFound)
	{
		pTex->SetWidth(nWidth);
		pTex->SetHeight(nHeight);
		pTex->m_nMips = nMips;
		pTex->m_composition.assign(pCompositions, pCompositions + nCompositions);

		// Strip all invalid textures from the composition

		int w = 0;
		for (int r = 0, c = pTex->m_composition.size(); r != c; ++r)
		{
			if (!pTex->m_composition[r].pTexture)
			{
				CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "Composition %i for '%s' is missing", r, szName);
				continue;
			}

			if (r != w)
				pTex->m_composition[w] = pTex->m_composition[r];
			++w;
		}
		pTex->m_composition.resize(w);

		if (CTexture::s_bPrecachePhase)
		{
			pTex->m_bPostponed = true;
			pTex->m_bWasUnloaded = true;
		}
		else
		{
			pTex->StreamPrepareComposition();
		}
	}

	return pTex;
}

//=======================================================

bool CTexture::Reload()
{
	bool bOK = false;

	// If the texture is flagged to not be released, we skip the reloading
	if (m_eFlags & FT_DONT_RELEASE)
		return bOK;

	if (IsStreamed())
	{
		ReleaseDeviceTexture(false);
		return ToggleStreaming(true);
	}

	if (m_eFlags & FT_FROMIMAGE)
	{
		assert(!(m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_DYNAMIC)));
		bOK = LoadFromImage(m_SrcName.c_str());   // true=reloading
		if (!bOK)
			SetNoTexture(m_eTT == eTT_Cube ? s_ptexNoTextureCM : s_ptexNoTexture);
	}
	else if (m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_DYNAMIC))
	{
		const void** pData = nullptr;
		bOK = CreateDeviceTexture(pData);
		assert(bOK);
	}

	PostCreate();

	return bOK;
}

CTexture* CTexture::ForName(const char* name, uint32 nFlags, ETEX_Format eFormat)
{
	SLICE_AND_SLEEP();

	bool bFound = false;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Textures");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "%s", name);

	CRY_DEFINE_ASSET_SCOPE("Texture", name);

	CTexture* pTex = FindOrRegisterTextureObject(name, nFlags, eFormat, bFound);
	if (bFound || name[0] == '$')
	{
		if (!bFound)
		{
			pTex->m_SrcName = name;
		}
		else
		{
			// switch off streaming for the same texture with the same flags except DONT_STREAM
			if ((nFlags & FT_DONT_STREAM) != 0 && (pTex->GetFlags() & FT_DONT_STREAM) == 0)
			{
				if (!pTex->m_bPostponed)
					pTex->ReleaseDeviceTexture(false);
				pTex->m_eFlags |= FT_DONT_STREAM;
				if (!pTex->m_bPostponed)
					pTex->Reload();
			}
		}

		return pTex;
	}
	pTex->m_SrcName = name;

#ifndef _RELEASE
	pTex->m_sAssetScopeName = gEnv->pLog->GetAssetScopeString();
#endif

	if (CTexture::s_bPrecachePhase || (pTex->m_eFlags & FT_ASYNC_PREPARE))
	{
		// NOTE: attached alpha isn't detectable by flags before the header is loaded, so we do it by file-suffix
		if (/*(nFlags & FT_TEX_NORMAL_MAP) &&*/ TextureHelpers::VerifyTexSuffix(EFTT_NORMALS, name) && TextureHelpers::VerifyTexSuffix(EFTT_SMOOTHNESS, name))
			nFlags |= FT_HAS_ATTACHED_ALPHA;

		pTex->m_eDstFormat = eFormat;
		pTex->m_eFlags = nFlags;
		pTex->m_bPostponed = true;
		pTex->m_bWasUnloaded = true;
	}

	if (!CTexture::s_bPrecachePhase)
		pTex->Load(eFormat);

	return pTex;
}

_smart_ptr<CTexture> CTexture::ForNamePtr(const char* name, uint32 nFlags, ETEX_Format eFormat)
{
	CTexture* pTex = ForName(name, nFlags, eFormat);
	_smart_ptr<CTexture> result;
	result.Assign_NoAddRef(pTex);

	return result;
}


struct CompareTextures
{
	bool operator()(const CTexture* a, const CTexture* b)
	{
		return (stricmp(a->GetSourceName(), b->GetSourceName()) < 0);
	}
};

void CTexture::Precache()
{
	LOADING_TIME_PROFILE_SECTION(iSystem);

	if (!s_bPrecachePhase)
		return;
	if (!gRenDev)
		return;

	gEnv->pLog->UpdateLoadingScreen("Requesting textures precache ...");

	gRenDev->m_pRT->RC_PreloadTextures();

	gEnv->pLog->UpdateLoadingScreen("Textures precache done.");
}

void CTexture::RT_Precache()
{
	if (gRenDev->CheckDeviceLost())
		return;

	LOADING_TIME_PROFILE_SECTION(iSystem);

	// Disable invalid file access logging if texture streaming is disabled
	// If texture streaming is turned off, we will hit this on the renderthread
	// and stall due to the invalid file access stalls
	ICVar* sysPakLogInvalidAccess = NULL;
	int pakLogFileAccess = 0;
	if (!CRenderer::CV_r_texturesstreaming)
	{
		if (sysPakLogInvalidAccess = gEnv->pConsole->GetCVar("sys_PakLogInvalidFileAccess"))
		{
			pakLogFileAccess = sysPakLogInvalidAccess->GetIVal();
		}
	}

	CTimeValue t0 = gEnv->pTimer->GetAsyncTime();
	CryLog("-- Precaching textures...");
	iLog->UpdateLoadingScreen(0);

	std::vector<CTexture*> TexturesForPrecaching;
	std::vector<CTexture*> TexturesForComposition;

	bool bTextureCacheExists = false;

	{
		AUTO_LOCK(CBaseResource::s_cResLock);

		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (pRL)
		{
			TexturesForPrecaching.reserve(pRL->m_RMap.size());

			ResourcesMapItor itor;
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* tp = (CTexture*)itor->second;
				if (!tp)
					continue;
				if (tp->CTexture::IsPostponed())
				{
					if (tp->CTexture::GetFlags() & FT_COMPOSITE)
						TexturesForComposition.push_back(tp);
					else
						TexturesForPrecaching.push_back(tp);
				}
			}
		}
	}

	// Preload all the post poned textures
	{
		if (!gEnv->IsEditor())
			CryLog("=============================== Loading textures ================================");

		std::vector<CTexture*>& Textures = TexturesForPrecaching;
		std::sort(Textures.begin(), Textures.end(), CompareTextures());

		gEnv->pSystem->GetStreamEngine()->PauseStreaming(false, 1 << eStreamTaskTypeTexture);

		int numTextures = Textures.size();
		int prevProgress = 0;

		for (uint32 i = 0; i < Textures.size(); i++)
		{
			CTexture* tp = Textures[i];

			if (!CRenderer::CV_r_texturesstreaming || !tp->m_bStreamPrepared)
			{
				tp->m_bPostponed = false;
				tp->Load(tp->m_eDstFormat);
			}
			int progress = (i * 10) / numTextures;
			if (progress != prevProgress)
			{
				prevProgress = progress;
				gEnv->pLog->UpdateLoadingScreen("Precaching progress: %d", progress);
			}
		}

		while (s_StreamPrepTasks.GetNumLive())
		{
			if (gRenDev->m_pRT->IsRenderThread() && !gRenDev->m_pRT->IsRenderLoadingThread())
			{
				StreamState_Update();
				StreamState_UpdatePrep();
			}
			else if (gRenDev->m_pRT->IsRenderLoadingThread())
			{
				StreamState_UpdatePrep();
			}

			CrySleep(1);
		}

		for (uint32 i = 0; i < Textures.size(); i++)
		{
			CTexture* tp = Textures[i];

			if (tp->m_bStreamed && tp->m_bForceStreamHighRes)
			{
				tp->m_bStreamHighPriority |= 1;
				tp->m_fpMinMipCur = 0;
				s_pTextureStreamer->Precache(tp);
			}
		}

		if (!gEnv->IsEditor())
			CryLog("========================== Finished loading textures ============================");
	}

	{
		std::vector<CTexture*>& Textures = TexturesForComposition;

		for (uint32 i = 0; i < Textures.size(); i++)
		{
			CTexture* tp = Textures[i];

			if (!CRenderer::CV_r_texturesstreaming || !tp->m_bStreamPrepared)
			{
				tp->m_bPostponed = false;
				tp->StreamPrepareComposition();
			}
		}

		for (uint32 i = 0; i < Textures.size(); i++)
		{
			CTexture* tp = Textures[i];

			if (tp->m_bStreamed && tp->m_bForceStreamHighRes)
			{
				tp->m_bStreamHighPriority |= 1;
				tp->m_fpMinMipCur = 0;
				s_pTextureStreamer->Precache(tp);
			}
		}
	}

	if (bTextureCacheExists)
	{
		//GetISystem()->GetIResourceManager()->UnloadLevelCachePak( TEXTURE_LEVEL_CACHE_PAK );
	}

	CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
	float dt = (t1 - t0).GetSeconds();
	CryLog("Precaching textures done in %.2f seconds", dt);

	s_bPrecachePhase = false;

	// Restore pakLogFileAccess if it was disabled during precaching
	// because texture precaching was disabled
	if (pakLogFileAccess)
	{
		sysPakLogInvalidAccess->Set(pakLogFileAccess);
	}
}

bool CTexture::Load(ETEX_Format eFormat)
{
	LOADING_TIME_PROFILE_SECTION_NAMED_ARGS("CTexture::Load(ETEX_Format eTFDst)", m_SrcName);
	m_bWasUnloaded = false;
	m_bStreamed = false;

	bool bFound = LoadFromImage(m_SrcName.c_str(), eFormat);   // false=not reloading

	if (!bFound)
		SetNoTexture(m_eTT == eTT_Cube ? s_ptexNoTextureCM : s_ptexNoTexture);

	m_eFlags |= FT_FROMIMAGE;
	PostCreate();

	return bFound;
}

bool CTexture::ToggleStreaming(const bool bEnable)
{
	if (!(m_eFlags & (FT_FROMIMAGE | FT_DONT_RELEASE)) || (m_eFlags & FT_DONT_STREAM))
		return false;
	AbortStreamingTasks(this);
	if (bEnable)
	{
		if (IsStreamed())
			return true;
		ReleaseDeviceTexture(false);
		m_bStreamed = true;
		if (StreamPrepare(true))
			return true;
		if (m_pFileTexMips)
		{
			Unlink();
			StreamState_ReleaseInfo(this, m_pFileTexMips);
			m_pFileTexMips = NULL;
		}
		m_bStreamed = false;
		if (m_bNoTexture)
			return true;
	}
	ReleaseDeviceTexture(false);
	return Reload();
}

bool CTexture::LoadFromImage(const char* name, ETEX_Format eFormat)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(name);

	if (CRenderer::CV_r_texnoload)
	{
		if (SetNoTexture())
			return true;
	}

	string sFileName(name);
	sFileName.MakeLower();

	m_eDstFormat = eFormat;

	// try to stream-in the texture
	if (CRenderer::CV_r_texturesstreaming && !(m_eFlags & FT_DONT_STREAM) && (m_eTT == eTT_2D || m_eTT == eTT_Cube))
	{
		m_bStreamed = true;
		if (StreamPrepare(true))
		{
			assert(m_pDevTexture);
			return true;
		}
		m_eFlags |= FT_DONT_STREAM;
		m_bStreamed = false;
		m_bForceStreamHighRes = false;
		if (m_bNoTexture)
		{
			if (m_pFileTexMips)
			{
				Unlink();
				StreamState_ReleaseInfo(this, m_pFileTexMips);
				m_pFileTexMips = NULL;
				m_bStreamed = false;
			}
			return true;
		}
	}

#ifndef _RELEASE
	CRY_DEFINE_ASSET_SCOPE("Texture", m_sAssetScopeName);
#endif

	if (m_bPostponed)
	{
		if (s_pTextureStreamer->BeginPrepare(this, sFileName, (m_eFlags & FT_ALPHA) ? FIM_ALPHA : 0))
			return true;
	}

	uint32 nImageFlags =
	  ((m_eFlags & FT_ALPHA) ? FIM_ALPHA : 0) |
	  ((m_eFlags & FT_STREAMED_PREPARE) ? FIM_READ_VIA_STREAMS : 0);

	_smart_ptr<CImageFile> pImage = CImageFile::mfLoad_file(sFileName, nImageFlags);
	return Load(pImage);
}

bool CTexture::Load(CImageFile* pImage)
{
	if (!pImage || pImage->mfGetFormat() == eTF_Unknown)
		return false;

	LOADING_TIME_PROFILE_SECTION_NAMED_ARGS("CTexture::Load(CImageFile* pImage)", pImage->mfGet_filename().c_str());

	if ((m_eFlags & FT_ALPHA) && !pImage->mfIs_image(0))
	{
		SetNoTexture(s_ptexWhite);
		return true;
	}
	const char* name = pImage->mfGet_filename().c_str();
	if (pImage->mfGet_Flags() & FIM_SPLITTED)              // propagate splitted file flag
		m_eFlags |= FT_SPLITTED;
	if (pImage->mfGet_Flags() & FIM_X360_NOT_PRETILED)
		m_eFlags |= FT_TEX_WAS_NOT_PRE_TILED;
	if (pImage->mfGet_Flags() & FIM_FILESINGLE)   // propagate flag from image to texture
		m_eFlags |= FT_FILESINGLE;
	if (pImage->mfGet_Flags() & FIM_NORMALMAP)
	{
		if (!(m_eFlags & FT_TEX_NORMAL_MAP) && !CryStringUtils::stristr(name, "_ddn"))
		{
			// becomes reported as editor error
			gEnv->pSystem->Warning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE | VALIDATOR_FLAG_TEXTURE,
			                       name, "Not a normal map texture attempted to be used as a normal map: %s", name);
		}
	}

	if (!(m_eFlags & FT_ALPHA) && !(
	      pImage->mfGetFormat() == eTF_BC5U     || pImage->mfGetFormat() == eTF_BC5S || pImage->mfGetFormat() == eTF_BC7 ||
	      pImage->mfGetFormat() == eTF_EAC_RG11 || pImage->mfGetFormat() == eTF_EAC_RG11S
	      ) && CryStringUtils::stristr(name, "_ddn") != 0 && GetDevTexture()) // improvable code
	{
		// becomes reported as editor error
		gEnv->pSystem->Warning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE | VALIDATOR_FLAG_TEXTURE,
		                       name, "Wrong format '%s' for normal map texture '%s'", CTexture::GetFormatName(), name);
	}

	if (pImage->mfGet_Flags() & FIM_NOTSUPPORTS_MIPS && !(m_eFlags & FT_NOMIPS))
		m_eFlags |= FT_FORCE_MIPS;
	if (pImage->mfGet_Flags() & FIM_HAS_ATTACHED_ALPHA)
		m_eFlags |= FT_HAS_ATTACHED_ALPHA;      // if the image has alpha attached we store this in the CTexture

	m_eSrcTileMode = pImage->mfGetTileMode();
	m_nArraySize = pImage->mfGet_NumSides();
	m_eTT =
		 (pImage->mfGet_depth   () >  1) ? eTT_3D :
		 (pImage->mfGet_NumSides() == 6) ? eTT_Cube :
		!(pImage->mfGet_NumSides() %  6) ? eTT_CubeArray :
		 (pImage->mfGet_NumSides() == 1) ? eTT_2D :
		                                   eTT_2DArray;

	STexData td;
	td.m_nFlags = pImage->mfGet_Flags();
	td.m_pData[0] = pImage->mfGet_image(0);
	td.m_nWidth = pImage->mfGet_width();
	td.m_nHeight = pImage->mfGet_height();
	td.m_nDepth = pImage->mfGet_depth();
	td.m_eFormat = pImage->mfGetFormat();
	td.m_nMips = pImage->mfGet_numMips();
	td.m_fAvgBrightness = pImage->mfGet_avgBrightness();
	td.m_cMinColor = pImage->mfGet_minColor();
	td.m_cMaxColor = pImage->mfGet_maxColor();
	if ((m_eFlags & FT_NOMIPS) || td.m_nMips <= 0)
		td.m_nMips = 1;
	td.m_pFilePath = pImage->mfGet_filename();

	// base range after normalization, fe. [0,1] for 8bit images, or [0,2^15] for RGBE/HDR data
	if (CImageExtensionHelper::IsDynamicRange(td.m_eFormat))
	{
		td.m_cMinColor /= td.m_cMaxColor.a;
		td.m_cMaxColor /= td.m_cMaxColor.a;
	}

	// check if it's a cubemap
	if (pImage->mfIs_image(1))
	{
		for (int i = 1; i < 6; i++)
		{
			td.m_pData[i] = pImage->mfGet_image(i);
		}
	}

	bool bRes = false;
	if (pImage)
	{
		FormatFixup(td);
		bRes = CreateShaderResource(td);
	}

	for (int i = 0; i < 6; i++)
		if (td.m_pData[i] && td.WasReallocated(i))
			SAFE_DELETE_ARRAY(td.m_pData[i]);

	return bRes;
}

void CTexture::UpdateData(STexData &td, int flags)
{
	m_eFlags = flags;
	m_eDstFormat = td.m_eFormat;
	CreateShaderResource(td);
}

ETEX_Format CTexture::FormatFixup(ETEX_Format eFormat)
{
	switch (eFormat)
	{
	case eTF_L8V8U8X8:
	case eTF_L8V8U8:
		return eTF_R8G8B8A8S;
	case eTF_B8G8R8:
	case eTF_A8L8:
	case eTF_L8:
		return eTF_R8G8B8A8;

	// only available as hardware format under DX11.1 with DXGI 1.2
	case eTF_B5G5R5A1:
	case eTF_B5G6R5:
	case eTF_B4G4R4A4:

	//! Only available as hardware format under Vulkan or XBO.
	case eTF_R4G4:
	case eTF_R4G4B4A4:
	{
		const SPixFormat* pPF;
		return GetClosestFormatSupported(eFormat, pPF);
	}

	default:
		return eFormat;
	}
}

bool CTexture::FormatFixup(STexData& td)
{
	const ETEX_Format eSrcFormat = td.m_eFormat;
	const ETEX_Format eDstFormat = FormatFixup(td.m_eFormat);
	CRY_ASSERT(eDstFormat != eTF_Unknown);

	if (m_eSrcTileMode == eTM_None)
	{
		// Try and expand
		int nSourceSize = CTexture::TextureDataSize(td.m_nWidth, td.m_nHeight, td.m_nDepth, td.m_nMips, 1, eSrcFormat);
		int nTargetSize = CTexture::TextureDataSize(td.m_nWidth, td.m_nHeight, td.m_nDepth, td.m_nMips, 1, eDstFormat);

		for (int nImage = 0; nImage < sizeof(td.m_pData) / sizeof(td.m_pData[0]); ++nImage)
		{
			if (td.m_pData[nImage])
			{
				byte* pNewImage = new byte[nTargetSize];
				CTexture::ExpandMipFromFile(pNewImage, nTargetSize, td.m_pData[nImage], nSourceSize, eSrcFormat, eDstFormat);
				td.AssignData(nImage, pNewImage);
			}
		}

		td.m_eFormat = eDstFormat;
	}
	else
	{
#ifndef _RELEASE
		if (eDstFormat != eSrcFormat)
			__debugbreak();
#endif
	}

	return true;
}

bool CTexture::ImagePreprocessing(STexData& td)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

	const char* pTexFileName = td.m_pFilePath ? td.m_pFilePath : "$Unknown";

	if (m_eDstFormat == eTF_Unknown)
	{
		td.m_pData[0] = td.m_pData[1] = td.m_pData[2] = td.m_pData[3] = td.m_pData[4] = td.m_pData[5] = 0;
		m_nWidth = m_nHeight = m_nDepth = m_nMips = 0;

#if !defined(_RELEASE)
		TextureError(pTexFileName, "Trying to create a texture with unsupported target format %s!", NameForTextureFormat(m_eSrcFormat));
#endif
		return false;
	}

	const ETEX_Format eSrcFormat = td.m_eFormat;
	const bool fmtConversionNeeded = eSrcFormat != m_eDstFormat;

#if !CRY_PLATFORM_WINDOWS || CRY_RENDERER_OPENGL
	if (fmtConversionNeeded)
	{
		td.m_pData[0] = td.m_pData[1] = td.m_pData[2] = td.m_pData[3] = td.m_pData[4] = td.m_pData[5] = 0;
		m_nWidth = m_nHeight = m_nDepth = m_nMips = 0;

	#if !defined(_RELEASE)
		TextureError(pTexFileName, "Trying an image format conversion from %s to %s. This is not supported on this platform!", NameForTextureFormat(eSrcFormat), NameForTextureFormat(m_eDstFormat));
	#endif
		return false;
	}
#else

	const bool doProcessing = fmtConversionNeeded && (m_eFlags & FT_TEX_FONT) == 0; // we generate the font in native format
	if (doProcessing)
	{
		const int nSrcWidth = td.m_nWidth;
		const int nSrcHeight = td.m_nHeight;

		for (int i = 0; i < 6; i++)
		{
			byte* pSrcData = td.m_pData[i];
			if (pSrcData)
			{
				int nOutSize = 0;
				byte* pNewData = Convert(pSrcData, nSrcWidth, nSrcHeight, td.m_nMips, eSrcFormat, m_eDstFormat, td.m_nMips, nOutSize, true);
				if (pNewData)
					td.AssignData(i, pNewData);
			}
		}
	}
#endif

#if defined(TEXTURE_GET_SYSTEM_COPY_SUPPORT)
	if (m_eFlags & FT_KEEP_LOWRES_SYSCOPY)
		PrepareLowResSystemCopy(td.m_pData[0], true);
#endif

	return true;
}

int CTexture::CalcNumMips(int nWidth, int nHeight)
{
	int nMips = 0;
	while (nWidth || nHeight)
	{
		if (!nWidth ) nWidth  = 1;
		if (!nHeight) nHeight = 1;
		nWidth  >>= 1;
		nHeight >>= 1;
		nMips++;
	}
	return nMips;
}

uint32 CTexture::TextureDataSize(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nSlices, const ETEX_Format eTF, ETEX_TileMode eTM)
{
	FUNCTION_PROFILER_RENDERER;

	// Don't allow 0 dimensions, it's clearly wrong to reflect on "unspecified-yet" textures.
	CRY_ASSERT(eTF != eTF_Unknown && nWidth && nHeight && nDepth);
	// Allow 0 mips and 0 slices to generate offsets with this function.
	if (!nMips || !nSlices)
		return 0;

	const bool bIsBlockCompressed = IsBlockCompressed(eTF);
	nWidth  = max(1U, nWidth );
	nHeight = max(1U, nHeight);
	nDepth  = max(1U, nDepth );

	if (eTM != eTM_None)
	{
		// NOTE: Using this function to acquire strides of elements or rows (and even slices in arrays),
		//       is not yielding any usable information. In the moment the clients need to be aware that
		//       the internal layout for tiled modes can't be interpreted and stay away from it.
		// TODO: Create separate interfaces for sub-resource size queries and for layout-stride queries

#if CRY_PLATFORM_ORBIS
		if (bIsBlockCompressed)
		{
			nWidth  = ((nWidth  + 3) & (-4));
			nHeight = ((nHeight + 3) & (-4));
		}
#endif

#if CRY_PLATFORM_CONSOLE
		return CDeviceTexture::TextureDataSize(nWidth, nHeight, nDepth, nMips, nSlices, eTF, eTM, CDeviceObjectFactory::BIND_SHADER_RESOURCE);
#endif

		__debugbreak();
		return 0;
	}
	else
	{
		const uint32 nBytesPerElement = bIsBlockCompressed ? BytesPerBlock(eTF) : BytesPerPixel(eTF);

		uint32 nSize = 0;
		while ((nWidth || nHeight || nDepth) && nMips)
		{
			nWidth  = max(1U, nWidth );
			nHeight = max(1U, nHeight);
			nDepth  = max(1U, nDepth );

			uint32 nU = nWidth;
			uint32 nV = nHeight;
			uint32 nW = nDepth;

			if (bIsBlockCompressed)
			{
				// depth is not 4x4x4 compressed, but 4x4x1
				nU = ((nWidth  + 3) / (4));
				nV = ((nHeight + 3) / (4));
			}

			nSize += nU * nV * nW * nBytesPerElement;

			nWidth  >>= 1;
			nHeight >>= 1;
			nDepth  >>= 1;

			--nMips;
		}

		return nSize * nSlices;
	}
}

bool CTexture::IsInPlaceFormat(const ETEX_Format fmt)
{
	switch (fmt)
	{
	case eTF_R8G8B8A8S:
	case eTF_R8G8B8A8:

	case eTF_R1:
	case eTF_A8:
	case eTF_R8:
	case eTF_R8S:
	case eTF_R16:
	case eTF_R16S:
	case eTF_R16F:
	case eTF_R32F:
	case eTF_R8G8:
	case eTF_R8G8S:
	case eTF_R16G16:
	case eTF_R16G16S:
	case eTF_R16G16F:
	case eTF_R32G32F:
	case eTF_R11G11B10F:
	case eTF_R10G10B10A2:
	case eTF_R16G16B16A16:
	case eTF_R16G16B16A16S:
	case eTF_R16G16B16A16F:
	case eTF_R32G32B32A32F:

	case eTF_BC1:
	case eTF_BC2:
	case eTF_BC3:
	case eTF_BC4U:
	case eTF_BC4S:
	case eTF_BC5U:
	case eTF_BC5S:
#if defined(CRY_DDS_DX10_SUPPORT)
	case eTF_BC6UH:
	case eTF_BC6SH:
	case eTF_BC7:
	case eTF_R9G9B9E5:
#endif
	case eTF_CTX1:
	case eTF_EAC_R11:
	case eTF_EAC_R11S:
	case eTF_EAC_RG11:
	case eTF_EAC_RG11S:
	case eTF_ETC2:
	case eTF_ETC2A:
	case eTF_ASTC_LDR_4x4:

	case eTF_B8G8R8A8:
	case eTF_B8G8R8X8:
		return true;
	default:
		return false;
	}
}

void CTexture::ExpandMipFromFile(byte* pSrcData, const int dstSize, const byte* src, const int srcSize, const ETEX_Format eSrcFormat, const ETEX_Format eDstFormat)
{
	if (IsInPlaceFormat(eSrcFormat))
	{
		assert(dstSize == srcSize);
		if (pSrcData != src)
		{
			cryMemcpy(pSrcData, src, srcSize);
		}

		return;
	}

	// upload mip from file with conversions depending on format and platform specifics
	switch (eSrcFormat)
	{
	case eTF_B8G8R8: // -> eTF_R8G8B8A8
		assert(eDstFormat == eTF_R8G8B8A8);
		{
			for (int i = srcSize / 3 - 1; i >= 0; --i)
			{
				pSrcData[i * 4 + 0] = src[i * 3 + 2];
				pSrcData[i * 4 + 1] = src[i * 3 + 1];
				pSrcData[i * 4 + 2] = src[i * 3 + 0];
				pSrcData[i * 4 + 3] = 255;
			}
		}
		break;
	case eTF_L8V8U8X8: // -> eTF_R8G8B8A8S
		assert(eDstFormat == eTF_R8G8B8A8S);
		{
			for (int i = srcSize / 4 - 1; i >= 0; --i)
			{
				pSrcData[i * 4 + 0] = src[i * 3 + 0];
				pSrcData[i * 4 + 1] = src[i * 3 + 1];
				pSrcData[i * 4 + 2] = src[i * 3 + 2];
				pSrcData[i * 4 + 3] = src[i * 3 + 3];
			}
		}
		break;
	case eTF_L8V8U8: // -> eTF_R8G8B8A8S
		assert(eDstFormat == eTF_R8G8B8A8S);
		{
			for (int i = srcSize / 3 - 1; i >= 0; --i)
			{
				pSrcData[i * 4 + 0] = src[i * 3 + 0];
				pSrcData[i * 4 + 1] = src[i * 3 + 1];
				pSrcData[i * 4 + 2] = src[i * 3 + 2];
				pSrcData[i * 4 + 3] = 255;
			}
		}
		break;
	case eTF_L8: // -> eTF_R8G8B8A8
		assert(eDstFormat == eTF_R8G8B8A8);
		{
			for (int i = srcSize - 1; i >= 0; --i)
			{
				const byte bSrc = src[i];
				pSrcData[i * 4 + 0] = bSrc;
				pSrcData[i * 4 + 1] = bSrc;
				pSrcData[i * 4 + 2] = bSrc;
				pSrcData[i * 4 + 3] = 255;
			}
		}
		break;
	case eTF_A8L8: // -> eTF_R8G8B8A8
		assert(eDstFormat == eTF_R8G8B8A8);
		{
			for (int i = srcSize - 1; i >= 0; i -= 2)
			{
				const byte bSrcL = src[i - 1];
				const byte bSrcA = src[i - 0];
				pSrcData[i * 4 + 0] = bSrcL;
				pSrcData[i * 4 + 1] = bSrcL;
				pSrcData[i * 4 + 2] = bSrcL;
				pSrcData[i * 4 + 3] = bSrcA;
			}
		}
		break;

	case eTF_B5G5R5A1: // -> eTF_B8G8R8A8
		assert(eDstFormat == eTF_B8G8R8A8);
		{
			for (int i = srcSize / 2 - 1; i >= 0; --i)
			{
				const uint16 rgb5551 = uint16((src[i * 2 + 0] << 8) + src[i * 2 + 1]);
				pSrcData[i * 4 + 0] = ((rgb5551 >>  0) * 33) >> 2;
				pSrcData[i * 4 + 1] = ((rgb5551 >>  5) * 33) >> 2;
				pSrcData[i * 4 + 2] = ((rgb5551 >> 10) * 33) >> 2;
				pSrcData[i * 4 + 3] = ((rgb5551 >> 15) ? 255 : 0);
			}
		}
		break;
	case eTF_B5G6R5: // -> eTF_B8G8R8X8
		assert(eDstFormat == eTF_B8G8R8X8);
		{
			for (int i = srcSize / 2 - 1; i >= 0; --i)
			{
				const uint16 rgb565 = uint16((src[i * 2 + 0] << 8) + src[i * 2 + 1]);
				pSrcData[i * 4 + 0] = ((rgb565 >>  0) * 33) >> 2;
				pSrcData[i * 4 + 1] = ((rgb565 >>  5) * 65) >> 4;
				pSrcData[i * 4 + 2] = ((rgb565 >> 11) * 33) >> 2;
				pSrcData[i * 4 + 3] = 255;
			}
		}
		break;
	case eTF_B4G4R4A4: // -> eTF_B8G8R8A8
	case eTF_R4G4B4A4: // -> eTF_R8G8B8A8
		assert((eSrcFormat == eTF_B4G4R4A4 && eDstFormat == eTF_B8G8R8A8) ||
		       (eSrcFormat == eTF_R4G4B4A4 && eDstFormat == eTF_R8G8B8A8));
		{
			for (int i = srcSize / 2 - 1; i >= 0; --i)
			{
				const uint16 rgb4444 = uint16((src[i * 2 + 0] << 8) + src[i * 2 + 1]);
				pSrcData[i * 4 + 0] = (rgb4444 >> 0) * 17;
				pSrcData[i * 4 + 1] = (rgb4444 >> 4) * 17;
				pSrcData[i * 4 + 2] = (rgb4444 >> 8) * 17;
				pSrcData[i * 4 + 3] = (rgb4444 >> 12) * 17;
			}
		}
		break;
	case eTF_R4G4: // -> eTF_R8G8|eTF_R8G8B8A8
		assert(eDstFormat == eTF_R8G8 || eDstFormat == eTF_R8G8B8A8);
		if (eDstFormat == eTF_R8G8)
		{
			for (int i = srcSize / 1 - 1; i >= 0; --i)
			{
				const uint8 rgb44 = uint8(src[i * 1 + 0]);
				pSrcData[i * 2 + 0] = (rgb44 >> 0) * 17;
				pSrcData[i * 2 + 1] = (rgb44 >> 4) * 17;
			}
		}
		else
		{
			for (int i = srcSize / 1 - 1; i >= 0; --i)
			{
				const uint8 rgb44 = uint8(src[i * 1 + 0]);
				pSrcData[i * 4 + 0] = (rgb44 >> 0) * 17;
				pSrcData[i * 4 + 1] = (rgb44 >> 4) * 17;
				pSrcData[i * 4 + 2] = 0;
				pSrcData[i * 4 + 3] = 255;
			}
		}
		break;

	default:
		assert(0);
	}
}

bool CTexture::Invalidate(int nNewWidth, int nNewHeight, ETEX_Format eNewFormat)
{
	bool bRelease = false;
	if (nNewWidth > 0 && nNewWidth != m_nWidth)
	{
		m_nWidth = nNewWidth;
		bRelease = true;
	}
	if (nNewHeight > 0 && nNewHeight != m_nHeight)
	{
		m_nHeight = nNewHeight;
		bRelease = true;
	}
	if (eNewFormat != eTF_Unknown && eNewFormat != m_eSrcFormat)
	{
		m_eSrcFormat = eNewFormat;
		SetClosestFormatSupported();

		bRelease = true;
	}

	if (!m_pDevTexture)
		return false;

	if (bRelease)
	{
		if (m_eFlags & FT_FORCE_MIPS)
			m_nMips = 1;

		ReleaseDeviceTexture(true);
	}

	return bRelease;
}

D3DBaseView* CTexture::GetResourceView(const SResourceView& rvDesc)
{
	if (CDeviceTexture* pDevTex = GetDevTexture(rvDesc.m_Desc.bMultisample))
	{
		ResourceViewHandle hView = pDevTex->GetOrCreateResourceViewHandle(rvDesc);
		return pDevTex->LookupResourceView(hView).second;
	}

	return nullptr;
}

D3DBaseView* CTexture::GetResourceView(const SResourceView& rvDesc) const
{
	if (CDeviceTexture* pDevTex = GetDevTexture(rvDesc.m_Desc.bMultisample))
	{
		ResourceViewHandle hView = pDevTex->GetResourceViewHandle(rvDesc);
		if (hView != ResourceViewHandle::Unspecified)
			return pDevTex->LookupResourceView(hView).second;
	}

	return nullptr;
}

void CTexture::SetResourceView(const SResourceView& rvDesc, D3DBaseView* pView)
{
	if (CDeviceTexture* pDevTex = GetDevTexture(rvDesc.m_Desc.bMultisample))
	{
		ResourceViewHandle hView = pDevTex->GetResourceViewHandle(rvDesc);
		if (hView != ResourceViewHandle::Unspecified)
		{
			SAFE_RELEASE(pDevTex->LookupResourceView(hView).second);

			pDevTex->LookupResourceView(hView).second = pView;
			pDevTex->LookupResourceView(hView).second->AddRef();
		}
	}
}

void CTexture::SetDefaultShaderResourceView(D3DBaseView* pDeviceShaderResource, bool bMultisampled /*= false*/)
{
	CDeviceTexture* pDevTex = GetDevTexture(bMultisampled && IsMSAA());

	SAFE_RELEASE(pDevTex->LookupResourceView(EDefaultResourceViews::Default).second);

	pDevTex->LookupResourceView(EDefaultResourceViews::Default).second = pDeviceShaderResource;
	pDevTex->LookupResourceView(EDefaultResourceViews::Default).second->AddRef();

	// Notify that resource is dirty
	if (!(m_eFlags & FT_USAGE_RENDERTARGET))
	{
		InvalidateDeviceResource(this, eDeviceResourceViewDirty);
	}
}

byte* CTexture::GetData32(int nSide, int nLevel, byte* pDstData, ETEX_Format eDstFormat)
{
#if CRY_PLATFORM_WINDOWS
	// NOTE: the function will not maintain any dirty state and always download the data, don't use it in the render-loop
	CDeviceTexture* pDevTexture = GetDevTexture();
	pDevTexture->DownloadToStagingResource(D3D11CalcSubresource(nLevel, nSide, m_nMips), [&](void* pData, uint32 rowPitch, uint32 slicePitch)
	{
		if (m_eDstFormat != eTF_R8G8B8A8)
		{
		  int nOutSize = 0;

		  if (m_eSrcFormat == eDstFormat && pDstData)
		  {
		    memcpy(pDstData, pData, GetDeviceDataSize());
		  }
		  else
		  {
		    pDstData = Convert((byte*)pData, m_nWidth, m_nHeight, 1, m_eSrcFormat, eDstFormat, 1, nOutSize, true);
		  }
		}
		else
		{
		  if (!pDstData)
		  {
		    pDstData = new byte[m_nWidth * m_nHeight * 4];
		  }

		  memcpy(pDstData, pData, m_nWidth * m_nHeight * 4);
		}

		return true;
	});

	return pDstData;
#else
	return 0;
#endif
}

const int CTexture::GetSize(bool bIncludePool) const
{
	int nSize = sizeof(CTexture);
	nSize += m_SrcName.capacity();

	// TODO: neccessary?
//	if (m_pRenderTargetData)
//	{
//		nSize += sizeof(*m_pRenderTargetData);
//		nSize += m_pRenderTargetData->m_DirtyRects.capacity() * sizeof(RECT);
//	}

	if (m_pFileTexMips)
	{
		nSize += m_pFileTexMips->GetSize(m_nMips, m_CacheFileHeader.m_nSides);
		if (bIncludePool && m_pFileTexMips->m_pPoolItem)
			nSize += m_pFileTexMips->m_pPoolItem->GetSize();
	}

	return nSize;
}

void CTexture::Init()
{
	SDynTexture::Init();
	InitStreaming();
	ResetTMUs();

	SDynTexture2::Init(eTP_Clouds);
	SDynTexture2::Init(eTP_Sprites);
	SDynTexture2::Init(eTP_VoxTerrain);
	SDynTexture2::Init(eTP_DynTexSources);

	if (!gRenDev->IsShaderCacheGenMode())
		LoadScaleformSystemTextures();
}

void CTexture::PostInit()
{
	LOADING_TIME_PROFILE_SECTION;
	if (!gRenDev->IsShaderCacheGenMode())
		CTexture::LoadDefaultSystemTextures();
}

int __cdecl TexCallback(const VOID* arg1, const VOID* arg2)
{
	CTexture** pi1 = (CTexture**)arg1;
	CTexture** pi2 = (CTexture**)arg2;
	CTexture* ti1 = *pi1;
	CTexture* ti2 = *pi2;

	if (ti1->GetDeviceDataSize() > ti2->GetDeviceDataSize())
		return -1;
	if (ti1->GetDeviceDataSize() < ti2->GetDeviceDataSize())
		return 1;
	return stricmp(ti1->GetSourceName(), ti2->GetSourceName());
}

int __cdecl TexCallbackMips(const VOID* arg1, const VOID* arg2)
{
	CTexture** pi1 = (CTexture**)arg1;
	CTexture** pi2 = (CTexture**)arg2;
	CTexture* ti1 = *pi1;
	CTexture* ti2 = *pi2;

	int nSize1, nSize2;

	nSize1 = ti1->GetActualSize();
	nSize2 = ti2->GetActualSize();

	if (nSize1 > nSize2)
		return -1;
	if (nSize1 < nSize2)
		return 1;
	return stricmp(ti1->GetSourceName(), ti2->GetSourceName());
}

void CTexture::Update()
{
	FUNCTION_PROFILER_RENDERER;

	CRenderer* rd = gRenDev;
	char buf[256] = "";

	// reload pending texture reload requests
	{
		std::set<string> queue;

		s_xTexReloadLock.Lock();
		s_vTexReloadRequests.swap(queue);
		s_xTexReloadLock.Unlock();

		for (std::set<string>::iterator i = queue.begin(); i != queue.end(); ++i)
			ReloadFile(*i);
	}

	CTexture::s_bStreamingFromHDD = gEnv->pSystem->GetStreamEngine()->IsStreamDataOnHDD();
	CTexture::s_nStatsStreamPoolInUseMem = CTexture::s_pPoolMgr->GetInUseSize();

	s_pTextureStreamer->ApplySchedule(ITextureStreamer::eASF_Full);
	s_pTextureStreamer->BeginUpdateSchedule();

#ifdef ENABLE_TEXTURE_STREAM_LISTENER
	StreamUpdateStats();
#endif

	SDynTexture::Tick();

	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
	ResourcesMapItor itor;

	if ((s_nStreamingMode != CRenderer::CV_r_texturesstreaming) || (s_nStreamingUpdateMode != CRenderer::CV_r_texturesstreamingUpdateType))
	{
		InitStreaming();
	}

	if (pRL)
	{
#ifndef CONSOLE_CONST_CVAR_MODE
		uint32 i;
		if (CRenderer::CV_r_texlog == 2 || CRenderer::CV_r_texlog == 3 || CRenderer::CV_r_texlog == 4)
		{
			FILE* fp = NULL;
			TArray<CTexture*> Texs;
			int Size = 0;
			int PartSize = 0;
			if (CRenderer::CV_r_texlog == 2 || CRenderer::CV_r_texlog == 3)
			{
				for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
				{
					CTexture* tp = (CTexture*)itor->second;
					if (CRenderer::CV_r_texlog == 3 && tp->IsNoTexture())
					{
						Texs.AddElem(tp);
					}
					else if (CRenderer::CV_r_texlog == 2 && !tp->IsNoTexture() && tp->m_pFileTexMips) // (tp->GetFlags() & FT_FROMIMAGE))
					{
						Texs.AddElem(tp);
					}
				}
				if (CRenderer::CV_r_texlog == 3)
				{
					CryLogAlways("Logging to MissingTextures.txt...");
					fp = fxopen("MissingTextures.txt", "w");
				}
				else
				{
					CryLogAlways("Logging to UsedTextures.txt...");
					fp = fxopen("UsedTextures.txt", "w");
				}
				fprintf(fp, "*** All textures: ***\n");

				if (Texs.Num())
					qsort(&Texs[0], Texs.Num(), sizeof(CTexture*), TexCallbackMips);

				for (i = 0; i < Texs.Num(); i++)
				{
					int w = Texs[i]->GetWidth();
					int h = Texs[i]->GetHeight();

					int nTSize = Texs[i]->m_pFileTexMips->GetSize(Texs[i]->GetNumMips(), Texs[i]->GetNumSides());

					fprintf(fp, "%d\t\t%d x %d\t\tType: %s\t\tMips: %d\t\tFormat: %s\t\t(%s)\n", nTSize, w, h, Texs[i]->NameForTextureType(Texs[i]->GetTextureType()), Texs[i]->GetNumMips(), Texs[i]->NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
					//Size += Texs[i]->GetDataSize();
					Size += nTSize;

					PartSize += Texs[i]->GetDeviceDataSize();
				}
				fprintf(fp, "*** Total Size: %d\n\n", Size /*, PartSize, PartSize */);

				Texs.Free();
			}
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* tp = (CTexture*)itor->second;
				if (tp->m_nAccessFrameID == rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_nFrameUpdateID)
				{
					Texs.AddElem(tp);
				}
			}

			if (fp)
			{
				fclose(fp);
				fp = 0;
			}

			fp = fxopen("UsedTextures_Frame.txt", "w");

			if (fp)
				fprintf(fp, "\n\n*** Textures used in current frame: ***\n");
			else
				IRenderAuxText::TextToScreenColor(4, 13, 1, 1, 0, 1, "*** Textures used in current frame: ***");
			int nY = 17;

			if (Texs.Num())
				qsort(&Texs[0], Texs.Num(), sizeof(CTexture*), TexCallback);

			Size = 0;
			for (i = 0; i < Texs.Num(); i++)
			{
				if (fp)
					fprintf(fp, "%.3fKb\t\tType: %s\t\tFormat: %s\t\t(%s)\n", Texs[i]->GetDeviceDataSize() / 1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
				else
				{
					cry_sprintf(buf, "%.3fKb  Type: %s  Format: %s  (%s)", Texs[i]->GetDeviceDataSize() / 1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
					IRenderAuxText::TextToScreenColor(4, nY, 0, 1, 0, 1, buf);
					nY += 3;
				}
				PartSize += Texs[i]->GetDeviceDataSize();
				Size += Texs[i]->GetDataSize();
			}
			if (fp)
			{
				fprintf(fp, "*** Total Size: %.3fMb, Device Size: %.3fMb\n\n", Size / (1024.0f * 1024.0f), PartSize / (1024.0f * 1024.0f));
			}
			else
			{
				cry_sprintf(buf, "*** Total Size: %.3fMb, Device Size: %.3fMb", Size / (1024.0f * 1024.0f), PartSize / (1024.0f * 1024.0f));
				IRenderAuxText::TextToScreenColor(4, nY + 1, 0, 1, 1, 1, buf);
			}

			Texs.Free();
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* tp = (CTexture*)itor->second;
				if (tp && !tp->IsNoTexture())
				{
					Texs.AddElem(tp);
				}
			}

			if (fp)
			{
				fclose(fp);
				fp = 0;
			}
			fp = fxopen("UsedTextures_All.txt", "w");

			if (fp)
				fprintf(fp, "\n\n*** All Existing Textures: ***\n");
			else
				IRenderAuxText::TextToScreenColor(4, 13, 1, 1, 0, 1, "*** Textures loaded: ***");

			if (Texs.Num())
				qsort(&Texs[0], Texs.Num(), sizeof(CTexture*), TexCallback);

			Size = 0;
			for (i = 0; i < Texs.Num(); i++)
			{
				if (fp)
				{
					int w = Texs[i]->GetWidth();
					int h = Texs[i]->GetHeight();
					fprintf(fp, "%d\t\t%d x %d\t\t%d mips (%.3fKb)\t\tType: %s \t\tFormat: %s\t\t(%s)\n", Texs[i]->GetDataSize(), w, h, Texs[i]->GetNumMips(), Texs[i]->GetDeviceDataSize() / 1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
				}
				else
				{
					cry_sprintf(buf, "%.3fKb  Type: %s  Format: %s  (%s)", Texs[i]->GetDataSize() / 1024.0f, CTexture::NameForTextureType(Texs[i]->GetTextureType()), CTexture::NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
					IRenderAuxText::TextToScreenColor(4, nY, 0, 1, 0, 1, buf);
					nY += 3;
				}
				Size += Texs[i]->GetDeviceDataSize();
			}
			if (fp)
			{
				fprintf(fp, "*** Total Size: %.3fMb\n\n", Size / (1024.0f * 1024.0f));
			}
			else
			{
				cry_sprintf(buf, "*** Total Size: %.3fMb", Size / (1024.0f * 1024.0f));
				IRenderAuxText::TextToScreenColor(4, nY + 1, 0, 1, 1, 1, buf);
			}

			Texs.Free();
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* tp = (CTexture*)itor->second;
				if (tp && !tp->IsNoTexture() && !tp->IsStreamed())
				{
					Texs.AddElem(tp);
				}
			}

			if (fp)
			{
				fclose(fp);
				fp = 0;
			}

			if (CRenderer::CV_r_texlog != 4)
				CRenderer::CV_r_texlog = 0;
		}
		else if (CRenderer::CV_r_texlog == 1)
		{
			//char *str = GetTexturesStatusText();

			TArray<CTexture*> Texs;
			//TArray<CTexture *> TexsNM;
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* tp = (CTexture*)itor->second;
				if (tp && !tp->IsNoTexture())
				{
					Texs.AddElem(tp);
					//if (tp->GetFlags() & FT_TEX_NORMAL_MAP)
					//  TexsNM.AddElem(tp);
				}
			}

			if (Texs.Num())
				qsort(&Texs[0], Texs.Num(), sizeof(CTexture*), TexCallback);

			int64 AllSize = 0;
			int64 Size = 0;
			int64 PartSize = 0;
			int64 NonStrSize = 0;
			int nNoStr = 0;
			int64 SizeNM = 0;
			int64 SizeDynCom = 0;
			int64 SizeDynAtl = 0;
			int64 PartSizeNM = 0;
			int nNumTex = 0;
			int nNumTexNM = 0;
			int nNumTexDynAtl = 0;
			int nNumTexDynCom = 0;
			for (i = 0; i < Texs.Num(); i++)
			{
				CTexture* tex = Texs[i];
				const uint32 texFlags = tex->GetFlags();
				const int texDataSize = tex->GetDataSize();
				const int texDeviceDataSize = tex->GetDeviceDataSize();

				if (tex->GetDevTexture() && !(texFlags & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET)))
				{
					AllSize += texDataSize;
					if (!Texs[i]->IsStreamed())
					{
						NonStrSize += texDataSize;
						nNoStr++;
					}
				}

				if (texFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC))
				{
					if (texFlags & FT_USAGE_ATLAS)
					{
						++nNumTexDynAtl;
						SizeDynAtl += texDataSize;
					}
					else
					{
						++nNumTexDynCom;
						SizeDynCom += texDataSize;
					}
				}
				else if (0 == (texFlags & FT_TEX_NORMAL_MAP))
				{
					if (!Texs[i]->IsUnloaded())
					{
						++nNumTex;
						Size += texDataSize;
						PartSize += texDeviceDataSize;
					}
				}
				else
				{
					if (!Texs[i]->IsUnloaded())
					{
						++nNumTexNM;
						SizeNM += texDataSize;
						PartSizeNM += texDeviceDataSize;
					}
				}
			}

			cry_sprintf(buf, "All texture objects: %u (Size: %.3fMb), NonStreamed: %d (Size: %.3fMb)", Texs.Num(), AllSize / (1024.0 * 1024.0), nNoStr, NonStrSize / (1024.0 * 1024.0));
			IRenderAuxText::TextToScreenColor(4, 13, 1, 1, 0, 1, buf);
			cry_sprintf(buf, "All Loaded Texture Maps: %d (All MIPS: %.3fMb, Loaded MIPS: %.3fMb)", nNumTex, Size / (1024.0f * 1024.0f), PartSize / (1024.0 * 1024.0));
			IRenderAuxText::TextToScreenColor(4, 16, 1, 1, 0, 1, buf);
			cry_sprintf(buf, "All Loaded Normal Maps: %d (All MIPS: %.3fMb, Loaded MIPS: %.3fMb)", nNumTexNM, SizeNM / (1024.0 * 1024.0), PartSizeNM / (1024.0 * 1024.0));
			IRenderAuxText::TextToScreenColor(4, 19, 1, 1, 0, 1, buf);
			cry_sprintf(buf, "All Dynamic textures: %d (%.3fMb), %d Atlases (%.3fMb), %d Separared (%.3fMb)", nNumTexDynAtl + nNumTexDynCom, (SizeDynAtl + SizeDynCom) / (1024.0 * 1024.0), nNumTexDynAtl, SizeDynAtl / (1024.0 * 1024.0), nNumTexDynCom, SizeDynCom / (1024.0 * 1024.0));
			IRenderAuxText::TextToScreenColor(4, 22, 1, 1, 0, 1, buf);

			Texs.Free();
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* tp = (CTexture*)itor->second;
				if (tp && !tp->IsNoTexture() && tp->m_nAccessFrameID == rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_nFrameUpdateID)
				{
					Texs.AddElem(tp);
				}
			}

			if (Texs.Num())
				qsort(&Texs[0], Texs.Num(), sizeof(CTexture*), TexCallback);

			Size = 0;
			SizeDynAtl = 0;
			SizeDynCom = 0;
			PartSize = 0;
			NonStrSize = 0;
			for (i = 0; i < Texs.Num(); i++)
			{
				Size += Texs[i]->GetDataSize();
				if (Texs[i]->GetFlags() & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET))
				{
					if (Texs[i]->GetFlags() & FT_USAGE_ATLAS)
						SizeDynAtl += Texs[i]->GetDataSize();
					else
						SizeDynCom += Texs[i]->GetDataSize();
				}
				else
					PartSize += Texs[i]->GetDeviceDataSize();
				if (!Texs[i]->IsStreamed())
					NonStrSize += Texs[i]->GetDataSize();
			}
			cry_sprintf(buf, "Current tex. objects: %u (Size: %.3fMb, Dyn. Atlases: %.3f, Dyn. Separated: %.3f, Loaded: %.3f, NonStreamed: %.3f)", Texs.Num(), Size / (1024.0f * 1024.0f), SizeDynAtl / (1024.0f * 1024.0f), SizeDynCom / (1024.0f * 1024.0f), PartSize / (1024.0f * 1024.0f), NonStrSize / (1024.0f * 1024.0f));
			IRenderAuxText::TextToScreenColor(4, 27, 1, 0, 0, 1, buf);
		}
#endif
	}
}

void CTexture::RT_LoadingUpdate()
{
	CTexture::s_bStreamingFromHDD = gEnv->pSystem->GetStreamEngine()->IsStreamDataOnHDD();
	CTexture::s_nStatsStreamPoolInUseMem = CTexture::s_pPoolMgr->GetInUseSize();

	ITextureStreamer::EApplyScheduleFlags asf = CTexture::s_bPrecachePhase
	                                            ? ITextureStreamer::eASF_InOut // Exclude the prep update, as it will be done by the RLT (and can be expensive)
	                                            : ITextureStreamer::eASF_Full;

	s_pTextureStreamer->ApplySchedule(asf);
}

void CTexture::RLT_LoadingUpdate()
{
	s_pTextureStreamer->BeginUpdateSchedule();
}

//=========================================================================

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

SEnvTexture* CTexture::FindSuitableEnvTex(Vec3& Pos, Ang3& Angs, bool bMustExist, int RendFlags, bool bUseExistingREs, CShader* pSH, CShaderResources* pRes, CRenderObject* pObj, bool bReflect, CRenderElement* pRE, bool* bMustUpdate)
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
			pRE->mfCenter(objPos, pObj);
		else
			objPos = pObj->GetTranslation();
	}
	float dist = 999999;
	for (i = 0; i < MAX_ENVTEXTURES; i++)
	{
		SEnvTexture* cur = &CTexture::s_EnvTexts[i];
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
		return &CTexture::s_EnvTexts[firstForUse];
	if (bReflect)
		dist = distO;

	float curTime = iTimer->GetCurrTime();
	int nUpdate = -2;
	float fTimeInterval = dist * CRenderer::CV_r_envtexupdateinterval + CRenderer::CV_r_envtexupdateinterval * 0.5f;
	float fDelta = curTime - CTexture::s_EnvTexts[firstForUse].m_TimeLastUpdated;
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
	if (nUpdate == -2)
	{
		// No need to update (Up to date)
		return &CTexture::s_EnvTexts[firstForUse];
	}
	if (!CTexture::s_EnvTexts[nUpdate].m_pTex)
		return NULL;
	if (nUpdate >= 0)
	{
		if (!CTexture::s_EnvTexts[nUpdate].m_pTex->m_pTexture || gRenDev->m_RP.m_PS[gRenDev->m_RP.m_nProcessThreadID].m_fEnvTextUpdateTime < 0.1f)
		{
			int n = nUpdate;
			CTexture::s_EnvTexts[n].m_TimeLastUpdated = curTime;
			CTexture::s_EnvTexts[n].m_CamPos = Pos;
			CTexture::s_EnvTexts[n].m_Angle = Angs;
			CTexture::s_EnvTexts[n].m_ObjPos = objPos;
			CTexture::s_EnvTexts[n].m_bReflected = bReflect;
			if (bMustUpdate)
				*bMustUpdate = true;
		}
		gRenDev->m_RP.m_PS[gRenDev->m_RP.m_nProcessThreadID].m_fEnvTextUpdateTime += iTimer->GetAsyncCurTime() - time0;
		return &CTexture::s_EnvTexts[nUpdate];
	}

	dist = 0;
	firstForUse = -1;
	for (i = 0; i < MAX_ENVTEXTURES; i++)
	{
		SEnvTexture* cur = &CTexture::s_EnvTexts[i];
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
	CTexture::s_EnvTexts[n].m_TimeLastUpdated = curTime;
	CTexture::s_EnvTexts[n].m_CamPos = Pos;
	CTexture::s_EnvTexts[n].m_ObjPos = objPos;
	CTexture::s_EnvTexts[n].m_Angle = Angs;
	CTexture::s_EnvTexts[n].m_bReflected = bReflect;
	if (bMustUpdate)
		*bMustUpdate = true;

	gRenDev->m_RP.m_PS[gRenDev->m_RP.m_nProcessThreadID].m_fEnvTextUpdateTime += iTimer->GetAsyncCurTime() - time0;
	return &CTexture::s_EnvTexts[n];

}

//===========================================================================

void CTexture::ShutDown()
{
	uint32 i;

	RT_FlushAllStreamingTasks(true);

	ReleaseSystemTextures(true);

	if (CRenderer::CV_r_releaseallresourcesonexit)
	{
		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (pRL)
		{
			int n = 0;
			ResourcesMapItor itor;
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); )
			{
				CTexture* pTX = (CTexture*)itor->second;
				itor++;
				if (!pTX)
					continue;
				if (CRenderer::CV_r_printmemoryleaks)
					iLog->Log("Warning: CTexture::ShutDown: Texture %s was not deleted (%d)", pTX->GetName(), pTX->GetRefCounter());
				SAFE_RELEASE_FORCE(pTX);
				n++;
			}
		}
		// To avoid crash on ShutDown
		CTexture::s_ptexSceneNormalsMap = NULL;
		CTexture::s_ptexSceneDiffuse = NULL;
		CTexture::s_ptexSceneSpecular = NULL;
		CTexture::s_ptexSceneDiffuseAccMap = NULL;
		CTexture::s_ptexSceneSpecularAccMap = NULL;
		CTexture::s_ptexBackBuffer = NULL;
		CTexture::s_ptexPrevBackBuffer[0][0] = NULL;
		CTexture::s_ptexPrevBackBuffer[1][0] = NULL;
		CTexture::s_ptexPrevBackBuffer[0][1] = NULL;
		CTexture::s_ptexPrevBackBuffer[1][1] = NULL;
		CTexture::s_ptexSceneTarget = NULL;
		CTexture::s_ptexZTarget = NULL;
		CTexture::s_ptexHDRTarget = NULL;
		CTexture::s_ptexStereoL = NULL;
		CTexture::s_ptexStereoR = NULL;
		for (uint32 i = 0; i < 2; ++i)
			CTexture::s_ptexQuadLayers[i] = NULL;
	}

	if (s_ShaderTemplatesInitialized)
	{
		for (i = 0; i < EFTT_MAX; i++)
		{
			s_ShaderTemplates[i].~CTexture();
		}
	}
	s_ShaderTemplates.Free();

	SAFE_DELETE(s_pTexNULL);

	s_pPoolMgr->Flush();
}

bool CTexture::ReloadFile_Request(const char* szFileName)
{
	s_xTexReloadLock.Lock();
	s_vTexReloadRequests.insert(szFileName);
	s_xTexReloadLock.Unlock();

	return true;
}

bool CTexture::ReloadFile(const char* szFileName)
{
	char realNameBuffer[256];
	fpConvertDOSToUnixName(realNameBuffer, szFileName);

	char gameFolderPath[256];
	cry_strcpy(gameFolderPath, PathUtil::GetGameFolder());
	PREFAST_SUPPRESS_WARNING(6054); // it is nullterminated
	int gameFolderPathLength = strlen(gameFolderPath);
	if (gameFolderPathLength > 0 && gameFolderPath[gameFolderPathLength - 1] == '\\')
	{
		gameFolderPath[gameFolderPathLength - 1] = '/';
	}
	else if (gameFolderPathLength > 0 && gameFolderPath[gameFolderPathLength - 1] != '/')
	{
		gameFolderPath[gameFolderPathLength++] = '/';
		gameFolderPath[gameFolderPathLength] = 0;
	}

	char* realName = realNameBuffer;
	if (strlen(realNameBuffer) >= (uint32)gameFolderPathLength && memcmp(realName, gameFolderPath, gameFolderPathLength) == 0)
		realName += gameFolderPathLength;

	bool bStatus = true;

	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
	if (pRL)
	{
		AUTO_LOCK(CBaseResource::s_cResLock);

		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CTexture* tp = (CTexture*)itor->second;
			if (!tp)
				continue;

			if (!(tp->m_eFlags & FT_FROMIMAGE))
				continue;
			char srcNameBuffer[MAX_PATH + 1];
			fpConvertDOSToUnixName(srcNameBuffer, tp->m_SrcName.c_str());
			char* srcName = srcNameBuffer;
			if (strlen(srcName) >= (uint32)gameFolderPathLength && _strnicmp(srcName, gameFolderPath, gameFolderPathLength) == 0)
				srcName += gameFolderPathLength;
			//CryLogAlways("realName = %s srcName = %s gameFolderPath = %s szFileName = %s", realName, srcName, gameFolderPath, szFileName);
			if (!stricmp(realName, srcName))
			{
				if (!tp->Reload())
				{
					bStatus = false;
				}
			}
		}
	}

	return bStatus;
}

void CTexture::ReloadTextures()
{
	LOADING_TIME_PROFILE_SECTION;

	// Flush any outstanding texture requests before reloading
	gEnv->pRenderer->FlushPendingTextureTasks();

	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
	if (pRL)
	{
		ResourcesMapItor itor;
		int nID = 0;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++, nID++)
		{
			CTexture* tp = (CTexture*)itor->second;
			if (!tp)
				continue;
			if (!(tp->m_eFlags & FT_FROMIMAGE))
				continue;
			tp->Reload();
		}
	}
}

bool CTexture::SetNoTexture(CTexture* pDefaultTexture /* = s_ptexNoTexture*/)
{
	if (pDefaultTexture)
	{
		m_pDevTexture = pDefaultTexture->m_pDevTexture;
		m_eSrcFormat = pDefaultTexture->GetSrcFormat();
		m_eDstFormat = pDefaultTexture->GetDstFormat();
		m_nMips = pDefaultTexture->GetNumMips();
		m_nWidth = pDefaultTexture->GetWidth();
		m_nHeight = pDefaultTexture->GetHeight();
		m_nDepth = 1;
		m_nDefState = pDefaultTexture->m_nDefState;
		m_fAvgBrightness = 1.0f;
		m_cMinColor = 0.0f;
		m_cMaxColor = 1.0f;
		m_cClearColor = ColorF(0.0f, 0.0f, 0.0f, 1.0f);

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		m_nDeviceAddressInvalidated = m_pDevTexture->GetBaseAddressInvalidated();
#endif
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
		m_nESRAMOffset = SKIP_ESRAM;
#endif

		m_bNoTexture = true;
		if (m_pFileTexMips)
		{
			Unlink();
			StreamState_ReleaseInfo(this, m_pFileTexMips);
			m_pFileTexMips = NULL;
		}
		m_bStreamed = false;
		m_bPostponed = false;
		m_eFlags |= FT_FAILED;
		m_nDevTextureSize = 0;
		m_nPersistentSize = 0;
		return true;
	}
	return false;
}

//===========================================================================

void CTexture::ReleaseSystemTextures(bool bFinalRelease)
{
	if (gRenDev->m_pTextureManager)
		gRenDev->m_pTextureManager->ReleaseDefaultTextures();

	if (s_pStatsTexWantedLists)
	{
		for (int i = 0; i < 2; ++i)
			s_pStatsTexWantedLists[i].clear();
	}

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
	//SAFE_RELEASE_FORCE(s_ptexRainOcclusion);
	//SAFE_RELEASE_FORCE(s_ptexRainSSOcclusion[0]);
	//SAFE_RELEASE_FORCE(s_ptexRainSSOcclusion[1]);

	//SAFE_RELEASE_FORCE(s_ptexHitAreaRT[0]);
	//SAFE_RELEASE_FORCE(s_ptexHitAreaRT[1]);

	SAFE_RELEASE_FORCE(s_ptexVolumetricFog);
	SAFE_RELEASE_FORCE(s_ptexVolumetricClipVolumeStencil);

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

	SAFE_RELEASE_FORCE(s_ptexFromObj);
	SAFE_RELEASE_FORCE(s_ptexSvoTree);
	SAFE_RELEASE_FORCE(s_ptexSvoTris);
	SAFE_RELEASE_FORCE(s_ptexSvoGlobalCM);
	SAFE_RELEASE_FORCE(s_ptexSvoRgbs);
	SAFE_RELEASE_FORCE(s_ptexSvoNorm);
	SAFE_RELEASE_FORCE(s_ptexSvoOpac);

	SAFE_RELEASE_FORCE(s_ptexColorChart);

	SAFE_RELEASE_FORCE(s_ptexWindGrid);

	for (i = 0; i < MAX_ENVTEXTURES; i++)
	{
		s_EnvTexts[i].Release();
	}
	//m_EnvTexTemp.Release();

	SAFE_RELEASE_FORCE(s_ptexMipColors_Diffuse);
	SAFE_RELEASE_FORCE(s_ptexMipColors_Bump);
	SAFE_RELEASE_FORCE(s_ptexSkyDomeMie);
	SAFE_RELEASE_FORCE(s_ptexSkyDomeRayleigh);
	SAFE_RELEASE_FORCE(s_ptexSkyDomeMoon);
	SAFE_RELEASE_FORCE(s_ptexRT_ShadowPool);
	SAFE_RELEASE_FORCE(s_ptexFarPlane);

	SAFE_RELEASE_FORCE(s_ptexSceneNormalsMapMS);
	SAFE_RELEASE_FORCE(s_ptexSceneDiffuseAccMapMS);
	SAFE_RELEASE_FORCE(s_ptexSceneSpecularAccMapMS);

	s_CustomRT_2D.Free();
	//s_ShaderTemplates.Free();

	s_pPoolMgr->Flush();

	// release targets pools
	//SDynTexture_Shadow::ShutDown();
	SDynTexture::ShutDown();
	SDynTexture2::ShutDown();

	//ReleaseSystemTargets();

	m_bLoadedSystem = false;
}

void CTexture::LoadScaleformSystemTextures()
{
	LOADING_TIME_PROFILE_SECTION;
}

void CTexture::LoadDefaultSystemTextures()
{
	LOADING_TIME_PROFILE_SECTION;

	char str[256];
	int i;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Engine textures");

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
			{ s_ptexWhite,                       "%ENGINE%/EngineAssets/Textures/White.dds",                         FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexBlack,                       "%ENGINE%/EngineAssets/Textures/Black.dds",                         FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexBlackAlpha,                  "%ENGINE%/EngineAssets/Textures/BlackAlpha.dds",                    FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexBlackCM,                     "%ENGINE%/EngineAssets/Textures/BlackCM.dds",                       FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexGray,                        "%ENGINE%/EngineAssets/Textures/Grey.dds",                          FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexDefaultMergedDetail,         "%ENGINE%/EngineAssets/Textures/GreyAlpha.dds",                     FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexDefaultProbeCM,              "%ENGINE%/EngineAssets/Shading/defaultProbe_cm.tif",                FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexFlatBump,                    "%ENGINE%/EngineAssets/Textures/White_ddn.dds",                     FT_DONT_RELEASE | FT_DONT_STREAM | FT_TEX_NORMAL_MAP },
			{ s_ptexPaletteDebug,                "%ENGINE%/EngineAssets/Textures/palletteInst.dds",                  FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexPaletteTexelsPerMeter,       "%ENGINE%/EngineAssets/Textures/TexelsPerMeterGrad.dds",            FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexIconShaderCompiling,         "%ENGINE%/EngineAssets/Icons/ShaderCompiling.tif",                  FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexIconStreaming,               "%ENGINE%/EngineAssets/Icons/Streaming.tif",                        FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexIconStreamingTerrainTexture, "%ENGINE%/EngineAssets/Icons/StreamingTerrain.tif",                 FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexIconNavigationProcessing,    "%ENGINE%/EngineAssets/Icons/NavigationProcessing.tif",             FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexShadowJitterMap,             "%ENGINE%/EngineAssets/Textures/rotrandom.dds",                     FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexEnvironmentBRDF,             "%ENGINE%/EngineAssets/Shading/environmentBRDF.tif",                FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexScreenNoiseMap,              "%ENGINE%/EngineAssets/Textures/JumpNoiseHighFrequency_x27y19.dds", FT_DONT_RELEASE | FT_DONT_STREAM | FT_NOMIPS         },
			{ s_ptexDissolveNoiseMap,            "%ENGINE%/EngineAssets/Textures/noise.dds",                         FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexNoise3D,                     "%ENGINE%/EngineAssets/Textures/noise3d.dds",                       FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexGrainFilterMap,              "%ENGINE%/EngineAssets/ScreenSpace/grain_bayer_mul.tif",            FT_DONT_RELEASE | FT_DONT_STREAM | FT_NOMIPS         },
			{ s_ptexFilmGrainMap,                "%ENGINE%/EngineAssets/ScreenSpace/film_grain.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM | FT_NOMIPS         },
			{ s_ptexVignettingMap,               "%ENGINE%/EngineAssets/Shading/vignetting.tif",                     FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexAOJitter,                    "%ENGINE%/EngineAssets/ScreenSpace/PointsOnSphere4x4.tif",          FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexAOVOJitter,                  "%ENGINE%/EngineAssets/ScreenSpace/PointsOnSphereVO4x4.tif",        FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexNormalsFitting,              "%ENGINE%/EngineAssets/ScreenSpace/NormalsFitting.dds",             FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexPerlinNoiseMap,              "%ENGINE%/EngineAssets/Textures/perlinNoise2D.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM                     },

#if !defined(_RELEASE)
			{ s_ptexNoTexture,                   "%ENGINE%/EngineAssets/TextureMsg/ReplaceMe.tif",                   FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexNoTextureCM,                 "%ENGINE%/EngineAssets/TextureMsg/ReplaceMeCM.tif",                 FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexMipMapDebug,                 "%ENGINE%/EngineAssets/TextureMsg/MipMapDebug.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexColorBlue,                   "%ENGINE%/EngineAssets/TextureMsg/color_Blue.dds",                  FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexColorCyan,                   "%ENGINE%/EngineAssets/TextureMsg/color_Cyan.dds",                  FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexColorGreen,                  "%ENGINE%/EngineAssets/TextureMsg/color_Green.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexColorPurple,                 "%ENGINE%/EngineAssets/TextureMsg/color_Purple.dds",                FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexColorRed,                    "%ENGINE%/EngineAssets/TextureMsg/color_Red.dds",                   FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexColorWhite,                  "%ENGINE%/EngineAssets/TextureMsg/color_White.dds",                 FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexColorYellow,                 "%ENGINE%/EngineAssets/TextureMsg/color_Yellow.dds",                FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexColorOrange,                 "%ENGINE%/EngineAssets/TextureMsg/color_Orange.dds",                FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexColorMagenta,                "%ENGINE%/EngineAssets/TextureMsg/color_Magenta.dds",               FT_DONT_RELEASE | FT_DONT_STREAM                     },
#else
			{ s_ptexNoTexture,                   "%ENGINE%/EngineAssets/Textures/Grey.dds",                          FT_DONT_RELEASE | FT_DONT_STREAM                     },
			{ s_ptexNoTextureCM,                 "%ENGINE%/EngineAssets/Shading/defaultProbe_cm.tif",                FT_DONT_RELEASE | FT_DONT_STREAM                     },
#endif
		};

		for (int t = 0; t < CRY_ARRAY_COUNT(texturesFromFile); ++t)
		{
			if (!texturesFromFile[t].pTexture)
				texturesFromFile[t].pTexture = CTexture::ForName(texturesFromFile[t].szFileName, texturesFromFile[t].flags, eTF_Unknown);
		}

		// Associate dummy NULL-resource with s_pTexNULL
		s_pTexNULL = new CTexture(FT_DONT_RELEASE | FT_DONT_STREAM, Clr_Transparent, CDeviceTexture::Associate(s_ptexBlackAlpha->GetLayout(), nullptr));

		// Default Template textures
		int nRTFlags = FT_DONT_RELEASE | FT_DONT_STREAM | FT_STATE_CLAMP | FT_USAGE_RENDERTARGET;
		s_ptexMipColors_Diffuse = CTexture::GetOrCreateTextureObject("$MipColors_Diffuse", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_MIPCOLORS_DIFFUSE);
		s_ptexMipColors_Bump = CTexture::GetOrCreateTextureObject("$MipColors_Bump", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_MIPCOLORS_BUMP);

		s_ptexRT_2D = CTexture::GetOrCreateTextureObject("$RT_2D", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_RT_2D);

		s_ptexRainOcclusion = CTexture::GetOrCreateTextureObject("$RainOcclusion", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		s_ptexRainSSOcclusion[0] = CTexture::GetOrCreateTextureObject("$RainSSOcclusion0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		s_ptexRainSSOcclusion[1] = CTexture::GetOrCreateTextureObject("$RainSSOcclusion1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

		//s_ptexHitAreaRT[0] = CTexture::CreateTextureObject("$HitEffectAccumBuffRT_0", 128, 128, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		//s_ptexHitAreaRT[1] = CTexture::CreateTextureObject("$HitEffectAccumBuffRT_1", 128, 128, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

		s_ptexFromObj = CTexture::GetOrCreateTextureObject("FromObj", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_FROMOBJ);
		s_ptexSvoTree = CTexture::GetOrCreateTextureObject("SvoTree", 0, 0, 1, eTT_3D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SVOTREE);
		s_ptexSvoTris = CTexture::GetOrCreateTextureObject("SvoTris", 0, 0, 1, eTT_3D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SVOTRIS);
		s_ptexSvoGlobalCM = CTexture::GetOrCreateTextureObject("SvoGlobalCM", 0, 0, 1, eTT_Cube, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SVOGLCM);
		s_ptexSvoRgbs = CTexture::GetOrCreateTextureObject("SvoRgbs", 0, 0, 1, eTT_3D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SVORGBS);
		s_ptexSvoNorm = CTexture::GetOrCreateTextureObject("SvoNorm", 0, 0, 1, eTT_3D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SVONORM);
		s_ptexSvoOpac = CTexture::GetOrCreateTextureObject("SvoOpac", 0, 0, 1, eTT_3D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SVOOPAC);

		s_ptexWindGrid = CTexture::GetOrCreateTextureObject("WindGrid", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_STAGE_UPLOAD, eTF_Unknown, TO_WINDGRID);
		if (!CTexture::IsTextureExist(CTexture::s_ptexWindGrid))
		{
			CTexture::s_ptexWindGrid->Create2DTexture(256, 256, 1, FT_DONT_RELEASE | FT_DONT_STREAM | FT_STAGE_UPLOAD, nullptr, eTF_R16G16F);
		}

		s_ptexZTargetReadBack[0] = CTexture::GetOrCreateTextureObject("$ZTargetReadBack0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexZTargetReadBack[1] = CTexture::GetOrCreateTextureObject("$ZTargetReadBack1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexZTargetReadBack[2] = CTexture::GetOrCreateTextureObject("$ZTargetReadBack2", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexZTargetReadBack[3] = CTexture::GetOrCreateTextureObject("$ZTargetReadBack3", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);

		s_ptexZTargetDownSample[0] = CTexture::GetOrCreateTextureObject("$ZTargetDownSample0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexZTargetDownSample[1] = CTexture::GetOrCreateTextureObject("$ZTargetDownSample1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexZTargetDownSample[2] = CTexture::GetOrCreateTextureObject("$ZTargetDownSample2", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);
		s_ptexZTargetDownSample[3] = CTexture::GetOrCreateTextureObject("$ZTargetDownSample3", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM, eTF_Unknown);

		s_ptexSceneNormalsMapMS = CTexture::GetOrCreateTextureObject("$SceneNormalsMapMS", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SCENE_NORMALMAP_MS);
		s_ptexSceneDiffuseAccMapMS = CTexture::GetOrCreateTextureObject("$SceneDiffuseAccMS", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SCENE_DIFFUSE_ACC_MS);
		s_ptexSceneSpecularAccMapMS = CTexture::GetOrCreateTextureObject("$SceneSpecularAccMS", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SCENE_SPECULAR_ACC_MS);

		s_ptexRT_ShadowPool = CTexture::GetOrCreateTextureObject("$RT_ShadowPool", 0, 0, 1, eTT_2D, FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);
		s_ptexFarPlane = CTexture::GetOrCreateTextureObject("$FarPlane", 8, 8, 1, eTT_2D, FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX
		s_ptexDepthBufferQuarter = CTexture::GetOrCreateTextureObject("$DepthBufferQuarter", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);
#else
		s_ptexDepthBufferQuarter = NULL;
#endif
		s_ptexDepthBufferHalfQuarter = CTexture::GetOrCreateTextureObject("$DepthBufferHalfQuarter", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);

		if (!s_ptexModelHudBuffer)
			s_ptexModelHudBuffer = CTexture::GetOrCreateTextureObject("$ModelHud", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_MODELHUD);

		if (!s_ptexBackBuffer)
		{
			s_ptexSceneTarget = CTexture::GetOrCreateTextureObject("$SceneTarget", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SCENE_TARGET);
			s_ptexCurrSceneTarget = s_ptexSceneTarget;

			s_ptexSceneTargetR11G11B10F[0] = CTexture::GetOrCreateTextureObject("$SceneTargetR11G11B10F_0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexSceneTargetR11G11B10F[1] = CTexture::GetOrCreateTextureObject("$SceneTargetR11G11B10F_1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexSceneTargetScaledR11G11B10F[0] = CTexture::GetOrCreateTextureObject("$SceneTargetScaled0R11G11B10F", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexSceneTargetScaledR11G11B10F[1] = CTexture::GetOrCreateTextureObject("$SceneTargetScaled1R11G11B10F", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexSceneTargetScaledR11G11B10F[2] = CTexture::GetOrCreateTextureObject("$SceneTargetScaled2R11G11B10F", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexSceneTargetScaledR11G11B10F[3] = CTexture::GetOrCreateTextureObject("$SceneTargetScaled3R11G11B10F", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);

			s_ptexVelocity = CTexture::GetOrCreateTextureObject("$Velocity", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexVelocityTiles[0] = CTexture::GetOrCreateTextureObject("$VelocityTilesTmp0", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexVelocityTiles[1] = CTexture::GetOrCreateTextureObject("$VelocityTilesTmp1", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexVelocityTiles[2] = CTexture::GetOrCreateTextureObject("$VelocityTiles", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			s_ptexVelocityObjects[0] = CTexture::GetOrCreateTextureObject("$VelocityObjects", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
			// Only used for VR, but we need to support runtime switching
			s_ptexVelocityObjects[1] = CTexture::GetOrCreateTextureObject("$VelocityObjects_R", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);

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
			s_ptexSceneDiffuseAccMap = CTexture::GetOrCreateTextureObject("$SceneDiffuseAcc", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8, TO_SCENE_DIFFUSE_ACC);
			s_ptexSceneSpecularAccMap = CTexture::GetOrCreateTextureObject("$SceneSpecularAcc", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8, TO_SCENE_SPECULAR_ACC);
			s_ptexShadowMask = CTexture::GetOrCreateTextureObject("$ShadowMask", 0, 0, 1, eTT_2DArray, nRTFlags, eTF_R8, TO_SHADOWMASK);

			s_ptexFlaresGather = CTexture::GetOrCreateTextureObject("$FlaresGather", 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
			for (i = 0; i < MAX_OCCLUSION_READBACK_TEXTURES; i++)
			{
				cry_sprintf(str, "$FlaresOcclusion_%d", i);
				s_ptexFlaresOcclusionRing[i] = CTexture::GetOrCreateTextureObject(str, 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
			}

			// fixme: get texture resolution from CREWaterOcean
			// TODO: make s_ptexWaterVolumeTemp an array texture
			s_ptexWaterOcean = CTexture::GetOrCreateTextureObject("$WaterOceanMap", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_NOMIPS | FT_STAGE_UPLOAD | FT_DONT_STREAM, eTF_Unknown, TO_WATEROCEANMAP);
			s_ptexWaterVolumeTemp[0] = CTexture::GetOrCreateTextureObject("$WaterVolumeTemp_0", 64, 64, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_NOMIPS | FT_STAGE_UPLOAD | FT_DONT_STREAM, eTF_Unknown);
			s_ptexWaterVolumeTemp[1] = CTexture::GetOrCreateTextureObject("$WaterVolumeTemp_1", 64, 64, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_NOMIPS | FT_STAGE_UPLOAD | FT_DONT_STREAM, eTF_Unknown);
			s_ptexWaterVolumeDDN = CTexture::GetOrCreateTextureObject("$WaterVolumeDDN", 64, 64, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, eTF_Unknown, TO_WATERVOLUMEMAP);
			s_ptexWaterVolumeRefl[0] = CTexture::GetOrCreateTextureObject("$WaterVolumeRefl", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, eTF_Unknown, TO_WATERVOLUMEREFLMAP);
			s_ptexWaterVolumeRefl[1] = CTexture::GetOrCreateTextureObject("$WaterVolumeReflPrev", 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, eTF_Unknown, TO_WATERVOLUMEREFLMAPPREV);
			s_ptexWaterCaustics[0] = CTexture::GetOrCreateTextureObject("$WaterVolumeCaustics", 512, 512, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_WATERVOLUMECAUSTICSMAP);
			s_ptexWaterCaustics[1] = CTexture::GetOrCreateTextureObject("$WaterVolumeCausticsTemp", 512, 512, 1, eTT_2D, /*FT_DONT_RELEASE |*/ FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_WATERVOLUMECAUSTICSMAPTEMP);

			s_ptexRainDropsRT[0] = CTexture::GetOrCreateTextureObject("$RainDropsAccumRT_0", 512, 512, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
			s_ptexRainDropsRT[1] = CTexture::GetOrCreateTextureObject("$RainDropsAccumRT_1", 512, 512, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

			if (!s_ptexZTarget)
			{
				//for d3d10 we cannot free it during level transition, therefore allocate once and keep it
				s_ptexZTarget = CTexture::GetOrCreateTextureObject("$ZTarget", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
			}

			s_ptexZTargetScaled = CTexture::GetOrCreateTextureObject("$ZTargetScaled", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_DOWNSCALED_ZTARGET_FOR_AO);
			s_ptexZTargetScaled2 = CTexture::GetOrCreateTextureObject("$ZTargetScaled2", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_QUARTER_ZTARGET_FOR_AO);
			s_ptexZTargetScaled3 = CTexture::GetOrCreateTextureObject("$ZTargetScaled3", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		}

		s_ptexSceneSelectionIDs = CTexture::GetOrCreateTextureObject("$SceneSelectionIDs", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_R32F);
		s_ptexSceneHalfDepthStencil = CTexture::GetOrCreateTextureObject("$SceneHalfDepthStencil", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_D32F);
		
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

		s_ptexSkyDomeMie = CTexture::GetOrCreateTextureObject("$SkyDomeMie", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SKYDOME_MIE);
		s_ptexSkyDomeRayleigh = CTexture::GetOrCreateTextureObject("$SkyDomeRayleigh", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SKYDOME_RAYLEIGH);
		s_ptexSkyDomeMoon = CTexture::GetOrCreateTextureObject("$SkyDomeMoon", 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SKYDOME_MOON);

		for (i = 0; i < EFTT_MAX; i++)
		{
			new(&s_ShaderTemplates[i])CTexture(FT_DONT_RELEASE);
			s_ShaderTemplates[i].SetCustomID(EFTT_DIFFUSE + i);
			s_ShaderTemplates[i].SetFlags(FT_DONT_RELEASE);
		}
		s_ShaderTemplatesInitialized = true;

		s_ptexVolumetricFog = CTexture::GetOrCreateTextureObject("$VolFogInscattering", 0, 0, 0, eTT_3D, FT_NOMIPS | FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_UNORDERED_ACCESS, eTF_Unknown);
		s_ptexVolumetricClipVolumeStencil = CTexture::GetOrCreateTextureObject("$ClipVolumeStencilVolume", 0, 0, 0, eTT_2D, FT_NOMIPS | FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL | FT_USAGE_RENDERTARGET, eTF_Unknown);

		s_ptexVolCloudShadow = CTexture::GetOrCreateTextureObject("$VolCloudShadows", 0, 0, 0, eTT_3D, FT_NOMIPS | FT_USAGE_UNORDERED_ACCESS, eTF_Unknown);

#if defined(DURANGO_USE_ESRAM)
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

		if (gRenDev->m_pTextureManager)
			gRenDev->m_pTextureManager->PreloadDefaultTextures();
	}

}

//////////////////////////////////////////////////////////////////////////
const char* CTexture::GetFormatName() const
{
	return NameForTextureFormat(GetDstFormat());
}

//////////////////////////////////////////////////////////////////////////
const char* CTexture::GetTypeName() const
{
	return NameForTextureType(GetTextureType());
}

//////////////////////////////////////////////////////////////////////////
CDynTextureSource::CDynTextureSource()
	: m_refCount(1)
	, m_width(0)
	, m_height(0)
	, m_lastUpdateTime(0)
	, m_lastUpdateFrameID(0)
	, m_pDynTexture(0)
{
}

CDynTextureSource::~CDynTextureSource()
{
	SAFE_DELETE(m_pDynTexture);
}

void CDynTextureSource::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CDynTextureSource::Release()
{
	long refCnt = CryInterlockedDecrement(&m_refCount);
	if (refCnt <= 0)
		delete this;
}

void CDynTextureSource::CalcSize(uint32& width, uint32& height, float distToCamera) const
{
	uint32 size;
	uint32 logSize;
	switch (CRenderer::CV_r_envtexresolution)
	{
	case 0:
	case 1:
		size = 256;
		logSize = 8;
		break;
	case 2:
	case 3:
	default:
		size = 512;
		logSize = 9;
		break;
	}

	if (distToCamera > 0)
	{
		int x(0), y(0), vpwidth(1), vpheight(1);
		gRenDev->GetViewport(&x, &y, &vpwidth, &vpheight);
		float lod = logf(max(distToCamera * 1024.0f / (float) max(vpwidth, vpheight), 1.0f));
		uint32 lodFixed = fastround_positive(lod);
		size = 1 << max(logSize - lodFixed, 5u);
	}

	width = size;
	height = size;
}

bool CDynTextureSource::Apply(int nTUnit, SamplerStateHandle nTS)
{
	assert(m_pDynTexture);
	if (!m_pDynTexture || !m_pDynTexture->IsValid())
		return false;

	m_pDynTexture->Apply(nTUnit, nTS);
	return true;
}

void CDynTextureSource::GetTexGenInfo(float& offsX, float& offsY, float& scaleX, float& scaleY) const
{
	assert(m_pDynTexture);
	if (!m_pDynTexture || !m_pDynTexture->IsValid())
	{
		offsX = 0;
		offsY = 0;
		scaleX = 1;
		scaleY = 1;
		return;
	}

	ITexture* pSrcTex(m_pDynTexture->GetTexture());
	float invSrcWidth(1.0f / (float) pSrcTex->GetWidth());
	float invSrcHeight(1.0f / (float) pSrcTex->GetHeight());
	offsX = m_pDynTexture->m_nX * invSrcWidth;
	offsY = m_pDynTexture->m_nY * invSrcHeight;
	assert(m_width <= m_pDynTexture->m_nWidth && m_height <= m_pDynTexture->m_nHeight);
	scaleX = m_width * invSrcWidth;
	scaleY = m_height * invSrcHeight;
}

void CDynTextureSource::InitDynTexture(ETexPool eTexPool)
{
	CalcSize(m_width, m_height);
	m_pDynTexture = new SDynTexture2(m_width, m_height, FT_USAGE_RENDERTARGET | FT_STATE_CLAMP | FT_NOMIPS, "DynTextureSource", eTexPool);
}

struct FlashTextureSourceSharedRT_AutoUpdate
{
public:
	static void Add(CFlashTextureSourceBase* pSrc)
	{
		CryAutoCriticalSection lock(ms_lock);
		stl::push_back_unique(ms_sources, pSrc);
	}

	static void AddToLightList(CFlashTextureSourceBase* pSrc)
	{
		if (stl::find(ms_LightTextures, pSrc)) return;
		ms_LightTextures.push_back(pSrc);
	}

	static void Remove(CFlashTextureSourceBase* pSrc)
	{
		CryAutoCriticalSection lock(ms_lock);

		const size_t size = ms_sources.size();
		for (size_t i = 0; i < size; ++i)
		{
			if (ms_sources[i] == pSrc)
			{
				ms_sources[i] = ms_sources[size - 1];
				ms_sources.pop_back();
				if (ms_sources.empty())
				{
					stl::reconstruct(ms_sources);
				}
				break;
			}
		}
		stl::find_and_erase(ms_LightTextures, pSrc);
	}

	static void Tick()
	{
		SRenderThread* pRT = gRenDev->m_pRT;
		if (!pRT || (pRT->IsMainThread() && pRT->m_eVideoThreadMode == SRenderThread::eVTM_Disabled))
		{
			CTimeValue curTime = gEnv->pTimer->GetAsyncTime();
			const int frameID = gRenDev->GetFrameID(false);
			if (ms_lastTickFrameID != frameID)
			{
				CryAutoCriticalSection lock(ms_lock);

				const float deltaTime = gEnv->pTimer->GetFrameTime();
				const bool isEditing = gEnv->IsEditing();
				const bool isPaused = gEnv->pSystem->IsPaused();

				const size_t size = ms_sources.size();
				for (size_t i = 0; i < size; ++i)
				{
					ms_sources[i]->AutoUpdate(curTime, deltaTime, isEditing, isPaused);
				}

				ms_lastTickFrameID = frameID;
			}
		}
	}

	static void TickRT()
	{
		SRenderThread* pRT = gRenDev->m_pRT;
		if (!pRT || (pRT->IsRenderThread() && pRT->m_eVideoThreadMode == SRenderThread::eVTM_Disabled))
		{
			const int frameID = gRenDev->GetFrameID(false);
			if (ms_lastTickRTFrameID != frameID)
			{
				CryAutoCriticalSection lock(ms_lock);

				const size_t size = ms_sources.size();
				for (size_t i = 0; i < size; ++i)
				{
					ms_sources[i]->AutoUpdateRT(frameID);
				}

				ms_lastTickRTFrameID = frameID;
			}
		}
	}

	static void RenderLightTextures()
	{
		for (std::vector<CFlashTextureSourceBase*>::const_iterator it = ms_LightTextures.begin(); it != ms_LightTextures.end(); ++it)
			(*it)->Update();
		ms_LightTextures.clear();
	}

private:
	static std::vector<CFlashTextureSourceBase*> ms_sources;
	static std::vector<CFlashTextureSourceBase*> ms_LightTextures;
	static CryCriticalSection                    ms_lock;
	static int ms_lastTickFrameID;
	static int ms_lastTickRTFrameID;
};

void CFlashTextureSourceBase::Tick()
{
	FlashTextureSourceSharedRT_AutoUpdate::Tick();
}

void CFlashTextureSourceBase::TickRT()
{
	FlashTextureSourceSharedRT_AutoUpdate::TickRT();
}

void CFlashTextureSourceBase::RenderLights()
{
	FlashTextureSourceSharedRT_AutoUpdate::RenderLightTextures();
}

void CFlashTextureSourceBase::AddToLightRenderList(const IDynTextureSource* pSrc)
{
	FlashTextureSourceSharedRT_AutoUpdate::AddToLightList((CFlashTextureSourceBase*)pSrc);
}

void CFlashTextureSourceBase::AutoUpdate(const CTimeValue& curTime, const float delta, const bool isEditing, const bool isPaused)
{
	if (m_autoUpdate)
	{
		m_pFlashPlayer->UpdatePlayer(this);
#if CRY_PLATFORM_WINDOWS
		m_perFrameRendering &= !isEditing;
#endif

		if (m_perFrameRendering || (curTime - m_lastVisible).GetSeconds() < 1.0f)
		{
			Advance(delta, isPaused);
		}
	}
}

void CFlashTextureSourceBase::AutoUpdateRT(const int frameID)
{
	if (m_autoUpdate)
	{
		if (m_perFrameRendering && (frameID != m_lastVisibleFrameID))
		{
			Update();
		}
	}
}

std::vector<CFlashTextureSourceBase*> FlashTextureSourceSharedRT_AutoUpdate::ms_sources;
std::vector<CFlashTextureSourceBase*> FlashTextureSourceSharedRT_AutoUpdate::ms_LightTextures;
CryCriticalSection FlashTextureSourceSharedRT_AutoUpdate::ms_lock;
int FlashTextureSourceSharedRT_AutoUpdate::ms_lastTickFrameID = 0;
int FlashTextureSourceSharedRT_AutoUpdate::ms_lastTickRTFrameID = 0;

CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::~CFlashPlayerInstanceWrapper()
{
	SAFE_RELEASE(m_pBootStrapper);
	SAFE_RELEASE(m_pPlayer);
}

IFlashPlayer* CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::GetTempPtr() const
{
	CryAutoCriticalSection lock(m_lock);

	if (m_pPlayer)
		m_pPlayer->AddRef();

	return m_pPlayer;
}

IFlashPlayer* CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::GetPermPtr(CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	m_canDeactivate = false;
	CreateInstance(pSrc);

	return m_pPlayer;
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::Activate(bool activate, CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	if (activate)
	{
		CreateInstance(pSrc);
	}
	else
	{
		if (m_canDeactivate)
		{
			SAFE_RELEASE(m_pPlayer);
		}
	}
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::CreateInstance(CFlashTextureSourceBase* pSrc)
{
	if (!m_pPlayer && m_pBootStrapper)
	{
		m_pPlayer = m_pBootStrapper->CreatePlayerInstance(IFlashPlayer::DEFAULT_NO_MOUSE);
		if (m_pPlayer)
		{
#if defined(ENABLE_DYNTEXSRC_PROFILING)
			m_pPlayer->LinkDynTextureSource(pSrc);
#endif
			m_pPlayer->Advance(0.0f);
			m_width = m_pPlayer->GetWidth();
			m_height = m_pPlayer->GetHeight();
		}
	}
}

const char* CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::GetSourceFilePath() const
{
	return m_pBootStrapper ? m_pBootStrapper->GetFilePath() : "UNDEFINED";
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::Advance(float delta)
{
	if (m_pPlayer)
		m_pPlayer->Advance(delta);
}

CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::~CFlashPlayerInstanceWrapperLayoutElement()
{
	if (m_pUILayout)
	{
		m_pUILayout->Unload();
	}
	m_pUILayout = NULL;
	m_pPlayer = NULL;
}

IFlashPlayer* CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::GetTempPtr() const
{
	CryAutoCriticalSection lock(m_lock);

	if (m_pPlayer)
		m_pPlayer->AddRef();

	return m_pPlayer;
}

IFlashPlayer* CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::GetPermPtr(CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	m_canDeactivate = false;
	CreateInstance(pSrc, m_layoutName.c_str());

	return m_pPlayer;
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::Activate(bool activate, CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	if (activate)
	{
		CreateInstance(pSrc, m_layoutName.c_str());
	}
	else
	{
		if (m_canDeactivate)
		{
			SAFE_RELEASE(m_pPlayer);
		}
	}
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::CreateInstance(CFlashTextureSourceBase* pSrc, const char* layoutName)
{
	UIFramework::IUIFramework* pUIFramework = gEnv->pUIFramework;
	if (pUIFramework)
	{
		char name[_MAX_PATH];
		cry_strcpy(name, layoutName);
		PathUtil::RemoveExtension(name);
		const char* pExt = fpGetExtension(layoutName);
		if (!pExt || strcmpi(pExt, ".layout") != 0)
		{
			return;
		}

		if (pUIFramework->LoadLayout(name) != INVALID_LAYOUT_ID)
		{
			m_layoutName = name;
			m_pUILayout = pUIFramework->GetLayoutBase(m_layoutName);
			if (m_pUILayout)
			{
				m_pPlayer = m_pUILayout->GetPlayer();

				if (m_pPlayer)
				{
					pSrc->m_autoUpdate = !m_pPlayer->HasMetadata("CE_NoAutoUpdate");

#if defined(ENABLE_DYNTEXSRC_PROFILING)
					m_pPlayer->LinkDynTextureSource(pSrc);
#endif
					m_pPlayer->Advance(0.0f);
					m_width = m_pPlayer->GetWidth();
					m_height = m_pPlayer->GetHeight();
				}
			}
		}
	}
}

const char* CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::GetSourceFilePath() const
{
	return m_pPlayer ? m_pPlayer->GetFilePath() : "UNDEFINED";
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::Advance(float delta)
{
	if (m_pPlayer)
		m_pPlayer->Advance(delta);
}

CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::~CFlashPlayerInstanceWrapperUIElement()
{
	SAFE_RELEASE(m_pUIElement);
	SAFE_RELEASE(m_pPlayer);
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::SetUIElement(IUIElement* p)
{
	assert(!m_pUIElement);
	m_pUIElement = p;
	m_pUIElement->AddRef();
}

IFlashPlayer* CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::GetTempPtr() const
{
	CryAutoCriticalSection lock(m_lock);

	if (m_pPlayer)
		m_pPlayer->AddRef();

	return m_pPlayer;
}

IFlashPlayer* CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::GetPermPtr(CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	m_canDeactivate = false;
	m_activated = true;
	UpdateUIElementPlayer(pSrc);

	return m_pPlayer;
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::Activate(bool activate, CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	m_activated = m_canDeactivate ? activate : m_activated;
	UpdateUIElementPlayer(pSrc);
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::Clear(CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	if (m_pUIElement && m_pPlayer)
		m_pUIElement->RemoveTexture(pSrc);

	SAFE_RELEASE(m_pPlayer);
}

const char* CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::GetSourceFilePath() const
{
	return m_pUIElement ? m_pUIElement->GetFlashFile() : "UNDEFINED";
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::UpdatePlayer(CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	UpdateUIElementPlayer(pSrc);
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::UpdateUIElementPlayer(CFlashTextureSourceBase* pSrc)
{
	if (m_pUIElement)
	{
		if (m_activated)
		{
			const bool isVisible = m_pUIElement->IsVisible();
			IFlashPlayer* pPlayer = isVisible ? m_pUIElement->GetFlashPlayer() : NULL;
			if (pPlayer != m_pPlayer)
			{
				assert(gRenDev->m_pRT->IsMainThread());

				const bool addTex = m_pPlayer == NULL;
				SAFE_RELEASE(m_pPlayer);
				if (isVisible)
					m_pPlayer = pPlayer;

				if (m_pPlayer)
				{
					m_width = m_pPlayer->GetWidth();
					m_height = m_pPlayer->GetHeight();
					m_pPlayer->AddRef();
					if (addTex)
						m_pUIElement->AddTexture(pSrc);
				}
				else
					m_pUIElement->RemoveTexture(pSrc);
			}
		}
		else
		{
			assert(gRenDev->m_pRT->IsMainThread());

			if (m_pPlayer)
				m_pUIElement->RemoveTexture(pSrc);
			SAFE_RELEASE(m_pPlayer);
		}
	}
}

CFlashTextureSourceBase::CFlashTextureSourceBase(const char* pFlashFileName, const IRenderer::SLoadShaderItemArgs* pArgs)
	: m_refCount(1)
	, m_lastVisible()
	, m_lastVisibleFrameID(0)
	, m_width(16)
	, m_height(16)
	, m_aspectRatio(1.0f)
	, m_pElement(NULL)
	, m_autoUpdate(true)
	, m_pFlashFileName(pFlashFileName)
	, m_perFrameRendering(false)
	, m_pFlashPlayer(CFlashPlayerInstanceWrapperNULL::Get())
	//, m_texStateIDs
#if defined(ENABLE_DYNTEXSRC_PROFILING)
	, m_pMatSrc(pArgs ? pArgs->m_pMtlSrc : 0)
	, m_pMatSrcParent(pArgs ? pArgs->m_pMtlSrcParent : 0)
#endif
{
	bool valid = false;

	if (pFlashFileName)
	{
		if (IsFlashUIFile(pFlashFileName))
		{
			valid = CreateTexFromFlashFile(pFlashFileName);
			if (valid)
			{
				gEnv->pFlashUI->RegisterModule(this, pFlashFileName);
			}
		}
		else if (IsFlashUILayoutFile(pFlashFileName))
		{
			if (m_pFlashPlayer)
				m_pFlashPlayer->Clear(this);

			SAFE_RELEASE(m_pFlashPlayer);

			CFlashPlayerInstanceWrapperLayoutElement* pInstanceWrapper = new CFlashPlayerInstanceWrapperLayoutElement();
			pInstanceWrapper->CreateInstance(this, pFlashFileName);
			m_pFlashPlayer = pInstanceWrapper;
			valid = true;
		}
		else
		{
			IFlashPlayerBootStrapper* pBootStrapper = gEnv->pScaleformHelper ? gEnv->pScaleformHelper->CreateFlashPlayerBootStrapper() : nullptr;
			if (pBootStrapper)
			{
				if (pBootStrapper->Load(pFlashFileName))
				{
					m_autoUpdate = !pBootStrapper->HasMetadata("CE_NoAutoUpdate");

					CFlashPlayerInstanceWrapper* pInstanceWrapper = new CFlashPlayerInstanceWrapper();
					pInstanceWrapper->SetBootStrapper(pBootStrapper);
					pInstanceWrapper->CreateInstance(this);
					m_pFlashPlayer = pInstanceWrapper;
					valid = true;
				}
				else
				{
					SAFE_RELEASE(pBootStrapper);
				}
			}
		}
	}

	for (size_t i = 0; i < NumCachedTexStateIDs; ++i)
	{
		m_texStateIDs[i].original = EDefaultSamplerStates::Unspecified;
		m_texStateIDs[i].patched = EDefaultSamplerStates::Unspecified;
	}

	if (valid && m_autoUpdate)
		FlashTextureSourceSharedRT_AutoUpdate::Add(this);
}

bool CFlashTextureSourceBase::IsFlashFile(const char* pFlashFileName)
{
	if (pFlashFileName)
	{
		const char* pExt = fpGetExtension(pFlashFileName);
		const bool bPath = strchr(pFlashFileName, '/') || strchr(pFlashFileName, '\\');

		if (pExt)
		{
			if (!bPath)
			{
				// Pseudo files (no path, looks up flow-node)
				return (!stricmp(pExt, ".ui"));
			}

			// Real files (looks up filesystem)
			return (!stricmp(pExt, ".layout") || !stricmp(pExt, ".gfx") || !stricmp(pExt, ".swf") || !stricmp(pExt, ".usm"));
		}
	}

	return false;
}

bool CFlashTextureSourceBase::IsFlashUIFile(const char* pFlashFileName)
{
	if (pFlashFileName)
	{
		const char* pExt = fpGetExtension(pFlashFileName);
		const bool bPath = strchr(pFlashFileName, '/') || strchr(pFlashFileName, '\\');

		if (pExt)
		{
			if (!bPath)
			{
				// Pseudo files (no path, looks up flow-node)
				return !stricmp(pExt, ".ui");
			}
		}
	}

	return false;
}

bool CFlashTextureSourceBase::IsFlashUILayoutFile(const char* pFlashFileName)
{
	if (pFlashFileName)
	{
		const char* pExt = fpGetExtension(pFlashFileName);

		if (pExt)
		{
			return !stricmp(pExt, ".layout");
		}
	}

	return false;
}

bool CFlashTextureSourceBase::DestroyTexOfFlashFile(const char* name)
{
	if (gEnv->pFlashUI)
	{
		IUIElement* pElement = gEnv->pFlashUI->GetUIElementByInstanceStr(name);
		if (pElement)
		{
			pElement->Unload();
			pElement->DestroyThis();

		}
	}

	return false;
}

bool CFlashTextureSourceBase::CreateTexFromFlashFile(const char* name)
{
	if (gEnv->pFlashUI)
	{
		//delete old one
		if (m_pFlashPlayer)
			m_pFlashPlayer->Clear(this);
		SAFE_RELEASE(m_pFlashPlayer);

		m_pElement = gEnv->pFlashUI->GetUIElementByInstanceStr(name);
		if (m_pElement)
		{
			CFlashPlayerInstanceWrapperUIElement* pInstanceWrapper = new CFlashPlayerInstanceWrapperUIElement();
			m_pElement->SetVisible(true);
			pInstanceWrapper->SetUIElement(m_pElement);
			pInstanceWrapper->Activate(true, this);
			m_pFlashPlayer = pInstanceWrapper;
			return true;
		}
	}

	return false;
}

CFlashTextureSourceBase::~CFlashTextureSourceBase()
{
	FlashTextureSourceSharedRT_AutoUpdate::Remove(this);

	if (m_pElement) // If module registration happened m_pElement is set
	{
		gEnv->pFlashUI->UnregisterModule(this);
	}

	if (m_pFlashPlayer)
		m_pFlashPlayer->Clear(this);

	SAFE_RELEASE(m_pFlashPlayer);
}

void CFlashTextureSourceBase::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void CFlashTextureSourceBase::Release()
{
	long refCnt = CryInterlockedDecrement(&m_refCount);
	if (refCnt <= 0)
		delete this;
}

void CFlashTextureSourceBase::Activate(bool activate)
{
	m_pFlashPlayer->Activate(activate, this);
}

#if defined(ENABLE_DYNTEXSRC_PROFILING)
string CFlashTextureSourceBase::GetProfileInfo() const
{

	const char* pMtlName = "NULL";
	if (m_pMatSrc || m_pMatSrcParent)
	{
		if (m_pMatSrcParent)
		{
			if (m_pMatSrcParent->GetName())
				pMtlName = m_pMatSrcParent->GetName();
		}
		else if (m_pMatSrc)
		{
			if (m_pMatSrc->GetName())
				pMtlName = m_pMatSrc->GetName();
		}
	}

	const char* pSubMtlName = "NULL";
	if (m_pMatSrcParent)
	{
		if (m_pMatSrc)
		{
			if (m_pMatSrc->GetName())
				pSubMtlName = m_pMatSrc->GetName();
		}
	}
	else
	{
		pSubMtlName = 0;
	}

	CryFixedStringT<128> info;
	info.Format("mat: %s%s%s%s", pMtlName, pSubMtlName ? "|sub: " : "", pSubMtlName ? pSubMtlName : "", !m_pFlashPlayer->CanDeactivate() ? "|$4can't be deactivated!$O" : "");

	return info.c_str();
}
#endif

void* CFlashTextureSourceBase::GetSourceTemp(EDynTextureSource type) const
{
	if (m_pFlashPlayer != nullptr && type == DTS_I_FLASHPLAYER)
	{
		return m_pFlashPlayer->GetTempPtr();
	}
	return nullptr;
}

void* CFlashTextureSourceBase::GetSourcePerm(EDynTextureSource type)
{
	if (m_pFlashPlayer != nullptr && type == DTS_I_FLASHPLAYER)
	{
		return m_pFlashPlayer->GetPermPtr(this);
	}
	return nullptr;
}

const char* CFlashTextureSourceBase::GetSourceFilePath() const
{
	if (m_pFlashPlayer != nullptr)
	{
		return m_pFlashPlayer->GetSourceFilePath();
	}
	return nullptr;
}

bool CFlashTextureSourceBase::Apply(int nTUnit, SamplerStateHandle nTS)
{
	SDynTexture* pDynTexture = GetDynTexture();
	assert(pDynTexture);
	if (!m_pFlashPlayer || !m_pFlashPlayer->CheckPtr() || !pDynTexture)
		return false;

	SamplerStateHandle patchedTexStateID = EDefaultSamplerStates::Unspecified;

	size_t i = 0;
	for (; i < NumCachedTexStateIDs; ++i)
	{
		if (m_texStateIDs[i].original == EDefaultSamplerStates::Unspecified)
			break;

		if (m_texStateIDs[i].original == nTS)
		{
			patchedTexStateID = m_texStateIDs[i].patched;
			assert(patchedTexStateID != EDefaultSamplerStates::Unspecified);
			break;
		}
	}

	if (patchedTexStateID == EDefaultSamplerStates::Unspecified)
	{
		SSamplerState State = CDeviceObjectFactory::LookupSamplerState(nTS).first;
		State.m_bSRGBLookup = true;
		patchedTexStateID = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(State);

		if (i < NumCachedTexStateIDs)
		{
			m_texStateIDs[i].original = nTS;
			m_texStateIDs[i].patched = patchedTexStateID;
		}
	}

	pDynTexture->Apply(nTUnit, patchedTexStateID);
	return true;
}

void CFlashTextureSourceBase::GetTexGenInfo(float& offsX, float& offsY, float& scaleX, float& scaleY) const
{
	SDynTexture* pDynTexture = GetDynTexture();
	assert(pDynTexture);
	if (!pDynTexture || !pDynTexture->IsValid())
	{
		offsX = 0;
		offsY = 0;
		scaleX = 1;
		scaleY = 1;
		return;
	}

	ITexture* pSrcTex = pDynTexture->GetTexture();
	float invSrcWidth = 1.0f / (float) pSrcTex->GetWidth();
	float invSrcHeight = 1.0f / (float) pSrcTex->GetHeight();
	offsX = 0;
	offsY = 0;
	assert(m_width <= pDynTexture->m_nWidth && m_height <= pDynTexture->m_nHeight);
	scaleX = m_width * invSrcWidth;
	scaleY = m_height * invSrcHeight;
}

//////////////////////////////////////////////////////////////////////////
CFlashTextureSource::CFlashTextureSource(const char* pFlashFileName, const IRenderer::SLoadShaderItemArgs* pArgs)
	: CFlashTextureSourceBase(pFlashFileName, pArgs)
{
	// create render-target with mip-maps
	m_pDynTexture = new SDynTexture(GetWidth(), GetHeight(), eTF_R8G8B8A8, eTT_2D, FT_USAGE_RENDERTARGET | FT_STATE_CLAMP | FT_FORCE_MIPS | FT_USAGE_ALLOWREADSRGB, "FlashTextureSourceUniqueRT");
	m_pMipMapper = nullptr;
}

CFlashTextureSource::~CFlashTextureSource()
{
	SAFE_DELETE(m_pMipMapper);
	SAFE_DELETE(m_pDynTexture);
}

int CFlashTextureSource::GetWidth() const
{
	IFlashPlayerInstanceWrapper* pFlash = GetFlashPlayerInstanceWrapper();
	return pFlash ? max(Align8(pFlash->GetWidth()), 16) : 16;
}

int CFlashTextureSource::GetHeight() const
{
	IFlashPlayerInstanceWrapper* pFlash = GetFlashPlayerInstanceWrapper();
	return pFlash ? max(Align8(pFlash->GetHeight()), 16) : 16;
}

bool CFlashTextureSource::UpdateDynTex(int rtWidth, int rtHeight)
{
	bool needResize = rtWidth != m_pDynTexture->m_nWidth || rtHeight != m_pDynTexture->m_nHeight || !m_pDynTexture->IsValid();
	if (needResize)
	{
		if (!m_pDynTexture->Update(rtWidth, rtHeight))
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
SDynTexture* CFlashTextureSourceSharedRT::ms_pDynTexture = nullptr;
CMipmapGenPass* CFlashTextureSourceSharedRT::ms_pMipMapper = nullptr;
int CFlashTextureSourceSharedRT::ms_instCount = 0;
int CFlashTextureSourceSharedRT::ms_sharedRTWidth = 256;
int CFlashTextureSourceSharedRT::ms_sharedRTHeight = 256;

CFlashTextureSourceSharedRT::CFlashTextureSourceSharedRT(const char* pFlashFileName, const IRenderer::SLoadShaderItemArgs* pArgs)
	: CFlashTextureSourceBase(pFlashFileName, pArgs)
{
	++ms_instCount;
	if (!ms_pDynTexture)
	{
		// create render-target with mip-maps
		ms_pDynTexture = new SDynTexture(ms_sharedRTWidth, ms_sharedRTHeight, eTF_R8G8B8A8, eTT_2D, FT_USAGE_RENDERTARGET | FT_STATE_CLAMP | FT_FORCE_MIPS | FT_USAGE_ALLOWREADSRGB, "FlashTextureSourceSharedRT");
		ms_pMipMapper = nullptr;
	}
}

CFlashTextureSourceSharedRT::~CFlashTextureSourceSharedRT()
{
	--ms_instCount;
	if (ms_instCount <= 0)
	{
		SAFE_DELETE(ms_pMipMapper);
		SAFE_DELETE(ms_pDynTexture);
	}
}

int CFlashTextureSourceSharedRT::GetSharedRTWidth()
{
	return CRenderer::CV_r_DynTexSourceSharedRTWidth > 0 ? Align8(max(CRenderer::CV_r_DynTexSourceSharedRTWidth, 16)) : ms_sharedRTWidth;
}

int CFlashTextureSourceSharedRT::GetSharedRTHeight()
{
	return CRenderer::CV_r_DynTexSourceSharedRTHeight > 0 ? Align8(max(CRenderer::CV_r_DynTexSourceSharedRTHeight, 16)) : ms_sharedRTHeight;
}

int CFlashTextureSourceSharedRT::NearestPowerOfTwo(int n)
{
	int k = n;
	k--;
	k |= k >> 1;
	k |= k >> 2;
	k |= k >> 4;
	k |= k >> 8;
	k |= k >> 16;
	k++;
	return (k - n) <= (n - (k >> 1)) ? k : (k >> 1);
}

void CFlashTextureSourceSharedRT::SetSharedRTDim(int width, int height)
{
	ms_sharedRTWidth = NearestPowerOfTwo(width > 16 ? width : 16);
	ms_sharedRTHeight = NearestPowerOfTwo(height > 16 ? height : 16);
}

void CFlashTextureSourceSharedRT::SetupSharedRenderTargetRT()
{
	if (ms_pDynTexture)
	{
		const int rtWidth = GetSharedRTWidth();
		const int rtHeight = GetSharedRTHeight();
		bool needResize = rtWidth != ms_pDynTexture->m_nWidth || rtHeight != ms_pDynTexture->m_nHeight;
		if (!ms_pDynTexture->IsValid() || needResize)
		{
			ms_pDynTexture->Update(rtWidth, rtHeight);
			ms_pDynTexture->SetUpdateMask();

			CTexture* pTex = (CTexture*) ms_pDynTexture->GetTexture();
			// prevent leak, this code is only needed on D3D11 to force texture creating
			if (pTex)
				pTex->GetSurface(-1, 0);
			ProbeDepthStencilSurfaceCreation(rtWidth, rtHeight);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CDynTextureSourceLayerActivator::PerLayerDynSrcMtls CDynTextureSourceLayerActivator::s_perLayerDynSrcMtls;

void CDynTextureSourceLayerActivator::LoadLevelInfo()
{
	ReleaseData();

	const char* pLevelPath = iSystem->GetI3DEngine()->GetLevelFilePath("dyntexsrclayeract.xml");

	XmlNodeRef root = iSystem->LoadXmlFromFile(pLevelPath);
	if (root)
	{
		const int numLayers = root->getChildCount();
		for (int curLayer = 0; curLayer < numLayers; ++curLayer)
		{
			XmlNodeRef layer = root->getChild(curLayer);
			if (layer)
			{
				const char* pLayerName = layer->getAttr("Name");
				if (pLayerName)
				{
					const int numMtls = layer->getChildCount();

					std::vector<string> mtls;
					mtls.reserve(numMtls);

					for (int curMtl = 0; curMtl < numMtls; ++curMtl)
					{
						XmlNodeRef mtl = layer->getChild(curMtl);
						if (mtl)
						{
							const char* pMtlName = mtl->getAttr("Name");
							if (pMtlName)
								mtls.push_back(pMtlName);
						}
					}

					s_perLayerDynSrcMtls.insert(PerLayerDynSrcMtls::value_type(pLayerName, mtls));
				}
			}
		}
	}
}

void CDynTextureSourceLayerActivator::ReleaseData()
{
	stl::reconstruct(s_perLayerDynSrcMtls);
}

void CDynTextureSourceLayerActivator::ActivateLayer(const char* pLayerName, bool activate)
{
	PerLayerDynSrcMtls::const_iterator it = pLayerName ? s_perLayerDynSrcMtls.find(pLayerName) : s_perLayerDynSrcMtls.end();
	if (it != s_perLayerDynSrcMtls.end())
	{
		const std::vector<string>& mtls = (*it).second;
		const size_t numMtls = mtls.size();

		const IMaterialManager* pMatMan = gEnv->p3DEngine->GetMaterialManager();

		for (size_t i = 0; i < numMtls; ++i)
		{
			const char* pMtlName = mtls[i].c_str();
			IMaterial* pMtl = pMatMan->FindMaterial(pMtlName);
			if (pMtl)
			{
				pMtl->ActivateDynamicTextureSources(activate);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::EF_AddRTStat(CTexture* pTex, int nFlags, int nW, int nH)
{
	SRTargetStat TS;
	int nSize;
	ETEX_Format eTF;
	if (!pTex)
	{
		eTF = eTF_R8G8B8A8;
		if (nW < 0)
			nW = m_width;
		if (nH < 0)
			nH = m_height;
		nSize = CTexture::TextureDataSize(nW, nH, 1, 1, 1, eTF);
		TS.m_Name = "Back buffer";
	}
	else
	{
		eTF = pTex->GetDstFormat();
		if (nW < 0)
			nW = pTex->GetWidth();
		if (nH < 0)
			nH = pTex->GetHeight();
		nSize = CTexture::TextureDataSize(nW, nH, 1, pTex->GetNumMips(), 1, eTF);
		const char* szName = pTex->GetName();
		if (szName && szName[0] == '$')
			TS.m_Name = string("@") + string(&szName[1]);
		else
			TS.m_Name = szName;
	}
	TS.m_eTF = eTF;

	if (nFlags > 0)
	{
		if (nFlags == 1)
			TS.m_Name += " (Target)";
		else if (nFlags == 2)
		{
			TS.m_Name += " (Depth)";
			nSize = nW * nH * 3;
		}
		else if (nFlags == 4)
		{
			TS.m_Name += " (Stencil)";
			nSize = nW * nH;
		}
		else if (nFlags == 3)
		{
			TS.m_Name += " (Target + Depth)";
			nSize += nW * nH * 3;
		}
		else if (nFlags == 6)
		{
			TS.m_Name += " (Depth + Stencil)";
			nSize = nW * nH * 4;
		}
		else if (nFlags == 5)
		{
			TS.m_Name += " (Target + Stencil)";
			nSize += nW * nH;
		}
		else if (nFlags == 7)
		{
			TS.m_Name += " (Target + Depth + Stencil)";
			nSize += nW * nH * 4;
		}
		else
		{
			assert(0);
		}
	}
	TS.m_nSize = nSize;
	TS.m_nWidth = nW;
	TS.m_nHeight = nH;

	m_RP.m_RTStats.push_back(TS);
}

void CRenderer::EF_PrintRTStats(const char* szName)
{
	const int nYstep = 14;
	int nY = 30; // initial Y pos
	int nX = 20; // initial X pos
	ColorF col = Col_Green;
	IRenderAuxText::Draw2dLabel((float)nX, (float)nY, 1.6f, &col.r, false, "%s", szName);
	nX += 10;
	nY += 25;

	col = Col_White;
	int nYstart = nY;
	int nSize = 0;
	for (int i = 0; i < m_RP.m_RTStats.size(); i++)
	{
		SRTargetStat* pRT = &m_RP.m_RTStats[i];

		IRenderAuxText::Draw2dLabel((float)nX, (float)nY, 1.4f, &col.r, false, "%s (%d x %d x %s), Size: %.3f Mb", pRT->m_Name.c_str(), pRT->m_nWidth, pRT->m_nHeight, CTexture::NameForTextureFormat(pRT->m_eTF), (float)pRT->m_nSize / 1024.0f / 1024.0f);
		nY += nYstep;
		if (nY >= m_height - 25)
		{
			nY = nYstart;
			nX += 500;
		}
		nSize += pRT->m_nSize;
	}
	col = Col_Yellow;
	IRenderAuxText::Draw2dLabel((float)nX, (float)(nY + 10), 1.4f, &col.r, false, "Total: %d RT's, Size: %.3f Mb", m_RP.m_RTStats.size(), nSize / 1024.0f / 1024.0f);
}

STexPool::~STexPool()
{
	bool poolEmpty = true;
	STexPoolItemHdr* pITH = m_ItemsList.m_Next;
	while (pITH != &m_ItemsList)
	{
		STexPoolItemHdr* pNext = pITH->m_Next;
		STexPoolItem* pIT = static_cast<STexPoolItem*>(pITH);

		assert(pIT->m_pOwner == this);
#ifndef _RELEASE
		CryLogAlways("***** Texture %p (%s) still in pool %p! Memory leak and crash will follow *****\n", pIT->m_pTex, pIT->m_pTex ? pIT->m_pTex->GetName() : "NULL", this);
		poolEmpty = false;
#else
		if (pIT->m_pTex)
		{
			pIT->m_pTex->ReleaseDeviceTexture(true); // Try to recover in release
		}
#endif
		*const_cast<STexPool**>(&pIT->m_pOwner) = NULL;
		pITH = pNext;
	}
	CRY_ASSERT_MESSAGE(poolEmpty, "Texture pool was not empty on shutdown");
}

const ETEX_Type CTexture::GetTextureType() const
{
	return m_eTT;
}

const int CTexture::GetTextureID() const
{
	return GetID();
}

#ifdef TEXTURE_GET_SYSTEM_COPY_SUPPORT

const ColorB* CTexture::GetLowResSystemCopy(uint16& nWidth, uint16& nHeight, int** ppLowResSystemCopyAtlasId)
{
	const LowResSystemCopyType::iterator& it = s_LowResSystemCopy.find(this);
	if (it != CTexture::s_LowResSystemCopy.end())
	{
		nWidth = (*it).second.m_nLowResCopyWidth;
		nHeight = (*it).second.m_nLowResCopyHeight;
		*ppLowResSystemCopyAtlasId = &(*it).second.m_nLowResSystemCopyAtlasId;
		return (*it).second.m_lowResSystemCopy.GetElements();
	}

	return NULL;
}

/*#ifndef eTF_BC3
   #define eTF_BC1 eTF_DXT1
   #define eTF_BC2 eTF_DXT3
   #define eTF_BC3 eTF_DXT5
 #endif*/

void CTexture::PrepareLowResSystemCopy(byte* pTexData, bool bTexDataHasAllMips)
{
	if (m_eTT != eTT_2D || (m_nMips <= 1 && (m_nWidth > 16 || m_nHeight > 16)))
		return;

	// this function handles only compressed textures for now
	if (m_eDstFormat != eTF_BC3 && m_eDstFormat != eTF_BC1 && m_eDstFormat != eTF_BC2 && m_eDstFormat != eTF_BC7)
		return;

	// make sure we skip non diffuse textures
	if (strstr(GetName(), "_ddn")
	    || strstr(GetName(), "_ddna")
	    || strstr(GetName(), "_mask")
	    || strstr(GetName(), "_spec.")
	    || strstr(GetName(), "_gloss")
	    || strstr(GetName(), "_displ")
	    || strstr(GetName(), "characters")
	    || strstr(GetName(), "$")
	    )
		return;

	if (pTexData)
	{
		SLowResSystemCopy& rSysCopy = s_LowResSystemCopy[this];

		rSysCopy.m_nLowResCopyWidth = m_nWidth;
		rSysCopy.m_nLowResCopyHeight = m_nHeight;

		int nSrcOffset = 0;
		int nMipId = 0;

		while ((rSysCopy.m_nLowResCopyWidth > 16 || rSysCopy.m_nLowResCopyHeight > 16 || nMipId < 2) && (rSysCopy.m_nLowResCopyWidth >= 8 && rSysCopy.m_nLowResCopyHeight >= 8))
		{
			nSrcOffset += TextureDataSize(rSysCopy.m_nLowResCopyWidth, rSysCopy.m_nLowResCopyHeight, 1, 1, 1, m_eDstFormat);
			rSysCopy.m_nLowResCopyWidth /= 2;
			rSysCopy.m_nLowResCopyHeight /= 2;
			nMipId++;
		}

		int nSizeDxtMip = TextureDataSize(rSysCopy.m_nLowResCopyWidth, rSysCopy.m_nLowResCopyHeight, 1, 1, 1, m_eDstFormat);
		int nSizeRgbaMip = TextureDataSize(rSysCopy.m_nLowResCopyWidth, rSysCopy.m_nLowResCopyHeight, 1, 1, 1, eTF_R8G8B8A8);

		rSysCopy.m_lowResSystemCopy.CheckAllocated(nSizeRgbaMip / sizeof(ColorB));

		gRenDev->DXTDecompress(pTexData + (bTexDataHasAllMips ? nSrcOffset : 0), nSizeDxtMip,
		                       (byte*)rSysCopy.m_lowResSystemCopy.GetElements(), rSysCopy.m_nLowResCopyWidth, rSysCopy.m_nLowResCopyHeight, 1, m_eDstFormat, false, 4);
	}
}

#endif // TEXTURE_GET_SYSTEM_COPY_SUPPORT
