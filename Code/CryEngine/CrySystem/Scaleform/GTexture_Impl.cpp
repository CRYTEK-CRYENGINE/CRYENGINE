// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef INCLUDE_SCALEFORM_SDK

#include "GTexture_Impl.h"

	#include <CrySystem/Scaleform/GMemorySTLAlloc.h>
	#include <CryString/StringUtils.h>
	#include <GImage.h>

	#include <CryRenderer/IRenderer.h>
	#include <CrySystem/ISystem.h>
	#include <CrySystem/ILog.h>
	#include <vector>

	#include <CryCore/CryEndian.h>

	#ifndef RELEASE
		#include <CrySystem/Scaleform/IFlashUI.h>
	#endif

static inline ETEX_Format MapImageType(const GImageBase::ImageFormat& fmt)
{
	switch (fmt)
	{
	// MapImageType returns BGR/BGRA for RGB/RGBA for lack of RGB-enum
	case GImageBase::Image_ARGB_8888:
		return eTF_B8G8R8A8;
	case GImageBase::Image_RGB_888:
		return eTF_B8G8R8;
	case GImageBase::Image_L_8:
		return eTF_L8;
	case GImageBase::Image_A_8:
		return eTF_A8;
	case GImageBase::Image_DXT1:
		return eTF_BC1;
	case GImageBase::Image_DXT3:
		return eTF_BC2;
	case GImageBase::Image_DXT5:
		return eTF_BC3;
	default:
		return eTF_Unknown;
	}
}

//////////////////////////////////////////////////////////////////////////
// GTextureXRenderBase

	#if defined(ENABLE_FLASH_INFO)
volatile int32 GTextureXRenderBase::ms_textureMemoryUsed(0);
volatile int32 GTextureXRenderBase::ms_numTexturesCreated(0);
volatile int32 GTextureXRenderBase::ms_numTexturesCreatedTotal(0);
volatile int32 GTextureXRenderBase::ms_numTexturesCreatedTemp(0);
volatile int32 GTextureXRenderBase::ms_fontCacheTextureID(0);
	#endif

GTextureXRenderBase::GTextureXRenderBase(GRenderer* pRenderer, int32 texID /*= -1*/, ETEX_Format fmt /*= eTF_Unknown*/, bool bIsTemp /*= false*/)
	: m_texID(texID)
	, m_fmt(fmt)
	, m_userData(0)
	, m_pRenderer(pRenderer)
	#if defined(ENABLE_FLASH_INFO)
	, m_isTemp(bIsTemp)
	#endif
{
	assert(m_pRenderer);
	#if defined(ENABLE_FLASH_INFO)
	CryInterlockedIncrement(&ms_numTexturesCreated);
	if (bIsTemp)
		CryInterlockedIncrement(&ms_numTexturesCreatedTemp);
	else
		CryInterlockedIncrement(&ms_numTexturesCreatedTotal);
	#endif

	m_pRenderer->AddRef();
}

GTextureXRenderBase::~GTextureXRenderBase()
{
	#if defined(ENABLE_FLASH_INFO)
	CryInterlockedDecrement(&ms_numTexturesCreated);
	if (m_isTemp)
		CryInterlockedDecrement(&ms_numTexturesCreatedTemp);
	#endif

	m_pRenderer->Release();
}

GRenderer* GTextureXRenderBase::GetRenderer() const
{
	return m_pRenderer;
}

//////////////////////////////////////////////////////////////////////////
// GTextureXRender

GTextureXRender::GTextureXRender(GRenderer* pRenderer, int texID /*= -1*/, ETEX_Format fmt /*= eTF_Unknown*/) : GTextureXRenderBase(pRenderer, texID, fmt)
{
}

GTextureXRender::~GTextureXRender()
{
	if (m_texID > 0)
	{
		IRenderer* pRenderer(gEnv->pRenderer);
		ITexture* pTexture(pRenderer->EF_GetTextureByID(m_texID));
		assert(pTexture);
	#if defined(ENABLE_FLASH_INFO)
		if (!pTexture->IsShared())
		{
			int size = pTexture->GetDeviceDataSize();
			CryInterlockedAdd(&ms_textureMemoryUsed, -size);
		}
		if (ms_fontCacheTextureID == m_texID)
			ms_fontCacheTextureID = 0;
	#endif
		pRenderer->FlashRemoveTexture(pTexture);
	}
}

bool GTextureXRender::InitTexture(GImageBase* pIm, UInt /*usage*/)
{
	assert(m_texID == -1);
	if (InitTextureInternal(pIm))
	{
		IRenderer* pRenderer(gEnv->pRenderer);
		ITexture* pTexture(pRenderer->EF_GetTextureByID(m_texID));
		assert(pTexture && !pTexture->IsShared());
	#if defined(ENABLE_FLASH_INFO)
		int size = pTexture->GetDeviceDataSize();
		CryInterlockedAdd(&ms_textureMemoryUsed, size);
	#endif
		m_fmt = pTexture->GetTextureSrcFormat();
	}
	return m_texID > 0;
}

bool GTextureXRender::InitTextureFromFile(const char* pFilename)
{
	assert(m_texID == -1);

	string sFile = PathUtil::ToUnixPath(pFilename);
	sFile.replace("//", "/");

	IRenderer* pRenderer(gEnv->pRenderer);
	ICVar* pMipmapsCVar = gEnv->pConsole->GetCVar("sys_flash_mipmaps");
	uint32 mips_flag = pMipmapsCVar && pMipmapsCVar->GetIVal() ? 0 : FT_NOMIPS;

	ITexture* pTexture(pRenderer->EF_LoadTexture(sFile.c_str(), FT_DONT_STREAM | mips_flag));
	if (pTexture)
	{
	#ifndef RELEASE
		if (gEnv->pFlashUI)
		{
			gEnv->pFlashUI->CheckPreloadedTexture(pTexture);
		}
	#endif
		m_texID = pTexture->GetTextureID();
	#if defined(ENABLE_FLASH_INFO)
		if (!pTexture->IsShared())
		{
			int size = pTexture->GetDeviceDataSize();
			CryInterlockedAdd(&ms_textureMemoryUsed, size);
		}
	#endif
		m_fmt = pTexture->GetTextureSrcFormat();
	}
	return m_texID > 0;
}

bool GTextureXRender::InitTextureFromTexId(int texid)
{
	assert(m_texID == -1);
	IRenderer* pRenderer(gEnv->pRenderer);
	ITexture* pTexture(pRenderer->EF_GetTextureByID(texid));
	if (pTexture)
	{
		pTexture->AddRef();
		m_texID = pTexture->GetTextureID();
	#if defined(ENABLE_FLASH_INFO)
		if (!pTexture->IsShared())
		{
			int size = pTexture->GetDeviceDataSize();
			CryInterlockedAdd(&ms_textureMemoryUsed, size);
		}
	#endif
		m_fmt = pTexture->GetTextureSrcFormat();
	}
	return m_texID > 0;
}

bool GTextureXRender::InitTextureInternal(const GImageBase* pIm)
{
	ETEX_Format texFmt(MapImageType(pIm->Format));
	if (texFmt != eTF_Unknown)
	{
		return InitTextureInternal(texFmt, pIm->Width, pIm->Height, pIm->Pitch, pIm->pData);
	}
	else
	{
		gEnv->pLog->LogWarning("<Flash> GTextureXRender::InitTextureInternal( ... ) -- Attempted to load texture with unsupported texture format!\n");
		return false;
	}
}

bool GTextureXRender::InitTextureInternal(ETEX_Format texFmt, int32 width, int32 height, int32 pitch, uint8* pData)
{
	IRenderer* pRenderer(gEnv->pRenderer);

	bool bRGBA((pRenderer->GetFeatures() & RFT_RGBA) != 0 || pRenderer->GetRenderType() >= ERenderType::Direct3D11);
	bool bSwapRB(texFmt == eTF_B8G8R8 || texFmt == eTF_B8G8R8X8 || texFmt == eTF_B8G8R8A8);
	ETEX_Format texFmtOrig(texFmt);

	// expand RGB to RGBX if necessary
	std::vector<uint8, GMemorySTLAlloc<uint8>> expandRGB8;
	if (texFmt == eTF_B8G8R8)
	{
		expandRGB8.reserve(width * height * 4);
		for (int32 y(0); y < height; ++y)
		{
			uint8* pCol(&pData[pitch * y]);
			for (int32 x(0); x < width; ++x, pCol += 3)
			{
				expandRGB8.push_back(pCol[0]);
				expandRGB8.push_back(pCol[1]);
				expandRGB8.push_back(pCol[2]);
				expandRGB8.push_back(255);
			}
		}

		pData = &expandRGB8[0];
		pitch = width * 4;
		texFmt = eTF_B8G8R8X8;
	}

	if (bSwapRB)
	{
		// software-swap if no RGBA layout supported
		if (!bRGBA)
			SwapRB(pData, width, height, pitch);
		// otherwise swap by casting to swizzled format
		else if (texFmt == eTF_B8G8R8X8 || texFmt == eTF_B8G8R8A8)
			texFmt = eTF_R8G8B8A8;
		else if (texFmt == eTF_R8G8B8A8)
			texFmt = eTF_B8G8R8A8;
	}

	#if defined(NEED_ENDIAN_SWAP)
	if (texFmt == eTF_R8G8B8A8 || texFmt == eTF_B8G8R8A8 || texFmt == eTF_B8G8R8X8)
		SwapEndian(pData, width, height, pitch);
	#endif

	// Create texture...
	// Note that mip-maps should be generated for font textures (A8/L8) as
	// Scaleform generates font textures only once and relies on mip-mapping
	// to implement various font sizes.
	m_texID = pRenderer->SF_CreateTexture(width, height, (texFmt == eTF_A8 || texFmt == eTF_L8) ? 0 : 1, pData, texFmt, FT_DONT_STREAM);

	if (m_texID <= 0)
	{
		static const char* s_fomats[] =
		{
			"Unknown",
			"RGBA8",
			"RGB8->RGBA8",
			"L8",
			"A8",
			"DXT1",
			"DXT3",
			"DXT5",
			0
		};

		int fmtIdx(0);
		switch (texFmtOrig)
		{
		case eTF_B8G8R8A8:
			fmtIdx = 1;
			break;
		case eTF_B8G8R8:
			fmtIdx = 2;
			break;
		case eTF_L8:
			fmtIdx = 3;
			break;
		case eTF_A8:
			fmtIdx = 4;
			break;
		case eTF_BC1:
			fmtIdx = 5;
			break;
		case eTF_BC2:
			fmtIdx = 6;
			break;
		case eTF_BC3:
			fmtIdx = 7;
			break;
		}

		gEnv->pLog->LogWarning("<Flash> GTextureXRender::InitTextureInternal( ... ) "
		                       "-- Texture creation failed (%dx%d, %s)\n", width, height, s_fomats[fmtIdx]);
	}

	#if defined(NEED_ENDIAN_SWAP)
	if (texFmt == eTF_R8G8B8A8 || texFmt == eTF_B8G8R8A8 || texFmt == eTF_B8G8R8X8)
		SwapEndian(pData, width, height, pitch);
	#endif

	if (bSwapRB && !bRGBA)
		SwapRB(pData, width, height, pitch);

	return m_texID > 0;
}

bool GTextureXRender::InitDynamicTexture(int width, int height, GImage::ImageFormat format, int mipmaps, UInt usage)
{
	// Currently only used for font cache texture.
	if (Usage_Update == usage)
	{
		IRenderer* pRenderer(gEnv->pRenderer);
		assert(m_texID == -1);
		m_texID = pRenderer->SF_CreateTexture(width, height, mipmaps + 1, 0, MapImageType(format), FT_DONT_STREAM);
		if (m_texID > 0)
		{
			ITexture* pTexture(pRenderer->EF_GetTextureByID(m_texID));
			assert(pTexture && !pTexture->IsShared());
	#if defined(ENABLE_FLASH_INFO)
			int size = pTexture->GetDeviceDataSize();
			CryInterlockedAdd(&ms_textureMemoryUsed, size);
			assert(!ms_fontCacheTextureID);
			ms_fontCacheTextureID = m_texID;
	#endif
			m_fmt = pTexture->GetTextureSrcFormat();
		}
	}
	else
	{
		assert(0 && "GTextureXRender::InitDynamicTexture() -- Unsupported usage flag!");
	}
	return m_texID > 0;
}

void GTextureXRender::Update(int level, int n, const UpdateRect* pRects, const GImageBase* pIm)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	assert(m_texID > 0);
	if (!pRects || !n || !pIm || m_texID <= 0)
		return;

	PREFAST_SUPPRESS_WARNING(6255)
	IRenderer::SUpdateRect * pSrcRects((IRenderer::SUpdateRect*) alloca(n * sizeof(IRenderer::SUpdateRect)));
	if (pSrcRects)
	{
		for (int i(0); i < n; ++i)
			pSrcRects[i].Set(pRects[i].dest.x, pRects[i].dest.y, pRects[i].src.Left, pRects[i].src.Top, pRects[i].src.Width(), pRects[i].src.Height());

		IRenderer* pRenderer(gEnv->pRenderer);
		pRenderer->SF_UpdateTexture(m_texID, level, n, pSrcRects, pIm->pData, pIm->Pitch, pIm->DataSize, MapImageType(pIm->Format));
	}
}

int GTextureXRender::Map(int /*level*/, int /*n*/, MapRect* /*maps*/, int /*flags*/)
{
	assert(0 && "GTextureXRender::Map() -- Not implemented!");
	return 0;
}

bool GTextureXRender::Unmap(int /*level*/, int /*n*/, MapRect* /*maps*/, int /*flags*/)
{
	assert(0 && "GTextureXRender::Unmap() -- Not implemented!");
	return false;
}

void GTextureXRender::SwapRB(uint8* pImageData, uint32 width, uint32 height, uint32 pitch) const
{
	for (uint32 y(0); y < height; ++y)
	{
		uint8* pCol(&pImageData[pitch * y]);
		for (uint32 x(0); x < width; ++x, pCol += 4)
		{
			uint8 s(pCol[0]);
			pCol[0] = pCol[2];
			pCol[2] = s;
		}
	}
}

	#if defined(NEED_ENDIAN_SWAP)
void GTextureXRender::SwapEndian(uint8* pImageData, uint32 width, uint32 height, uint32 pitch) const
{
	for (uint32 y = 0; y < height; y++)
	{
		uint32* pCol((uint32*) &pImageData[pitch * y]);
		::SwapEndian(pCol, width);
	}
}
	#endif

//////////////////////////////////////////////////////////////////////////
// GTextureXRenderTempRT
GTextureXRenderTempRT::GTextureXRenderTempRT(GRenderer* pRenderer, int texID /*= -1*/, ETEX_Format fmt /*= eTF_Unknown*/) : GTextureXRender(pRenderer, texID, fmt)
{
}

GTextureXRenderTempRT::~GTextureXRenderTempRT()
{
}

//////////////////////////////////////////////////////////////////////////
// GTextureXRenderTempRTLockless
GTextureXRenderTempRTLockless::GTextureXRenderTempRTLockless(GRenderer* pRenderer) : GTextureXRenderBase(pRenderer, -1, eTF_Unknown, true)
	, m_pRT(0)
{
}

GTextureXRenderTempRTLockless::~GTextureXRenderTempRTLockless()
{
	m_pRT = 0;
}

//////////////////////////////////////////////////////////////////////////
// GTextureXRenderYUV

GTextureXRenderYUV::GTextureXRenderYUV(GRenderer* pRenderer) : GTextureXRenderBase(pRenderer)
	//, m_texIDs()
	, m_numIDs(0)
	, m_width(0)
	, m_height(0)
	, m_isStereoContent(false)
{
	m_texID = 0;
	m_texIDs[0] = m_texIDs[1] = m_texIDs[2] = m_texIDs[3] = -1;
}

GTextureXRenderYUV::~GTextureXRenderYUV()
{
	IRenderer* pRenderer = gEnv->pRenderer;
	for (int32 i = 0; i < m_numIDs; ++i)
	{
		int32 texId = m_texIDs[i];
		if (texId > 0)
		{
			ITexture* pTexture = pRenderer->EF_GetTextureByID(texId);
			assert(pTexture);
	#if defined(ENABLE_FLASH_INFO)
			if (!pTexture->IsShared())
			{
				int size = pTexture->GetDeviceDataSize();
				CryInterlockedAdd(&ms_textureMemoryUsed, -size);
			}
	#endif
			pRenderer->FlashRemoveTexture(pTexture);
		}
	}
}

bool GTextureXRenderYUV::InitTexture(GImageBase* /*pIm*/, UInt /*usage*/)
{
	assert(0 && "GTextureXRenderYUV::InitTexture() -- Not implemented!");
	return false;
}

bool GTextureXRenderYUV::InitTextureFromFile(const char* /*pFilename*/)
{
	assert(0 && "GTextureXRenderYUV::InitTextureFromFile() -- Not implemented!");
	return false;
}

bool GTextureXRenderYUV::InitTextureFromTexId(int /*texid*/)
{
	assert(0 && "GTextureXRenderYUV::InitTextureFromTexId() -- Not implemented!");
	return false;
}

void GTextureXRenderYUV::Clear()
{
	static const unsigned char clearVal[][4] = { {0,0,0,0}, {128,128,128,128}, {128,128,128,128}, {255,255,255,255} };

	const int level = 0; // currently don't have mips so only clear level 0

	IRenderer* pRenderer(gEnv->pRenderer);
	for (int32 i = 0; i < m_numIDs; ++i)
	{
		pRenderer->SF_ClearTexture(m_texIDs[i], level, 1, nullptr, clearVal[i]);
	}
}

bool GTextureXRenderYUV::InitDynamicTexture(int width, int height, GImage::ImageFormat format, int mipmaps, UInt usage)
{
	assert(format == GImageBase::Image_RGB_888 || format == GImageBase::Image_ARGB_8888);
	if (Usage_Map == usage && (format == GImageBase::Image_RGB_888 || format == GImageBase::Image_ARGB_8888))
	{
		IRenderer* pRenderer(gEnv->pRenderer);
		bool ok = true;

		ITexture* pTextures[4];
		int32 textureMemoryUsed = 0;
		const int32 numTex = format == GImage::Image_ARGB_8888 ? 4 : 3;
		for (int32 i = 0; i < numTex; ++i)
		{
			assert(m_texIDs[i] == -1);
			int w = Res(i, (uint32) width);
			int h = Res(i, (uint32) height);
			int32 texId = pRenderer->SF_CreateTexture(w, h, mipmaps + 1, 0, eTF_A8, FT_DONT_STREAM);
			ok = texId > 0;
			if (ok)
			{
				pTextures[i] = pRenderer->EF_GetTextureByID(texId);
				assert(pTextures[i] && !pTextures[i]->IsShared());
				textureMemoryUsed += pTextures[i]->GetDeviceDataSize();
				m_texIDs[i] = texId;
			}
			else
				break;
		}

		if (ok)
		{
			// Link YUVA planes circularly together (Y->U->V->A-> Y...)
			for (int32 i = 0; i < numTex; ++i)
				if (pTextures[i])
					pTextures[(i + (numTex - 1)) % numTex]->SetCustomID(m_texIDs[i]);

			m_width = (uint32) width;
			m_height = (uint32) height;
			m_numIDs = numTex;
			m_fmt = GImage::Image_ARGB_8888 ? eTF_YUVA : eTF_YUV;
	#if defined(ENABLE_FLASH_INFO)
			CryInterlockedAdd(&ms_textureMemoryUsed, textureMemoryUsed);
	#endif
			Clear();
		}
		else
		{
			for (int32 i = 0; i < numTex; ++i)
			{
				int32 texId = m_texIDs[i];
				if (texId > 0)
				{
					ITexture* pTexture = pRenderer->EF_GetTextureByID(texId);
					assert(pTexture);
					pRenderer->FlashRemoveTexture(pTexture);
				}
				else
					break;
			}
		}
	}
	else
	{
		assert(0 && "GTextureXRenderYUV::InitDynamicTexture() -- Unsupported usage flag or format!");
	}

	return m_numIDs > 0;
}

void GTextureXRenderYUV::Update(int /*level*/, int /*n*/, const UpdateRect* /*pRects*/, const GImageBase* /*pIm*/)
{
	assert(0 && "GTextureXRenderYUV::Update() -- Not implemented!");
}

int GTextureXRenderYUV::Map(int level, int n, MapRect* maps, int /*flags*/)
{
	assert(m_numIDs > 0 && maps && !level && n >= m_numIDs);
	if (m_numIDs <= 0 || !maps || level > 0 || n < m_numIDs)
		return 0;

	IRenderer* pRenderer(gEnv->pRenderer);
	bool ok = true;
	for (int32 i = 0; i < m_numIDs; ++i)
	{
		void* pBits = 0;
		uint32 pitch = m_width;
		ok = !!(pBits = CryModuleMemalign(m_width * m_height * sizeof(unsigned char), CRY_PLATFORM_ALIGNMENT));
		IF(ok, 1)
		{
			maps[i].width = Res(i, m_width);
			maps[i].height = Res(i, m_height);
			maps[i].pData = (UByte*) pBits;
			maps[i].pitch = pitch;
		}
		else
		{
			memset(&maps[i], 0, sizeof(MapRect) * (m_numIDs - i));
			break;
		}
	}
	return ok ? m_numIDs : 0;
}

bool GTextureXRenderYUV::Unmap(int level, int n, MapRect* maps, int /*flags*/)
{
	assert(m_numIDs > 0);
	if (m_numIDs <= 0)
		return false;

	if (!maps)
		return false;

	#if 0
	// 2x2 pixel checker board pattern to test 1:1 texel to pixel mapping for unscaled videos (i.e. video res = screen res)
	for (int32 i = 0; i < n; ++i)
	{
		for (uint32 y = 0; y < maps[i].height; ++y)
		{
			unsigned char* p = maps[i].pData + y * maps[i].pitch;
			if (i == 0)
			{
				for (uint32 x = 0; x < maps[i].width; ++x)
					*p++ = (unsigned char) (-(char) ((x + y) & 1));
			}
			else if (i == 1 || i == 2)
			{
				for (uint32 x = 0; x < maps[i].width; ++x)
					*p++ = 128;
			}
			else
			{
				for (uint32 x = 0; x < maps[i].width; ++x)
					*p++ = 255;
			}
		}
	}
	#endif

	IRenderer::SUpdateRect SrcRect;
	IRenderer* pRenderer(gEnv->pRenderer);
	for (int32 i = 0; i < m_numIDs; ++i)
	{
		IF(maps[i].pData, 1)
		{
			SrcRect.Set(0, 0, 0, 0, maps[i].width, maps[i].height);

			pRenderer->SF_UpdateTexture(m_texIDs[i], level, 1, &SrcRect, maps[i].pData, maps[i].pitch, maps[i].width * maps[i].height * sizeof(unsigned char), eTF_A8);

			CryModuleMemalignFree(maps[i].pData);
			maps[i].pData = nullptr;
		}
	}
	return true;
}

#endif // #ifdef INCLUDE_SCALEFORM_SDK
