// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeviceFormats.h"

#include <CryRenderer/ITexture.h> // ETEX_Format

#include "DriverD3D.h"

bool SPixFormat::CheckSupport(D3DFormat Format, const char* szDescr)
{
	bool bRes = false;
	CD3D9Renderer* rd = gcpRendD3D;

	Init();
	if ((Options = GetDeviceObjectFactory().QueryFormatSupport(Format)) & (FMTSUPPORT_TEXTURE2D | FMTSUPPORT_TEXTURECUBE))
	{
		Options |= DeviceFormats::IsSRGBReadable(Format) * FMTSUPPORT_SRGB;

		DeviceFormat = Format;
#if !CRY_PLATFORM_ORBIS
		MaxWidth = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
		MaxHeight = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
#else
		MaxWidth = 2048;
		MaxHeight = 2048;
#endif
		Desc = szDescr;
		BitsPerPixel = -CTexture::BitsPerPixel(DeviceFormats::ConvertToTexFormat(Format));

		// If we check for support of DepthStencil formats (by using the typeless variant) also include the capabilities of the casted DS format
		if (DeviceFormats::IsTypeless(Format) && DeviceFormats::IsDepthStencil(Format))
		{
			Options |= GetDeviceObjectFactory().QueryFormatSupport(DeviceFormats::ConvertToDepthStencil(Format));

#if CRY_PLATFORM_DURANGO
			// Buggy XDK
			Options |= FMTSUPPORT_DEPTH_STENCIL;
#endif
		}

		// *INDENT-OFF*
		const bool bCanDS            = (Options & FMTSUPPORT_DEPTH_STENCIL           ) != 0;
		const bool bCanRT            = (Options & FMTSUPPORT_RENDER_TARGET           ) != 0;
		const bool bCanMultiSampleRT = (Options & FMTSUPPORT_MULTISAMPLE_RENDERTARGET) != 0;
		const bool bCanMips          = (Options & FMTSUPPORT_MIP                     ) != 0;
		const bool bCanSample        = (Options & FMTSUPPORT_SHADER_SAMPLE           ) != 0;
		const bool bCanSampleCmp     = (Options & FMTSUPPORT_SHADER_SAMPLE_COMPARISON) != 0;
		const bool bCanGather        = (Options & FMTSUPPORT_SHADER_GATHER           ) != 0;
		const bool bCanGatherCmp     = (Options & FMTSUPPORT_SHADER_GATHER_COMPARISON) != 0;
		const bool bCanBlend         = (Options & FMTSUPPORT_BLENDABLE               ) != 0;
		const bool bCanReadSRGB      = (Options & FMTSUPPORT_SRGB                    ) != 0;
		// *INDENT-ON*

		if (bCanMips || bCanReadSRGB || bCanDS || bCanRT || bCanSample || bCanGather || bCanBlend)
			iLog->Log("  %s" "%s%s%s%s%s%s%s%s%s%s",
				szDescr,
				bCanMips ? ", mips" : "",
				bCanReadSRGB ? ", sRGB" : "",
				bCanDS ? ", DS" : "",
				bCanRT ? ", RT" : "",
				bCanBlend ? " (blendable)" : "",
				bCanMultiSampleRT ? " (multi-sampled)" : "",
				bCanSample ? ", sample" : "",
				bCanSampleCmp ? " (comparable)" : "",
				bCanGather ? ", gather" : "",
				bCanGatherCmp ? " (comparable)" : ""
			);
		else
			iLog->Log("  %s", szDescr);

		Next = CRendererResources::s_hwTexFormatSupport.m_FirstPixelFormat;
		CRendererResources::s_hwTexFormatSupport.m_FirstPixelFormat = this;

		bRes = true;
	}

	return bRes;
}

void SPixFormatSupport::CheckFormatSupport()
{
	iLog->Log("Using pixel texture formats:");

	m_FirstPixelFormat = NULL;

	m_FormatR8G8B8A8S.CheckSupport(DXGI_FORMAT_R8G8B8A8_SNORM, "R8G8B8A8S");
	m_FormatR8G8B8A8.CheckSupport(DXGI_FORMAT_R8G8B8A8_UNORM, "R8G8B8A8");

	m_FormatR1.CheckSupport(DXGI_FORMAT_R1_UNORM, "R1");
	m_FormatA8.CheckSupport(DXGI_FORMAT_A8_UNORM, "A8");
	m_FormatR8.CheckSupport(DXGI_FORMAT_R8_UNORM, "R8");
	m_FormatR8S.CheckSupport(DXGI_FORMAT_R8_SNORM, "R8S");
	m_FormatR16.CheckSupport(DXGI_FORMAT_R16_UNORM, "R16");
	m_FormatR16S.CheckSupport(DXGI_FORMAT_R16_SNORM, "R16S");
	m_FormatR16F.CheckSupport(DXGI_FORMAT_R16_FLOAT, "R16F");
	m_FormatR32F.CheckSupport(DXGI_FORMAT_R32_FLOAT, "R32F");
	m_FormatR8G8.CheckSupport(DXGI_FORMAT_R8G8_UNORM, "R8G8");
	m_FormatR8G8S.CheckSupport(DXGI_FORMAT_R8G8_SNORM, "R8G8S");
	m_FormatR16G16.CheckSupport(DXGI_FORMAT_R16G16_UNORM, "R16G16");
	m_FormatR16G16S.CheckSupport(DXGI_FORMAT_R16G16_SNORM, "R16G16S");
	m_FormatR16G16F.CheckSupport(DXGI_FORMAT_R16G16_FLOAT, "R16G16F");
	m_FormatR32G32F.CheckSupport(DXGI_FORMAT_R32G32_FLOAT, "R32G32F");
	m_FormatR11G11B10F.CheckSupport(DXGI_FORMAT_R11G11B10_FLOAT, "R11G11B10F");
	m_FormatR10G10B10A2.CheckSupport(DXGI_FORMAT_R10G10B10A2_UNORM, "R10G10B10A2");
	m_FormatR16G16B16A16.CheckSupport(DXGI_FORMAT_R16G16B16A16_UNORM, "R16G16B16A16");
	m_FormatR16G16B16A16S.CheckSupport(DXGI_FORMAT_R16G16B16A16_SNORM, "R16G16B16A16S");
	m_FormatR16G16B16A16F.CheckSupport(DXGI_FORMAT_R16G16B16A16_FLOAT, "R16G16B16A16F");
	m_FormatR32G32B32A32F.CheckSupport(DXGI_FORMAT_R32G32B32A32_FLOAT, "R32G32B32A32F");

	m_FormatBC1.CheckSupport(DXGI_FORMAT_BC1_UNORM, "BC1");
	m_FormatBC2.CheckSupport(DXGI_FORMAT_BC2_UNORM, "BC2");
	m_FormatBC3.CheckSupport(DXGI_FORMAT_BC3_UNORM, "BC3");
	m_FormatBC4U.CheckSupport(DXGI_FORMAT_BC4_UNORM, "BC4");
	m_FormatBC4S.CheckSupport(DXGI_FORMAT_BC4_SNORM, "BC4S");
	m_FormatBC5U.CheckSupport(DXGI_FORMAT_BC5_UNORM, "BC5");
	m_FormatBC5S.CheckSupport(DXGI_FORMAT_BC5_SNORM, "BC5S");
	m_FormatBC6UH.CheckSupport(DXGI_FORMAT_BC6H_UF16, "BC6UH");
	m_FormatBC6SH.CheckSupport(DXGI_FORMAT_BC6H_SF16, "BC6SH");
	m_FormatBC7.CheckSupport(DXGI_FORMAT_BC7_UNORM, "BC7");
	m_FormatR9G9B9E5.CheckSupport(DXGI_FORMAT_R9G9B9E5_SHAREDEXP, "R9G9B9E5");

	// Depth/Stencil formats
	m_FormatD32FS8.CheckSupport(DXGI_FORMAT_R32G8X24_TYPELESS, "D32FS8 (R32G8T)");
	m_FormatD32F.CheckSupport(DXGI_FORMAT_R32_TYPELESS, "D32F (R32T)");
#if CRY_PLATFORM_DURANGO
	m_FormatD24S8.CheckSupport(DXGI_FORMAT_UNKNOWN, "D24S8 (R24G8T)");
#else
	m_FormatD24S8.CheckSupport(DXGI_FORMAT_R24G8_TYPELESS, "D24S8 (R24G8T)");
#endif

#if CRY_RENDERER_VULKAN
	m_FormatD24.CheckSupport(DXGI_FORMAT_R24X8_TYPELESS, "D24 (R24T)");
	m_FormatD16S8.CheckSupport(DXGI_FORMAT_R16G8X8_TYPELESS, "D16S8 (R16G8T)");
#elif CRY_RENDERER_GNM
	m_FormatD24.CheckSupport(DXGI_FORMAT_UNKNOWN, "D24 (R24T)"); // Not supported, but should initialize
	m_FormatD16S8.CheckSupport(DXGI_FORMAT_R16G8_TYPELESS, "D16S8 (R16G8T)");
#endif
	m_FormatD16.CheckSupport(DXGI_FORMAT_R16_TYPELESS, "D16 (R16T)");
#if CRY_RENDERER_VULKAN
	m_FormatS8.CheckSupport(DXGI_FORMAT_R8_TYPELESS, "S8 (R8T)");
#elif CRY_RENDERER_GNM
	m_FormatS8.CheckSupport(DXGI_FORMAT_S8_UINT, "S8 (R8T)");
#endif

#if CRY_RENDERER_VULKAN
	m_FormatR4G4.CheckSupport(DXGI_FORMAT_R4G4_UNORM, "R4G4");
	m_FormatR4G4B4A4.CheckSupport(DXGI_FORMAT_R4G4B4A4_UNORM, "R4G4B4A4");
#endif
	m_FormatB5G6R5.CheckSupport(DXGI_FORMAT_B5G6R5_UNORM, "B5G6R5");
	m_FormatB5G5R5.CheckSupport(DXGI_FORMAT_B5G5R5A1_UNORM, "B5G5R5");
	m_FormatB4G4R4A4.CheckSupport(DXGI_FORMAT_B4G4R4A4_UNORM, "B4G4R4A4");

	m_FormatB8G8R8A8.CheckSupport(DXGI_FORMAT_B8G8R8A8_UNORM, "B8G8R8A8");
	m_FormatB8G8R8X8.CheckSupport(DXGI_FORMAT_B8G8R8X8_UNORM, "B8G8R8X8");

#if CRY_RENDERER_VULKAN || CRY_RENDERER_OPENGL
	m_FormatEAC_R11.CheckSupport(DXGI_FORMAT_EAC_R11_UNORM, "EAC_R11");
	m_FormatEAC_R11S.CheckSupport(DXGI_FORMAT_EAC_R11_SNORM, "EAC_R11S");
	m_FormatEAC_RG11.CheckSupport(DXGI_FORMAT_EAC_RG11_UNORM, "EAC_RG11");
	m_FormatEAC_RG11S.CheckSupport(DXGI_FORMAT_EAC_RG11_SNORM, "EAC_RG11S");
	m_FormatETC2.CheckSupport(DXGI_FORMAT_ETC2_UNORM, "ETC2");
	m_FormatETC2A.CheckSupport(DXGI_FORMAT_ETC2A_UNORM, "ETC2A");

	m_FormatASTC_LDR.CheckSupport(DXGI_FORMAT_ASTC_4x4_UNORM, "ASTC_LDR");
//	m_FormatASTC_HDR.CheckSupport(DXGI_FORMAT_ASTC_4x4_UINT, "ASTC_HDR");
#endif //CRY_RENDERER_OPENGL

	// Pre-calculate all possible queries
	for (ETEX_Format eQueryable = eTF_Unknown; eQueryable != eTF_MaxFormat; eQueryable = ETEX_Format(eQueryable + 1))
		m_FormatSupportedCache[eQueryable] = _IsFormatSupported(eQueryable);
	for (ETEX_Format eQueryable = eTF_Unknown; eQueryable != eTF_MaxFormat; eQueryable = ETEX_Format(eQueryable + 1))
		m_FormatClosestCacheEnm[eQueryable] = _GetClosestFormatSupported(eQueryable, m_FormatClosestCachePtr[eQueryable]);
	for (ETEX_Format eQueryable = eTF_Unknown; eQueryable != eTF_MaxFormat; eQueryable = ETEX_Format(eQueryable + 1))
		m_FormatLessPreciseCache[eQueryable] = _GetLessPreciseFormatSupported(eQueryable);
}

bool SPixFormatSupport::_IsFormatSupported(ETEX_Format eTFDst)
{
	D3DFormat D3DFmt = DeviceFormats::ConvertFromTexFormat(eTFDst);
	if (!D3DFmt || (D3DFmt == DXGI_FORMAT_UNKNOWN))
		return false;

	for (SPixFormat* pFmt = m_FirstPixelFormat; pFmt; pFmt = pFmt->Next)
		if (pFmt->DeviceFormat == D3DFmt && pFmt->IsValid())
			return true;

	return false;
}

ETEX_Format SPixFormatSupport::_GetLessPreciseFormatSupported(ETEX_Format eTFDst)
{
	switch (eTFDst)
	{
	case eTF_R16:
		if (m_FormatR16.IsValid())
			return eTF_R16;
	case eTF_R16S:
		if (m_FormatR16S.IsValid())
			return eTF_R16S;
	case eTF_R16F:
		if (m_FormatR16F.IsValid())
			return eTF_R16F;

		break;

	case eTF_R16G16:
		if (m_FormatR16G16.IsValid())
			return eTF_R16G16;
	case eTF_R16G16S:
		if (m_FormatR16G16S.IsValid())
			return eTF_R16G16S;
	case eTF_R16G16F:
		if (m_FormatR16G16F.IsValid())
			return eTF_R16G16F;

		break;

	case eTF_R16G16B16A16:
		if (m_FormatR16G16B16A16.IsValid())
			return eTF_R16G16B16A16;
	case eTF_R16G16B16A16S:
		if (m_FormatR16G16B16A16S.IsValid())
			return eTF_R16G16B16A16S;
	case eTF_R16G16B16A16F:
		if (m_FormatR16G16B16A16F.IsValid())
			return eTF_R16G16B16A16F;

		break;

	default:
		if (IsFormatSupported(eTFDst))
			return eTFDst;

		break;
	}

	return eTF_Unknown;
}

ETEX_Format SPixFormatSupport::_GetClosestFormatSupported(ETEX_Format eTFDst, const SPixFormat*& pPF)
{
	switch (eTFDst)
	{
#if CRY_RENDERER_VULKAN
	case eTF_R4G4:
		if (m_FormatR4G4.IsValid())
		{
			pPF = &m_FormatR4G4;
			return eTF_R4G4;
		}
		if (m_FormatR8G8.IsValid())
		{
			pPF = &m_FormatR8G8;
			return eTF_R8G8;
		}
	case eTF_R4G4B4A4:
		if (m_FormatR4G4B4A4.IsValid())
		{
			pPF = &m_FormatR4G4B4A4;
			return eTF_R4G4;
		}
		if (m_FormatR8G8B8A8.IsValid())
		{
			pPF = &m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		break;
#endif

	case eTF_B5G6R5:
		if (m_FormatB5G6R5.IsValid())
		{
			pPF = &m_FormatB5G6R5;
			return eTF_B5G6R5;
		}
	case eTF_B8G8R8X8:
		if (m_FormatB8G8R8X8.IsValid())
		{
			pPF = &m_FormatB8G8R8X8;
			return eTF_B8G8R8X8;
		}
		if (m_FormatB8G8R8A8.IsValid())
		{
			pPF = &m_FormatB8G8R8A8;
			return eTF_B8G8R8A8;
		}
		break;

	case eTF_B5G5R5A1:
		if (m_FormatB5G5R5.IsValid())
		{
			pPF = &m_FormatB5G5R5;
			return eTF_B5G5R5A1;
		}
		if (m_FormatB8G8R8A8.IsValid())
		{
			pPF = &m_FormatB8G8R8A8;
			return eTF_B8G8R8A8;
		}
		break;

	case eTF_B4G4R4A4:
		if (m_FormatB4G4R4A4.IsValid())
		{
			pPF = &m_FormatB4G4R4A4;
			return eTF_B4G4R4A4;
		}
	case eTF_B8G8R8A8:
		if (m_FormatB8G8R8A8.IsValid())
		{
			pPF = &m_FormatB8G8R8A8;
			return eTF_B8G8R8A8;
		}
		break;

	case eTF_A8:
		if (m_FormatA8.IsValid())
		{
			pPF = &m_FormatA8;
			return eTF_A8;
		}
		break;

	case eTF_R1:
		if (m_FormatR1.IsValid())
		{
			pPF = &m_FormatR1;
			return eTF_R1;
		}
		break;

	case eTF_R8:
		if (m_FormatR8.IsValid())
		{
			pPF = &m_FormatR8;
			return eTF_R8;
		}
	case eTF_R8G8:
		if (m_FormatR8G8.IsValid())
		{
			pPF = &m_FormatR8G8;
			return eTF_R8G8;
		}
	case eTF_R8G8B8A8:
		if (m_FormatR8G8B8A8.IsValid())
		{
			pPF = &m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		break;

	case eTF_R8S:
		if (m_FormatR8S.IsValid())
		{
			pPF = &m_FormatR8S;
			return eTF_R8S;
		}
	case eTF_R8G8S:
		if (m_FormatR8G8S.IsValid())
		{
			pPF = &m_FormatR8G8S;
			return eTF_R8G8S;
		}
	case eTF_R8G8B8A8S:
		if (m_FormatR8G8B8A8S.IsValid())
		{
			pPF = &m_FormatR8G8B8A8S;
			return eTF_R8G8B8A8S;
		}
		break;

	case eTF_R16:
		if (m_FormatR16.IsValid())
		{
			pPF = &m_FormatR16;
			return eTF_R16;
		}
	case eTF_R16G16:
		if (m_FormatR16G16.IsValid())
		{
			pPF = &m_FormatR16G16;
			return eTF_R16G16;
		}
	case eTF_R16G16B16A16:
		if (m_FormatR16G16B16A16.IsValid())
		{
			pPF = &m_FormatR16G16B16A16;
			return eTF_R16G16B16A16;
		}
		break;

	case eTF_R16S:
		if (m_FormatR16S.IsValid())
		{
			pPF = &m_FormatR16S;
			return eTF_R16S;
		}
	case eTF_R16G16S:
		if (m_FormatR16G16S.IsValid())
		{
			pPF = &m_FormatR16G16S;
			return eTF_R16G16S;
		}
	case eTF_R16G16B16A16S:
		if (m_FormatR16G16B16A16S.IsValid())
		{
			pPF = &m_FormatR16G16B16A16S;
			return eTF_R16G16B16A16S;
		}
		break;

	case eTF_R16F:
		if (m_FormatR16F.IsValid())
		{
			pPF = &m_FormatR16F;
			return eTF_R16F;
		}
	case eTF_R16G16F:
		if (m_FormatR16G16F.IsValid())
		{
			pPF = &m_FormatR16G16F;
			return eTF_R16G16F;
		}
	case eTF_R16G16B16A16F:
		if (m_FormatR16G16B16A16F.IsValid())
		{
			pPF = &m_FormatR16G16B16A16F;
			return eTF_R16G16B16A16F;
		}
		break;

	case eTF_R32F:
		if (m_FormatR32F.IsValid())
		{
			pPF = &m_FormatR32F;
			return eTF_R32F;
		}
	case eTF_R32G32F:
		if (m_FormatR32G32F.IsValid())
		{
			pPF = &m_FormatR32G32F;
			return eTF_R32G32F;
		}
	case eTF_R32G32B32A32F:
		if (m_FormatR32G32B32A32F.IsValid())
		{
			pPF = &m_FormatR32G32B32A32F;
			return eTF_R32G32B32A32F;
		}
		break;

	case eTF_R11G11B10F:
		if (m_FormatR11G11B10F.IsValid())
		{
			pPF = &m_FormatR11G11B10F;
			return eTF_R11G11B10F;
		}
		break;

	case eTF_R10G10B10A2:
		if (m_FormatR10G10B10A2.IsValid())
		{
			pPF = &m_FormatR10G10B10A2;
			return eTF_R10G10B10A2;
		}
		break;

	case eTF_BC1:
		if (m_FormatBC1.IsValid())
		{
			pPF = &m_FormatBC1;
			return eTF_BC1;
		}
		if (m_FormatR8G8B8A8.IsValid())
		{
			pPF = &m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		break;

	case eTF_BC2:
		if (m_FormatBC2.IsValid())
		{
			pPF = &m_FormatBC2;
			return eTF_BC2;
		}
		if (m_FormatR8G8B8A8.IsValid())
		{
			pPF = &m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		break;

	case eTF_BC3:
		if (m_FormatBC3.IsValid())
		{
			pPF = &m_FormatBC3;
			return eTF_BC3;
		}
		if (m_FormatR8G8B8A8.IsValid())
		{
			pPF = &m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		break;

	case eTF_BC4U:
		if (m_FormatBC4U.IsValid())
		{
			pPF = &m_FormatBC4U;
			return eTF_BC4U;
		}
		if (m_FormatR8.IsValid())
		{
			pPF = &m_FormatR8;
			return eTF_R8;
		}
		break;

	case eTF_BC4S:
		if (m_FormatBC4S.IsValid())
		{
			pPF = &m_FormatBC4S;
			return eTF_BC4S;
		}
		if (m_FormatR8S.IsValid())
		{
			pPF = &m_FormatR8S;
			return eTF_R8S;
		}
		break;

	case eTF_BC5U:
		if (m_FormatBC5U.IsValid())
		{
			pPF = &m_FormatBC5U;
			return eTF_BC5U;
		}
		if (m_FormatR8G8.IsValid())
		{
			pPF = &m_FormatR8G8;
			return eTF_R8G8;
		}
		break;

	case eTF_BC5S:
		if (m_FormatBC5S.IsValid())
		{
			pPF = &m_FormatBC5S;
			return eTF_BC5S;
		}
		if (m_FormatR8G8S.IsValid())
		{
			pPF = &m_FormatR8G8S;
			return eTF_R8G8S;
		}
		break;

	case eTF_BC6UH:
		if (m_FormatBC6UH.IsValid())
		{
			pPF = &m_FormatBC6UH;
			return eTF_BC6UH;
		}
		if (m_FormatR16G16B16A16F.IsValid())
		{
			pPF = &m_FormatR16G16B16A16F;
			return eTF_R16G16B16A16F;
		}
		break;

	case eTF_BC6SH:
		if (m_FormatBC6SH.IsValid())
		{
			pPF = &m_FormatBC6SH;
			return eTF_BC6SH;
		}
		if (m_FormatR16G16B16A16F.IsValid())
		{
			pPF = &m_FormatR16G16B16A16F;
			return eTF_R16G16B16A16F;
		}
		break;

	case eTF_BC7:
		if (m_FormatBC7.IsValid())
		{
			pPF = &m_FormatBC7;
			return eTF_BC7;
		}
		if (m_FormatR8G8B8A8.IsValid())
		{
			pPF = &m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		break;

	case eTF_R9G9B9E5:
		if (m_FormatR9G9B9E5.IsValid())
		{
			pPF = &m_FormatR9G9B9E5;
			return eTF_R9G9B9E5;
		}
		if (m_FormatR16G16B16A16F.IsValid())
		{
			pPF = &m_FormatR16G16B16A16F;
			return eTF_R16G16B16A16F;
		}
		break;

#if CRY_RENDERER_VULKAN || CRY_RENDERER_GNM
	case eTF_S8:
		if (m_FormatS8.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatS8;
			return eTF_S8;
		}
	case eTF_D16S8:
		if (m_FormatD16S8.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatD16S8;
			return eTF_D16S8;
		}
#else
	case eTF_S8:
	case eTF_D16S8:
#endif
	case eTF_D24S8:
		if (m_FormatD24S8.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatD24S8;
			return eTF_D24S8;
		}
	case eTF_D32FS8:
		if (m_FormatD32FS8.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatD32FS8;
			return eTF_D32FS8;
		}
		break;

	case eTF_D16:
		if (m_FormatD16.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatD16;
			return eTF_D16;
		}
#if CRY_RENDERER_VULKAN || CRY_RENDERER_GNM
		if (m_FormatD16S8.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatD16S8;
			return eTF_D16S8;
		}
	case eTF_D24:
		if (m_FormatD24.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatD24;
			return eTF_D24;
		}
		if (m_FormatD24S8.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatD24S8;
			return eTF_D24S8;
		}
#else
	case eTF_D24:
#endif
	case eTF_D32F:
		if (m_FormatD32F.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatD32F;
			return eTF_D32F;
		}
		if (m_FormatD32FS8.IsValid(FMTSUPPORT_DEPTH_STENCIL))
		{
			pPF = &m_FormatD32FS8;
			return eTF_D32FS8;
		}
		break;

#if CRY_RENDERER_VULKAN || CRY_RENDERER_OPENGL
	case eTF_EAC_R11:
		if (m_FormatEAC_R11.IsValid())
		{
			pPF = &m_FormatEAC_R11;
			return eTF_EAC_R11;
		}
		if (m_FormatR8.IsValid())
		{
			pPF = &m_FormatR8;
			return eTF_R8;
		}
		break;

	case eTF_EAC_R11S:
		if (m_FormatEAC_R11S.IsValid())
		{
			pPF = &m_FormatEAC_R11S;
			return eTF_EAC_R11S;
		}
		if (m_FormatR8S.IsValid())
		{
			pPF = &m_FormatR8S;
			return eTF_R8S;
		}
		break;

	case eTF_EAC_RG11:
		if (m_FormatEAC_RG11.IsValid())
		{
			pPF = &m_FormatEAC_RG11;
			return eTF_EAC_RG11;
		}
		if (m_FormatR8G8.IsValid())
		{
			pPF = &m_FormatR8G8;
			return eTF_R8G8;
		}
		break;

	case eTF_EAC_RG11S:
		if (m_FormatEAC_RG11S.IsValid())
		{
			pPF = &m_FormatEAC_RG11S;
			return eTF_EAC_RG11S;
		}
		if (m_FormatR8G8S.IsValid())
		{
			pPF = &m_FormatR8G8S;
			return eTF_R8G8S;
		}
		break;

	case eTF_ETC2:
		if (m_FormatETC2.IsValid())
		{
			pPF = &m_FormatETC2;
			return eTF_ETC2;
		}
		if (m_FormatR8G8B8A8.IsValid())
		{
			pPF = &m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		break;

	case eTF_ETC2A:
		if (m_FormatETC2A.IsValid())
		{
			pPF = &m_FormatETC2A;
			return eTF_ETC2A;
		}
		if (m_FormatR8G8B8A8.IsValid())
		{
			pPF = &m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		break;

	case eTF_ASTC_LDR_4x4:
		if (m_FormatASTC_LDR.IsValid())
		{
			pPF = &m_FormatASTC_LDR;
			return eTF_ASTC_LDR_4x4;
		}
		if (m_FormatR8G8B8A8.IsValid())
		{
			pPF = &m_FormatR8G8B8A8;
			return eTF_R8G8B8A8;
		}
		break;
#endif

	default:
		pPF = nullptr;
		return eTF_Unknown;
	}

	pPF = nullptr;
	return eTF_Unknown;
}

//---------------------------------------------------------------------------------------------------------------------
bool DeviceFormats::IsTypeless(D3DFormat nFormat)
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
#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R16G8X8_TYPELESS:
	case DXGI_FORMAT_R24X8_TYPELESS:
#endif

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

bool DeviceFormats::IsDepthStencil(D3DFormat nFormat)
{
	switch (nFormat)
	{
#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_S8_UINT:
#endif

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_D16_UNORM:

#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R16G8X8_TYPELESS:
	case DXGI_FORMAT_D16_UNORM_S8_UINT:
	case DXGI_FORMAT_R24X8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM:
#endif

#if CRY_RENDERER_GNM
	case DXGI_FORMAT_R16G8_TYPELESS:
	case DXGI_FORMAT_D16_UNORM_S8_UINT:
#endif

	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT:

	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return true;

	default:
		break;
	}

	return false;
}

bool DeviceFormats::IsSRGBReadable(D3DFormat nFormat)
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

#if CRY_RENDERER_VULKAN || (CRY_RENDERER_OPENGL >= 430)
	case DXGI_FORMAT_ETC2_UNORM:
		return true;
	case DXGI_FORMAT_ETC2A_UNORM:
		return true;
	case DXGI_FORMAT_ASTC_4x4_UNORM:
		return true;
#endif //CRY_RENDERER_OPENGL

	default:
		break;
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------------
D3DFormat DeviceFormats::ConvertFromTexFormat(ETEX_Format eTF)
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
	case eTF_R16S:
		return DXGI_FORMAT_R16_SNORM;
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
	case eTF_R32G32F:
		return DXGI_FORMAT_R32G32_FLOAT;
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
#if CRY_RENDERER_VULKAN
	case eTF_S8:
		return DXGI_FORMAT_S8_UINT;
#endif
	case eTF_D16:
		return DXGI_FORMAT_D16_UNORM;
#if CRY_RENDERER_VULKAN
	case eTF_D16S8:
		return DXGI_FORMAT_D16_UNORM_S8_UINT;
	case eTF_D24:
		return DXGI_FORMAT_D24_UNORM;
#endif
	case eTF_D24S8:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case eTF_D32F:
		return DXGI_FORMAT_D32_FLOAT;
	case eTF_D32FS8:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

		// only available as hardware format under DX11.1 with DXGI 1.2
	case eTF_B5G6R5:
		return DXGI_FORMAT_B5G6R5_UNORM;
	case eTF_B5G5R5A1:
		return DXGI_FORMAT_B5G5R5A1_UNORM;
	case eTF_B4G4R4A4:
		return DXGI_FORMAT_B4G4R4A4_UNORM;

#if CRY_RENDERER_VULKAN
		// only available as hardware format under Vulkan
	case eTF_R4G4:
		return DXGI_FORMAT_R4G4_UNORM;
	case eTF_R4G4B4A4:
		return DXGI_FORMAT_R4G4B4A4_UNORM;
#endif
#if CRY_RENDERER_VULKAN || CRY_RENDERER_OPENGL
		// only available as hardware format under OpenGL/
	case eTF_EAC_R11:
		return DXGI_FORMAT_EAC_R11_UNORM;
	case eTF_EAC_R11S:
		return DXGI_FORMAT_EAC_R11_SNORM;
	case eTF_EAC_RG11:
		return DXGI_FORMAT_EAC_RG11_UNORM;
	case eTF_EAC_RG11S:
		return DXGI_FORMAT_EAC_RG11_SNORM;
	case eTF_ETC2:
		return DXGI_FORMAT_ETC2_UNORM;
	case eTF_ETC2A:
		return DXGI_FORMAT_ETC2A_UNORM;
	case eTF_ASTC_LDR_4x4:
		return DXGI_FORMAT_ASTC_4x4_UNORM;
#endif //CRY_RENDERER_OPENGL

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
		return DXGI_FORMAT_UNKNOWN;
	}

	return DXGI_FORMAT_UNKNOWN;
}

uint32 DeviceFormats::GetWriteMask(ETEX_Format eTF)
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
	case eTF_R8S:
	case eTF_R16:
	case eTF_R16S:
	case eTF_R16F:
	case eTF_R32F:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_R8G8:
	case eTF_R8G8S:
	case eTF_R16G16:
	case eTF_R16G16S:
	case eTF_R16G16F:
	case eTF_R32G32F:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_R11G11B10F:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_R10G10B10A2:
	case eTF_R16G16B16A16:
	case eTF_R16G16B16A16S:
	case eTF_R16G16B16A16F:
	case eTF_R32G32B32A32F:
		return D3D11_COLOR_WRITE_ENABLE_ALL;

	case eTF_BC1:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_BC2:
	case eTF_BC3:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_BC4U:
	case eTF_BC4S:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_BC5U:
	case eTF_BC5S:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_BC6UH:
	case eTF_BC6SH:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_BC7:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_R9G9B9E5:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;

		// hardware depth buffers
#if CRY_RENDERER_VULKAN
	case eTF_S8:
		return D3D11_COLOR_WRITE_ENABLE_RED;
#endif
	case eTF_D16:
		return D3D11_COLOR_WRITE_ENABLE_RED;
#if CRY_RENDERER_VULKAN
	case eTF_D16S8:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_D24:
		return D3D11_COLOR_WRITE_ENABLE_RED;
#endif
	case eTF_D24S8:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_D32F:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_D32FS8:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;

		// only available as hardware format under DX11.1 with DXGI 1.2
	case eTF_B5G6R5:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_B5G5R5A1:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_B4G4R4A4:
		return D3D11_COLOR_WRITE_ENABLE_ALL;

#if CRY_RENDERER_VULKAN
	case eTF_R4G4:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_R4G4B4A4:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
#endif
#if CRY_RENDERER_VULKAN || CRY_RENDERER_OPENGL
		// only available as hardware format under OpenGL
	case eTF_EAC_R11:
	case eTF_EAC_R11S:
		return D3D11_COLOR_WRITE_ENABLE_RED;
	case eTF_EAC_RG11:
	case eTF_EAC_RG11S:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
	case eTF_ETC2:
		return D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
	case eTF_ETC2A:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
	case eTF_ASTC_LDR_4x4:
		return D3D11_COLOR_WRITE_ENABLE_ALL;
#endif //CRY_RENDERER_OPENGL

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

uint32 DeviceFormats::GetWriteMask(D3DFormat nFormat)
{
	return GetWriteMask(ConvertToTexFormat(nFormat));
}

ETEX_Format DeviceFormats::ConvertToTexFormat(D3DFormat nFormat)
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
	case DXGI_FORMAT_R8_TYPELESS:
		return eTF_R8;
	case DXGI_FORMAT_R16_UNORM:
		return eTF_R16;
	case DXGI_FORMAT_R16_SNORM:
		return eTF_R16S;
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
	case DXGI_FORMAT_R32G32_FLOAT:
		return eTF_R32G32F;
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
#if CRY_RENDERER_VULKAN || CRY_RENDERER_GNM
	case DXGI_FORMAT_S8_UINT:
		return eTF_S8;
#endif
	case DXGI_FORMAT_D16_UNORM:
		return eTF_D16;
#if CRY_RENDERER_VULKAN || CRY_RENDERER_GNM
	case DXGI_FORMAT_D16_UNORM_S8_UINT:
		return eTF_D16S8;
#endif

#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_D24_UNORM:
		return eTF_D24;
#endif

	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return eTF_D24S8;
	case DXGI_FORMAT_D32_FLOAT:
		return eTF_D32F;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return eTF_D32FS8;

#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R16G8X8_TYPELESS:
		return eTF_D16S8;
	case DXGI_FORMAT_R24X8_TYPELESS:
		return eTF_D24;
#endif

#if CRY_RENDERER_GNM
	case DXGI_FORMAT_R16G8_TYPELESS:
		return eTF_D16S8;
#endif

	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		return eTF_D24S8;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return eTF_D32FS8;

		// only available as hardware format under DX11.1 with DXGI 1.2
	case DXGI_FORMAT_B5G6R5_UNORM:
		return eTF_B5G6R5;
	case DXGI_FORMAT_B5G5R5A1_UNORM:
		return eTF_B5G5R5A1;
	case DXGI_FORMAT_B4G4R4A4_UNORM:
		return eTF_B4G4R4A4;

#if CRY_RENDERER_VULKAN
		// only available as hardware format under Vulkan
	case DXGI_FORMAT_R4G4_UNORM:
		return eTF_R4G4;
	case DXGI_FORMAT_R4G4B4A4_UNORM:
		return eTF_R4G4B4A4;
#endif
#if CRY_RENDERER_VULKAN || CRY_RENDERER_OPENGL
		// only available as hardware format under OpenGL
	case DXGI_FORMAT_EAC_R11_UNORM:
		return eTF_EAC_R11;
	case DXGI_FORMAT_EAC_R11_SNORM:
		return eTF_EAC_R11S;
	case DXGI_FORMAT_EAC_RG11_UNORM:
		return eTF_EAC_RG11;
	case DXGI_FORMAT_EAC_RG11_SNORM:
		return eTF_EAC_RG11S;
	case DXGI_FORMAT_ETC2_UNORM:
		return eTF_ETC2;
	case DXGI_FORMAT_ETC2_UNORM_SRGB:
		return eTF_ETC2;
	case DXGI_FORMAT_ETC2A_UNORM:
		return eTF_ETC2A;
	case DXGI_FORMAT_ETC2A_UNORM_SRGB:
		return eTF_ETC2A;
	case DXGI_FORMAT_ASTC_4x4_UNORM:
		return eTF_ASTC_LDR_4x4;
	case DXGI_FORMAT_ASTC_4x4_UNORM_SRGB:
		return eTF_ASTC_LDR_4x4;
#endif

		// only available as hardware format under DX9
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return eTF_B8G8R8A8;
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return eTF_B8G8R8X8;

	default:
		assert(0);
	}

	return eTF_Unknown;
}

//---------------------------------------------------------------------------------------------------------------------
D3DFormat DeviceFormats::ConvertToDepthStencil(D3DFormat nFormat)
{
	switch (nFormat)
	{
#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_S8_UINT:
		return DXGI_FORMAT_S8_UINT;
#endif

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;

#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R16G8X8_TYPELESS:
	case DXGI_FORMAT_D16_UNORM_S8_UINT:
		return DXGI_FORMAT_D16_UNORM_S8_UINT;
	case DXGI_FORMAT_R24X8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM:
		return DXGI_FORMAT_D24_UNORM;
#endif

	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;

	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	default:
		assert(0);
	}

	return DXGI_FORMAT_UNKNOWN;
}

D3DFormat DeviceFormats::ConvertToStencilOnly(D3DFormat nFormat)
{
	switch (nFormat)
	{
#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_S8_UINT:
		return DXGI_FORMAT_S8_UINT;
#endif

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_UNKNOWN;

#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R16G8X8_TYPELESS:
	case DXGI_FORMAT_D16_UNORM_S8_UINT:
		return DXGI_FORMAT_D16_UNORM_S8_UINT;
	case DXGI_FORMAT_R24X8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM:
		return DXGI_FORMAT_UNKNOWN;
#endif

	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_UNKNOWN;

	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

	default:
		assert(0);
	}

	return DXGI_FORMAT_UNKNOWN;
}

D3DFormat DeviceFormats::ConvertToDepthOnly(D3DFormat nFormat)
{
	//handle special cases
	switch (nFormat)
	{
#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_S8_UINT:
		return DXGI_FORMAT_UNKNOWN;
#endif

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_UNORM;

#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R16G8X8_TYPELESS:
	case DXGI_FORMAT_D16_UNORM_S8_UINT:
		return DXGI_FORMAT_D16_UNORM_S8_UINT;
	case DXGI_FORMAT_R24X8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM:
		return DXGI_FORMAT_D24_UNORM;
#endif

	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;

	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

	default:
		assert(0);
	}

	return DXGI_FORMAT_UNKNOWN;
}

D3DFormat DeviceFormats::ConvertToSRGB(D3DFormat nFormat)
{
	switch (nFormat)
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

#if CRY_RENDERER_VULKAN || CRY_RENDERER_OPENGL
	case DXGI_FORMAT_ETC2_UNORM:
		return DXGI_FORMAT_ETC2_UNORM_SRGB;
	case DXGI_FORMAT_ETC2A_UNORM:
		return DXGI_FORMAT_ETC2A_UNORM_SRGB;
	case DXGI_FORMAT_ASTC_4x4_UNORM:
		return DXGI_FORMAT_ASTC_4x4_UNORM_SRGB;
#endif

		// AntonK: we don't need sRGB space for fp formats, because there is enough precision
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;

	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
#if CRY_RENDERER_VULKAN || CRY_RENDERER_OPENGL
	case DXGI_FORMAT_ETC2_UNORM_SRGB:
	case DXGI_FORMAT_ETC2A_UNORM_SRGB:
	case DXGI_FORMAT_ASTC_4x4_UNORM_SRGB:
#endif
		break;

	default:
		assert(0);
	}

	return nFormat;
}

D3DFormat DeviceFormats::ConvertToSigned(D3DFormat nFormat)
{
	switch (nFormat)
	{
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
		return DXGI_FORMAT_R8_SNORM;
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
		return DXGI_FORMAT_R8G8_SNORM;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_UNORM:
		return DXGI_FORMAT_R16_SNORM;
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_UNORM:
		return DXGI_FORMAT_R16G16_SNORM;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
		return DXGI_FORMAT_R16G16B16A16_SNORM;

	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
		return DXGI_FORMAT_BC4_SNORM;
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
		return DXGI_FORMAT_BC5_SNORM;
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
		return DXGI_FORMAT_BC6H_SF16;

	case DXGI_FORMAT_R8_UINT:
		return DXGI_FORMAT_R8_SINT;
	case DXGI_FORMAT_R8G8_UINT:
		return DXGI_FORMAT_R8G8_SINT;
	case DXGI_FORMAT_R8G8B8A8_UINT:
		return DXGI_FORMAT_R8G8B8A8_SINT;

	case DXGI_FORMAT_R16_UINT:
		return DXGI_FORMAT_R16_SINT;
	case DXGI_FORMAT_R16G16_UINT:
		return DXGI_FORMAT_R16G16_SINT;
	case DXGI_FORMAT_R16G16B16A16_UINT:
		return DXGI_FORMAT_R16G16B16A16_SINT;

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_UINT:
		return DXGI_FORMAT_R32_SINT;
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_UINT:
		return DXGI_FORMAT_R32G32_SINT;
	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_UINT:
		return DXGI_FORMAT_R32G32B32_SINT;
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_UINT:
		return DXGI_FORMAT_R32G32B32A32_SINT;

	default:
		assert(0);
	}

	return nFormat;
}

D3DFormat DeviceFormats::ConvertToUnsigned(D3DFormat nFormat)
{
	switch (nFormat)
	{
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_SNORM:
		return DXGI_FORMAT_R8_UNORM;
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_SNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		return DXGI_FORMAT_R10G10B10A2_UNORM;

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_SNORM:
		return DXGI_FORMAT_R16_UNORM;
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_SNORM:
		return DXGI_FORMAT_R16G16_UNORM;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
		return DXGI_FORMAT_R16G16B16A16_UNORM;

	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_SNORM:
		return DXGI_FORMAT_BC4_UNORM;
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_SNORM:
		return DXGI_FORMAT_BC5_UNORM;
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_SF16:
		return DXGI_FORMAT_BC6H_UF16;

	case DXGI_FORMAT_R16_SINT:
		return DXGI_FORMAT_R16_UINT;
	case DXGI_FORMAT_R16G16_SINT:
		return DXGI_FORMAT_R16G16_UINT;
	case DXGI_FORMAT_R16G16B16A16_SINT:
		return DXGI_FORMAT_R16G16B16A16_UINT;

	case DXGI_FORMAT_R8_SINT:
		return DXGI_FORMAT_R8_UINT;
	case DXGI_FORMAT_R8G8_SINT:
		return DXGI_FORMAT_R8G8_UINT;
	case DXGI_FORMAT_R8G8B8A8_SINT:
		return DXGI_FORMAT_R8G8B8A8_UINT;

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_SINT:
		return DXGI_FORMAT_R32_UINT;
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_SINT:
		return DXGI_FORMAT_R32G32_UINT;
	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_SINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;

	default:
		assert(0);
	}

	return nFormat;
}

D3DFormat DeviceFormats::ConvertToFloat(D3DFormat nFormat)
{
	switch (nFormat)
	{
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_UNORM:
//	case DXGI_FORMAT_R16_SINT:
//	case DXGI_FORMAT_R16_UINT:
		return DXGI_FORMAT_R16_FLOAT;
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_UNORM:
//	case DXGI_FORMAT_R16G16_SINT:
//	case DXGI_FORMAT_R16G16_UINT:
		return DXGI_FORMAT_R16G16_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
//	case DXGI_FORMAT_R16G16B16A16_SINT:
//	case DXGI_FORMAT_R16G16B16A16_UINT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;

	case DXGI_FORMAT_R32_TYPELESS:
//	case DXGI_FORMAT_R32_SINT:
//	case DXGI_FORMAT_R32_UINT:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R32G32_TYPELESS:
//	case DXGI_FORMAT_R32G32_SINT:
//	case DXGI_FORMAT_R32G32_UINT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case DXGI_FORMAT_R32G32B32_TYPELESS:
//	case DXGI_FORMAT_R32G32B32_SINT:
//	case DXGI_FORMAT_R32G32B32_UINT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
//	case DXGI_FORMAT_R32G32B32A32_SINT:
//	case DXGI_FORMAT_R32G32B32A32_UINT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;

	default:
		assert(0);
	}

	return nFormat;
}

D3DFormat DeviceFormats::ConvertToTypeless(D3DFormat nFormat)
{
	switch (nFormat)
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

#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_S8_UINT:
		return DXGI_FORMAT_R8_UINT;
#endif
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_D16_UNORM_S8_UINT:
		return DXGI_FORMAT_R16G8X8_TYPELESS;
	case DXGI_FORMAT_D24_UNORM:
		return DXGI_FORMAT_R24X8_TYPELESS;
#endif
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32G8X24_TYPELESS;

	default:
		if (!IsTypeless(nFormat))
		{
			// todo: add missing formats if they found required
			assert(0);
		}
	}

	return nFormat;
}

//---------------------------------------------------------------------------------------------------------------------
UINT DeviceFormats::GetStride(D3DFormat format)
{
	switch (format)
	{
	default:
	case DXGI_FORMAT_UNKNOWN:
		return 0;
		break;

	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 4 * sizeof(float);
		break;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 3 * sizeof(float);
		break;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
		return 4 * sizeof(short);
		break;

	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
		return 2 * sizeof(float);
		break;

	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return 2 * sizeof(float);
		break;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
		return 4 * sizeof(char);
		break;

	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
		return 2 * sizeof(short);
		break;

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
		return 1 * sizeof(float);
		break;

#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_D16_UNORM_S8_UINT:
	case DXGI_FORMAT_R16G8X8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM:
	case DXGI_FORMAT_R24X8_TYPELESS:
#endif
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return 4 * sizeof(char);
		break;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
		return 2 * sizeof(char);
		break;

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
		return 1 * sizeof(short);
		break;

	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
		return 1 * sizeof(short);
		break;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_S8_UINT:
#endif
	case DXGI_FORMAT_A8_UNORM:
		return 1 * sizeof(char);
		break;

	case DXGI_FORMAT_R1_UNORM:
		return 1 * sizeof(char);
		break;
	}

	CRY_ASSERT(0);
	return 0;
}

//---------------------------------------------------------------------------------------------------------------------
SClearValue SClearValue::GetDefaults(D3DFormat format, bool depth)
{
	SClearValue clearValue;

	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 0.0f;

	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
		format = DXGI_FORMAT_R32G32B32_FLOAT;
		break;

	case DXGI_FORMAT_R32_TYPELESS:
		format = depth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_R32_FLOAT;
		break;

	case DXGI_FORMAT_R16G16_TYPELESS:
		format = DXGI_FORMAT_R16G16_FLOAT;
		break;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;

	case DXGI_FORMAT_R32G32_TYPELESS:
		format = DXGI_FORMAT_R32G32_FLOAT;
		break;

	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		break;

#if CRY_RENDERER_VULKAN
	case DXGI_FORMAT_R16G8X8_TYPELESS:
		format = DXGI_FORMAT_D16_UNORM_S8_UINT;
		break;
	case DXGI_FORMAT_R24X8_TYPELESS:
		format = DXGI_FORMAT_D24_UNORM;
		break;
#endif
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		break;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;

	case DXGI_FORMAT_R8G8_TYPELESS:
		format = DXGI_FORMAT_R8G8_UNORM;
		break;

	case DXGI_FORMAT_R16_TYPELESS:
		format = depth ? DXGI_FORMAT_D16_UNORM : DXGI_FORMAT_R16_FLOAT;
		break;

	case DXGI_FORMAT_R8_TYPELESS:
		format = depth ? DXGI_FORMAT_R8_UINT : DXGI_FORMAT_R8_UNORM;
		break;
	}

	clearValue.Format = format;
	return clearValue;
}
