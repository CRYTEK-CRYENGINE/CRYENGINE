// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../CommonRender.h" // CBaseResource, SResourceView, SSamplerState

struct SDepthTexture
{
	int              nWidth;
	int              nHeight;
	int              nFrameAccess;
	D3DTexture*      pTarget;
	D3DDepthSurface* pSurface;
	CTexture*        pTexture;

	SDepthTexture()
		: nWidth(0)
		, nHeight(0)
		, nFrameAccess(-1)
		, pTarget(nullptr)
		, pSurface(nullptr)
		, pTexture(nullptr)
	{}

	~SDepthTexture();

	void Release(bool bReleaseTexture);
	bool IsLocked() const;
};
