// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "DepthTexture.h"
#include "Texture.h"

SDepthTexture::~SDepthTexture()
{
}

void SDepthTexture::Release(bool bReleaseTexture)
{
	if (bReleaseTexture && pTexture)
		pTexture->Release();

	pTarget = nullptr;
	pSurface = nullptr;
	pTexture = nullptr;
}

bool SDepthTexture::IsLocked() const
{
	return pTexture && pTexture->IsLocked();
}
