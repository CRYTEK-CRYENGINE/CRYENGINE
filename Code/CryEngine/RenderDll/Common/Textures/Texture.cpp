// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

int CTexture::s_nStreamingMode;
int CTexture::s_nStreamingUpdateMode;
bool CTexture::s_bPrecachePhase;
bool CTexture::s_bInLevelPhase = false;
bool CTexture::s_bPrestreamPhase;

size_t CTexture::s_nStreamingThroughput = 0;
float CTexture::s_nStreamingTotalTime = 0;

CTextureStreamPoolMgr* CTexture::s_pPoolMgr;
std::set<string> CTexture::s_vTexReloadRequests;
CryCriticalSection CTexture::s_xTexReloadLock;
#ifdef TEXTURE_GET_SYSTEM_COPY_SUPPORT
CTexture::LowResSystemCopyType CTexture::s_LowResSystemCopy[TEX_SYS_COPY_MAX_SLOTS];
CryReadModifyLock CTexture::s_LowResSystemCopyLock;
#endif

#if defined(TEXSTRM_DEFERRED_UPLOAD)
ID3D11DeviceContext* CTexture::s_pStreamDeferredCtx = nullptr;
#endif

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
		(!(eFlags & FT_DONT_STREAM           ) ?  CDeviceObjectFactory::USAGE_STREAMING                                             : 0) |
		( (eFlags & FT_STAGE_READBACK        ) ? (CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_READ ) : 0) |
		( (eFlags & FT_STAGE_UPLOAD          ) ? (CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_WRITE) : 0) |
		( (eFlags & FT_FORCE_MIPS            ) ?  CDeviceObjectFactory::USAGE_AUTOGENMIPS                                           : 0) |
		( (eFlags & FT_USAGE_TEMPORARY       ) ?  CDeviceObjectFactory::USAGE_HIFREQ_HEAP                                           : 0) |
		( (eFlags & FT_USAGE_UAV_OVERLAP     ) ?  CDeviceObjectFactory::USAGE_UAV_OVERLAP                                           : 0) |
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
		(!(eFlags &  CDeviceObjectFactory::USAGE_STREAMING                                            ) ? FT_DONT_STREAM            : 0) |
		( (eFlags & (CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_READ )) ? FT_STAGE_READBACK         : 0) |
		( (eFlags & (CDeviceObjectFactory::USAGE_STAGE_ACCESS | CDeviceObjectFactory::USAGE_CPU_WRITE)) ? FT_STAGE_UPLOAD           : 0) |
		( (eFlags &  CDeviceObjectFactory::USAGE_AUTOGENMIPS                                          ) ? FT_FORCE_MIPS             : 0) |
		( (eFlags &  CDeviceObjectFactory::USAGE_HIFREQ_HEAP                                          ) ? FT_USAGE_TEMPORARY        : 0) |
		( (eFlags &  CDeviceObjectFactory::USAGE_UAV_OVERLAP                                          ) ? FT_USAGE_UAV_OVERLAP      : 0) |
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

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	m_nESRAMOffset = SKIP_ESRAM;
#endif

	m_nUpdateFrameID = -1;
	m_nAccessFrameID = -1;
	m_nCustomID = -1;
	m_pDevTexture = nullptr;

	m_bIsLocked = false;
	m_bNeedRestoring = false;
	m_bNoTexture = false;
	m_bResolved = true;
	m_bUseMultisampledRTV = false;
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
	static_assert((offsetof(CTexture, m_fCurrentMipBias) - offsetof(CTexture, m_pFileTexMips)) <= 64, "Invalid offset!");

#ifndef _RELEASE
	if (!gRenDev->m_pRT->IsRenderThread() || gRenDev->m_pRT->IsRenderLoadingThread())
		__debugbreak();
#endif

#ifndef _RELEASE
	if (IsStreaming())
		__debugbreak();
#endif

	ReleaseDeviceTexture(false);

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
	{
		AUTO_MODIFYLOCK(s_LowResSystemCopyLock);

		for (int slot = 0; slot < TEX_SYS_COPY_MAX_SLOTS; slot++)
			s_LowResSystemCopy[slot].erase(this);
	}
#endif
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
	CTexture* pTex = CRendererResources::s_ptexNoTexture;

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

void CTexture::RefDevTexture(CDeviceTexture* pDeviceTex)
{
	// Hard-wired device-resources can't have a unique owner (they are shared)
	if ((m_pDevTexture))
		CRY_ASSERT(!DEVICE_TEXTURE_STORE_OWNER || m_pDevTexture->GetOwner() == nullptr);

	if ((m_pDevTexture = pDeviceTex))
		m_pDevTexture->SetOwner(nullptr);

	InvalidateDeviceResource(this, eDeviceResourceDirty);
}

void CTexture::SetDevTexture(CDeviceTexture* pDeviceTex)
{
	// Substitute device-resource by a strictly subset one (texture-config doesn't change, only residency)
	if ((m_pDevTexture))
	{
		CRY_ASSERT(!DEVICE_TEXTURE_STORE_OWNER || m_pDevTexture->GetOwner() == this);
		m_pDevTexture->SetOwner(nullptr);
		m_pDevTexture->Release();
	}

	if ((m_pDevTexture = pDeviceTex))
	{
		m_pDevTexture->SetNoDelete(!!(m_eFlags & FT_DONT_RELEASE));
		m_pDevTexture->SetOwner(this);

		m_nDevTextureSize = m_nPersistentSize = m_pDevTexture->GetDeviceSize();
	}

	InvalidateDeviceResource(this, eDeviceResourceDirty);
}

void CTexture::OwnDevTexture(CDeviceTexture* pDeviceTex)
{
	// Take ownership of an entirely different device-resource (texture-config does change)
	if ((m_pDevTexture))
	{
		CRY_ASSERT(!DEVICE_TEXTURE_STORE_OWNER || m_pDevTexture->GetOwner() == this);
		m_pDevTexture->SetOwner(nullptr);
		m_pDevTexture->Release();
	}

	if ((m_pDevTexture = pDeviceTex))
	{
		m_pDevTexture->SetOwner(this);

		const STextureLayout Layput = m_pDevTexture->GetLayout();

		m_nWidth       = Layput.m_nWidth;
		m_nHeight      = Layput.m_nHeight;
		m_nDepth       = Layput.m_nDepth;
		m_nArraySize   = Layput.m_nArraySize;
		m_nMips        = Layput.m_nMips;
		m_eSrcFormat   = Layput.m_eSrcFormat;
		m_eDstFormat   = Layput.m_eDstFormat;
		m_eTT          = Layput.m_eTT;
		m_eFlags       = Layput.m_eFlags; /* TODO: change FT_... to CDeviceObjectFactory::... */
		m_bIsSRGB      = Layput.m_bIsSRGB;

		m_nDevTextureSize = m_nPersistentSize = m_pDevTexture->GetDeviceSize();
	}

	InvalidateDeviceResource(this, eDeviceResourceDirty);
}

void CTexture::PostCreate()
{
	m_nUpdateFrameID = gRenDev->m_pRT->IsRenderThread() ? gRenDev->GetRenderFrameID() : gRenDev->GetMainFrameID();
	m_bPostponed = false;
}

void CTexture::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddObject(m_SrcName);

#ifdef TEXTURE_GET_SYSTEM_COPY_SUPPORT
	{
		AUTO_MODIFYLOCK(s_LowResSystemCopyLock);

		for (int slot = 0; slot < TEX_SYS_COPY_MAX_SLOTS; slot++)
		{
			const LowResSystemCopyType::iterator& it = s_LowResSystemCopy[slot].find(this);
			if (it != CTexture::s_LowResSystemCopy[slot].end())
				pSizer->AddObject((*it).second.m_lowResSystemCopy);
		}
	}
#endif

	if (m_pFileTexMips)
		m_pFileTexMips->GetMemoryUsage(pSizer, m_nMips, m_CacheFileHeader.m_nSides);
}

//=======================================================
// Low-level functions calling CreateDeviceTexture()

void CTexture::CreateRenderTarget(ETEX_Format eFormat, const ColorF& cClear)
{
	if (m_eSrcFormat == eTF_Unknown)
		m_eSrcFormat = eFormat;
	if (m_eSrcFormat == eTF_Unknown)
	{
		m_eFlags |= FT_FAILED;
		return;
	}

	SetClosestFormatSupported();
	m_eFlags |= FT_USAGE_RENDERTARGET;
	m_cClearColor = cClear;
	m_nMips = m_eFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(m_nWidth, m_nHeight) : m_nMips;
	
	SSubresourceData pData(nullptr);
	CreateDeviceTexture(std::move(pData));
	PostCreate();
}

void CTexture::CreateDepthStencil(ETEX_Format eFormat, const ColorF& cClear)
{
	if (m_eSrcFormat == eTF_Unknown)
		m_eSrcFormat = eFormat;
	if (m_eSrcFormat == eTF_Unknown)
	{
		m_eFlags |= FT_FAILED;
		return;
	}

	SetClosestFormatSupported();
	m_eFlags |= FT_USAGE_DEPTHSTENCIL;
	m_cClearColor = cClear;
	m_nMips = m_eFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(m_nWidth, m_nHeight) : m_nMips;

	SSubresourceData pData(nullptr);
	CreateDeviceTexture(std::move(pData));
	PostCreate();
}

void CTexture::CreateShaderResource(STexDataPtr&& td)
{
	m_nWidth = td->m_nWidth;
	m_nHeight = td->m_nHeight;
	m_nDepth = td->m_nDepth;
	m_eSrcFormat = td->m_eFormat;
	m_nMips = td->m_nMips;
	m_fAvgBrightness = td->m_fAvgBrightness;
	m_cMinColor = td->m_cMinColor;
	m_cMaxColor = td->m_cMaxColor;
	m_bUseDecalBorderCol = (td->m_nFlags & FIM_DECAL) != 0;
	m_bIsSRGB = (td->m_nFlags & FIM_SRGB_READ) != 0;

	assert((m_nDepth == 1) || (m_eTT == eTT_3D));
	assert((m_nArraySize == 1) || (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray || m_eTT == eTT_2DArray));
	assert((m_nArraySize % 6) || (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray));
	assert(!td->m_pData[1] || !(m_eFlags & FT_REPLICATE_TO_ALL_SIDES) || (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray));
	assert(m_nWidth && m_nHeight && m_nMips);

	SetClosestFormatSupported();
	if (!(td = ImagePreprocessing(std::move(td), m_eDstFormat)))
	{
		SetNoTexture(m_eTT == eTT_Cube ? CRendererResources::s_ptexNoTextureCM : CRendererResources::s_ptexNoTexture);
		delete td;
		return;
	}

#if defined(TEXTURE_GET_SYSTEM_COPY_SUPPORT)
	if (m_eFlags & FT_KEEP_LOWRES_SYSCOPY)
	{
		uint16 width, height;
		ITexture::GetLowResSystemCopy(width, height);
	}
#endif

	assert(m_nWidth && m_nHeight && m_nMips);

	const int nMaxTextureSize = gRenDev->GetMaxTextureSize();
	if (nMaxTextureSize > 0)
	{
		if (m_nWidth > nMaxTextureSize || m_nHeight > nMaxTextureSize)
		{
			SetNoTexture(m_eTT == eTT_Cube ? CRendererResources::s_ptexNoTextureCM : CRendererResources::s_ptexNoTexture);
			delete td;
			return;
		}
	}

	SSubresourceData pData = td->Transfer();
	CreateDeviceTexture(std::move(pData));
	delete td;
}

//=======================================================
// Mid-level functions calling Create...()

void CTexture::Create2DTexture(int nWidth, int nHeight, int nMips, int nFlags, const byte* pSrcData, ETEX_Format eSrcFormat)
{
	if (nMips <= 0)
		nMips = CTexture::CalcNumMips(nWidth, nHeight);
	m_eSrcTileMode = pSrcData ? eTM_None : eTM_Unspecified;
	m_eSrcFormat = eSrcFormat;
	m_nMips = nMips;

	STexDataPtr td = new STexData;

	td->m_eFormat = eSrcFormat;
	td->m_eTileMode = m_eSrcTileMode;
	td->m_nWidth = nWidth;
	td->m_nHeight = nHeight;
	td->m_nDepth = 1;
	td->m_nMips = nMips;
	td->m_pData[0] = pSrcData;
	if (nFlags & FT_TAKEOVER_DATA_POINTER)
		td->SetReallocated(0);

	CreateShaderResource(std::move(td));
	PostCreate();
}

void CTexture::Create3DTexture(int nWidth, int nHeight, int nDepth, int nMips, int nFlags, const byte* pSrcData, ETEX_Format eSrcFormat)
{
	//if (nMips <= 0)
	//  nMips = CTexture::CalcNumMips(nWidth, nHeight);
	m_eSrcTileMode = pSrcData ? eTM_None : eTM_Unspecified;
	m_eSrcFormat = eSrcFormat;
	m_nMips = nMips;

	STexDataPtr td = new STexData;

	td->m_eFormat = eSrcFormat;
	td->m_eTileMode = m_eSrcTileMode;
	td->m_nWidth = nWidth;
	td->m_nHeight = nHeight;
	td->m_nDepth = nDepth;
	td->m_nMips = nMips;
	td->m_pData[0] = pSrcData;
	if (nFlags & FT_TAKEOVER_DATA_POINTER)
		td->SetReallocated(0);

	CreateShaderResource(std::move(td));
	PostCreate();
}

//=======================================================
// High-level functions calling Create...()

CTexture* CTexture::GetOrCreateTextureObject(const char* name, uint32 nWidth, uint32 nHeight, int nDepth, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID)
{
	SYNCHRONOUS_LOADING_TICK();

	bool bFound = false;

	MEMSTAT_CONTEXT(EMemStatContextType::Texture, name);

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
	pTex->m_nArraySize = nArraySize;
	assert((eType != eTT_CubeArray) || !(nArraySize % 6));
	pTex->m_nDepth = 1;

	if (nFlags & FT_USAGE_RENDERTARGET)
	{
		pTex->CreateRenderTarget(eFormat, Clr_Unknown);
	}
	else if (nFlags & FT_USAGE_DEPTHSTENCIL)
	{
		pTex->CreateDepthStencil(eFormat, Clr_Unknown);
	}
	else
	{
		STexDataPtr td = new STexData;

		td->m_eFormat = eFormat;
		td->m_nDepth = 1;
		td->m_nWidth = nWidth;
		td->m_nHeight = nHeight;
		td->m_nMips = nMips;
		td->m_nFlags = sRGB ? FIM_SRGB_READ : 0;

		pTex->CreateShaderResource(std::move(td));
	}

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

	pTex->CreateRenderTarget(eFormat, cClear);
	pTex->PostCreate();

	return pTex;
}

_smart_ptr<CTexture> CTexture::GetOrCreateRenderTargetPtr(const char* name, uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID)
{
	CTexture* pTex = GetOrCreateRenderTarget(name, nWidth, nHeight, cClear, eTT, nFlags, eFormat, nCustomID);
	_smart_ptr<CTexture> result;
	result.Assign_NoAddRef(pTex);

	return result;
}

CTexture* CTexture::GetOrCreateDepthStencil(const char* name, uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID)
{
	CTexture* pTex = GetOrCreateTextureObject(name, nWidth, nHeight, 1, eTT, nFlags | FT_USAGE_DEPTHSTENCIL, eFormat, nCustomID);
	pTex->SetWidth(nWidth);
	pTex->SetHeight(nHeight);
	pTex->m_nMips = nFlags & FT_FORCE_MIPS ? CTexture::CalcNumMips(pTex->m_nWidth, pTex->m_nHeight) : pTex->m_nMips;
	pTex->m_eFlags |= nFlags;

	pTex->CreateDepthStencil(eFormat, cClear);
	pTex->PostCreate();

	return pTex;
}

_smart_ptr<CTexture> CTexture::GetOrCreateDepthStencilPtr(const char* name, uint32 nWidth, uint32 nHeight, const ColorF& cClear, ETEX_Type eTT, uint32 nFlags, ETEX_Format eFormat, int nCustomID)
{
	CTexture* pTex = GetOrCreateDepthStencil(name, nWidth, nHeight, cClear, eTT, nFlags, eFormat, nCustomID);
	_smart_ptr<CTexture> result;
	result.Assign_NoAddRef(pTex);

	return result;
}

CTexture* CTexture::GetOrCreate2DTexture(const char* szName, int nWidth, int nHeight, int nMips, int nFlags, const byte* pSrcData, ETEX_Format eSrcFormat, bool bAsyncDevTexCreation)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	CTexture* pTex = GetOrCreateTextureObject(szName, nWidth, nHeight, 1, eTT_2D, nFlags, eSrcFormat, -1);
	pTex->Create2DTexture(nWidth, nHeight, nMips, nFlags, pSrcData, eSrcFormat);

	return pTex;
}

CTexture* CTexture::GetOrCreate3DTexture(const char* szName, int nWidth, int nHeight, int nDepth, int nMips, int nFlags, const byte* pSrcData, ETEX_Format eSrcFormat)
{
	CTexture* pTex = GetOrCreateTextureObject(szName, nWidth, nHeight, nDepth, eTT_3D, nFlags, eSrcFormat, -1);
	pTex->Create3DTexture(nWidth, nHeight, nDepth, nMips, nFlags, pSrcData, eSrcFormat);

	return pTex;
}

//=======================================================

void CTexture::Reload()
{
	// If the texture is flagged to not be released, we skip the reloading
	if (m_eFlags & FT_DONT_RELEASE)
		return;

	if (IsStreamed())
	{
		ReleaseDeviceTexture(false);
		ToggleStreaming(true);
		return;
	}

	if (m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL))
	{
		SSubresourceData pData(nullptr);
		CreateDeviceTexture(std::move(pData));
	}
	else
	{
		LoadFromImage(m_SrcName.c_str());
	}

	PostCreate();
}

CTexture* CTexture::ForName(const char* name, uint32 nFlags, ETEX_Format eFormat)
{
	SLICE_AND_SLEEP();

	bool bFound = false;

	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Textures");
	MEMSTAT_CONTEXT(EMemStatContextType::Texture, name);

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

void CTexture::Precache(const bool isBlocking)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY)(iSystem);

	if (!s_bPrecachePhase)
		return;
	if (!gRenDev)
		return;

	gEnv->pLog->UpdateLoadingScreen("Requesting textures precache ...");

	gRenDev->ExecuteRenderThreadCommand( [isBlocking]{ CTexture::RT_Precache(isBlocking); },
		ERenderCommandFlags::LevelLoadingThread_executeDirect
		| (isBlocking ? ERenderCommandFlags::FlushAndWait : ERenderCommandFlags::None));
	if (isBlocking)
	{
		gEnv->pLog->UpdateLoadingScreen("Textures precache done.");
	}
}

void CTexture::RT_Precache(const bool isFinalPrecache)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	// Disable invalid file access logging if texture streaming is disabled
	// If texture streaming is turned off, we will hit this on the renderthread
	// and stall due to the invalid file access stalls
	ICVar* sysPakLogInvalidAccess = NULL;
	int pakLogFileAccess = 0;
	if (!CRenderer::CV_r_texturesstreaming)
	{
		if ((sysPakLogInvalidAccess = gEnv->pConsole->GetCVar("sys_PakLogInvalidFileAccess")))
		{
			pakLogFileAccess = sysPakLogInvalidAccess->GetIVal();
		}
	}

#if !defined(EXCLUDE_NORMAL_LOG)
	CTimeValue t0 = gEnv->pTimer->GetAsyncTime();
#endif

	CryLog("-- Precaching textures...");
	iLog->UpdateLoadingScreen(0);

	if (!gEnv->IsEditor())
		CryLog("=============================== Loading textures ================================");

	// Search texture(s) in a thread-safe manner, and protect the found texture(s) from deletion
	// Loop should block the resource-library as briefly as possible (don't call heavy stuff in the loop)
	std::forward_list<_smart_ptr<CTexture>> pFoundTextures;
	size_t countableTextures = 0;

	{
		CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (pRL)
		{
			ResourcesMapItor itor;
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* pTexture = (CTexture*)itor->second;
				if (!pTexture || !pTexture->CTexture::IsPostponed())
					continue;

				pFoundTextures.emplace_front(pTexture);
				countableTextures++;
			}
		}
	}

	// Preload all the post poned textures
	{
		pFoundTextures.sort(CompareTextures());

		gEnv->pSystem->GetStreamEngine()->PauseStreaming(false, 1 << eStreamTaskTypeTexture);

		// Trigger the texture(s)'s load without holding the resource-library lock to evade dead-locks
		{
			int countedTextures = 0;
			int numTextures = countableTextures;
			int prevProgress = 0;

			// TODO: jobbable
			pFoundTextures.remove_if([&, numTextures](_smart_ptr<CTexture>& pTexture)
			{
				if (!pTexture->m_bStreamPrepared || !pTexture->IsStreamable())
				{
					pTexture->m_fpMinMipCur = MAX_MIP_LEVELS << 8;
					pTexture->m_bPostponed = false;
					pTexture->Load(pTexture->m_eDstFormat);
				}

				int progress = (++countedTextures * 10) / numTextures;
				if (progress != prevProgress)
				{
					prevProgress = progress;
					gEnv->pLog->UpdateLoadingScreen("Precaching progress: %d0.0%% (%d of %d)", progress, countedTextures, countableTextures);
				}

				// Only keep texture which really need Precache()
				return !(pTexture->m_bStreamed && pTexture->m_bForceStreamHighRes);
			});
		}

		{
			CTimeValue time0 = iTimer->GetAsyncTime();

			while (s_StreamPrepTasks.GetNumLive())
			{
				if (gRenDev->m_pRT->IsRenderThread() && !gRenDev->m_pRT->IsRenderLoadingThread() && !gRenDev->m_pRT->IsLevelLoadingThread())
				{
					StreamState_Update();
					StreamState_UpdatePrep();
				}
				else if (gRenDev->m_pRT->IsRenderLoadingThread() || gRenDev->m_pRT->IsLevelLoadingThread())
				{
					StreamState_UpdatePrep();
				}

				CrySleep(1);
			}

			SRenderStatistics::Write().m_fTexUploadTime += (iTimer->GetAsyncTime() - time0).GetSeconds();
		}

		// Trigger the texture(s)'s load without holding the resource-library lock to evade dead-locks
		{
			// TODO: jobbable
			pFoundTextures.remove_if([](_smart_ptr<CTexture>& pTexture)
			{
				pTexture->m_bStreamHighPriority |= 1;
				pTexture->m_fpMinMipCur = MAX_MIP_LEVELS << 8;

				s_pTextureStreamer->Precache(pTexture);

				return true;
			});
		}
	}

	if (!gEnv->IsEditor())
		CryLog("========================== Finished loading textures ============================");

#if !defined(EXCLUDE_NORMAL_LOG)
	CTimeValue t1 = gEnv->pTimer->GetAsyncTime();
	float dt = (t1 - t0).GetSeconds();
	CryLog("Precaching textures done in %.2f seconds", dt);
#endif

	if (isFinalPrecache)
	{
		s_bPrecachePhase = false;
	}

	// Restore pakLogFileAccess if it was disabled during precaching
	// because texture precaching was disabled
	if (pakLogFileAccess)
	{
		sysPakLogInvalidAccess->Set(pakLogFileAccess);
	}
}

void CTexture::Load(ETEX_Format eFormat)
{
	CRY_PROFILE_SECTION_ARG(PROFILE_LOADING_ONLY, "CTexture::Load(ETEX_Format eTFDst)", m_SrcName);
	m_bWasUnloaded = false;
	m_bStreamed = false;

	LoadFromImage(m_SrcName.c_str(), eFormat);   // false=not reloading

	m_eFlags |= FT_FROMIMAGE;
	PostCreate();
}

void CTexture::ToggleStreaming(const bool bEnable)
{
	if (!(m_eFlags & (FT_FROMIMAGE | FT_DONT_RELEASE)) || (m_eFlags & FT_DONT_STREAM))
		return;

	AbortStreamingTasks(this);
	if (bEnable)
	{
		if (IsStreamed())
			return;

		ReleaseDeviceTexture(false);
		m_bStreamed = true;
		if (StreamPrepare(true))
			return;

		if (m_pFileTexMips)
		{
			Unlink();
			StreamState_ReleaseInfo(this, m_pFileTexMips);
			m_pFileTexMips = NULL;
		}
		m_bStreamed = false;
		if (m_bNoTexture)
			return;
	}

	ReleaseDeviceTexture(false);
	Reload();
}

void CTexture::LoadFromImage(const char* name, ETEX_Format eFormat)
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, name);

	if (CRenderer::CV_r_texnoload && SetNoTexture())
		return;

	string sFileName(name);
	sFileName.MakeLower();

	m_eDstFormat = eFormat;

	// try to stream-in the texture
	if (IsStreamable())
	{
		m_bStreamed = true;
		if (StreamPrepare(true))
		{
			assert(m_pDevTexture);
			return;
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
		}
	}

#ifndef _RELEASE
	CRY_DEFINE_ASSET_SCOPE("Texture", m_sAssetScopeName);
#endif

	if (m_bPostponed)
	{
		if (s_pTextureStreamer->BeginPrepare(this, sFileName, (m_eFlags & FT_ALPHA) ? FIM_ALPHA : 0))
			return;
	}

	uint32 nImageFlags =
	  ((m_eFlags & FT_ALPHA) ? FIM_ALPHA : 0) |
	  ((m_eFlags & FT_STREAMED_PREPARE) ? FIM_READ_VIA_STREAMS : 0) |
	  ((m_eFlags & FT_DONT_STREAM) ? FIM_IMMEDIATE_RC : 0);

	Load(CImageFile::mfLoad_file(sFileName, nImageFlags));
}

void CTexture::Load(CImageFilePtr&& pImage)
{
	if (!pImage || pImage->mfGetFormat() == eTF_Unknown)
	{
		SetNoTexture(m_eTT == eTT_Cube ? CRendererResources::s_ptexNoTextureCM : CRendererResources::s_ptexNoTexture);
		return;
	}

	//CRY_PROFILE_SECTION_ARG(PROFILE_LOADING_ONLY, "CTexture::Load(CImageFile* pImage)", pImage->mfGet_filename().c_str());

	if ((m_eFlags & FT_ALPHA) && !pImage->mfIs_image(0))
	{
		SetNoTexture(CRendererResources::s_ptexWhite);
		return;
	}

	const char* name = pImage->mfGet_filename().c_str();
	if (pImage->mfGet_Flags() & FIM_SPLITTED)              // propagate splitted file flag
		m_eFlags |= FT_SPLITTED;
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
	      pImage->mfGetFormat() == eTF_BC5U || pImage->mfGetFormat() == eTF_BC5S || pImage->mfGetFormat() == eTF_BC7 ||
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
	  (pImage->mfGet_depth() > 1) ? eTT_3D :
	  (pImage->mfGet_NumSides() == 6) ? eTT_Cube :
	  !(pImage->mfGet_NumSides() % 6) ? eTT_CubeArray :
	  (pImage->mfGet_NumSides() == 1) ? eTT_2D :
	  eTT_2DArray;

	STexDataPtr td = new STexData;

	td->m_nFlags = pImage->mfGet_Flags();
	td->m_nWidth = pImage->mfGet_width();
	td->m_nHeight = pImage->mfGet_height();
	td->m_nDepth = pImage->mfGet_depth();
	td->m_eFormat = pImage->mfGetFormat();
	td->m_eTileMode = pImage->mfGetTileMode();
	td->m_nMips = pImage->mfGet_numMips();
	td->m_fAvgBrightness = pImage->mfGet_avgBrightness();
	td->m_cMinColor = pImage->mfGet_minColor();
	td->m_cMaxColor = pImage->mfGet_maxColor();
	if ((m_eFlags & FT_NOMIPS) || td->m_nMips <= 0)
		td->m_nMips = 1;
	td->m_pFilePath = pImage->mfGet_filename();

	// base range after normalization, fe. [0,1] for 8bit images, or [0,2^15] for RGBE/HDR data
	if (CImageExtensionHelper::IsDynamicRange(td->m_eFormat))
	{
		td->m_cMinColor /= td->m_cMaxColor.a;
		td->m_cMaxColor /= td->m_cMaxColor.a;
	}

	// Move data-pointer from Image into TexData
	for (int i = 0, j = !pImage->mfIs_image(1) ? 1 : 6; i < j; i++)
	{
		td->AssignData(i, pImage->mfGet_image(i, true));
	}

	CreateShaderResource(std::move(CTexture::FormatFixup(std::move(td))));
}

void CTexture::UpdateData(STexDataPtr&& td, int flags)
{
	m_eFlags = flags;
	m_eDstFormat = td->m_eFormat;

	CreateShaderResource(std::move(td));
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
		return CTexture::GetClosestFormatSupported(eFormat, pPF);
	}

	default:
		return eFormat;
	}
}

STexDataPtr CTexture::FormatFixup(STexDataPtr&& td)
{
	const ETEX_Format eSrcFormat = td->m_eFormat;
	const ETEX_Format eDstFormat = FormatFixup(eSrcFormat);
	CRY_ASSERT(eDstFormat != eTF_Unknown);

	if (td->m_eTileMode == eTM_None)
	{
		// Try and expand
		uint32 nSourceSize = CTexture::TextureDataSize(td->m_nWidth, td->m_nHeight, td->m_nDepth, td->m_nMips, 1, eSrcFormat, eTM_None);
		uint32 nTargetSize = CTexture::TextureDataSize(td->m_nWidth, td->m_nHeight, td->m_nDepth, td->m_nMips, 1, eDstFormat, eTM_None);

		for (int nImage = 0; nImage < sizeof(td->m_pData) / sizeof(td->m_pData[0]); ++nImage)
		{
			if (td->m_pData[nImage])
			{
				byte* pNewImage = new byte[nTargetSize];
				CTexture::ExpandMipFromFile(pNewImage, nTargetSize, td->m_pData[nImage], nSourceSize, eSrcFormat, eDstFormat);
				td->AssignData(nImage, pNewImage);
			}
		}

		td->m_eFormat = eDstFormat;
	}
	else
	{
#ifndef _RELEASE
		if (eDstFormat != eSrcFormat)
			__debugbreak();
#endif
	}

	return td;
}

STexDataPtr CTexture::ImagePreprocessing(STexDataPtr&& td, ETEX_Format eDstFormat)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	#if !defined(_RELEASE)
	const char* pTexFileName = td->m_pFilePath ? td->m_pFilePath : "$Unknown";
#endif

	if (eDstFormat == eTF_Unknown)
	{
		// NOTE: leaks memory
		td->m_pData[0] = td->m_pData[1] = td->m_pData[2] = td->m_pData[3] = td->m_pData[4] = td->m_pData[5] = 0;

#if !defined(_RELEASE)
		TextureError(pTexFileName, "Trying to process a texture with unsupported target format %s->unknown!", NameForTextureFormat(td->m_eFormat));
#endif
		return nullptr;
	}

	const ETEX_Format eSrcFormat = td->m_eFormat;
	const bool fmtConversionNeeded = eSrcFormat != eDstFormat;

#if !CRY_PLATFORM_WINDOWS || CRY_RENDERER_OPENGL
	if (fmtConversionNeeded)
	{
		// NOTE: leaks memory
		td->m_pData[0] = td->m_pData[1] = td->m_pData[2] = td->m_pData[3] = td->m_pData[4] = td->m_pData[5] = 0;

	#if !defined(_RELEASE)
		TextureError(pTexFileName, "Trying an image format conversion from %s to %s. This is not supported on this platform!", NameForTextureFormat(eSrcFormat), NameForTextureFormat(eDstFormat));
	#endif
		return nullptr;
	}
#else

	const bool doProcessing = fmtConversionNeeded;
	if (doProcessing)
	{
		const int nSrcWidth = td->m_nWidth;
		const int nSrcHeight = td->m_nHeight;

		for (int i = 0; i < 6; i++)
		{
			if (const byte* pSrcData = td->m_pData[i])
			{
				uint32 nOutSize = 0;
				const byte* pNewData = Convert(pSrcData, nSrcWidth, nSrcHeight, td->m_nMips, eSrcFormat, eDstFormat, td->m_nMips, nOutSize, true);
				if (pNewData)
					td->AssignData(i, pNewData);
			}
		}

		td->m_eFormat = eDstFormat;
	}
#endif

	return td;
}

int8 CTexture::CalcNumMips(int nWidth, int nHeight)
{
	int8 nMips = 0;
	while (nWidth || nHeight)
	{
		if (!nWidth)   nWidth = 1;
		if (!nHeight)  nHeight = 1;
		nWidth >>= 1;
		nHeight >>= 1;
		nMips++;
	}
	return nMips;
}

uint32 CTexture::TextureDataSize(uint32 nWidth, uint32 nHeight, uint32 nDepth, int8 nMips, uint32 nSlices, const ETEX_Format eTF, ETEX_TileMode eTM)
{
	FUNCTION_PROFILER_RENDERER();

	// Don't allow 0 dimensions, it's clearly wrong to reflect on "unspecified-yet" textures.
	CRY_ASSERT(eTF != eTF_Unknown && nWidth && nHeight && nDepth);
	// Allow 0 mips and 0 slices to generate offsets with this function.
	if (!nMips || !nSlices)
		return 0;

	const bool bIsBlockCompressed = IsBlockCompressed(eTF);
	nWidth  = max(1U, nWidth);
	nHeight = max(1U, nHeight);
	nDepth  = max(1U, nDepth);

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
	}

	{
		const uint32 nBytesPerElement = bIsBlockCompressed ? BytesPerBlock(eTF) : BytesPerPixel(eTF);

		uint32 nSize = 0;
		while ((nWidth || nHeight || nDepth) && nMips)
		{
			nWidth  = max(1U, nWidth);
			nHeight = max(1U, nHeight);
			nDepth  = max(1U, nDepth);

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

void CTexture::ExpandMipFromFile(byte* dst, const uint32 dstSize, const byte* src, const uint32 srcSize, const ETEX_Format eSrcFormat, const ETEX_Format eDstFormat)
{
	if (IsInPlaceFormat(eSrcFormat))
	{
		assert(dstSize == srcSize);
		if (dst != src)
		{
			memcpy(dst, src, srcSize);
		}

		return;
	}

	// upload mip from file with conversions depending on format and platform specifics
	switch (eSrcFormat)
	{
	case eTF_B8G8R8: // -> eTF_R8G8B8A8
		assert(eDstFormat == eTF_R8G8B8A8);
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize / 3) - 1; i >= 0; --i)
			{
				dst[i * 4 + 0] = src[i * 3 + 2];
				dst[i * 4 + 1] = src[i * 3 + 1];
				dst[i * 4 + 2] = src[i * 3 + 0];
				dst[i * 4 + 3] = 255;
			}
		}
		break;
	case eTF_L8V8U8X8: // -> eTF_R8G8B8A8S
		assert(eDstFormat == eTF_R8G8B8A8S);
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize / 4) - 1; i >= 0; --i)
			{
				dst[i * 4 + 0] = src[i * 3 + 0];
				dst[i * 4 + 1] = src[i * 3 + 1];
				dst[i * 4 + 2] = src[i * 3 + 2];
				dst[i * 4 + 3] = src[i * 3 + 3];
			}
		}
		break;
	case eTF_L8V8U8: // -> eTF_R8G8B8A8S
		assert(eDstFormat == eTF_R8G8B8A8S);
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize / 3) - 1; i >= 0; --i)
			{
				dst[i * 4 + 0] = src[i * 3 + 0];
				dst[i * 4 + 1] = src[i * 3 + 1];
				dst[i * 4 + 2] = src[i * 3 + 2];
				dst[i * 4 + 3] = 255;
			}
		}
		break;
	case eTF_L8: // -> eTF_R8G8B8A8
		assert(eDstFormat == eTF_R8G8B8A8);
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize) - 1; i >= 0; --i)
			{
				const byte bSrc = src[i];
				dst[i * 4 + 0] = bSrc;
				dst[i * 4 + 1] = bSrc;
				dst[i * 4 + 2] = bSrc;
				dst[i * 4 + 3] = 255;
			}
		}
		break;
	case eTF_A8L8: // -> eTF_R8G8B8A8
		assert(eDstFormat == eTF_R8G8B8A8);
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize) - 1; i >= 0; i -= 2)
			{
				const byte bSrcL = src[i - 1];
				const byte bSrcA = src[i - 0];
				dst[i * 4 + 0] = bSrcL;
				dst[i * 4 + 1] = bSrcL;
				dst[i * 4 + 2] = bSrcL;
				dst[i * 4 + 3] = bSrcA;
			}
		}
		break;

	case eTF_B5G5R5A1: // -> eTF_B8G8R8A8
		assert(eDstFormat == eTF_B8G8R8A8);
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize / 2) - 1; i >= 0; --i)
			{
				const uint16 rgb5551 = uint16((src[i * 2 + 0] << 8) + src[i * 2 + 1]);
				dst[i * 4 + 0] = ((rgb5551 >> 0) * 33) >> 2;
				dst[i * 4 + 1] = ((rgb5551 >> 5) * 33) >> 2;
				dst[i * 4 + 2] = ((rgb5551 >> 10) * 33) >> 2;
				dst[i * 4 + 3] = ((rgb5551 >> 15) ? 255 : 0);
			}
		}
		break;
	case eTF_B5G6R5: // -> eTF_B8G8R8X8
		assert(eDstFormat == eTF_B8G8R8X8);
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize / 2) - 1; i >= 0; --i)
			{
				const uint16 rgb565 = uint16((src[i * 2 + 0] << 8) + src[i * 2 + 1]);
				dst[i * 4 + 0] = ((rgb565 >> 0) * 33) >> 2;
				dst[i * 4 + 1] = ((rgb565 >> 5) * 65) >> 4;
				dst[i * 4 + 2] = ((rgb565 >> 11) * 33) >> 2;
				dst[i * 4 + 3] = 255;
			}
		}
		break;
	case eTF_B4G4R4A4: // -> eTF_B8G8R8A8
	case eTF_R4G4B4A4: // -> eTF_R8G8B8A8
		assert((eSrcFormat == eTF_B4G4R4A4 && eDstFormat == eTF_B8G8R8A8) ||
		       (eSrcFormat == eTF_R4G4B4A4 && eDstFormat == eTF_R8G8B8A8));
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize / 2) - 1; i >= 0; --i)
			{
				const uint16 rgb4444 = uint16((src[i * 2 + 0] << 8) + src[i * 2 + 1]);
				dst[i * 4 + 0] = (rgb4444 >> 0) * 17;
				dst[i * 4 + 1] = (rgb4444 >> 4) * 17;
				dst[i * 4 + 2] = (rgb4444 >> 8) * 17;
				dst[i * 4 + 3] = (rgb4444 >> 12) * 17;
			}
		}
		break;
	case eTF_R4G4: // -> eTF_R8G8|eTF_R8G8B8A8
		assert(eDstFormat == eTF_R8G8 || eDstFormat == eTF_R8G8B8A8);
		if (eDstFormat == eTF_R8G8)
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize / 1) - 1; i >= 0; --i)
			{
				const uint8 rgb44 = uint8(src[i * 1 + 0]);
				dst[i * 2 + 0] = (rgb44 >> 0) * 17;
				dst[i * 2 + 1] = (rgb44 >> 4) * 17;
			}
		}
		else
		{
			for (ptrdiff_t i = ptrdiff_t(srcSize / 1) - 1; i >= 0; --i)
			{
				const uint8 rgb44 = uint8(src[i * 1 + 0]);
				dst[i * 4 + 0] = (rgb44 >> 0) * 17;
				dst[i * 4 + 1] = (rgb44 >> 4) * 17;
				dst[i * 4 + 2] = 0;
				dst[i * 4 + 3] = 255;
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
			uint32 nOutSize = 0;

			if (m_eSrcFormat == eDstFormat && pDstData)
			{
				memcpy(pDstData, pData, GetDeviceDataSize());
			}
			else
			{
				pDstData = (byte*)Convert((byte*)pData, m_nWidth, m_nHeight, 1, m_eSrcFormat, eDstFormat, 1, nOutSize, true);
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

const size_t CTexture::GetAllocatedSystemMemory(bool bIncludePool, bool bIncludeCache) const
{
	size_t nSize = sizeof(*this);
	nSize += m_SrcName.capacity();

	// TODO: neccessary?
	//	if (m_pRenderTargetData)
	//	{
	//		nSize += sizeof(*m_pRenderTargetData);
	//		nSize += m_pRenderTargetData->m_DirtyRects.capacity() * sizeof(RECT);
	//	}

	if (m_pFileTexMips)
	{
		if (bIncludeCache)
			nSize += m_pFileTexMips->GetAllocatedSystemMemory(m_nMips, m_CacheFileHeader.m_nSides);
		if (bIncludePool && m_pFileTexMips->m_pPoolItem)
			nSize += m_pFileTexMips->m_pPoolItem->GetAllocatedSystemMemory();
	}

	return nSize;
}

void CTexture::Init()
{
	SDynTexture::Init();
	InitStreaming();
}

void CTexture::PostInit()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
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
	FUNCTION_PROFILER_RENDERER();

	// reload pending texture reload requests
	{
		std::set<string> queue;

		s_xTexReloadLock.Lock();
		s_vTexReloadRequests.swap(queue);
		s_xTexReloadLock.Unlock();

		// TODO: jobbable
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

	if ((s_nStreamingMode != CRenderer::CV_r_texturesstreaming) || (s_nStreamingUpdateMode != CRenderer::CV_r_texturesstreamingUpdateType))
	{
		InitStreaming();
	}

#ifndef CONSOLE_CONST_CVAR_MODE
	char buf[256] = "";

	if (CRenderer::CV_r_texlog)
	{
		CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		ResourcesMapItor itor;

		uint32 i;
		if (pRL && (CRenderer::CV_r_texlog == 2 || CRenderer::CV_r_texlog == 3 || CRenderer::CV_r_texlog == 4))
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

					size_t nTSize = Texs[i]->m_pFileTexMips->GetAllocatedSystemMemory(Texs[i]->GetNumMips(), Texs[i]->GetNumSides());

					fprintf(fp, "%" PRISIZE_T "\t\t%d x %d\t\tType: %s\t\tMips: %d\t\tFormat: %s\t\t(%s)\n", nTSize, w, h, Texs[i]->NameForTextureType(Texs[i]->GetTextureType()), Texs[i]->GetNumMips(), Texs[i]->NameForTextureFormat(Texs[i]->GetDstFormat()), Texs[i]->GetName());
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
				if (tp->m_nAccessFrameID == gRenDev->GetMainFrameID())
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
		else if (pRL && (CRenderer::CV_r_texlog == 1))
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

				if (tex->GetDevTexture() && !(texFlags & FT_USAGE_RENDERTARGET))
				{
					AllSize += texDataSize;
					if (!Texs[i]->IsStreamed())
					{
						NonStrSize += texDataSize;
						nNoStr++;
					}
				}

				if (texFlags & FT_USAGE_RENDERTARGET)
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
				if (tp && !tp->IsNoTexture() && tp->m_nAccessFrameID == gRenDev->GetMainFrameID())
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
				if (Texs[i]->GetFlags() & FT_USAGE_RENDERTARGET)
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
	}
#endif
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

void CTexture::ShutDown()
{
	RT_FlushAllStreamingTasks(true);

	if (s_pStatsTexWantedLists)
	{
		for (int i = 0; i < 2; ++i)
			s_pStatsTexWantedLists[i].clear();
	}

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
	}

	s_pPoolMgr->Flush();
}

void CTexture::ReloadFile_Request(const char* szFileName)
{
	s_xTexReloadLock.Lock();
	s_vTexReloadRequests.insert(szFileName);
	s_xTexReloadLock.Unlock();
}

void CTexture::ReloadFile(const char* szFileName) threadsafe
{
	FUNCTION_PROFILER_RENDERER();

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

	string realName = PathUtil::ToUnixPath(szFileName);
	if (realName.size() >= (uint32)gameFolderPathLength && memcmp(realName.data(), gameFolderPath, gameFolderPathLength) == 0)
		realName += gameFolderPathLength;

	CCryNameTSCRC crc32NameBase = GenName(realName, 0);
	CCryNameTSCRC crc32NameAtch = GenName(realName, FT_ALPHA);

	// Search texture in a thread-safe manner, and protect the found texture from deletion
	_smart_ptr<CTexture> pFoundTextureBase = nullptr;
	_smart_ptr<CTexture> pFoundTextureAtch = nullptr;

	{
		CryAutoReadLock<CryRWLock> lock(s_cResLock);

		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (pRL)
		{
			const auto& itorBase = pRL->m_RMap.find(crc32NameBase);
			const auto& itorAtch = pRL->m_RMap.find(crc32NameAtch);

			if (itorBase != pRL->m_RMap.end())
				pFoundTextureBase = (CTexture*)itorBase->second;
			if (itorAtch != pRL->m_RMap.end())
				pFoundTextureAtch = (CTexture*)itorAtch->second;
		}
	}

	// Trigger the texture's reload without holding the resource-library lock to evade dead-locks
	if (pFoundTextureBase)
		pFoundTextureBase->Reload();
	if (pFoundTextureAtch)
		pFoundTextureAtch->Reload();
}

void CTexture::ReloadTextures() threadsafe
{
	FUNCTION_PROFILER_RENDERER();

	// Flush any outstanding texture requests before reloading
	gEnv->pRenderer->FlushPendingTextureTasks();

	std::forward_list<_smart_ptr<CTexture>> pFoundTextures;

	// Search texture(s) in a thread-safe manner, and protect the found texture(s) from deletion
	// Loop should block the resource-library as briefly as possible (don't call heavy stuff in the loop)
	{
		CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (pRL)
		{
			ResourcesMapItor itor;
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* pTexture = (CTexture*)itor->second;
				if (!pTexture || !(pTexture->m_eFlags & FT_FROMIMAGE))
					continue;

				pFoundTextures.emplace_front(pTexture);
			}
		}
	}

	// Trigger the texture(s)'s reload without holding the resource-library lock to evade dead-locks
	{
		// TODO: jobbable
		pFoundTextures.remove_if([](_smart_ptr<CTexture>& pTexture) { pTexture->Reload(); return true; });
	}
}

void CTexture::ToggleTexturesStreaming() threadsafe
{
	FUNCTION_PROFILER_RENDERER();

	// Flush any outstanding texture requests before reloading
	gEnv->pRenderer->FlushPendingTextureTasks();

	std::forward_list<_smart_ptr<CTexture>> pFoundTextures;

	// Search texture(s) in a thread-safe manner, and protect the found texture(s) from deletion
	// Loop should block the resource-library as briefly as possible (don't call heavy stuff in the loop)
	{
		CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (pRL)
		{
			ResourcesMapItor itor;
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* pTexture = (CTexture*)itor->second;
				if (!pTexture || !(pTexture->m_eFlags & FT_FROMIMAGE))
					continue;

				pFoundTextures.emplace_front(pTexture);
			}
		}
	}

	// Trigger the texture(s)'s reload without holding the resource-library lock to evade dead-locks
	{
		// TODO: jobbable
		pFoundTextures.remove_if([](_smart_ptr<CTexture>& pTexture) { pTexture->ToggleStreaming(CRenderer::CV_r_texturesstreaming != 0); return true; });
	}
}

void CTexture::LogTextures(ILog* pLog) threadsafe
{
	std::forward_list<_smart_ptr<CTexture>> pFoundTextures;

	// Search texture(s) in a thread-safe manner, and protect the found texture(s) from deletion
	// Loop should block the resource-library as briefly as possible (don't call heavy stuff in the loop)
	{
		CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (pRL)
		{
			ResourcesMapItor itor;
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* pTexture = (CTexture*)itor->second;
				if (!pTexture)
					continue;
				const char* pName = pTexture->GetName();
				if (!pName || strchr(pName, '/'))
					continue;

				pFoundTextures.emplace_front(pTexture);
			}
		}
	}

	// Trigger the texture(s)'s reload without holding the resource-library lock to evade dead-locks
	{
		pFoundTextures.remove_if([](_smart_ptr<CTexture>& pTexture) { iLog->Log("\t%s -- fmt: %s, dim: %d x %d\n", pTexture->GetName(), pTexture->GetFormatName(), pTexture->GetWidth(), pTexture->GetHeight()); return true; });
	}
}

bool CTexture::SetNoTexture(CTexture* pDefaultTexture /* = s_ptexNoTexture*/)
{
	m_eFlags |= FT_FAILED;

	if (pDefaultTexture)
	{
		RefDevTexture(pDefaultTexture->GetDevTexture());

		m_eSrcFormat = pDefaultTexture->GetSrcFormat();
		m_eDstFormat = pDefaultTexture->GetDstFormat();
		m_nMips = pDefaultTexture->GetNumMips();
		m_nWidth = pDefaultTexture->GetWidth();
		m_nHeight = pDefaultTexture->GetHeight();
		m_nDepth = 1;
		m_fAvgBrightness = 1.0f;
		m_cMinColor = 0.0f;
		m_cMaxColor = 1.0f;
		m_cClearColor = ColorF(0.0f, 0.0f, 0.0f, 1.0f);

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

		m_nDevTextureSize = 0;
		m_nPersistentSize = 0;

		return true;
	}

	return false;
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
			const int frameID = gRenDev->GetMainFrameID();
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
			const int frameID = gRenDev->GetRenderFrameID();
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
}

std::shared_ptr<IFlashPlayer> CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::GetTempPtr() const
{
	return m_pPlayer;
}

std::shared_ptr<IFlashPlayer> CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::GetPermPtr(CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	m_canDeactivate = false;
	CreateInstance(pSrc);

	return m_pPlayer;
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapper::Activate(bool activate, CFlashTextureSourceBase* pSrc)
{
	if (activate)
	{
		CryAutoCriticalSection lock(m_lock);
		CreateInstance(pSrc);
	}
	else if (m_canDeactivate)
	{
		m_pPlayer = nullptr;
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

std::shared_ptr<IFlashPlayer> CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::GetTempPtr() const
{
	return m_pPlayer;
}

std::shared_ptr<IFlashPlayer> CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::GetPermPtr(CFlashTextureSourceBase* pSrc)
{
	CryAutoCriticalSection lock(m_lock);

	m_canDeactivate = false;
	CreateInstance(pSrc, m_layoutName.c_str());

	return m_pPlayer;
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperLayoutElement::Activate(bool activate, CFlashTextureSourceBase* pSrc)
{
	if (activate)
	{
		CryAutoCriticalSection lock(m_lock);
		CreateInstance(pSrc, m_layoutName.c_str());
	}
	else if (m_canDeactivate)
	{
		m_pPlayer = nullptr;
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
		const char* pExt = PathUtil::GetExt(layoutName);
		if (!pExt || strcmpi(pExt, "layout") != 0)
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
}

void CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::SetUIElement(IUIElement* p)
{
	assert(!m_pUIElement);
	m_pUIElement = p;
	m_pUIElement->AddRef();
}

std::shared_ptr<IFlashPlayer> CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::GetTempPtr() const
{
	return m_pPlayer;
}

std::shared_ptr<IFlashPlayer> CFlashTextureSourceBase::CFlashPlayerInstanceWrapperUIElement::GetPermPtr(CFlashTextureSourceBase* pSrc)
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

	m_pPlayer = nullptr;
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
	CRY_ASSERT(gRenDev->m_pRT->IsMainThread() || gRenDev->m_pRT->IsLevelLoadingThread());

	if (m_pUIElement)
	{
		if (m_activated)
		{
			const bool isVisible = m_pUIElement->IsVisible();
			std::shared_ptr<IFlashPlayer> pPlayer = isVisible ? m_pUIElement->GetFlashPlayer() : NULL;
			if (pPlayer != m_pPlayer)
			{
				const bool addTex = m_pPlayer == nullptr;
				m_pPlayer = nullptr;
				if (isVisible)
					m_pPlayer = pPlayer;

				if (m_pPlayer)
				{
					m_width = m_pPlayer->GetWidth();
					m_height = m_pPlayer->GetHeight();
					if (addTex)
						m_pUIElement->AddTexture(pSrc);
				}
				else
					m_pUIElement->RemoveTexture(pSrc);
			}
		}
		else
		{
			if (m_pPlayer)
				m_pUIElement->RemoveTexture(pSrc);
			m_pPlayer = nullptr;
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
	, m_autoUpdate(true)
	, m_perFrameRendering(false)
	, m_pFlashFileName(pFlashFileName)
	, m_pFlashPlayer(CFlashPlayerInstanceWrapperNULL::Get())
	, m_pElement(NULL)
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
			else
			{
#ifndef _RELEASE
				std::pair<IUIElement*, IUIElement*> result = gEnv->pFlashUI->GetUIElementsByInstanceStr(pFlashFileName);
				std::pair<string, int> identifiers = gEnv->pFlashUI->GetUIIdentifiersByInstanceStr(pFlashFileName);

				if (!result.first)
					CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_WARNING,
						"UI-Element identifier \"%s\" can't be found in the UI-Database.\n", identifiers.first.c_str());
				else if (!result.second)
					CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_WARNING,
						"UI-Element \"%s\" doesn't have an instance %d.\n", identifiers.first.c_str(), identifiers.second);
#endif
			}
		}
		else if (IsFlashUILayoutFile(pFlashFileName))
		{
			if (m_pFlashPlayer)
				m_pFlashPlayer->Clear(this);

			m_pFlashPlayer = nullptr;

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
		const char* pExt = PathUtil::GetExt(pFlashFileName);
		const bool bPath = strchr(pFlashFileName, '/') || strchr(pFlashFileName, '\\');

		if (pExt)
		{
			if (!bPath)
			{
				// Pseudo files (no path, looks up flow-node)
				return (!stricmp(pExt, "ui"));
			}

			// Real files (looks up filesystem)
			return (!stricmp(pExt, "layout") || !stricmp(pExt, "gfx") || !stricmp(pExt, "swf") || !stricmp(pExt, "usm"));
		}
	}

	return false;
}

bool CFlashTextureSourceBase::IsFlashUIFile(const char* pFlashFileName)
{
	if (pFlashFileName)
	{
		const char* pExt = PathUtil::GetExt(pFlashFileName);
		const bool bPath = strchr(pFlashFileName, '/') || strchr(pFlashFileName, '\\');

		if (pExt)
		{
			if (!bPath)
			{
				// Pseudo files (no path, looks up flow-node)
				return !stricmp(pExt, "ui");
			}
		}
	}

	return false;
}

bool CFlashTextureSourceBase::IsFlashUILayoutFile(const char* pFlashFileName)
{
	if (pFlashFileName)
	{
		const char* pExt = PathUtil::GetExt(pFlashFileName);

		if (pExt)
		{
			return !stricmp(pExt, "layout");
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
		m_pFlashPlayer = nullptr;

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

	m_pFlashPlayer = nullptr;
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
		return m_pFlashPlayer->GetTempPtr().get();
	}
	return nullptr;
}

void* CFlashTextureSourceBase::GetSourcePerm(EDynTextureSource type)
{
	if (m_pFlashPlayer != nullptr && type == DTS_I_FLASHPLAYER)
	{
		return m_pFlashPlayer->GetPermPtr(this).get();
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
	m_pDynTexture = new SDynTexture(GetWidth(), GetHeight(), Clr_Transparent, eTF_R8G8B8A8, eTT_2D, FT_USAGE_RENDERTARGET | FT_STATE_CLAMP | FT_FORCE_MIPS | FT_USAGE_ALLOWREADSRGB, "FlashTextureSourceUniqueRT");
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
uint16 CFlashTextureSourceSharedRT::ms_sharedRTWidth = 256;
uint16 CFlashTextureSourceSharedRT::ms_sharedRTHeight = 256;

CFlashTextureSourceSharedRT::CFlashTextureSourceSharedRT(const char* pFlashFileName, const IRenderer::SLoadShaderItemArgs* pArgs)
	: CFlashTextureSourceBase(pFlashFileName, pArgs)
{
	++ms_instCount;
	if (!ms_pDynTexture)
	{
		// create render-target with mip-maps
		ms_pDynTexture = new SDynTexture(ms_sharedRTWidth, ms_sharedRTHeight, Clr_Transparent, eTF_R8G8B8A8, eTT_2D, FT_USAGE_RENDERTARGET | FT_STATE_CLAMP | FT_FORCE_MIPS | FT_USAGE_ALLOWREADSRGB, "FlashTextureSourceSharedRT");
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
	ms_sharedRTWidth  = NearestPowerOfTwo(width  > 16 ? width  : 16);
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
	uint32 nSize;
	ETEX_Format eTF;
	if (!pTex)
	{
		eTF = eTF_R8G8B8A8;
		if (nW < 0)
			nW = CRendererResources::s_renderWidth;
		if (nH < 0)
			nH = CRendererResources::s_renderHeight;
		nSize = CTexture::TextureDataSize(nW, nH, 1, 1, 1, eTF, eTM_Optimal);
		TS.m_Name = "Back buffer";
	}
	else
	{
		eTF = pTex->GetDstFormat();
		if (nW < 0)
			nW = pTex->GetWidth();
		if (nH < 0)
			nH = pTex->GetHeight();
		nSize = CTexture::TextureDataSize(nW, nH, 1, pTex->GetNumMips(), 1, eTF, eTM_Optimal);
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

	m_renderTargetStats.push_back(TS);
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
	for (size_t i = 0; i < m_renderTargetStats.size(); i++)
	{
		SRTargetStat* pRT = &m_renderTargetStats[i];

		IRenderAuxText::Draw2dLabel((float)nX, (float)nY, 1.4f, &col.r, false, "%s (%d x %d x %s), Size: %.3f Mb", pRT->m_Name.c_str(), pRT->m_nWidth, pRT->m_nHeight, CTexture::NameForTextureFormat(pRT->m_eTF), (float)pRT->m_nSize / 1024.0f / 1024.0f);
		nY += nYstep;
		if (nY >= CRendererResources::s_displayHeight - 25)
		{
			nY = nYstart;
			nX += 500;
		}
		nSize += pRT->m_nSize;
	}
	col = Col_Yellow;
	IRenderAuxText::Draw2dLabel((float)nX, (float)(nY + 10), 1.4f, &col.r, false, "Total: %d RT's, Size: %.3f Mb", m_renderTargetStats.size(), nSize / 1024.0f / 1024.0f);
}

STexPool::~STexPool()
{
#ifndef _RELEASE
	bool poolEmpty = true;
#endif

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

const ColorB* CTexture::GetLowResSystemCopy(uint16& width, uint16& height, int** ppUserData, int maxTexSize)
{
	AUTO_READLOCK(s_LowResSystemCopyLock);

	// find slot based on requested texture size, snap texture size to power of 2
	int slot = CLAMP((int)log2(maxTexSize) - 4, 0, TEX_SYS_COPY_MAX_SLOTS - 1);
	maxTexSize = (int)exp2(slot + 4);

	LowResSystemCopyType::iterator it = s_LowResSystemCopy[slot].find(this);

	// load if not ready yet
	if (it == CTexture::s_LowResSystemCopy[slot].end())
	{
		if (m_eTT != eTT_2D || (m_nMips <= 1 && (m_nWidth > maxTexSize || m_nHeight > maxTexSize)) || m_eDstFormat < eTF_BC1 || m_eDstFormat > eTF_BC7)
			return nullptr;

		// to switch to modify we need to unlock first
		s_LowResSystemCopyLock.UnlockRead();
		{ // to reduce contention, we want to hold the modify lock as shortly as possible
			AUTO_MODIFYLOCK(s_LowResSystemCopyLock);
			s_LowResSystemCopy[slot][this]; // make a map entry for this
		}
		
		// 'restore' AUTO_READLOCK
		s_LowResSystemCopyLock.LockRead();
		it = s_LowResSystemCopy[slot].find(this);
		SLowResSystemCopy& rCopy = it->second;
		PrepareLowResSystemCopy(maxTexSize, rCopy.m_lowResSystemCopy, rCopy.m_nLowResCopyWidth, rCopy.m_nLowResCopyHeight);
	}

	if (it != CTexture::s_LowResSystemCopy[slot].end())
	{
		width = (*it).second.m_nLowResCopyWidth;
		height = (*it).second.m_nLowResCopyHeight;
		if (ppUserData)
			*ppUserData = &(*it).second.m_nLowResSystemCopyAtlasId;
		return (*it).second.m_lowResSystemCopy.GetElements();
	}

	assert(!"CTexture::GetLowResSystemCopy failed");

	return NULL;
}

bool CTexture::PrepareLowResSystemCopy(const uint16 maxTexSize, PodArray<ColorB>& textureData, uint16& width, uint16& height)
{
	_smart_ptr<IImageFile> imageData = gEnv->pRenderer->EF_LoadImage(GetName(), 0);

	if (imageData)
	{
		width = imageData->mfGet_width();
		height = imageData->mfGet_height();
		byte* pSrcData = imageData->mfGet_image(0);
		ETEX_Format texFormat = imageData->mfGetFormat();

		int mipLevel = 0;

		while (width > maxTexSize || height > maxTexSize)
		{
			int sizeDxtMip = gRenDev->GetTextureFormatDataSize(width, height, 1, 1, texFormat, eTM_None);
			pSrcData += sizeDxtMip;

			width /= 2;
			height /= 2;
			mipLevel++;
		}

		assert(texFormat >= eTF_BC1 && texFormat <= eTF_BC7);

		int nSizeDxtMip0 = gRenDev->GetTextureFormatDataSize(width, height, 1, 1, texFormat, eTM_None);
		int nSizeMip0 = gRenDev->GetTextureFormatDataSize(width, height, 1, 1, eTF_R8G8B8A8, eTM_None);

		textureData.PreAllocate(nSizeMip0 / sizeof(ColorB), nSizeMip0 / sizeof(ColorB));

		gRenDev->DXTDecompress(pSrcData, nSizeDxtMip0, (byte*)textureData.GetElements(), width, height, 1, texFormat, false, 4);

		if (m_bIsSRGB)
		{
			for (int i = 0; i < textureData.Count(); i++)
			{
				ColorF colF = textureData[i];
				colF.srgb2rgb();
				textureData[i] = colF;
			}
		}
	}

	return !textureData.IsEmpty();
}

#endif // TEXTURE_GET_SYSTEM_COPY_SUPPORT
