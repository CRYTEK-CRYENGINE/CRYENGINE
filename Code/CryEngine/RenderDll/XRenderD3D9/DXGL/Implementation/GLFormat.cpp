// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLFormat.cpp
//  Version:     v1.00
//  Created:     24/04/2013 by Valerio Guagliumi.
//  Description: Defines the required information about image formats used
//               by DXGL as well as utility functions.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "GLFormat.hpp"

namespace NCryOpenGL
{

#define DXGL_DXGI_FORMAT(_FormatID)          DXGI_FORMAT_ ## _FormatID
#define DXGL_UNCOMPRESSED_LAYOUT(_FormatID)  g_kUncompressedLayout_ ## _FormatID
#define DXGL_TEXTURE_FORMAT(_FormatID)       g_kTextureFormat_ ## _FormatID
#define DXGL_GL_INTERNAL_FORMAT(_InternalID) GL ## _InternalID
#define DXGL_GI_CHANNEL_TYPE(_Type)          eGICT_ ## _Type

#define _PTR_MAP(_Type)                      _Type ## PtrMap
#define _PTR_MAP_DEFAULT(_Type, _Enum, _Ptr)                                      \
  template<_Enum eValue> struct _PTR_MAP (_Type) {static const _Type* ms_pPtr; }; \
  template<_Enum eValue> const _Type * _PTR_MAP(_Type) < eValue > ::ms_pPtr = _Ptr;
#define _PTR_MAP_VALUE(_Type, _Value, _Ptr)                                      \
  template<> struct _PTR_MAP (_Type) < _Value > {static const _Type* ms_pPtr; }; \
  const _Type* _PTR_MAP(_Type) < _Value > ::ms_pPtr = _Ptr;

#define _UINT_MAP(_Name) S ## _Name ## UintMap
#define _UINT_MAP_DEFAULT(_Name, _Enum, _UIntVal) \
  template<_Enum eValue> struct _UINT_MAP (_Name) {enum {VALUE = _UIntVal}; };
#define _UINT_MAP_VALUE(_Name, _Value, _UIntVal) \
  template<> struct _UINT_MAP (_Name) < _Value > {enum {VALUE = _UIntVal}; };

////////////////////////////////////////////////////////////////////////////////
// Uncompressed layouts for renderable formats
////////////////////////////////////////////////////////////////////////////////

_PTR_MAP_DEFAULT(SUncompressedLayout, EGIFormat, NULL)

#define _UNCOMPR_LAY(_FormatID, _NumR, _NumG, _NumB, _NumA, _ShiftR, _ShiftG, _ShiftB, _ShiftA, _TypeRGBA, _Depth, _TypeD, _Stencil, _TypeS, _Spare) \
  static const SUncompressedLayout DXGL_UNCOMPRESSED_LAYOUT(_FormatID) =                                                                             \
  {                                                                                                                                                  \
    _NumR,                           _NumG,                        _NumB,   _NumA,                                                                   \
    _ShiftR,                         _ShiftG,                      _ShiftB, _ShiftA,                                                                 \
    DXGL_GI_CHANNEL_TYPE(_TypeRGBA),                                                                                                                 \
    _Depth,                          DXGL_GI_CHANNEL_TYPE(_TypeD),                                                                                   \
    _Stencil,                        DXGL_GI_CHANNEL_TYPE(_TypeS),                                                                                   \
    _Spare                                                                                                                                           \
  };                                                                                                                                                 \
  _PTR_MAP_VALUE(SUncompressedLayout, DXGL_GI_FORMAT(_FormatID), &DXGL_UNCOMPRESSED_LAYOUT(_FormatID))

//          | FORMAT_ID           | RGBA_SIZES        | RGBA_SHIFTS       | RGBA     | DEPTH         | STENCIL       | -  |
//          |                     | NR   NG   NB   NA | SR   SG   SB   SA | TYPE     | N    TYPE     | N    TYPE     | X  |
_UNCOMPR_LAY(R32G32B32A32_FLOAT, 32, 32, 32, 32, 0, 32, 64, 96, FLOAT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32G32B32A32_UINT, 32, 32, 32, 32, 0, 32, 64, 96, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32G32B32A32_SINT, 32, 32, 32, 32, 0, 32, 64, 96, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32G32B32_FLOAT, 32, 32, 32, 0, 0, 32, 64, 96, FLOAT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32G32B32_UINT, 32, 32, 32, 0, 0, 32, 64, 96, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32G32B32_SINT, 32, 32, 32, 0, 0, 32, 64, 96, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16B16A16_FLOAT, 16, 16, 16, 16, 0, 16, 32, 48, FLOAT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16B16A16_UNORM, 16, 16, 16, 16, 0, 16, 32, 48, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16B16A16_UINT, 16, 16, 16, 16, 0, 16, 32, 48, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16B16A16_SNORM, 16, 16, 16, 16, 0, 16, 32, 48, SNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16B16A16_SINT, 16, 16, 16, 16, 0, 16, 32, 48, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32G32_FLOAT, 32, 32, 0, 0, 0, 32, -1, -1, FLOAT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32G32_UINT, 32, 32, 0, 0, 0, 32, -1, -1, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32G32_SINT, 32, 32, 0, 0, 0, 32, -1, -1, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(D32_FLOAT_S8X24_UINT, 0, 0, 0, 0, -1, -1, -1, -1, UNUSED, 32, FLOAT, 8, UINT, 24)
_UNCOMPR_LAY(R10G10B10A2_UNORM, 10, 10, 10, 2, 0, 10, 20, 30, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R10G10B10A2_UINT, 10, 10, 10, 2, 0, 10, 20, 30, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R11G11B10_FLOAT, 11, 11, 10, 0, 0, 11, 22, -1, FLOAT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8G8B8A8_UNORM, 8, 8, 8, 8, 0, 8, 16, 24, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8G8B8A8_UNORM_SRGB, 8, 8, 8, 8, 0, 8, 16, 24, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8G8B8A8_UINT, 8, 8, 8, 8, 0, 8, 16, 24, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8G8B8A8_SNORM, 8, 8, 8, 8, 0, 8, 16, 24, SNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8G8B8A8_SINT, 8, 8, 8, 8, 0, 8, 16, 24, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16_FLOAT, 16, 16, 0, 0, 0, 16, -1, -1, FLOAT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16_UNORM, 16, 16, 0, 0, 0, 16, -1, -1, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16_UINT, 16, 16, 0, 0, 0, 16, -1, -1, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16_SNORM, 16, 16, 0, 0, 0, 16, -1, -1, SNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16G16_SINT, 16, 16, 0, 0, 0, 16, -1, -1, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(D32_FLOAT, 0, 0, 0, 0, -1, -1, -1, -1, UNUSED, 32, FLOAT, 0, UNUSED, 0)
_UNCOMPR_LAY(R32_FLOAT, 32, 0, 0, 0, 0, -1, -1, -1, FLOAT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32_UINT, 32, 0, 0, 0, 0, -1, -1, -1, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R32_SINT, 32, 0, 0, 0, 0, -1, -1, -1, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(D24_UNORM_S8_UINT, 0, 0, 0, 0, -1, -1, -1, -1, UNUSED, 24, UNORM, 8, UINT, 0)
_UNCOMPR_LAY(R24_UNORM_X8_TYPELESS, 24, 0, 0, 0, -1, -1, -1, -1, UNORM, 0, UNUSED, 0, UNUSED, 8)
_UNCOMPR_LAY(R8G8_UNORM, 8, 8, 0, 0, 0, 8, -1, -1, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8G8_UINT, 8, 8, 0, 0, 0, 8, -1, -1, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8G8_SNORM, 8, 8, 0, 0, 0, 8, -1, -1, SNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8G8_SINT, 8, 8, 0, 0, 0, 8, -1, -1, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16_FLOAT, 16, 0, 0, 0, 0, -1, -1, -1, FLOAT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(D16_UNORM, 0, 0, 0, 0, -1, -1, -1, -1, UNUSED, 16, UNORM, 0, UNUSED, 0)
_UNCOMPR_LAY(R16_UNORM, 16, 0, 0, 0, 0, -1, -1, -1, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16_UINT, 16, 0, 0, 0, 0, -1, -1, -1, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16_SNORM, 16, 0, 0, 0, 0, -1, -1, -1, SNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R16_SINT, 16, 0, 0, 0, 0, -1, -1, -1, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8_UNORM, 8, 0, 0, 0, 0, -1, -1, -1, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8_UINT, 8, 0, 0, 0, 0, -1, -1, -1, UINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8_SNORM, 8, 0, 0, 0, 0, -1, -1, -1, SNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(R8_SINT, 8, 0, 0, 0, 0, -1, -1, -1, SINT, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(A8_UNORM, 0, 0, 0, 8, -1, -1, -1, 0, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(B5G6R5_UNORM, 5, 6, 5, 0, 11, 5, 0, -1, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(B5G5R5A1_UNORM, 5, 5, 5, 1, 10, 5, 0, 15, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(B8G8R8A8_UNORM, 8, 8, 8, 8, 16, 8, 0, 24, UNORM, 0, UNUSED, 0, UNUSED, 0)
_UNCOMPR_LAY(B8G8R8X8_UNORM, 8, 8, 8, 0, 16, 8, 0, -1, UNORM, 0, UNUSED, 0, UNUSED, 8)

#undef _UNCOMPR_LAY

////////////////////////////////////////////////////////////////////////////////
// Swizzle required to map each texture format to the corresponding DXGI format
////////////////////////////////////////////////////////////////////////////////

#define DXGL_SWIZZLE_RED   0
#define DXGL_SWIZZLE_GREEN 1
#define DXGL_SWIZZLE_BLUE  2
#define DXGL_SWIZZLE_ALPHA 3
#define DXGL_SWIZZLE_ZERO  4
#define DXGL_SWIZZLE_ONE   5
#define _SWIZZLE_MASK(_Red, _Green, _Blue, _Alpha) \
  DXGL_SWIZZLE_ ## _Red << 9 |                     \
    DXGL_SWIZZLE_ ## _Green << 6 |                 \
    DXGL_SWIZZLE_ ## _Blue << 3 |                  \
    DXGL_SWIZZLE_ ## _Alpha

_UINT_MAP_DEFAULT(DXGITextureSwizzle, TSwizzleMask, _SWIZZLE_MASK(RED, GREEN, BLUE, ALPHA))

#define _DXGI_TEX_SWIZZLE(_FormatID, _Red, _Green, _Blue, _Alpha) _UINT_MAP_VALUE(DXGITextureSwizzle, DXGL_GI_FORMAT(_FormatID), _SWIZZLE_MASK(_Red, _Green, _Blue, _Alpha))

_DXGI_TEX_SWIZZLE(A8_UNORM, ZERO, ZERO, ZERO, RED)
_DXGI_TEX_SWIZZLE(B8G8R8A8_UNORM, BLUE, GREEN, RED, ALPHA)
_DXGI_TEX_SWIZZLE(B8G8R8X8_UNORM, BLUE, GREEN, RED, ZERO)

#undef _DXGI_TEX_SWIZZLE
#undef _SWIZZLE_MASK

////////////////////////////////////////////////////////////////////////////////
// Texturable formats (split between uncompressed and compressed)
////////////////////////////////////////////////////////////////////////////////

_PTR_MAP_DEFAULT(STextureFormat, EGIFormat, NULL)

#define _TEX(_FormatID, _GLInternalFormat, _GLBaseFormat, _Compressed, _GLType, _BlockWidth, _BlockHeight, _BlockDepth, _NumBlockBytes, _SRGB) \
  static const STextureFormat DXGL_TEXTURE_FORMAT(_FormatID) =                                                                                 \
  {                                                                                                                                            \
    _GLInternalFormat,                                                                                                                         \
    _GLBaseFormat,                                                                                                                             \
    _Compressed,                                                                                                                               \
    _SRGB,                                                                                                                                     \
    _GLType,                                                                                                                                   \
    _BlockWidth,                                                                                                                               \
    _BlockHeight,                                                                                                                              \
    _BlockDepth,                                                                                                                               \
    _NumBlockBytes                                                                                                                             \
  };                                                                                                                                           \
  _PTR_MAP_VALUE(STextureFormat, DXGL_GI_FORMAT(_FormatID), &DXGL_TEXTURE_FORMAT(_FormatID))

#define _UNCOMPR_TEX(_FormatID, _GLInternalFormat, _GLBaseFormat, _GLType, _SRGB, _Bytes)                                      _TEX(_FormatID, _GLInternalFormat, _GLBaseFormat, false, _GLType, 1, 1, 1, _Bytes, _SRGB)
#define _COMPR_TEX(_FormatID, _GLInternalFormat, _GLBaseFormat, _BlockWidth, _BlockHeight, _BlockDepth, _NumBlockBytes, _SRGB) _TEX(_FormatID, _GLInternalFormat, _GLBaseFormat, true, 0, _BlockWidth, _BlockHeight, _BlockDepth, _NumBlockBytes, _SRGB)

//          | FORMAT_ID               | GL_INTERNAL_FORMAT     | GL_FORMAT          | GL_TYPE                           | SRGB  | BYTES |
//          |                         |                        |                    |                                   | SRGB  | BYTES |
_UNCOMPR_TEX(R32G32B32A32_FLOAT, GL_RGBA32F, GL_RGBA, GL_FLOAT, false, 16)
_UNCOMPR_TEX(R32G32B32A32_UINT, GL_RGBA32UI, GL_RGBA, GL_UNSIGNED_INT, false, 16)
_UNCOMPR_TEX(R32G32B32A32_SINT, GL_RGBA32I, GL_RGBA, GL_INT, false, 16)
_UNCOMPR_TEX(R32G32B32_FLOAT, GL_RGB32F, GL_RGB, GL_FLOAT, false, 12)
_UNCOMPR_TEX(R32G32B32_UINT, GL_RGB32UI, GL_RGB, GL_UNSIGNED_INT, false, 12)
_UNCOMPR_TEX(R32G32B32_SINT, GL_RGB32I, GL_RGB, GL_INT, false, 12)
_UNCOMPR_TEX(R16G16B16A16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, 8)
#if DXGLES
_UNCOMPR_TEX(R16G16B16A16_UNORM, GL_RGBA16UI, GL_RGBA, GL_UNSIGNED_SHORT, false, 8)
#else
_UNCOMPR_TEX(R16G16B16A16_UNORM, GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, false, 8)
#endif
_UNCOMPR_TEX(R16G16B16A16_UINT, GL_RGBA16UI, GL_RGBA, GL_UNSIGNED_SHORT, false, 8)
#if !DXGLES
_UNCOMPR_TEX(R16G16B16A16_SNORM, GL_RGBA16_SNORM, GL_RGBA, GL_SHORT, false, 8)
#else
_UNCOMPR_TEX(R16G16B16A16_SNORM, GL_NONE, GL_RGBA, GL_SHORT, false, 8)
#endif
_UNCOMPR_TEX(R16G16B16A16_SINT, GL_RGBA16I, GL_RGBA, GL_SHORT, false, 8)
_UNCOMPR_TEX(R32G32_FLOAT, GL_RG32F, GL_RG, GL_FLOAT, false, 8)
_UNCOMPR_TEX(R32G32_UINT, GL_RG32UI, GL_RG, GL_UNSIGNED_INT, false, 8)
_UNCOMPR_TEX(R32G32_SINT, GL_RG32I, GL_RG, GL_INT, false, 8)
_UNCOMPR_TEX(D32_FLOAT_S8X24_UINT, GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, false, 8)
#if DXGLES
_UNCOMPR_TEX(R10G10B10A2_UNORM, GL_RGB10_A2UI, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, false, 4)
_UNCOMPR_TEX(R10G10B10A2_UINT, GL_RGB10_A2UI, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, false, 4)
#else
_UNCOMPR_TEX(R10G10B10A2_UNORM, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, false, 4)
_UNCOMPR_TEX(R10G10B10A2_UINT, GL_RGB10_A2UI, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, false, 4)
#endif
_UNCOMPR_TEX(R11G11B10_FLOAT, GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, false, 4)
_UNCOMPR_TEX(R8G8B8A8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, 4)
_UNCOMPR_TEX(R8G8B8A8_UNORM_SRGB, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, true, 4)
_UNCOMPR_TEX(R8G8B8A8_UINT, GL_RGBA8UI, GL_RGBA, GL_UNSIGNED_BYTE, false, 4)
_UNCOMPR_TEX(R8G8B8A8_SNORM, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, false, 4)
_UNCOMPR_TEX(R8G8B8A8_SINT, GL_RGBA8I, GL_RGBA, GL_BYTE, false, 4)
_UNCOMPR_TEX(R16G16_FLOAT, GL_RG16F, GL_RG, GL_HALF_FLOAT, false, 4)
#if DXGLES
_UNCOMPR_TEX(R16G16_UNORM, GL_RG16UI, GL_RG, GL_UNSIGNED_SHORT, false, 4)
#else
_UNCOMPR_TEX(R16G16_UNORM, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, false, 4)
#endif
_UNCOMPR_TEX(R16G16_UINT, GL_RG16UI, GL_RG, GL_UNSIGNED_SHORT, false, 4)
_UNCOMPR_TEX(R16G16_SINT, GL_RG16I, GL_RG, GL_SHORT, false, 4)
_UNCOMPR_TEX(D32_FLOAT, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, false, 4)
_UNCOMPR_TEX(R32_FLOAT, GL_R32F, GL_RED, GL_FLOAT, false, 4)
_UNCOMPR_TEX(R32_UINT, GL_R32UI, GL_RED, GL_UNSIGNED_INT, false, 4)
_UNCOMPR_TEX(R32_SINT, GL_R32I, GL_RED, GL_INT, false, 4)
_UNCOMPR_TEX(D24_UNORM_S8_UINT, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, false, 4)
_UNCOMPR_TEX(R8G8_UNORM, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, false, 2)
_UNCOMPR_TEX(R8G8_UINT, GL_RG8UI, GL_RG, GL_UNSIGNED_BYTE, false, 2)
_UNCOMPR_TEX(R8G8_SNORM, GL_RG8_SNORM, GL_RG, GL_BYTE, false, 2)
_UNCOMPR_TEX(R8G8_SINT, GL_RG8I, GL_RG, GL_BYTE, false, 2)
_UNCOMPR_TEX(R16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT, false, 2)
_UNCOMPR_TEX(D16_UNORM, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, false, 2)
#if DXGLES
_UNCOMPR_TEX(R16_UNORM, GL_R16UI, GL_RED, GL_UNSIGNED_SHORT, false, 2)
#else
_UNCOMPR_TEX(R16_UNORM, GL_R16, GL_RED, GL_UNSIGNED_SHORT, false, 2)
#endif
_UNCOMPR_TEX(R16_UINT, GL_R16UI, GL_RED, GL_UNSIGNED_SHORT, false, 2)
_UNCOMPR_TEX(R16_SINT, GL_R16I, GL_RED, GL_SHORT, false, 2)
_UNCOMPR_TEX(R8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, false, 1)
_UNCOMPR_TEX(R8_UINT, GL_R8UI, GL_RED, GL_UNSIGNED_BYTE, false, 1)
_UNCOMPR_TEX(R8_SNORM, GL_R8_SNORM, GL_RED, GL_BYTE, false, 1)
_UNCOMPR_TEX(R8_SINT, GL_R8I, GL_RED, GL_BYTE, false, 1)
_UNCOMPR_TEX(A8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, false, 1)
_UNCOMPR_TEX(R9G9B9E5_SHAREDEXP, GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, false, 4)
_UNCOMPR_TEX(B8G8R8A8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, 4)
_UNCOMPR_TEX(B8G8R8A8_UNORM_SRGB, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false, 4)
_UNCOMPR_TEX(B8G8R8X8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, 4)
_UNCOMPR_TEX(B8G8R8X8_UNORM_SRGB, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false, 4)
#if !DXGLES
_UNCOMPR_TEX(B5G6R5_UNORM, GL_RGB565, GL_BGR, GL_UNSIGNED_SHORT_5_6_5_REV, false, 2)
_UNCOMPR_TEX(B5G5R5A1_UNORM, GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_5_5_5_1, false, 2)
_UNCOMPR_TEX(R16_SNORM, GL_R16_SNORM, GL_RED, GL_SHORT, false, 2)
_UNCOMPR_TEX(R16G16_SNORM, GL_RG16_SNORM, GL_RG, GL_SHORT, false, 4)
#endif

//        | FORMAT_ID       | GL_INTERNAL_FORMAT                        | GL_FORMAT  |      BLOCK        | SRGB  |
//        |                 |                                           |            | X | Y | Z | BYTES |       |
#if DXGL_SUPPORT_ETC2_EAC_COMPRESSION
_COMPR_TEX(EAC_R11_UNORM, GL_COMPRESSED_R11_EAC, GL_RED, 4, 4, 1, 8, false)
_COMPR_TEX(EAC_R11_SNORM, GL_COMPRESSED_SIGNED_R11_EAC, GL_RED, 4, 4, 1, 8, false)
_COMPR_TEX(EAC_RG11_UNORM, GL_COMPRESSED_RG11_EAC, GL_RG, 4, 4, 1, 16, false)
_COMPR_TEX(EAC_RG11_SNORM, GL_COMPRESSED_SIGNED_RG11_EAC, GL_RG, 4, 4, 1, 16, false)
_COMPR_TEX(ETC2_UNORM, GL_COMPRESSED_RGB8_ETC2, GL_RGB, 4, 4, 1, 8, false)
_COMPR_TEX(ETC2_UNORM_SRGB, GL_COMPRESSED_SRGB8_ETC2, GL_RGB, 4, 4, 1, 8, true)
_COMPR_TEX(ETC2A_UNORM, GL_COMPRESSED_RGBA8_ETC2_EAC, GL_RGBA, 4, 4, 1, 16, false)
_COMPR_TEX(ETC2A_UNORM_SRGB, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, GL_RGBA, 4, 4, 1, 16, true)
#endif //DXGL_SUPPORT_ETC2_EAC_COMPRESSION
#if DXGL_SUPPORT_S3TC_TEXTURES
_COMPR_TEX(BC1_UNORM, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_RGBA, 4, 4, 1, 8, false)
_COMPR_TEX(BC1_UNORM_SRGB, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, GL_RGBA, 4, 4, 1, 8, true)
_COMPR_TEX(BC2_UNORM, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_RGBA, 4, 4, 1, 16, false)
_COMPR_TEX(BC2_UNORM_SRGB, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, GL_RGBA, 4, 4, 1, 16, true)
_COMPR_TEX(BC3_UNORM, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_RGBA, 4, 4, 1, 16, false)
_COMPR_TEX(BC3_UNORM_SRGB, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, GL_RGBA, 4, 4, 1, 16, true)
#endif //DXGL_SUPPORT_S3TC_TEXTURES
#if !DXGLES
_COMPR_TEX(BC4_UNORM, GL_COMPRESSED_RED_RGTC1, GL_RED, 4, 4, 1, 8, false)
_COMPR_TEX(BC4_SNORM, GL_COMPRESSED_SIGNED_RED_RGTC1, GL_RED, 4, 4, 1, 8, false)
_COMPR_TEX(BC5_UNORM, GL_COMPRESSED_RG_RGTC2, GL_RG, 4, 4, 1, 16, false)
_COMPR_TEX(BC5_SNORM, GL_COMPRESSED_SIGNED_RG_RGTC2, GL_RG, 4, 4, 1, 16, false)
	#if DXGL_SUPPORT_BPTC
_COMPR_TEX(BC6H_UF16, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB, GL_RGB, 4, 4, 1, 16, false)
_COMPR_TEX(BC6H_SF16, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB, GL_RGB, 4, 4, 1, 16, false)
_COMPR_TEX(BC7_UNORM, GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, GL_RGBA, 4, 4, 1, 16, false)
_COMPR_TEX(BC7_UNORM_SRGB, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB, GL_RGBA, 4, 4, 1, 16, true)
	#endif //DXGL_SUPPORT_BPTC
#endif   //!DXGLES

#undef _TEX
#undef _UNCOMPR_TEX
#undef _COMPR_TEX

////////////////////////////////////////////////////////////////////////////////
// Default support for each format
////////////////////////////////////////////////////////////////////////////////

_UINT_MAP_DEFAULT(DefaultSupport, EGIFormat, 0)

#define _SUPPORT(_FormatID, _Mask) _UINT_MAP_VALUE(DefaultSupport, DXGL_GI_FORMAT(_FormatID), _Mask)

DXGL_TODO("Add here hardcoded default support masks (see _SUPPORT) for each format to use when format queries are not supported and for particular features")

#undef _SUPPORT

////////////////////////////////////////////////////////////////////////////////
// Typeless format mappings
////////////////////////////////////////////////////////////////////////////////

_UINT_MAP_DEFAULT(TypelessFormat, EGIFormat, eGIF_NUM)
_UINT_MAP_DEFAULT(TypelessConversion, EGIFormat, eGIFC_UNSUPPORTED)

#define _TYPED_CONVERT(_FormatID, _TypelessID, _Conversion)                               \
  _UINT_MAP_VALUE(TypelessFormat, DXGL_GI_FORMAT(_FormatID), DXGL_GI_FORMAT(_TypelessID)) \
  _UINT_MAP_VALUE(TypelessConversion, DXGL_GI_FORMAT(_FormatID), eGIFC_ ## _Conversion)
#define _TYPED_DEFAULT(_FormatID, _TypelessID)                                                                                            \
  _PTR_MAP_VALUE(STextureFormat, DXGL_GI_FORMAT(_TypelessID), &DXGL_TEXTURE_FORMAT(_FormatID))                                            \
  _PTR_MAP_VALUE(SUncompressedLayout, DXGL_GI_FORMAT(_TypelessID), _PTR_MAP(SUncompressedLayout) < DXGL_GI_FORMAT(_FormatID) > ::ms_pPtr) \
  _TYPED_CONVERT(_FormatID, _TypelessID, NONE)

_TYPED_DEFAULT(R32G32B32A32_FLOAT, R32G32B32A32_TYPELESS)
_TYPED_CONVERT(R32G32B32A32_UINT, R32G32B32A32_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R32G32B32A32_SINT, R32G32B32A32_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(R32G32B32_FLOAT, R32G32B32_TYPELESS)
_TYPED_CONVERT(R32G32B32_UINT, R32G32B32_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R32G32B32_SINT, R32G32B32_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(R16G16B16A16_FLOAT, R16G16B16A16_TYPELESS)
_TYPED_CONVERT(R16G16B16A16_UNORM, R16G16B16A16_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R16G16B16A16_UINT, R16G16B16A16_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R16G16B16A16_SNORM, R16G16B16A16_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R16G16B16A16_SINT, R16G16B16A16_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(R32G32_FLOAT, R32G32_TYPELESS)
_TYPED_CONVERT(R32G32_UINT, R32G32_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R32G32_SINT, R32G32_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(D32_FLOAT_S8X24_UINT, R32G8X24_TYPELESS)
_TYPED_CONVERT(R32_FLOAT_X8X24_TYPELESS, R32G8X24_TYPELESS, DEPTH_TO_RED)
_TYPED_CONVERT(X32_TYPELESS_G8X24_UINT, R32G8X24_TYPELESS, STENCIL_TO_RED)
_TYPED_DEFAULT(R10G10B10A2_UNORM, R10G10B10A2_TYPELESS)
_TYPED_CONVERT(R10G10B10A2_UINT, R10G10B10A2_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(R8G8B8A8_UNORM, R8G8B8A8_TYPELESS)
_TYPED_CONVERT(R8G8B8A8_UNORM_SRGB, R8G8B8A8_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R8G8B8A8_UINT, R8G8B8A8_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R8G8B8A8_SNORM, R8G8B8A8_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R8G8B8A8_SINT, R8G8B8A8_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(R16G16_FLOAT, R16G16_TYPELESS)
_TYPED_CONVERT(R16G16_UNORM, R16G16_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R16G16_UINT, R16G16_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R16G16_SNORM, R16G16_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R16G16_SINT, R16G16_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(D32_FLOAT, R32_TYPELESS)
_TYPED_CONVERT(R32_FLOAT, R32_TYPELESS, DEPTH_TO_RED)
_TYPED_DEFAULT(D24_UNORM_S8_UINT, R24G8_TYPELESS)
_TYPED_CONVERT(R24_UNORM_X8_TYPELESS, R24G8_TYPELESS, DEPTH_TO_RED)
_TYPED_CONVERT(X24_TYPELESS_G8_UINT, R24G8_TYPELESS, STENCIL_TO_RED)
_TYPED_DEFAULT(R8G8_UNORM, R8G8_TYPELESS)
_TYPED_CONVERT(R8G8_UINT, R8G8_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R8G8_SNORM, R8G8_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R8G8_SINT, R8G8_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(D16_UNORM, R16_TYPELESS)
_TYPED_CONVERT(R16_UNORM, R16_TYPELESS, DEPTH_TO_RED)
_TYPED_DEFAULT(R8_UNORM, R8_TYPELESS)
_TYPED_CONVERT(R8_UINT, R8_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R8_SNORM, R8_TYPELESS, TEXTURE_VIEW)
_TYPED_CONVERT(R8_SINT, R8_TYPELESS, TEXTURE_VIEW)
#if !DXGLES || (DXGLES && (GL_APPLE_texture_format_BGRA8888 || GL_IMG_read_format))
_TYPED_DEFAULT(B8G8R8A8_UNORM, B8G8R8A8_TYPELESS)
_TYPED_CONVERT(B8G8R8A8_UNORM_SRGB, B8G8R8A8_TYPELESS, TEXTURE_VIEW)
#endif //!DXGLES || (DXGLES && (GL_APPLE_texture_format_BGRA8888 || GL_IMG_read_format))
#if DXGL_SUPPORT_ETC2_EAC_COMPRESSION
_TYPED_DEFAULT(EAC_R11_UNORM, EAC_R11_TYPELESS)
_TYPED_CONVERT(EAC_R11_SNORM, EAC_R11_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(EAC_RG11_UNORM, EAC_RG11_TYPELESS)
_TYPED_CONVERT(EAC_RG11_SNORM, EAC_RG11_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(ETC2_UNORM, ETC2_TYPELESS)
_TYPED_CONVERT(ETC2_UNORM_SRGB, ETC2_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(ETC2A_UNORM, ETC2A_TYPELESS)
_TYPED_CONVERT(ETC2A_UNORM_SRGB, ETC2A_TYPELESS, TEXTURE_VIEW)
#endif //DXGL_SUPPORT_ETC2_EAC_COMPRESSION
#if !DXGLES
_TYPED_DEFAULT(B8G8R8X8_UNORM, B8G8R8X8_TYPELESS)
_TYPED_CONVERT(B8G8R8X8_UNORM_SRGB, B8G8R8X8_TYPELESS, TEXTURE_VIEW)
#endif //!DXGLES
#if DXGL_SUPPORT_S3TC_TEXTURES
_TYPED_DEFAULT(BC1_UNORM, BC1_TYPELESS)
_TYPED_CONVERT(BC1_UNORM_SRGB, BC1_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(BC2_UNORM, BC2_TYPELESS)
_TYPED_CONVERT(BC2_UNORM_SRGB, BC2_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(BC3_UNORM, BC3_TYPELESS)
_TYPED_CONVERT(BC3_UNORM_SRGB, BC3_TYPELESS, TEXTURE_VIEW)
#endif //DXGL_SUPPORT_S3TC_TEXTURES
#if !DXGLES
_TYPED_DEFAULT(BC4_UNORM, BC4_TYPELESS)
_TYPED_CONVERT(BC4_SNORM, BC4_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(BC5_UNORM, BC5_TYPELESS)
_TYPED_CONVERT(BC5_SNORM, BC5_TYPELESS, TEXTURE_VIEW)
	#if DXGL_SUPPORT_BPTC
_TYPED_DEFAULT(BC6H_UF16, BC6H_TYPELESS)
_TYPED_CONVERT(BC6H_SF16, BC6H_TYPELESS, TEXTURE_VIEW)
_TYPED_DEFAULT(BC7_UNORM, BC7_TYPELESS)
_TYPED_CONVERT(BC7_UNORM_SRGB, BC7_TYPELESS, TEXTURE_VIEW)
	#endif //DXGL_SUPPORT_BPTC
#endif   //!DXGLES

#undef _TYPED_DEFAULT
#undef _TYPED_CONVERT

////////////////////////////////////////////////////////////////////////////////
// Format information accessors
////////////////////////////////////////////////////////////////////////////////

static SGIFormatInfo g_akFormatInfo[] =
{
#define _FORMAT_INFO(_FormatID)                                                                            \
  {                                                                                                        \
    DXGL_DXGI_FORMAT(_FormatID),                                                                           \
    _UINT_MAP(DefaultSupport) < DXGL_GI_FORMAT(_FormatID) > ::VALUE,                                       \
    _PTR_MAP(STextureFormat) < DXGL_GI_FORMAT(_FormatID) > ::ms_pPtr,                                      \
    _PTR_MAP(SUncompressedLayout) < DXGL_GI_FORMAT(_FormatID) > ::ms_pPtr,                                 \
    static_cast<EGIFormat>(_UINT_MAP(TypelessFormat) < DXGL_GI_FORMAT(_FormatID) > ::VALUE),               \
    static_cast<EGIFormatConversion>(_UINT_MAP(TypelessConversion) < DXGL_GI_FORMAT(_FormatID) > ::VALUE), \
    _UINT_MAP(DXGITextureSwizzle) < DXGL_GI_FORMAT(_FormatID) > ::VALUE,                                   \
  },

	DXGL_GI_FORMATS(_FORMAT_INFO)

#undef _FORMAT_INFO
};

const SGIFormatInfo* GetGIFormatInfo(EGIFormat eGIFormat)
{
	assert(eGIFormat < eGIF_NUM);
	return &g_akFormatInfo[eGIFormat];
}

DXGI_FORMAT GetDXGIFormat(EGIFormat eGIFormat)
{
	const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(eGIFormat));
	return (pFormatInfo == NULL) ? DXGI_FORMAT_INVALID : pFormatInfo->m_eDXGIFormat;
}

EGIFormat GetGIFormat(DXGI_FORMAT eDXGIFormat)
{
	switch (eDXGIFormat)
	{
#define _FORMAT_CASE(_FormatID, ...) \
  case DXGL_DXGI_FORMAT(_FormatID):  \
    return DXGL_GI_FORMAT(_FormatID);

		DXGL_GI_FORMATS(_FORMAT_CASE)
#undef _FORMAT_CASE

	default:
		return eGIF_NUM;
	}
}

#undef _PTR_MAP_VALUE
#undef _PTR_MAP_DEFAULT
#undef _PTR_MAP
#undef _UINT_MAP_VALUE
#undef _UINT_MAP_DEFAULT
#undef _UINT_MAP

#undef DXGL_DXGI_FORMAT
#undef DXGL_UNCOMPRESSED_LAYOUT
#undef DXGL_TEXTURE_FORMAT
#undef DXGL_GL_INTERNAL_FORMAT
#undef DXGL_GI_CHANNEL_TYPE

////////////////////////////////////////////////////////////////////////////////
// Texture swizzle encoding
////////////////////////////////////////////////////////////////////////////////

void SwizzleMaskToRGBA(TSwizzleMask uMask, GLint aiRGBA[4])
{
	uint32 uChannel(4);
	while (uChannel--)
	{
		switch (uMask & 7)
		{
		case DXGL_SWIZZLE_RED:
			aiRGBA[uChannel] = GL_RED;
			break;
		case DXGL_SWIZZLE_GREEN:
			aiRGBA[uChannel] = GL_GREEN;
			break;
		case DXGL_SWIZZLE_BLUE:
			aiRGBA[uChannel] = GL_BLUE;
			break;
		case DXGL_SWIZZLE_ALPHA:
			aiRGBA[uChannel] = GL_ALPHA;
			break;
		case DXGL_SWIZZLE_ZERO:
			aiRGBA[uChannel] = GL_ZERO;
			break;
		case DXGL_SWIZZLE_ONE:
			aiRGBA[uChannel] = GL_ONE;
			break;
		default:
			assert(false);
		}
		uMask >>= 3;
	}
}

bool RGBAToSwizzleMask(const GLint aiRGBA[4], TSwizzleMask& uMask)
{
	uMask = 0;
	uint32 uChannel;
	for (uChannel = 0; uChannel < 4; ++uChannel, uMask <<= 3)
	{
		switch (aiRGBA[uChannel])
		{
		case GL_RED:
			uMask |= DXGL_SWIZZLE_RED;
			break;
		case GL_GREEN:
			uMask |= DXGL_SWIZZLE_GREEN;
			break;
		case GL_BLUE:
			uMask |= DXGL_SWIZZLE_BLUE;
			break;
		case GL_ALPHA:
			uMask |= DXGL_SWIZZLE_ALPHA;
			break;
		case GL_ZERO:
			uMask |= DXGL_SWIZZLE_ZERO;
			break;
		case GL_ONE:
			uMask |= DXGL_SWIZZLE_ONE;
			break;
		default:
			return false;
		}
	}
	return true;
}

#undef DXGL_SWIZZLE_RED
#undef DXGL_SWIZZLE_GREEN
#undef DXGL_SWIZZLE_BLUE
#undef DXGL_SWIZZLE_ALPHA
#undef DXGL_SWIZZLE_ZERO
#undef DXGL_SWIZZLE_ONE

bool GetImageFormat(EGIFormat eGIFormat, GLenum* peImageFormat)
{
	const SGIFormatInfo* pFormatInfo;
	DXGL_TODO("If possible add a specific list of valid image formats rather than re-using texture formats and checking that it's not compressed")
	if (eGIFormat == eGIF_NUM ||
	    (pFormatInfo = GetGIFormatInfo(eGIFormat)) == NULL ||
	    pFormatInfo->m_pTexture == NULL ||
	    pFormatInfo->m_pTexture->m_bCompressed)
		return false;

	*peImageFormat = (GLenum)pFormatInfo->m_pTexture->m_iInternalFormat;
	return true;
}

}
