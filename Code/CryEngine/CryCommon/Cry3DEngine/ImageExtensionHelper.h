// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

	#include <CryRenderer/ITexture.h>
	#include <CryCore/CryEndian.h>
	#include <CryMath/Cry_Math.h>
	#include <CryMath/Cry_Vector3.h>
	#include <CryMath/Cry_Color.h>

	#ifndef MAKEFOURCC
		#define MAKEFOURCC(ch0, ch1, ch2, ch3)                  \
		  ((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) | \
		   ((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24))
	#endif /* defined(MAKEFOURCC) */

// This header defines constants and structures that are useful when parsing
// DDS files.  DDS files were originally designed to use several structures
// and constants that are native to DirectDraw and are defined in ddraw.h,
// such as DDSURFACEDESC2 and DDSCAPS2.  This file defines similar
// (compatible) constants and structures so that one can use DDS files
// without needing to include ddraw.h.

// Crytek specific image extensions
// Usually added to the end of DDS files

	#define DDS_FOURCC                  0x00000004 //!< DDPF_FOURCC
	#define DDS_RGB                     0x00000040 //!< DDPF_RGB
	#define DDS_LUMINANCE               0x00020000 //!< DDPF_LUMINANCE
	#define DDS_SIGNED                  0x00080000 //!< DDPF_SIGNED
	#define DDS_RGBA                    0x00000041 //!< DDPF_RGB | DDPF_ALPHAPIXELS
	#define DDS_LUMINANCEA              0x00020001 //!< DDS_LUMINANCE | DDPF_ALPHAPIXELS
	#define DDS_A                       0x00000001 //!< DDPF_ALPHAPIXELS
	#define DDS_A_ONLY                  0x00000002 //!< DDPF_ALPHA

	#define DDS_FOURCC_A16B16G16R16     0x00000024 //!< FOURCC A16B16G16R16
	#define DDS_FOURCC_V16U16           0x00000040 //!< FOURCC V16U16
	#define DDS_FOURCC_Q16W16V16U16     0x0000006E //!< FOURCC Q16W16V16U16
	#define DDS_FOURCC_R16F             0x0000006F //!< FOURCC R16F
	#define DDS_FOURCC_G16R16F          0x00000070 //!< FOURCC G16R16F
	#define DDS_FOURCC_A16B16G16R16F    0x00000071 //!< FOURCC A16B16G16R16F
	#define DDS_FOURCC_R32F             0x00000072 //!< FOURCC R32F
	#define DDS_FOURCC_G32R32F          0x00000073 //!< FOURCC G32R32F
	#define DDS_FOURCC_A32B32G32R32F    0x00000074 //!< FOURCC A32B32G32R32F

	#define DDSD_CAPS                   0x00000001l //!< Default.
	#define DDSD_PIXELFORMAT            0x00001000l
	#define DDSD_WIDTH                  0x00000004l
	#define DDSD_HEIGHT                 0x00000002l
	#define DDSD_LINEARSIZE             0x00080000l

	#define DDS_HEADER_FLAGS_TEXTURE    0x00001007 //!< DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
	#define DDS_HEADER_FLAGS_MIPMAP     0x00020000 //!< DDSD_MIPMAPCOUNT
	#define DDS_HEADER_FLAGS_VOLUME     0x00800000 //!< DDSD_DEPTH
	#define DDS_HEADER_FLAGS_PITCH      0x00000008 //!< DDSD_PITCH
	#define DDS_HEADER_FLAGS_LINEARSIZE 0x00080000 //!< DDSD_LINEARSIZE

	#define DDS_SURFACE_FLAGS_TEXTURE   0x00001000 //!< DDSCAPS_TEXTURE
	#define DDS_SURFACE_FLAGS_MIPMAP    0x00400008 //!< DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
	#define DDS_SURFACE_FLAGS_CUBEMAP   0x00000008 //!< DDSCAPS_COMPLEX

	#define DDS_CUBEMAP_POSITIVEX       0x00000600 //!< DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
	#define DDS_CUBEMAP_NEGATIVEX       0x00000a00 //!< DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
	#define DDS_CUBEMAP_POSITIVEY       0x00001200 //!< DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
	#define DDS_CUBEMAP_NEGATIVEY       0x00002200 //!< DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
	#define DDS_CUBEMAP_POSITIVEZ       0x00004200 //!< DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
	#define DDS_CUBEMAP_NEGATIVEZ       0x00008200 //!< DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

	#define DDS_CUBEMAP_ALLFACES        (DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX | \
	  DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |                                    \
	  DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ)

	#define DDS_FLAGS_VOLUME            0x00200000 //!< DDSCAPS2_VOLUME

	#define DDS_RESF1_NORMALMAP         0x01000000
	#define DDS_RESF1_DSDT              0x02000000

	#define CRY_DDS_DX10_SUPPORT

	#if CRY_PLATFORM_WINDOWS && !defined(DXGI_FORMAT_DEFINED)
		#include <dxgiformat.h> // DX10+ formats.
	#endif

namespace CImageExtensionHelper
{
struct DDS_PIXELFORMAT
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	DWORD dwRGBBitCount;
	DWORD dwRBitMask;
	DWORD dwGBitMask;
	DWORD dwBBitMask;
	DWORD dwABitMask;

	const bool operator==(const DDS_PIXELFORMAT& fmt) const
	{
		return dwFourCC == fmt.dwFourCC &&
		       dwFlags == fmt.dwFlags &&
		       dwRGBBitCount == fmt.dwRGBBitCount &&
		       dwRBitMask == fmt.dwRBitMask &&
		       dwGBitMask == fmt.dwGBitMask &&
		       dwBBitMask == fmt.dwBBitMask &&
		       dwABitMask == fmt.dwABitMask &&
		       dwSize == fmt.dwSize;
	}

	AUTO_STRUCT_INFO;
};

struct DDS_HEADER_DXT10
{
	//! We're unable to use native enums because of TypeInfo(), so we use DWORD instead.
	DWORD /*DXGI_FORMAT*/              dxgiFormat;
	DWORD /*D3D10_RESOURCE_DIMENSION*/ resourceDimension;
	UINT                               miscFlag;
	UINT                               arraySize;
	UINT                               reserved;

	AUTO_STRUCT_INFO;
};

struct DDS_HEADER
{
	DWORD           dwSize;
	DWORD           dwHeaderFlags;
	DWORD           dwHeight;
	DWORD           dwWidth;
	DWORD           dwPitchOrLinearSize;
	DWORD           dwDepth;  //!< Only if DDS_HEADER_FLAGS_VOLUME is set in dwHeaderFlags.
	DWORD           dwMipMapCount;
	DWORD           dwAlphaBitDepth;
	DWORD           dwReserved1;    //!< Crytek image flags.
	float           fAvgBrightness; //!< Average top mip brightness. Could be f16/half.
	ColorF          cMinColor;
	ColorF          cMaxColor;
	DDS_PIXELFORMAT ddspf;
	DWORD           dwSurfaceFlags;
	DWORD           dwCubemapFlags;
	BYTE            bNumPersistentMips;
	BYTE            bTileMode;
	BYTE			bCompressedBlockWidth;
	BYTE			bCompressedBlockHeight;
	BYTE            bReserved2[4];
	DWORD           dwTextureStage;

	AUTO_STRUCT_INFO;

	inline const bool   IsValid() const     { return sizeof(*this) == dwSize; }
	inline const bool   IsDX10Ext() const   { return ddspf.dwFourCC == MAKEFOURCC('D', 'X', '1', '0'); }
	inline const uint32 GetMipCount() const { return max(1u, (uint32)dwMipMapCount); }

	inline const size_t GetFullHeaderSize() const
	{
		if (IsDX10Ext())
		{
			return sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);
		}

		return sizeof(DDS_HEADER);
	}
};

//! Standard description of file header.
struct DDS_FILE_DESC
{
	DWORD      dwMagic;
	DDS_HEADER header;

	AUTO_STRUCT_INFO;

	inline const bool   IsValid() const           { return dwMagic == MAKEFOURCC('D', 'D', 'S', ' ') && header.IsValid(); }
	inline const size_t GetFullHeaderSize() const { return sizeof(dwMagic) + header.GetFullHeaderSize(); }
};

// Chunk identifier.
const static uint32 FOURCC_CExt = MAKEFOURCC('C', 'E', 'x', 't');               //!< Crytek extension start.
const static uint32 FOURCC_AvgC = MAKEFOURCC('A', 'v', 'g', 'C');               //!< Average color.
const static uint32 FOURCC_CEnd = MAKEFOURCC('C', 'E', 'n', 'd');               //!< Crytek extension end.
const static uint32 FOURCC_AttC = MAKEFOURCC('A', 't', 't', 'C');               //!< Chunk Attached Channel.

//! Flags to propagate from the RC to the engine through GetImageFlags() 32bit bitmask.
//! Numbers should not change as engine relies on them.
//! Free ones: 0x20, 0x80, 0x100, 0x1000, 0x2000, 0x4000
const static uint32 EIF_Cubemap = 0x1;
const static uint32 EIF_Volumetexture = 0x2;
const static uint32 EIF_Decal = 0x4;                         //!< This is usually set through the preset.
const static uint32 EIF_Greyscale = 0x8;                     //!< Hint for the engine (e.g. greyscale light beams can be applied to shadow mask), can be for DXT1 because compression artfacts don't count as color.
const static uint32 EIF_SupressEngineReduce = 0x10;          //!< Info for the engine: don't reduce texture resolution on this texture.
const static uint32 EIF_FileSingle = 0x40;                   //!< Info for the engine: no need to search for other files (e.g. DDNDIF) (only used for build).
const static uint32 EIF_Compressed = 0x200;                  //!< Info for the engine: it's an MCT or PTC compressed texture for XBox.
const static uint32 EIF_AttachedAlpha = 0x400;               //!< Info for the engine: it's a texture with attached alpha channel.
const static uint32 EIF_SRGBRead = 0x800;                    //!< Info for the engine: if gamma corrected rendering is on, this texture requires SRGBRead (it's not stored in linear).
const static uint32 EIF_DontResize = 0x8000;                 //!< Info for the engine: for dds textures that shouldn't be resized with r_TexResolution.
const static uint32 EIF_RenormalizedTexture = 0x10000;       //!< Info for the engine: for dds textures that have renormalized color range.
const static uint32 EIF_CafeNative = 0x20000;                //!< Info for the engine: native Cafe texture format.
const static uint32 EIF_OrbisNative = 0x40000;               //!< Info for the engine: native Orbis texture format.
const static uint32 EIF_Tiled = 0x80000;                     //!< Info for the engine: texture has been tiled for the platform.
const static uint32 EIF_DurangoNative = 0x100000;            //!< Info for the engine: native Durango texture format.
const static uint32 EIF_Splitted = 0x200000;                 //!< Info for the engine: this texture is splitted.
const static uint32 EIF_Colormodel = 0x7000000;              //!< Info for the engine: bitmask: colormodel used in the texture.
const static uint32 EIF_Colormodel_RGB = 0x0000000;          //!< Info for the engine: colormodel is RGB (default).
const static uint32 EIF_Colormodel_CIE = 0x1000000;          //!< Info for the engine: colormodel is CIE (used for terrain).
const static uint32 EIF_Colormodel_YCC = 0x2000000;          //!< Info for the engine: colormodel is Y'CbCr (used for reflectance).
const static uint32 EIF_Colormodel_YFF = 0x3000000;          //!< Info for the engine: colormodel is Y'FbFr (used for reflectance).
const static uint32 EIF_Colormodel_IRB = 0x4000000;          //!< Info for the engine: colormodel is IRB (used for reflectance).

enum ETileMode
{
	eTM_None = 0,
	eTM_LinearPadded,
	eTM_Optimal,
};

//! \param pDDSHeader Must not be 0.
//! \return Chunk flags (combined from EIF_Cubemap,EIF_Volumetexture,EIF_Decal,...)
inline uint32 GetImageFlags(DDS_HEADER* pDDSHeader)
{
	assert(pDDSHeader);

	// non standardized way to expose some features in the header (same information is in attached chunk but then
	// streaming would need to find this spot in the file)
	// if this is causing problems we need to change it
	if (pDDSHeader->dwSize >= sizeof(DDS_HEADER))
	{
		if (pDDSHeader->dwTextureStage == 'CRYF')
		{
			return pDDSHeader->dwReserved1;
		}
	}

	return 0;
}

//! \param pDDSHeader Must not be 0.
//! \return Chunk flags (combined from EIF_Cubemap,EIF_Volumetexture,EIF_Decal,...)
inline bool SetImageFlags(DDS_HEADER* pDDSHeader, uint32 flags)
{
	assert(pDDSHeader);

	// non standardized way to expose some features in the header (same information is in attached chunk but then
	// streaming would need to find this spot in the file)
	// if this is causing problems we need to change it
	if (pDDSHeader->dwSize >= sizeof(DDS_HEADER))
	{
		if (pDDSHeader->dwTextureStage == 'CRYF')
		{
			pDDSHeader->dwReserved1 = flags;
			return true;
		}
	}

	return false;
}

//! \param pMem Usually first byte behind DDS file data, can be 0 (e.g. in case there no more bytes than DDS file data).
//! \return start of the named chunk, or 0 if it does not exist.
inline uint8 const* _findChunkStart(uint8 const* pMem, const uint32 dwChunkName, uint32* dwOutSize = NULL)
{
	if (pMem)
		if (*(uint32*)pMem == SwapEndianValue(FOURCC_CExt))
		{
			pMem += 4;  // jump over chunk name
			while (*(uint32*)pMem != SwapEndianValue(FOURCC_CEnd))
			{
				if (*(uint32*)pMem == SwapEndianValue(dwChunkName))
				{
					pMem += 4;  // jump over chunk name
					const uint32 size = SwapEndianValue(*(uint32*)(pMem));
					if (dwOutSize)
						*dwOutSize = size;
					pMem += 4;  // jump over chunk size
					return size > 0 ? pMem : NULL;
				}

				pMem += 8 + SwapEndianValue(*(uint32*)(&pMem[4]));      // jump over chunk
			}
		}

	return 0;     // chunk does not exist
}

//! \param pMem Usually first byte behind DDS file data, can be 0 (e.g. in case there no more bytes than DDS file data).
static ColorF GetAverageColor(uint8 const* pMem)
{
	pMem = _findChunkStart(pMem, FOURCC_AvgC);

	if (pMem)
	{
		ColorF ret = ColorF(SwapEndianValue(*(uint32*)pMem));
		//flip red and blue
		const float cRed = ret.r;
		ret.r = ret.b;
		ret.b = cRed;
		return ret;
	}

	return Col_White;   // chunk does not exist
}

//! \param pMem Usually first byte behind DDS file data, can be 0 (e.g. in case there no more bytes than DDS file data).
// Returns:
//   pointer to the DDS header
inline DDS_HEADER* GetAttachedImage(uint8 const* pMem, uint32* dwOutSize = NULL)
{
	pMem = _findChunkStart(pMem, FOURCC_AttC, dwOutSize);
	if (pMem)
		return (DDS_HEADER*)(pMem + 4);

	if (dwOutSize)
		*dwOutSize = 0;
	return 0;   // chunk does not exist
}

inline int BytesPerBlock(ETEX_Format eTF)
{
	switch (eTF)
	{
	case eTF_CTX1:
	case eTF_BC1:
	case eTF_BC4U:
	case eTF_BC4S:
	case eTF_ETC2:
	case eTF_EAC_R11:
	case eTF_EAC_R11S:
		return 8;
	case eTF_BC2:
	case eTF_BC3:
	case eTF_BC5U:
	case eTF_BC5S:
	case eTF_BC6UH:
	case eTF_BC6SH:
	case eTF_BC7:
	case eTF_ETC2A:
	case eTF_EAC_RG11:
	case eTF_EAC_RG11S:
		return 16;
	default:
		assert(0);
	}
	return 0;
}

inline int BitsPerPixel(ETEX_Format eTF)
{
	switch (eTF)
	{
	case eTF_R8G8B8A8S:
		return 32;
	case eTF_R8G8B8A8:
		return 32;

	case eTF_R1:
		return 1;
	case eTF_A8:
		return 8;
	case eTF_R8:
		return 8;
	case eTF_R8S:
		return 8;
	case eTF_R16:
		return 16;
	case eTF_R16S:
		return 16;
	case eTF_R16F:
		return 16;
	case eTF_R32F:
		return 32;
	case eTF_R8G8:
		return 16;
	case eTF_R8G8S:
		return 16;
	case eTF_R16G16:
		return 32;
	case eTF_R16G16S:
		return 32;
	case eTF_R16G16F:
		return 32;
	case eTF_R32G32F:
		return 64;
	case eTF_R11G11B10F:
		return 32;
	case eTF_R10G10B10A2:
		return 32;
	case eTF_R16G16B16A16:
		return 64;
	case eTF_R16G16B16A16S:
		return 64;
	case eTF_R16G16B16A16F:
		return 64;
	case eTF_R32G32B32A32F:
		return 128;

	case eTF_CTX1:
		return 4;
	case eTF_BC1:
		return 4;
	case eTF_BC2:
		return 8;
	case eTF_BC3:
		return 8;
	case eTF_BC4U:
		return 4;
	case eTF_BC4S:
		return 4;
	case eTF_BC5U:
		return 8;
	case eTF_BC5S:
		return 8;
	case eTF_BC6UH:
		return 8;
	case eTF_BC6SH:
		return 8;
	case eTF_BC7:
		return 8;
	case eTF_R9G9B9E5:
		return 32;

	case eTF_S8:
		return 8;
	case eTF_D16:
		return 16;
	case eTF_D16S8:
		return 16;
	case eTF_D24:
		return 32;
	case eTF_D24S8:
		return 32;
	case eTF_D32F:
		return 32;
	case eTF_D32FS8:
		return 32;

	case eTF_B5G6R5:
		return 16;
	case eTF_B5G5R5A1:
		return 16;
	case eTF_B4G4R4A4:
		return 16;

	case eTF_R4G4:
		return 8;
	case eTF_R4G4B4A4:
		return 16;

	case eTF_EAC_R11:
		return 4;
	case eTF_EAC_R11S:
		return 4;
	case eTF_EAC_RG11:
		return 8;
	case eTF_EAC_RG11S:
		return 8;
	case eTF_ETC2:
		return 4;
	case eTF_ETC2A:
		return 8;
	case eTF_ASTC_LDR_4x4:
		return 8;

	case eTF_A8L8:
		return 16;
	case eTF_L8:
		return 8;
	case eTF_L8V8U8:
		return 24;
	case eTF_B8G8R8:
		return 24;
	case eTF_L8V8U8X8:
		return 32;
	case eTF_B8G8R8X8:
		return 32;
	case eTF_B8G8R8A8:
		return 32;

	default:
		assert(0);                         // pass through for better behaviour in non debug
	}

	return 0;
}

inline bool IsBlockCompressed(ETEX_Format eTF)
{
	return (eTF == eTF_BC1 ||
	        eTF == eTF_BC2 ||
	        eTF == eTF_BC3 ||
	        eTF == eTF_BC4U ||
	        eTF == eTF_BC4S ||
	        eTF == eTF_BC5S ||
	        eTF == eTF_BC5U ||
	        eTF == eTF_BC6UH ||
	        eTF == eTF_BC6SH ||
	        eTF == eTF_BC7 ||
	        eTF == eTF_CTX1 ||
	        eTF == eTF_ETC2 ||
	        eTF == eTF_EAC_R11 ||
	        eTF == eTF_EAC_R11S ||
	        eTF == eTF_ETC2A ||
	        eTF == eTF_EAC_RG11 ||
	        eTF == eTF_EAC_RG11S);
}

static bool IsRangeless(ETEX_Format eTF)
{
	return (eTF == eTF_BC6UH ||
	        eTF == eTF_BC6SH ||
	        eTF == eTF_R9G9B9E5 ||
	        eTF == eTF_R16G16B16A16F ||
	        eTF == eTF_R32G32B32A32F ||
	        eTF == eTF_R16F ||
	        eTF == eTF_R32F ||
	        eTF == eTF_R16G16F ||
	        eTF == eTF_R32G32F ||
	        eTF == eTF_R11G11B10F);
}

static bool IsQuantized(ETEX_Format eTF)
{
	return (eTF == eTF_B4G4R4A4 ||
	        eTF == eTF_B5G6R5 ||
	        eTF == eTF_B5G5R5A1 ||
	        eTF == eTF_BC1 ||
	        eTF == eTF_BC2 ||
	        eTF == eTF_BC3 ||
	        eTF == eTF_BC4U ||
	        eTF == eTF_BC4S ||
	        eTF == eTF_BC5U ||
	        eTF == eTF_BC5S ||
	        eTF == eTF_BC6UH ||
	        eTF == eTF_BC6SH ||
	        eTF == eTF_BC7 ||
	        eTF == eTF_R9G9B9E5 ||
	        eTF == eTF_ETC2 ||
	        eTF == eTF_EAC_R11 ||
	        eTF == eTF_EAC_R11S ||
	        eTF == eTF_ETC2A ||
	        eTF == eTF_EAC_RG11 ||
	        eTF == eTF_EAC_RG11S);
}

// Added this code from Image, as it has less dependencies here.
// Warning: duplicate code.
inline const char* NameForTextureFormat(ETEX_Format ETF)
{
	switch (ETF)
	{
	case eTF_Unknown:
		return "Unknown";

	case eTF_R8G8B8A8S:
		return "R8G8B8A8S";
	case eTF_R8G8B8A8:
		return "R8G8B8A8";

	case eTF_R1:
		return "R1";
	case eTF_A8:
		return "A8";
	case eTF_R8:
		return "R8";
	case eTF_R8S:
		return "R8S";
	case eTF_R16:
		return "R16";
	case eTF_R16S:
		return "R16S";
	case eTF_R16F:
		return "R16F";
	case eTF_R32F:
		return "R32F";
	case eTF_R8G8:
		return "R8G8";
	case eTF_R8G8S:
		return "R8G8S";
	case eTF_R16G16:
		return "R16G16";
	case eTF_R16G16S:
		return "R16G16S";
	case eTF_R16G16F:
		return "R16G16F";
	case eTF_R32G32F:
		return "R32G32F";
	case eTF_R11G11B10F:
		return "R11G11B10F";
	case eTF_R10G10B10A2:
		return "R10G10B10A2";
	case eTF_R16G16B16A16:
		return "R16G16B16A16";
	case eTF_R16G16B16A16S:
		return "R16G16B16A16S";
	case eTF_R16G16B16A16F:
		return "R16G16B16A16F";
	case eTF_R32G32B32A32F:
		return "R32G32B32A32F";

	case eTF_CTX1:
		return "CTX1";
	case eTF_BC1:
		return "BC1";
	case eTF_BC2:
		return "BC2";
	case eTF_BC3:
		return "BC3";
	case eTF_BC4U:
		return "BC4";
	case eTF_BC4S:
		return "BC4S";
	case eTF_BC5U:
		return "BC5";
	case eTF_BC5S:
		return "BC5S";
	case eTF_BC6UH:
		return "BC6UH";
	case eTF_BC6SH:
		return "BC6SH";
	case eTF_BC7:
		return "BC7";
	case eTF_R9G9B9E5:
		return "R9G9B9E5";

	case eTF_S8:
		return "S8";
	case eTF_D16:
		return "D16";
	case eTF_D16S8:
		return "D16S8";
	case eTF_D24:
		return "D24";
	case eTF_D24S8:
		return "D24S8";
	case eTF_D32F:
		return "D32F";
	case eTF_D32FS8:
		return "D32FS8";

	case eTF_B5G6R5:
		return "R5G6B5";
	case eTF_B5G5R5A1:
		return "R5G5B5A1";
	case eTF_B4G4R4A4:
		return "B4G4R4A4";

	case eTF_R4G4:
		return "R4G4";
	case eTF_R4G4B4A4:
		return "R4G4B4A4";

	case eTF_EAC_R11:
		return "EAC_R11";
	case eTF_EAC_R11S:
		return "EAC_R11S";
	case eTF_EAC_RG11:
		return "EAC_RG11";
	case eTF_EAC_RG11S:
		return "EAC_RG11S";
	case eTF_ETC2:
		return "ETC2";
	case eTF_ETC2A:
		return "ETC2A";
	case eTF_ASTC_LDR_4x4:
		return "ASTC_LDR_4x4";

	case eTF_A8L8:
		return "A8L8";
	case eTF_L8:
		return "L8";
	case eTF_L8V8U8:
		return "L8V8U8";
	case eTF_B8G8R8:
		return "B8G8R8";
	case eTF_L8V8U8X8:
		return "L8V8U8X8";
	case eTF_B8G8R8X8:
		return "B8G8R8X8";
	case eTF_B8G8R8A8:
		return "B8G8R8A8";

	default:
		assert(0);                         // pass through for better behaviour in non debug
	}

	return "Unknown";
}

// Added this code from Image, as it has less dependencies here.
// Warning: duplicate code.
inline ETEX_Format TextureFormatForName(const char* sETF)
{
	if (!stricmp(sETF, "Unknown"))         return eTF_Unknown;

	if (!stricmp(sETF, "R8G8B8A8S"))       return eTF_R8G8B8A8S;
	if (!stricmp(sETF, "R8G8B8A8"))        return eTF_R8G8B8A8;

	if (!stricmp(sETF, "R1"))              return eTF_R1;
	if (!stricmp(sETF, "A8"))              return eTF_A8;
	if (!stricmp(sETF, "R8"))              return eTF_R8;
	if (!stricmp(sETF, "R8S"))             return eTF_R8S;
	if (!stricmp(sETF, "R16"))             return eTF_R16;
	if (!stricmp(sETF, "R16S"))            return eTF_R16S;
	if (!stricmp(sETF, "R16F"))            return eTF_R16F;
	if (!stricmp(sETF, "R32F"))            return eTF_R32F;
	if (!stricmp(sETF, "R8G8"))            return eTF_R8G8;
	if (!stricmp(sETF, "R8G8S"))           return eTF_R8G8S;
	if (!stricmp(sETF, "R16G16"))          return eTF_R16G16;
	if (!stricmp(sETF, "R16G16S"))         return eTF_R16G16S;
	if (!stricmp(sETF, "R16G16F"))         return eTF_R16G16F;
	if (!stricmp(sETF, "R32G32F"))         return eTF_R32G32F;
	if (!stricmp(sETF, "R11G11B10F"))      return eTF_R11G11B10F;
	if (!stricmp(sETF, "R10G10B10A2"))     return eTF_R10G10B10A2;
	if (!stricmp(sETF, "R16G16B16A16"))    return eTF_R16G16B16A16;
	if (!stricmp(sETF, "R16G16B16A16S"))   return eTF_R16G16B16A16S;
	if (!stricmp(sETF, "R16G16B16A16F"))   return eTF_R16G16B16A16F;
	if (!stricmp(sETF, "R32G32B32A32F"))   return eTF_R32G32B32A32F;

	if (!stricmp(sETF, "CTX1"))            return eTF_CTX1;
	if (!stricmp(sETF, "BC1"))             return eTF_BC1;
	if (!stricmp(sETF, "BC2"))             return eTF_BC2;
	if (!stricmp(sETF, "BC3"))             return eTF_BC3;
	if (!stricmp(sETF, "BC4"))             return eTF_BC4U;
	if (!stricmp(sETF, "BC4S"))            return eTF_BC4S;
	if (!stricmp(sETF, "BC5"))             return eTF_BC5U;
	if (!stricmp(sETF, "BC5S"))            return eTF_BC5S;
	if (!stricmp(sETF, "BC6UH"))           return eTF_BC6UH;
	if (!stricmp(sETF, "BC6SH"))           return eTF_BC6SH;
	if (!stricmp(sETF, "BC7"))             return eTF_BC7;
	if (!stricmp(sETF, "R9G9B9E5"))        return eTF_R9G9B9E5;

	if (!stricmp(sETF, "S8"))              return eTF_S8;
	if (!stricmp(sETF, "D16"))             return eTF_D16;
	if (!stricmp(sETF, "D16S8"))           return eTF_D16S8;
	if (!stricmp(sETF, "D24"))             return eTF_D24;
	if (!stricmp(sETF, "D24S8"))           return eTF_D24S8;
	if (!stricmp(sETF, "D32F"))            return eTF_D32F;
	if (!stricmp(sETF, "D32FS8"))          return eTF_D32FS8;

	if (!stricmp(sETF, "R5G6B5"))          return eTF_B5G6R5;
	if (!stricmp(sETF, "R5G5B5A1"))        return eTF_B5G5R5A1;
	if (!stricmp(sETF, "B4G4R4A4"))        return eTF_B4G4R4A4;

	if (!stricmp(sETF, "R4G4"))            return eTF_R4G4;
	if (!stricmp(sETF, "R4G4B4A4"))        return eTF_R4G4B4A4;

	if (!stricmp(sETF, "EAC_R11"))         return eTF_EAC_R11;
	if (!stricmp(sETF, "EAC_R11S"))        return eTF_EAC_R11S;
	if (!stricmp(sETF, "EAC_RG11"))        return eTF_EAC_RG11;
	if (!stricmp(sETF, "EAC_RG11S"))       return eTF_EAC_RG11S;
	if (!stricmp(sETF, "ETC2"))            return eTF_ETC2;
	if (!stricmp(sETF, "ETC2A"))           return eTF_ETC2A;
	if (!stricmp(sETF, "ASTC_LDR_4x4"))    return eTF_ASTC_LDR_4x4;

	if (!stricmp(sETF, "A8L8"))            return eTF_A8L8;
	if (!stricmp(sETF, "L8"))              return eTF_L8;
	if (!stricmp(sETF, "L8V8U8"))          return eTF_L8V8U8;
	if (!stricmp(sETF, "B8G8R8"))          return eTF_B8G8R8;
	if (!stricmp(sETF, "L8V8U8X8"))        return eTF_L8V8U8X8;
	if (!stricmp(sETF, "B8G8R8X8"))        return eTF_B8G8R8X8;
	if (!stricmp(sETF, "B8G8R8A8"))        return eTF_B8G8R8A8;

	// Legacy strings
	if (!stricmp(sETF, "V8U8"))            return eTF_R8G8S;
	if (!stricmp(sETF, "V16U16"))          return eTF_R16G16S;

	if (!stricmp(sETF, "DXT1"))            return eTF_BC1;
	if (!stricmp(sETF, "DXT3"))            return eTF_BC2;
	if (!stricmp(sETF, "DXT5"))            return eTF_BC3;
	if (!stricmp(sETF, "ATI1"))            return eTF_BC4U;
	if (!stricmp(sETF, "ATI2"))            return eTF_BC5U;
	if (!stricmp(sETF, "3DCp"))            return eTF_BC4U;
	if (!stricmp(sETF, "3DC"))             return eTF_BC5U;
	if (!stricmp(sETF, "RGBE"))            return eTF_R9G9B9E5;

	assert(0);
	return eTF_Unknown;
}

// Added this code from Image, as it has less dependencies here.
// Warning: duplicate code.
inline const char* NameForTextureType(ETEX_Type eTT)
{
	const char* sETT;
	switch (eTT)
	{
	case eTT_1D:
		sETT = "1D";
		break;
	case eTT_2D:
		sETT = "2D";
		break;
	case eTT_2DArray:
		sETT = "2D array";
		break;
	case eTT_2DMS:
		sETT = "2D multi-sampled";
		break;
	case eTT_3D:
		sETT = "3D";
		break;
	case eTT_Cube:
		sETT = "Cube";
		break;
	case eTT_CubeArray:
		sETT = "CubeArray";
		break;
	case eTT_Auto2D:
		sETT = "Auto2D";
		break;
	case eTT_Dyn2D:
		sETT = "Dyn2D";
		break;
	default:
		assert(0);
		sETT = "Unknown";     // for better behaviour in non debug
		break;
	}
	return sETT;
}

inline ETEX_Type TextureTypeForName(const char* sETT)
{
	if (!stricmp(sETT, "1D"))
		return eTT_1D;
	if (!stricmp(sETT, "2D"))
		return eTT_2D;
	if (!stricmp(sETT, "2DArray"))
		return eTT_2DArray;
	if (!stricmp(sETT, "3D"))
		return eTT_3D;
	if (!stricmp(sETT, "Cube"))
		return eTT_Cube;
	if (!stricmp(sETT, "CubeArray"))
		return eTT_CubeArray;
	if (!stricmp(sETT, "Auto2D"))
		return eTT_Auto2D;
	if (!stricmp(sETT, "Dyn2D"))
		return eTT_Dyn2D;
	if (!stricmp(sETT, "User"))
		return eTT_User;
	assert(0);
	return eTT_2D;
}

inline bool HasAlphaForName(const char* sETF)
{
	if (!stricmp(sETF, "R8G8B8A8S"))     return true;
	if (!stricmp(sETF, "R8G8B8A8"))      return true;

	if (!stricmp(sETF, "A8"))            return true;
	if (!stricmp(sETF, "R10G10B10A2"))   return true;
	if (!stricmp(sETF, "R16G16B16A16"))  return true;
	if (!stricmp(sETF, "R16G16B16A16S")) return true;
	if (!stricmp(sETF, "R16G16B16A16F")) return true;
	if (!stricmp(sETF, "R32G32B32A32F")) return true;

	if (!stricmp(sETF, "BC2"))           return true;
	if (!stricmp(sETF, "BC3"))           return true;
	if (!stricmp(sETF, "BC7"))           return true;

	if (!stricmp(sETF, "B4G4R4A4"))      return true;

	if (!stricmp(sETF, "ETC2A"))         return true;

	if (!stricmp(sETF, "A8L8"))          return true;
	if (!stricmp(sETF, "B8G8R8A8"))      return true;
	if (!stricmp(sETF, "B5G5R5A1"))      return true;

	if (!stricmp(sETF, "DXT3"))          return true;
	if (!stricmp(sETF, "DXT5"))          return true;

	return false;
}

inline bool HasAlphaForTextureFormat(ETEX_Format ETF)
{
	if (ETF == eTF_R8G8B8A8S)            return true;
	if (ETF == eTF_R8G8B8A8)             return true;

	if (ETF == eTF_A8)                   return true;
	if (ETF == eTF_R10G10B10A2)          return true;
	if (ETF == eTF_R16G16B16A16)         return true;
	if (ETF == eTF_R16G16B16A16S)        return true;
	if (ETF == eTF_R16G16B16A16F)        return true;
	if (ETF == eTF_R32G32B32A32F)        return true;

	if (ETF == eTF_BC2)                  return true;
	if (ETF == eTF_BC3)                  return true;
	if (ETF == eTF_BC7)                  return true;

	if (ETF == eTF_B4G4R4A4)             return true;

	if (ETF == eTF_ETC2A)                return true;

	if (ETF == eTF_A8L8)                 return true;
	if (ETF == eTF_B8G8R8A8)             return true;
	if (ETF == eTF_B5G5R5A1)             return true;

	return false;
}

static bool IsDynamicRange(ETEX_Format eTF)
{
	return
		// datatype has range outside [0,1]
		IsRangeless(eTF) &&
		// range is encoded in alpha-channel of min/max color range if the format itself has no alpha channel
		!HasAlphaForTextureFormat(eTF);
}

inline const char* NameForDesc(const DDS_PIXELFORMAT& ddspf, DWORD /*DXGI_FORMAT*/ dxgif);
};

namespace DDSFormats
{
const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DX10 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D', 'X', '1', '0'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT1 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D', 'X', 'T', '1'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT2 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D', 'X', 'T', '2'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT3 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D', 'X', 'T', '3'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT4 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D', 'X', 'T', '4'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_DXT5 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('D', 'X', 'T', '5'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_CTX1 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('C', 'T', 'X', '1'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_3DC =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '2'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_3DCP =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('A', 'T', 'I', '1'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_EAC_R11 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('E', 'A', 'R', ' '), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_EAC_R11S =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('E', 'A', 'r', ' '), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_EAC_RG11 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('E', 'A', 'R', 'G'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_EAC_RG11S =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('E', 'A', 'r', 'g'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_ETC2 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('E', 'T', '2', ' '), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_ETC2A =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, MAKEFOURCC('E', 'T', '2', 'A'), 0, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_R32F =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_R32F, 32, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_G32R32F =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_G32R32F, 64, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A32B32G32R32F =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_A32B32G32R32F, 128, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_R16F =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_R16F, 16, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_G16R16F =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_G16R16F, 32, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A16B16G16R16F =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_A16B16G16R16F, 64, 0, 0, 0, 0 };

//! Unofficial.
const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_U16 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_SIGNED, 0, 16, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000 };

//! Unofficial.
const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_V16U16 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_SIGNED, 0, 32, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_Q16W16V16U16 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_Q16W16V16U16, 64, 0, 0, 0, 0 };

//! Unofficial.
const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_R16 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000 };

//! Unofficial.
const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_G16R16 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 32, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A16B16G16R16 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_FOURCC, DDS_FOURCC_A16B16G16R16, 64, 0, 0, 0, 0 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_R8 =   // unofficial
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 8, 0x000000ff, 0x00000000, 0x00000000, 0x00000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_G8R8 =   // unofficial
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 8, 0x000000ff, 0x0000ff00, 0x00000000, 0x00000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A8B8G8R8 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A8R8G8B8 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A1R5G5B5 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A4R4G4B4 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGBA, 0, 16, 0x00000f00, 0x000000f0, 0x0000000f, 0x0000f000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_R8G8B8 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 24, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_X8R8G8B8 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_R5G6B5 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_RGB, 0, 16, 0x0000f800, 0x000007e0, 0x0000001f, 0x00000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A8 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_A, 0, 8, 0x00000000, 0x00000000, 0x00000000, 0x000000ff };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_L8 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_LUMINANCE, 0, 8, 0x000000ff, 0x000000ff, 0x000000ff, 0x00000000 };

const CImageExtensionHelper::DDS_PIXELFORMAT DDSPF_A8L8 =
{ sizeof(CImageExtensionHelper::DDS_PIXELFORMAT), DDS_LUMINANCEA, 0, 8, 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff };

inline ETEX_Format GetFormatByDesc(const CImageExtensionHelper::DDS_PIXELFORMAT& ddspf)
{
	if (ddspf.dwFourCC == DDSPF_DXT1.dwFourCC)
		return eTF_BC1;
	else if (ddspf.dwFourCC == DDSPF_DXT3.dwFourCC)
		return eTF_BC2;
	else if (ddspf.dwFourCC == DDSPF_DXT5.dwFourCC)
		return eTF_BC3;
	else if (ddspf.dwFourCC == DDSPF_3DCP.dwFourCC)
		return eTF_BC4U;
	else if (ddspf.dwFourCC == DDSPF_3DC.dwFourCC)
		return eTF_BC5U;
	else if (ddspf.dwFourCC == DDSPF_CTX1.dwFourCC)
		return eTF_CTX1;
	else if (ddspf == DDSPF_R8)
		return eTF_R8;
	else if (ddspf == DDSPF_R16)
		return eTF_R16;
	else if (ddspf == DDSPF_U16)
		return eTF_R16S;
	else if (ddspf.dwFourCC == DDSPF_R32F.dwFourCC)
		return eTF_R32F;
	else if (ddspf == DDSPF_G8R8)
		return eTF_R8G8;
	else if (ddspf == DDSPF_G16R16)
		return eTF_R16G16;
	else if (ddspf == DDSPF_V16U16)
		return eTF_R16G16S;
	else if (ddspf.dwFourCC == DDSPF_R16F.dwFourCC)
		return eTF_R16F;
	else if (ddspf.dwFourCC == DDSPF_G16R16F.dwFourCC)
		return eTF_R16G16F;
	else if (ddspf.dwFourCC == DDSPF_G32R32F.dwFourCC)
		return eTF_R32G32F;
	else if (ddspf.dwFlags == DDS_RGBA && ddspf.dwRGBBitCount == 16 && ddspf.dwRBitMask == 0x0000000f && ddspf.dwABitMask == 0x0000f000)
		return eTF_R4G4B4A4;
	else if (ddspf.dwFlags == DDS_RGBA && ddspf.dwRGBBitCount == 32 && ddspf.dwRBitMask == 0x000000ff && ddspf.dwABitMask == 0xff000000)
		return eTF_R8G8B8A8;
	else if (ddspf.dwFourCC == DDSPF_A16B16G16R16F.dwFourCC)
		return eTF_R16G16B16A16F;
	else if (ddspf.dwFourCC == DDSPF_Q16W16V16U16.dwFourCC)
		return eTF_R16G16B16A16S;
	else if (ddspf.dwFourCC == DDSPF_A32B32G32R32F.dwFourCC)
		return eTF_R32G32B32A32F;
	else if (ddspf.dwFourCC == DDSPF_A16B16G16R16.dwFourCC)
		return eTF_R16G16B16A16;
	else if (ddspf.dwFourCC == DDSPF_EAC_R11.dwFourCC)
		return eTF_EAC_R11;
	else if (ddspf.dwFourCC == DDSPF_EAC_R11S.dwFourCC)
		return eTF_EAC_R11S;
	else if (ddspf.dwFourCC == DDSPF_EAC_RG11.dwFourCC)
		return eTF_EAC_RG11;
	else if (ddspf.dwFourCC == DDSPF_EAC_RG11S.dwFourCC)
		return eTF_EAC_RG11S;
	else if (ddspf.dwFourCC == DDSPF_ETC2.dwFourCC)
		return eTF_ETC2;
	else if (ddspf.dwFourCC == DDSPF_ETC2A.dwFourCC)
		return eTF_ETC2A;
	else if (ddspf.dwFlags == DDS_RGBA && ddspf.dwRGBBitCount == 16 && ddspf.dwRBitMask == 0x00000f00 && ddspf.dwABitMask == 0x0000f000)
		return eTF_B4G4R4A4;
	else if (ddspf.dwFlags == DDS_RGBA && ddspf.dwRGBBitCount == 32 && ddspf.dwRBitMask == 0x00ff0000 && ddspf.dwABitMask == 0xff000000)
		return eTF_B8G8R8A8;
	else if (ddspf.dwFlags == DDS_RGB && ddspf.dwRGBBitCount == 32 && ddspf.dwRBitMask == 0x00ff0000)
		return eTF_B8G8R8X8;
	else if (ddspf.dwFlags == DDS_RGBA && ddspf.dwRGBBitCount == 16)
		return eTF_B4G4R4A4;
	else if (ddspf.dwFlags == DDS_RGB && ddspf.dwRGBBitCount == 24)
		return eTF_B8G8R8;
	else if (ddspf.dwFlags == DDS_LUMINANCEA && ddspf.dwRGBBitCount == 8)
		return eTF_A8L8;
	else if (ddspf.dwFlags == DDS_LUMINANCE && ddspf.dwRGBBitCount == 8)
		return eTF_L8;
	else if ((ddspf.dwFlags == DDS_A || ddspf.dwFlags == DDS_A_ONLY || ddspf.dwFlags == (DDS_A | DDS_A_ONLY)) && ddspf.dwRGBBitCount == 8)
		return eTF_A8;

	assert(0);
	return eTF_Unknown;
}

inline ETEX_Format GetFormatByDesc(const CImageExtensionHelper::DDS_PIXELFORMAT& ddspf, const DWORD /*DXGI_FORMAT*/ dxgif)
{
	// 'DX10' indicates the format is not in the FourCC, but in the extended header
	if (ddspf.dwFourCC == DDSPF_DX10.dwFourCC)
	{
	#if defined(CRY_DDS_DX10_SUPPORT)
		switch (dxgif)
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
		case DXGI_FORMAT_R16_SNORM:
			return eTF_R16S;
		case DXGI_FORMAT_R16_FLOAT:
			return eTF_R16F;
		case DXGI_FORMAT_R16_TYPELESS:
			return eTF_R16F;                                         // arbitrary choice for F
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

		// only available as hardware format under DX11.1 with DXGI 1.2
		case DXGI_FORMAT_B5G6R5_UNORM:
			return eTF_B5G6R5;
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			return eTF_B5G5R5A1;
		case DXGI_FORMAT_B4G4R4A4_UNORM:
			return eTF_B4G4R4A4;

		#if (CRY_RENDERER_OPENGL >= 430)
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
		case DXGI_FORMAT_ETC2A_UNORM:
			return eTF_ETC2A;
		#endif

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
		}
	#endif
		return eTF_Unknown;
	}
	else
		return GetFormatByDesc(ddspf);
}

inline const bool IsNormalMap(const ETEX_Format eTF)
{
	if (eTF == eTF_BC5U || eTF == eTF_BC5S || eTF == eTF_CTX1 ||
		eTF == eTF_EAC_RG11 || eTF == eTF_EAC_RG11S)
		return true;
	return false;
}

inline const bool IsSigned(const ETEX_Format eTF)
{
	if (eTF == eTF_BC4S || eTF == eTF_BC5S || eTF == eTF_BC6SH || eTF == eTF_R8S || eTF == eTF_R8G8S || eTF == eTF_R16G16S || eTF == eTF_R8G8B8A8S || eTF == eTF_R16G16B16A16S || eTF == eTF_EAC_RG11S)
		return true;
	return false;
}

// Added this code from Image, as it has less dependencies here.
// Warning: duplicate code.
inline const CImageExtensionHelper::DDS_PIXELFORMAT& GetDescByFormat(const ETEX_Format eTF)
{
	switch (eTF)
	{
	case eTF_BC1:
		return DDSPF_DXT1;
	case eTF_BC2:
		return DDSPF_DXT3;
	case eTF_BC3:
		return DDSPF_DXT5;
	case eTF_BC4U:
		return DDSPF_3DCP;
	case eTF_BC5U:
		return DDSPF_3DC;
	case eTF_CTX1:
		return DDSPF_CTX1;
	case eTF_R16:
		return DDSPF_R16;
	case eTF_R16S:
		return DDSPF_U16;
	case eTF_R16F:
		return DDSPF_R16F;
	case eTF_R32F:
		return DDSPF_R32F;
	case eTF_R16G16:
		return DDSPF_G16R16;
	case eTF_R16G16S:
		return DDSPF_V16U16;
	case eTF_R16G16F:
		return DDSPF_G16R16F;
	case eTF_R32G32F:
		return DDSPF_G32R32F;
	case eTF_R16G16B16A16:
		return DDSPF_A16B16G16R16;
	case eTF_R16G16B16A16S:
		return DDSPF_Q16W16V16U16;
	case eTF_R16G16B16A16F:
		return DDSPF_A16B16G16R16F;
	case eTF_R32G32B32A32F:
		return DDSPF_A32B32G32R32F;
	case eTF_B8G8R8:
	case eTF_L8V8U8:
		return DDSPF_R8G8B8;
	case eTF_R8G8B8A8:
		return DDSPF_A8B8G8R8;
	case eTF_B8G8R8X8:
	case eTF_L8V8U8X8:
		return DDSPF_X8R8G8B8;
	case eTF_B8G8R8A8:
		return DDSPF_A8R8G8B8;
	case eTF_B5G6R5:
		return DDSPF_R5G6B5;
	case eTF_A8:
		return DDSPF_A8;
	case eTF_L8:
		return DDSPF_L8;
	case eTF_A8L8:
		return DDSPF_A8L8;
	case eTF_EAC_R11:
		return DDSPF_EAC_R11;
	case eTF_EAC_R11S:
		return DDSPF_EAC_R11S;
	case eTF_EAC_RG11:
		return DDSPF_EAC_RG11;
	case eTF_EAC_RG11S:
		return DDSPF_EAC_RG11S;
	case eTF_ETC2:
		return DDSPF_ETC2;
	case eTF_ETC2A:
		return DDSPF_ETC2A;
	default:
		assert(0);
		return DDSPF_A8B8G8R8;
	}
}

inline const CImageExtensionHelper::DDS_PIXELFORMAT& GetDescByFormat(DWORD& dxgifOut, const ETEX_Format eTF)
{
	dxgifOut = 0;

	switch (eTF)
	{
	#if defined(CRY_DDS_DX10_SUPPORT)
	case eTF_R1:
		dxgifOut = DXGI_FORMAT_R1_UNORM;
		return DDSPF_DX10;
	case eTF_R8:
		dxgifOut = DXGI_FORMAT_R8_UNORM;
		return DDSPF_DX10;
	case eTF_R8S:
		dxgifOut = DXGI_FORMAT_R8_SNORM;
		return DDSPF_DX10;
	case eTF_R16:
		dxgifOut = DXGI_FORMAT_R16_UNORM;
		return DDSPF_DX10;
	case eTF_R16S:
		dxgifOut = DXGI_FORMAT_R16_SNORM;
		return DDSPF_DX10;
	case eTF_R16F:
		dxgifOut = DXGI_FORMAT_R16_FLOAT;
		return DDSPF_DX10;
	case eTF_R8G8:
		dxgifOut = DXGI_FORMAT_R8G8_UNORM;
		return DDSPF_DX10;
	case eTF_R8G8S:
		dxgifOut = DXGI_FORMAT_R8G8_SNORM;
		return DDSPF_DX10;
	case eTF_R16G16:
		dxgifOut = DXGI_FORMAT_R16G16_UNORM;
		return DDSPF_DX10;
	case eTF_R16G16S:
		dxgifOut = DXGI_FORMAT_R16G16_SNORM;
		return DDSPF_DX10;
	case eTF_R16G16F:
		dxgifOut = DXGI_FORMAT_R16G16_FLOAT;
		return DDSPF_DX10;
	case eTF_R32G32F:
		dxgifOut = DXGI_FORMAT_R32G32_FLOAT;
		return DDSPF_DX10;
	case eTF_R11G11B10F:
		dxgifOut = DXGI_FORMAT_R11G11B10_FLOAT;
		return DDSPF_DX10;
	case eTF_R10G10B10A2:
		dxgifOut = DXGI_FORMAT_R10G10B10A2_UNORM;
		return DDSPF_DX10;
	case eTF_R8G8B8A8S:
		dxgifOut = DXGI_FORMAT_R8G8B8A8_SNORM;
		return DDSPF_DX10;
	case eTF_R16G16B16A16:
		dxgifOut = DXGI_FORMAT_R16G16B16A16_UNORM;
		return DDSPF_DX10;
	case eTF_R16G16B16A16S:
		dxgifOut = DXGI_FORMAT_R16G16B16A16_SNORM;
		return DDSPF_DX10;
	case eTF_R32G32B32A32F:
		dxgifOut = DXGI_FORMAT_R32G32B32A32_FLOAT;
		return DDSPF_DX10;

	case eTF_BC4S:
		dxgifOut = DXGI_FORMAT_BC4_SNORM;
		return DDSPF_DX10;
	case eTF_BC5S:
		dxgifOut = DXGI_FORMAT_BC5_SNORM;
		return DDSPF_DX10;
	case eTF_BC6SH:
		dxgifOut = DXGI_FORMAT_BC6H_SF16;
		return DDSPF_DX10;
	case eTF_BC6UH:
		dxgifOut = DXGI_FORMAT_BC6H_UF16;
		return DDSPF_DX10;
	case eTF_BC7:
		dxgifOut = DXGI_FORMAT_BC7_UNORM;
		return DDSPF_DX10;
	case eTF_R9G9B9E5:
		dxgifOut = DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
		return DDSPF_DX10;
	#endif

	default:
		return GetDescByFormat(eTF);
	}
}
};

namespace CImageExtensionHelper
{
inline const char* NameForDesc(const DDS_PIXELFORMAT& ddspf)
{
	ETEX_Format nFormat = DDSFormats::GetFormatByDesc(ddspf);
	return NameForTextureFormat(nFormat);
}

inline const char* NameForDesc(const DDS_PIXELFORMAT& ddspf, DWORD /*DXGI_FORMAT*/ dxgif)
{
	ETEX_Format nFormat = DDSFormats::GetFormatByDesc(ddspf, dxgif);
	return NameForTextureFormat(nFormat);
}
};

//! \endcond