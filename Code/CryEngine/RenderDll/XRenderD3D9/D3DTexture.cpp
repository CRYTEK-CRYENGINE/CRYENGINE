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

#undef min
#undef max

//===============================================================================

RenderTargetData::~RenderTargetData()
{
	CDeviceTexture* pTexMSAA = (CDeviceTexture*)m_pDeviceTextureMSAA;
	SAFE_RELEASE(pTexMSAA);
}

ResourceViewData::~ResourceViewData()
{
	for (size_t i = 0; i < m_ResourceViews.size(); ++i)
	{
		const SResourceView& rv = m_ResourceViews[i];
		switch (rv.m_Desc.eViewType)
		{
		case SResourceView::eShaderResourceView:
			{
				D3DShaderResource* pView = static_cast<D3DShaderResource*>(rv.m_pDeviceResourceView);
				SAFE_RELEASE(pView);
				break;
			}
		case SResourceView::eRenderTargetView:
			{
				D3DSurface* pView = static_cast<D3DSurface*>(rv.m_pDeviceResourceView);
				SAFE_RELEASE(pView);
				break;
			}
		case SResourceView::eDepthStencilView:
			{
				D3DDepthSurface* pView = static_cast<D3DDepthSurface*>(rv.m_pDeviceResourceView);
				SAFE_RELEASE(pView);
				break;
			}
#if !CRY_PLATFORM_ORBIS
		case SResourceView::eUnorderedAccessView:
			{
				D3DUAV* pView = static_cast<D3DUAV*>(rv.m_pDeviceResourceView);
				SAFE_RELEASE(pView);
				break;
			}
#endif
		default:
			assert(false);
		}
	}
}
//===============================================================================

#if defined(TEXTURE_GET_SYSTEM_COPY_SUPPORT)
byte* CTexture::Convert(byte* pSrc, int nWidth, int nHeight, int nMips, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nOutMips, int& nOutSize, bool bLinear)
{
	nOutSize = 0;
	byte* pDst = NULL;
	if (eTFSrc == eTFDst && nMips == nOutMips)
		return pSrc;
	CD3D9Renderer* r = gcpRendD3D;
	DXGI_FORMAT DeviceFormatSRC = (DXGI_FORMAT)DeviceFormatFromTexFormat(eTFSrc);
	if (eTFDst == eTF_L8)
		eTFDst = eTF_A8;
	DXGI_FORMAT DeviceFormatDST = (DXGI_FORMAT)DeviceFormatFromTexFormat(eTFDst);
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

	if (IsDeviceFormatTypeless(m_pPixelFormat->DeviceFormat))
	{
		iLog->Log("Error: CTexture::GetSurface: typeless formats can't be specified for RTVs, failed to create surface for the texture %s", GetSourceName());
		return NULL;
	}

	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	HRESULT hr = S_OK;
	D3DTexture* pID3DTexture = NULL;
	D3DTexture* pID3DTexture3D = NULL;
	D3DTexture* pID3DTextureCube = NULL;
	D3DSurface* pTargSurf = m_pDeviceRTV;
	const bool bUseMultisampledRTV = ((m_nFlags & FT_USAGE_MSAA) && m_bUseMultisampledRTV) != 0;
	if (bUseMultisampledRTV)
		pTargSurf = m_pDeviceRTVMS;

	if (!pTargSurf)
	{
		MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "Create Render Target: %s", GetSourceName());
		int nMipLevel = 0;
		int nSlice = 0;
		int nSliceCount = -1;

		if (m_eTT == eTT_Cube)
		{
			nMipLevel = (m_nFlags & FT_FORCE_MIPS) ? min((int)(m_nMips - 1), nLevel) : 0;
			nSlice = nCMSide;
			nSliceCount = 1;
		}
		pTargSurf = (D3DSurface*)GetResourceView(SResourceView::RenderTargetView(m_eTFDst, nSlice, nSliceCount, nMipLevel, bUseMultisampledRTV)).m_pDeviceResourceView;

		if (bUseMultisampledRTV)
			m_pDeviceRTVMS = pTargSurf;
		else
			m_pDeviceRTV = pTargSurf;

	}
	assert(hr == S_OK);

	if (FAILED(hr))
		pTargSurf = NULL;

	return pTargSurf;
}

D3DSurface* CTexture::GetSurface(int nCMSide, int nLevel) const
{
	if (!m_pDevTexture)
		return NULL;

	if (IsDeviceFormatTypeless(m_pPixelFormat->DeviceFormat))
	{
		iLog->Log("Error: CTexture::GetSurface: typeless formats can't be specified for RTVs, failed to create surface for the texture %s", GetSourceName());
		return NULL;
	}

	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	D3DSurface* pTargSurf = m_pDeviceRTV;
	const bool bUseMultisampledRTV = ((m_nFlags & FT_USAGE_MSAA) && m_bUseMultisampledRTV) != 0;
	if (bUseMultisampledRTV)
		pTargSurf = m_pDeviceRTVMS;

	return pTargSurf;
}

D3DPOOL CTexture::GetPool()
{
	if (!m_pDevTexture)
		return D3DPOOL_SYSTEMMEM;
	HRESULT hr = S_OK;
	int nLevel = 0;
	D3DPOOL Pool = D3DPOOL_MANAGED;
	if (m_eTT == eTT_2D || m_eTT == eTT_Auto2D || m_eTT == eTT_Dyn2D)
	{
		D3D11_TEXTURE2D_DESC Desc;
		D3DTexture* pID3DTexture = m_pDevTexture->Get2DTexture();
		pID3DTexture->GetDesc(&Desc);
		if (Desc.BindFlags & D3D11_BIND_RENDER_TARGET)
			Pool = D3DPOOL_DEFAULT;
	}
	else if (m_eTT == eTT_Cube)
	{
		D3D11_TEXTURE2D_DESC Desc;
		D3DCubeTexture* pID3DTexture = m_pDevTexture->GetCubeTexture();
		pID3DTexture->GetDesc(&Desc);
		if (Desc.BindFlags & D3D11_BIND_RENDER_TARGET)
			Pool = D3DPOOL_DEFAULT;
	}
	else if (m_eTT == eTT_3D)
	{
		D3D11_TEXTURE3D_DESC Desc;
		D3DVolumeTexture* pID3DTexture = m_pDevTexture->GetVolumeTexture();
		pID3DTexture->GetDesc(&Desc);
		if (Desc.BindFlags & D3D11_BIND_RENDER_TARGET)
			Pool = D3DPOOL_DEFAULT;
	}
	assert(hr == S_OK);

	return Pool;
}

//===============================================================================

bool CTexture::IsDeviceFormatTypeless(D3DFormat nFormat)
{
	switch (nFormat)
	{
	//128 bits
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_TYPELESS:

	//64 bits
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:

	//32 bits
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:

	//16 bits
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R16_TYPELESS:

	//8 bits
	case DXGI_FORMAT_R8_TYPELESS:

	//block formats
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC5_TYPELESS:

#if !CRY_PLATFORM_ORBIS
	case DXGI_FORMAT_BC6H_TYPELESS:
#endif
	case DXGI_FORMAT_BC7_TYPELESS:
		return true;

	default:
		break;
	}

	return false;
}

bool CTexture::IsDeviceFormatSRGBReadable(D3DFormat nFormat)
{
	switch (nFormat)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return true;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return true;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return true;

	case DXGI_FORMAT_BC1_UNORM:
		return true;
	case DXGI_FORMAT_BC2_UNORM:
		return true;
	case DXGI_FORMAT_BC3_UNORM:
		return true;
	case DXGI_FORMAT_BC7_UNORM:
		return true;

#if defined(OPENGL)
	case DXGI_FORMAT_ETC2_UNORM:
		return true;
	case DXGI_FORMAT_ETC2A_UNORM:
		return true;
#endif //defined(OPENGL)

	default:
		break;
	}

	return false;
}

// this function is valid for FT_USAGE_DEPTHSTENCIL textures only
D3DFormat CTexture::DeviceFormatFromTexFormat(ETEX_Format eTF)
{
	switch (eTF)
	{
	case eTF_R8G8B8A8:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case eTF_R8G8B8A8S:
		return DXGI_FORMAT_R8G8B8A8_SNORM;

	case eTF_R1:
		return DXGI_FORMAT_R1_UNORM;
	case eTF_A8:
		return DXGI_FORMAT_A8_UNORM;
	case eTF_R8:
		return DXGI_FORMAT_R8_UNORM;
	case eTF_R8S:
		return DXGI_FORMAT_R8_SNORM;
	case eTF_R16:
		return DXGI_FORMAT_R16_UNORM;
	case eTF_R16F:
		return DXGI_FORMAT_R16_FLOAT;
	case eTF_R32F:
		return DXGI_FORMAT_R32_FLOAT;
	case eTF_R8G8:
		return DXGI_FORMAT_R8G8_UNORM;
	case eTF_R8G8S:
		return DXGI_FORMAT_R8G8_SNORM;
	case eTF_R16G16:
		return DXGI_FORMAT_R16G16_UNORM;
	case eTF_R16G16S:
		return DXGI_FORMAT_R16G16_SNORM;
	case eTF_R16G16F:
		return DXGI_FORMAT_R16G16_FLOAT;
	case eTF_R11G11B10F:
		return DXGI_FORMAT_R11G11B10_FLOAT;
	case eTF_R10G10B10A2:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case eTF_R16G16B16A16:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	case eTF_R16G16B16A16S:
		return DXGI_FORMAT_R16G16B16A16_SNORM;
	case eTF_R16G16B16A16F:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case eTF_R32G32B32A32F:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;

	case eTF_BC1:
		return DXGI_FORMAT_BC1_UNORM;
	case eTF_BC2:
		return DXGI_FORMAT_BC2_UNORM;
	case eTF_BC3:
		return DXGI_FORMAT_BC3_UNORM;
	case eTF_BC4U:
		return DXGI_FORMAT_BC4_UNORM;
	case eTF_BC4S:
		return DXGI_FORMAT_BC4_SNORM;
	case eTF_BC5U:
		return DXGI_FORMAT_BC5_UNORM;
	case eTF_BC5S:
		return DXGI_FORMAT_BC5_SNORM;
	case eTF_BC6UH:
		return DXGI_FORMAT_BC6H_UF16;
	case eTF_BC6SH:
		return DXGI_FORMAT_BC6H_SF16;
	case eTF_BC7:
		return DXGI_FORMAT_BC7_UNORM;
	case eTF_R9G9B9E5:
		return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;

	// hardware depth buffers
	case eTF_D16:
		return DXGI_FORMAT_D16_UNORM;
	case eTF_D24S8:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case eTF_D32F:
		return DXGI_FORMAT_D32_FLOAT;
	case eTF_D32FS8:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	// only available as hardware format under DX11.1 with DXGI 1.2
	case eTF_B5G6R5:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case eTF_B5G5R5:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	//      case eTF_B4G4R4A4:                      return DXGI_FORMAT_B4G4R4A4_UNORM;
	case eTF_B4G4R4A4:
		return DXGI_FORMAT_UNKNOWN;

#if defined(OPENGL)
	// only available as hardware format under OpenGL
	case eTF_EAC_R11:
		return DXGI_FORMAT_EAC_R11_UNORM;
	case eTF_EAC_RG11:
		return DXGI_FORMAT_EAC_RG11_UNORM;
	case eTF_ETC2:
		return DXGI_FORMAT_ETC2_UNORM;
	case eTF_ETC2A:
		return DXGI_FORMAT_ETC2A_UNORM;
#endif //defined(OPENGL)

	// only available as hardware format under DX9
	case eTF_A8L8:
		return DXGI_FORMAT_UNKNOWN;
	case eTF_L8:
		return DXGI_FORMAT_UNKNOWN;
	case eTF_L8V8U8:
		return DXGI_FORMAT_UNKNOWN;
	case eTF_B8G8R8:
		return DXGI_FORMAT_UNKNOWN;
	case eTF_L8V8U8X8:
		return DXGI_FORMAT_UNKNOWN;
	case eTF_B8G8R8X8:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	case eTF_B8G8R8A8:
		return DXGI_FORMAT_B8G8R8A8_UNORM;

	default:
		assert(0);
	}

	return DXGI_FORMAT_UNKNOWN;
}

uint32 CTexture::WriteMaskFromTexFormat(ETEX_Format eTF)
{
	switch (eTF)
	{
	case eTF_R8G8B8A8:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_R8G8B8A8S:
		return D3D11_COLOR_WRITE_ENABLE_ALL;

	case eTF_R1:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_A8:
		return D3D11_COLOR_WRITE_ENABLE_ALPHA;
	case eTF_R8:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_R8S:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_R16:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_R16F:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_R32F:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_R8G8:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_R8G8S:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_R16G16:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_R16G16S:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_R16G16F:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_R11G11B10F:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_R10G10B10A2:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_R16G16B16A16:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_R16G16B16A16S:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_R16G16B16A16F:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_R32G32B32A32F:
		return D3D11_COLOR_WRITE_ENABLE_ALL;

	case eTF_BC1:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_BC2:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_BC3:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_BC4U:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_BC4S:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_BC5U:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_BC5S:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_BC6UH:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_BC6SH:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_BC7:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_R9G9B9E5:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;

	// hardware depth buffers
	case eTF_D16:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_D24S8:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_D32F:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_D32FS8:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;

	// only available as hardware format under DX11.1 with DXGI 1.2
	case eTF_B5G6R5:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_B5G5R5:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	//  case eTF_B4G4R4A4:                      return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_B4G4R4A4:
		return 0;

#if defined(OPENGL)
	// only available as hardware format under OpenGL
	case eTF_EAC_R11:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_EAC_RG11:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_ETC2:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_ETC2A:
		return DXGI_FORMAT_ETC2A_UNORM;
#endif //defined(OPENGL)

	// only available as hardware format under DX9
	case eTF_A8L8:
		return 0;
	case eTF_L8:
		return 0;
	case eTF_L8V8U8:
		return 0;
	case eTF_B8G8R8:
		return 0;
	case eTF_L8V8U8X8:
		return 0;
	case eTF_B8G8R8X8:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_B8G8R8A8:
		return D3D11_COLOR_WRITE_ENABLE_ALL;

	default:
		return 0;
	}

	return 0;
}

D3DFormat CTexture::GetD3DLinFormat(D3DFormat nFormat)
{
	return nFormat;
}

D3DFormat CTexture::ConvertToSRGBFmt(D3DFormat fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

	case DXGI_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM_SRGB;
	case DXGI_FORMAT_BC2_UNORM:
		return DXGI_FORMAT_BC2_UNORM_SRGB;
	case DXGI_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM_SRGB;
	case DXGI_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM_SRGB;

#if defined(OPENGL)
	case DXGI_FORMAT_ETC2_UNORM:
		return DXGI_FORMAT_ETC2_UNORM_SRGB;
	case DXGI_FORMAT_ETC2A_UNORM:
		return DXGI_FORMAT_ETC2A_UNORM_SRGB;
#endif //defined(OPENGL)

	// AntonK: we don't need sRGB space for fp formats, because there is enough precision
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;

	default:
		assert(0);
	}

	return fmt;
}

D3DFormat CTexture::ConvertToSignedFmt(D3DFormat fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_R8_UNORM:
		return DXGI_FORMAT_R8_SNORM;
	case DXGI_FORMAT_R8G8_UNORM:
		return DXGI_FORMAT_R8G8_SNORM;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case DXGI_FORMAT_R16_UNORM:
		return DXGI_FORMAT_R16_SNORM;
	case DXGI_FORMAT_R16G16_UNORM:
		return DXGI_FORMAT_R16G16_SNORM;
	case DXGI_FORMAT_R16G16B16A16_UNORM:
		return DXGI_FORMAT_R16G16B16A16_SNORM;
	case DXGI_FORMAT_BC4_UNORM:
		return DXGI_FORMAT_BC4_SNORM;
	case DXGI_FORMAT_BC5_UNORM:
		return DXGI_FORMAT_BC5_SNORM;
	case DXGI_FORMAT_BC6H_UF16:
		return DXGI_FORMAT_BC6H_SF16;

	case DXGI_FORMAT_R32G32B32A32_UINT:
		return DXGI_FORMAT_R32G32B32A32_SINT;
	case DXGI_FORMAT_R32G32B32_UINT:
		return DXGI_FORMAT_R32G32B32_SINT;
	case DXGI_FORMAT_R16G16B16A16_UINT:
		return DXGI_FORMAT_R16G16B16A16_SINT;
	case DXGI_FORMAT_R32G32_UINT:
		return DXGI_FORMAT_R32G32_SINT;
	case DXGI_FORMAT_R8G8B8A8_UINT:
		return DXGI_FORMAT_R8G8B8A8_SINT;
	case DXGI_FORMAT_R16G16_UINT:
		return DXGI_FORMAT_R16G16_SINT;
	case DXGI_FORMAT_R32_UINT:
		return DXGI_FORMAT_R32_SINT;
	case DXGI_FORMAT_R8G8_UINT:
		return DXGI_FORMAT_R8G8_SINT;
	case DXGI_FORMAT_R16_UINT:
		return DXGI_FORMAT_R16_SINT;
	case DXGI_FORMAT_R8_UINT:
		return DXGI_FORMAT_R8_SINT;

	default:
		assert(0);
	}

	return fmt;
}

ETEX_Format CTexture::TexFormatFromDeviceFormat(D3DFormat nFormat)
{
	switch (nFormat)
	{
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		return eTF_R8G8B8A8;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return eTF_R8G8B8A8;
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return eTF_R8G8B8A8;
	case DXGI_FORMAT_R8G8B8A8_SNORM:
		return eTF_R8G8B8A8S;

	case DXGI_FORMAT_R1_UNORM:
		return eTF_R1;
	case DXGI_FORMAT_A8_UNORM:
		return eTF_A8;
	case DXGI_FORMAT_R8_UNORM:
		return eTF_R8;
	case DXGI_FORMAT_R8_SNORM:
		return eTF_R8S;
	case DXGI_FORMAT_R16_UNORM:
		return eTF_R16;
	case DXGI_FORMAT_R16_FLOAT:
		return eTF_R16F;
	case DXGI_FORMAT_R16_TYPELESS:
		return eTF_R16F;
	case DXGI_FORMAT_R32_FLOAT:
		return eTF_R32F;
	case DXGI_FORMAT_R32_TYPELESS:
		return eTF_R32F;
	case DXGI_FORMAT_R8G8_UNORM:
		return eTF_R8G8;
	case DXGI_FORMAT_R8G8_SNORM:
		return eTF_R8G8S;
	case DXGI_FORMAT_R16G16_UNORM:
		return eTF_R16G16;
	case DXGI_FORMAT_R16G16_SNORM:
		return eTF_R16G16S;
	case DXGI_FORMAT_R16G16_FLOAT:
		return eTF_R16G16F;
	case DXGI_FORMAT_R11G11B10_FLOAT:
		return eTF_R11G11B10F;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		return eTF_R10G10B10A2;
	case DXGI_FORMAT_R16G16B16A16_UNORM:
		return eTF_R16G16B16A16;
	case DXGI_FORMAT_R16G16B16A16_SNORM:
		return eTF_R16G16B16A16S;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return eTF_R16G16B16A16F;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return eTF_R32G32B32A32F;
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		return eTF_R32G32B32A32F;

	case DXGI_FORMAT_BC1_TYPELESS:
		return eTF_BC1;
	case DXGI_FORMAT_BC1_UNORM:
		return eTF_BC1;
	case DXGI_FORMAT_BC1_UNORM_SRGB:
		return eTF_BC1;
	case DXGI_FORMAT_BC2_TYPELESS:
		return eTF_BC2;
	case DXGI_FORMAT_BC2_UNORM:
		return eTF_BC2;
	case DXGI_FORMAT_BC2_UNORM_SRGB:
		return eTF_BC2;
	case DXGI_FORMAT_BC3_TYPELESS:
		return eTF_BC3;
	case DXGI_FORMAT_BC3_UNORM:
		return eTF_BC3;
	case DXGI_FORMAT_BC3_UNORM_SRGB:
		return eTF_BC3;
	case DXGI_FORMAT_BC4_TYPELESS:
		return eTF_BC4U;
	case DXGI_FORMAT_BC4_UNORM:
		return eTF_BC4U;
	case DXGI_FORMAT_BC4_SNORM:
		return eTF_BC4S;
	case DXGI_FORMAT_BC5_TYPELESS:
		return eTF_BC5U;
	case DXGI_FORMAT_BC5_UNORM:
		return eTF_BC5U;
	case DXGI_FORMAT_BC5_SNORM:
		return eTF_BC5S;
	case DXGI_FORMAT_BC6H_UF16:
		return eTF_BC6UH;
	case DXGI_FORMAT_BC6H_SF16:
		return eTF_BC6SH;
	case DXGI_FORMAT_BC7_TYPELESS:
		return eTF_BC7;
	case DXGI_FORMAT_BC7_UNORM:
		return eTF_BC7;
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return eTF_BC7;
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		return eTF_R9G9B9E5;

	// hardware depth buffers
	case DXGI_FORMAT_D16_UNORM:
		return eTF_D16;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return eTF_D24S8;
	case DXGI_FORMAT_D32_FLOAT:
		return eTF_D32F;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return eTF_D32FS8;

	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		return eTF_D24S8;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return eTF_D24S8;
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		return eTF_D32FS8;
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return eTF_D32FS8;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return eTF_D32FS8;

	// only available as hardware format under DX11.1 with DXGI 1.2
	case DXGI_FORMAT_B5G6R5_UNORM:
		return eTF_B5G6R5;
	case DXGI_FORMAT_B5G5R5A1_UNORM:
		return eTF_B5G5R5;
		//	case DXGI_FORMAT_B4G4R4A4_UNORM:        return eTF_B4G4R4A4;

#if defined(OPENGL)
	// only available as hardware format under OpenGL
	case DXGI_FORMAT_EAC_R11_UNORM:
		return eTF_EAC_R11;
	case DXGI_FORMAT_EAC_RG11_UNORM:
		return eTF_EAC_RG11;
	case DXGI_FORMAT_ETC2_UNORM:
		return eTF_ETC2;
	case DXGI_FORMAT_ETC2_UNORM_SRGB:
		return eTF_ETC2;
	case DXGI_FORMAT_ETC2A_UNORM:
		return eTF_ETC2A;
	case DXGI_FORMAT_ETC2A_UNORM_SRGB:
		return eTF_ETC2A;
#endif //defined(OPENGL)

	// only available as hardware format under DX9
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		return eTF_B8G8R8A8;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return eTF_B8G8R8A8;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return eTF_B8G8R8A8;
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		return eTF_B8G8R8X8;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return eTF_B8G8R8X8;
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return eTF_B8G8R8X8;

	default:
		assert(0);
	}

	return eTF_Unknown;
}

// this function is valid for FT_USAGE_DEPTHSTENCIL textures only
D3DFormat CTexture::ConvertToDepthStencilFmt(D3DFormat nFormat)
{
	switch (nFormat)
	{
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_D16_UNORM;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_D32_FLOAT;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	default:
		assert(nFormat == DXGI_FORMAT_D16_UNORM || nFormat == DXGI_FORMAT_D24_UNORM_S8_UINT || nFormat == DXGI_FORMAT_D32_FLOAT || nFormat == DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
	}

	return nFormat;
}

D3DFormat CTexture::ConvertToStencilOnlyFmt(D3DFormat nFormat)
{
	switch (nFormat)
	{
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

	default:
		assert(nFormat == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT || nFormat == DXGI_FORMAT_X24_TYPELESS_G8_UINT);
	}

	return nFormat;
}

D3DFormat CTexture::ConvertToDepthOnlyFmt(D3DFormat nFormat)
{
	//handle special cases
	switch (nFormat)
	{
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_UNORM;
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

	default:
		break;                                       //pass through
	}

	return nFormat;
}

D3DFormat CTexture::ConvertToTypelessFmt(D3DFormat fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
	case DXGI_FORMAT_R8G8B8A8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
	case DXGI_FORMAT_R8G8B8A8_UINT:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
	case DXGI_FORMAT_R8G8B8A8_SINT:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;

	case DXGI_FORMAT_R8_UNORM:
		return DXGI_FORMAT_R8_TYPELESS;
	case DXGI_FORMAT_R8_SNORM:
		return DXGI_FORMAT_R8_TYPELESS;
	case DXGI_FORMAT_R8_UINT:
		return DXGI_FORMAT_R8_TYPELESS;
	case DXGI_FORMAT_R8_SINT:
		return DXGI_FORMAT_R8_TYPELESS;

	case DXGI_FORMAT_R16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	case DXGI_FORMAT_R16_SNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	case DXGI_FORMAT_R16_FLOAT:
		return DXGI_FORMAT_R16_TYPELESS;
	case DXGI_FORMAT_R16_UINT:
		return DXGI_FORMAT_R16_TYPELESS;
	case DXGI_FORMAT_R16_SINT:
		return DXGI_FORMAT_R16_TYPELESS;

	case DXGI_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_R32_UINT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_R32_SINT:
		return DXGI_FORMAT_R32_TYPELESS;

	case DXGI_FORMAT_R8G8_UNORM:
		return DXGI_FORMAT_R8G8_TYPELESS;
	case DXGI_FORMAT_R8G8_SNORM:
		return DXGI_FORMAT_R8G8_TYPELESS;
	case DXGI_FORMAT_R8G8_UINT:
		return DXGI_FORMAT_R8G8_TYPELESS;
	case DXGI_FORMAT_R8G8_SINT:
		return DXGI_FORMAT_R8G8_TYPELESS;

	case DXGI_FORMAT_R16G16_FLOAT:
		return DXGI_FORMAT_R16G16_TYPELESS;
	case DXGI_FORMAT_R16G16_UNORM:
		return DXGI_FORMAT_R16G16_TYPELESS;
	case DXGI_FORMAT_R16G16_SNORM:
		return DXGI_FORMAT_R16G16_TYPELESS;
	case DXGI_FORMAT_R16G16_UINT:
		return DXGI_FORMAT_R16G16_TYPELESS;
	case DXGI_FORMAT_R16G16_SINT:
		return DXGI_FORMAT_R16G16_TYPELESS;

	case DXGI_FORMAT_R32G32_FLOAT:
		return DXGI_FORMAT_R32G32_TYPELESS;
	case DXGI_FORMAT_R32G32_UINT:
		return DXGI_FORMAT_R32G32_TYPELESS;
	case DXGI_FORMAT_R32G32_SINT:
		return DXGI_FORMAT_R32G32_TYPELESS;

	case DXGI_FORMAT_R32G32B32_FLOAT:
		return DXGI_FORMAT_R32G32B32_TYPELESS;
	case DXGI_FORMAT_R32G32B32_UINT:
		return DXGI_FORMAT_R32G32B32_TYPELESS;
	case DXGI_FORMAT_R32G32B32_SINT:
		return DXGI_FORMAT_R32G32B32_TYPELESS;

	case DXGI_FORMAT_R10G10B10A2_UNORM:
		return DXGI_FORMAT_R10G10B10A2_TYPELESS;
	case DXGI_FORMAT_R10G10B10A2_UINT:
		return DXGI_FORMAT_R10G10B10A2_TYPELESS;

	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_TYPELESS;
	case DXGI_FORMAT_R16G16B16A16_UNORM:
		return DXGI_FORMAT_R16G16B16A16_TYPELESS;
	case DXGI_FORMAT_R16G16B16A16_SNORM:
		return DXGI_FORMAT_R16G16B16A16_TYPELESS;
	case DXGI_FORMAT_R16G16B16A16_UINT:
		return DXGI_FORMAT_R16G16B16A16_TYPELESS;
	case DXGI_FORMAT_R16G16B16A16_SINT:
		return DXGI_FORMAT_R16G16B16A16_TYPELESS;

	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_TYPELESS;
	case DXGI_FORMAT_R32G32B32A32_UINT:
		return DXGI_FORMAT_R32G32B32A32_TYPELESS;
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return DXGI_FORMAT_R32G32B32A32_TYPELESS;

	case DXGI_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_TYPELESS;
	case DXGI_FORMAT_BC1_UNORM_SRGB:
		return DXGI_FORMAT_BC1_TYPELESS;

	case DXGI_FORMAT_BC2_UNORM:
		return DXGI_FORMAT_BC2_TYPELESS;
	case DXGI_FORMAT_BC2_UNORM_SRGB:
		return DXGI_FORMAT_BC2_TYPELESS;

	case DXGI_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_TYPELESS;
	case DXGI_FORMAT_BC3_UNORM_SRGB:
		return DXGI_FORMAT_BC3_TYPELESS;

	case DXGI_FORMAT_BC4_UNORM:
		return DXGI_FORMAT_BC4_TYPELESS;
	case DXGI_FORMAT_BC4_SNORM:
		return DXGI_FORMAT_BC4_TYPELESS;

	case DXGI_FORMAT_BC5_UNORM:
		return DXGI_FORMAT_BC5_TYPELESS;
	case DXGI_FORMAT_BC5_SNORM:
		return DXGI_FORMAT_BC5_TYPELESS;

#if !CRY_PLATFORM_ORBIS
	case DXGI_FORMAT_BC6H_UF16:
		return DXGI_FORMAT_BC6H_TYPELESS;
	case DXGI_FORMAT_BC6H_SF16:
		return DXGI_FORMAT_BC6H_TYPELESS;
#endif
	case DXGI_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_TYPELESS;
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return DXGI_FORMAT_BC7_TYPELESS;

	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32G8X24_TYPELESS;

	// todo: add missing formats if they found required
	default:
		assert(0);
	}

	return fmt;
}

bool CTexture::IsFormatSupported(ETEX_Format eTFDst)
{
	const SPixFormatSupport* rd = &gcpRendD3D->m_hwTexFormatSupport;

	int D3DFmt = DeviceFormatFromTexFormat(eTFDst);
	if (!D3DFmt)
	{
		return false;
	}
	SPixFormat* pFmt;
	for (pFmt = rd->m_FirstPixelFormat; pFmt; pFmt = pFmt->Next)
	{
		if (pFmt->DeviceFormat == D3DFmt && pFmt->IsValid())
		{
			return true;
		}
	}
	return false;
}

ETEX_Format CTexture::ClosestFormatSupported(ETEX_Format eTFDst)
{
	return ClosestFormatSupported(eTFDst, m_pPixelFormat);
}

ETEX_Format CTexture::ClosestFormatSupported(ETEX_Format eTFDst, const SPixFormat*& pPF)
{
	const SPixFormatSupport* rd = &gcpRendD3D->m_hwTexFormatSupport;

	switch (eTFDst)
	{
	case eTF_R8G8B8A8S:
		if (rd->m_FormatR8G8B8A8S.IsValid())
		{
			pPF = &rd->m_FormatR8G8B8A8S;
			return eTF_R8G8B8A8S;
		}
		return eTF_Unknown;
	case eTF_R8G8B8A8:
		if (rd->m_FormatR8G8B8A8.BitsPerPixel)
		{
			pPF = &rd->m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		return eTF_Unknown;

	case eTF_B5G5R5:
		if (rd->m_FormatB5G5R5.IsValid())
		{
			pPF = &rd->m_FormatB5G5R5;
			return eTF_B5G5R5;
		}
	case eTF_B5G6R5:
		if (rd->m_FormatB5G6R5.IsValid())
		{
			pPF = &rd->m_FormatB5G6R5;
			return eTF_B5G6R5;
		}

	case eTF_B8G8R8X8:
		if (rd->m_FormatB8G8R8X8.BitsPerPixel)
		{
			pPF = &rd->m_FormatB8G8R8X8;
			return eTF_B8G8R8X8;
		}
		return eTF_Unknown;
	case eTF_B4G4R4A4:
		if (rd->m_FormatB4G4R4A4.BitsPerPixel)
		{
			pPF = &rd->m_FormatB4G4R4A4;
			return eTF_B4G4R4A4;
		}
	case eTF_B8G8R8A8:
		if (rd->m_FormatB8G8R8A8.BitsPerPixel)
		{
			pPF = &rd->m_FormatB8G8R8A8;
			return eTF_B8G8R8A8;
		}
		return eTF_Unknown;

	case eTF_A8:
		if (rd->m_FormatA8.IsValid())
		{
			pPF = &rd->m_FormatA8;
			return eTF_A8;
		}
		return eTF_Unknown;

	case eTF_R1:
		if (rd->m_FormatR1.IsValid())
		{
			pPF = &rd->m_FormatR1;
			return eTF_R1;
		}
		return eTF_Unknown;

	case eTF_R8:
		if (rd->m_FormatR8.IsValid())
		{
			pPF = &rd->m_FormatR8;
			return eTF_R8;
		}
		return eTF_Unknown;

	case eTF_R8S:
		if (rd->m_FormatR8S.IsValid())
		{
			pPF = &rd->m_FormatR8S;
			return eTF_R8S;
		}
		return eTF_Unknown;

	case eTF_R16:
		if (rd->m_FormatR16.IsValid())
		{
			pPF = &rd->m_FormatR16;
			return eTF_R16;
		}
		if (rd->m_FormatR16G16.IsValid())
		{
			pPF = &rd->m_FormatR16G16;
			return eTF_R16G16;
		}
		return eTF_Unknown;

	case eTF_R16F:
		if (rd->m_FormatR16F.IsValid())
		{
			pPF = &rd->m_FormatR16F;
			return eTF_R16F;
		}
		if (rd->m_FormatR16G16F.IsValid())
		{
			pPF = &rd->m_FormatR16G16F;
			return eTF_R16G16F;
		}
		return eTF_Unknown;

	case eTF_R32F:
		if (rd->m_FormatR32F.IsValid())
		{
			pPF = &rd->m_FormatR32F;
			return eTF_R32F;
		}
		if (rd->m_FormatR16G16F.IsValid())
		{
			pPF = &rd->m_FormatR16G16F;
			return eTF_R16G16F;
		}
		return eTF_Unknown;

	case eTF_R8G8:
		if (rd->m_FormatR8G8.IsValid())
		{
			pPF = &rd->m_FormatR8G8;
			return eTF_R8G8;
		}
		return eTF_Unknown;

	case eTF_R8G8S:
		if (rd->m_FormatR8G8S.IsValid())
		{
			pPF = &rd->m_FormatR8G8S;
			return eTF_R8G8S;
		}
		return eTF_Unknown;

	case eTF_R16G16:
		if (rd->m_FormatR16G16.IsValid())
		{
			pPF = &rd->m_FormatR16G16;
			return eTF_R16G16;
		}
		return eTF_Unknown;

	case eTF_R16G16S:
		if (rd->m_FormatR16G16S.IsValid())
		{
			pPF = &rd->m_FormatR16G16S;
			return eTF_R16G16S;
		}
		return eTF_Unknown;

	case eTF_R16G16F:
		if (rd->m_FormatR16G16F.IsValid())
		{
			pPF = &rd->m_FormatR16G16F;
			return eTF_R16G16F;
		}
		return eTF_Unknown;

	case eTF_R11G11B10F:
		if (rd->m_FormatR11G11B10F.IsValid())
		{
			pPF = &rd->m_FormatR11G11B10F;
			return eTF_R11G11B10F;
		}
		return eTF_Unknown;

	case eTF_R10G10B10A2:
		if (rd->m_FormatR10G10B10A2.IsValid())
		{
			pPF = &rd->m_FormatR10G10B10A2;
			return eTF_R10G10B10A2;
		}
		return eTF_Unknown;

	case eTF_R16G16B16A16:
		if (rd->m_FormatR16G16B16A16.IsValid())
		{
			pPF = &rd->m_FormatR16G16B16A16;
			return eTF_R16G16B16A16;
		}
		return eTF_Unknown;

	case eTF_R16G16B16A16S:
		if (rd->m_FormatR16G16B16A16S.IsValid())
		{
			pPF = &rd->m_FormatR16G16B16A16S;
			return eTF_R16G16B16A16S;
		}
		return eTF_Unknown;

	case eTF_R16G16B16A16F:
		if (rd->m_FormatR16G16B16A16F.IsValid())
		{
			pPF = &rd->m_FormatR16G16B16A16F;
			return eTF_R16G16B16A16F;
		}
		return eTF_Unknown;

	case eTF_R32G32B32A32F:
		if (rd->m_FormatR32G32B32A32F.IsValid())
		{
			pPF = &rd->m_FormatR32G32B32A32F;
			return eTF_R32G32B32A32F;
		}
		return eTF_Unknown;

	case eTF_BC1:
		if (rd->m_FormatBC1.IsValid())
		{
			pPF = &rd->m_FormatBC1;
			return eTF_BC1;
		}
		if (rd->m_FormatR8G8B8A8.IsValid())
		{
			pPF = &rd->m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		return eTF_Unknown;

	case eTF_BC2:
		if (rd->m_FormatBC2.IsValid())
		{
			pPF = &rd->m_FormatBC2;
			return eTF_BC2;
		}
		if (rd->m_FormatR8G8B8A8.IsValid())
		{
			pPF = &rd->m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		return eTF_Unknown;

	case eTF_BC3:
		if (rd->m_FormatBC3.IsValid())
		{
			pPF = &rd->m_FormatBC3;
			return eTF_BC3;
		}
		if (rd->m_FormatR8G8B8A8.IsValid())
		{
			pPF = &rd->m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		return eTF_Unknown;

	case eTF_BC4U:
		if (rd->m_FormatBC4U.IsValid())
		{
			pPF = &rd->m_FormatBC4U;
			return eTF_BC4U;
		}
		if (rd->m_FormatR8.IsValid())
		{
			pPF = &rd->m_FormatR8;
			return eTF_R8;
		}
		return eTF_Unknown;

	case eTF_BC4S:
		if (rd->m_FormatBC4S.IsValid())
		{
			pPF = &rd->m_FormatBC4S;
			return eTF_BC4S;
		}
		if (rd->m_FormatR8S.IsValid())
		{
			pPF = &rd->m_FormatR8S;
			return eTF_R8S;
		}
		return eTF_Unknown;

	case eTF_BC5U:
		if (rd->m_FormatBC5U.IsValid())
		{
			pPF = &rd->m_FormatBC5U;
			return eTF_BC5U;
		}
		if (rd->m_FormatR8G8.IsValid())
		{
			pPF = &rd->m_FormatR8G8;
			return eTF_R8G8;
		}
		return eTF_Unknown;

	case eTF_BC5S:
		if (rd->m_FormatBC5S.IsValid())
		{
			pPF = &rd->m_FormatBC5S;
			return eTF_BC5S;
		}
		if (rd->m_FormatR8G8S.IsValid())
		{
			pPF = &rd->m_FormatR8G8S;
			return eTF_R8G8S;
		}
		return eTF_Unknown;

	case eTF_BC6UH:
		if (rd->m_FormatBC6UH.IsValid())
		{
			pPF = &rd->m_FormatBC6UH;
			return eTF_BC6UH;
		}
		if (rd->m_FormatR16F.IsValid())
		{
			pPF = &rd->m_FormatR16F;
			return eTF_R16F;
		}
		return eTF_Unknown;

	case eTF_BC6SH:
		if (rd->m_FormatBC6SH.IsValid())
		{
			pPF = &rd->m_FormatBC6SH;
			return eTF_BC6SH;
		}
		if (rd->m_FormatR16F.IsValid())
		{
			pPF = &rd->m_FormatR16F;
			return eTF_R16F;
		}
		return eTF_Unknown;

	case eTF_BC7:
		if (rd->m_FormatBC7.IsValid())
		{
			pPF = &rd->m_FormatBC7;
			return eTF_BC7;
		}
		if (rd->m_FormatR8G8B8A8.IsValid())
		{
			pPF = &rd->m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		return eTF_Unknown;

	case eTF_R9G9B9E5:
		if (rd->m_FormatR9G9B9E5.IsValid())
		{
			pPF = &rd->m_FormatR9G9B9E5;
			return eTF_R9G9B9E5;
		}
		if (rd->m_FormatR16G16B16A16F.IsValid())
		{
			pPF = &rd->m_FormatR16G16B16A16F;
			return eTF_R16G16B16A16F;
		}
		return eTF_Unknown;

	case eTF_D16:
		if (rd->m_FormatD16.IsValid())
		{
			pPF = &rd->m_FormatD16;
			return eTF_D16;
		}
	case eTF_D24S8:
		if (rd->m_FormatD24S8.IsValid())
		{
			pPF = &rd->m_FormatD24S8;
			return eTF_D24S8;
		}
	case eTF_D32F:
		if (rd->m_FormatD32F.IsValid())
		{
			pPF = &rd->m_FormatD32F;
			return eTF_D32F;
		}
	case eTF_D32FS8:
		if (rd->m_FormatD32FS8.IsValid())
		{
			pPF = &rd->m_FormatD32FS8;
			return eTF_D32FS8;
		}
		return eTF_Unknown;

	case eTF_EAC_R11:
		if (rd->m_FormatEAC_R11.IsValid())
		{
			pPF = &rd->m_FormatEAC_R11;
			return eTF_EAC_R11;
		}
		return eTF_Unknown;

	case eTF_EAC_RG11:
		if (rd->m_FormatEAC_RG11.IsValid())
		{
			pPF = &rd->m_FormatEAC_RG11;
			return eTF_EAC_RG11;
		}
		return eTF_Unknown;

	case eTF_ETC2:
		if (rd->m_FormatETC2.IsValid())
		{
			pPF = &rd->m_FormatETC2;
			return eTF_ETC2;
		}
		return eTF_Unknown;

	case eTF_ETC2A:
		if (rd->m_FormatETC2A.IsValid())
		{
			pPF = &rd->m_FormatETC2A;
			return eTF_ETC2A;
		}
		return eTF_Unknown;

	default:
		assert(0);
	}
	return eTF_Unknown;
}

//===============================================================================

bool CTexture::CreateRenderTarget(ETEX_Format eTF, const ColorF& cClear)
{
	if (eTF == eTF_Unknown)
		eTF = m_eTFDst;
	if (eTF == eTF_Unknown)
		return false;
	byte* pData[6] = { NULL };

	ETEX_Format eTFDst = ClosestFormatSupported(eTF);
	if (eTF != eTFDst)
		return false;
	m_eTFDst = eTF;
	m_nFlags |= FT_USAGE_RENDERTARGET;
	m_cClearColor = cClear;
	bool bRes = CreateDeviceTexture(pData);
	PostCreate();

	return bRes;
}

// Resolve anti-aliased RT to the texture
bool CTexture::Resolve(int nTarget, bool bUseViewportSize)
{
	if (m_bResolved)
		return true;

	m_bResolved = true;
	if (!(m_nFlags & FT_USAGE_MSAA))
		return true;

	assert((m_nFlags & FT_USAGE_RENDERTARGET) && (m_nFlags & FT_USAGE_MSAA) && m_pDeviceShaderResource && m_pDevTexture && m_pRenderTargetData->m_pDeviceTextureMSAA);
	CDeviceTexture* pDestSurf = GetDevTexture();
	CDeviceTexture* pSrcSurf = GetDevTextureMSAA();

	assert(pSrcSurf != NULL);
	assert(pDestSurf != NULL);

	gcpRendD3D->GetDeviceContext().ResolveSubresource(pDestSurf->Get2DTexture(), 0, pSrcSurf->Get2DTexture(), 0, (DXGI_FORMAT)m_pPixelFormat->DeviceFormat);
	return true;
}

bool CTexture::CreateDeviceTexture(byte* pData[6])
{
	if (!m_pPixelFormat)
	{
		ETEX_Format eTF = ClosestFormatSupported(m_eTFDst);

		assert(eTF != eTF_Unknown);
		assert(eTF == m_eTFDst);
	}
	assert(m_pPixelFormat);
	if (!m_pPixelFormat)
		return false;

	if (gRenDev->m_pRT->RC_CreateDeviceTexture(this, pData))
	{
		// Assign name to Texture for enhanced debugging
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
		m_pDevTexture->GetBaseTexture()->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(m_SrcName.c_str()), m_SrcName.c_str());
#elif !defined(RELEASE) && CRY_PLATFORM_ORBIS
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

bool CTexture::RT_CreateDeviceTexture(byte* pData[6])
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Texture, 0, "Creating Texture");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "%s %ix%ix%i %08x", m_SrcName.c_str(), m_nWidth, m_nHeight, m_nMips, m_nFlags);
	SCOPED_RENDERER_ALLOCATION_NAME_HINT(GetSourceName());

	HRESULT hr;

	int32 nESRAMOffset = -1;
#if CRY_PLATFORM_DURANGO
	if (m_pRenderTargetData)
		nESRAMOffset = m_pRenderTargetData->m_nESRAMOffset;
#endif

#if CRY_PLATFORM_MAC
	if (!(m_nFlags & FT_FROMIMAGE) && IsBlockCompressed(m_eTFDst))
		m_bIsSRGB = true;
#endif

	//if we have any device owned resources allocated, we must sync with render thread
	if (m_pDevTexture)
	{
		ReleaseDeviceTexture(false);
	}
	else
	{
		SAFE_DELETE(m_pResourceViewData);
		SAFE_DELETE(m_pRenderTargetData);
	}

	CD3D9Renderer* r = gcpRendD3D;
	int nWdt = m_nWidth;
	int nHgt = m_nHeight;
	int nDepth = m_nDepth;
	int nMips = m_nMips;
	assert(nWdt && nHgt && nMips);

	byte* pTemp = NULL;

	CDeviceManager* pDevMan = &r->m_DevMan;

	bool resetSRGB = true;

	assert(m_pResourceViewData == nullptr);
	m_pResourceViewData = new ResourceViewData();
	if (m_nFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS))
	{
		assert(m_pRenderTargetData == nullptr);
		m_pRenderTargetData = new RenderTargetData();
#if CRY_PLATFORM_DURANGO
		m_pRenderTargetData->m_nESRAMOffset = nESRAMOffset;
#endif
	}

	uint32 nArraySize = m_nArraySize;

	if (m_eTT == eTT_2D)
	{
		STextureInfo TI;
		D3DFormat D3DFmt = m_pPixelFormat->DeviceFormat;

		//nMips = 1;
		DXGI_FORMAT nFormatOrig = D3DFmt;
		DXGI_FORMAT nFormatSRGB = D3DFmt;

		resetSRGB = false;
		{
			m_bIsSRGB &= m_pPixelFormat->bCanReadSRGB && (m_nFlags & (FT_USAGE_MSAA | FT_USAGE_RENDERTARGET)) == 0;
			if ((m_bIsSRGB || m_nFlags & FT_USAGE_ALLOWREADSRGB))
				nFormatSRGB = ConvertToSRGBFmt(D3DFmt);

			if (m_bIsSRGB)
				D3DFmt = nFormatSRGB;

			// must use typeless format to allow runtime casting
#if !CRY_PLATFORM_MAC
			if (m_nFlags & FT_USAGE_ALLOWREADSRGB)
				D3DFmt = ConvertToTypelessFmt(D3DFmt);
#endif
		}

		uint32 nUsage = 0;
		if (m_nFlags & FT_USAGE_DEPTHSTENCIL)
			nUsage |= CDeviceManager::USAGE_DEPTH_STENCIL;
		if (m_nFlags & FT_USAGE_RENDERTARGET)
			nUsage |= CDeviceManager::USAGE_RENDER_TARGET;
		if (m_nFlags & FT_USAGE_DYNAMIC)
			nUsage |= CDeviceManager::USAGE_DYNAMIC;
		if (m_nFlags & FT_STAGE_READBACK)
			nUsage |= CDeviceManager::USAGE_STAGE_ACCESS | CDeviceManager::USAGE_CPU_READ;
		if (m_nFlags & FT_STAGE_UPLOAD)
			nUsage |= CDeviceManager::USAGE_STAGE_ACCESS | CDeviceManager::USAGE_CPU_WRITE;
		if (m_nFlags & FT_USAGE_UNORDERED_ACCESS
#if defined(SUPPORT_DEVICE_INFO)
		    && r->DevInfo().FeatureLevel() >= D3D_FEATURE_LEVEL_11_0
#endif //defined(SUPPORT_DEVICE_INFO)
		    )
			nUsage |= CDeviceManager::USAGE_UNORDERED_ACCESS;
		if ((m_nFlags & (FT_DONT_RELEASE | FT_DONT_STREAM)) == (FT_DONT_RELEASE | FT_DONT_STREAM))
			nUsage |= CDeviceManager::USAGE_ETERNAL;
		if (m_nFlags & FT_USAGE_UAV_RWTEXTURE)
			nUsage |= CDeviceManager::USAGE_UAV_RWTEXTURE;

		if (m_nFlags & FT_FORCE_MIPS)
		{
			nUsage |= CDeviceManager::USAGE_AUTOGENMIPS;
			if (nMips <= 1) m_nMips = nMips = max(1, CTexture::CalcNumMips(nWdt, nHgt) - 2);
		}

		if (m_nFlags & FT_USAGE_MSAA)
		{
			m_pRenderTargetData->m_nMSAASamples = (uint8)r->m_RP.m_MSAAData.Type;
			m_pRenderTargetData->m_nMSAAQuality = (uint8)r->m_RP.m_MSAAData.Quality;

			TI.m_nMSAASamples = m_pRenderTargetData->m_nMSAASamples;
			TI.m_nMSAAQuality = m_pRenderTargetData->m_nMSAAQuality;

			hr = pDevMan->Create2DTexture(nWdt, nHgt, nMips, nArraySize, nUsage, m_cClearColor, D3DFmt, (D3DPOOL)0, &m_pRenderTargetData->m_pDeviceTextureMSAA, &TI);

			assert(SUCCEEDED(hr));
			m_bResolved = false;

			TI.m_nMSAASamples = 1;
			TI.m_nMSAAQuality = 0;
		}

		if (pData[0])
		{
			//assert(TI.m_nArraySize==1); //there is no implementation for tex array data

			//if (!IsBlockCompressed(m_eTFSrc))
			{
				STextureInfoData InitData[20];
				int w = nWdt;
				int h = nHgt;
				int nOffset = 0;
				byte* src = pData[0];
				for (int i = 0; i < nMips; i++)
				{
					if (!w && !h)
						break;
					if (!w) w = 1;
					if (!h) h = 1;

					int nSlicePitch = TextureDataSize(w, h, 1, 1, 1, m_eTFSrc, m_eSrcTileMode);

					InitData[i].pSysMem = &src[nOffset];
					if (m_eSrcTileMode == eTM_None)
					{
						InitData[i].SysMemPitch = TextureDataSize(w, 1, 1, 1, 1, m_eTFSrc, eTM_None);
						InitData[i].SysMemSlicePitch = nSlicePitch;
						InitData[i].SysMemTileMode = eTM_None;
					}
					else
					{
						InitData[i].SysMemPitch = 0;
						InitData[i].SysMemSlicePitch = 0;
						InitData[i].SysMemTileMode = m_eSrcTileMode;
					}

					w >>= 1;
					h >>= 1;
					nOffset += nSlicePitch;
				}

				TI.m_pData = InitData;

				hr = pDevMan->Create2DTexture(nWdt, nHgt, nMips, nArraySize, nUsage, m_cClearColor, D3DFmt, (D3DPOOL)0, &m_pDevTexture, &TI);
				if (!FAILED(hr) && m_pDevTexture)
					m_pDevTexture->SetNoDelete(!!(m_nFlags & FT_DONT_RELEASE));
			}
		}
		else
		{
			hr = pDevMan->Create2DTexture(nWdt, nHgt, nMips, nArraySize, nUsage, m_cClearColor, D3DFmt, (D3DPOOL)0, &m_pDevTexture, &TI, nESRAMOffset);
			if (!FAILED(hr) && m_pDevTexture)
				m_pDevTexture->SetNoDelete(!!(m_nFlags & FT_DONT_RELEASE));
		}

		if (FAILED(hr))
		{
			assert(0);
			return false;
		}

		// Restore format
		if (m_nFlags & FT_USAGE_ALLOWREADSRGB)
			D3DFmt = nFormatOrig;

		//////////////////////////////////////////////////////////////////////////
		m_pDeviceShaderResource = (D3DShaderResource*)GetResourceView(SResourceView::ShaderResourceView(m_eTFDst, 0, -1, 0, nMips, m_bIsSRGB, false)).m_pDeviceResourceView;
		m_nMinMipVidActive = 0;

		if (m_nFlags & FT_USAGE_ALLOWREADSRGB)
		{
			m_pDeviceShaderResourceSRGB = (D3DShaderResource*)GetResourceView(SResourceView::ShaderResourceView(m_eTFDst, 0, -1, 0, nMips, true, false)).m_pDeviceResourceView;
		}
	}
	else if (m_eTT == eTT_Cube)
	{
		STextureInfo TI;
		D3DFormat D3DFmt = m_pPixelFormat->DeviceFormat;
		uint32 nUsage = 0;
		if (m_nFlags & FT_USAGE_DEPTHSTENCIL)
			nUsage |= CDeviceManager::USAGE_DEPTH_STENCIL;
		if (m_nFlags & FT_USAGE_RENDERTARGET)
			nUsage |= CDeviceManager::USAGE_RENDER_TARGET;
		if (m_nFlags & FT_USAGE_DYNAMIC)
			nUsage |= CDeviceManager::USAGE_DYNAMIC;
		if (m_nFlags & FT_STAGE_READBACK)
			nUsage |= CDeviceManager::USAGE_STAGE_ACCESS | CDeviceManager::USAGE_CPU_READ;
		if (m_nFlags & FT_STAGE_UPLOAD)
			nUsage |= CDeviceManager::USAGE_STAGE_ACCESS | CDeviceManager::USAGE_CPU_WRITE;
		if ((m_nFlags & (FT_DONT_RELEASE | FT_DONT_STREAM)) == (FT_DONT_RELEASE | FT_DONT_STREAM))
			nUsage |= CDeviceManager::USAGE_ETERNAL;

		if (m_nFlags & FT_FORCE_MIPS)
		{
			nUsage |= CDeviceManager::USAGE_AUTOGENMIPS;
			if (nMips <= 1) m_nMips = nMips = max(1, CTexture::CalcNumMips(nWdt, nHgt) - 2);
		}

		if (m_nFlags & FT_USAGE_MSAA)
		{
			m_pRenderTargetData->m_nMSAASamples = (uint8)r->m_RP.m_MSAAData.Type;
			m_pRenderTargetData->m_nMSAAQuality = (uint8)r->m_RP.m_MSAAData.Quality;

			TI.m_nMSAASamples = m_pRenderTargetData->m_nMSAASamples;
			TI.m_nMSAAQuality = m_pRenderTargetData->m_nMSAAQuality;

			hr = pDevMan->CreateCubeTexture(nWdt, nMips, m_nArraySize, nUsage, m_cClearColor, D3DFmt, (D3DPOOL)0, &m_pRenderTargetData->m_pDeviceTextureMSAA, &TI);

			assert(SUCCEEDED(hr));
			m_bResolved = false;

			TI.m_nMSAASamples = 1;
			TI.m_nMSAAQuality = 0;
		}
		DXGI_FORMAT nFormatOrig = D3DFmt;
		DXGI_FORMAT nFormatSRGB = D3DFmt;

		resetSRGB = false;

		{
			m_bIsSRGB &= m_pPixelFormat->bCanReadSRGB && (m_nFlags & (FT_USAGE_MSAA | FT_USAGE_RENDERTARGET)) == 0;

			if ((m_bIsSRGB || m_nFlags & FT_USAGE_ALLOWREADSRGB))
				nFormatSRGB = ConvertToSRGBFmt(D3DFmt);

			if (m_bIsSRGB)
				D3DFmt = nFormatSRGB;

			// must use typeless format to allow runtime casting
			if (m_nFlags & FT_USAGE_ALLOWREADSRGB)
				D3DFmt = ConvertToTypelessFmt(D3DFmt);
		}

		if (pData[0])
		{
			assert(m_nArraySize == 1); //there is no implementation for tex array data

			STextureInfoData InitData[g_nD3D10MaxSupportedSubres];

			for (int nSide = 0; nSide < 6; nSide++)
			{
				int w = nWdt;
				int h = nHgt;
				int nOffset = 0;
				byte* src = (!(m_nFlags & FT_REPLICATE_TO_ALL_SIDES)) ? pData[nSide] : pData[0];

				for (int i = 0; i < nMips; i++)
				{
					if (!w && !h)
						break;
					if (!w) w = 1;
					if (!h) h = 1;

					int nSubresInd = nSide * nMips + i;
					int nSlicePitch = TextureDataSize(w, h, 1, 1, 1, m_eTFSrc, m_eSrcTileMode);

					InitData[nSubresInd].pSysMem = &src[nOffset];
					if (m_eSrcTileMode == eTM_None)
					{
						InitData[nSubresInd].SysMemPitch = TextureDataSize(w, 1, 1, 1, 1, m_eTFSrc, eTM_None);
						InitData[nSubresInd].SysMemSlicePitch = nSlicePitch;
						InitData[nSubresInd].SysMemTileMode = eTM_None;
					}
					else
					{
						InitData[nSubresInd].SysMemPitch = 0;
						InitData[nSubresInd].SysMemSlicePitch = 0;
						InitData[nSubresInd].SysMemTileMode = m_eSrcTileMode;
					}

					w >>= 1;
					h >>= 1;
					nOffset += nSlicePitch;
				}
			}

			TI.m_pData = InitData;

			hr = pDevMan->CreateCubeTexture(nWdt, nMips, m_nArraySize, nUsage, m_cClearColor, D3DFmt, (D3DPOOL)0, &m_pDevTexture, &TI);
			if (!FAILED(hr) && m_pDevTexture)
				m_pDevTexture->SetNoDelete(!!(m_nFlags & FT_DONT_RELEASE));
		}
		else
		{
			hr = pDevMan->CreateCubeTexture(nWdt, nMips, m_nArraySize, nUsage, m_cClearColor, D3DFmt, (D3DPOOL)0, &m_pDevTexture, &TI);
			if (!FAILED(hr) && m_pDevTexture)
				m_pDevTexture->SetNoDelete(!!(m_nFlags & FT_DONT_RELEASE));
		}
		if (FAILED(hr))
		{
			assert(0);
			return false;
		}

		if (m_nFlags & FT_USAGE_ALLOWREADSRGB)
			D3DFmt = nFormatOrig;

		//////////////////////////////////////////////////////////////////////////
		m_pDeviceShaderResource = (D3DShaderResource*)GetResourceView(SResourceView::ShaderResourceView(m_eTFDst, 0, -1, 0, nMips, m_bIsSRGB, false)).m_pDeviceResourceView;
		m_nMinMipVidActive = 0;

		if (m_nFlags & FT_USAGE_ALLOWREADSRGB)
		{
			m_pDeviceShaderResourceSRGB = (D3DShaderResource*)GetResourceView(SResourceView::ShaderResourceView(m_eTFDst, 0, -1, 0, nMips, true, false)).m_pDeviceResourceView;
		}
	}
	else if (m_eTT == eTT_3D)
	{
		STextureInfo TI;
		D3DFormat D3DFmt = m_pPixelFormat->DeviceFormat;

		uint32 nUsage = 0;
		if (m_nFlags & FT_USAGE_DEPTHSTENCIL)
			nUsage |= CDeviceManager::USAGE_DEPTH_STENCIL;
		if (m_nFlags & FT_USAGE_RENDERTARGET)
			nUsage |= CDeviceManager::USAGE_RENDER_TARGET;
		if (m_nFlags & FT_USAGE_DYNAMIC)
			nUsage |= CDeviceManager::USAGE_DYNAMIC;
		if (m_nFlags & FT_STAGE_READBACK)
			nUsage |= CDeviceManager::USAGE_STAGE_ACCESS | CDeviceManager::USAGE_CPU_READ;
		if (m_nFlags & FT_STAGE_UPLOAD)
			nUsage |= CDeviceManager::USAGE_STAGE_ACCESS | CDeviceManager::USAGE_CPU_WRITE;
		if ((m_nFlags & (FT_DONT_RELEASE | FT_DONT_STREAM)) == (FT_DONT_RELEASE | FT_DONT_STREAM))
			nUsage |= CDeviceManager::USAGE_ETERNAL;
		if (m_nFlags & FT_USAGE_UNORDERED_ACCESS
#if defined(SUPPORT_DEVICE_INFO)
		    && r->DevInfo().FeatureLevel() >= D3D_FEATURE_LEVEL_11_0
#endif //defined(SUPPORT_DEVICE_INFO)
		    )
			nUsage |= CDeviceManager::USAGE_UNORDERED_ACCESS;
		if (m_nFlags & FT_USAGE_UAV_RWTEXTURE)
			nUsage |= CDeviceManager::USAGE_UAV_RWTEXTURE;

		if (m_nFlags & FT_USAGE_MSAA)
		{
			m_pRenderTargetData->m_nMSAASamples = (uint8)r->m_RP.m_MSAAData.Type;
			m_pRenderTargetData->m_nMSAAQuality = (uint8)r->m_RP.m_MSAAData.Quality;

			TI.m_nMSAASamples = m_pRenderTargetData->m_nMSAASamples;
			TI.m_nMSAAQuality = m_pRenderTargetData->m_nMSAAQuality;

			hr = pDevMan->CreateVolumeTexture(nWdt, nHgt, m_nDepth, nMips, nUsage, m_cClearColor, D3DFmt, (D3DPOOL)0, &m_pRenderTargetData->m_pDeviceTextureMSAA, &TI);

			assert(SUCCEEDED(hr));
			m_bResolved = false;

			TI.m_nMSAASamples = 1;
			TI.m_nMSAAQuality = 0;
		}
		if (pData[0])
		{
			STextureInfoData InitData[15];

			int w = nWdt;
			int h = nHgt;
			int d = nDepth;
			int nOffset = 0;
			byte* src = pData[0];

			for (int i = 0; i < nMips; i++)
			{
				if (!w && !h && !d)
					break;
				if (!w) w = 1;
				if (!h) h = 1;
				if (!d) d = 1;

				int nSlicePitch = TextureDataSize(w, h, 1, 1, 1, m_eTFSrc);
				int nMipSize = TextureDataSize(w, h, d, 1, 1, m_eTFSrc);
				InitData[i].pSysMem = &src[nOffset];
				InitData[i].SysMemPitch = TextureDataSize(w, 1, 1, 1, 1, m_eTFSrc);
				InitData[i].SysMemSlicePitch = nSlicePitch;
				InitData[i].SysMemTileMode = eTM_None;

				w >>= 1;
				h >>= 1;
				d >>= 1;

				nOffset += nMipSize;
			}

			TI.m_pData = InitData;

			hr = pDevMan->CreateVolumeTexture(nWdt, nHgt, nDepth, nMips, nUsage, m_cClearColor, D3DFmt, (D3DPOOL)0, &m_pDevTexture, &TI);
			if (!FAILED(hr) && m_pDevTexture)
				m_pDevTexture->SetNoDelete(!!(m_nFlags & FT_DONT_RELEASE));
		}
		else
		{
			hr = pDevMan->CreateVolumeTexture(nWdt, nHgt, nDepth, nMips, nUsage, m_cClearColor, D3DFmt, (D3DPOOL)0, &m_pDevTexture, &TI);
			if (!FAILED(hr) && m_pDevTexture)
				m_pDevTexture->SetNoDelete(!!(m_nFlags & FT_DONT_RELEASE));
		}
		if (FAILED(hr))
		{
			assert(0);
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		m_pDeviceShaderResource = (D3DShaderResource*)GetResourceView(SResourceView::ShaderResourceView(m_eTFDst, 0, -1, 0, nMips, m_bIsSRGB, false)).m_pDeviceResourceView;
		m_nMinMipVidActive = 0;

		if (m_nFlags & FT_USAGE_ALLOWREADSRGB)
		{
			m_pDeviceShaderResourceSRGB = (D3DShaderResource*)GetResourceView(SResourceView::ShaderResourceView(m_eTFDst, 0, -1, 0, nMips, true, false)).m_pDeviceResourceView;
		}
	}
	else
	{
		assert(0);
		return false;
	}

	SetTexStates();

	assert(!IsStreamed());
	if (m_pDevTexture)
	{
		m_nActualSize = m_nPersistentSize = m_pDevTexture->GetDeviceSize();
		if (m_nFlags & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET))
		{
			CryInterlockedAdd(&CTexture::s_nStatsCurDynamicTexMem, m_nActualSize);
		}
		else
		{
			CryInterlockedAdd(&CTexture::s_nStatsCurManagedNonStreamedTexMem, m_nActualSize);
		}
	}

	// Notify that resource is dirty
	InvalidateDeviceResource(eDeviceResourceDirty | eDeviceResourceViewDirty);

	if (!pData[0])
		return true;

#ifdef DEVMAN_USE_PINNING
	CDeviceTexturePin pin(pDevTexture);
#endif

	int nOffset = 0;

	if (resetSRGB)
		m_bIsSRGB = false;

	SetWasUnload(false);

	SAFE_DELETE_ARRAY(pTemp);

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

	m_pDeviceRTV = nullptr;
	m_pDeviceRTVMS = nullptr;
	m_pDeviceShaderResource = nullptr;
	m_pDeviceShaderResourceSRGB = nullptr;

	SAFE_DELETE(m_pResourceViewData);
	SAFE_DELETE(m_pRenderTargetData);

	if (!m_bNoTexture)
	{
		CDeviceTexture* pTex = m_pDevTexture;

		if (!m_pFileTexMips || !m_pFileTexMips->m_pPoolItem)
		{
			if (IsStreamed())
			{
				SAFE_DELETE(pTex);      // for manually created textures
			}
			else
			{
				SAFE_RELEASE(pTex);
			}
		}

		m_pDevTexture = NULL;

		// otherwise it's already taken into account in the m_pFileTexMips->m_pPoolItem's dtor
		if (!m_pFileTexMips || !m_pFileTexMips->m_pPoolItem)
		{
			if (IsDynamic())
			{
				assert(CTexture::s_nStatsCurDynamicTexMem >= m_nActualSize);
				CryInterlockedAdd(&CTexture::s_nStatsCurDynamicTexMem, -m_nActualSize);
			}
			else if (!IsStreamed())
			{
				assert(CTexture::s_nStatsCurManagedNonStreamedTexMem >= m_nActualSize);
				CryInterlockedAdd(&CTexture::s_nStatsCurManagedNonStreamedTexMem, -m_nActualSize);
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

		m_nActualSize = 0;
		m_nPersistentSize = 0;
	}
	else
	{
		m_pDevTexture = NULL;
	}

	m_bNoTexture = false;
}

void* CTexture::CreateDeviceResourceView(const SResourceView& rv)
{
	const SPixFormat* pPixFormat = NULL;
	if (ClosestFormatSupported((ETEX_Format)rv.m_Desc.nFormat, pPixFormat) == eTF_Unknown)
		return NULL;

	HRESULT hr = E_FAIL;
	void* pResult = NULL;

	// DX expects -1 for selecting all mip maps/slices. max count throws an exception
	const uint nMipCount = rv.m_Desc.nMipCount == SResourceView().m_Desc.nMipCount ? (uint) - 1 : (uint)rv.m_Desc.nMipCount;
	const uint nSliceCount = rv.m_Desc.nSliceCount == SResourceView().m_Desc.nSliceCount ? (uint) - 1 : (uint)rv.m_Desc.nSliceCount;

	if (rv.m_Desc.eViewType == SResourceView::eShaderResourceView)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroStruct(srvDesc);

		// stencil only SRV
		if (rv.m_Desc.nFlags & SResourceView::eSRV_StencilOnly)
			srvDesc.Format = ConvertToStencilOnlyFmt(pPixFormat->DeviceFormat);
		// depth only SRV
		else
			srvDesc.Format = ConvertToDepthOnlyFmt(pPixFormat->DeviceFormat);

		// sRGB SRV
		if (rv.m_Desc.bSrgbRead)
			srvDesc.Format = ConvertToSRGBFmt(srvDesc.Format);

		D3DTexture* pTex = m_pDevTexture->Get2DTexture();

		if (rv.m_Desc.bMultisample && m_eTT == eTT_2D)
		{
			srvDesc.ViewDimension = m_nArraySize > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DMS;

			if (m_nArraySize > 1)
			{
				srvDesc.Texture2DMSArray.ArraySize = nSliceCount;
				srvDesc.Texture2DMSArray.FirstArraySlice = rv.m_Desc.nFirstSlice;
			}

			pTex = m_pRenderTargetData->m_pDeviceTextureMSAA->Get2DTexture();
		}
		else
		{
			srvDesc.ViewDimension =
			  m_eTT == eTT_1D ? (m_nArraySize > 1 ? D3D11_SRV_DIMENSION_TEXTURE1DARRAY : D3D11_SRV_DIMENSION_TEXTURE1D)
			  : (m_eTT == eTT_2D ? (m_nArraySize > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DARRAY : D3D11_SRV_DIMENSION_TEXTURE2D)
			     : (m_eTT == eTT_Cube ? (m_nArraySize > 1 ? D3D11_SRV_DIMENSION_TEXTURECUBEARRAY : D3D11_SRV_DIMENSION_TEXTURECUBE)
			        : (m_eTT == eTT_3D && m_nArraySize <= 1 ? D3D11_SRV_DIMENSION_TEXTURE3D : D3D11_SRV_DIMENSION_UNKNOWN)));

			// D3D11_TEX1D_SRV, D3D11_TEX2D_SRV, D3D11_TEX3D_SRV, D3D11_TEXCUBE_SRV and array versions can be aliased here
			srvDesc.Texture1D.MostDetailedMip = rv.m_Desc.nMostDetailedMip;
			srvDesc.Texture1D.MipLevels = nMipCount;

			if (m_nArraySize > 1)
			{
				srvDesc.Texture1DArray.FirstArraySlice = rv.m_Desc.nFirstSlice;
				srvDesc.Texture1DArray.ArraySize = nSliceCount;
			}
		}

		D3DShaderResource* pSRV = NULL;
		hr = gcpRendD3D->GetDevice().CreateShaderResourceView(pTex, &srvDesc, &pSRV);
		pResult = pSRV;
	}
	else // SResourceView::eRenderTargetView || SResourceView::eDepthStencilView || SResourceView::eUnorderedAccessView)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		ZeroStruct(rtvDesc);
		rtvDesc.Format = pPixFormat->DeviceFormat;
		D3DTexture* pTex = m_pDevTexture->Get2DTexture();

		if (rv.m_Desc.bMultisample && m_eTT == eTT_2D && rv.m_Desc.eViewType != SResourceView::eUnorderedAccessView)
		{
			rtvDesc.ViewDimension = m_nArraySize > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DMS;

			if (m_nArraySize > 1)
			{
				rtvDesc.Texture2DMSArray.FirstArraySlice = rv.m_Desc.nFirstSlice;
				rtvDesc.Texture2DMSArray.ArraySize = nSliceCount;
			}

			pTex = m_pRenderTargetData->m_pDeviceTextureMSAA->Get2DTexture();
		}
		else
		{
			rtvDesc.ViewDimension =
			  m_eTT == eTT_1D ? (m_nArraySize > 1 ? D3D11_RTV_DIMENSION_TEXTURE1DARRAY : D3D11_RTV_DIMENSION_TEXTURE1D)
			  : (m_eTT == eTT_2D ? (m_nArraySize > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2D)
			     : (m_eTT == eTT_Cube ? (m_nArraySize > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
			        : (m_eTT == eTT_3D && m_nArraySize <= 1 ? D3D11_RTV_DIMENSION_TEXTURE3D : D3D11_RTV_DIMENSION_UNKNOWN)));

			rtvDesc.Texture1D.MipSlice = rv.m_Desc.nMostDetailedMip;

			if (m_nArraySize > 1 || m_eTT == eTT_3D || m_eTT == eTT_Cube)
			{
				rtvDesc.Texture1DArray.FirstArraySlice = rv.m_Desc.nFirstSlice;
				rtvDesc.Texture1DArray.ArraySize = nSliceCount;
			}
		}

		switch (rv.m_Desc.eViewType)
		{
		case SResourceView::eRenderTargetView:
			{
				D3DSurface* pRTV = NULL;
				hr = gcpRendD3D->GetDevice().CreateRenderTargetView(pTex, &rtvDesc, &pRTV);
				pResult = pRTV;
			}
			break;
		case SResourceView::eDepthStencilView:
			{
				D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
				ZeroStruct(dsvDesc);

				dsvDesc.Format = (DXGI_FORMAT)ConvertToDepthStencilFmt(rtvDesc.Format);
				dsvDesc.Flags = rv.m_Desc.nFlags;
				memcpy(&dsvDesc.Texture1DArray, &rtvDesc.Texture1DArray, sizeof(dsvDesc.Texture1DArray));

				// Depth/Stencil read only DSV
				if (rv.m_Desc.nFlags & SResourceView::eDSV_ReadOnly)
					dsvDesc.Flags |= (D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL);
				// Depth/Stencil read/write DSV
				else
					dsvDesc.Flags &= ~(D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL);

				if (rtvDesc.ViewDimension != D3D11_RTV_DIMENSION_UNKNOWN && rtvDesc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE3D)
					dsvDesc.ViewDimension = (D3D11_DSV_DIMENSION)(rtvDesc.ViewDimension - 1);

				D3DDepthSurface* pDSV = NULL;
				hr = gcpRendD3D->GetDevice().CreateDepthStencilView(pTex, &dsvDesc, &pDSV);
				pResult = pDSV;
			}
			break;
		case SResourceView::eUnorderedAccessView:
			{
				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				memcpy(&uavDesc, &rtvDesc, sizeof(uavDesc));

				// typed UAV loads are only allowed for single-component 32-bit element types
				if (rv.m_Desc.nFlags & SResourceView::eUAV_ReadWrite)
					uavDesc.Format = DXGI_FORMAT_R32_UINT;

				if (rtvDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS || rtvDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
					uavDesc.ViewDimension = D3D11_UAV_DIMENSION_UNKNOWN;

				D3DUAV* pUAV = NULL;
				hr = gcpRendD3D->GetDevice().CreateUnorderedAccessView(pTex, &uavDesc, &pUAV);
				pResult = pUAV;
			}
			break;
		}
	}

	if (FAILED(hr))
	{
		assert(0);
		return NULL;
	}

	return pResult;
}

void CTexture::SetTexStates()
{
	STexState s;

	const bool noMipFiltering = m_nMips <= 1 && !(m_nFlags & FT_FORCE_MIPS);
	s.m_nMinFilter = FILTER_LINEAR;
	s.m_nMagFilter = FILTER_LINEAR;
	s.m_nMipFilter = noMipFiltering ? FILTER_NONE : FILTER_LINEAR;

	const int addrMode = (m_nFlags & FT_STATE_CLAMP || m_eTT == eTT_Cube) ? TADDR_CLAMP : TADDR_WRAP;
	s.SetClampMode(addrMode, addrMode, addrMode);

	m_nDefState = (uint16)CTexture::GetTexState(s);
}

static uint32 sAddressMode(int nAddress)
{
	IF (nAddress < 0, 0)
		nAddress = TADDR_WRAP;

	switch (nAddress)
	{
	case TADDR_WRAP:
		return D3D11_TEXTURE_ADDRESS_WRAP;
	case TADDR_CLAMP:
		return D3D11_TEXTURE_ADDRESS_CLAMP;
	case TADDR_BORDER:
		return D3D11_TEXTURE_ADDRESS_BORDER;
	case TADDR_MIRROR:
		return D3D11_TEXTURE_ADDRESS_MIRROR;
	default:
		assert(0);
		return D3D11_TEXTURE_ADDRESS_WRAP;
	}
}

void STexState::SetComparisonFilter(bool bEnable)
{
	if (m_pDeviceState)
	{
		D3DSamplerState* pSamp = (D3DSamplerState*)m_pDeviceState;
		SAFE_RELEASE(pSamp);
		m_pDeviceState = NULL;
	}
	m_bComparison = bEnable;
}

bool STexState::SetClampMode(int nAddressU, int nAddressV, int nAddressW)
{
	if (m_pDeviceState)
	{
		D3DSamplerState* pSamp = (D3DSamplerState*)m_pDeviceState;
		SAFE_RELEASE(pSamp);
		m_pDeviceState = NULL;
	}

	m_nAddressU = sAddressMode(nAddressU);
	m_nAddressV = sAddressMode(nAddressV);
	m_nAddressW = sAddressMode(nAddressW);
	return true;
}

bool STexState::SetFilterMode(int nFilter)
{
	IF (nFilter < 0, 0)
		nFilter = FILTER_TRILINEAR;

	if (m_pDeviceState)
	{
		D3DSamplerState* pSamp = (D3DSamplerState*)m_pDeviceState;
		SAFE_RELEASE(pSamp);
		m_pDeviceState = NULL;
	}

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

void STexState::SetBorderColor(DWORD dwColor)
{
	if (m_pDeviceState)
	{
		D3DSamplerState* pSamp = (D3DSamplerState*)m_pDeviceState;
		SAFE_RELEASE(pSamp);
		m_pDeviceState = NULL;
	}

	m_dwBorderColor = dwColor;
}

void STexState::PostCreate()
{
	if (m_pDeviceState)
		return;

	D3D11_SAMPLER_DESC Desc;
	ZeroStruct(Desc);
	D3DSamplerState* pSamp = NULL;
	// AddressMode of 0 is INVALIDARG
	Desc.AddressU = m_nAddressU ? (D3D11_TEXTURE_ADDRESS_MODE)m_nAddressU : D3D11_TEXTURE_ADDRESS_WRAP;
	Desc.AddressV = m_nAddressV ? (D3D11_TEXTURE_ADDRESS_MODE)m_nAddressV : D3D11_TEXTURE_ADDRESS_WRAP;
	Desc.AddressW = m_nAddressW ? (D3D11_TEXTURE_ADDRESS_MODE)m_nAddressW : D3D11_TEXTURE_ADDRESS_WRAP;
	ColorF col((uint32)m_dwBorderColor);
	Desc.BorderColor[0] = col.r;
	Desc.BorderColor[1] = col.g;
	Desc.BorderColor[2] = col.b;
	Desc.BorderColor[3] = col.a;
	if (m_bComparison)
		Desc.ComparisonFunc = D3D11_COMPARISON_LESS;
	else
		Desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

	Desc.MaxAnisotropy = 1;
	Desc.MinLOD = 0;
	if (m_nMipFilter == FILTER_NONE)
		Desc.MaxLOD = 0.0f;
	else
		Desc.MaxLOD = 100.0f;

	Desc.MipLODBias = 0;

	if (m_bComparison)
	{

		if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && m_nMipFilter == FILTER_LINEAR || m_nMinFilter == FILTER_TRILINEAR || m_nMagFilter == FILTER_TRILINEAR)
		{
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		}
		else if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && (m_nMipFilter == FILTER_NONE || m_nMipFilter == FILTER_POINT) || m_nMinFilter == FILTER_BILINEAR || m_nMagFilter == FILTER_BILINEAR)
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		else if (m_nMinFilter == FILTER_POINT && m_nMagFilter == FILTER_POINT && (m_nMipFilter == FILTER_NONE || m_nMipFilter == FILTER_POINT))
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		else if (m_nMinFilter >= FILTER_ANISO2X && m_nMagFilter >= FILTER_ANISO2X && m_nMipFilter >= FILTER_ANISO2X)
		{
			Desc.Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
			Desc.MaxAnisotropy = m_nAnisotropy;
		}
	}
	else
	{
		if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && m_nMipFilter == FILTER_LINEAR || m_nMinFilter == FILTER_TRILINEAR || m_nMagFilter == FILTER_TRILINEAR)
		{
			Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
		else if (m_nMinFilter == FILTER_LINEAR && m_nMagFilter == FILTER_LINEAR && (m_nMipFilter == FILTER_NONE || m_nMipFilter == FILTER_POINT) || m_nMinFilter == FILTER_BILINEAR || m_nMagFilter == FILTER_BILINEAR)
			Desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		else if (m_nMinFilter == FILTER_POINT && m_nMagFilter == FILTER_POINT && (m_nMipFilter == FILTER_NONE || m_nMipFilter == FILTER_POINT))
			Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		else if (m_nMinFilter >= FILTER_ANISO2X && m_nMagFilter >= FILTER_ANISO2X && m_nMipFilter >= FILTER_ANISO2X)
		{
			Desc.Filter = D3D11_FILTER_ANISOTROPIC;
			Desc.MaxAnisotropy = m_nAnisotropy;
		}
		else
			assert(0);
	}

	HRESULT hr = gcpRendD3D->GetDevice().CreateSamplerState(&Desc, &pSamp);
	if (SUCCEEDED(hr))
		m_pDeviceState = pSamp;
	else
		assert(0);
}

STexState::~STexState()
{
	if (m_pDeviceState)
	{
		D3DSamplerState* pSamp = (D3DSamplerState*)m_pDeviceState;
		SAFE_RELEASE(pSamp);
		m_pDeviceState = NULL;
	}
}

STexState::STexState(const STexState& src)
{
	memcpy(this, &src, sizeof(src));
	if (m_pDeviceState)
	{
		D3DSamplerState* pSamp = (D3DSamplerState*)m_pDeviceState;
		pSamp->AddRef();
	}
}

bool CTexture::SetClampingMode(int nAddressU, int nAddressV, int nAddressW)
{
	return s_sDefState.SetClampMode(nAddressU, nAddressV, nAddressW);
}

bool CTexture::SetFilterMode(int nFilter)
{
	return s_sDefState.SetFilterMode(nFilter);
}

void CTexture::UpdateTexStates()
{
	m_nDefState = (uint16)CTexture::GetTexState(s_sDefState);
}

void CTexture::SetSamplerState(int nTS, int nSUnit, EHWShaderClass eHWSC)
{
	FUNCTION_PROFILER_RENDER_FLAT
	  assert(gcpRendD3D->m_pRT->IsRenderThread());
	STexStageInfo* const __restrict TexStages = s_TexStages;

	if (s_TexStateIDs[eHWSC][nSUnit] != nTS)
	{
		STexState* pTS = &s_TexStates[nTS];
		D3DSamplerState* pSamp = (D3DSamplerState*)pTS->m_pDeviceState;
		D3D11_SAMPLER_DESC Desc;

		assert(pSamp);

		if (pSamp)
		{
			pSamp->GetDesc(&Desc);
			s_TexStateIDs[eHWSC][nSUnit] = nTS;

			if (eHWSC == eHWSC_Pixel)
				gcpRendD3D->m_DevMan.BindSampler(CDeviceManager::TYPE_PS, &pSamp, nSUnit, 1);
			else if (eHWSC == eHWSC_Domain)
				gcpRendD3D->m_DevMan.BindSampler(CDeviceManager::TYPE_DS, &pSamp, nSUnit, 1);
			else if (eHWSC == eHWSC_Vertex)
				gcpRendD3D->m_DevMan.BindSampler(CDeviceManager::TYPE_VS, &pSamp, nSUnit, 1);
			else if (eHWSC == eHWSC_Compute)
				gcpRendD3D->m_DevMan.BindSampler(CDeviceManager::TYPE_CS, &pSamp, nSUnit, 1);
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

void CTexture::ApplySamplerState(int nSUnit, EHWShaderClass eHWSC, int nState)
{
	FUNCTION_PROFILER_RENDER_FLAT

	uint32 nTSSel = Isel32(nState, (int32)m_nDefState);
	assert(nTSSel >= 0 && nTSSel < s_TexStates.size());

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

	SetSamplerState(nTSSel, nSUnit, eHWSC);
}

void CTexture::ApplyTexture(int nTUnit, EHWShaderClass eHWSC, SResourceView::KeyType nResViewKey)
{
	FUNCTION_PROFILER_RENDER_FLAT

	STexStageInfo* TexStages = s_TexStages;
	CDeviceTexture* pDevTex = m_pDevTexture;

	// avoiding L2 cache misses from usage from up ahead
	PrefetchLine(pDevTex, 0);

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
					Load(m_eTFDst);

				pDevTex = m_pDevTexture;
			}
		}
	}

	IF (this != CTexture::s_pTexNULL && (!pDevTex || !pDevTex->GetBaseTexture()), 0)
	{
		// apply black by default
		if (CTexture::s_ptexBlack)
		{
			CTexture::s_ptexBlack->ApplyTexture(nTUnit, eHWSC, nResViewKey);
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
		if (m_nFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC))
		{
			rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_DynTexturesSize += m_nActualSize;
		}
		else
		{
			if (bStreamed)
			{
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesStreamVidSize += m_nActualSize;
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesStreamSysSize += StreamComputeDevDataSize(0);
			}
			else
			{
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesSysMemSize += m_nActualSize;
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesVidMemSize += m_nActualSize;
			}
		}
#endif

#ifndef OPENGL
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

	const bool bUnnorderedAcessView = SResourceView(nResViewKey).m_Desc.eViewType == SResourceView::eUnorderedAccessView;

	D3DShaderResource* pResView = GetShaderResourceView(nResViewKey);

	if (pDevTex == TexStages[nTUnit].m_DevTexture && pResView == TexStages[nTUnit].m_pCurResView && eHWSC == TexStages[nTUnit].m_eHWSC)
		return;

	TexStages[nTUnit].m_pCurResView = pResView;
	TexStages[nTUnit].m_eHWSC = eHWSC;

	//	<DEPRECATED> This must get re-factored post C3.
	//	-	This check is ultra-buggy, render targets setup is deferred until last moment might not be matching this check at all. Also very wrong for MRTs

	if (rd->m_pCurTarget[0] == this)
	{
		//assert(rd->m_pCurTarget[0]->m_pDeviceRTV);
		rd->m_pNewTarget[0]->m_bWasSetRT = false;
		rd->GetDeviceContext().OMSetRenderTargets(1, &rd->m_pNewTarget[0]->m_pTarget, rd->m_pNewTarget[0]->m_pDepth);
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
		if (CRenderer::CV_r_log >= 3 && int64(nResViewKey) >= 0)
		{
			rd->Logv("CTexture::Apply(): Shader Resource View: %ul \n", nResViewKey);
		}
#endif

		if (bUnnorderedAcessView)
		{
			// todo:
			// - add support for pixel shader side via OMSetRenderTargetsAndUnorderedAccessViews
			// - DX11.1 very likely API will be similar to CSSetUnorderedAccessViews, but for all stages
			D3DUAV* pUAV = (D3DUAV*)pResView;
			rd->GetDeviceContext().CSSetUnorderedAccessViews(nTUnit, 1, &pUAV, NULL);
			return;
		}

		{
			if (IsVertexTexture())
				eHWSC = eHWSC_Vertex;

			if (eHWSC == eHWSC_Pixel)
				rd->m_DevMan.BindSRV(CDeviceManager::TYPE_PS, pResView, nTUnit);
			else if (eHWSC == eHWSC_Vertex)
				rd->m_DevMan.BindSRV(CDeviceManager::TYPE_VS, pResView, nTUnit);
			else if (eHWSC == eHWSC_Domain)
				rd->m_DevMan.BindSRV(CDeviceManager::TYPE_DS, pResView, nTUnit);
			else if (eHWSC == eHWSC_Compute)
				rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pResView, nTUnit);
			else
				assert(0);
		}
	}
}

void CTexture::Apply(int nTUnit, int nState, int nTexMatSlot, int nSUnit, SResourceView::KeyType nResViewKey, EHWShaderClass eHWSC)
{
	FUNCTION_PROFILER_RENDER_FLAT

	  assert(nTUnit >= 0);
	uint32 nTSSel = Isel32(nState, (int32)m_nDefState);
	assert(nTSSel >= 0 && nTSSel < s_TexStates.size());

	STexStageInfo* TexStages = s_TexStages;
	CDeviceTexture* pDevTex = m_pDevTexture;

	// avoiding L2 cache misses from usage from up ahead
	PrefetchLine(pDevTex, 0);

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
					Load(m_eTFDst);

				pDevTex = m_pDevTexture;
			}
		}

		if (nTexMatSlot != EFTT_UNKNOWN)
			m_nStreamingPriority = max((int8)m_nStreamingPriority, TextureHelpers::LookupTexPriority((EEfResTextures)nTexMatSlot));
	}

	IF (this != CTexture::s_pTexNULL && (!pDevTex || !pDevTex->GetBaseTexture()), 0)
	{
		// apply black by default
		if (CTexture::s_ptexBlack)
		{
			CTexture::s_ptexBlack->Apply(nTUnit, nState, nTexMatSlot, nSUnit, nResViewKey);
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
		if (m_nFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DYNAMIC))
		{
			rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_DynTexturesSize += m_nActualSize;
		}
		else
		{
			if (bStreamed)
			{
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesStreamVidSize += m_nActualSize;
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesStreamSysSize += StreamComputeDevDataSize(0);
			}
			else
			{
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesSysMemSize += m_nActualSize;
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_ManagedTexturesVidMemSize += m_nActualSize;
			}
		}
#endif

		// mip map fade in
#ifndef OPENGL
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

	const bool bUnnorderedAcessView = SResourceView(nResViewKey).m_Desc.eViewType == SResourceView::eUnorderedAccessView;

	if (!bUnnorderedAcessView && nSUnit >= 0)
	{
		SetSamplerState(nTSSel, nSUnit, eHWSC);
	}

	D3DShaderResource* pResView = GetShaderResourceView(nResViewKey, s_TexStates[nTSSel].m_bSRGBLookup);

	if (pDevTex == TexStages[nTUnit].m_DevTexture && pResView == TexStages[nTUnit].m_pCurResView && eHWSC == TexStages[nTUnit].m_eHWSC)
		return;

	TexStages[nTUnit].m_pCurResView = pResView;
	TexStages[nTUnit].m_eHWSC = eHWSC;

	//	<DEPRECATED> This must get re-factored post C3.
	//	-	This check is ultra-buggy, render targets setup is deferred until last moment might not be matching this check at all. Also very wrong for MRTs

	if (rd->m_pCurTarget[0] == this)
	{
		//assert(rd->m_pCurTarget[0]->m_pDeviceRTV);
		rd->m_pNewTarget[0]->m_bWasSetRT = false;
		rd->GetDeviceContext().OMSetRenderTargets(1, &rd->m_pNewTarget[0]->m_pTarget, rd->m_pNewTarget[0]->m_pDepth);
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
		if (CRenderer::CV_r_log >= 3 && int64(nResViewKey) >= 0)
		{
			rd->Logv("CTexture::Apply(): Shader Resource View: %ul \n", nResViewKey);
		}
#endif

		if (bUnnorderedAcessView)
		{
			// todo:
			// - add support for pixel shader side via OMSetRenderTargetsAndUnorderedAccessViews
			// - DX11.1 very likely API will be similar to CSSetUnorderedAccessViews, but for all stages
			D3DUAV* pUAV = (D3DUAV*)pResView;
			rd->GetDeviceContext().CSSetUnorderedAccessViews(nTUnit, 1, &pUAV, NULL);
			return;
		}

		{
			if (IsVertexTexture())
				eHWSC = eHWSC_Vertex;

			if (eHWSC == eHWSC_Pixel)
				rd->m_DevMan.BindSRV(CDeviceManager::TYPE_PS, pResView, nTUnit);
			else if (eHWSC == eHWSC_Vertex)
				rd->m_DevMan.BindSRV(CDeviceManager::TYPE_VS, pResView, nTUnit);
			else if (eHWSC == eHWSC_Domain)
				rd->m_DevMan.BindSRV(CDeviceManager::TYPE_DS, pResView, nTUnit);
			else if (eHWSC == eHWSC_Compute)
				rd->m_DevMan.BindSRV(CDeviceManager::TYPE_CS, pResView, nTUnit);
			else
				assert(0);
		}
	}
}

void CTexture::UpdateTextureRegion(byte* data, int nX, int nY, int nZ, int USize, int VSize, int ZSize, ETEX_Format eTFSrc)
{
	gRenDev->m_pRT->RC_UpdateTextureRegion(this, data, nX, nY, nZ, USize, VSize, ZSize, eTFSrc);
}

//void CTexture::RT_UpdateTextureRegion(byte *data, int nX, int nY, int USize, int VSize, ETEX_Format eTFSrc)
void CTexture::RT_UpdateTextureRegion(byte* data, int nX, int nY, int nZ, int USize, int VSize, int ZSize, ETEX_Format eTFSrc)
{
	PROFILE_FRAME(UpdateTextureRegion);

	if (m_eTT != eTT_2D && m_eTT != eTT_3D)
	{
		assert(0);
		return;
	}

	HRESULT hr = S_OK;
	CDeviceTexture* pDevTexture = GetDevTexture();
	assert(pDevTexture);
	if (!pDevTexture)
		return;

	DXGI_FORMAT frmtSrc = (DXGI_FORMAT)CTexture::DeviceFormatFromTexFormat(eTFSrc);
	bool bDone = false;
	D3D11_BOX rc = { UINT(nX), UINT(nY), 0U, UINT(nX + USize), UINT(nY + VSize), 1U };
	if (m_eTT == eTT_2D)
	{
		if (!IsBlockCompressed(m_eTFDst))
		{
			const int nBPPSrc = CTexture::BitsPerPixel(eTFSrc);
			const int nBPPDst = CTexture::BitsPerPixel(m_eTFDst);
			if (nBPPSrc == nBPPDst)
			{
				const int nRowPitch = CTexture::TextureDataSize(USize, 1, 1, 1, 1, eTFSrc);
				const int nSlicePitch = CTexture::TextureDataSize(USize, VSize, 1, 1, 1, eTFSrc);
				gcpRendD3D->GetDeviceContext().UpdateSubresource(pDevTexture->Get2DTexture(), 0, &rc, data, nRowPitch, nSlicePitch);
				bDone = true;
			}
			else
			{
				assert(0);
				bDone = true;
			}
		}
	}
	else if (m_eTT == eTT_3D)
	{
		int nFrame = gRenDev->m_nFrameSwapID;
		rc.front = nZ;
		rc.back = nZ + ZSize;

		const int nBPPSrc = CTexture::BitsPerPixel(eTFSrc);
		const int nBPPDst = CTexture::BitsPerPixel(m_eTFDst);
		if (nBPPSrc == nBPPDst)
		{
			if (m_nFlags & FT_USAGE_DYNAMIC)
			{
				D3DVolumeTexture* pDT = pDevTexture->GetVolumeTexture();
				int cZ, cY;
				for (cZ = nZ; cZ < ZSize; cZ++)
				{
					D3D11_MAPPED_SUBRESOURCE lrct;
					uint32 nLockFlags = D3D11_MAP_WRITE_DISCARD_SR;
					uint32 nSubRes = D3D11CalcSubresource(0, cZ, 1);

					hr = gcpRendD3D->GetDeviceContext().Map(pDT, nSubRes, (D3D11_MAP)nLockFlags, 0, &lrct);
					assert(hr == S_OK);

					byte* pDst = ((byte*)lrct.pData) + nX * 4 + nY * lrct.RowPitch;
					for (cY = 0; cY < VSize; cY++)
					{
						cryMemcpy(pDst, data, USize * 4);
						data += USize * 4;
						pDst += lrct.RowPitch;
					}
					gcpRendD3D->GetDeviceContext().Unmap(pDT, nSubRes);
				}
			}
			else
			{
				int U = USize;
				int V = VSize;
				int Z = ZSize;
				for (int i = 0; i < m_nMips; i++)
				{
					if (!U)
						U = 1;
					if (!V)
						V = 1;
					if (!Z)
						Z = 1;

					const int nRowPitch = CTexture::TextureDataSize(U, 1, 1, 1, 1, eTFSrc);
					const int nSlicePitch = CTexture::TextureDataSize(U, V, 1, 1, 1, eTFSrc);
					gcpRendD3D->GetDeviceContext().UpdateSubresource(pDevTexture->GetBaseTexture(), i, &rc, data, nRowPitch, nSlicePitch);
					bDone = true;

					data += nSlicePitch * Z;

					U >>= 1;
					V >>= 1;
					Z >>= 1;

					rc.front >>= 1;
					rc.left >>= 1;
					rc.top >>= 1;

					rc.back = max(rc.front + 1, rc.back >> 1);
					rc.right = max(rc.left + 4, rc.right >> 1);
					rc.bottom = max(rc.top + 4, rc.bottom >> 1);
				}
			}
		}
		else if ((eTFSrc == eTF_B8G8R8A8 || eTFSrc == eTF_B8G8R8X8) && (m_eTFDst == eTF_B5G6R5))
		{
			assert(0);
			bDone = true;
		}
	}
	if (!bDone)
	{
		D3D11_BOX box;
		ZeroStruct(box);
		box.left = nX;
		box.top = nY;
		box.right = box.left + USize;
		box.bottom = box.top + VSize;
		box.back = 1;
		const int nRowPitch = CTexture::TextureDataSize(USize, 1, 1, 1, 1, eTFSrc);
		const int nSlicePitch = CTexture::TextureDataSize(USize, VSize, 1, 1, 1, eTFSrc);
		gcpRendD3D->GetDeviceContext().UpdateSubresource(pDevTexture->Get2DTexture(), 0, &box, data, nRowPitch, nSlicePitch);
	}
}

bool CTexture::Clear()
{
	if (!(m_nFlags & FT_USAGE_RENDERTARGET))
		return false;

	gRenDev->m_pRT->RC_ClearTarget(this, m_cClearColor);
	return true;
}

bool CTexture::Clear(const ColorF& cClear)
{
	if (!(m_nFlags & FT_USAGE_RENDERTARGET))
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

	CTexture* ptexGenEnvironmentCM = CTexture::Create2DTexture("$GenEnvironmentCM", size, size, 0, FT_DONT_STREAM, 0, eTF_R16G16B16A16F, eTF_R16G16B16A16F);
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

		// Transfer to sysmem
		D3D11_BOX srcBox;
		srcBox.left = 0;
		srcBox.right = size;
		srcBox.top = 0;
		srcBox.bottom = size;
		srcBox.front = 0;
		srcBox.back = 1;

		CDeviceTexture* pDevTextureSrc = CTexture::s_ptexHDRTarget->GetDevTexture();
		CDeviceTexture* pDevTextureDst = ptexGenEnvironmentCM->GetDevTexture();

		gcpRendD3D->GetDeviceContext().GetRealDeviceContext()->CopySubresourceRegion(pDevTextureDst->Get2DTexture(), 0, 0, 0, 0, pDevTextureSrc->Get2DTexture(), 0, &srcBox);

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

	D3D11_TEXTURE2D_DESC pDesc;
	pTex->Get2DTexture()->GetDesc(&pDesc);

	// all d3d11 devices support autogenmipmaps
	if (m_pRenderTargetData)
		gcpRendD3D->GetDeviceContext().GenerateMips(m_pDeviceShaderResource);

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
		s_ptexZTarget = CreateRenderTarget("$ZTarget", nWidth, nHeight, ColorF(1.0f, 1.0f, 1.0f, 1.0f), eTT_2D, nFlags, eTFZ);
	else
	{
		s_ptexZTarget->m_nFlags = nFlags;
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
		s_ptexSceneTarget = CreateRenderTarget("$SceneTarget", nWidth, nHeight, Clr_Empty, eTT_2D, nFlags, eTF, TO_SCENE_TARGET);
	else
	{
		s_ptexSceneTarget->m_nFlags = nFlags;
		s_ptexSceneTarget->SetWidth(nWidth);
		s_ptexSceneTarget->SetHeight(nHeight);
		s_ptexSceneTarget->CreateRenderTarget(eTF, Clr_Empty);
	}

	nFlags &= ~(FT_USAGE_MSAA | FT_USAGE_UNORDERED_ACCESS);

	// This RT used for all post processes passes and shadow mask (group 0) as well
	if (!CTexture::IsTextureExist(s_ptexBackBuffer))
		s_ptexBackBuffer = CreateRenderTarget("$BackBuffer", nWidth, nHeight, Clr_Empty, eTT_2D, nFlags, eTF_R8G8B8A8, TO_BACKBUFFERMAP);
	else
	{
		s_ptexBackBuffer->m_nFlags = nFlags;
		s_ptexBackBuffer->SetWidth(nWidth);
		s_ptexBackBuffer->SetHeight(nHeight);
		s_ptexBackBuffer->CreateRenderTarget(eTF_R8G8B8A8, Clr_Empty);
	}

	nFlags &= ~(FT_USAGE_MSAA | FT_USAGE_UNORDERED_ACCESS);

	// This RT can be used by the Render3DModelMgr if the buffer needs to be persistent
	if (CRenderer::CV_r_UsePersistentRTForModelHUD > 0)
	{
		if (!CTexture::IsTextureExist(s_ptexModelHudBuffer))
			s_ptexModelHudBuffer = CreateRenderTarget("$ModelHUD", nWidth, nHeight, Clr_Transparent, eTT_2D, nFlags, eTF_R8G8B8A8, TO_BACKBUFFERMAP);
		else
		{
			s_ptexModelHudBuffer->m_nFlags = nFlags;
			s_ptexModelHudBuffer->SetWidth(nWidth);
			s_ptexModelHudBuffer->SetHeight(nHeight);
			s_ptexModelHudBuffer->CreateRenderTarget(eTF_R8G8B8A8, Clr_Transparent);
		}
	}

	// Editor fix: it is possible at this point that resolution has changed outside of ChangeResolution and stereoR, stereoL have not been resized
	if (gEnv->IsEditor())
		gcpRendD3D->GetS3DRend().OnResolutionChanged();
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

	const ETEX_Format texFormat = gEnv->pConsole->GetCVar("r_ShadowsCacheFormat")->GetIVal() == 0 ? eTF_R32F : eTF_D16;
	const int cachedShadowsStart = clamp_tpl(CRenderer::CV_r_ShadowsCache, 0, MAX_GSM_LODS_NUM - 1);

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

			pTx = CTexture::CreateTextureObject(szName, nResolutions[i], nResolutions[i], 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, texFormat);
		}

		pTx->Invalidate(nResolutions[i], nResolutions[i], texFormat);

		// delete existing texture in case it's not needed anymore
		if (CTexture::IsTextureExist(pTx) && nResolutions[i] == 0)
			pTx->ReleaseDeviceTexture(false);

		// allocate texture directly for all cached cascades
		if (!CTexture::IsTextureExist(pTx) && nResolutions[i] > 0 && i < cachedCascadesCount)
		{
			CryLog("Allocating shadow map cache %d x %d: %.2f MB", nResolutions[i], nResolutions[i], sqr(nResolutions[i]) * CTexture::BitsPerPixel(texFormat) / (1024.f * 1024.f * 8.f));
			pTx->CreateRenderTarget(texFormat, Clr_FarPlane);
		}
	}

	// height map AO
	if (CRenderer::CV_r_HeightMapAO)
	{
		const int nTexRes = (int)clamp_tpl(CRenderer::CV_r_HeightMapAOResolution, 0.f, 16384.f);
		const ETEX_Format texFormatMips = texFormat == eTF_D32F ? eTF_R32F : eTF_R16;

		if (!s_ptexHeightMapAODepth[0])
		{
			s_ptexHeightMapAODepth[0] = CTexture::CreateTextureObject("HeightMapAO_Depth_0", nTexRes, nTexRes, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, texFormat);
			s_ptexHeightMapAODepth[1] = CTexture::CreateTextureObject("HeightMapAO_Depth_1", nTexRes, nTexRes, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, texFormatMips);
		}

		s_ptexHeightMapAODepth[0]->Invalidate(nTexRes, nTexRes, texFormat);
		s_ptexHeightMapAODepth[1]->Invalidate(nTexRes, nTexRes, texFormatMips);

		if (!CTexture::IsTextureExist(s_ptexHeightMapAODepth[0]) && nTexRes > 0)
		{
			s_ptexHeightMapAODepth[0]->CreateRenderTarget(texFormat, Clr_FarPlane);
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
	const int texResolution = CRenderer::CV_r_ShadowsNearestMapResolution;
	const ETEX_Format texFormat = eTF_D32F;
	s_ptexNearestShadowMap = CTexture::CreateTextureObject("NearestShadowMap", texResolution, texResolution, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, texFormat);
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
		m_pTexture->AddDirtRect(rc, rc.left, rc.top);
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

	RT_FlushTextMessages();
#endif
}

bool CFlashTextureSourceBase::Update()
{
	if (!m_pFlashPlayer)
		return false;

	AutoReleasedFlashPlayerPtr pFlashPlayer(m_pFlashPlayer->GetTempPtr());
	SDynTexture* pDynTexture = GetDynTexture();
	assert(pDynTexture);
	if (!pDynTexture)
		return false;

	const int rtWidth = GetWidth();
	const int rtHeight = GetHeight();
	if (!UpdateDynTex(rtWidth, rtHeight))
		return false;

	if (pFlashPlayer)
	{
		m_width = min(pFlashPlayer->GetWidth(), rtWidth);
		m_height = min(pFlashPlayer->GetHeight(), rtHeight);
		m_aspectRatio = ((float) pFlashPlayer->GetWidth() / (float) m_width) * ((float) m_height / (float) pFlashPlayer->GetHeight());

		pFlashPlayer->SetViewport(0, 0, m_width, m_height, m_aspectRatio);
		pFlashPlayer->SetScissorRect(0, 0, m_width, m_height);
		pFlashPlayer->SetBackgroundAlpha(0.0f);
	}

	PROFILE_LABEL_SCOPE("FlashDynTextureSource");

	RECT rect = { 0, 0, clamp_tpl(Align8(m_width), 16, rtWidth), clamp_tpl(Align8(m_height), 16, rtHeight) };

	gcpRendD3D->FX_ClearTarget(pDynTexture->m_pTexture, Clr_Transparent, 1, &rect, true);
	pDynTexture->SetRT(0, true, gcpRendD3D->FX_GetDepthSurface(rtWidth, rtHeight, false));
	gcpRendD3D->RT_SetViewport(0, 0, clamp_tpl(Align8(m_width), 16, rtWidth), clamp_tpl(Align8(m_height), 16, rtHeight));
	gcpRendD3D->FX_Commit();

	if (pFlashPlayer)
	{
		pFlashPlayer->Render(false);
	}

	pDynTexture->RestoreRT(0, true);
	gcpRendD3D->FX_SetState(gcpRendD3D->m_RP.m_CurState & ~GS_BLEND_MASK);

	pDynTexture->SetUpdateMask();

	m_lastVisible = gEnv->pTimer->GetAsyncTime();
	m_lastVisibleFrameID = gRenDev->GetFrameID(false);

	// calculate mip-maps after update, if mip-maps have been allocated
	CTexture* pTex = (CTexture*)pDynTexture->GetTexture();
	if (pTex->GetNumMips() > 1)
		pTex->GenerateMipMaps();

	return true;
}

void CFlashTextureSourceSharedRT::ProbeDepthStencilSurfaceCreation(int width, int height)
{
	gcpRendD3D->FX_GetDepthSurface(width, height, false);
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
	SAFE_RELEASE_FORCE(s_ptexWaterRipplesDDN);

	SAFE_RELEASE_FORCE(s_ptexSceneNormalsMap);
	SAFE_RELEASE_FORCE(s_ptexSceneNormalsBent);
	SAFE_RELEASE_FORCE(s_ptexAOColorBleed);
	SAFE_RELEASE_FORCE(s_ptexSceneDiffuse);
	SAFE_RELEASE_FORCE(s_ptexSceneSpecular);
#if defined(DURANGO_USE_ESRAM)
	SAFE_RELEASE_FORCE(s_ptexSceneSpecularESRAM);
#endif
	SAFE_RELEASE_FORCE(s_ptexVelocityObjects[0]);
	SAFE_RELEASE_FORCE(s_ptexVelocityObjects[1]);
	SAFE_RELEASE_FORCE(s_ptexSceneDiffuseAccMap);
	SAFE_RELEASE_FORCE(s_ptexSceneSpecularAccMap);
	SAFE_RELEASE_FORCE(s_ptexBackBuffer);
	SAFE_RELEASE_FORCE(s_ptexSceneTarget);
	SAFE_RELEASE_FORCE(s_ptexZTargetScaled);
	SAFE_RELEASE_FORCE(s_ptexZTargetScaled2);

	gcpRendD3D->m_bSystemTargetsInit = 0;
}

void CTexture::ReleaseMiscTargets()
{
	if (gcpRendD3D->m_pColorGradingControllerD3D)
		gcpRendD3D->m_pColorGradingControllerD3D->ReleaseTextures();
}

void CTexture::CreateSystemTargets()
{
	if (!gcpRendD3D->m_bSystemTargetsInit)
	{
		gcpRendD3D->m_bSystemTargetsInit = 1;

		ETEX_Format eTF = (gcpRendD3D->m_RP.m_bUseHDR && gcpRendD3D->m_nHDRType == 1) ? eTF_R16G16B16A16F : eTF_R8G8B8A8;

		// Create HDR targets
		CTexture::GenerateHDRMaps();

		// Create scene targets
		CTexture::GenerateSceneMap(eTF);

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
		gcpRendD3D->GetVolumetricFog().CreateResources();

		// Create post effects targets
		SPostEffectsUtils::Create();
	}
}

void CTexture::CopySliceChain(CDeviceTexture* const pDevTexture, int ownerMips, int nDstSlice, int nDstMip, CDeviceTexture* pSrcDevTex, int nSrcSlice, int nSrcMip, int nSrcMips, int nNumMips)
{
	HRESULT hr = S_OK;
	assert(pSrcDevTex && pDevTexture);

#if CRY_PLATFORM_DURANGO
	pDevTexture->InitD3DTexture();
#endif

	D3DBaseTexture* pDstResource = pDevTexture->GetBaseTexture();
	D3DBaseTexture* pSrcResource = pSrcDevTex->GetBaseTexture();

#ifndef _RELEASE
	if (!pDstResource) __debugbreak();
	if (!pSrcResource) __debugbreak();
#endif

	if (0)
	{
	}
#if CRY_PLATFORM_DURANGO
	else if (!gcpRendD3D->m_pRT->IsRenderThread())
	{
		// We can use the move engine!

		CryAutoLock<CryCriticalSection> dmaLock(gcpRendD3D->m_dma1Lock);

		ID3D11DmaEngineContextX* pDMA = gcpRendD3D->m_pDMA1;

		for (int nMip = 0; nMip < nNumMips; ++nMip)
		{
			pDMA->CopySubresourceRegion(
			  pDstResource,
			  D3D11CalcSubresource(nDstMip + nMip, nDstSlice, ownerMips),
			  0, 0, 0,
			  pSrcResource,
			  D3D11CalcSubresource(nSrcMip + nMip, nSrcSlice, nSrcMips),
			  NULL,
			  D3D11_COPY_NO_OVERWRITE);
		}

	#ifdef DURANGO_MONOD3D_DRIVER
		UINT64 fence = pDMA->InsertFence(D3D11_INSERT_FENCE_NO_KICKOFF);
	#else
		UINT64 fence = pDMA->InsertFence();
	#endif
		pDMA->Submit();
		while (gcpRendD3D->GetPerformanceDevice().IsFencePending(fence))
			CrySleep(1);
	}
#endif
	else
	{
		assert(nSrcMip >= 0 && nDstMip >= 0);
		for (int i = 0; i < nNumMips; ++i)
		{
#if defined(DEVICE_SUPPORTS_D3D11_1)
			gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(
			  pDstResource,
			  D3D11CalcSubresource(nDstMip + i, nDstSlice, ownerMips),
			  0, 0, 0,
			  pSrcResource,
			  D3D11CalcSubresource(nSrcMip + i, nSrcSlice, nSrcMips),
			  NULL,
			  D3D11_COPY_NO_OVERWRITE);
#else
			gcpRendD3D->GetDeviceContext().CopySubresourceRegion(
			  pDstResource,
			  D3D11CalcSubresource(nDstMip + i, nDstSlice, ownerMips),
			  0, 0, 0,
			  pSrcResource,
			  D3D11CalcSubresource(nSrcMip + i, nSrcSlice, nSrcMips),
			  NULL);
#endif
		}
	}
}

//===========================================================================================================
