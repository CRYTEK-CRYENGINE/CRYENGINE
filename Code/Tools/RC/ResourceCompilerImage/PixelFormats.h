// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PIXELFORMATS_H__
#define __PIXELFORMATS_H__

#include <d3d9.h>                    // WORD (used in d3d9types)
#include <d3d9types.h>               // D3DFORMAT
#include <dxgiformat.h>              // DX10+ formats

#include <CryCore/Platform/platform.h>                // uint32
#include <CryRenderer/ITexture.h>                // ETEX_Format
#include <Cry3DEngine/ImageExtensionHelper.h>


enum EPixelFormat
{
	// NOTES: 
	// The ARGB format has a memory layout of [B][G][R][A], on modern DX10+ architectures
	// this format became disfavored over ABGR with a memory layout of [R][G][B][A].
	ePixelFormat_A8R8G8B8 = 0,
	ePixelFormat_X8R8G8B8,
	ePixelFormat_R8G8B8,
	ePixelFormat_A8,
	ePixelFormat_L8,
	ePixelFormat_A8L8,

	ePixelFormat_DXT1,
	ePixelFormat_DXT1a,
	ePixelFormat_DXT3,
	ePixelFormat_DXT3t,
	ePixelFormat_DXT5,
	ePixelFormat_DXT5t,
	ePixelFormat_3DC,
	ePixelFormat_3DCp,
	ePixelFormat_CTX1,
	ePixelFormat_PVRTC2,
	ePixelFormat_PVRTC4,
	ePixelFormat_EAC_R11,
	ePixelFormat_EAC_RG11,
	ePixelFormat_ETC2,
	ePixelFormat_ETC2a,
	ePixelFormat_ASTC_LDR_L,
	ePixelFormat_ASTC_LDR_A,
	ePixelFormat_ASTC_LDR_LA,
	ePixelFormat_ASTC_LDR_RG,
	ePixelFormat_ASTC_LDR_N,
	ePixelFormat_ASTC_LDR_RGB,
	ePixelFormat_ASTC_LDR_RGBA,
	ePixelFormat_ASTC_HDR_L,
	ePixelFormat_ASTC_HDR_RGB,
	ePixelFormat_ASTC_HDR_RGBA,

	ePixelFormat_BC1,
	ePixelFormat_BC1a,
	ePixelFormat_BC2,
	ePixelFormat_BC2t,
	ePixelFormat_BC3,
	ePixelFormat_BC3t,
	ePixelFormat_BC4,
	ePixelFormat_BC4s,
	ePixelFormat_BC5,
	ePixelFormat_BC5s,
	ePixelFormat_BC6UH,
	ePixelFormat_BC7,
	ePixelFormat_BC7t,

	ePixelFormat_E5B9G9R9,

	ePixelFormat_A32B32G32R32F,
	ePixelFormat_G32R32F,
	ePixelFormat_R32F,
	ePixelFormat_A16B16G16R16F,
	ePixelFormat_G16R16F,
	ePixelFormat_R16F,

	ePixelFormat_A16B16G16R16,
	ePixelFormat_G16R16,
	ePixelFormat_R16,

//	ePixelFormat_A8B8G8R8,
//	ePixelFormat_X8B8G8R8,
	ePixelFormat_G8R8,
	ePixelFormat_R8,

	ePixelFormat_Count
};

enum ESampleType
{
	eSampleType_Uint8,
	eSampleType_Uint16,
	eSampleType_Half,
	eSampleType_Float,
	eSampleType_Compressed,
};

struct PixelFormatInfo
{
	int         bytesPerBlock;
	int         nChannels;
	bool        bHasAlpha;
	const char* szAlpha;
	int         minWidth;
	int         minHeight;
	int         blockWidth;
	int         blockHeight;
	bool        bSquarePow2;
	D3DFORMAT   d3d9Format;
	DXGI_FORMAT d3d10Format;
	ESampleType eSampleType;
	const char* szName;
	const char* szDescription;
	bool        bCompressed;
	bool        bSelectable;     // shows up in the list of usable destination pixel formats in the dialog window

	PixelFormatInfo()
		: bytesPerBlock(-1)
		, nChannels(-1)
		, bHasAlpha(false)
		, szAlpha(0)
		, minWidth(-1)
		, minHeight(-1)
		, blockWidth(-1)
		, blockHeight(-1)
		, bSquarePow2(false)
		, d3d9Format(D3DFMT_UNKNOWN)
		, d3d10Format(DXGI_FORMAT_UNKNOWN)
		, eSampleType(eSampleType_Uint8)
		, szName(0)
		, szDescription(0)
		, bCompressed(false)
		, bSelectable(false)
	{
	}

	PixelFormatInfo(
		int         a_bytesPerBlock,
		int         a_Channels,
		bool        a_Alpha,
		const char* a_szAlpha,
		int         a_minWidth,
		int         a_minHeight,
		int         a_blockWidth,
		int         a_blockHeight,
		bool        a_bSquarePow2,
		D3DFORMAT   a_d3d9Format,
		DXGI_FORMAT a_d3d10Format,
		ESampleType a_eSampleType,
		const char* a_szName,
		const char* a_szDescription,
		bool        a_bCompressed,
		bool        a_bSelectable)
		: bytesPerBlock(a_bytesPerBlock)
		, nChannels(a_Channels)
		, bHasAlpha(a_Alpha)
		, minWidth(a_minWidth)
		, minHeight(a_minHeight)
		, blockWidth(a_blockWidth)
		, blockHeight(a_blockHeight)
		, bSquarePow2(a_bSquarePow2)
		, szAlpha(a_szAlpha)
		, d3d9Format(a_d3d9Format)
		, d3d10Format(a_d3d10Format)
		, eSampleType(a_eSampleType)
		, szName(a_szName)
		, szDescription(a_szDescription)
		, bCompressed(a_bCompressed)
		, bSelectable(a_bSelectable)
	{
	}
};


class ImageObject;

class CPixelFormats
{
public:
	static inline bool IsFormatWithoutAlpha(enum EPixelFormat fmt)
	{
		// all these formats have no alpha-channel at all
		return (fmt == ePixelFormat_DXT1 || fmt == ePixelFormat_BC1);
	}

	static inline bool IsFormatWithThresholdAlpha(enum EPixelFormat fmt)
	{
		// all these formats have a 1bit alpha-channel
		return (fmt == ePixelFormat_DXT1a || fmt == ePixelFormat_BC1a);
	}

	static inline bool IsFormatWithWeightingAlpha(enum EPixelFormat fmt)
	{
		// all these formats use alpha to weight the primary channels
		return (fmt == ePixelFormat_DXT1a || fmt == ePixelFormat_DXT3t || fmt == ePixelFormat_DXT5t || fmt == ePixelFormat_BC1a || fmt == ePixelFormat_BC2t || fmt == ePixelFormat_BC3t || fmt == ePixelFormat_BC7t);
	}

	static inline bool IsFormatSingleChannel(enum EPixelFormat fmt)
	{
		// all these formats have a single channel
		return (fmt == ePixelFormat_A8 || fmt == ePixelFormat_R8 || fmt == ePixelFormat_R16 || fmt == ePixelFormat_R16F || fmt == ePixelFormat_R32F || fmt == ePixelFormat_BC4 || fmt == ePixelFormat_BC4s);
	}
	
	static inline bool IsFormatSigned(enum EPixelFormat fmt)
	{
		// all these formats contain signed data, the FP-formats contain scale & biased unsigned data
		return (fmt == ePixelFormat_BC4s || fmt == ePixelFormat_BC5s /*|| fmt == ePixelFormat_BC6SH*/);
	}

	static inline bool IsFormatFloatingPoint(enum EPixelFormat fmt, bool bFullPrecision)
	{
		// all these formats contain floating point data
		if (!bFullPrecision)
		{
			return ((fmt == ePixelFormat_R16F || fmt == ePixelFormat_G16R16F || fmt == ePixelFormat_A16B16G16R16F) || (fmt == ePixelFormat_BC6UH || fmt == ePixelFormat_E5B9G9R9));
		}
		else
		{
			return ((fmt == ePixelFormat_R32F || fmt == ePixelFormat_G32R32F || fmt == ePixelFormat_A32B32G32R32F));
		}
	}

	static const PixelFormatInfo* GetPixelFormatInfo(EPixelFormat format);
	static bool GetPixelFormatInfo(EPixelFormat format, ESampleType* pSampleType, int* pChannelCount, bool* pHasAlpha);
	static int GetBitsPerPixel(EPixelFormat format);

	static bool IsPixelFormatWithoutAlpha(EPixelFormat format);
	static bool IsPixelFormatUncompressed(EPixelFormat format);
	static bool IsPixelFormatAnyRGB(EPixelFormat format);
	static bool IsPixelFormatAnyRG(EPixelFormat format);
	static bool IsPixelFormatForExtendedDDSOnly(EPixelFormat format);

	// Note: case is ignored
	// Returns (EPixelFormat)-1 if the name was not recognized
	static EPixelFormat FindPixelFormatByName(const char* name);
	static EPixelFormat FindFinalTextureFormat(EPixelFormat format, bool bAlphaChannelUsed);

	static uint32 ComputeMaxMipCount(EPixelFormat format, uint32 width, uint32 height, bool bCubemap);

	static bool BuildSurfaceHeader(const ImageObject* pImage, uint32 maxMipCount, CImageExtensionHelper::DDS_HEADER& header, bool bForceDX10);
	static bool BuildSurfaceExtendedHeader(const ImageObject* pImage, uint32 sliceCount, CImageExtensionHelper::DDS_HEADER_DXT10& exthead);

	static bool ParseSurfaceHeader(ImageObject* pImage, uint32& maxMipCount, const CImageExtensionHelper::DDS_HEADER& header, bool bForceDX10);
	static bool ParseSurfaceExtendedHeader(ImageObject* pImage, uint32& sliceCount, const CImageExtensionHelper::DDS_HEADER_DXT10& exthead);
};


#define D3DFMT_3DC           ((D3DFORMAT)(MAKEFOURCC('A', 'T', 'I', '2')))   // two channel compressed normal maps 8bit -> 8 bits per pixel
#define D3DFMT_3DCp          ((D3DFORMAT)(MAKEFOURCC('A', 'T', 'I', '1')))   // one channel compressed maps 8bit -> 4 bits per pixel
#define D3DFMT_CTX1          ((D3DFORMAT)(MAKEFOURCC('C', 'T', 'X', '1')))   // two channel compressed normal maps 4bit -> 4 bits per pixel
#define D3DFMT_PVRTC2        ((D3DFORMAT)(MAKEFOURCC('P', 'V', 'R', '2')))   // POWERVR texture compression, 2 bits per pixel, block is 8x4 pixels, 64 bits
#define D3DFMT_PVRTC4        ((D3DFORMAT)(MAKEFOURCC('P', 'V', 'R', '4')))   // POWERVR texture compression, 4 bits per pixel, block is 4x4 pixels, 64 bits
// Sokov: ETC2/EAC fourcc codes below are not official, I made them by myself. Feel free to replace them by better codes.
#define D3DFMT_ETC2          ((D3DFORMAT)(MAKEFOURCC('E', 'T', '2', ' ')))   // ETC2 RGB texture compression, 4 bits per pixel, block is 4x4 pixels, 64 bits
#define D3DFMT_ETC2a         ((D3DFORMAT)(MAKEFOURCC('E', 'T', '2', 'A')))   // ETC2 RGBA texture compression, 8 bits per pixel, block is 4x4 pixels, 128 bits
#define D3DFMT_EAC_R11       ((D3DFORMAT)(MAKEFOURCC('E', 'A', 'R', ' ')))   // EAC one channel texture compression, 4 bits per pixel, block is 4x4 pixels, 64 bits
#define D3DFMT_EAC_RG11      ((D3DFORMAT)(MAKEFOURCC('E', 'A', 'R', 'G')))   // EAC two channel texture compression, 4 bits per pixel, block is 4x4 pixels, 128 bits
#define D3DFMT_ASTC_LDR_L    ((D3DFORMAT)(MAKEFOURCC('A', 'L', '1', ' ')))   // ASTC compressed texture format for single channel LDR maps, 128 bits
#define D3DFMT_ASTC_LDR_A    ((D3DFORMAT)(MAKEFOURCC('A', 'L', '1', 'A')))   // ASTC compressed texture format for single channel LDR alpha maps, 128 bits
#define D3DFMT_ASTC_LDR_LA   ((D3DFORMAT)(MAKEFOURCC('A', 'L', '2', 'A')))   // ASTC compressed texture format for dual channel LDR luminance-alpha maps, 128 bits
#define D3DFMT_ASTC_LDR_RG   ((D3DFORMAT)(MAKEFOURCC('A', 'L', '2', ' ')))   // ASTC compressed texture format for dual channel LDR red-green maps, 128 bits
#define D3DFMT_ASTC_LDR_N    ((D3DFORMAT)(MAKEFOURCC('A', 'L', 'N', ' ')))   // ASTC compressed texture format for dual channel LDR normalmaps, 128 bits
#define D3DFMT_ASTC_LDR_RGB  ((D3DFORMAT)(MAKEFOURCC('A', 'L', '3', ' ')))   // ASTC compressed texture format for RGB LDR maps, 128 bits
#define D3DFMT_ASTC_LDR_RGBA ((D3DFORMAT)(MAKEFOURCC('A', 'L', '4', ' ')))   // ASTC compressed texture format for RGBA LDR maps, 128 bits
#define D3DFMT_ASTC_HDR_L    ((D3DFORMAT)(MAKEFOURCC('A', 'H', '1', ' ')))   // ASTC compressed texture format for single channel HDR maps, 128 bits
#define D3DFMT_ASTC_HDR_RGB  ((D3DFORMAT)(MAKEFOURCC('A', 'H', '3', ' ')))   // ASTC compressed texture format for RGB HDR maps, 128 bits
#define D3DFMT_ASTC_HDR_RGBA ((D3DFORMAT)(MAKEFOURCC('A', 'H', '4', ' ')))   // ASTC compressed texture format for RGBA HDR maps, 128 bits
#define D3DFMT_DX10          ((D3DFORMAT)(MAKEFOURCC('D', 'X', '1', '0')))   // DirectX 10+ header

#endif // __PIXELFORMATS_H__
