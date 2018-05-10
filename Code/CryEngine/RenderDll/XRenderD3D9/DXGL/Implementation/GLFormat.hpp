// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLFormat.hpp
//  Version:     v1.00
//  Created:     24/04/2013 by Valerio Guagliumi.
//  Description: Declares the image formats used by DXGL and the utility
//               methods for converting to and from DXGI formats.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLFORMAT__
#define __GLFORMAT__

#include "GLCommon.hpp"

// Enumeration of the IDs of all the supported GI format (each ID must match one
// DXGI_FORMAT enumeration value - excluding the DXGI_FORMAT prefix).
#define DXGL_UNCOMPRESSED_GI_FORMATS(_Func) \
  _Func(R32G32B32A32_TYPELESS)              \
  _Func(R32G32B32A32_FLOAT)                 \
  _Func(R32G32B32A32_UINT)                  \
  _Func(R32G32B32A32_SINT)                  \
  _Func(R32G32B32_TYPELESS)                 \
  _Func(R32G32B32_FLOAT)                    \
  _Func(R32G32B32_UINT)                     \
  _Func(R32G32B32_SINT)                     \
  _Func(R16G16B16A16_TYPELESS)              \
  _Func(R16G16B16A16_FLOAT)                 \
  _Func(R16G16B16A16_UNORM)                 \
  _Func(R16G16B16A16_UINT)                  \
  _Func(R16G16B16A16_SNORM)                 \
  _Func(R16G16B16A16_SINT)                  \
  _Func(R32G32_TYPELESS)                    \
  _Func(R32G32_FLOAT)                       \
  _Func(R32G32_UINT)                        \
  _Func(R32G32_SINT)                        \
  _Func(R32G8X24_TYPELESS)                  \
  _Func(D32_FLOAT_S8X24_UINT)               \
  _Func(R32_FLOAT_X8X24_TYPELESS)           \
  _Func(X32_TYPELESS_G8X24_UINT)            \
  _Func(R10G10B10A2_TYPELESS)               \
  _Func(R10G10B10A2_UNORM)                  \
  _Func(R10G10B10A2_UINT)                   \
  _Func(R11G11B10_FLOAT)                    \
  _Func(R8G8B8A8_TYPELESS)                  \
  _Func(R8G8B8A8_UNORM)                     \
  _Func(R8G8B8A8_UNORM_SRGB)                \
  _Func(R8G8B8A8_UINT)                      \
  _Func(R8G8B8A8_SNORM)                     \
  _Func(R8G8B8A8_SINT)                      \
  _Func(R16G16_TYPELESS)                    \
  _Func(R16G16_FLOAT)                       \
  _Func(R16G16_UNORM)                       \
  _Func(R16G16_UINT)                        \
  _Func(R16G16_SNORM)                       \
  _Func(R16G16_SINT)                        \
  _Func(R32_TYPELESS)                       \
  _Func(D32_FLOAT)                          \
  _Func(R32_FLOAT)                          \
  _Func(R32_UINT)                           \
  _Func(R32_SINT)                           \
  _Func(R24G8_TYPELESS)                     \
  _Func(D24_UNORM_S8_UINT)                  \
  _Func(R24_UNORM_X8_TYPELESS)              \
  _Func(X24_TYPELESS_G8_UINT)               \
  _Func(R8G8_TYPELESS)                      \
  _Func(R8G8_UNORM)                         \
  _Func(R8G8_UINT)                          \
  _Func(R8G8_SNORM)                         \
  _Func(R8G8_SINT)                          \
  _Func(R16_TYPELESS)                       \
  _Func(R16_FLOAT)                          \
  _Func(D16_UNORM)                          \
  _Func(R16_UNORM)                          \
  _Func(R16_UINT)                           \
  _Func(R16_SNORM)                          \
  _Func(R16_SINT)                           \
  _Func(R8_TYPELESS)                        \
  _Func(R8_UNORM)                           \
  _Func(R8_UINT)                            \
  _Func(R8_SNORM)                           \
  _Func(R8_SINT)                            \
  _Func(A8_UNORM)                           \
  _Func(R9G9B9E5_SHAREDEXP)                 \
  _Func(B5G6R5_UNORM)                       \
  _Func(B5G5R5A1_UNORM)                     \
  _Func(B8G8R8A8_TYPELESS)                  \
  _Func(B8G8R8A8_UNORM)                     \
  _Func(B8G8R8A8_UNORM_SRGB)                \
  _Func(B8G8R8X8_TYPELESS)                  \
  _Func(B8G8R8X8_UNORM)                     \
  _Func(B8G8R8X8_UNORM_SRGB)
#define DXGL_COMPRESSED_GI_FORMATS(_Func) \
  _Func(BC1_TYPELESS)                     \
  _Func(BC1_UNORM)                        \
  _Func(BC1_UNORM_SRGB)                   \
  _Func(BC2_TYPELESS)                     \
  _Func(BC2_UNORM)                        \
  _Func(BC2_UNORM_SRGB)                   \
  _Func(BC3_TYPELESS)                     \
  _Func(BC3_UNORM)                        \
  _Func(BC3_UNORM_SRGB)                   \
  _Func(BC4_TYPELESS)                     \
  _Func(BC4_UNORM)                        \
  _Func(BC4_SNORM)                        \
  _Func(BC5_TYPELESS)                     \
  _Func(BC5_UNORM)                        \
  _Func(BC5_SNORM)                        \
  _Func(BC6H_TYPELESS)                    \
  _Func(BC6H_UF16)                        \
  _Func(BC6H_SF16)                        \
  _Func(BC7_TYPELESS)                     \
  _Func(BC7_UNORM)                        \
  _Func(BC7_UNORM_SRGB)                   \
  _Func(EAC_R11_TYPELESS)                 \
  _Func(EAC_R11_UNORM)                    \
  _Func(EAC_R11_SNORM)                    \
  _Func(EAC_RG11_TYPELESS)                \
  _Func(EAC_RG11_UNORM)                   \
  _Func(EAC_RG11_SNORM)                   \
  _Func(ETC2_TYPELESS)                    \
  _Func(ETC2_UNORM)                       \
  _Func(ETC2_UNORM_SRGB)                  \
  _Func(ETC2A_TYPELESS)                   \
  _Func(ETC2A_UNORM)                      \
  _Func(ETC2A_UNORM_SRGB)
#define DXGL_GI_FORMATS(_Func)        \
  DXGL_UNCOMPRESSED_GI_FORMATS(_Func) \
  DXGL_COMPRESSED_GI_FORMATS(_Func)

namespace NCryOpenGL
{

// Supported DXGI format identifiers
enum EGIFormat
{
#define DXGL_GI_FORMAT(_FormatID)            eGIF_ ## _FormatID
#define _GI_FORMAT_ENUM_FUNC(_FormatID, ...) DXGL_GI_FORMAT(_FormatID),
	DXGL_GI_FORMATS(_GI_FORMAT_ENUM_FUNC)
#undef _GI_FORMAT_ENUM_FUNC
	DXGL_GI_FORMAT(NUM)
};

// Data types supported for DXGI format channels
enum EGIChannelType
{
	eGICT_UNUSED,   // Not used by the format
	eGICT_UNORM,    // Unsigned normalized integer (range linearly mapped to [0, 1])
	eGICT_UINT,     // Unsigned integer
	eGICT_FLOAT,    // Floating point value
	eGICT_SNORM,    // Signed normalized integer (range linearly mapped to [-1, 1])
	eGICT_SINT,     // Signed integer
};

// Utility information about an uncompressed pixel format
struct SUncompressedLayout
{
	// Number of bits per pixel for the red, green, blue and alpha channels
	int32 m_uRedBits;
	int32 m_uGreenBits;
	int32 m_uBlueBits;
	int32 m_uAlphaBits;

	// Position for the red, green, blue and alpha channels within each pixel
	int32 m_uRedShift;
	int32 m_uGreenShift;
	int32 m_uBlueShift;
	int32 m_uAlphaShift;

	// The data type for the red, green, blue and alpha channels
	EGIChannelType m_eColorType;

	// Number of bits of the depth channel
	int32          m_uDepthBits;
	// The data type for the depth channel
	EGIChannelType m_eDepthType;

	// Number of bits of the stencil channel
	int32          m_uStencilBits;
	// The data type for the stencil channel
	EGIChannelType m_eStencilType;

	// Number of spare bits not used by the format layout
	int32        m_uSpareBits;

	inline int32 GetColorBits() const
	{
		return m_uRedBits + m_uGreenBits + m_uBlueBits + m_uAlphaBits;
	}

	inline int32 GetDepthStencilBits() const
	{
		return m_uDepthBits + m_uStencilBits;
	}

	inline int32 GetPixelBits() const
	{
		return GetColorBits() + GetDepthStencilBits() + m_uSpareBits;
	}

	inline int32 GetPixelBytes() const
	{
		int32 uPixelBits(GetPixelBits());
		assert(uPixelBits % 8 == 0);
		return uPixelBits >> 3;
	}
};

// Identifies a set of swizzle modes for each color channel
typedef uint16 TSwizzleMask;

struct STextureFormat
{
	// The OpenGL image format used to store textures internally
	GLint m_iInternalFormat;

	// The OpenGL image format used at application level for pixel data
	GLenum m_eBaseFormat;

	// Tells if the image data is compressed
	bool m_bCompressed;

	// Tells if the the format assumes data in sRGB space
	bool m_bSRGB;

	// Native OpenGL pixel data type (not meaningful if m_bCompressed is true)
	GLenum m_eDataType;

	// The size in pixels of a texture block (compression block if m_bCompressed is true, otherwise one pixel).
	uint32 m_uBlockWidth;
	uint32 m_uBlockHeight;
	uint32 m_uBlockDepth;

	// The size in bytes of a texture block (compression block if m_bCompressed is true, otherwise one pixel).
	uint32 m_uNumBlockBytes;
};

// Identifies the possible format conversion procedures available
enum EGIFormatConversion
{
	// A texture view is required to reinterpret the source format as the destination format
	eGIFC_TEXTURE_VIEW,

	// The depth channel of the source is mapped to the red channel of the destination
	eGIFC_DEPTH_TO_RED,

	// The stencil channel of the source is mapped to the red channel of the destination
	eGIFC_STENCIL_TO_RED,

	// The source format cannot be converted to the destination format
	eGIFC_UNSUPPORTED,

	// The source format can be used in place of the destination format without conversion
	eGIFC_NONE
};

struct SGIFormatInfo
{
	// Equivalent DXGI format
	DXGI_FORMAT m_eDXGIFormat;

	// Default supported usage mask for features/GL versions where support detection is not possible (union of D3D11_FORMAT_SUPPORT flags)
	uint32 m_uDefaultSupport;

	// Pointer to the texture information, or NULL if not a valid texture format
	const STextureFormat* m_pTexture;

	// Pointer to the uncompressed layout information, or NULL if compressed
	const SUncompressedLayout* m_pUncompressed;

	// Identifies the the compatible typeless format, or eGIF_NUM if no such format exists
	EGIFormat m_eTypelessFormat;

	// Describes how to convert to the compatible typeless format
	EGIFormatConversion m_eTypelessConversion;

	// The color channel swizzle modes required to map this format to the associated DXGI format
	TSwizzleMask m_uDXGISwizzleMask;
};

extern const DXGI_FORMAT DXGI_FORMAT_INVALID;

// Finds the DXGI format corresponding to the given EGIFormat value.
// Returns DXGI_FORMAT_INVALID if the format was not found.
DXGI_FORMAT GetDXGIFormat(EGIFormat eGIFormat);

// Finds the DXGL EGIFormat value corresponding to the given DXGI_FORMAT.
// Returns eGIF_NUM if the given format is not supported.
EGIFormat GetGIFormat(DXGI_FORMAT eDXGIFormat);

// Gets the global format information for the given EGIFormat
const SGIFormatInfo* GetGIFormatInfo(EGIFormat eGIFormat);

// Converts a TSwizzleMask to red, green, blue and alpha texture swizzle modes
void SwizzleMaskToRGBA(TSwizzleMask uMask, GLint aiRGBA[4]);

// Converts a combination of red, green, blue and alpha texture swizzle modes to a TSwizzleMask
bool RGBAToSwizzleMask(const GLint aRGBA[4], TSwizzleMask& uMask);

// Tries to match a valid image format for a given EGIFormat value and returns true on success
bool GetImageFormat(EGIFormat eGIFormat, GLenum* peImageFormat);

};

#endif //__GLFORMAT__
