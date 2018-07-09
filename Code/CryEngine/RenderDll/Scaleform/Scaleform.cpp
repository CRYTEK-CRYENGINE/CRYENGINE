// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "../Common/Renderer.h"
#include <memory>

//////////////////////////////////////////////////////////////////////////
void CRenderer::FlashRender(std::shared_ptr<IFlashPlayer_RenderProxy> &&pPlayer)
{
	m_pRT->RC_FlashRender(std::move(pPlayer));
}

void CRenderer::FlashRenderPlayer(std::shared_ptr<IFlashPlayer> &&pPlayer)
{
	m_pRT->RC_FlashRenderPlayer(std::move(pPlayer));
}

void CRenderer::FlashRenderPlaybackLockless(std::shared_ptr<IFlashPlayer_RenderProxy> &&pPlayer, int cbIdx, bool finalPlayback)
{
	m_pRT->RC_FlashRenderPlaybackLockless(std::move(pPlayer), cbIdx, finalPlayback);
}

void CRenderer::FlashRemoveTexture(ITexture* pTexture)
{
	pTexture->Release();
}
