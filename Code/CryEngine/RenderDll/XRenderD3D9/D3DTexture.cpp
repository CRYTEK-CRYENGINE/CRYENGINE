// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DTexture.cpp : Direct3D specific texture manager implementation.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryString/StringUtils.h>
#include <Cry3DEngine/ImageExtensionHelper.h>
#include <CrySystem/Scaleform/IFlashPlayer.h>
#include <CryCore/BitFiddling.h>    // ConvertBlock3DcToDXT5()
#include "D3DStereo.h"
#include "../Common/PostProcess/PostProcessUtils.h"
#include "D3DPostProcess.h"
#include "../Common/Textures/TextureHelpers.h"
#include "GraphicsPipeline/Common/UtilityPasses.h"
#include "DeviceManager/DeviceFormats.h" // SPixFormat

#undef min
#undef max

//===============================================================================

static bool DecompressSubresourcePayload(const STextureLayout& pLayout, const ETEX_TileMode eSrcTileMode, const void* pData[], STexturePayload& pPayload)
{
	assert(!pPayload.m_pSysMemSubresourceData);
	if (!pData || !pData[0])
	{
		pPayload.m_pSysMemSubresourceData = nullptr;
		pPayload.m_eSysMemTileMode = eTM_Unspecified;

		return false;
	}

	const int nSlices = pLayout.m_nArraySize;
	const int nMips = pLayout.m_nMips;

	SSubresourcePayload* InitData = new SSubresourcePayload[nMips * nSlices];

	// Contrary to hardware here the sub-resources are sorted by mips, then by slices.
	// In hardware it's sorted by slices, then by mips (so same mips are consecutive).
	const byte* pAutoData;
	const void** pDataCursor = pData;

	for (int nSlice = 0, nSubresource = 0; (nSlice < nSlices); ++nSlice)
	{
		int w = pLayout.m_nWidth;
		int h = pLayout.m_nHeight;
		int d = pLayout.m_nDepth;

		for (int nMip = 0; (nMip < nMips) && (w | h | d); ++nMip, ++nSubresource)
		{
			if (!w) w = 1;
			if (!h) h = 1;
			if (!d) d = 1;

			if (*pDataCursor)
				pAutoData = reinterpret_cast<const byte*>(*pDataCursor++);
			else
				pAutoData = pAutoData + InitData[nSubresource - 1].m_sSysMemAlignment.volumeStride;

			InitData[nSubresource].m_pSysMem = pAutoData;
			InitData[nSubresource].m_sSysMemAlignment.typeStride   = CTexture::TextureDataSize(1, 1, 1, 1, 1, pLayout.m_eSrcFormat, eSrcTileMode);
			InitData[nSubresource].m_sSysMemAlignment.rowStride    = CTexture::TextureDataSize(w, 1, 1, 1, 1, pLayout.m_eSrcFormat, eSrcTileMode);
			InitData[nSubresource].m_sSysMemAlignment.planeStride  = CTexture::TextureDataSize(w, h, 1, 1, 1, pLayout.m_eSrcFormat, eSrcTileMode);
			InitData[nSubresource].m_sSysMemAlignment.volumeStride = CTexture::TextureDataSize(w, h, d, 1, 1, pLayout.m_eSrcFormat, eSrcTileMode);

			w >>= 1;
			h >>= 1;
			d >>= 1;
		}

		// reset to face 0
		if (pLayout.m_eFlags & FT_REPLICATE_TO_ALL_SIDES)
			pDataCursor = pData;
		// each face is nullptr-terminated
		else if (!*pDataCursor)
			pDataCursor++;
	}

	pPayload.m_pSysMemSubresourceData = InitData;
	pPayload.m_eSysMemTileMode = eSrcTileMode;

	return true;
}

#if defined(TEXTURE_GET_SYSTEM_COPY_SUPPORT)
byte* CTexture::Convert(byte* pSrc, int nWidth, int nHeight, int nMips, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nOutMips, int& nOutSize, bool bLinear)
{
	nOutSize = 0;
	byte* pDst = NULL;
	if (eTFSrc == eTFDst && nMips == nOutMips)
		return pSrc;
	CD3D9Renderer* r = gcpRendD3D;
	DXGI_FORMAT DeviceFormatSRC = (DXGI_FORMAT)DeviceFormats::ConvertFromTexFormat(eTFSrc);
	if (eTFDst == eTF_L8)
		eTFDst = eTF_A8;
	DXGI_FORMAT DeviceFormatDST = (DXGI_FORMAT)DeviceFormats::ConvertFromTexFormat(eTFDst);
	if (DeviceFormatSRC == DXGI_FORMAT_UNKNOWN || DeviceFormatDST == DXGI_FORMAT_UNKNOWN)
	{
		assert(0);
		return NULL;
	}
	if (nMips <= 0)
		nMips = 1;
	int nSizeDSTMips = 0;
	int i;

	if (eTFSrc == eTF_BC5S)
	{
		assert(0);
		return NULL;
	}
	else if (eTFSrc == eTF_BC5U)
	{
		if (eTFDst == eTF_BC3)
		{
			// convert 3Dc to DXT5 - for compressed normals
			if (!nOutMips)
				nOutMips = nMips;
			int wdt = nWidth;
			int hgt = nHeight;
			nSizeDSTMips = CTexture::TextureDataSize(wdt, hgt, 1, nOutMips, 1, eTF_BC3);
			pDst = new byte[nSizeDSTMips];
			int nOffsDST = 0;
			int nOffsSRC = 0;
			for (i = 0; i < nOutMips; i++)
			{
				if (i >= nMips)
					break;
				if (wdt <= 0)
					wdt = 1;
				if (hgt <= 0)
					hgt = 1;
				void* outSrc = &pSrc[nOffsSRC];
				DWORD outSize = CTexture::TextureDataSize(wdt, hgt, 1, 1, 1, eTFDst);

				nOffsSRC += CTexture::TextureDataSize(wdt, hgt, 1, 1, 1, eTFSrc);

				{
					byte* src = (byte*)outSrc;

					for (uint32 n = 0; n < outSize / 16; n++)     // for each block
					{
						uint8* pSrcBlock = &src[n * 16];
						uint8* pDstBlock = &pDst[nOffsDST + n * 16];

						ConvertBlock3DcToDXT5(pDstBlock, pSrcBlock);
					}

					nOffsDST += outSize;
				}

				wdt >>= 1;
				hgt >>= 1;
			}
			if ((nOutMips > 1 && nOutMips != nMips))
			{
				assert(0);      // generate mips if needed - should not happen
				return 0;
			}
			nOutSize = nSizeDSTMips;
			return pDst;
		}
		else
		{
			assert(0);
			return NULL;
		}
	}
	else if (DDSFormats::IsNormalMap(eTFDst))
	{
		assert(0);
		return NULL;
	}
	nOutSize = nSizeDSTMips;
	return pDst;
}
#endif //#if CRY_PLATFORM_WINDOWS

D3DSurface* CTexture::GetSurface(int nCMSide, int nLevel)
{
	if (!m_pDevTexture)
		return NULL;

	if (DeviceFormats::IsTypeless(m_pPixelFormat->DeviceFormat))
	{
		iLog->Log("Error: CTexture::GetSurface: typeless formats can't be specified for RTVs, failed to create surface for the texture %s", GetSourceName());
		return NULL;
	}

	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "Create Render Target: %s", GetSourceName());

	const int nMipLevel = (m_eFlags & FT_FORCE_MIPS) ? min((int)(m_nMips - 1), nLevel) : 0;
	const int nSlice = (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray ? nCMSide : 0);
	const int nSliceCount = (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray ? 1 : -1);

	return (D3DSurface*)GetResourceView(SResourceView::RenderTargetView(DeviceFormats::ConvertFromTexFormat(m_eDstFormat), nSlice, nSliceCount, nMipLevel, IsMSAA()));
}

D3DSurface* CTexture::GetSurface(int nCMSide, int nLevel) const
{
	if (!m_pDevTexture)
		return NULL;

	if (DeviceFormats::IsTypeless(m_pPixelFormat->DeviceFormat))
	{
		iLog->Log("Error: CTexture::GetSurface: typeless formats can't be specified for RTVs, failed to create surface for the texture %s", GetSourceName());
		return NULL;
	}

	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "Create Render Target: %s", GetSourceName());

	const int nMipLevel = (m_eFlags & FT_FORCE_MIPS) ? min((int)(m_nMips - 1), nLevel) : 0;
	const int nSlice = (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray ? nCMSide : 0);
	const int nSliceCount = (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray ? 1 : -1);

	return (D3DSurface*)GetResourceView(SResourceView::RenderTargetView(DeviceFormats::ConvertFromTexFormat(m_eDstFormat), nSlice, nSliceCount, nMipLevel, IsMSAA()));
}

//===============================================================================

// Resolve anti-aliased RT to the texture
bool CTexture::Resolve(int nTarget, bool bUseViewportSize)
{
	if (m_bResolved)
		return true;

	m_bResolved = true;
	if (!(m_eFlags & FT_USAGE_MSAA))
		return true;

	assert((m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL)) && (m_eFlags & FT_USAGE_MSAA) && GetDevTexture() && GetDevTexture()->GetMSAATexture());
	CDeviceTexture* pDestSurf = GetDevTexture();
	CDeviceTexture* pSrcSurf = pDestSurf->GetMSAATexture();

	assert(pSrcSurf != NULL);
	assert(pDestSurf != NULL);

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
	gcpRendD3D->GetDeviceContext().ResolveSubresource(pDestSurf->Get2DTexture(), 0, pSrcSurf->Get2DTexture(), 0, (DXGI_FORMAT)m_pPixelFormat->DeviceFormat);
#endif
	return true;
}

bool CTexture::CreateDeviceTexture(const void* pData[])
{
	CRY_ASSERT(m_pPixelFormat);
	CRY_ASSERT(m_eDstFormat != eTF_Unknown);
	CRY_ASSERT(!pData || !*pData || m_eSrcTileMode != eTM_Unspecified);

	if (gRenDev->m_pRT->RC_CreateDeviceTexture(this, pData))
	{
		// Assign name to Texture for enhanced debugging
#if !defined(RELEASE) && (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_ORBIS)
	#if CRY_RENDERER_VULKAN || CRY_PLATFORM_ORBIS
		m_pDevTexture->GetBaseTexture()->DebugSetName(m_SrcName.c_str());
	#else
		m_pDevTexture->GetBaseTexture()->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(m_SrcName.c_str()), m_SrcName.c_str());
	#endif
#endif

		return true;
	}

	return false;
}

bool CTexture::CreateDeviceTexture(D3DResource* pTex)
{
	CRY_ASSERT(m_pPixelFormat);
	CRY_ASSERT(m_eDstFormat != eTF_Unknown);

	if (gRenDev->m_pRT->RC_CreateDeviceTexture(this, pTex))
	{
		// Assign name to Texture for enhanced debugging
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
		m_pDevTexture->GetBaseTexture()->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(m_SrcName.c_str()), m_SrcName.c_str());
#elif !defined(RELEASE) && CRY_PLATFORM_ORBIS && !CRY_RENDERER_GNM
		static_cast<CCryDXOrbisTexture*>(m_pDevTexture->GetBaseTexture())->DebugSetName(m_SrcName.c_str());
#endif

		return true;
	}

	return false;
}

void CTexture::Unbind()
{
	CDeviceTexture* pDevTex = m_pDevTexture;

	if (pDevTex)
		pDevTex->Unbind();
}

bool CTexture::RT_CreateDeviceTexture(const void* pData[])
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Texture, 0, "Creating Texture");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "%s %ix%ix%i %08x", m_SrcName.c_str(), m_nWidth, m_nHeight, m_nMips, m_eFlags);
	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	if (m_eFlags & FT_USAGE_MSAA)
		m_bResolved = false;

	m_nMinMipVidActive = 0;

	//if we have any device owned resources allocated, we must sync with render thread
	if (m_pDevTexture)
	{
		ReleaseDeviceTexture(false);
	}

	// The payload can only be passed untiled currently
	STextureLayout TL = GetLayout();
	STexturePayload TI;

	CRY_ASSERT(!pData || !*pData || m_eSrcTileMode != eTM_Unspecified);
	bool bHasPayload = DecompressSubresourcePayload(TL, m_eSrcTileMode, pData, TI);
	if (!(m_pDevTexture = CDeviceTexture::Create(TL, bHasPayload ? &TI : nullptr)))
		return false;

	SetTexStates();

	assert(!IsStreamed());

	{
		m_nDevTextureSize = m_nPersistentSize = m_pDevTexture->GetDeviceSize();

		volatile size_t* pTexMem = &CTexture::s_nStatsCurManagedNonStreamedTexMem;
		if (IsDynamic())
			pTexMem = &CTexture::s_nStatsCurDynamicTexMem;
		
		CryInterlockedAdd(pTexMem, m_nDevTextureSize);
	}

	// Notify that resource is dirty
	InvalidateDeviceResource(this, eDeviceResourceDirty | eDeviceResourceViewDirty);

	if (!pData || !pData[0])
		return true;

	if (m_eTT == eTT_3D)
		m_bIsSRGB = false;

	SetWasUnload(false);

	return true;
}

bool CTexture::RT_CreateDeviceTexture(D3DResource* pNatTex)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Texture, 0, "Creating Texture");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "%s %ix%ix%i %08x", m_SrcName.c_str(), m_nWidth, m_nHeight, m_nMips, m_eFlags);
	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	if (m_eFlags & FT_USAGE_MSAA)
		m_bResolved = false;

	m_nMinMipVidActive = 0;

	//if we have any device owned resources allocated, we must sync with render thread
	if (m_pDevTexture)
	{
		ReleaseDeviceTexture(false);
	}

	m_pDevTexture = CDeviceTexture::Associate(GetLayout(), pNatTex);

	SetTexStates();

	assert(!IsStreamed());
	if (m_pDevTexture)
	{
#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		m_nDeviceAddressInvalidated = m_pDevTexture->GetBaseAddressInvalidated();
#endif

		m_nDevTextureSize = m_nPersistentSize = m_pDevTexture->GetDeviceSize();

		volatile size_t* pTexMem = &CTexture::s_nStatsCurManagedNonStreamedTexMem;
		if (IsDynamic())
			pTexMem = &CTexture::s_nStatsCurDynamicTexMem;

		CryInterlockedAdd(pTexMem, m_nDevTextureSize);
	}

	// Notify that resource is dirty
	InvalidateDeviceResource(this, eDeviceResourceDirty | eDeviceResourceViewDirty);

	if (!pNatTex)
		return true;

	if (m_eTT == eTT_3D)
		m_bIsSRGB = false;

	SetWasUnload(false);

	return true;
}

void CTexture::ReleaseDeviceTexture(bool bKeepLastMips, bool bFromUnload)
{
	PROFILE_FRAME(CTexture_ReleaseDeviceTexture);

	if (!gcpRendD3D->m_pRT->IsRenderThread())
	{
		if (!gcpRendD3D->m_pRT->IsMainThread())
			CryFatalError("Texture is deleted from non-main and non-render thread, which causes command buffer corruption!");

		// Push to render thread to process
		gcpRendD3D->m_pRT->RC_ReleaseDeviceTexture(this);
		return;
	}

	Unbind();

	if (!bFromUnload)
		AbortStreamingTasks(this);

	if (s_pTextureStreamer)
		s_pTextureStreamer->OnTextureDestroy(this);

	if (!m_bNoTexture)
	{
		// The responsible code-path for deconstruction is the m_pFileTexMips->m_pPoolItem's dtor, if either of these is set
		if (!m_pFileTexMips || !m_pFileTexMips->m_pPoolItem)
		{
			CDeviceTexture* pDevTex = m_pDevTexture;
			if (pDevTex)
				pDevTex->SetOwner(NULL);

			if (IsStreamed())
			{
				SAFE_DELETE(pDevTex);      // for manually created textures
			}
			else
			{
				SAFE_RELEASE(pDevTex);

				volatile size_t* pTexMem = &CTexture::s_nStatsCurManagedNonStreamedTexMem;
				if (IsDynamic())
					pTexMem = &CTexture::s_nStatsCurDynamicTexMem;

				assert(*pTexMem >= m_nDevTextureSize);
				CryInterlockedAdd(pTexMem, -m_nDevTextureSize);
			}
		}

		if (m_pFileTexMips)
		{
			m_bStreamPrepared = false;
			StreamRemoveFromPool();
			if (bKeepLastMips)
			{
				const int nLastMipsStart = m_nMips - m_CacheFileHeader.m_nMipsPersistent;
				int nSides = StreamGetNumSlices();
				for (int nS = 0; nS < nSides; nS++)
				{
					for (int i = 0; i < nLastMipsStart; i++)
					{
						SMipData* mp = &m_pFileTexMips->m_pMipHeader[i].m_Mips[nS];
						mp->Free();
					}
				}
			}
			else
			{
				Unlink();
				StreamState_ReleaseInfo(this, m_pFileTexMips);
				m_pFileTexMips = NULL;
				m_bStreamed = false;
				SetStreamingInProgress(InvalidStreamSlot);
				m_bStreamRequested = false;
			}
		}

		m_nDevTextureSize = 0;
		m_nPersistentSize = 0;
	}

	m_pDevTexture = nullptr;
	m_bNoTexture = false;
}

ETEX_Format CTexture::GetClosestFormatSupported(ETEX_Format eTFDst, const SPixFormat*& pPF)
{
	return gcpRendD3D->m_hwTexFormatSupport.GetClosestFormatSupported(eTFDst, pPF);
}

ETEX_Format CTexture::SetClosestFormatSupported()
{
	if (m_eSrcFormat != eTF_Unknown)
		return m_eDstFormat = gcpRendD3D->m_hwTexFormatSupport.GetClosestFormatSupported(m_eSrcFormat, m_pPixelFormat);
	else
		return m_eDstFormat = eTF_Unknown;
}

void CTexture::SetTexStates()
{
	SSamplerState s;

	const bool noMipFiltering = m_nMips <= 1 && !(m_eFlags & FT_FORCE_MIPS);
	s.m_nMinFilter = FILTER_LINEAR;
	s.m_nMagFilter = FILTER_LINEAR;
	s.m_nMipFilter = noMipFiltering ? FILTER_NONE : FILTER_LINEAR;

	const ESamplerAddressMode addrMode = (m_eFlags & FT_STATE_CLAMP || m_eTT == eTT_Cube) ? eSamplerAddressMode_Clamp : eSamplerAddressMode_Wrap;
	s.SetClampMode(addrMode, addrMode, addrMode);

	m_nDefState = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(s);
}

void SSamplerState::SetComparisonFilter(bool bEnable)
{
	m_bComparison = bEnable;
}

void SSamplerState::SetClampMode(ESamplerAddressMode nAddressU, ESamplerAddressMode nAddressV, ESamplerAddressMode nAddressW)
{
	m_nAddressU = nAddressU;
	m_nAddressV = nAddressV;
	m_nAddressW = nAddressW;
}

bool SSamplerState::SetFilterMode(int nFilter)
{
	IF (nFilter < 0, 0)
		nFilter = FILTER_TRILINEAR;

	switch (nFilter)
	{
	case FILTER_POINT:
	case FILTER_NONE:
		m_nMinFilter = FILTER_POINT;
		m_nMagFilter = FILTER_POINT;
		m_nMipFilter = FILTER_NONE;
		m_nAnisotropy = 1;
		break;
	case FILTER_LINEAR:
		m_nMinFilter = FILTER_LINEAR;
		m_nMagFilter = FILTER_LINEAR;
		m_nMipFilter = FILTER_NONE;
		m_nAnisotropy = 1;
		break;
	case FILTER_BILINEAR:
		m_nMinFilter = FILTER_LINEAR;
		m_nMagFilter = FILTER_LINEAR;
		m_nMipFilter = FILTER_POINT;
		m_nAnisotropy = 1;
		break;
	case FILTER_TRILINEAR:
		m_nMinFilter = FILTER_LINEAR;
		m_nMagFilter = FILTER_LINEAR;
		m_nMipFilter = FILTER_LINEAR;
		m_nAnisotropy = 1;
		break;
	case FILTER_ANISO2X:
	case FILTER_ANISO4X:
	case FILTER_ANISO8X:
	case FILTER_ANISO16X:
		m_nMinFilter = nFilter;
		m_nMagFilter = nFilter;
		m_nMipFilter = nFilter;
		if (nFilter == FILTER_ANISO2X)
			m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 2);
		else if (nFilter == FILTER_ANISO4X)
			m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 4);
		else if (nFilter == FILTER_ANISO8X)
			m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 8);
		else if (nFilter == FILTER_ANISO16X)
			m_nAnisotropy = min(gcpRendD3D->m_MaxAnisotropyLevel, 16);
		break;
	default:
		assert(0);
		return false;
	}
	return true;
}

void SSamplerState::SetBorderColor(DWORD dwColor)
{
	m_dwBorderColor = dwColor;
}

void SSamplerState::SetMipLodBias(float fMipLodBias)
{
	m_fMipLodBias = fMipLodBias;
}

// TODO: deprecate these functions
void SSamplerState::SetDefaultClampingMode(ESamplerAddressMode nAddressU, ESamplerAddressMode nAddressV, ESamplerAddressMode nAddressW)
{
	s_sDefState.SetClampMode(nAddressU, nAddressV, nAddressW);
}

bool SSamplerState::SetDefaultFilterMode(int nFilter)
{
	return s_sDefState.SetFilterMode(nFilter);
}

void CTexture::UpdateTexStates()
{
	m_nDefState = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState::s_sDefState);
}

void CTexture::SetSampler(SamplerStateHandle nTS, int nSUnit, EHWShaderClass eHWSC)
{
	FUNCTION_PROFILER_RENDER_FLAT
	  assert(gcpRendD3D->m_pRT->IsRenderThread());
	STexStageInfo* const __restrict TexStages = s_TexStages;

	if (s_TexStateIDs[eHWSC][nSUnit] != nTS)
	{
		D3DSamplerState* pSamp = CDeviceObjectFactory::LookupSamplerState(nTS).second;

		assert(pSamp);

		if (pSamp)
		{
			s_TexStateIDs[eHWSC][nSUnit] = nTS;

			if (eHWSC == eHWSC_Pixel)
				gcpRendD3D->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_PS, &pSamp, nSUnit, 1);
			else if (eHWSC == eHWSC_Domain)
				gcpRendD3D->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_DS, &pSamp, nSUnit, 1);
			else if (eHWSC == eHWSC_Hull)
				gcpRendD3D->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_HS, &pSamp, nSUnit, 1);
			else if (eHWSC == eHWSC_Vertex)
				gcpRendD3D->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_VS, &pSamp, nSUnit, 1);
			else if (eHWSC == eHWSC_Compute)
				gcpRendD3D->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_CS, &pSamp, nSUnit, 1);
			else
				assert(0);
		}
	}
}

/* DEPRECATED: not compatible with DX12 resource sets:

   static int sRecursion = 0;

   #if !defined(_RELEASE)
   if (!sRecursion)
   {
    if (CRenderer::CV_r_detailtextures==0 && nTexMatSlot==EFTT_DETAIL_OVERLAY)
      {
        sRecursion++;
      CTexture::s_ptexDefaultMergedDetail->Apply(nTUnit);
        sRecursion--;
        return;
      }
   if (CRenderer::CV_r_texbindmode)
   {
      if (CRenderer::CV_r_texbindmode>=1)
      {
        if (CRenderer::CV_r_texbindmode==1 && !(m_nFlags & FT_TEX_FONT))
      {
        sRecursion++;
          CTexture::s_ptexNoTexture->Apply(nTUnit);
        sRecursion--;
        return;
      }
   }
      else if (CRenderer::CV_r_texbindmode==3 && (m_nFlags & FT_FROMIMAGE) && !(m_nFlags & FT_DONT_RELEASE))
      {
        sRecursion++;
        CTexture::s_ptexGray->Apply(nTUnit);
        sRecursion--;
        return;
      }
      else if (CRenderer::CV_r_texbindmode==4 && (m_nFlags & FT_FROMIMAGE) && !(m_nFlags & FT_DONT_RELEASE) && m_eTT == eTT_2D)
      {
        sRecursion++;
        CTexture::s_ptexGray->Apply(nTUnit);
        sRecursion--;
        return;
      }
      else if (CRenderer::CV_r_texbindmode==5 && (m_nFlags & FT_FROMIMAGE) && !(m_nFlags & FT_DONT_RELEASE) && (m_nFlags & FT_TEX_NORMAL_MAP))
      {
        sRecursion++;
        CTexture::s_ptexFlatBump->Apply(nTUnit);
        sRecursion--;
        return;
      }
      else if (CRenderer::CV_r_texbindmode==6 && (nTexMatSlot==EFTT_DIFFUSE || nTexMatSlot==EFTT_CUSTOM))
      {
        sRecursion++;
        CTexture::s_ptexWhite->Apply(nTUnit);
        sRecursion--;
        return;
      }
      else if (CRenderer::CV_r_texbindmode==7 && nTexMatSlot==EFTT_DIFFUSE)
      {
        sRecursion++;
        CTexture::s_ptexMipMapDebug->Apply(nTUnit);
        sRecursion--;
        return;
      }
      else if (CRenderer::CV_r_texbindmode==8 && nTexMatSlot==EFTT_DIFFUSE)
      {
        assert(IsStreamed());
        sRecursion++;
        Apply(nTUnit);
        if(m_pFileTexMips)
        {
          switch (m_nMinMipVidUploaded)
          {
          case 0:
            CTexture::s_ptexColorGreen->Apply(nTUnit);
            break;
          case 1:
            CTexture::s_ptexColorCyan->Apply(nTUnit);
            break;
          case 2:
            CTexture::s_ptexColorBlue->Apply(nTUnit);
            break;
          case 3:
            CTexture::s_ptexColorPurple->Apply(nTUnit);
            break;
          case 4:
            CTexture::s_ptexColorMagenta->Apply(nTUnit);
            break;
          case 5:
            CTexture::s_ptexColorYellow->Apply(nTUnit);
            break;
          case 6:
            CTexture::s_ptexColorOrange->Apply(nTUnit);
            break;
          case 7:
            CTexture::s_ptexColorRed->Apply(nTUnit);
            break;
          default:
            CTexture::s_ptexColorWhite->Apply(nTUnit);
            break;
          }
        }
        sRecursion--;
        return;
      }
      else
      if (CRenderer::CV_r_texbindmode==9 && nTexMatSlot==EFTT_DIFFUSE)
      {
        sRecursion++;
        Apply(nTUnit);
        if (IsStreaming() && m_pFileTexMips)
        {
          assert(IsStreamed());
          if(m_nMinMipVidUploaded < GetRequiredMipNonVirtual())
          {
            CTexture::s_ptexColorRed->Apply(nTUnit);
          }
          else if(m_nMinMipVidUploaded > GetRequiredMipNonVirtual())
          {
            CTexture::s_ptexColorGreen->Apply(nTUnit);
          }
        }
        sRecursion--;
        return;
      }
      else
      if (CRenderer::CV_r_texbindmode==10 && nTexMatSlot==EFTT_DIFFUSE)
      {
        sRecursion++;
        Apply(nTUnit);
        if (m_pFileTexMips)
        {
          int nCurMip = m_fpMinMipCur >> 8;
          if(nCurMip < -2)
          {
            CTexture::s_ptexColorRed->Apply(nTUnit);
          }
          else if(nCurMip < -1)
          {
            CTexture::s_ptexColorYellow->Apply(nTUnit);
          }
          else if(nCurMip < 0)
          {
            CTexture::s_ptexColorGreen->Apply(nTUnit);
          }
        }
        sRecursion--;
        return;
      }
      else if (CRenderer::CV_r_texbindmode==11)
      {
        if (nTexMatSlot==EFTT_DIFFUSE)
        {
          sRecursion++;
          CTexture::s_ptexWhite->Apply(nTUnit);
          sRecursion--;
          return;
        }
        if ((m_nFlags & FT_FROMIMAGE) && !(m_nFlags & FT_DONT_RELEASE) && (m_nFlags & FT_TEX_NORMAL_MAP))
        {
          sRecursion++;
          CTexture::s_ptexFlatBump->Apply(nTUnit);
          sRecursion--;
          return;
        }
      }
      else
      if (CRenderer::CV_r_texbindmode==12 && nTexMatSlot==EFTT_DIFFUSE)
      {
        sRecursion++;
        Apply(nTUnit);
        if (m_pFileTexMips)
        {
          assert(IsStreamed());
          if(m_nMinMipVidUploaded < GetRequiredMipNonVirtual())
          {
            CTexture::s_ptexColorGreen->Apply(nTUnit);
          }
          else if(m_nMinMipVidUploaded > GetRequiredMipNonVirtual())
          {
            CTexture::s_ptexColorRed->Apply(nTUnit);
          }
        }
        sRecursion--;
        return;
      }
        }
   #endif
        }

 */

void CTexture::ApplySampler(int nSUnit, EHWShaderClass eHWSC, SamplerStateHandle eState)
{
	FUNCTION_PROFILER_RENDER_FLAT

	SamplerStateHandle nTSSel = (eState == EDefaultSamplerStates::Unspecified ? m_nDefState : eState);
	assert(nTSSel != EDefaultSamplerStates::Unspecified);

	STexStageInfo* TexStages = s_TexStages;
	CDeviceTexture* pDevTex = m_pDevTexture;

	// avoiding L2 cache misses from usage from up ahead
	PrefetchLine(pDevTex, 0);

	assert(nSUnit >= 0 && nSUnit < 16);
	assert((nSUnit >= 0 || nSUnit == -2) && nSUnit < gcpRendD3D->m_NumSamplerSlots);

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const int nFrameID = rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_nFrameUpdateID;

	if (IsVertexTexture())
		eHWSC = eHWSC_Vertex;

	SetSampler(nTSSel, nSUnit, eHWSC);
}

void CTexture::ApplyTexture(int nTUnit, EHWShaderClass eHWSC /*= eHWSC_Pixel*/, ResourceViewHandle hView /*= EDefaultResourceViews::Default*/, bool bMSAA /*= false*/)
{
	FUNCTION_PROFILER_RENDER_FLAT

	STexStageInfo* TexStages = s_TexStages;

	assert(nTUnit >= 0 && nTUnit < gcpRendD3D->m_NumResourceSlots);

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const int nFrameID = rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_nFrameUpdateID;

	if (IsStreamed() && !m_bPostponed)
	{
		bool bIsUnloaded = IsUnloaded();

		assert(m_pFileTexMips);
		if (bIsUnloaded || !m_bStreamPrepared || IsPartiallyLoaded())
		{
			PROFILE_FRAME(Texture_Precache);
			if (!CRenderer::CV_r_texturesstreaming || !m_bStreamPrepared || bIsUnloaded)
			{
				if (bIsUnloaded)
					StreamLoadFromCache(0);
				else
					Load(m_eDstFormat);
			}
		}
	}

	CDeviceTexture* pDevTex = GetDevTexture(bMSAA);

	// avoiding L2 cache misses from usage from up ahead
	PrefetchLine(pDevTex, 0);

	IF (this != CTexture::s_pTexNULL && (!pDevTex || !pDevTex->GetBaseTexture()), 0)
	{
		// apply black by default
		if (CTexture::s_ptexBlack && this != CTexture::s_ptexBlack)
		{
			CTexture::s_ptexBlack->ApplyTexture(nTUnit, eHWSC, hView, bMSAA);
		}
		return;
	}

	// Resolve multisampled RT to texture
	if (!m_bResolved)
		Resolve();

	bool bStreamed = IsStreamed();
	if (m_nAccessFrameID != nFrameID)
	{
		m_nAccessFrameID = nFrameID;

#if !defined(_RELEASE)
		rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_NumTextures++;
		if (m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC))
		{
			rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_DynTexturesSize += m_nDevTextureSize;
		}
		else
		{
			if (bStreamed)
			{
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesStreamVidSize += m_nDevTextureSize;
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesStreamSysSize += StreamComputeSysDataSize(0);
			}
			else
			{
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesVidMemSize += m_nDevTextureSize;
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesSysMemSize += m_nDevTextureSize;
			}
		}
#endif

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		// mip map fade in
		if (bStreamed && CRenderer::CV_r_texturesstreamingmipfading && (m_fCurrentMipBias != 0.0f) && !gEnv->IsEditor())
		{
			const float fMipFadeIn = (m_nMips / (float)CRenderer::CV_r_texturesstreamingmipfading); // how much Mip-Levels per tick
			const float fCurrentMipBias = m_fCurrentMipBias = max(0.0f, min(m_fCurrentMipBias, float(GetStreamableMipNumber())) - fMipFadeIn);

			gcpRendD3D->GetDeviceContext().SetResourceMinLOD(pDevTex->Get2DTexture(), fCurrentMipBias);
		}
#endif
	}

	if (IsVertexTexture())
		eHWSC = eHWSC_Vertex;

	const std::pair<SResourceView, CDeviceResourceView*>& pView = pDevTex->LookupResourceView(hView);
	const bool bUnnorderedAcessView = pView.first.m_Desc.eViewType == SResourceView::eUnorderedAccessView;

	{
		D3DShaderResource* pResView = reinterpret_cast<D3DShaderResource*>(pView.second);

		if (pDevTex == TexStages[nTUnit].m_DevTexture && pResView == TexStages[nTUnit].m_pCurResView && eHWSC == TexStages[nTUnit].m_eHWSC)
			return;

		TexStages[nTUnit].m_pCurResView = pResView;
		TexStages[nTUnit].m_eHWSC = eHWSC;
	}

	//	<DEPRECATED> This must get re-factored post C3.
	//	-	This check is ultra-buggy, render targets setup is deferred until last moment might not be matching this check at all. Also very wrong for MRTs

	if (rd->m_pCurTarget[0] == this)
	{
		//assert(rd->m_pCurTarget[0]->m_pDeviceRTV);
		rd->m_pNewTarget[0]->m_bWasSetRT = false;
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
		rd->GetDeviceContext().OMSetRenderTargets(1, &rd->m_pNewTarget[0]->m_pTarget, rd->m_pNewTarget[0]->m_pDepth);
#endif
	}

	TexStages[nTUnit].m_DevTexture = pDevTex;

#if !defined(_RELEASE)
	rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_NumTextChanges++;
#endif

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
	{
		if (IsNoTexture())
			rd->Logv("CTexture::Apply(): (%d) \"%s\" (Not found)\n", nTUnit, m_SrcName.c_str());
		else
			rd->Logv("CTexture::Apply(): (%d) \"%s\"\n", nTUnit, m_SrcName.c_str());
	}
#endif

	{
#ifdef DO_RENDERLOG
		if (CRenderer::CV_r_log >= 3 && int64(hView) >= EDefaultResourceViews::PreAllocated)
		{
			rd->Logv("CTexture::Apply(): Shader Resource View: %ul \n", GetDevTexture(bMSAA)->LookupResourceView(hView).first.m_Desc.Key);
		}
#endif

		if (bUnnorderedAcessView)
		{
			// todo:
			// - add support for pixel shader side via OMSetRenderTargetsAndUnorderedAccessViews
			// - DX11.1 very likely API will be similar to CSSetUnorderedAccessViews, but for all stages
			D3DUAV* pUAV = reinterpret_cast<D3DUAV*>(pView.second);

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			rd->GetDeviceContext().CSSetUnorderedAccessViews(nTUnit, 1, &pUAV, NULL);
#endif
			return;
		}

		{
			D3DShaderResource* pSRV = reinterpret_cast<D3DShaderResource*>(pView.second);

			if (IsVertexTexture())
				eHWSC = eHWSC_Vertex;

			if (eHWSC == eHWSC_Pixel)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_PS, pSRV, nTUnit);
			else if (eHWSC == eHWSC_Vertex)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_VS, pSRV, nTUnit);
			else if (eHWSC == eHWSC_Domain)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_DS, pSRV, nTUnit);
			else if (eHWSC == eHWSC_Hull)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_HS, pSRV, nTUnit);
			else if (eHWSC == eHWSC_Compute)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_CS, pSRV, nTUnit);
			else
				assert(0);
		}
	}
}

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
void CTexture::ValidateSRVs()
{
	// We should only end up here if the texture was moved due to a defrag. This should only be for static textures.
	m_pDevTexture->ReleaseResourceViews();
	m_pDevTexture->AllocatePredefinedResourceViews();
	// TODO: recreate all views that existed ...

	// Notify that resource is dirty
	InvalidateDeviceResource(this, eDeviceResourceDirty | eDeviceResourceViewDirty);
}
#endif

void CTexture::Apply(int nTUnit, SamplerStateHandle nState /*= EDefaultSamplerStates::Unspecified*/, int nTexMatSlot /*= EFTT_UNKNOWN*/, int nSUnit /*= -1*/, ResourceViewHandle hView /*= EDefaultResourceViews::Default*/, bool bMSAA /*= false*/, EHWShaderClass eHWSC /*= eHWSC_Pixel*/)
{
	FUNCTION_PROFILER_RENDER_FLAT

	assert(nTUnit >= 0);
	SamplerStateHandle nTSSel = (nState == EDefaultSamplerStates::Unspecified ? m_nDefState : nState);
	assert(nTSSel != EDefaultSamplerStates::Unspecified);

	STexStageInfo* TexStages = s_TexStages;

	if (nSUnit >= -1)
		nSUnit = Isel32(nSUnit, nTUnit);

	assert(nTUnit >= 0 && nTUnit < gcpRendD3D->m_NumResourceSlots);
	assert((nSUnit >= 0 || nSUnit == -2) && nSUnit < gcpRendD3D->m_NumSamplerSlots);

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const int nFrameID = rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_nFrameUpdateID;

	if (IsStreamed() && !m_bPostponed)
	{
		bool bIsUnloaded = IsUnloaded();

		assert(m_pFileTexMips);
		if (bIsUnloaded || !m_bStreamPrepared || IsPartiallyLoaded())
		{
			PROFILE_FRAME(Texture_Precache);
			if (!CRenderer::CV_r_texturesstreaming || !m_bStreamPrepared || bIsUnloaded)
			{
				if (bIsUnloaded)
					StreamLoadFromCache(0);
				else
					Load(m_eDstFormat);
			}
		}

		if (nTexMatSlot != EFTT_UNKNOWN)
			m_nStreamingPriority = max((int8)m_nStreamingPriority, TextureHelpers::LookupTexPriority((EEfResTextures)nTexMatSlot));
	}

	CDeviceTexture* pDevTex = GetDevTexture(bMSAA);

	// avoiding L2 cache misses from usage from up ahead
	PrefetchLine(pDevTex, 0);

	IF (this != CTexture::s_pTexNULL && (!pDevTex || !pDevTex->GetBaseTexture()), 0)
	{
		// apply black by default
		if (CTexture::s_ptexBlack && this != CTexture::s_ptexBlack)
		{
			CTexture::s_ptexBlack->Apply(nTUnit, nState, nTexMatSlot, nSUnit, hView);
		}
		return;
	}

	// Resolve multisampled RT to texture
	if (!m_bResolved)
		Resolve();

	bool bStreamed = IsStreamed();
	if (m_nAccessFrameID != nFrameID)
	{
		m_nAccessFrameID = nFrameID;

#if !defined(_RELEASE)
		rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_NumTextures++;
		if (m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC))
		{
			rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_DynTexturesSize += m_nDevTextureSize;
		}
		else
		{
			if (bStreamed)
			{
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesStreamVidSize += m_nDevTextureSize;
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesStreamSysSize += StreamComputeSysDataSize(0);
			}
			else
			{
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesSysMemSize += m_nDevTextureSize;
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesVidMemSize += m_nDevTextureSize;
			}
		}
#endif

		// mip map fade in
#ifndef OPENGL
		if (bStreamed && CRenderer::CV_r_texturesstreamingmipfading && (m_fCurrentMipBias != 0.0f) && !gEnv->IsEditor())
		{
			const float fMipFadeIn = (m_nMips / (float)CRenderer::CV_r_texturesstreamingmipfading); // how much Mip-Levels per tick
			const float fCurrentMipBias = m_fCurrentMipBias = max(0.0f, min(m_fCurrentMipBias, float(GetStreamableMipNumber())) - fMipFadeIn);

	#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			gcpRendD3D->GetDeviceContext().SetResourceMinLOD(pDevTex->Get2DTexture(), fCurrentMipBias);
	#endif
		}
#endif
	}

	if (IsVertexTexture())
		eHWSC = eHWSC_Vertex;

	// Patch view to sRGB it isn't an sRGB format
	{
		const std::pair<SResourceView, CDeviceResourceView*>& pView = pDevTex->LookupResourceView(hView);
		const std::pair<SSamplerState, CDeviceSamplerState*>& pSamp = CDeviceObjectFactory::LookupSamplerState(nTSSel);

		if (pSamp.first.m_bSRGBLookup && !pView.first.m_Desc.bSrgbRead)
		{
			SResourceView sRGB = pView.first;
			sRGB.m_Desc.bSrgbRead = pSamp.first.m_bSRGBLookup;
			hView = pDevTex->GetOrCreateResourceViewHandle(sRGB);
		}
	}

	const std::pair<SResourceView, CDeviceResourceView*>& pView = pDevTex->LookupResourceView(hView);
	const bool bUnnorderedAcessView = pView.first.m_Desc.eViewType == SResourceView::eUnorderedAccessView;

	if (!bUnnorderedAcessView && nSUnit >= 0)
	{
		SetSampler(nTSSel, nSUnit, eHWSC);
	}

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	CheckValidateSRVs();
#endif

	{
		D3DShaderResource* pResView = reinterpret_cast<D3DShaderResource*>(pView.second);

		if (pDevTex == TexStages[nTUnit].m_DevTexture && pResView == TexStages[nTUnit].m_pCurResView && eHWSC == TexStages[nTUnit].m_eHWSC)
			return;

		TexStages[nTUnit].m_pCurResView = pResView;
		TexStages[nTUnit].m_eHWSC = eHWSC;
	}

	//	<DEPRECATED> This must get re-factored post C3.
	//	-	This check is ultra-buggy, render targets setup is deferred until last moment might not be matching this check at all. Also very wrong for MRTs

	if (rd->m_pCurTarget[0] == this)
	{
		//assert(rd->m_pCurTarget[0]->m_pDeviceRTV);
		rd->m_pNewTarget[0]->m_bWasSetRT = false;

		// NOTE: bottom of the stack is always the active swap-chain back-buffer
		if (rd->m_nRTStackLevel[0] == 0)
		{
			assert(rd->m_pNewTarget[0]->m_pTarget == (D3DSurface*)0xDEADBEEF);
			assert(rd->m_pNewTarget[0]->m_pDepth  == (D3DDepthSurface*)0xDEADBEEF);
			assert(rd->m_pNewTarget[1]->m_pTarget == nullptr);
			
			D3DSurface* pRTV = rd->GetCurrentTargetOutput()->GetDevTexture()->LookupRTV(EDefaultResourceViews::RenderTarget);
			D3DDepthSurface* pDSV = rd->GetCurrentDepthOutput()->GetDevTexture()->LookupDSV(EDefaultResourceViews::DepthStencil);

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			rd->GetDeviceContext().OMSetRenderTargets(1, &pRTV, !rd->m_pActiveContext || rd->m_pActiveContext->m_bMainViewport ? pDSV : nullptr);
#endif
		}
		else
		{
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			rd->GetDeviceContext().OMSetRenderTargets(1, &rd->m_pNewTarget[0]->m_pTarget, rd->m_pNewTarget[0]->m_pDepth);
#endif
		}
	}

	TexStages[nTUnit].m_DevTexture = pDevTex;

#if !defined(_RELEASE)
	rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_NumTextChanges++;
#endif

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log >= 3)
	{
		if (IsNoTexture())
			rd->Logv("CTexture::Apply(): (%d) \"%s\" (Not found)\n", nTUnit, m_SrcName.c_str());
		else
			rd->Logv("CTexture::Apply(): (%d) \"%s\"\n", nTUnit, m_SrcName.c_str());
	}
#endif

	{
#ifdef DO_RENDERLOG
		if (CRenderer::CV_r_log >= 3 && int64(hView) >= EDefaultResourceViews::PreAllocated)
		{
			rd->Logv("CTexture::Apply(): Shader Resource View: %ul \n", GetDevTexture(bMSAA)->LookupResourceView(hView).first.m_Desc.Key);
		}
#endif

		if (bUnnorderedAcessView)
		{
			// todo:
			// - add support for pixel shader side via OMSetRenderTargetsAndUnorderedAccessViews
			// - DX11.1 very likely API will be similar to CSSetUnorderedAccessViews, but for all stages
			D3DUAV* pUAV = reinterpret_cast<D3DUAV*>(pView.second);

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			rd->GetDeviceContext().CSSetUnorderedAccessViews(nTUnit, 1, &pUAV, NULL);
#endif
			return;
		}

		{
			D3DShaderResource* pSRV = reinterpret_cast<D3DShaderResource*>(pView.second);

			if (IsVertexTexture())
				eHWSC = eHWSC_Vertex;

			if (eHWSC == eHWSC_Pixel)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_PS, pSRV, nTUnit);
			else if (eHWSC == eHWSC_Vertex)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_VS, pSRV, nTUnit);
			else if (eHWSC == eHWSC_Domain)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_DS, pSRV, nTUnit);
			else if (eHWSC == eHWSC_Hull)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_HS, pSRV, nTUnit);
			else if (eHWSC == eHWSC_Compute)
				rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_CS, pSRV, nTUnit);
			else
				assert(0);
		}
	}
}

void CTexture::UpdateTextureRegion(byte* pSrcData, int nX, int nY, int nZ, int USize, int VSize, int ZSize, ETEX_Format eSrcFormat)
{
	gRenDev->m_pRT->RC_UpdateTextureRegion(this, pSrcData, nX, nY, nZ, USize, VSize, ZSize, eSrcFormat);
}

void CTexture::RT_UpdateTextureRegion(byte* pSrcData, int nX, int nY, int nZ, int USize, int VSize, int ZSize, ETEX_Format eSrcFormat)
{
	PROFILE_FRAME(UpdateTextureRegion);

	if (m_eTT != eTT_1D && m_eTT != eTT_2D && m_eTT != eTT_3D)
	{
		CRY_ASSERT(0);
		return;
	}

	HRESULT hr = S_OK;
	CDeviceTexture* pDevTex = GetDevTexture();
	assert(pDevTex);
	if (!pDevTex)
		return;

	GPUPIN_DEVICE_TEXTURE(gcpRendD3D->GetPerformanceDeviceContext(), pDevTex);

	if (eSrcFormat != m_eDstFormat)
	{
		D3DFormat frmtSrc = DeviceFormats::ConvertFromTexFormat(eSrcFormat);
		D3DFormat frmtDst = DeviceFormats::ConvertFromTexFormat(m_eDstFormat);
		frmtSrc = DeviceFormats::ConvertToTypeless(frmtSrc);
		frmtDst = DeviceFormats::ConvertToTypeless(frmtDst);
		if (frmtSrc != frmtDst)
		{
			CRY_ASSERT(0);
			return;
		}
	}

	CRY_ASSERT(m_eSrcTileMode == eTM_None || m_eSrcTileMode == eTM_Unspecified);
	CRY_ASSERT(m_nArraySize == 1);
	for (int i = 0; i < m_nMips; i++)
	{
		if (!USize)
			USize = 1;
		if (!VSize)
			VSize = 1;
		if (!ZSize)
			ZSize = 1;

		const SResourceMemoryMapping mapping =
		{
			{                                                    // src alignment == plain software alignment
				CTexture::TextureDataSize(1    , 1    , 1    , 1, 1, m_eDstFormat, eTM_None),
				CTexture::TextureDataSize(USize, 1    , 1    , 1, 1, m_eDstFormat, eTM_None),
				CTexture::TextureDataSize(USize, VSize, 1    , 1, 1, m_eDstFormat, eTM_None),
				CTexture::TextureDataSize(USize, VSize, ZSize, 1, 1, m_eDstFormat, eTM_None),
			},
			{ nX, nY, nZ, D3D11CalcSubresource(i, 0, m_nMips) }, // dst position
			{ USize, VSize, ZSize, 1 }                           // dst size
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pSrcData, pDevTex, mapping);

		pSrcData += mapping.MemoryLayout.volumeStride;

		USize >>= 1;
		VSize >>= 1;
		ZSize >>= 1;

		nX >>= 1;
		nY >>= 1;
		nZ >>= 1;
	}
}

bool CTexture::Clear()
{
	if (!(m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL)))
		return false;

	gRenDev->m_pRT->RC_ClearTarget(this, m_cClearColor);
	return true;
}

bool CTexture::Clear(const ColorF& cClear)
{
	if (!(m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL)))
		return false;

	gRenDev->m_pRT->RC_ClearTarget(this, cClear);
	return true;
}

void SEnvTexture::Release()
{
	ReleaseDeviceObjects();
	SAFE_DELETE(m_pTex);
}

void SEnvTexture::RT_SetMatrix()
{
	Matrix44A matView, matProj;
	gRenDev->GetModelViewMatrix(matView.GetData());
	gRenDev->GetProjectionMatrix(matProj.GetData());

	float fWidth = m_pTex ? (float)m_pTex->GetWidth() : 1;
	float fHeight = m_pTex ? (float)m_pTex->GetHeight() : 1;

	Matrix44A matScaleBias(0.5f, 0, 0, 0,
	                       0, -0.5f, 0, 0,
	                       0, 0, 0.5f, 0,
	                       // texel alignment - also push up y axis reflection up a bit
	                       0.5f + 0.5f / fWidth, 0.5f + 1.0f / fHeight, 0.5f, 1.0f);

	Matrix44A m = matProj * matScaleBias;
	Matrix44A mm = matView * m;
	m_Matrix = mm;
}

void SEnvTexture::ReleaseDeviceObjects()
{
	//if (m_pTex)
	//  m_pTex->ReleaseDynamicRT(true);
}

bool CTexture::RenderEnvironmentCMHDR(int size, Vec3& Pos, TArray<unsigned short>& vecData)
{
#if CRY_PLATFORM_DESKTOP

	iLog->Log("Start generating a cubemap...");

	vecData.SetUse(0);

	int vX, vY, vWidth, vHeight;
	gRenDev->GetViewport(&vX, &vY, &vWidth, &vHeight);

	const int nOldWidth = gRenDev->GetCurrentContextViewportWidth();
	const int nOldHeight = gRenDev->GetCurrentContextViewportHeight();
	bool bFullScreen = (iConsole->GetCVar("r_Fullscreen")->GetIVal() != 0) && (!gEnv->IsEditor());
	gRenDev->ChangeViewport(0, 0, size, size);

	int nPFlags = gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_PersFlags;

	CTexture* ptexGenEnvironmentCM = CTexture::GetOrCreate2DTexture("$GenEnvironmentCM", size, size, 1, FT_DONT_STREAM, nullptr, eTF_R16G16B16A16F);
	if (!ptexGenEnvironmentCM || !ptexGenEnvironmentCM->GetDevTexture())
	{
		iLog->Log("Failed generating a cubemap: out of video memory");
		gRenDev->ChangeViewport(0, 0, nOldWidth, nOldHeight);

		SAFE_RELEASE(ptexGenEnvironmentCM);
		return false;
	}

	// Disable/set cvars that can affect cube map generation. This is thread unsafe (we assume editor will not run in mt mode), no other way around at this time
	//	- coverage buffer unreliable for multiple views
	//	- custom view distance ratios

	ICVar* pCoverageBufferCV = gEnv->pConsole->GetCVar("e_CoverageBuffer");
	const int32 nCoverageBuffer = pCoverageBufferCV ? pCoverageBufferCV->GetIVal() : 0;
	if (pCoverageBufferCV)
		pCoverageBufferCV->Set(0);

	ICVar* pStatObjBufferRenderTasksCV = gEnv->pConsole->GetCVar("e_StatObjBufferRenderTasks");
	const int32 nStatObjBufferRenderTasks = pStatObjBufferRenderTasksCV ? pStatObjBufferRenderTasksCV->GetIVal() : 0;
	if (pStatObjBufferRenderTasksCV)
		pStatObjBufferRenderTasksCV->Set(0);

	ICVar* pViewDistRatioCV = gEnv->pConsole->GetCVar("e_ViewDistRatio");
	const float fOldViewDistRatio = pViewDistRatioCV ? pViewDistRatioCV->GetFVal() : 1.f;
	if (pViewDistRatioCV)
		pViewDistRatioCV->Set(10000.f);

	ICVar* pViewDistRatioVegetationCV = gEnv->pConsole->GetCVar("e_ViewDistRatioVegetation");
	const float fOldViewDistRatioVegetation = pViewDistRatioVegetationCV ? pViewDistRatioVegetationCV->GetFVal() : 100.f;
	if (pViewDistRatioVegetationCV)
		pViewDistRatioVegetationCV->Set(10000.f);

	ICVar* pLodRatioCV = gEnv->pConsole->GetCVar("e_LodRatio");
	const float fOldLodRatio = pLodRatioCV ? pLodRatioCV->GetFVal() : 1.f;
	if (pLodRatioCV)
		pLodRatioCV->Set(1000.f);

	Vec3 oldSunDir, oldSunStr, oldSunRGB;
	float oldSkyKm, oldSkyKr, oldSkyG;
	if (CRenderer::CV_r_HideSunInCubemaps)
	{
		gEnv->p3DEngine->GetSkyLightParameters(oldSunDir, oldSunStr, oldSkyKm, oldSkyKr, oldSkyG, oldSunRGB);
		gEnv->p3DEngine->SetSkyLightParameters(oldSunDir, oldSunStr, oldSkyKm, oldSkyKr, 1.0f, oldSunRGB, true); // Hide sun disc
	}

	const int32 nFlaresCV = CRenderer::CV_r_flares;
	CRenderer::CV_r_flares = 0;

	ICVar* pSSDOHalfResCV = gEnv->pConsole->GetCVar("r_ssdoHalfRes");
	const int nOldSSDOHalfRes = pSSDOHalfResCV ? pSSDOHalfResCV->GetIVal() : 1;
	if (pSSDOHalfResCV)
		pSSDOHalfResCV->Set(0);

	const int nDesktopWidth = gcpRendD3D->m_deskwidth;
	const int nDesktopHeight = gcpRendD3D->m_deskheight;
	gcpRendD3D->m_deskwidth = gcpRendD3D->m_deskheight = size;

	gcpRendD3D->EnableSwapBuffers(false);
	for (int nSide = 0; nSide < 6; nSide++)
	{
		gcpRendD3D->BeginFrame();
		gcpRendD3D->SetViewport(0, 0, size, size);

		gcpRendD3D->SetWidth(size);
		gcpRendD3D->SetHeight(size);

		gcpRendD3D->EF_ClearTargetsLater(FRT_CLEAR, Clr_Transparent);

		DrawSceneToCubeSide(Pos, size, nSide);

		CDeviceTexture* pDevTextureSrc = CTexture::s_ptexHDRTarget->GetDevTexture();
		CDeviceTexture* pDevTextureDst = ptexGenEnvironmentCM->GetDevTexture();
		CRY_ASSERT(CTexture::s_ptexHDRTarget->GetNumMips() == ptexGenEnvironmentCM->GetNumMips());

		GPUPIN_DEVICE_TEXTURE(gcpRendD3D->GetPerformanceDeviceContext(), pDevTextureDst);
		CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
		commandList.GetCopyInterface()->Copy(pDevTextureSrc, pDevTextureDst);

		CDeviceTexture* pDstDevTex = ptexGenEnvironmentCM->GetDevTexture();
		pDstDevTex->DownloadToStagingResource(0, [&](void* pData, uint32 rowPitch, uint32 slicePitch)
		{
			unsigned short* pTarg = (unsigned short*)pData;
			const uint32 nLineStride = CTexture::TextureDataSize(size, 1, 1, 1, 1, eTF_R16G16B16A16F) / sizeof(unsigned short);

			// Copy vertically flipped image
			for (uint32 nLine = 0; nLine < size; ++nLine)
				vecData.Copy(&pTarg[((size - 1) - nLine) * nLineStride], nLineStride);

			return true;
		});

		gcpRendD3D->EndFrame();
	}

	SAFE_RELEASE(ptexGenEnvironmentCM);

	// Restore previous states

	gcpRendD3D->m_deskwidth = nDesktopWidth;
	gcpRendD3D->m_deskheight = nDesktopHeight;
	gRenDev->ChangeViewport(0, 0, nOldWidth, nOldHeight);

	gcpRendD3D->EnableSwapBuffers(true);
	gcpRendD3D->SetWidth(vWidth);
	gcpRendD3D->SetHeight(vHeight);
	gcpRendD3D->RT_SetViewport(vX, vY, vWidth, vHeight);
	gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_PersFlags = nPFlags;
	gcpRendD3D->ResetToDefault();

	if (pCoverageBufferCV)
		pCoverageBufferCV->Set(nCoverageBuffer);

	if (pStatObjBufferRenderTasksCV)
		pStatObjBufferRenderTasksCV->Set(nStatObjBufferRenderTasks);

	if (pViewDistRatioCV)
		pViewDistRatioCV->Set(fOldViewDistRatio);

	if (pViewDistRatioVegetationCV)
		pViewDistRatioVegetationCV->Set(fOldViewDistRatioVegetation);

	if (pLodRatioCV)
		pLodRatioCV->Set(fOldLodRatio);

	if (CRenderer::CV_r_HideSunInCubemaps)
		gEnv->p3DEngine->SetSkyLightParameters(oldSunDir, oldSunStr, oldSkyKm, oldSkyKr, oldSkyG, oldSunRGB, true);

	CRenderer::CV_r_flares = nFlaresCV;

	if (pSSDOHalfResCV)
		pSSDOHalfResCV->Set(nOldSSDOHalfRes);

	iLog->Log("Successfully finished generating a cubemap");
#endif

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CTexture::DrawSceneToCubeSide(Vec3& Pos, int tex_size, int side)
{
	const float sCubeVector[6][7] =
	{
		{ 1,  0,  0,  0, 0, 1,  -90 }, //posx
		{ -1, 0,  0,  0, 0, 1,  90  }, //negx
		{ 0,  1,  0,  0, 0, -1, 0   }, //posy
		{ 0,  -1, 0,  0, 0, 1,  0   }, //negy
		{ 0,  0,  1,  0, 1, 0,  0   }, //posz
		{ 0,  0,  -1, 0, 1, 0,  0   }, //negz
	};

	if (!iSystem)
		return;

	CRenderer* r = gRenDev;
	CCamera prevCamera = r->GetCamera();
	CCamera tmpCamera = prevCamera;

	I3DEngine* eng = gEnv->p3DEngine;

	Vec3 vForward = Vec3(sCubeVector[side][0], sCubeVector[side][1], sCubeVector[side][2]);
	Vec3 vUp = Vec3(sCubeVector[side][3], sCubeVector[side][4], sCubeVector[side][5]);

	Matrix33 matRot = Matrix33::CreateOrientation(vForward, vUp, DEG2RAD(sCubeVector[side][6]));
	Matrix34 mFinal = Matrix34(matRot, Pos);
	tmpCamera.SetMatrix(mFinal);
	tmpCamera.SetFrustum(tex_size, tex_size, 90.0f * gf_PI / 180.0f, prevCamera.GetNearPlane(), prevCamera.GetFarPlane()); //90.0f*gf_PI/180.0f

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log)
		r->Logv(".. DrawSceneToCubeSide .. (DrawCubeSide %d)\n", side);
#endif

	eng->RenderWorld(SHDF_CUBEMAPGEN | SHDF_ALLOWPOSTPROCESS | SHDF_ALLOWHDR | SHDF_ZPASS | SHDF_NOASYNC | SHDF_STREAM_SYNC, SRenderingPassInfo::CreateGeneralPassRenderingInfo(tmpCamera, (SRenderingPassInfo::DEFAULT_FLAGS | SRenderingPassInfo::CUBEMAP_GEN)), __FUNCTION__);

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log)
		r->Logv(".. End DrawSceneToCubeSide .. (DrawCubeSide %d)\n", side);
#endif

	r->SetCamera(prevCamera);
}

//////////////////////////////////////////////////////////////////////////

bool CTexture::GenerateMipMaps(bool bSetOrthoProj, bool bUseHW, bool bNormalMap)
{
	if (!(GetFlags() & FT_FORCE_MIPS)
	    || bSetOrthoProj || !bUseHW || bNormalMap) //todo: implement
		return false;

	PROFILE_LABEL_SCOPE("GENERATE_MIPS");

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	CDeviceTexture* pTex = GetDevTexture();
	if (!pTex)
	{
		return false;
	}

	// all d3d11 devices support autogenmipmaps
	if (GetDevTexture()->IsWritable())
	{
		//	gcpRendD3D->GetDeviceContext().GenerateMips(m_pDeviceShaderResource);

		gcpRendD3D->GetGraphicsPipeline().SwitchFromLegacyPipeline();
		CMipmapGenPass().Execute(this);
		gcpRendD3D->GetGraphicsPipeline().SwitchToLegacyPipeline();
	}

	return true;

	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	return true;
}

void CTexture::DestroyZMaps()
{
	//SAFE_RELEASE(s_ptexZTarget);
}

void CTexture::GenerateZMaps()
{
	int nWidth = gcpRendD3D->m_MainViewport.nWidth;
	int nHeight = gcpRendD3D->m_MainViewport.nHeight;
	ETEX_Format eTFZ = CTexture::s_eTFZ;
	uint32 nFlags = FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_DONT_RELEASE;
	if (CRenderer::CV_r_msaa)
	{
		nFlags |= FT_USAGE_MSAA;
	}

	if (!s_ptexZTarget)
		s_ptexZTarget = GetOrCreateRenderTarget("$ZTarget", nWidth, nHeight, ColorF(1.0f, 1.0f, 1.0f, 1.0f), eTT_2D, nFlags, eTFZ);
	else
	{
		s_ptexZTarget->m_eFlags = nFlags;
		s_ptexZTarget->SetWidth(nWidth);
		s_ptexZTarget->SetHeight(nHeight);
		s_ptexZTarget->CreateRenderTarget(eTFZ, ColorF(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

void CTexture::DestroySceneMap()
{
	//SAFE_RELEASE(s_ptexSceneTarget);
}

void CTexture::GenerateSceneMap(ETEX_Format eTF)
{
	const int32 nWidth = gcpRendD3D->GetWidth();
	const int32 nHeight = gcpRendD3D->GetHeight();
	uint32 nFlags = FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS;

	if (!s_ptexSceneTarget)
		s_ptexSceneTarget = GetOrCreateRenderTarget("$SceneTarget", nWidth, nHeight, Clr_Empty, eTT_2D, nFlags, eTF, TO_SCENE_TARGET);
	else
	{
		s_ptexSceneTarget->m_eFlags = nFlags;
		s_ptexSceneTarget->SetWidth(nWidth);
		s_ptexSceneTarget->SetHeight(nHeight);
		s_ptexSceneTarget->CreateRenderTarget(eTF, Clr_Empty);
	}

	nFlags &= ~(FT_USAGE_MSAA | FT_USAGE_UNORDERED_ACCESS);

	// This RT used for all post processes passes and shadow mask (group 0) as well
	if (!CTexture::IsTextureExist(s_ptexBackBuffer))
		s_ptexBackBuffer = GetOrCreateRenderTarget("$BackBuffer", nWidth, nHeight, Clr_Empty, eTT_2D, nFlags | FT_USAGE_ALLOWREADSRGB, eTF_R8G8B8A8, TO_BACKBUFFERMAP);
	else
	{
		s_ptexBackBuffer->m_eFlags = nFlags | FT_USAGE_ALLOWREADSRGB;
		s_ptexBackBuffer->SetWidth(nWidth);
		s_ptexBackBuffer->SetHeight(nHeight);
		s_ptexBackBuffer->CreateRenderTarget(eTF_R8G8B8A8, Clr_Empty);
	}

	nFlags &= ~(FT_USAGE_MSAA | FT_USAGE_UNORDERED_ACCESS);

	// This RT can be used by the Render3DModelMgr if the buffer needs to be persistent
	if (CRenderer::CV_r_UsePersistentRTForModelHUD > 0)
	{
		if (!CTexture::IsTextureExist(s_ptexModelHudBuffer))
			s_ptexModelHudBuffer = GetOrCreateRenderTarget("$ModelHUD", nWidth, nHeight, Clr_Transparent, eTT_2D, nFlags, eTF_R8G8B8A8, TO_BACKBUFFERMAP);
		else
		{
			s_ptexModelHudBuffer->m_eFlags = nFlags;
			s_ptexModelHudBuffer->SetWidth(nWidth);
			s_ptexModelHudBuffer->SetHeight(nHeight);
			s_ptexModelHudBuffer->CreateRenderTarget(eTF_R8G8B8A8, Clr_Transparent);
		}
	}

	if (gEnv->IsEditor())
	{
		int highlightWidth = (nWidth + 1) / 2;
		int highlightHeight = (nHeight + 1) / 2;
		highlightWidth = nWidth;
		highlightHeight = nHeight;

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneSelectionIDs"    , CTexture::s_ptexSceneSelectionIDs    , highlightWidth, highlightHeight, Clr_Transparent, false, false, eTF_R32F, -1, nFlags);
		SD3DPostEffectsUtils::GetOrCreateDepthStencil("$SceneHalfDepthStencil", CTexture::s_ptexSceneHalfDepthStencil, highlightWidth, highlightHeight, Clr_FarPlane   , false, false, eTF_D32F, -1, nFlags | FT_USAGE_DEPTHSTENCIL);
		
		// Editor fix: it is possible at this point that resolution has changed outside of ChangeResolution and stereoR, stereoL have not been resized
		gcpRendD3D->GetS3DRend().OnResolutionChanged();
	}
}

void CTexture::GenerateCachedShadowMaps()
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
			pTx->CreateDepthStencil(texFormat, Clr_FarPlane);
		}
	}

	// height map AO
	if (CRendererCVars::CV_r_HeightMapAO)
	{
		const int nTexRes = (int)clamp_tpl(CRendererCVars::CV_r_HeightMapAOResolution, 0.f, 16384.f);
		ETEX_Format texFormatMips = texFormat == eTF_D32F ? eTF_R32F : eTF_R16;
		// Allow non-supported SNORM/UNORM to fall back to a FLOAT format with slightly less precision
		texFormatMips = gcpRendD3D->m_hwTexFormatSupport.GetLessPreciseFormatSupported(texFormatMips);

		if (!s_ptexHeightMapAODepth[0])
		{
			s_ptexHeightMapAODepth[0] = CTexture::GetOrCreateTextureObject("HeightMapAO_Depth_0", nTexRes, nTexRes, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, texFormat);
			s_ptexHeightMapAODepth[1] = CTexture::GetOrCreateTextureObject("HeightMapAO_Depth_1", nTexRes, nTexRes, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, texFormatMips);
		}

		s_ptexHeightMapAODepth[0]->Invalidate(nTexRes, nTexRes, texFormat);
		s_ptexHeightMapAODepth[1]->Invalidate(nTexRes, nTexRes, texFormatMips);

		if (!CTexture::IsTextureExist(s_ptexHeightMapAODepth[0]) && nTexRes > 0)
		{
			s_ptexHeightMapAODepth[0]->CreateDepthStencil(texFormat, Clr_FarPlane);
			s_ptexHeightMapAODepth[1]->CreateRenderTarget(texFormatMips, Clr_FarPlane);
		}
	}

	if (ShadowFrustumMGPUCache* pShadowMGPUCache = gRenDev->GetShadowFrustumMGPUCache())
	{
		pShadowMGPUCache->nUpdateMaskRT = 0;
		pShadowMGPUCache->nUpdateMaskMT = 0;
	}
}

void CTexture::DestroyCachedShadowMaps()
{
	for (int i = 0; i < MAX_GSM_LODS_NUM; ++i)
	{
		SAFE_RELEASE_FORCE(s_ptexCachedShadowMap[i]);
	}

	SAFE_RELEASE_FORCE(s_ptexHeightMapAO[0]);
	SAFE_RELEASE_FORCE(s_ptexHeightMapAO[1]);
}

void CTexture::GenerateNearestShadowMap()
{
	const int texResolution = CRendererCVars::CV_r_ShadowsNearestMapResolution;
	const ETEX_Format texFormat = CRendererCVars::CV_r_shadowtexformat == 0 ? eTF_D32F : eTF_D16;

	s_ptexNearestShadowMap = CTexture::GetOrCreateTextureObject("NearestShadowMap", texResolution, texResolution, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, texFormat);
}

void CTexture::DestroyNearestShadowMap()
{
	if (s_ptexNearestShadowMap)
	{
		SAFE_RELEASE_FORCE(s_ptexNearestShadowMap);
	}
}

bool SDynTexture::RT_SetRT(int nRT, int nWidth, int nHeight, bool bPush, bool bScreenVP)
{
	Update(m_nWidth, m_nHeight);

	SDepthTexture* pDepthSurf = nWidth > 0 ? gcpRendD3D->FX_GetDepthSurface(nWidth, nHeight, false, true) : &gcpRendD3D->m_DepthBufferOrig;

	assert(m_pTexture);
	if (m_pTexture)
	{
		if (bPush)
			return gcpRendD3D->FX_PushRenderTarget(nRT, m_pTexture, pDepthSurf, -1, bScreenVP);
		else
			return gcpRendD3D->FX_SetRenderTarget(nRT, m_pTexture, pDepthSurf, false, -1, bScreenVP);
	}
	return false;
}

bool SDynTexture::SetRT(int nRT, bool bPush, SDepthTexture* pDepthSurf, bool bScreenVP)
{
	Update(m_nWidth, m_nHeight);

	assert(m_pTexture);
	if (m_pTexture)
	{
		if (bPush)
			return gcpRendD3D->FX_PushRenderTarget(nRT, m_pTexture, pDepthSurf, -1, bScreenVP);
		else
			return gcpRendD3D->FX_SetRenderTarget(nRT, m_pTexture, pDepthSurf, false, -1, bScreenVP);
	}
	return false;
}

bool SDynTexture::RestoreRT(int nRT, bool bPop)
{
	if (bPop)
		return gcpRendD3D->FX_PopRenderTarget(nRT);
	else
		return gcpRendD3D->FX_RestoreRenderTarget(nRT);
}

bool SDynTexture::ClearRT()
{
	gcpRendD3D->FX_ClearTarget(m_pTexture);
	return true;
}

bool SDynTexture2::ClearRT()
{
	gcpRendD3D->FX_ClearTarget(m_pTexture);
	return true;
}

bool SDynTexture2::SetRT(int nRT, bool bPush, SDepthTexture* pDepthSurf, bool bScreenVP)
{
	Update(m_nWidth, m_nHeight);

	assert(m_pTexture);
	if (m_pTexture)
	{
		bool bRes = false;
		if (bPush)
			bRes = gcpRendD3D->FX_PushRenderTarget(nRT, m_pTexture, pDepthSurf);
		else
			bRes = gcpRendD3D->FX_SetRenderTarget(nRT, m_pTexture, pDepthSurf);
		SetRectStates();
		gcpRendD3D->FX_Commit();
	}
	return false;
}

bool SDynTexture2::SetRectStates()
{
	assert(m_pTexture);
	gcpRendD3D->RT_SetViewport(m_nX, m_nY, m_nWidth, m_nHeight);
	gcpRendD3D->EF_Scissor(true, m_nX, m_nY, m_nWidth, m_nHeight);
	RECT rc;
	rc.left = m_nX;
	rc.right = m_nX + m_nWidth;
	rc.top = m_nY;
	rc.bottom = m_nY + m_nHeight;
	if (m_pTexture)
		m_pTexture->GetDevTexture()->AddDirtRect(rc, rc.left, rc.top);
	return true;
}

bool SDynTexture2::RestoreRT(int nRT, bool bPop)
{
	bool bRes = false;
	gcpRendD3D->EF_Scissor(false, m_nX, m_nY, m_nWidth, m_nHeight);
	if (bPop)
		bRes = gcpRendD3D->FX_PopRenderTarget(nRT);
	else
		bRes = gcpRendD3D->FX_RestoreRenderTarget(nRT);
	gcpRendD3D->FX_Commit();

	return bRes;
}

void               _DrawText(ISystem* pSystem, int x, int y, const float fScale, const char* format, ...);

static int __cdecl RTCallback(const VOID* arg1, const VOID* arg2)
{
	CTexture* p1 = *(CTexture**)arg1;
	CTexture* p2 = *(CTexture**)arg2;

	// show big textures first
	int nSize1 = p1->GetDataSize();
	int nSize2 = p2->GetDataSize();
	if (nSize1 > nSize2)
		return -1;
	else if (nSize2 > nSize1)
		return 1;

	return strcmp(p1->GetName(), p2->GetName());
}

void CD3D9Renderer::DrawAllDynTextures(const char* szFilter, const bool bLogNames, const bool bOnlyIfUsedThisFrame)
{
#ifndef _RELEASE
	SDynTexture2::TextureSet2Itor itor;
	char name[256]; //, nm[256];
	cry_strcpy(name, szFilter);
	strlwr(name);
	TArray<CTexture*> UsedRT;
	int nMaxCount = CV_r_ShowDynTexturesMaxCount;

	float width = 800;
	float height = 600;
	float fArrDim = max(1.f, sqrtf((float)nMaxCount));
	float fPicDimX = width / fArrDim;
	float fPicDimY = height / fArrDim;
	float x = 0;
	float y = 0;
	Set2DMode(true, (int)width, (int)height);
	EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
	if (name[0] == '*' && !name[1])
	{
		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		ResourcesMapItor it;
		for (it = pRL->m_RMap.begin(); it != pRL->m_RMap.end(); it++)
		{
			CTexture* tp = (CTexture*)it->second;
			if (tp && !tp->IsNoTexture())
			{
				if ((tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC)) && tp->GetDevTexture())
					UsedRT.AddElem(tp);
			}
		}
	}
	else
	{
		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		ResourcesMapItor it;
		for (it = pRL->m_RMap.begin(); it != pRL->m_RMap.end(); it++)
		{
			CTexture* tp = (CTexture*)it->second;
			if (!tp || tp->IsNoTexture())
				continue;
			if ((tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC)) && tp->GetDevTexture())
			{
				char nameBuffer[128];
				cry_strcpy(nameBuffer, tp->GetName());
				strlwr(nameBuffer);
				if (CryStringUtils::MatchWildcard(nameBuffer, name))
					UsedRT.AddElem(tp);
			}
		}
	#if 0
		for (itor = SDynTexture2::m_TexturePool.begin(); itor != SDynTexture2::m_TexturePool.end(); itor++)
		{
			if (UsedRT.Num() >= nMaxCount)
				break;
			STextureSetFormat* pTS = (STextureSetFormat*)itor->second;
			SDynTexture2* pDT;
			int nID = 0;
			for (pDT = pTS->m_pRoot; pDT; pDT = pDT->m_Next)
			{
				if (nID && pDT == pTS->m_pRoot)
					break;
				nID++;
				if (UsedRT.Num() >= nMaxCount)
					break;
				if (!pDT->m_sSource)
					continue;
				strcpy(nm, pDT->m_sSource);
				strlwr(nm);
				if (name[0] != '*' && !strstr(nm, name))
					continue;
				for (i = 0; i < UsedRT.Num(); i++)
				{
					if (pDT->m_pTexture == UsedRT[i])
						break;
				}
				if (i == UsedRT.Num())
				{
					UsedRT.AddElem(pDT->m_pTexture);

					x += fPicDimX;
					if (x >= width - 10)
					{
						x = 0;
						y += fPicDimY;
					}
				}
			}
		}
	#endif
	}
	if (true /* szFilter[0] == '*' */)
	{
		if (UsedRT.Num() > 1)
		{
			qsort(&UsedRT[0], UsedRT.Num(), sizeof(CTexture*), RTCallback);
		}
	}
	fPicDimX = width / fArrDim;
	fPicDimY = height / fArrDim;
	x = 0;
	y = 0;
	for (uint32 i = 0; i < UsedRT.Num(); i++)
	{
		SetState(GS_NODEPTHTEST);
		CTexture* tp = UsedRT[i];
		int nSavedAccessFrameID = tp->m_nAccessFrameID;

		if (bOnlyIfUsedThisFrame)
			if (tp->m_nUpdateFrameID < m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameUpdateID - 2)
				continue;

		if (tp->GetTextureType() == eTT_2D)
			Draw2dImage(x, y, fPicDimX - 2, fPicDimY - 2, tp->GetID(), 0, 1, 1, 0, 0);

		tp->m_nAccessFrameID = nSavedAccessFrameID;

		const char* pTexName = tp->GetName();
		char nameBuffer[128];
		memset(nameBuffer, 0, sizeof nameBuffer);
		for (int iStr = 0, jStr = 0; pTexName[iStr] && jStr < sizeof nameBuffer - 1; ++iStr)
		{
			if (pTexName[iStr] == '$')
			{
				nameBuffer[jStr] = '$';
				nameBuffer[jStr + 1] = '$';
				jStr += 2;
			}
			else
			{
				nameBuffer[jStr] = pTexName[iStr];
				++jStr;
			}
		}
		nameBuffer[sizeof nameBuffer - 1] = 0;
		pTexName = nameBuffer;

		int32 nPosX = (int32)ScaleCoordX(x);
		int32 nPosY = (int32)ScaleCoordY(y);
		_DrawText(iSystem, nPosX, nPosY, 1.0f, "%8s", pTexName);
		_DrawText(iSystem, nPosX, nPosY += 10, 1.0f, "%d-%d", tp->m_nUpdateFrameID, tp->m_nAccessFrameID);
		_DrawText(iSystem, nPosX, nPosY += 10, 1.0f, "%dx%d", tp->GetWidth(), tp->GetHeight());

		if (bLogNames)
		{
			iLog->Log("Mem:%d  %dx%d  Type:%s  Format:%s (%s)",
			          tp->GetDeviceDataSize(), tp->GetWidth(), tp->GetHeight(), tp->NameForTextureType(tp->GetTextureType()), tp->NameForTextureFormat(tp->GetDstFormat()), tp->GetName());
		}

		x += fPicDimX;
		if (x >= width - 10)
		{
			x = 0;
			y += fPicDimY;
		}
	}

	Set2DMode(false, m_width, m_height);
#endif
}

void CFlashTextureSourceBase::Advance(const float delta, bool isPaused)
{
	if (!m_pFlashPlayer)
		return;

	AutoReleasedFlashPlayerPtr pFlashPlayer(m_pFlashPlayer->GetTempPtr());
	if (!pFlashPlayer)
		return;

	if (pFlashPlayer)
	{
		if (isPaused != pFlashPlayer->IsPaused())
			pFlashPlayer->Pause(isPaused);

		m_pFlashPlayer->Advance(delta);
	}
}

bool CFlashTextureSourceBase::Update()
{
	if (!m_pFlashPlayer)
		return false;

	AutoReleasedFlashPlayerPtr pFlashPlayer(m_pFlashPlayer->GetTempPtr());
	if (!pFlashPlayer)
		return false;

	SDynTexture* pDynTexture = GetDynTexture();
	assert(pDynTexture);
	if (!pDynTexture)
		return false;

	const int rtWidth = GetWidth();
	const int rtHeight = GetHeight();
	if (!UpdateDynTex(rtWidth, rtHeight))
		return false;

	m_width = min(pFlashPlayer->GetWidth(), rtWidth);
	m_height = min(pFlashPlayer->GetHeight(), rtHeight);
	m_aspectRatio = ((float)pFlashPlayer->GetWidth() / (float)m_width) * ((float)m_height / (float)pFlashPlayer->GetHeight());

	pFlashPlayer->SetViewport(0, 0, m_width, m_height, m_aspectRatio);
	pFlashPlayer->SetScissorRect(0, 0, m_width, m_height);
	pFlashPlayer->SetBackgroundAlpha(Clr_Transparent.a);
	
	m_lastVisibleFrameID = gRenDev->GetFrameID(false);
	m_lastVisible = gEnv->pTimer->GetAsyncTime();

	{
		PROFILE_LABEL_SCOPE("FlashDynTexture");

		const RECT rect = { 0, 0, clamp_tpl(Align8(m_width), 16, rtWidth), clamp_tpl(Align8(m_height), 16, rtHeight) };

		gcpRendD3D->FX_ClearTarget(pDynTexture->m_pTexture, Clr_Transparent, 1, &rect, true);
		pDynTexture->SetRT(0, true, gcpRendD3D->FX_GetDepthSurface(rtWidth, rtHeight, false));
		gcpRendD3D->RT_SetViewport(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
		gcpRendD3D->FX_Commit();

		pFlashPlayer->Render(false);

		pDynTexture->RestoreRT(0, true);
		gcpRendD3D->FX_SetState(gcpRendD3D->m_RP.m_CurState & ~GS_BLEND_MASK);

		pDynTexture->SetUpdateMask();
	}

	return true;
}

bool CFlashTextureSource::Update()
{
	if (!CFlashTextureSourceBase::Update())
		return false;
	if (!m_pMipMapper)
		m_pMipMapper = new CMipmapGenPass();

	// calculate mip-maps after update, if mip-maps have been allocated
	{
		PROFILE_LABEL_SCOPE("FlashDynTexture MipMap");

		if (!(CRenderer::CV_r_GraphicsPipeline > 0))
			gcpRendD3D->GetGraphicsPipeline().SwitchFromLegacyPipeline();
		m_pMipMapper->Execute((CTexture*)m_pDynTexture->GetTexture());
		if (!(CRenderer::CV_r_GraphicsPipeline > 0))
			gcpRendD3D->GetGraphicsPipeline().SwitchToLegacyPipeline();
	}

	return true;
}

void CFlashTextureSourceSharedRT::ProbeDepthStencilSurfaceCreation(int width, int height)
{
	gcpRendD3D->FX_GetDepthSurface(width, height, false);
}

bool CFlashTextureSourceSharedRT::Update()
{
	if (!CFlashTextureSourceBase::Update())
		return false;
	if (!ms_pMipMapper)
		ms_pMipMapper = new CMipmapGenPass();

	// calculate mip-maps after update, if mip-maps have been allocated
	{
		PROFILE_LABEL_SCOPE("FlashDynTexture MipMap");

		if (!(CRenderer::CV_r_GraphicsPipeline > 0))
			gcpRendD3D->GetGraphicsPipeline().SwitchFromLegacyPipeline();
		ms_pMipMapper->Execute((CTexture*)ms_pDynTexture->GetTexture());
		if (!(CRenderer::CV_r_GraphicsPipeline > 0))
			gcpRendD3D->GetGraphicsPipeline().SwitchToLegacyPipeline();
	}

	return true;
}

void CTexture::ReleaseSystemTargets()
{
	CTexture::DestroyHDRMaps();
	CTexture::DestroySceneMap();
	CTexture::DestroyCachedShadowMaps();
	CTexture::DestroyNearestShadowMap();

	if (CDeferredShading::Instance().IsValid())
		CDeferredShading::Instance().DestroyDeferredMaps();

	SPostEffectsUtils::Release();

	SAFE_RELEASE_FORCE(s_ptexWaterOcean);
	SAFE_RELEASE_FORCE(s_ptexWaterVolumeTemp[0]);
	SAFE_RELEASE_FORCE(s_ptexWaterVolumeTemp[1]);

	SAFE_RELEASE_FORCE(s_ptexSceneNormalsMap);
	SAFE_RELEASE_FORCE(s_ptexSceneNormalsBent);
	SAFE_RELEASE_FORCE(s_ptexAOColorBleed);
	SAFE_RELEASE_FORCE(s_ptexSceneDiffuse);
	SAFE_RELEASE_FORCE(s_ptexSceneSpecular);
	SAFE_RELEASE_FORCE(s_ptexSceneSelectionIDs);
	SAFE_RELEASE_FORCE(s_ptexSceneHalfDepthStencil);
#if defined(DURANGO_USE_ESRAM)
	SAFE_RELEASE_FORCE(s_ptexSceneSpecularESRAM);
#endif
	SAFE_RELEASE_FORCE(s_ptexVelocityObjects[0]);
	SAFE_RELEASE_FORCE(s_ptexVelocityObjects[1]);
	SAFE_RELEASE_FORCE(s_ptexSceneDiffuseAccMap);
	SAFE_RELEASE_FORCE(s_ptexSceneSpecularAccMap);
	SAFE_RELEASE_FORCE(s_ptexBackBuffer);
	SAFE_RELEASE_FORCE(s_ptexSceneTarget);
	SAFE_RELEASE_FORCE(s_ptexZTargetScaled[0]);
	SAFE_RELEASE_FORCE(s_ptexZTargetScaled[1]);
	SAFE_RELEASE_FORCE(s_ptexZTargetScaled[2]);
	SAFE_RELEASE_FORCE(s_ptexDepthBufferQuarter);
	SAFE_RELEASE_FORCE(s_ptexDepthBufferHalfQuarter);

	gcpRendD3D->m_bSystemTargetsInit = 0;
}

void CTexture::CreateSystemTargets()
{
	if (!gcpRendD3D->m_bSystemTargetsInit)
	{
		gcpRendD3D->m_bSystemTargetsInit = 1;

		// Create HDR targets
		CTexture::GenerateHDRMaps();

		// Create scene targets
		CTexture::GenerateSceneMap(CRenderer::CV_r_HDRTexFormat == 0 ? eTF_R11G11B10F : eTF_R16G16B16A16F);

		// Create ZTarget
		CTexture::GenerateZMaps();

		// Allocate cached shadow maps if required
		CTexture::GenerateCachedShadowMaps();

		// Allocate the nearest shadow map if required
		CTexture::GenerateNearestShadowMap();

		// Create deferred lighting targets
		if (CDeferredShading::Instance().IsValid())
			CDeferredShading::Instance().CreateDeferredMaps();

		gcpRendD3D->GetGraphicsPipeline().Init();
		gcpRendD3D->GetTiledShading().CreateResources();

		// Create post effects targets
		SPostEffectsUtils::Create();
	}
}

void CTexture::CopySliceChain(CDeviceTexture* const pDstDevTex, int nDstNumMips, int nDstSliceOffset, int nDstMipOffset,
                              CDeviceTexture* const pSrcDevTex, int nSrcNumMips, int nSrcSliceOffset, int nSrcMipOffset, int nNumSlices, int nNumMips)
{
	HRESULT hr = S_OK;
	assert(pSrcDevTex && pDstDevTex);
	assert(nNumSlices == 1 && "TODO: allow multiple slices being uploaded with the same command");

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	pDstDevTex->InitD3DTexture();
#endif

	D3DBaseTexture* pDstResource = pDstDevTex->GetBaseTexture();
	D3DBaseTexture* pSrcResource = pSrcDevTex->GetBaseTexture();

#ifndef _RELEASE
	if (!pDstResource) __debugbreak();
	if (!pSrcResource) __debugbreak();
#endif

	if (0)
	{
	}
#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)  && defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	else if (!gcpRendD3D->m_pRT->IsRenderThread(true))
	{
		// We can use the move engine!

		CryAutoLock<CryCriticalSection> dmaLock(GetDeviceObjectFactory().m_dma1Lock);
		ID3D11DmaEngineContextX* pDMA = GetDeviceObjectFactory().m_pDMA1;

		for (int nMip = 0; nMip < nNumMips; ++nMip)
		{
			pDMA->CopySubresourceRegion(
			  pDstResource,
			  D3D11CalcSubresource(nDstMipOffset + nMip, nDstSliceOffset, nDstNumMips),
			  0, 0, 0,
			  pSrcResource,
			  D3D11CalcSubresource(nSrcMipOffset + nMip, nSrcSliceOffset, nSrcNumMips),
			  NULL,
			  D3D11_COPY_NO_OVERWRITE);
		}

		UINT64 fence = pDMA->InsertFence(D3D11_INSERT_FENCE_NO_KICKOFF);
		pDMA->Submit();
		while (gcpRendD3D->GetPerformanceDevice().IsFencePending(fence))
			CrySleep(1);
	}
#endif
	else
	{
		assert(nSrcMipOffset >= 0 && nDstMipOffset >= 0);

		for (int nMip = 0; nMip < nNumMips; ++nMip)
		{
			SResourceRegionMapping mapping =
			{
				{ 0, 0, 0, D3D11CalcSubresource(nSrcMipOffset + nMip, nSrcSliceOffset, nSrcNumMips) }, // src position
				{ 0, 0, 0, D3D11CalcSubresource(nDstMipOffset + nMip, nDstSliceOffset, nDstNumMips) }, // dst position
				pSrcDevTex->GetDimension(nSrcMipOffset + nMip, nNumSlices), // size
				D3D11_COPY_NO_OVERWRITE_REVERT | D3D11_COPY_NO_OVERWRITE_PXLSRV
			};

			// "TODO: allow multiple slices being uploaded with the same command"
			mapping.Extent.Subresources = 1;

			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
				pSrcDevTex,
				pDstDevTex,
				mapping);
		}
	}
}

//===========================================================================================================
