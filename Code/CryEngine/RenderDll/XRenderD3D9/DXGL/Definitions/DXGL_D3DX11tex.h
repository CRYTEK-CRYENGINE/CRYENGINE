// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DXGL_D3DX11tex.h
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Contains portable definition of structs and enums to match
//               those in D3DX11tex.h in the DirectX SDK
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DXGL_D3DX11tex_h__
#define __DXGL_D3DX11tex_h__

////////////////////////////////////////////////////////////////////////////
//  Enums
////////////////////////////////////////////////////////////////////////////

typedef enum D3DX11_FILTER_FLAG
{
	D3DX11_FILTER_NONE             = (1 << 0),
	D3DX11_FILTER_POINT            = (2 << 0),
	D3DX11_FILTER_LINEAR           = (3 << 0),
	D3DX11_FILTER_TRIANGLE         = (4 << 0),
	D3DX11_FILTER_BOX              = (5 << 0),

	D3DX11_FILTER_MIRROR_U         = (1 << 16),
	D3DX11_FILTER_MIRROR_V         = (2 << 16),
	D3DX11_FILTER_MIRROR_W         = (4 << 16),
	D3DX11_FILTER_MIRROR           = (7 << 16),

	D3DX11_FILTER_DITHER           = (1 << 19),
	D3DX11_FILTER_DITHER_DIFFUSION = (2 << 19),

	D3DX11_FILTER_SRGB_IN          = (1 << 21),
	D3DX11_FILTER_SRGB_OUT         = (2 << 21),
	D3DX11_FILTER_SRGB             = (3 << 21),
} D3DX11_FILTER_FLAG;

typedef enum D3DX11_NORMALMAP_FLAG
{
	D3DX11_NORMALMAP_MIRROR_U          = (1 << 16),
	D3DX11_NORMALMAP_MIRROR_V          = (2 << 16),
	D3DX11_NORMALMAP_MIRROR            = (3 << 16),
	D3DX11_NORMALMAP_INVERTSIGN        = (8 << 16),
	D3DX11_NORMALMAP_COMPUTE_OCCLUSION = (16 << 16),
} D3DX11_NORMALMAP_FLAG;

typedef enum D3DX11_CHANNEL_FLAG
{
	D3DX11_CHANNEL_RED       = (1 << 0),
	D3DX11_CHANNEL_BLUE      = (1 << 1),
	D3DX11_CHANNEL_GREEN     = (1 << 2),
	D3DX11_CHANNEL_ALPHA     = (1 << 3),
	D3DX11_CHANNEL_LUMINANCE = (1 << 4),
} D3DX11_CHANNEL_FLAG;

typedef enum D3DX11_IMAGE_FILE_FORMAT
{
	D3DX11_IFF_BMP         = 0,
	D3DX11_IFF_JPG         = 1,
	D3DX11_IFF_PNG         = 3,
	D3DX11_IFF_DDS         = 4,
	D3DX11_IFF_TIFF        = 10,
	D3DX11_IFF_GIF         = 11,
	D3DX11_IFF_WMP         = 12,
	D3DX11_IFF_FORCE_DWORD = 0x7fffffff

} D3DX11_IMAGE_FILE_FORMAT;

typedef enum D3DX11_SAVE_TEXTURE_FLAG
{
	D3DX11_STF_USEINPUTBLOB = 0x0001,
} D3DX11_SAVE_TEXTURE_FLAG;

////////////////////////////////////////////////////////////////////////////
//  Structs
////////////////////////////////////////////////////////////////////////////

typedef struct D3DX11_IMAGE_INFO
{
	UINT                     Width;
	UINT                     Height;
	UINT                     Depth;
	UINT                     ArraySize;
	UINT                     MipLevels;
	UINT                     MiscFlags;
	DXGI_FORMAT              Format;
	D3D11_RESOURCE_DIMENSION ResourceDimension;
	D3DX11_IMAGE_FILE_FORMAT ImageFileFormat;
} D3DX11_IMAGE_INFO;

typedef struct D3DX11_IMAGE_LOAD_INFO
{
	UINT               Width;
	UINT               Height;
	UINT               Depth;
	UINT               FirstMipLevel;
	UINT               MipLevels;
	D3D11_USAGE        Usage;
	UINT               BindFlags;
	UINT               CpuAccessFlags;
	UINT               MiscFlags;
	DXGI_FORMAT        Format;
	UINT               Filter;
	UINT               MipFilter;
	D3DX11_IMAGE_INFO* pSrcInfo;
} D3DX11_IMAGE_LOAD_INFO;

typedef struct _D3DX11_TEXTURE_LOAD_INFO
{
	D3D11_BOX* pSrcBox;
	D3D11_BOX* pDstBox;
	UINT       SrcFirstMip;
	UINT       DstFirstMip;
	UINT       NumMips;
	UINT       SrcFirstElement;
	UINT       DstFirstElement;
	UINT       NumElements;
	UINT       Filter;
	UINT       MipFilter;
} D3DX11_TEXTURE_LOAD_INFO;

#endif //__DXGL_D3DX11tex_h__
