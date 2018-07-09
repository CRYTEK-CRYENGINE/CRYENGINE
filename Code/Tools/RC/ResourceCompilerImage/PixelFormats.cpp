// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PixelFormats.h"           // CPixelFormats
#include "ImageObject.h"            // ImageObject
#include <Cry3DEngine/ImageExtensionHelper.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h> // eRcExitCode_FatalError
#include <ddraw.h>                  // DDSCAPS_xxx

// Including the whole DX10-headers isn't necessary, as
// this is the only type we need for the header.
#ifndef __d3d10_h__
typedef enum D3D10_RESOURCE_DIMENSION
{	
	D3D10_RESOURCE_DIMENSION_UNKNOWN		= 0,
	D3D10_RESOURCE_DIMENSION_BUFFER			= 1,
	D3D10_RESOURCE_DIMENSION_TEXTURE1D	= 2,
	D3D10_RESOURCE_DIMENSION_TEXTURE2D	= 3,
	D3D10_RESOURCE_DIMENSION_TEXTURE3D	= 4
} D3D10_RESOURCE_DIMENSION;
#endif

// Indicates a 2D texture is a cube-map texture.
#define DDS_RESOURCE_MISC_TEXTURECUBE	0x4

PixelFormatInfo s_pixelFormatInfo[ePixelFormat_Count];

class PixelFormatsInitializer
{
public:
	PixelFormatsInitializer();
private:
	void InitPixelFormat(EPixelFormat format, const PixelFormatInfo& formatInfo);
	void InitPixelFormats();
};

static PixelFormatsInitializer s_pixelFormatsInitializer;


PixelFormatsInitializer::PixelFormatsInitializer()
{
	InitPixelFormats();
}

void PixelFormatsInitializer::InitPixelFormat(EPixelFormat format, const PixelFormatInfo& formatInfo)
{
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		assert(0);
		exit(eRcExitCode_FatalError);
	}

	if (s_pixelFormatInfo[format].szName)
	{
		// double initialization
		assert(0);
		exit(eRcExitCode_FatalError);
	}

	s_pixelFormatInfo[format] = formatInfo;
}

void PixelFormatsInitializer::InitPixelFormats()
{
	// Unsigned Formats
	// Data in an unsigned format must be positive. Unsigned formats use combinations of
	// (R)ed, (G)reen, (B)lue, (A)lpha, (L)uminance
	InitPixelFormat(ePixelFormat_A8R8G8B8,      PixelFormatInfo( 4,4,true ,"8",   1,1,1,1,false, D3DFMT_A8R8G8B8,      DXGI_FORMAT_B8G8R8A8_UNORM, eSampleType_Uint8     , "A8R8G8B8",      "32-bit ARGB pixel format with alpha, using 8 bits per channel",false,true));
	InitPixelFormat(ePixelFormat_X8R8G8B8,      PixelFormatInfo( 4,4,false,"0",   1,1,1,1,false, D3DFMT_X8R8G8B8,      DXGI_FORMAT_B8G8R8X8_UNORM, eSampleType_Uint8     , "X8R8G8B8",      "32-bit RGB pixel format, where 8 bits are reserved for each color",false,true));
	InitPixelFormat(ePixelFormat_R8G8B8,        PixelFormatInfo( 3,3,false,"0",   1,1,1,1,false, D3DFMT_R8G8B8,        DXGI_FORMAT_UNKNOWN,        eSampleType_Uint8     , "R8G8B8",        "24-bit RGB pixel format with 8 bits per channel",false,false));
	InitPixelFormat(ePixelFormat_G8R8,          PixelFormatInfo( 2,2,false,"0",   1,1,1,1,false, D3DFMT_DX10,          DXGI_FORMAT_R8G8_UNORM,     eSampleType_Uint8     , "G8R8",          "16-bit red/green, using 8 bits per channel",false,false));
	InitPixelFormat(ePixelFormat_R8,            PixelFormatInfo( 1,1,false,"0",   1,1,1,1,false, D3DFMT_DX10,          DXGI_FORMAT_R8_UNORM,       eSampleType_Uint8     , "R8",            "8-bit red only",false,false));
	InitPixelFormat(ePixelFormat_A8,            PixelFormatInfo( 1,1,true ,"8",   1,1,1,1,false, D3DFMT_A8,            DXGI_FORMAT_A8_UNORM,       eSampleType_Uint8     , "A8",            "8-bit alpha only",false,true));
	InitPixelFormat(ePixelFormat_L8,            PixelFormatInfo( 1,1,false,"0",   1,1,1,1,false, D3DFMT_L8,            DXGI_FORMAT_UNKNOWN,        eSampleType_Uint8     , "L8",            "8-bit luminance only",false,false));
	InitPixelFormat(ePixelFormat_A8L8,          PixelFormatInfo( 2,2,true ,"8",   1,1,1,1,false, D3DFMT_A8L8,          DXGI_FORMAT_UNKNOWN,        eSampleType_Uint8     , "A8L8",          "8-bit luminance and alpha",false,false));
	InitPixelFormat(ePixelFormat_A16B16G16R16,  PixelFormatInfo( 8,4,true ,"16",  1,1,1,1,false, D3DFMT_A16B16G16R16,  DXGI_FORMAT_R16G16B16A16_UNORM,eSampleType_Uint16 , "A16B16G16R16",  "64-bit ARGB pixel format with alpha, using 16 bits per channel",false,false));
	InitPixelFormat(ePixelFormat_G16R16,        PixelFormatInfo( 4,2,false,"0",   1,1,1,1,false, D3DFMT_DX10,          DXGI_FORMAT_R16G16_UNORM,   eSampleType_Uint16    , "G16R16",        "32-bit red/green, using 16 bits per channel",false,false));
	InitPixelFormat(ePixelFormat_R16,           PixelFormatInfo( 2,1,false,"0",   1,1,1,1,false, D3DFMT_DX10,          DXGI_FORMAT_R16_UNORM,      eSampleType_Uint16    , "R16",           "16-bit red only",false,false));

	// Standardized Compressed FourCC Formats (DX9+)
	// Data in these compressed formats is hardware decodable on all DX9 chips, and manageable with the DX9-API.
	InitPixelFormat(ePixelFormat_DXT1,          PixelFormatInfo( 8,3,false,"0",   4,4,4,4,false, D3DFMT_DXT1,          DXGI_FORMAT_BC1_UNORM,      eSampleType_Compressed, "DXT1",          "DXT1 compressed texture format",true,true));
	InitPixelFormat(ePixelFormat_DXT1a,         PixelFormatInfo( 8,4,true ,"1",   4,4,4,4,false, D3DFMT_DXT1,          DXGI_FORMAT_BC1_UNORM,      eSampleType_Compressed, "DXT1a",         "DXT1a compressed texture format with transparency",true,true));
	InitPixelFormat(ePixelFormat_DXT3,          PixelFormatInfo(16,4,true ,"4",   4,4,4,4,false, D3DFMT_DXT3,          DXGI_FORMAT_BC2_UNORM,      eSampleType_Compressed, "DXT3",          "DXT3 compressed texture format",true,true));
	InitPixelFormat(ePixelFormat_DXT3t,         PixelFormatInfo(16,4,true ,"4",   4,4,4,4,false, D3DFMT_DXT3,          DXGI_FORMAT_BC2_UNORM,      eSampleType_Compressed, "DXT3t",         "DXT3t compressed texture format with transparency",true,true));
	InitPixelFormat(ePixelFormat_DXT5,          PixelFormatInfo(16,4,true ,"3of8",4,4,4,4,false, D3DFMT_DXT5,          DXGI_FORMAT_BC3_UNORM,      eSampleType_Compressed, "DXT5",          "DXT5 compressed texture format",true,true));
	InitPixelFormat(ePixelFormat_DXT5t,         PixelFormatInfo(16,4,true ,"3of8",4,4,4,4,false, D3DFMT_DXT5,          DXGI_FORMAT_BC3_UNORM,      eSampleType_Compressed, "DXT5t",         "DXT5t compressed texture format with transparency",true,true));

	// Custom FourCC Formats
	// Data in these FourCC formats is custom compressed data and only decodable by certain hardware.
	InitPixelFormat(ePixelFormat_3DCp,          PixelFormatInfo( 8,1,true ,"4",   4,4,4,4,false, D3DFMT_3DCp,          DXGI_FORMAT_BC4_UNORM,      eSampleType_Compressed, "3DCp",          "ATI compressed texture format for single channel maps",true,true));
	InitPixelFormat(ePixelFormat_3DC,           PixelFormatInfo(16,2,false,"0",   4,4,4,4,false, D3DFMT_3DC,           DXGI_FORMAT_BC5_UNORM,      eSampleType_Compressed, "3DC",           "ATI compressed texture format for normalmaps",true,true));
	InitPixelFormat(ePixelFormat_CTX1,          PixelFormatInfo( 8,2,false,"0",   4,4,4,4,false, D3DFMT_CTX1,          DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "CTX1",          "CTX1 4 bpp compressed normal maps for Xbox 360",true,false));
	InitPixelFormat(ePixelFormat_PVRTC2,        PixelFormatInfo( 8,4,true ,"2", 16,16,8,4,true,  D3DFMT_PVRTC2,        DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "PVRTC2",        "POWERVR 2 bpp compressed texture format",true,false));
	InitPixelFormat(ePixelFormat_PVRTC4,        PixelFormatInfo( 8,4,true ,"2",   8,8,4,4,true,  D3DFMT_PVRTC4,        DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "PVRTC4",        "POWERVR 4 bpp compressed texture format",true,false));
	InitPixelFormat(ePixelFormat_EAC_R11,       PixelFormatInfo( 8,1,true ,"4",   4,4,4,4,true,  D3DFMT_EAC_R11,       DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "EAC_R11",       "EAC 4 bpp single channel texture format",true,false));
	InitPixelFormat(ePixelFormat_EAC_RG11,      PixelFormatInfo(16,2,false,"0",   4,4,4,4,false, D3DFMT_EAC_RG11,      DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "EAC_RG11",      "EAC 8 bpp dual channel texture format",true,false));
	InitPixelFormat(ePixelFormat_ETC2,          PixelFormatInfo( 8,3,false,"0",   4,4,4,4,false, D3DFMT_ETC2,          DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ETC2",          "ETC2 RGB 4 bpp compressed texture format",true,false));
	InitPixelFormat(ePixelFormat_ETC2a,         PixelFormatInfo(16,4,true ,"4",   4,4,4,4,false, D3DFMT_ETC2a,         DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ETC2a",         "ETC2 RGBA 8 bpp compressed texture format",true,false));
	InitPixelFormat(ePixelFormat_ASTC_LDR_L,    PixelFormatInfo(16,1,false,"0",   1,1,4,4,false, D3DFMT_ASTC_LDR_L,    DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_LDR_L",    "ASTC compressed texture format for single channel LDR maps",true,false));
	InitPixelFormat(ePixelFormat_ASTC_LDR_A,    PixelFormatInfo(16,1,true ,"8",   1,1,4,4,false, D3DFMT_ASTC_LDR_A,    DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_LDR_A",    "ASTC compressed texture format for single channel LDR alpha maps",true,false));
	InitPixelFormat(ePixelFormat_ASTC_LDR_LA,   PixelFormatInfo(16,2,true ,"8",   1,1,4,4,false, D3DFMT_ASTC_LDR_LA,   DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_LDR_LA",   "ASTC compressed texture format for dual channel LDR luminance-alpha maps",true,false));
	InitPixelFormat(ePixelFormat_ASTC_LDR_RG,   PixelFormatInfo(16,2,false,"0",   1,1,4,4,false, D3DFMT_ASTC_LDR_RG,   DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_LDR_RG",   "ASTC compressed texture format for dual channel LDR red-green maps",true,false));
	InitPixelFormat(ePixelFormat_ASTC_LDR_N,    PixelFormatInfo(16,2,false,"0",   1,1,4,4,false, D3DFMT_ASTC_LDR_N,    DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_LDR_N",    "ASTC compressed texture format for dual channel LDR normalmaps",true,false));
	InitPixelFormat(ePixelFormat_ASTC_LDR_RGB,  PixelFormatInfo(16,3,false,"0",   1,1,4,4,false, D3DFMT_ASTC_LDR_RGB,  DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_LDR_RGB",  "ASTC compressed texture format for RGB LDR maps",true,false));
	InitPixelFormat(ePixelFormat_ASTC_LDR_RGBA, PixelFormatInfo(16,4,true ,"8",   1,1,4,4,false, D3DFMT_ASTC_LDR_RGBA, DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_LDR_RGBA", "ASTC compressed texture format for RGBA LDR maps",true,false));
	InitPixelFormat(ePixelFormat_ASTC_HDR_L,    PixelFormatInfo(16,1,false,"0",   1,1,4,4,false, D3DFMT_ASTC_HDR_L,    DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_HDR_L",    "ASTC compressed texture format for single channel HDR maps",true,false));
	InitPixelFormat(ePixelFormat_ASTC_HDR_RGB,  PixelFormatInfo(16,3,false,"0",   1,1,4,4,false, D3DFMT_ASTC_HDR_RGB,  DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_HDR_RGB",  "ASTC compressed texture format for RGB HDR maps",true,false));
	InitPixelFormat(ePixelFormat_ASTC_HDR_RGBA, PixelFormatInfo(16,4,true ,"8",   1,1,4,4,false, D3DFMT_ASTC_HDR_RGBA, DXGI_FORMAT_UNKNOWN,        eSampleType_Compressed, "ASTC_HDR_RGBA", "ASTC compressed texture format for RGBA HDR maps",true,false));

	// Standardized Compressed DXGI Formats (DX10+)
	// Data in these compressed formats is hardware decodable on all DX10 chips, and manageable with the DX10-API.
	InitPixelFormat(ePixelFormat_BC1,           PixelFormatInfo( 8,3,false,"0",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC1_UNORM,      eSampleType_Compressed, "BC1",           "BC1 compressed texture format",true,true));
	InitPixelFormat(ePixelFormat_BC1a,          PixelFormatInfo( 8,4,true ,"1",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC1_UNORM,      eSampleType_Compressed, "BC1a",          "BC1a compressed texture format with transparency",true,true));
	InitPixelFormat(ePixelFormat_BC2,           PixelFormatInfo(16,4,true ,"4",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC2_UNORM,      eSampleType_Compressed, "BC2",           "BC2 compressed texture format",true,true));
	InitPixelFormat(ePixelFormat_BC2t,          PixelFormatInfo(16,4,true ,"4",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC2_UNORM,      eSampleType_Compressed, "BC2t",          "BC2t compressed texture format with transparency",true,true));
	InitPixelFormat(ePixelFormat_BC3,           PixelFormatInfo(16,4,true ,"3of8",4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC3_UNORM,      eSampleType_Compressed, "BC3",           "BC3 compressed texture format",true,true));
	InitPixelFormat(ePixelFormat_BC3t,          PixelFormatInfo(16,4,true ,"3of8",4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC3_UNORM,      eSampleType_Compressed, "BC3t",          "BC3t compressed texture format with transparency",true,true));
	InitPixelFormat(ePixelFormat_BC4,           PixelFormatInfo( 8,1,false,"0",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC4_UNORM,      eSampleType_Compressed, "BC4",           "BC4 compressed texture format for single channel maps",true,true));
	InitPixelFormat(ePixelFormat_BC4s,          PixelFormatInfo( 8,1,false,"0",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC4_SNORM,      eSampleType_Compressed, "BC4s",          "BC4 compressed texture format for signed single channel maps",true,true));
	InitPixelFormat(ePixelFormat_BC5,           PixelFormatInfo(16,2,false,"0",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC5_UNORM,      eSampleType_Compressed, "BC5",           "BC5 compressed texture format for two channel maps or normalmaps",true,true));
	InitPixelFormat(ePixelFormat_BC5s,          PixelFormatInfo(16,2,false,"0",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC5_SNORM,      eSampleType_Compressed, "BC5s",          "BC5 compressed texture format for signed two channel maps or normalmaps",true,true));
	InitPixelFormat(ePixelFormat_BC6UH,         PixelFormatInfo(16,3,false,"0",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC6H_UF16,      eSampleType_Compressed, "BC6UH",         "BC6 compressed texture format, unsigned half",true,true));
	InitPixelFormat(ePixelFormat_BC7,           PixelFormatInfo(16,4,true ,"8",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC7_UNORM,      eSampleType_Compressed, "BC7",           "BC7 compressed texture format",true,true));
	InitPixelFormat(ePixelFormat_BC7t,          PixelFormatInfo(16,4,true ,"8",   4,4,4,4,false, D3DFMT_DX10,          DXGI_FORMAT_BC7_UNORM,      eSampleType_Compressed, "BC7t",          "BC7t compressed texture format with transparency",true,true));

	// Float formats
	// Data in a Float format is floating point data.
	InitPixelFormat(ePixelFormat_E5B9G9R9,      PixelFormatInfo( 4,3,false,"0",   1,1,1,1,false, D3DFMT_DX10,          DXGI_FORMAT_R9G9B9E5_SHAREDEXP,eSampleType_Compressed,"R9G9B9E5",   "32-bit RGB pixel format with shared exponent",false,true));
	InitPixelFormat(ePixelFormat_A32B32G32R32F, PixelFormatInfo(16,4,true ,"23",  1,1,1,1,false, D3DFMT_A32B32G32R32F, DXGI_FORMAT_R32G32B32A32_FLOAT,eSampleType_Float,"A32B32G32R32F","four float channels",false,false));
	InitPixelFormat(ePixelFormat_G32R32F,       PixelFormatInfo( 8,2,false,"0",   1,1,1,1,false, D3DFMT_G32R32F,       DXGI_FORMAT_R32G32_FLOAT,   eSampleType_Float     , "G32R32F",      "two float channels",false,false));
	InitPixelFormat(ePixelFormat_R32F,          PixelFormatInfo( 4,1,false,"0",   1,1,1,1,false, D3DFMT_R32F,          DXGI_FORMAT_R32_FLOAT,      eSampleType_Float     , "R32F",         "one float channel",false,false));
	InitPixelFormat(ePixelFormat_A16B16G16R16F, PixelFormatInfo( 8,4,true ,"10",  1,1,1,1,false, D3DFMT_A16B16G16R16F, DXGI_FORMAT_R16G16B16A16_FLOAT,eSampleType_Half ,"A16B16G16R16F","four half channels",false,false));
	InitPixelFormat(ePixelFormat_G16R16F,       PixelFormatInfo( 4,2,false,"0",   1,1,1,1,false, D3DFMT_G16R16F,       DXGI_FORMAT_R16G16_FLOAT,   eSampleType_Half      , "G16R16F",      "two half channel",false,false));
	InitPixelFormat(ePixelFormat_R16F,          PixelFormatInfo( 2,1,false,"0",   1,1,1,1,false, D3DFMT_R16F,          DXGI_FORMAT_R16_FLOAT,      eSampleType_Half      , "R16F",         "one half channel",false,false));

	for (int i = 0; i < ePixelFormat_Count; ++i)
	{
		if (s_pixelFormatInfo[i].szName == 0)
		{
			// Uninitialized entry. Should never happen. But, if it happened: make sure that entries from
			// the EPixelFormat enum and InitPixelFormat() calls match.
			printf("Internal error in %s. Not all pixel formats have an implementation. Contact an RC programmer ASAP.\n", __FUNCTION__);
			assert(0);
			exit(eRcExitCode_FatalError);
		}
	}
}

EPixelFormat CPixelFormats::FindPixelFormatByName(const char* name)
{
	for (int i = 0; i < ePixelFormat_Count; ++i)
	{
		if (stricmp(s_pixelFormatInfo[i].szName, name) == 0)
		{
			return (EPixelFormat)i;
		}
	}

	return (EPixelFormat)-1;
}

EPixelFormat CPixelFormats::FindFinalTextureFormat(const EPixelFormat format, const bool bAlphaChannelUsed)
{
	struct FormatMapping
	{
		EPixelFormat requestedPixelFormat;
		bool bHasAlfaData;
		EPixelFormat finalPixelFormat;
	};

	static const FormatMapping formatMapping[] =
	{
		{ ePixelFormat_X8R8G8B8, true,  ePixelFormat_A8R8G8B8 },
		{ ePixelFormat_R8G8B8,   true,  ePixelFormat_A8R8G8B8 },
		{ ePixelFormat_L8,       true,  ePixelFormat_A8L8     },
		{ ePixelFormat_DXT1,     true,  ePixelFormat_DXT5     },
		{ ePixelFormat_BC1,      true,  ePixelFormat_BC3      },

		{ ePixelFormat_A8R8G8B8, false, ePixelFormat_X8R8G8B8 },
		{ ePixelFormat_A8L8,     false, ePixelFormat_L8       },
		{ ePixelFormat_DXT1a,    false, ePixelFormat_DXT1     },
		{ ePixelFormat_DXT3,     false, ePixelFormat_DXT1     },
		{ ePixelFormat_DXT3t,    false, ePixelFormat_DXT1     },
		{ ePixelFormat_DXT5,     false, ePixelFormat_DXT1     },
		{ ePixelFormat_DXT5t,    false, ePixelFormat_DXT1     },
		{ ePixelFormat_BC1a,     false, ePixelFormat_BC1      },
		{ ePixelFormat_BC2,      false, ePixelFormat_BC1      },
		{ ePixelFormat_BC2t,     false, ePixelFormat_BC1      },
		{ ePixelFormat_BC3,      false, ePixelFormat_BC1      },
		{ ePixelFormat_BC3t,     false, ePixelFormat_BC1      }
	};

	static const size_t formatMappingCount = sizeof(formatMapping) / sizeof(formatMapping[0]);

	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		return (EPixelFormat)-1;
	}

	for (size_t i = 0; i < formatMappingCount; ++i)
	{
		const FormatMapping& f = formatMapping[i];
		if ((f.requestedPixelFormat == format) && (f.bHasAlfaData == bAlphaChannelUsed))
		{
			return f.finalPixelFormat;
		}
	}

	// no better solution, stick to the input
	return format;
}


bool CPixelFormats::GetPixelFormatInfo(EPixelFormat format, ESampleType* pSampleType, int* pChannelCount, bool* pHasAlpha)
{
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		assert(0);
		RCLogError("%s: Bad format %d", __FUNCTION__, (int)format);
		return false;
	}

	const PixelFormatInfo* fmt = &s_pixelFormatInfo[format];

	if (pSampleType) 
	{
		*pSampleType = fmt->eSampleType;
	}

	if (pChannelCount) 
	{
		*pChannelCount = fmt->nChannels;
	}

	if (pHasAlpha) 
	{
		*pHasAlpha = fmt->bHasAlpha;
	}

	return !fmt->bCompressed;
}

const PixelFormatInfo* CPixelFormats::GetPixelFormatInfo(EPixelFormat format)
{
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		assert(0);
		RCLogError("%s: Bad format %d", __FUNCTION__, (int)format);
		return 0;
	}

	return &s_pixelFormatInfo[format];
}

int CPixelFormats::GetBitsPerPixel(EPixelFormat format)
{
	const PixelFormatInfo &info = s_pixelFormatInfo[format];

	return (info.bytesPerBlock / (info.blockWidth * info.blockHeight)) << 3;
}

bool CPixelFormats::IsPixelFormatUncompressed(EPixelFormat format)
{
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		assert(0);
		RCLogError("%s: Bad format %d", __FUNCTION__, (int)format);
		return false;
	}

	return !s_pixelFormatInfo[format].bCompressed;
}

bool CPixelFormats::IsPixelFormatWithoutAlpha(EPixelFormat format)
{
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		assert(0);
		RCLogError("%s: Bad format %d", __FUNCTION__, (int)format);
		return false;
	}

	return !s_pixelFormatInfo[format].bHasAlpha;
}

bool CPixelFormats::IsPixelFormatAnyRGB(EPixelFormat format)
{
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		assert(0);
		RCLogError("%s: Bad format %d", __FUNCTION__, (int)format);
		return false;
	}

	return !s_pixelFormatInfo[format].bCompressed && (s_pixelFormatInfo[format].nChannels >= 3);
}

bool CPixelFormats::IsPixelFormatAnyRG(EPixelFormat format)
{
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		assert(0);
		RCLogError("%s: Bad format %d", __FUNCTION__, (int)format);
		return false;
	}

	return !s_pixelFormatInfo[format].bCompressed && (s_pixelFormatInfo[format].nChannels >= 2) && (s_pixelFormatInfo[format].d3d9Format != D3DFMT_A8L8);
}

bool CPixelFormats::IsPixelFormatForExtendedDDSOnly(EPixelFormat format)
{
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		assert(0);
		RCLogError("%s: Bad format %d", __FUNCTION__, (int)format);
		return false;
	}

	return s_pixelFormatInfo[format].d3d9Format == D3DFMT_DX10;
}


uint32 CPixelFormats::ComputeMaxMipCount(EPixelFormat format, uint32 width, uint32 height, bool bCubemap)
{
	const PixelFormatInfo* const pFormatInfo = GetPixelFormatInfo(format);
	assert(pFormatInfo);

	uint32 tmpWidth = width;
	uint32 tmpHeight = height;

	if (bCubemap)
	{
		if (tmpWidth * 1 == tmpHeight * 6)
		{
			// cube strip
			tmpWidth /= 6;
		}
		else if (tmpWidth * 2 == tmpHeight * 4)
		{
			// longitude/latitude probe
			tmpWidth /= 4;
			tmpHeight /= 2;
		}
		else if (tmpWidth * 3 == tmpHeight * 4)
		{
			// vertical cross
			tmpWidth /= 4;
			tmpHeight /= 3;
		}
		else if (tmpWidth * 4 == tmpHeight * 3)
		{
			// horizontal cross
			tmpWidth /= 3;
			tmpHeight /= 4;
		}
	}

	uint32 mipCountW = 0;
	while (tmpWidth >= pFormatInfo->minWidth)
	{
		++mipCountW;
		tmpWidth >>= 1;
	}

	uint32 mipCountH = 0;
	while (tmpHeight >= pFormatInfo->minHeight)
	{
		++mipCountH;
		tmpHeight >>= 1;
	}

	const uint32 mipCount = (pFormatInfo->bCompressed)
		? Util::getMin(mipCountW, mipCountH)
		: Util::getMax(mipCountW, mipCountH);

	assert(mipCount > 0);
	return mipCount;
}

bool CPixelFormats::BuildSurfaceHeader(const ImageObject* pImage, uint32 maxMipCount, CImageExtensionHelper::DDS_HEADER& header, bool bForceDX10)
{
	assert(pImage);

	if (maxMipCount <= 0)
	{
		assert(0);
		RCLogError("%s: maxMipCount is %u", __FUNCTION__, (unsigned)maxMipCount);
		return false;
	}

	uint32 dwWidth, dwMips, dwHeight;
	pImage->GetExtent(dwWidth, dwHeight, dwMips);

	if (dwMips <= 0)
	{
		assert(0);
		RCLogError("%s: dwMips is %u", __FUNCTION__, (unsigned)dwMips);
		return false;
	}

	if (dwMips > maxMipCount)
	{
		dwMips = maxMipCount;
	}

	const EPixelFormat format = pImage->GetPixelFormat();
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		RCLogError("%s: Bad format %d", __FUNCTION__, (int)format);
		return false;
	}

	const PixelFormatInfo* const pPixelFormatInfo = CPixelFormats::GetPixelFormatInfo(format);

	D3DFORMAT d3dformat = pPixelFormatInfo->d3d9Format;

	// check if we hit a format which can't be stored into a DDS-file at all
	if (d3dformat == D3DFMT_UNKNOWN)
	{
		RCLogError("%s: Format can not be stored in a DX9 DDS-file %d", __FUNCTION__, (int)format);
		return false;
	}

	// force a DX10 header, for example when attached alpha-images are appended
	if (bForceDX10)
	{
		d3dformat = D3DFMT_DX10;
	}

	memset(&header, 0, sizeof(CImageExtensionHelper::DDS_HEADER));

	header.dwSize = sizeof(CImageExtensionHelper::DDS_HEADER);
	header.dwHeaderFlags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT;
	header.dwWidth = dwWidth;
	header.dwHeight = dwHeight;
	header.ddspf.dwSize = sizeof(CImageExtensionHelper::DDS_PIXELFORMAT);

	if (pImage->HasImageFlags(CImageExtensionHelper::EIF_Cubemap))
	{
		assert(pImage->HasCubemapCompatibleSizes());
		assert(header.dwWidth == header.dwHeight * 6);

		header.dwSurfaceFlags |= DDS_SURFACE_FLAGS_CUBEMAP;
		header.dwCubemapFlags |= DDS_CUBEMAP_ALLFACES;
		header.dwWidth /= 6;
	}

	switch (d3dformat)
	{
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		header.ddspf.dwFlags = DDPF_RGB;
		header.ddspf.dwRGBBitCount = CPixelFormats::GetBitsPerPixel(format);
		header.ddspf.dwABitMask = 0xff000000;
		header.ddspf.dwRBitMask = 0x00ff0000;
		header.ddspf.dwGBitMask = 0x0000ff00;
		header.ddspf.dwBBitMask = 0x000000ff;
		break;
	case D3DFMT_A8L8:
	case D3DFMT_L8:
		header.ddspf.dwFlags = DDPF_LUMINANCE;
		header.ddspf.dwRGBBitCount = 8;
		header.ddspf.dwABitMask = 0x000000ff;
		header.ddspf.dwRBitMask = 0x000000ff;
		header.ddspf.dwGBitMask = 0x000000ff;
		header.ddspf.dwBBitMask = 0x000000ff;
		break;
	case D3DFMT_A8:
		header.ddspf.dwFlags = DDPF_ALPHA;
		header.ddspf.dwRGBBitCount = 8;
		header.ddspf.dwABitMask = 0x000000ff;
		header.ddspf.dwRBitMask = 0x00000000;
		header.ddspf.dwGBitMask = 0x00000000;
		header.ddspf.dwBBitMask = 0x00000000;
		break;
	default:
		if (!pPixelFormatInfo->bCompressed &&
		     d3dformat != D3DFMT_A32B32G32R32F &&
		     d3dformat != D3DFMT_G32R32F &&
		     d3dformat != D3DFMT_R32F &&
		     d3dformat != D3DFMT_A16B16G16R16F &&
		     d3dformat != D3DFMT_G16R16F &&
		     d3dformat != D3DFMT_R16F &&
		     d3dformat != D3DFMT_A16B16G16R16 &&
		     d3dformat != D3DFMT_DX10)
		{
			RCLogError("%s: Unexpected format %d", __FUNCTION__, (int)format);
			return false;
		}

		header.ddspf.dwFlags = DDPF_FOURCC;
		header.ddspf.dwFourCC = d3dformat;
		break;
	}

	if (!pPixelFormatInfo->bCompressed &&
	     d3dformat != D3DFMT_A32B32G32R32F &&
	     d3dformat != D3DFMT_A16B16G16R16F &&
	     d3dformat != D3DFMT_A16B16G16R16 &&
	     pPixelFormatInfo->bHasAlpha)
	{
		header.ddspf.dwFlags |= DDPF_ALPHAPIXELS;
	}

	header.dwSurfaceFlags |= DDSCAPS_TEXTURE;

	if (dwMips > 1)
	{
		header.dwHeaderFlags |= DDSD_MIPMAPCOUNT;
		header.dwMipMapCount = dwMips;
		header.dwSurfaceFlags |= DDSCAPS_MIPMAP | DDSCAPS_COMPLEX;
	}

	// non standardized way to expose some features in the header (same information is in attached chunk but then
	// streaming would need to find this spot in the file)
	// if this is causing problems we need to change it 
	header.dwTextureStage = MAKEFOURCC('F','Y','R','C');
	header.dwReserved1 = pImage->GetImageFlags();
	header.bNumPersistentMips = (BYTE)pImage->GetNumPersistentMips();
	header.bCompressedBlockWidth = (BYTE)pImage->GetCompressedBlockWidth();
	header.bCompressedBlockHeight = (BYTE)pImage->GetCompressedBlockHeight();

	// setting up min and max colors
	Vec4 colMinARGB, colMaxARGB;
	pImage->GetColorRange(colMinARGB, colMaxARGB);
	header.cMinColor = *(ColorF*)&colMinARGB;
	header.cMaxColor = *(ColorF*)&colMaxARGB;

	// set avg brightness
	header.fAvgBrightness = pImage->GetAverageBrightness();

	// self-check
	assert(header.IsValid());

	return true;
}

bool CPixelFormats::BuildSurfaceExtendedHeader(const ImageObject* pImage, uint32 sliceCount, CImageExtensionHelper::DDS_HEADER_DXT10& exthead)
{
	assert(pImage);

	const EPixelFormat format = pImage->GetPixelFormat();
	if ((format < 0) || (format >= ePixelFormat_Count))
	{
		RCLogError("%s: Bad format %d", __FUNCTION__, (int)format);
		return false;
	}

	const PixelFormatInfo* const pPixelFormatInfo = CPixelFormats::GetPixelFormatInfo(format);

	DXGI_FORMAT dxgiformat = pPixelFormatInfo->d3d10Format;

	// check if we hit a format which can't be stored into a DX10 DDS-file (fe. L8)
	if (dxgiformat == DXGI_FORMAT_UNKNOWN)
	{
		RCLogError("%s: Format can not be stored in a DX10 DDS-file %d", __FUNCTION__, (int)format);
		return false;
	}

	if (pImage->HasImageFlags(CImageExtensionHelper::EIF_SRGBRead))
	{
		switch (dxgiformat)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM: dxgiformat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
		case DXGI_FORMAT_B8G8R8A8_UNORM: dxgiformat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; break;
		case DXGI_FORMAT_B8G8R8X8_UNORM: dxgiformat = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB; break;
		case DXGI_FORMAT_BC1_UNORM: dxgiformat = DXGI_FORMAT_BC1_UNORM_SRGB; break;
		case DXGI_FORMAT_BC2_UNORM: dxgiformat = DXGI_FORMAT_BC2_UNORM_SRGB; break;
		case DXGI_FORMAT_BC3_UNORM: dxgiformat = DXGI_FORMAT_BC3_UNORM_SRGB; break;
		case DXGI_FORMAT_BC7_UNORM: dxgiformat = DXGI_FORMAT_BC7_UNORM_SRGB; break;
		default: break;
		}
	}
	else 
	{
		switch (dxgiformat)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: dxgiformat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: dxgiformat = DXGI_FORMAT_B8G8R8A8_UNORM; break;
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: dxgiformat = DXGI_FORMAT_B8G8R8X8_UNORM; break;
		case DXGI_FORMAT_BC1_UNORM_SRGB: dxgiformat = DXGI_FORMAT_BC1_UNORM; break;
		case DXGI_FORMAT_BC2_UNORM_SRGB: dxgiformat = DXGI_FORMAT_BC2_UNORM; break;
		case DXGI_FORMAT_BC3_UNORM_SRGB: dxgiformat = DXGI_FORMAT_BC3_UNORM; break;
		case DXGI_FORMAT_BC7_UNORM_SRGB: dxgiformat = DXGI_FORMAT_BC7_UNORM; break;
		default: break;
		}
	}

	memset(&exthead, 0, sizeof(exthead));

	exthead.dxgiFormat = dxgiformat;

	if (pImage->HasImageFlags(CImageExtensionHelper::EIF_Volumetexture))
	{
		exthead.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE3D;
		exthead.miscFlag = 0;
		exthead.arraySize = 1;
	}
	else if (pImage->HasImageFlags(CImageExtensionHelper::EIF_Cubemap))
	{
		exthead.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
		exthead.miscFlag = DDS_RESOURCE_MISC_TEXTURECUBE;
		exthead.arraySize = 6;
	}
	else 
	{
		exthead.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
		exthead.miscFlag = 0;
		exthead.arraySize = sliceCount;
	}

	return true;
}

bool CPixelFormats::ParseSurfaceHeader(ImageObject* pImage, uint32& maxMipCount, const CImageExtensionHelper::DDS_HEADER& header, bool bForceDX10)
{
	EPixelFormat eFormat = ePixelFormat_Count;
	uint32 dwWidth, dwMips, dwHeight;
	pImage->GetExtent(dwWidth, dwHeight, dwMips);

	dwWidth = header.dwWidth;
	dwHeight = header.dwHeight;
	dwMips = 1;

	if (header.dwHeaderFlags & DDSD_MIPMAPCOUNT)
		dwMips = header.dwMipMapCount;

	ImageObject::ECubemap eCube = ImageObject::eCubemap_No;
	if ((header.dwSurfaceFlags & DDS_SURFACE_FLAGS_CUBEMAP) && (header.dwCubemapFlags & DDS_CUBEMAP_ALLFACES))
	{
		if (eCube == ImageObject::eCubemap_No)
		{
			dwWidth *= 6;
		}

		eCube = ImageObject::eCubemap_Yes;
	}

	const Vec4 colMinARGB = *(Vec4f*)&header.cMinColor;
	const Vec4 colMaxARGB = *(Vec4f*)&header.cMaxColor;

	{
		// DX10
		if (header.ddspf.dwFourCC == DDSFormats::DDSPF_DX10.dwFourCC)
			eFormat = ePixelFormat_A32B32G32R32F;

		else if (header.ddspf.dwFourCC == DDSFormats::DDSPF_DXT1.dwFourCC)
			eFormat = ePixelFormat_BC1;
		else if (header.ddspf.dwFourCC == DDSFormats::DDSPF_DXT3.dwFourCC)
			eFormat = ePixelFormat_BC2;
		else if (header.ddspf.dwFourCC == DDSFormats::DDSPF_DXT5.dwFourCC)
			eFormat = ePixelFormat_BC3;
		else if (header.ddspf.dwFourCC == DDSFormats::DDSPF_3DCP.dwFourCC)
			eFormat = ePixelFormat_BC4;
		else if (header.ddspf.dwFourCC == DDSFormats::DDSPF_3DC.dwFourCC)
			eFormat = ePixelFormat_BC5;
		else if (header.ddspf.dwFourCC == DDSFormats::DDSPF_CTX1.dwFourCC)
			eFormat = ePixelFormat_CTX1;
		else if( header.ddspf.dwFourCC == DDSFormats::DDSPF_R32F.dwFourCC)
			eFormat = ePixelFormat_R32F;
		else if( header.ddspf.dwFourCC == DDSFormats::DDSPF_G32R32F.dwFourCC)
			eFormat = ePixelFormat_G32R32F;
		else if( header.ddspf.dwFourCC == DDSFormats::DDSPF_A32B32G32R32F.dwFourCC)
			eFormat = ePixelFormat_A32B32G32R32F;
		else if( header.ddspf.dwFourCC == DDSFormats::DDSPF_R16F.dwFourCC)
			eFormat = ePixelFormat_R16F;
		else if( header.ddspf.dwFourCC == DDSFormats::DDSPF_G16R16F.dwFourCC)
			eFormat = ePixelFormat_G16R16F;
		else if( header.ddspf.dwFourCC == DDSFormats::DDSPF_A16B16G16R16F.dwFourCC)
			eFormat = ePixelFormat_A16B16G16R16F;
		else if( header.ddspf.dwFourCC == DDSFormats::DDSPF_A16B16G16R16.dwFourCC)
			eFormat = ePixelFormat_A16B16G16R16;
		else if (header.ddspf.dwFlags == DDS_RGBA && header.ddspf.dwRGBBitCount == 32 && header.ddspf.dwABitMask == 0xff000000)
			eFormat = ePixelFormat_A8R8G8B8;
//		else if (header.ddspf.dwFlags == DDS_RGBA && header.ddspf.dwRGBBitCount == 16)
//			eFormat = ePixelFormat_A4R4G4B4;
		else if (header.ddspf.dwFlags == DDS_RGB  && header.ddspf.dwRGBBitCount == 24)
			eFormat = ePixelFormat_R8G8B8;
		else if (header.ddspf.dwFlags == DDS_RGB  && header.ddspf.dwRGBBitCount == 32)
			eFormat = ePixelFormat_X8R8G8B8;
		else if (header.ddspf.dwFlags == DDS_LUMINANCEA && header.ddspf.dwRGBBitCount == 8)
			eFormat = ePixelFormat_A8L8;
		else if (header.ddspf.dwFlags == DDS_LUMINANCE  && header.ddspf.dwRGBBitCount == 8)
			eFormat = ePixelFormat_L8;
		else if ((header.ddspf.dwFlags == DDS_A || header.ddspf.dwFlags == DDS_A_ONLY || header.ddspf.dwFlags == (DDS_A | DDS_A_ONLY)) && header.ddspf.dwRGBBitCount == 8)
			eFormat = ePixelFormat_A8;
		else
			return false;
	}

	// Force the pixel format defaults if the provided compressed block sizes are not valid.
	const uint32 compressedBlockWidth = header.bCompressedBlockWidth > 0 ? header.bCompressedBlockWidth : -1;
	const uint32 compressedBlockHeight = header.bCompressedBlockHeight > 0 ? header.bCompressedBlockHeight : -1;

	pImage->ResetImage(dwWidth, dwHeight, dwMips, eFormat, eCube, compressedBlockWidth, compressedBlockHeight);

	pImage->SetImageFlags(header.dwReserved1);
	pImage->SetNumPersistentMips(header.bNumPersistentMips);
	pImage->SetColorRange(colMinARGB, colMaxARGB);
	pImage->SetAverageBrightness(header.fAvgBrightness);

	maxMipCount = dwMips;

	return true;
}

bool CPixelFormats::ParseSurfaceExtendedHeader(ImageObject* pImage, uint32& sliceCount, const CImageExtensionHelper::DDS_HEADER_DXT10& exthead)
{
	EPixelFormat eFormat = pImage->GetPixelFormat();
	uint32 dwWidth, dwMips, dwHeight;
	pImage->GetExtent(dwWidth, dwHeight, dwMips);

	uint32 dwReserved1 = pImage->GetImageFlags();
	uint32 bNumPersistentMips = pImage->GetNumPersistentMips();
	float fAvgBrightness = pImage->GetAverageBrightness();

	const uint32 compressedBlockWidth = pImage->GetCompressedBlockWidth();
	const uint32 compressedBlockHeight = pImage->GetCompressedBlockHeight();

	ImageObject::ECubemap eCube = pImage->GetCubemap();
	if (exthead.miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE)
	{
		if (eCube == ImageObject::eCubemap_No)
		{
			dwWidth *= 6;
		}

		eCube = ImageObject::eCubemap_Yes;
		dwReserved1 |= CImageExtensionHelper::EIF_Cubemap;
	}

	uint32 dwSlices = exthead.arraySize;
	if (exthead.resourceDimension == D3D10_RESOURCE_DIMENSION_TEXTURE3D)
	{
		dwReserved1 |= CImageExtensionHelper::EIF_Volumetexture;
	}

	Vec4 colMinARGB, colMaxARGB;
	pImage->GetColorRange(colMinARGB, colMaxARGB);

	{
		if      (exthead.dxgiFormat == DXGI_FORMAT_BC1_UNORM)
			eFormat = ePixelFormat_BC1;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC1_UNORM_SRGB)
			eFormat = ePixelFormat_BC1;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC2_UNORM)
			eFormat = ePixelFormat_BC2;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC2_UNORM_SRGB)
			eFormat = ePixelFormat_BC2;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC3_UNORM)
			eFormat = ePixelFormat_BC3;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC3_UNORM_SRGB)
			eFormat = ePixelFormat_BC3;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC4_UNORM)
			eFormat = ePixelFormat_BC4;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC4_SNORM)
			eFormat = ePixelFormat_BC4s;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC5_UNORM)
			eFormat = ePixelFormat_BC5;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC5_SNORM)
			eFormat = ePixelFormat_BC5s;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC6H_UF16)
			eFormat = ePixelFormat_BC6UH;
//		else if (exthead.dxgiFormat == DXGI_FORMAT_BC6H_SF16)
//			eFormat = ePixelFormat_BC6SH;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC7_UNORM)
			eFormat = ePixelFormat_BC7;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC7_UNORM_SRGB)
			eFormat = ePixelFormat_BC7;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R9G9B9E5_SHAREDEXP)
			eFormat = ePixelFormat_E5B9G9R9;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R32G32B32A32_FLOAT)
			eFormat = ePixelFormat_A32B32G32R32F;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R32G32_FLOAT)
			eFormat = ePixelFormat_G32R32F;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R32_FLOAT)
			eFormat = ePixelFormat_R32F;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
			eFormat = ePixelFormat_A16B16G16R16F;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R16G16_FLOAT)
			eFormat = ePixelFormat_G16R16F;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R16_FLOAT)
			eFormat = ePixelFormat_R16F;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R16G16B16A16_UNORM)
			eFormat = ePixelFormat_A16B16G16R16;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R16G16_UNORM)
			eFormat = ePixelFormat_G16R16;
		else if (exthead.dxgiFormat == DXGI_FORMAT_R16_UNORM)
			eFormat = ePixelFormat_R16;
		else if (exthead.dxgiFormat == DXGI_FORMAT_B8G8R8X8_UNORM)
			eFormat = ePixelFormat_X8R8G8B8;
		else if (exthead.dxgiFormat == DXGI_FORMAT_B8G8R8A8_UNORM)
			eFormat = ePixelFormat_A8R8G8B8;
		else if (exthead.dxgiFormat == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB)
			eFormat = ePixelFormat_X8R8G8B8;
		else if (exthead.dxgiFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
			eFormat = ePixelFormat_A8R8G8B8;
//		else if (exthead.dxgiFormat == DXGI_FORMAT_R8G8B8A8_UNORM)
//			eFormat = ?;
//		else if (exthead.dxgiFormat == DXGI_FORMAT_R8G8B8A8_SNORM)
//			eFormat = ?;
		else
			return false;

		// auto-correct missing SRGB-flags
		if      (exthead.dxgiFormat == DXGI_FORMAT_BC1_UNORM_SRGB)
			dwReserved1 |= CImageExtensionHelper::EIF_SRGBRead;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC2_UNORM_SRGB)
			dwReserved1 |= CImageExtensionHelper::EIF_SRGBRead;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC3_UNORM_SRGB)
			dwReserved1 |= CImageExtensionHelper::EIF_SRGBRead;
		else if (exthead.dxgiFormat == DXGI_FORMAT_BC7_UNORM_SRGB)
			dwReserved1 |= CImageExtensionHelper::EIF_SRGBRead;
		else if (exthead.dxgiFormat == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB)
			dwReserved1 |= CImageExtensionHelper::EIF_SRGBRead;
		else if (exthead.dxgiFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
			dwReserved1 |= CImageExtensionHelper::EIF_SRGBRead;
	}

	pImage->ResetImage(dwWidth, dwHeight, dwMips, eFormat, eCube, compressedBlockWidth, compressedBlockHeight);

	pImage->SetImageFlags(dwReserved1);
	pImage->SetNumPersistentMips(bNumPersistentMips);
	pImage->SetColorRange(colMinARGB, colMaxARGB);
	pImage->SetAverageBrightness(fAvgBrightness);

	sliceCount = dwSlices;

	return true;
}
