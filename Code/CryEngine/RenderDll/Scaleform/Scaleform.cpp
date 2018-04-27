// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "../Common/Renderer.h"

//////////////////////////////////////////////////////////////////////////
void CRenderer::FlashRender(IFlashPlayer_RenderProxy* pPlayer)
{
	m_pRT->RC_FlashRender(pPlayer);
}

void CRenderer::FlashRenderPlayer(IFlashPlayer* pPlayer)
{
	m_pRT->RC_FlashRenderPlayer(pPlayer);
}

void CRenderer::FlashRenderPlaybackLockless(IFlashPlayer_RenderProxy* pPlayer, int cbIdx, bool finalPlayback)
{
	m_pRT->RC_FlashRenderPlaybackLockless(pPlayer, cbIdx, finalPlayback);
}

void CRenderer::FlashRemoveTexture(ITexture* pTexture)
{
	pTexture->Release();
}
