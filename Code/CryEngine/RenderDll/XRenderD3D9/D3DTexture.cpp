// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DTexture.cpp : Direct3D specific texture manager implementation.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#include "StdAfx.h"
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
#include <Common/RenderDisplayContext.h>

#include <algorithm>
#include <iterator>

#undef min
#undef max

//===============================================================================

static bool DecompressSubresourcePayload(const STextureLayout& pLayout, const ETEX_TileMode eSrcTileMode, const SSubresourceData& pData, STexturePayload& pPayload)
{
	assert(!pPayload.m_pSysMemSubresourceData);
	if (!pData.m_subresourcePointers)
	{
		pPayload.m_pSysMemSubresourceData = nullptr;
		pPayload.m_eSysMemTileMode = eTM_Unspecified;

		return false;
	}

	const int nSlices = pLayout.m_nArraySize;
	const int8 nMips = pLayout.m_nMips;

	SSubresourcePayload* InitData = new SSubresourcePayload[nMips * nSlices];

	// Contrary to hardware here the sub-resources are sorted by mips, then by slices.
	// In hardware it's sorted by slices, then by mips (so same mips are consecutive).
	const byte* pAutoData = nullptr;
	const uint8** pDataCursor = pData.m_subresourcePointers;

	PREFAST_ASSUME(*pDataCursor != nullptr);

	for (int nSlice = 0, nSubresource = 0; (nSlice < nSlices); ++nSlice)
	{
		int w = pLayout.m_nWidth;
		int h = pLayout.m_nHeight;
		int d = pLayout.m_nDepth;

		for (int8 nMip = 0; (nMip < nMips) && (w | h | d); ++nMip, ++nSubresource)
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
			pDataCursor = pData.m_subresourcePointers;
		// each face is nullptr-terminated
		else if (!*pDataCursor)
			pDataCursor++;
	}

	pPayload.m_pSysMemSubresourceData = InitData;
	pPayload.m_eSysMemTileMode = eSrcTileMode;

	return true;
}

#if defined(TEXTURE_GET_SYSTEM_COPY_SUPPORT)
const byte* CTexture::Convert(const byte* pSrc, int nWidth, int nHeight, int nMips, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nOutMips, uint32& nOutSize, bool bLinear)
{
	nOutSize = 0;
	byte* pDst = NULL;
	if (eTFSrc == eTFDst && nMips == nOutMips)
		return pSrc;

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
	uint32 nSizeDSTMips = 0;
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
			nSizeDSTMips = CTexture::TextureDataSize(wdt, hgt, 1, nOutMips, 1, eTF_BC3, eTM_None);
			pDst = new byte[nSizeDSTMips];
			size_t nOffsDST = 0;
			size_t nOffsSRC = 0;
			for (i = 0; i < nOutMips; i++)
			{
				if (i >= nMips)
					break;
				if (wdt <= 0)
					wdt = 1;
				if (hgt <= 0)
					hgt = 1;
				const byte* outSrc = &pSrc[nOffsSRC];

				uint32 outSize = CTexture::TextureDataSize(wdt, hgt, 1, 1, 1, eTFDst, eTM_None);
				nOffsSRC += CTexture::TextureDataSize(wdt, hgt, 1, 1, 1, eTFSrc, eTM_None);

				{
					const byte* src = outSrc;

					for (uint32 n = 0; n < outSize / 16; n++)     // for each block
					{
						const uint8* pSrcBlock = &src[n * 16];
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

	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	MEMSTAT_CONTEXT_FMT(EMemStatContextType::Texture, "Create Render Target: %s", GetSourceName());

	const int nMipLevel = (m_eFlags & FT_FORCE_MIPS) ? min((int)(m_nMips - 1), nLevel) : 0;
	const int nSlice = (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray ? nCMSide : 0);
	const int nSliceCount = (m_eTT == eTT_Cube || m_eTT == eTT_CubeArray ? 1 : -1);

	return (D3DSurface*)GetResourceView(SResourceView::RenderTargetView(DeviceFormats::ConvertFromTexFormat(m_eDstFormat), nSlice, nSliceCount, nMipLevel, IsMSAA()));
}

D3DSurface* CTexture::GetSurface(int nCMSide, int nLevel) const
{
	if (!m_pDevTexture)
		return NULL;

	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	MEMSTAT_CONTEXT_FMT(EMemStatContextType::Texture, "Create Render Target: %s", GetSourceName());

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

	CRY_ASSERT((m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL)) && (m_eFlags & FT_USAGE_MSAA) && GetDevTexture() && GetDevTexture()->GetMSAATexture());

#if defined(USE_CRY_ASSERT) || defined(RENDERER_ENABLE_LEGACY_PIPELINE)
	CDeviceTexture* pDestSurf = GetDevTexture();
	CRY_ASSERT(pDestSurf != nullptr);
	CDeviceTexture* pSrcSurf = pDestSurf->GetMSAATexture();
	CRY_ASSERT(pSrcSurf != nullptr);
#endif

#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
	const SPixFormat* pPF;
	ETEX_Format eDstFormat = CTexture::GetClosestFormatSupported(m_eDstFormat, pPF);
	gcpRendD3D->GetDeviceContext().ResolveSubresource(pDestSurf->Get2DTexture(), 0, pSrcSurf->Get2DTexture(), 0, (DXGI_FORMAT)pPF->DeviceFormat);
#endif
	return true;
}

void CTexture::CreateDeviceTexture(SSubresourceData&& pData)
{
	CRY_ASSERT(m_eDstFormat != eTF_Unknown);
	CRY_ASSERT(!pData.m_subresourcePointers || m_eSrcTileMode != eTM_Unspecified);

	const bool bAsync = pData.m_owned;
	const uint8** pSRP = pData.m_subresourcePointers;

	this->AddRef();
	gRenDev->ExecuteRenderThreadCommand([=]
	{
		// Compensate for no move-capturing
		SSubresourceData pLocalData(pSRP, bAsync);

		bool bResult = this->RT_CreateDeviceTexture(pLocalData);
		if (!bResult)
			this->m_eFlags |=  FT_FAILED;
		else
			this->m_eFlags &= ~FT_FAILED;
		this->Release();

	}, ERenderCommandFlags::LevelLoadingThread_executeDirect
	| (bAsync ? ERenderCommandFlags::None : ERenderCommandFlags::FlushAndWait));

	pData.m_subresourcePointers = nullptr;
	pData.m_owned = true;
}

bool CTexture::RT_CreateDeviceTexture(const SSubresourceData& pData)
{
	CRY_PROFILE_SECTION(PROFILE_RENDERER, "CTexture::RT_CreateDeviceTexture");
	MEMSTAT_CONTEXT(EMemStatContextType::Texture, "Creating Texture");
	MEMSTAT_CONTEXT_FMT(EMemStatContextType::Texture, "%s %ix%ix%i %08x", m_SrcName.c_str(), m_nWidth, m_nHeight, m_nMips, m_eFlags);
	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	if (m_eFlags & FT_USAGE_MSAA)
		m_bResolved = false;

	m_nMinMipVidActive = 0;

	//if we have any device owned resources allocated, we must sync with render thread
	if (m_pDevTexture)
	{
		CRY_ASSERT(!DEVICE_TEXTURE_STORE_OWNER || m_pDevTexture->GetOwner() == this);
		RT_ReleaseDeviceTexture(false, false);
	}

	// The payload can only be passed untiled currently
	STextureLayout TL = GetLayout();
	STexturePayload TI;

	CRY_ASSERT(!pData.m_subresourcePointers || m_eSrcTileMode != eTM_Unspecified);
	bool bHasPayload = DecompressSubresourcePayload(TL, m_eSrcTileMode, pData, TI);
	if (!(m_pDevTexture = CDeviceTexture::Create(TL, bHasPayload ? &TI : nullptr)))
		return false;

	// Assign name to Texture for enhanced debugging
	m_pDevTexture->SetOwner(this);
	m_pDevTexture->SetDebugName(m_SrcName);

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

	if (!bHasPayload)
		return true;

	if (m_eTT == eTT_3D)
		m_bIsSRGB = false;

	SetWasUnload(false);

	return true;
}

void CTexture::ReleaseDeviceTexture(bool bKeepLastMips, bool bFromUnload) threadsafe
{
	this->AddRef();
	gRenDev->ExecuteRenderThreadCommand([=]
	{
		this->RT_ReleaseDeviceTexture(bKeepLastMips, bFromUnload);
		this->Release();

	}, ERenderCommandFlags::LevelLoadingThread_executeDirect
		| ERenderCommandFlags::FlushAndWait);
}

void CTexture::RT_ReleaseDeviceTexture(bool bKeepLastMips, bool bFromUnload) threadsafe
{
	CRY_PROFILE_SECTION(PROFILE_RENDERER, "CTexture::RT_ReleaseDeviceTexture");

	if (!bFromUnload)
		AbortStreamingTasks(this);

	if (s_pTextureStreamer)
		s_pTextureStreamer->OnTextureDestroy(this);

	if (!m_bNoTexture)
	{
		// The responsible code-path for deconstruction is the m_pFileTexMips->m_pPoolItem's dtor, if either of these is set
		if (!m_pFileTexMips || !m_pFileTexMips->m_pPoolItem)
		{
			if (CDeviceTexture* const pDevTex = m_pDevTexture)
			{
				CRY_ASSERT(!DEVICE_TEXTURE_STORE_OWNER || m_pDevTexture->GetOwner() == this);
				pDevTex->SetOwner(nullptr);

				// Ref-Counting only works when there is a backing native objects, otherwise it's -1
				if (IsStreamed() || !pDevTex->GetNativeResource())
				{
					// for manually created textures
					delete pDevTex;
				}
				else
				{
					pDevTex->Release();

					volatile size_t* pTexMem = &CTexture::s_nStatsCurManagedNonStreamedTexMem;
					if (IsDynamic())
						pTexMem = &CTexture::s_nStatsCurDynamicTexMem;

					CRY_ASSERT(*pTexMem >= ptrdiff_t(m_nDevTextureSize));
					CryInterlockedAdd(pTexMem, -ptrdiff_t(m_nDevTextureSize));
				}
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

const SPixFormat* CTexture::GetPixFormat(ETEX_Format eTFDst)
{
	return CRendererResources::s_hwTexFormatSupport.GetPixFormat(eTFDst);
}

ETEX_Format CTexture::GetClosestFormatSupported(ETEX_Format eTFDst, const SPixFormat*& pPF)
{
	return CRendererResources::s_hwTexFormatSupport.GetClosestFormatSupported(eTFDst, pPF);
}

ETEX_Format CTexture::SetClosestFormatSupported()
{
	return m_eDstFormat = CRendererResources::s_hwTexFormatSupport.GetClosestFormatSupported(m_eSrcFormat);
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

void CTexture::UpdateTextureRegion(const byte* pSrcData, int nX, int nY, int nZ, int USize, int VSize, int ZSize, ETEX_Format eSrcFormat)
{
	if (gRenDev->m_pRT->IsRenderThread())
	{
		RT_UpdateTextureRegion(pSrcData, nX, nY, nZ, USize, VSize, ZSize, eSrcFormat);
	}
	else
	{
		uint32 nSize = TextureDataSize(USize, VSize, ZSize, GetNumMips(), 1, eSrcFormat, eTM_None);
		std::shared_ptr<std::vector<uint8>> tempData = std::make_shared<std::vector<uint8>>(pSrcData, pSrcData + nSize);
		_smart_ptr<CTexture> pSelf(this);

		ERenderCommandFlags flags = ERenderCommandFlags::LevelLoadingThread_defer;
		if (gcpRendD3D->m_pRT->m_eVideoThreadMode != SRenderThread::eVTM_Disabled)
			flags |= ERenderCommandFlags::MainThread_defer;

		gRenDev->ExecuteRenderThreadCommand(
			[=]
		{
			uint8* pTempBuffer = tempData->data();
			pSelf->RT_UpdateTextureRegion(pTempBuffer, nX, nY, nZ, USize, VSize, ZSize, eSrcFormat);
		},
			flags
		);
	}
}

void CTexture::RT_UpdateTextureRegion(const byte* pSrcData, int nX, int nY, int nZ, int USize, int VSize, int ZSize, ETEX_Format eSrcFormat)
{
	CRY_PROFILE_SECTION(PROFILE_RENDERER, "CTexture::RT_UpdateTextureRegion");
	PROFILE_FRAME(UpdateTextureRegion);
	PROFILE_LABEL_SCOPE("UpdateTextureRegion");

	if (m_eTT != eTT_1D && m_eTT != eTT_2D && m_eTT != eTT_3D)
	{
		CRY_ASSERT(0);
		return;
	}

	CDeviceTexture* pDevTex = GetDevTexture();
	assert(pDevTex);
	if (!pDevTex)
		return;

//	GPUPIN_DEVICE_TEXTURE(gcpRendD3D->GetPerformanceDeviceContext(), pDevTex);

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
			{
				static_cast<UINT>(nX),
				static_cast<UINT>(nY),
				static_cast<UINT>(nZ),
				D3D11CalcSubresource(i, 0, m_nMips) }, // dst position
			{
				static_cast<UINT>(USize),
				static_cast<UINT>(VSize),
				static_cast<UINT>(ZSize),
				1
			}                                 // dst size
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
	return Clear(m_cClearColor);
}

bool CTexture::Clear(const ColorF& color)
{
	if (!(m_eFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL)))
		return false;

	_smart_ptr<CTexture> pSelf(this);
	gRenDev->ExecuteRenderThreadCommand([=]
	{
		// FT_USAGE_DEPTHSTENCIL takes preference over RenderTarget if both are possible
		if (pSelf->GetFlags() & FT_USAGE_DEPTHSTENCIL)
			CClearSurfacePass::Execute(pSelf, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, color.r, (uint8)color.g); // NOTE: normalized depth in color.r and unnormalized stencil in color.g
		else if (pSelf->GetFlags() & FT_USAGE_RENDERTARGET)
			CClearSurfacePass::Execute(pSelf, color);
	}, ERenderCommandFlags::FlushAndWait);

	return true;
}

void SEnvTexture::Release()
{
	ReleaseDeviceObjects();
	SAFE_DELETE(m_pTex);
}

void SEnvTexture::SetMatrix(const CCamera& camera)
{
	camera.CalculateRenderMatrices();
	Matrix44A matView = camera.GetRenderViewMatrix();
	Matrix44A matProj = camera.GetRenderProjectionMatrix();

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

//////////////////////////////////////////////////////////////////////////

static void DrawSceneToCubeSide(CRenderOutputPtr pRenderOutput, const Vec3& Pos, int tex_size, int side, uint32 shaderRenderflags, const SGraphicsPipelineKey& graphicsPipelineKey)
{
	CRY_ASSERT(gRenDev->m_pRT->IsMainThread());
	if (!iSystem)
		return;

	const float sCubeVector[6][7] =
	{
		{ 1,  0,  0,  0, 0, 1,  -90 }, //posx
		{ -1, 0,  0,  0, 0, 1,  90  }, //negx
		{ 0,  1,  0,  0, 0, -1, 0   }, //posy
		{ 0,  -1, 0,  0, 0, 1,  0   }, //negy
		{ 0,  0,  1,  0, 1, 0,  0   }, //posz
		{ 0,  0,  -1, 0, 1, 0,  0   }, //negz
	};

#ifdef DO_RENDERLOG
	CRenderer* r = gRenDev;
#endif

	CCamera prevCamera = gEnv->pSystem->GetViewCamera();
	CCamera tmpCamera = prevCamera;

	I3DEngine* pEngine = gEnv->p3DEngine;

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

	gEnv->pSystem->SetViewCamera(tmpCamera);

	uint32 nRenderPassFlags = SRenderingPassInfo::DEFAULT_FLAGS | SRenderingPassInfo::CUBEMAP_GEN;

	// TODO: Try to run cube-map generation as recursive pass
	auto generalPassInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(graphicsPipelineKey, tmpCamera, nRenderPassFlags, false, {});
	CRenderView* pRenderView = generalPassInfo.GetRenderView();
	pRenderView->AssignRenderOutput(pRenderOutput);
	pRenderView->SetSkinningDataPools(gcpRendD3D->GetSkinningDataPools());

	CCamera cam = generalPassInfo.GetCamera();
	pRenderView->SetCameras(&cam, 1);
	pRenderView->SetPreviousFrameCameras(&cam, 1);

	gcpRendD3D->ResizePipelineAndContext(generalPassInfo.GetGraphicsPipelineKey(), generalPassInfo.GetDisplayContextKey(), tex_size, tex_size);
	gcpRendD3D->BeginFrame(generalPassInfo.GetDisplayContextKey(), generalPassInfo.GetGraphicsPipelineKey());
	
	pEngine->RenderWorld(shaderRenderflags, generalPassInfo, __FUNCTION__);

	gcpRendD3D->EndFrame();

	gEnv->pSystem->SetViewCamera(prevCamera);

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log)
		r->Logv(".. End DrawSceneToCubeSide .. (DrawCubeSide %d)\n", side);
#endif
}

struct SCvarOverrideHelper
{
	template<typename T>
	SCvarOverrideHelper(const char* cvarName, T value)
	{
		m_desiredValue.emplace<T>(value);
		m_pCVar = gEnv->pConsole->GetCVar(cvarName);
	}

	void ApplyValue()
	{
		if (m_pCVar)
		{
			switch (m_pCVar->GetType())
			{
			case ECVarType::Int:
				m_previousValue.emplace<int>(m_pCVar->GetIVal());
				m_pCVar->Set(stl::get<int>(m_desiredValue));
				break;
			case ECVarType::Float:
				m_previousValue.emplace<float>(m_pCVar->GetFVal());
				m_pCVar->Set(stl::get<float>(m_desiredValue));
				break;
			}
		}
	}

	void RestoreValue()
	{
		if (m_pCVar)
		{
			switch (m_pCVar->GetType())
			{
			case ECVarType::Int:
				m_pCVar->Set(stl::get<int>(m_previousValue));
				break;
			case ECVarType::Float:
				m_pCVar->Set(stl::get<float>(m_previousValue));
				break;
			}
		}
	}

private:
	CryVariant<int, float> m_desiredValue;
	CryVariant<int, float> m_previousValue;
	ICVar*                 m_pCVar;
};

DynArray<std::uint16_t> CTexture::RenderEnvironmentCMHDR(std::size_t size, const Vec3& Pos)
{
	DynArray<std::uint16_t> vecData;

#if CRY_PLATFORM_DESKTOP

	float timeStart = gEnv->pTimer->GetAsyncTime().GetSeconds();

	iLog->Log("Start generating a cubemap (%d x %d) at position (%.1f, %.1f, %.1f)", size, size, Pos.x, Pos.y, Pos.z);

	CTexture* ptexGenEnvironmentCM = CTexture::GetOrCreate2DTexture("$GenEnvironmentCM", size, size, 1, FT_DONT_STREAM | FT_USAGE_RENDERTARGET, nullptr, eTF_R16G16B16A16F);
	if (!ptexGenEnvironmentCM)
	{
		iLog->Log("Failed generating the cubemap texture");

		SAFE_RELEASE(ptexGenEnvironmentCM);
		return DynArray<std::uint16_t>{};
	}

	// Disable/set cvars that can affect cube map generation.
	SCvarOverrideHelper cvarOverrides[]
	{
		{ "e_CheckOcclusion",           0      },
		{ "e_CoverageBuffer",           0      },
		{ "e_StatObjBufferRenderTasks", 0      },
		{ "e_ViewDistRatio",            1000.f },
		{ "e_ViewDistRatioVegetation",  100.f  },
		{ "e_LodRatio",                 1000.f },
		{ "e_LodTransitionTime",        0.f    },
		{ "r_flares",                   0      },
		{ "r_ssdoHalfRes",              0      },
	};

	for (auto& cvarOverride : cvarOverrides)
		cvarOverride.ApplyValue();

	Vec3 oldSunDir, oldSunStr, oldSunRGB;
	float oldSkyKm, oldSkyKr, oldSkyG;
	if (CRenderer::CV_r_HideSunInCubemaps)
	{
		// if (isSkyProcedural)
		gEnv->p3DEngine->GetSkyLightParameters(oldSunDir, oldSunStr, oldSkyKm, oldSkyKr, oldSkyG, oldSunRGB);
		gEnv->p3DEngine->SetSkyLightParameters(oldSunDir, oldSunStr, oldSkyKm, oldSkyKr, 1.0f, oldSunRGB, true); // Hide sun disc
	}

	// TODO: allow cube-map super-sampling
	CRenderOutputPtr pRenderOutput = std::make_shared<CRenderOutput>(ptexGenEnvironmentCM, FRT_CLEAR, Clr_Transparent, 1.0f);

	IRenderer::SGraphicsPipelineDescription pipelineDesc;
	pipelineDesc.type = EGraphicsPipelineType::Standard;
	pipelineDesc.shaderFlags = SHDF_CUBEMAPGEN | SHDF_ALLOWPOSTPROCESS | SHDF_ALLOWHDR | SHDF_ZPASS | SHDF_NOASYNC | SHDF_ALLOW_AO | SHDF_ALLOW_SKY;
	SGraphicsPipelineKey pipelineKey = gcpRendD3D->CreateGraphicsPipeline(pipelineDesc);

	vecData.reserve(size * size * 6 * 4);
	for (int nSide = 0; nSide < 6; nSide++)
	{
		int32 waitFrames = max(0, CRendererCVars::CV_r_CubemapGenerationTimeout);
		while (waitFrames-- > 0)
		{
#if defined(FEATURE_SVO_GI)
			const bool is_svo_ready_pre_draw = gEnv->p3DEngine->IsSvoReady(true);
#endif

			gEnv->nMainFrameID++;
			DrawSceneToCubeSide(pRenderOutput, Pos, size, nSide, pipelineDesc.shaderFlags, pipelineKey);

			SStreamEngineOpenStats streamStats;
			gEnv->pSystem->GetStreamEngine()->GetStreamingOpenStatistics(streamStats);
			if (streamStats.nOpenRequestCountByType[eStreamTaskTypeGeometry] == 0
				&& streamStats.nOpenRequestCountByType[eStreamTaskTypeTexture] == 0
				&& streamStats.nOpenRequestCountByType[eStreamTaskTypeTerrain] == 0
#if defined(FEATURE_SVO_GI)
				&& gEnv->p3DEngine->IsSvoReady(true)
				&& is_svo_ready_pre_draw
#endif
				)
				break;

			// Update streaming engine
			CrySleep(10);
			gEnv->pSystem->GetStreamEngine()->Update(eStreamTaskTypeTerrain | eStreamTaskTypeTexture | eStreamTaskTypeGeometry);

			// Flush and garbage collect
			gcpRendD3D->ExecuteRenderThreadCommand([&]
			{
				GetDeviceObjectFactory().FlushToGPU(false, true);
			}, ERenderCommandFlags::FlushAndWait);
		}

		if (waitFrames < 0)
		{
			CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING,
			           "Cubemap generation timeout: some outstanding tasks didn't finish on time, generated cubemap might be incorrect.\n" \
			           "Consider increasing r_CubemapGenerationTimeout");
		}

		bool success = false;
		gcpRendD3D->ExecuteRenderThreadCommand([&]
		{
			CDeviceTexture* pDstDevTex = ptexGenEnvironmentCM->GetDevTexture();
			if (!pDstDevTex)
			{
				iLog->Log("Failed generating a cubemap: out of video memory");
				return;
			}

			pDstDevTex->DownloadToStagingResource(0, [&](void* pData, uint32 rowPitch, uint32 slicePitch)
			{
				const auto* pTarg = reinterpret_cast<const std::uint16_t*>(pData);
				const uint32 nLineStride = CTexture::TextureDataSize(size, 1, 1, 1, 1, eTF_R16G16B16A16F, eTM_None) / sizeof(*pTarg);

				// Copy vertically flipped image
				for (uint32 nLine = 0; nLine < size; ++nLine)
				{
					const auto src = pTarg + ((size - 1) - nLine) * nLineStride;
					std::copy(src, src + nLineStride, std::back_inserter(vecData));
				}

				success = true;
				return true;
			});

			// After download clean up temporal memory pools
			GetDeviceObjectFactory().FlushToGPU(false, true);
		}, ERenderCommandFlags::FlushAndWait);

		if (!success)
			return DynArray<std::uint16_t>{};
	}

	gcpRendD3D->DeleteGraphicsPipeline(pipelineKey);
	SAFE_RELEASE(ptexGenEnvironmentCM);

	for (auto& cvarOverride : cvarOverrides)
		cvarOverride.RestoreValue();

	if (CRenderer::CV_r_HideSunInCubemaps)
		gEnv->p3DEngine->SetSkyLightParameters(oldSunDir, oldSunStr, oldSkyKm, oldSkyKr, oldSkyG, oldSunRGB, true);

	float timeUsed = gEnv->pTimer->GetAsyncTime().GetSeconds() - timeStart;
	iLog->Log("Successfully finished generating a cubemap in %.1f sec", timeUsed);
#endif

	return vecData;
}

//////////////////////////////////////////////////////////////////////////

bool CTexture::GenerateMipMaps(bool bSetOrthoProj, bool bUseHW, bool bNormalMap)
{
	if (!(GetFlags() & FT_FORCE_MIPS)
	    || bSetOrthoProj || !bUseHW || bNormalMap) //todo: implement
		return false;

	PROFILE_LABEL_SCOPE("GENERATE_MIPS");

	CDeviceTexture* pTex = GetDevTexture();
	if (!pTex)
	{
		return false;
	}

	// all d3d11 devices support autogenmipmaps
	if (GetDevTexture()->IsWritable())
	{
		//	gcpRendD3D->GetDeviceContext().GenerateMips(m_pDeviceShaderResource);
		CMipmapGenPass(gcpRendD3D->FindGraphicsPipeline(SGraphicsPipelineKey::BaseGraphicsPipelineKey).get()).Execute(this);
	}

	return true;
}

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

	GetIRenderAuxGeom()->SetOrthographicProjection(true, 0.0f, width, height, 0.0f);

	if (name[0] == '*' && !name[1])
	{
		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		ResourcesMapItor it;
		for (it = pRL->m_RMap.begin(); it != pRL->m_RMap.end(); it++)
		{
			CTexture* tp = (CTexture*)it->second;
			if (tp && !tp->IsNoTexture())
			{
				if ((tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS)) && tp->GetDevTexture())
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
			if ((tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS)) && tp->GetDevTexture())
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
		//SetState(GS_NODEPTHTEST);
		CTexture* tp = UsedRT[i];
		int nSavedAccessFrameID = tp->m_nAccessFrameID;

		if (bOnlyIfUsedThisFrame)
			if (tp->m_nUpdateFrameID < gRenDev->GetRenderFrameID() - 2)
				continue;

		float posX = ScaleCoordX(x);
		float posY = ScaleCoordY(y);

		if (tp->GetTextureType() == eTT_2D)
			IRenderAuxImage::Draw2dImage(posX, posY, ScaleCoordX(fPicDimX - 2), ScaleCoordY(fPicDimY - 2), tp->GetID(), 0, 1, 1, 0, 0);

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

		IRenderAuxText::AColor color(0, 1, 0, 1);

		IRenderAuxText::Draw2dLabel(posX, posY, 1.0f, color, false, "%8s", pTexName);
		IRenderAuxText::Draw2dLabel(posX, posY += 10, 1.0f, color, false, "%d-%d", tp->m_nUpdateFrameID, tp->m_nAccessFrameID);
		IRenderAuxText::Draw2dLabel(posX, posY += 10, 1.0f, color, false, "%dx%d", tp->GetWidth(), tp->GetHeight());

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

	GetIRenderAuxGeom()->SetOrthographicProjection(false);
#endif
}

void CFlashTextureSourceBase::Advance(const float delta, bool isPaused)
{
	if (!m_pFlashPlayer)
		return;

	const auto& pFlashPlayer = m_pFlashPlayer->GetTempPtr();
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

	const auto& pFlashPlayer = m_pFlashPlayer->GetTempPtr();
	if (!pFlashPlayer)
		return false;

	SDynTexture* pDynTexture = GetDynTexture();
	assert(pDynTexture);
	if (!pDynTexture)
		return false;

	const int rtWidth  = GetWidth();
	const int rtHeight = GetHeight();
	if (!UpdateDynTex(rtWidth, rtHeight))
		return false;

	m_width  = std::min(pFlashPlayer->GetWidth (), rtWidth);
	m_height = std::min(pFlashPlayer->GetHeight(), rtHeight);
	m_aspectRatio = ((float)pFlashPlayer->GetWidth() / (float)m_width) * ((float)m_height / (float)pFlashPlayer->GetHeight());

	pFlashPlayer->SetViewport(0, 0, m_width, m_height, m_aspectRatio);
	pFlashPlayer->SetScissorRect(0, 0, m_width, m_height);
	pFlashPlayer->SetBackgroundAlpha(Clr_Transparent.a);

	m_lastVisibleFrameID = gRenDev->GetRenderFrameID();
	m_lastVisible = gEnv->pTimer->GetAsyncTime();

	{
		PROFILE_LABEL_SCOPE("FlashDynTexture");

		CScaleformPlayback::RenderFlashPlayerToTexture(pFlashPlayer.get(), pDynTexture->m_pTexture);

		pDynTexture->SetUpdateMask();
	}

	return true;
}

bool CFlashTextureSource::Update()
{
	if (!CFlashTextureSourceBase::Update())
		return false;
	if (!m_pMipMapper)
		m_pMipMapper = new CMipmapGenPass(gcpRendD3D->FindGraphicsPipeline(SGraphicsPipelineKey::BaseGraphicsPipelineKey).get());

	// calculate mip-maps after update, if mip-maps have been allocated
	{
		PROFILE_LABEL_SCOPE("FlashDynTexture MipMap");

		m_pMipMapper->Execute((CTexture*)m_pDynTexture->GetTexture());
	}

	return true;
}

void CFlashTextureSourceSharedRT::ProbeDepthStencilSurfaceCreation(int width, int height)
{
	CTexture* pTex = CRendererResources::CreateDepthTarget(width, height, Clr_Empty, eTF_Unknown);
	SAFE_RELEASE(pTex);
}

bool CFlashTextureSourceSharedRT::Update()
{
	if (!CFlashTextureSourceBase::Update())
		return false;
	if (!ms_pMipMapper)
		ms_pMipMapper = new CMipmapGenPass(gcpRendD3D->FindGraphicsPipeline(SGraphicsPipelineKey::BaseGraphicsPipelineKey).get());

	// calculate mip-maps after update, if mip-maps have been allocated
	{
		PROFILE_LABEL_SCOPE("FlashDynTexture MipMap");

		ms_pMipMapper->Execute((CTexture*)ms_pDynTexture->GetTexture());
	}

	return true;
}

void CTexture::CopySliceChain(CDeviceTexture* const pDstDevTex, int8 nDstNumMips, int nDstSliceOffset, int8 nDstMipOffset,
                              CDeviceTexture* const pSrcDevTex, int8 nSrcNumMips, int nSrcSliceOffset, int8 nSrcMipOffset, int nNumSlices, int8 nNumMips)
{
	CRY_ASSERT(pSrcDevTex && pDstDevTex);
	CRY_ASSERT_MESSAGE(nNumSlices == 1, "TODO: allow multiple slices being uploaded with the same command");

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	pDstDevTex->InitD3DTexture();
#endif
#if !defined(_RELEASE) || (CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE))
	D3DBaseTexture* pDstResource = pDstDevTex->GetBaseTexture();
	D3DBaseTexture* pSrcResource = pSrcDevTex->GetBaseTexture();
#endif

#ifndef _RELEASE
	if (!pDstResource) __debugbreak();
	if (!pSrcResource) __debugbreak();
#endif

	if (0)
	{
	}
#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	else if (!gcpRendD3D->m_pRT->IsRenderThread(true))
	{
		// We can use the move engine!

		CryAutoLock<CryCriticalSection> dmaLock(GetDeviceObjectFactory().m_dma1Lock);
		ID3D11DmaEngineContextX* pDMA = GetDeviceObjectFactory().m_pDMA1;

		for (int8 nMip = 0; nMip < nNumMips; ++nMip)
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

		for (int8 nMip = 0; nMip < nNumMips; ++nMip)
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

int16 CTexture::StreamCalculateMipsSignedFP(float fMipFactor) const
{
	assert(IsStreamed());

	float currentMipFactor = fMipFactor * gRenDev->GetMipDistFactor(m_nWidth, m_nHeight);
	float fMip = (0.5f * logf(max(currentMipFactor, 0.1f)) / gf_ln2 + (CRenderer::CV_r_TexturesStreamingMipBias + gRenDev->m_fTexturesStreamingGlobalMipFactor));
	int16 nMip = crymath::clamp_to<int16, float>(floorf(fMip * 256.0f), -SStreamFormatCode::MaxMips * 256.0f, SStreamFormatCode::MaxMips * 256.0f);
	const int16 nNewMip = std::min<int16>(nMip, (m_nMips - m_CacheFileHeader.m_nMipsPersistent) << 8);

	return nNewMip;
}

float CTexture::StreamCalculateMipFactor(int16 nMipsSigned) const
{
	float fMip = nMipsSigned / 256.0f;
	float currentMipFactor = expf((fMip - (CRenderer::CV_r_TexturesStreamingMipBias + gRenDev->m_fTexturesStreamingGlobalMipFactor)) * 2.0f * gf_ln2);
	float fMipFactor = currentMipFactor / gRenDev->GetMipDistFactor(m_nWidth, m_nHeight);

	return fMipFactor;
}
//===========================================================================================================
