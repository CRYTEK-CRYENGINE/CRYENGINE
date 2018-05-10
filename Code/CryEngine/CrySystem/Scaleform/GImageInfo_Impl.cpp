// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef INCLUDE_SCALEFORM_SDK

	#include <CrySystem/ISystem.h>
	#include <CrySystem/Scaleform/IFlashPlayer.h>
	#include "GTexture_Impl.h"
	#include "GImageInfo_Impl.h"

//////////////////////////////////////////////////////////////////////////

GImageInfoXRender::GImageInfoXRender(GImage* pImage)
	: m_width(pImage ? pImage->Width : 0)
	, m_height(pImage ? pImage->Height : 0)
	, m_pImg(pImage)
	, m_pTex(0)
{
}

GImageInfoXRender::~GImageInfoXRender()
{
	m_pImg = 0;
	m_pTex = 0;
}

UInt GImageInfoXRender::GetWidth() const
{
	return m_width;
}

UInt GImageInfoXRender::GetHeight() const
{
	return m_height;
}

	#if defined(GFX_AMP_SERVER)
UPInt GImageInfoXRender::GetBytes() const
{
	return 0; // proper AMP Server support might come later
}

bool GImageInfoXRender::IsExternal() const
{
	return true; // proper AMP Server support might come later
}

UPInt GImageInfoXRender::GetExternalBytes() const
{
	return true; // proper AMP Server support might come later
}

UInt32 GImageInfoXRender::GetImageId() const
{
	return 0; // proper AMP Server support might come later
}

GImage* GImageInfoXRender::GetImage() const
{
	return NULL; // proper AMP Server support might come later
}
	#endif

GTexture* GImageInfoXRender::GetTexture(GRenderer* pRenderer)
{
	if (!m_pTex)
	{
		if (m_pImg)
		{
			GPtr<GTexture> pTex = *pRenderer->CreateTexture();
			if (pTex && pTex->InitTexture(m_pImg))
			{
				m_pTex = pTex;
				m_pImg = 0;
			}
		}
	}
	return m_pTex;
}

//////////////////////////////////////////////////////////////////////////

GImageInfoFileXRender::GImageInfoFileXRender(const char* pImgFilePath, uint32 targetWidth, uint32 targetHeight)
	: m_targetWidth(targetWidth)
	, m_targetHeight(targetHeight)
	, m_imgFilePath(pImgFilePath)
	, m_pTex(0)
{
}

GImageInfoFileXRender::~GImageInfoFileXRender()
{
	m_pTex = 0;
}

UInt GImageInfoFileXRender::GetWidth() const
{
	return m_targetWidth;
}

UInt GImageInfoFileXRender::GetHeight() const
{
	return m_targetHeight;
}

	#if defined(GFX_AMP_SERVER)
UPInt GImageInfoFileXRender::GetBytes() const
{
	return 0; // proper AMP Server support might come later
}

bool GImageInfoFileXRender::IsExternal() const
{
	return true; // proper AMP Server support might come later
}

UPInt GImageInfoFileXRender::GetExternalBytes() const
{
	return true; // proper AMP Server support might come later
}

UInt32 GImageInfoFileXRender::GetImageId() const
{
	return 0; // proper AMP Server support might come later
}

GImage* GImageInfoFileXRender::GetImage() const
{
	return NULL; // proper AMP Server support might come later
}
	#endif

GTexture* GImageInfoFileXRender::GetTexture(GRenderer* pRenderer)
{
	if (!m_pTex)
	{
		if (m_imgFilePath.c_str() && m_imgFilePath.c_str()[0] != '\0')
		{
			GPtr<GTexture> pTex = *pRenderer->CreateTexture();
			if (pTex && ((GTextureXRenderBase*)pTex.GetPtr())->InitTextureFromFile(m_imgFilePath)) // InitTextureFromFile() no longer part of GTexture interface! :( TODO: find better way to avoid cast
				m_pTex = pTex;
		}
	}
	return m_pTex;
}

//////////////////////////////////////////////////////////////////////////

GImageInfoTextureXRender::GImageInfoTextureXRender(ITexture* pTexture)
	: m_width(pTexture ? pTexture->GetWidth() : 0)
	, m_height(pTexture ? pTexture->GetHeight() : 0)
	, m_texId(pTexture ? pTexture->GetTextureID() : -1)
	, m_pTex(0)
{
}

GImageInfoTextureXRender::GImageInfoTextureXRender(ITexture* pTexture, int width, int height)
	: m_width(pTexture ? width : 0)
	, m_height(pTexture ? height : 0)
	, m_texId(pTexture ? pTexture->GetTextureID() : -1)
	, m_pTex(0)
{
}

GImageInfoTextureXRender::~GImageInfoTextureXRender()
{
	m_pTex = 0;
}

UInt GImageInfoTextureXRender::GetWidth() const
{
	return m_width;
}

UInt GImageInfoTextureXRender::GetHeight() const
{
	return m_height;
}

	#if defined(GFX_AMP_SERVER)
UPInt GImageInfoTextureXRender::GetBytes() const
{
	return 0; // proper AMP Server support might come later
}

bool GImageInfoTextureXRender::IsExternal() const
{
	return true; // proper AMP Server support might come later
}

UPInt GImageInfoTextureXRender::GetExternalBytes() const
{
	return true; // proper AMP Server support might come later
}

UInt32 GImageInfoTextureXRender::GetImageId() const
{
	return 0; // proper AMP Server support might come later
}

GImage* GImageInfoTextureXRender::GetImage() const
{
	return NULL; // proper AMP Server support might come later
}
	#endif

GTexture* GImageInfoTextureXRender::GetTexture(GRenderer* pRenderer)
{
	if (!m_pTex)
	{
		if (m_texId > 0)
		{
			GPtr<GTexture> pTex = *pRenderer->CreateTexture();
			if (pTex && ((GTextureXRenderBase*)pTex.GetPtr())->InitTextureFromTexId(m_texId))
				m_pTex = pTex;
		}
	}
	return m_pTex;
}

//////////////////////////////////////////////////////////////////////////

GImageInfoILMISrcXRender::GImageInfoILMISrcXRender(IFlashLoadMovieImage* pSrc)
	: m_width(pSrc ? pSrc->GetWidth() : 0)
	, m_height(pSrc ? pSrc->GetHeight() : 0)
	, m_pSrc(pSrc)
	, m_pTex(0)
{
}

GImageInfoILMISrcXRender::~GImageInfoILMISrcXRender()
{
	SAFE_RELEASE(m_pSrc);
	m_pTex = 0;
}

UInt GImageInfoILMISrcXRender::GetWidth() const
{
	return m_width;
}

UInt GImageInfoILMISrcXRender::GetHeight() const
{
	return m_height;
}

	#if defined(GFX_AMP_SERVER)
UPInt GImageInfoILMISrcXRender::GetBytes() const
{
	return 0; // proper AMP Server support might come later
}

bool GImageInfoILMISrcXRender::IsExternal() const
{
	return true; // proper AMP Server support might come later
}

UPInt GImageInfoILMISrcXRender::GetExternalBytes() const
{
	return true; // proper AMP Server support might come later
}

UInt32 GImageInfoILMISrcXRender::GetImageId() const
{
	return 0; // proper AMP Server support might come later
}

GImage* GImageInfoILMISrcXRender::GetImage() const
{
	return NULL; // proper AMP Server support might come later
}
	#endif

GTexture* GImageInfoILMISrcXRender::GetTexture(GRenderer* pRenderer)
{
	if (!m_pTex)
	{
		if (m_pSrc)
		{
			IFlashLoadMovieImage::EFmt fmt(m_pSrc->GetFormat());
			if (fmt == IFlashLoadMovieImage::eFmt_ARGB_8888 || fmt == IFlashLoadMovieImage::eFmt_RGB_888)
			{
				GImageBase img(fmt == IFlashLoadMovieImage::eFmt_ARGB_8888 ? GImage::Image_ARGB_8888 : GImage::Image_RGB_888,
				               m_pSrc->GetWidth(), m_pSrc->GetHeight(), m_pSrc->GetPitch());
				img.pData = (UByte*) m_pSrc->GetPtr();

				GPtr<GTexture> pTex = *pRenderer->CreateTexture();
				if (pTex && pTex->InitTexture(&img))
				{
					m_pTex = pTex;
					SAFE_RELEASE(m_pSrc);
				}
			}
		}
	}
	return m_pTex;
}

#endif // #ifdef INCLUDE_SCALEFORM_SDK
