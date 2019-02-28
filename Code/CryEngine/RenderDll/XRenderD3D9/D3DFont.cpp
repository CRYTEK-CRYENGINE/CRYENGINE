// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"

//=========================================================================================

#include "../CryFont/FBitmap.h"

bool CD3D9Renderer::FontUpdateTexture(int nTexId, int nX, int nY, int USize, int VSize, byte* pSrcData)
{
	CTexture* tp = CTexture::GetByID(nTexId);
	assert(tp);

	if (tp)
	{
		tp->UpdateTextureRegion(pSrcData, nX, nY, 0, USize, VSize, 1, eTF_A8);

		return true;
	}
	return false;
}

bool CD3D9Renderer::FontUploadTexture(class CFBitmap* pBmp, ETEX_Format eSrcFormat)
{
	if (!pBmp)
	{
		return false;
	}

	unsigned int* pSrcData = new unsigned int[pBmp->GetWidth() * pBmp->GetHeight()];

	if (!pSrcData)
	{
		return false;
	}

	pBmp->Get32Bpp(&pSrcData);

	char szName[128];
	cry_sprintf(szName, "$AutoFont_%d", m_TexGenID++);

	int iFlags = FT_TEX_FONT | FT_DONT_STREAM | FT_DONT_RELEASE;
	CTexture* tp = CTexture::GetOrCreate2DTexture(szName, pBmp->GetWidth(), pBmp->GetHeight(), 1, iFlags, (unsigned char*)pSrcData, eSrcFormat);

	SAFE_DELETE_ARRAY(pSrcData);

	pBmp->SetRenderData((void*)tp);

	return true;
}

int CD3D9Renderer::FontCreateTexture(int Width, int Height, byte* pSrcData, ETEX_Format eSrcFormat, bool genMips)
{
	if (!pSrcData)
		return -1;

	char szName[128];
	cry_sprintf(szName, "$AutoFont_%d", m_TexGenID++);

	int iFlags = FT_TEX_FONT | FT_DONT_STREAM | FT_DONT_RELEASE;
	if (genMips)
		iFlags |= FT_FORCE_MIPS;
	CTexture* tp = CTexture::GetOrCreate2DTexture(szName, Width, Height, 1, iFlags, (unsigned char*)pSrcData, eSrcFormat);

	return tp->GetID();
}

void CD3D9Renderer::FontReleaseTexture(class CFBitmap* pBmp)
{
	if (!pBmp)
	{
		return;
	}

	CTexture* tp = (CTexture*)pBmp->GetRenderData();

	SAFE_RELEASE(tp);
}
