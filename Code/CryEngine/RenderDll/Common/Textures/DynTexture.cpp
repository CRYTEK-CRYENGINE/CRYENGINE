// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   DynTexture.cpp : Common dynamic texture manager implementation.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include <DriverD3D.h>

//======================================================================
// Dynamic textures
SDynTexture SDynTexture::s_Root("Root");
size_t SDynTexture::s_nMemoryOccupied;
size_t SDynTexture::s_iNumTextureBytesCheckedOut;
size_t SDynTexture::s_iNumTextureBytesCheckedIn;

SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_BC1;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_BC1;
SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_R8G8B8A8;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_R8G8B8A8;
SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_R32F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_R32F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_R16F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_R16F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_R16G16F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_R16G16F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_R8G8B8A8S;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_R8G8B8A8S;
SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_R16G16B16A16F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_R16G16B16A16F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_R10G10B10A2;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_R10G10B10A2;
SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_R11G11B10F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_R11G11B10F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_R11G11B10F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_R11G11B10F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_R8G8S;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_R8G8S;
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_R8G8S;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_R8G8S;

SDynTexture::TextureSet SDynTexture::s_availableTexturePool2D_Shadows[SBP_MAX];
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2D_Shadows[SBP_MAX];
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_Shadows[SBP_MAX];
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_Shadows[SBP_MAX];

SDynTexture::TextureSet SDynTexture::s_availableTexturePool2DCustom_R16G16F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePool2DCustom_R16G16F;

SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_BC1;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_BC1;
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_R8G8B8A8;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_R8G8B8A8;
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_R32F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_R32F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_R16F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_R16F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_R16G16F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_R16G16F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_R8G8B8A8S;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_R8G8B8A8S;
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_R16G16B16A16F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_R16G16B16A16F;
SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCube_R10G10B10A2;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCube_R10G10B10A2;

SDynTexture::TextureSet SDynTexture::s_availableTexturePoolCubeCustom_R16G16F;
SDynTexture::TextureSubset SDynTexture::s_checkedOutTexturePoolCubeCustom_R16G16F;

uint32 SDynTexture::s_SuggestedDynTexMaxSize;
uint32 SDynTexture::s_CurDynTexMaxSize;

//======================================================================

/*void SDynTexture::DrawStat(TextureSet pSet)
   -> {
   ->   TextureSetItor itor=pSet->begin();
   ->   while (itor!=pSet->end())
   ->   {
   ->     TextureSubset *pSubset = itor->second;
   ->     TextureSubsetItor itorss = pSubset->begin();
   ->     while (itorss!=pSubset->end())
   ->     {
   ->       CTexture *pTex = itorss->second;
   ->       if (!bOldOnly || (pTex->m_nAccessFrameID<nFrame && pTex->m_nUpdateFrameID<nFrame))
   ->       {
   ->         itorss->second = NULL;
   ->         pSubset->erase(itorss);
   ->         itorss = pSubset->begin();
   ->         nSpace -= pTex->GetDataSize();
   ->         m_iNumTextureBytesCheckedIn -= pTex->GetDataSize();
   ->         SAFE_RELEASE(pTex);
   ->         if (nNeedSpace+nSpace < CRenderer::CV_r_dyntexmaxsize*1024*1024)
   ->           break;
   ->       }
   ->       else
   ->         itorss++;
   ->     }
   ->     if (pSubset->size() == 0)
   ->     {
   ->       delete pSubset;
   ->       pSet->erase(itor);
   ->       itor = pSet->begin();
   ->     }
   ->     else
   ->       itor++;
   ->     if (nNeedSpace+nSpace < CRenderer::CV_r_dyntexmaxsize*1024*1024)
   ->       break;
   ->   }
   -> }*/

SDynTexture::SDynTexture(const char* szSource)
{
	m_nWidth = 0;
	m_nHeight = 0;
	m_nReqWidth = m_nWidth;
	m_nReqHeight = m_nHeight;
	m_pTexture = NULL;
	m_eTF = eTF_Unknown;
	m_eTT = eTT_2D;
	m_nTexFlags = 0;
	cry_strcpy(m_sSource, szSource);
	m_bLocked = false;
	m_nUpdateMask = 0;
	if (gRenDev)
		m_nFrameReset = gRenDev->m_nFrameReset;

	m_Next = NULL;
	m_Prev = NULL;
	if (!s_Root.m_Next)
	{
		s_Root.m_Next = &s_Root;
		s_Root.m_Prev = &s_Root;
	}
	AdjustRealSize();
}

SDynTexture::SDynTexture(int nWidth, int nHeight, ColorF clearValue, ETEX_Format eTF, ETEX_Type eTT, int nTexFlags, const char* szSource)
{
	CRY_ASSERT(nTexFlags & (FT_USAGE_DEPTHSTENCIL | FT_USAGE_RENDERTARGET));
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_nReqWidth = m_nWidth;
	m_nReqHeight = m_nHeight;
	m_pTexture = NULL;
	m_eTF = eTF;
	m_eTT = eTT;
	m_clearValue = clearValue;
	m_nTexFlags = nTexFlags;
	cry_strcpy(m_sSource, szSource);
	m_bLocked = false;
	m_nUpdateMask = 0;
	m_pFrustumOwner = NULL;
	if (gRenDev)
		m_nFrameReset = gRenDev->m_nFrameReset;

	m_Next = NULL;
	m_Prev = NULL;
	if (!s_Root.m_Next)
	{
		s_Root.m_Next = &s_Root;
		s_Root.m_Prev = &s_Root;
	}
	Link();
	AdjustRealSize();
}

SDynTexture::~SDynTexture()
{
	if (m_pTexture)
		ReleaseDynamicRT(false);
	m_pTexture = NULL;
	Unlink();
}

int SDynTexture::GetTextureID()
{
	return m_pTexture ? m_pTexture->GetTextureID() : 0;
}

bool SDynTexture::FreeTextures(bool bOldOnly, size_t nNeedSpace)
{
	bool bFreed = false;
	if (bOldOnly)
	{
		SDynTexture* pTX = SDynTexture::s_Root.m_Prev;
		int nFrame = gRenDev->GetRenderFrameID();
		while (nNeedSpace + s_nMemoryOccupied > SDynTexture::s_CurDynTexMaxSize * 1024 * 1024)
		{
			if (pTX == &SDynTexture::s_Root)
				break;
			SDynTexture* pNext = pTX->m_Prev;
			{
				// We cannot unload locked texture or texture used in current frame
				// Better to increase pool size temporarily
				if (pTX->m_pTexture && !pTX->m_pTexture->IsLocked())
				{
					if (pTX->m_pTexture->m_nAccessFrameID < nFrame && pTX->m_pTexture->m_nUpdateFrameID < nFrame && !pTX->m_bLocked)
						pTX->ReleaseDynamicRT(true);
				}
			}
			pTX = pNext;
		}
		if (nNeedSpace + s_nMemoryOccupied < SDynTexture::s_CurDynTexMaxSize * 1024 * 1024)
			return true;
	}
	if (!bFreed)
		bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT == eTT_2D ? &s_availableTexturePool2D_BC1 : &s_availableTexturePoolCube_BC1, bOldOnly);
	if (!bFreed)
		bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT == eTT_2D ? &s_availableTexturePool2D_R32F : &s_availableTexturePoolCube_R32F, bOldOnly);
	if (!bFreed)
		bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT == eTT_2D ? &s_availableTexturePool2D_R11G11B10F : &s_availableTexturePoolCube_R11G11B10F, bOldOnly);

	//if (!bFreed && m_eTT==eTT_2D)
	//bFreed = FreeAvailableDynamicRT(nNeedSpace, &m_availableTexturePool2D_ATIDF24);

	//First pass - Free textures from the pools with the same texture types
	//shadows pools
	for (int nPool = SBP_D16; nPool < SBP_MAX; nPool++)
	{
		if (!bFreed && m_eTT == eTT_2D)
			bFreed = FreeAvailableDynamicRT(nNeedSpace, &s_availableTexturePool2D_Shadows[nPool], bOldOnly);
	}
	for (int nPool = SBP_D16; nPool < SBP_MAX; nPool++)
	{
		if (!bFreed && m_eTT != eTT_2D)
			bFreed = FreeAvailableDynamicRT(nNeedSpace, &s_availableTexturePoolCube_Shadows[nPool], bOldOnly);
	}

	if (!bFreed)
		bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT == eTT_2D ? &s_availableTexturePoolCube_BC1 : &s_availableTexturePool2D_BC1, bOldOnly);
	if (!bFreed)
		bFreed = FreeAvailableDynamicRT(nNeedSpace, m_eTT == eTT_2D ? &s_availableTexturePoolCube_R32F : &s_availableTexturePool2D_R32F, bOldOnly);
	//if (!bFreed && m_eTT!=eTT_2D)
	//	bFreed = FreeAvailableDynamicRT(nNeedSpace, &m_availableTexturePool2D_ATIDF24);

	//Second pass - Free textures from the pools with the different texture types
	//shadows pools
	for (int nPool = SBP_D16; nPool < SBP_MAX; nPool++)
	{
		if (!bFreed && m_eTT != eTT_2D)
			bFreed = FreeAvailableDynamicRT(nNeedSpace, &s_availableTexturePool2D_Shadows[nPool], bOldOnly);
	}
	for (int nPool = SBP_D16; nPool < SBP_MAX; nPool++)
	{
		if (!bFreed && m_eTT == eTT_2D)
			bFreed = FreeAvailableDynamicRT(nNeedSpace, &s_availableTexturePoolCube_Shadows[nPool], bOldOnly);
	}
	return bFreed;
}

bool SDynTexture::Update(int nNewWidth, int nNewHeight)
{
	gRenDev->ExecuteRenderThreadCommand(
		[=]{ this->RT_Update(nNewWidth, nNewHeight); },
		ERenderCommandFlags::None);
	return true;
}

bool SDynTexture::RT_Update(int nNewWidth, int nNewHeight)
{
	CRY_PROFILE_SECTION(PROFILE_RENDERER, "SDynTexture::RT_Update");

	Unlink();

	assert(s_iNumTextureBytesCheckedOut + s_iNumTextureBytesCheckedIn == s_nMemoryOccupied);

	if (nNewWidth != m_nReqWidth || nNewHeight != m_nReqHeight)
	{
		if (m_pTexture)
			ReleaseDynamicRT(false);
		m_pTexture = NULL;

		m_nReqWidth = nNewWidth;
		m_nReqHeight = nNewHeight;

		AdjustRealSize();
	}

	if (!m_pTexture)
	{
		size_t nNeedSpace = CTexture::TextureDataSize(m_nWidth, m_nHeight, 1, 1, m_eTT == eTT_Cube ? 6 : 1, m_eTF, eTM_Optimal);
		SDynTexture* pTX = SDynTexture::s_Root.m_Prev;
		if (nNeedSpace + s_nMemoryOccupied > SDynTexture::s_CurDynTexMaxSize * 1024 * 1024)
		{
			m_pTexture = GetDynamicRT();
			if (!m_pTexture)
			{
				bool bFreed = FreeTextures(true, nNeedSpace);
				if (!bFreed)
					bFreed = FreeTextures(false, nNeedSpace);

				if (!bFreed)
				{
					pTX = SDynTexture::s_Root.m_Next;
					int nFrame = gRenDev->GetRenderFrameID() - 1;
					while (nNeedSpace + s_nMemoryOccupied > SDynTexture::s_CurDynTexMaxSize * 1024 * 1024)
					{
						if (pTX == &SDynTexture::s_Root)
						{
							static int nThrash;
							if (nThrash != nFrame)
							{
								nThrash = nFrame;
								iLog->Log("Error: Dynamic textures thrashing (try to increase texture pool size - r_DynTexMaxSize)...");
							}
							break;
						}
						SDynTexture* pNext = pTX->m_Next;
						// We cannot unload locked texture or texture used in current frame
						// Better to increase pool size temporarily
						if (pTX->m_pTexture && !pTX->m_pTexture->IsLocked())
						{
							if (pTX->m_pTexture->m_nAccessFrameID < nFrame && pTX->m_pTexture->m_nUpdateFrameID < nFrame && !pTX->m_bLocked)
								pTX->ReleaseDynamicRT(true);
						}
						pTX = pNext;
					}
				}
			}
		}
	}
	if (!m_pTexture)
		m_pTexture = CreateDynamicRT();

	assert(s_iNumTextureBytesCheckedOut + s_iNumTextureBytesCheckedIn == s_nMemoryOccupied);

	if (m_pTexture)
	{
		Link();
		return true;
	}
	return false;
}

void SDynTexture::ShutDown()
{
	SDynTexture* pTX, * pTXNext;
	for (pTX = SDynTexture::s_Root.m_Next; pTX != &SDynTexture::s_Root; pTX = pTXNext)
	{
		pTXNext = pTX->m_Next;
		SAFE_RELEASE_FORCE(pTX);
	}
	SDynTexture Tex("Release");
	Tex.m_eTT = eTT_2D;
	Tex.FreeTextures(false, 1024 * 1024 * 1024);

	Tex.m_eTT = eTT_Cube;
	Tex.FreeTextures(false, 1024 * 1024 * 1024);
}

bool SDynTexture::FreeAvailableDynamicRT(size_t nNeedSpace, TextureSet* pSet, bool bOldOnly)
{
	assert(s_iNumTextureBytesCheckedOut + s_iNumTextureBytesCheckedIn == s_nMemoryOccupied);

	size_t nSpace = s_nMemoryOccupied;
	int nFrame = gRenDev->GetRenderFrameID() - 400;
	while (nNeedSpace + nSpace > SDynTexture::s_CurDynTexMaxSize * 1024 * 1024)
	{
		auto itor = pSet->begin();
		while (itor != pSet->end())
		{
			TextureSubset* pSubset = itor->second;
			auto itorss = pSubset->begin();
			while (itorss != pSubset->end())
			{
				CTexture* pTex = itorss->second;
				PREFAST_ASSUME(pTex);
				if (!bOldOnly || (pTex->m_nAccessFrameID < nFrame && pTex->m_nUpdateFrameID < nFrame))
				{
					itorss->second = NULL;
					pSubset->erase(itorss);
					itorss = pSubset->begin();
					nSpace -= pTex->GetDataSize();
					s_iNumTextureBytesCheckedIn -= pTex->GetDataSize();
					SAFE_RELEASE(pTex);
					if (nNeedSpace + nSpace < SDynTexture::s_CurDynTexMaxSize * 1024 * 1024)
						break;
				}
				else
					itorss++;
			}
			if (pSubset->size() == 0)
			{
				delete pSubset;
				pSet->erase(itor);
				itor = pSet->begin();
			}
			else
				itor++;
			if (nNeedSpace + nSpace < SDynTexture::s_CurDynTexMaxSize * 1024 * 1024)
				break;
		}
		if (itor == pSet->end())
			break;
	}
	s_nMemoryOccupied = nSpace;

	assert(s_iNumTextureBytesCheckedOut + s_iNumTextureBytesCheckedIn == s_nMemoryOccupied);

	if (nNeedSpace + nSpace > SDynTexture::s_CurDynTexMaxSize * 1024 * 1024)
		return false;
	return true;
}

void SDynTexture::ReleaseDynamicRT(bool bForce)
{
	if (!m_pTexture)
		return;
	m_nUpdateMask = 0;

	// first see if the texture is in the checked out pool.
	TextureSubset* pSubset;
	if (m_eTF == eTF_R8G8B8A8)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R8G8B8A8 : &s_checkedOutTexturePoolCube_R8G8B8A8;
	else if (m_eTF == eTF_BC1)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_BC1 : &s_checkedOutTexturePoolCube_BC1;
	else if (m_eTF == eTF_R32F)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R32F : &s_checkedOutTexturePoolCube_R32F;
	else if (m_eTF == eTF_R16F)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R16F : &s_checkedOutTexturePoolCube_R16F;
	else if (m_eTF == eTF_R16G16F)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R16G16F : &s_checkedOutTexturePoolCube_R16G16F;
	else if (m_eTF == eTF_R8G8B8A8S)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R8G8B8A8S : &s_checkedOutTexturePoolCube_R8G8B8A8S;
	else if (m_eTF == eTF_R16G16B16A16F)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R16G16B16A16F : &s_checkedOutTexturePoolCube_R16G16B16A16F;
	else
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	if (m_eTF == eTF_R11G11B10F)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R11G11B10F : &s_checkedOutTexturePoolCube_R11G11B10F;
	else
#endif
	if (m_eTF == eTF_R8G8S)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R8G8S : &s_checkedOutTexturePoolCube_R8G8S;
	else if (m_eTF == eTF_R10G10B10A2)
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R10G10B10A2 : &s_checkedOutTexturePoolCube_R10G10B10A2;
	else if (ConvertTexFormatToShadowsPool(m_eTF) != SBP_UNKNOWN)
	{
		if (m_eTT == eTT_2D)
		{
			pSubset = &s_checkedOutTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
		}
		else if (m_eTT == eTT_Cube)
		{
			pSubset = &s_checkedOutTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
		}
		else
		{
			pSubset = NULL;
			assert(0);
		}

	}
	else
	{
		pSubset = NULL;
		assert(0);
	}

	PREFAST_ASSUME(pSubset);

	auto coTexture = pSubset->find(m_pTexture->GetID());
	if (coTexture != pSubset->end())
	{
		// if it is there, remove it.
		pSubset->erase(coTexture);
		s_iNumTextureBytesCheckedOut -= m_pTexture->GetDataSize();
	}

	// Don't cache too many unused textures.
	if (bForce)
	{
		s_nMemoryOccupied -= m_pTexture->GetDataSize();
		assert(s_iNumTextureBytesCheckedOut + s_iNumTextureBytesCheckedIn == s_nMemoryOccupied);
		CRY_VERIFY(m_pTexture->Release() <= 0);
		m_pTexture = NULL;
		Unlink();
		return;
	}

	TextureSet* pSet;
	if (m_eTF == eTF_R8G8B8A8)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R8G8B8A8 : &s_availableTexturePoolCube_R8G8B8A8;
	else if (m_eTF == eTF_BC1)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_BC1 : &s_availableTexturePoolCube_BC1;
	else if (m_eTF == eTF_R32F)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R32F : &s_availableTexturePoolCube_R32F;
	else if (m_eTF == eTF_R16F)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R16F : &s_availableTexturePoolCube_R16F;
	else if (m_eTF == eTF_R16G16F)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R16G16F : &s_availableTexturePoolCube_R16G16F;
	else if (m_eTF == eTF_R8G8B8A8S)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R8G8B8A8S : &s_availableTexturePoolCube_R8G8B8A8S;
	else if (m_eTF == eTF_R16G16B16A16F)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R16G16B16A16F : &s_availableTexturePoolCube_R16G16B16A16F;
	else if (m_eTF == eTF_R11G11B10F)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R11G11B10F : &s_availableTexturePoolCube_R11G11B10F;
	else if (m_eTF == eTF_R8G8S)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R8G8S : &s_availableTexturePoolCube_R8G8S;
	else if (m_eTF == eTF_R10G10B10A2)
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R10G10B10A2 : &s_availableTexturePoolCube_R10G10B10A2;
	else if (ConvertTexFormatToShadowsPool(m_eTF) != SBP_UNKNOWN)
	{
		if (m_eTT == eTT_2D)
		{
			pSet = &s_availableTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
		}
		else if (m_eTT == eTT_Cube)
		{
			pSet = &s_availableTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
		}
		else
		{
			pSet = NULL;
			assert(0);
		}
	}
	else
	{
		pSet = NULL;
		assert(0);
	}

	PREFAST_ASSUME(pSet);

	auto subset = pSet->find(m_nWidth);
	if (subset != pSet->end())
	{
		subset->second->insert(TextureSubset::value_type(m_nHeight, m_pTexture));
		s_iNumTextureBytesCheckedIn += m_pTexture->GetDataSize();
	}
	else
	{
		pSubset = new TextureSubset;
		pSet->insert(TextureSet::value_type(m_nWidth, pSubset));
		pSubset->insert(TextureSubset::value_type(m_nHeight, m_pTexture));
		s_iNumTextureBytesCheckedIn += m_pTexture->GetDataSize();
	}
	m_pTexture = NULL;
	Unlink();
}

CTexture* SDynTexture::GetDynamicRT()
{
	TextureSet* pSet;
	TextureSubset* pSubset;

	assert(s_iNumTextureBytesCheckedOut + s_iNumTextureBytesCheckedIn == s_nMemoryOccupied);

	if (m_eTF == eTF_R8G8B8A8)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R8G8B8A8 : &s_availableTexturePoolCube_R8G8B8A8;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R8G8B8A8 : &s_checkedOutTexturePoolCube_R8G8B8A8;
	}
	else if (m_eTF == eTF_BC1)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_BC1 : &s_availableTexturePoolCube_BC1;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_BC1 : &s_checkedOutTexturePoolCube_BC1;
	}
	else if (m_eTF == eTF_R32F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R32F : &s_availableTexturePoolCube_R32F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R32F : &s_checkedOutTexturePoolCube_R32F;
	}
	else if (m_eTF == eTF_R16F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R16F : &s_availableTexturePoolCube_R16F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R16F : &s_checkedOutTexturePoolCube_R16F;
	}
	else if (m_eTF == eTF_R16G16F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R16G16F : &s_availableTexturePoolCube_R16G16F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R16G16F : &s_checkedOutTexturePoolCube_R16G16F;
	}
	else if (m_eTF == eTF_R8G8B8A8S)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R8G8B8A8S : &s_availableTexturePoolCube_R8G8B8A8S;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R8G8B8A8S : &s_checkedOutTexturePoolCube_R8G8B8A8S;
	}
	else if (m_eTF == eTF_R16G16B16A16F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R16G16B16A16F : &s_availableTexturePoolCube_R16G16B16A16F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R16G16B16A16F : &s_checkedOutTexturePoolCube_R16G16B16A16F;
	}
	else if (m_eTF == eTF_R11G11B10F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R11G11B10F : &s_availableTexturePoolCube_R11G11B10F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R11G11B10F : &s_checkedOutTexturePoolCube_R11G11B10F;
	}
	else if (m_eTF == eTF_R8G8S)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R8G8S : &s_availableTexturePoolCube_R8G8S;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R8G8S : &s_checkedOutTexturePoolCube_R8G8S;
	}
	else if (m_eTF == eTF_R10G10B10A2)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R10G10B10A2 : &s_availableTexturePoolCube_R10G10B10A2;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R10G10B10A2 : &s_checkedOutTexturePoolCube_R10G10B10A2;
	}
	else if (ConvertTexFormatToShadowsPool(m_eTF) != SBP_UNKNOWN)
	{
		if (m_eTT == eTT_2D)
		{
			pSet = &s_availableTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
			pSubset = &s_checkedOutTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
		}
		else if (m_eTT == eTT_Cube)
		{
			pSet = &s_availableTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
			pSubset = &s_checkedOutTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
		}
		else
		{
			pSet = NULL;
			pSubset = NULL;
			assert(0);
		}
	}
	else
	{
		pSet = NULL;
		pSubset = NULL;
		assert(0);
	}

	PREFAST_ASSUME(pSet);

	auto subset = pSet->find(m_nWidth);
	if (subset != pSet->end())
	{
		auto texture = subset->second->find(m_nHeight);
		if (texture != subset->second->end())
		{
			//  found one!
			// extract the texture
			CTexture* pTexture = texture->second;
			texture->second = NULL;
			// first remove it from this set.
			subset->second->erase(texture);
			// now add it to the checked out texture set.
			pSubset->insert(TextureSubset::value_type(pTexture->GetID(), pTexture));
			s_iNumTextureBytesCheckedOut += pTexture->GetDataSize();
			s_iNumTextureBytesCheckedIn -= pTexture->GetDataSize();

			assert(s_iNumTextureBytesCheckedOut + s_iNumTextureBytesCheckedIn == s_nMemoryOccupied);

			return pTexture;
		}
	}
	return NULL;
}

CTexture* SDynTexture::CreateDynamicRT()
{
	//assert(m_eTF == eTF_A8R8G8B8 && m_eTT == eTT_2D);

	assert(s_iNumTextureBytesCheckedOut + s_iNumTextureBytesCheckedIn == s_nMemoryOccupied);

	char name[256];
	CTexture* pTexture = GetDynamicRT();
	if (pTexture)
		return pTexture;

	TextureSet* pSet;
	TextureSubset* pSubset;
	if (m_eTF == eTF_R8G8B8A8)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R8G8B8A8 : &s_availableTexturePoolCube_R8G8B8A8;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R8G8B8A8 : &s_checkedOutTexturePoolCube_R8G8B8A8;
	}
	else if (m_eTF == eTF_BC1)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_BC1 : &s_availableTexturePoolCube_BC1;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_BC1 : &s_checkedOutTexturePoolCube_BC1;
	}
	else if (m_eTF == eTF_R32F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R32F : &s_availableTexturePoolCube_R32F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R32F : &s_checkedOutTexturePoolCube_R32F;
	}
	else if (m_eTF == eTF_R16F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R16F : &s_availableTexturePoolCube_R16F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R16F : &s_checkedOutTexturePoolCube_R16F;
	}
	else if (m_eTF == eTF_R16G16F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R16G16F : &s_availableTexturePoolCube_R16G16F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R16G16F : &s_checkedOutTexturePoolCube_R16G16F;
	}
	else if (m_eTF == eTF_R8G8B8A8S)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R8G8B8A8S : &s_availableTexturePoolCube_R8G8B8A8S;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R8G8B8A8S : &s_checkedOutTexturePoolCube_R8G8B8A8S;
	}
	else if (m_eTF == eTF_R16G16B16A16F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R16G16B16A16F : &s_availableTexturePoolCube_R16G16B16A16F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R16G16B16A16F : &s_checkedOutTexturePoolCube_R16G16B16A16F;
	}
	else if (m_eTF == eTF_R11G11B10F)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R11G11B10F : &s_availableTexturePoolCube_R11G11B10F;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R11G11B10F : &s_checkedOutTexturePoolCube_R11G11B10F;
	}
	else if (m_eTF == eTF_R8G8S)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R8G8S : &s_availableTexturePoolCube_R8G8S;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R8G8S : &s_checkedOutTexturePoolCube_R8G8S;
	}
	else if (m_eTF == eTF_R10G10B10A2)
	{
		pSet = m_eTT == eTT_2D ? &s_availableTexturePool2D_R10G10B10A2 : &s_availableTexturePoolCube_R10G10B10A2;
		pSubset = m_eTT == eTT_2D ? &s_checkedOutTexturePool2D_R10G10B10A2 : &s_checkedOutTexturePoolCube_R10G10B10A2;
	}
	else if (ConvertTexFormatToShadowsPool(m_eTF) != SBP_UNKNOWN)
	{
		if (m_eTT == eTT_2D)
		{
			pSet = &s_availableTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
			pSubset = &s_checkedOutTexturePool2D_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
		}
		else if (m_eTT == eTT_Cube)
		{
			pSet = &s_availableTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
			pSubset = &s_checkedOutTexturePoolCube_Shadows[ConvertTexFormatToShadowsPool(m_eTF)];
		}
		else
		{
			pSet = NULL;
			pSubset = NULL;
			assert(0);
		}
	}
	else
	{
		pSet = NULL;
		pSubset = NULL;
		assert(0);
	}

	if (m_eTT == eTT_2D)
		cry_sprintf(name, "$Dyn_%s_2D_%s_%d", m_sSource, CTexture::NameForTextureFormat(m_eTF), gRenDev->m_TexGenID++);
	else
		cry_sprintf(name, "$Dyn_%s_Cube_%s_%d", m_sSource, CTexture::NameForTextureFormat(m_eTF), gRenDev->m_TexGenID++);

	PREFAST_ASSUME(pSet);

	CTexture* pNewTexture = nullptr;
	if (m_nTexFlags & FT_USAGE_DEPTHSTENCIL)
	{
		pNewTexture = CTexture::GetOrCreateDepthStencil(name, m_nWidth, m_nHeight, m_clearValue, m_eTT, m_nTexFlags, m_eTF);
		CClearSurfacePass::Execute(pNewTexture, CLEAR_ZBUFFER | CLEAR_STENCIL, m_clearValue.r, uint8(m_clearValue.g));
	}
	else
	{
		pNewTexture = CTexture::GetOrCreateRenderTarget(name, m_nWidth, m_nHeight, m_clearValue, m_eTT, m_nTexFlags, m_eTF);
		CClearSurfacePass::Execute(pNewTexture, m_clearValue);
	}

	auto subset = pSet->find(m_nWidth);
	if (subset == pSet->end())
	{
		TextureSubset* pSSet = new TextureSubset;
		pSet->insert(TextureSet::value_type(m_nWidth, pSSet));
	}

	pSubset->insert(TextureSubset::value_type(pNewTexture->GetID(), pNewTexture));
	s_nMemoryOccupied += pNewTexture->GetDataSize();
	s_iNumTextureBytesCheckedOut += pNewTexture->GetDataSize();
	assert(s_iNumTextureBytesCheckedOut + s_iNumTextureBytesCheckedIn == s_nMemoryOccupied);

	return pNewTexture;

}

void SDynTexture::ResetUpdateMask()
{
	m_nUpdateMask = 0;
	if (gRenDev)
		m_nFrameReset = gRenDev->m_nFrameReset;
}

void SDynTexture::SetUpdateMask()
{
	int nFrame = gRenDev->RT_GetCurrGpuID();
	m_nUpdateMask |= 1 << nFrame;
}

void SDynTexture::ReleaseForce()
{
	ReleaseDynamicRT(true);
	delete this;
}

bool SDynTexture::IsValid()
{
	if (!m_pTexture)
		return false;
	if (m_nFrameReset != gRenDev->m_nFrameReset)
	{
		m_nFrameReset = gRenDev->m_nFrameReset;
		m_nUpdateMask = 0;
		return false;
	}
	if (gRenDev->GetActiveGPUCount() > 1)
	{
		if ((gRenDev->GetFeatures() & RFT_HW_MASK) == RFT_HW_ATI)
		{
			int nX, nY, nW, nH;
			GetImageRect(nX, nY, nW, nH);
			if (nW < 1024 && nH < 1024)
				return true;
		}

		int nFrame = gRenDev->RT_GetCurrGpuID();
		if (!((1 << nFrame) & m_nUpdateMask))
			return false;
	}
	return true;
}

void SDynTexture::Tick()
{
	if (s_SuggestedDynTexMaxSize != CRenderer::CV_r_dyntexmaxsize)
	{
		Init();
	}
}

void SDynTexture::Init()
{
	s_SuggestedDynTexMaxSize = CRenderer::CV_r_dyntexmaxsize;
	s_CurDynTexMaxSize = s_SuggestedDynTexMaxSize;
}

EShadowBuffers_Pool SDynTexture::ConvertTexFormatToShadowsPool(ETEX_Format e)
{
	switch (e)
	{
	case (eTF_D16):
		return SBP_D16;
	case (eTF_D24S8):
		return SBP_D24S8;
	case (eTF_D32F):
	case (eTF_D32FS8):
		return SBP_D32FS8;
	case (eTF_R16G16):
		return SBP_R16G16;

	default:
		break;
	}
	//assert( false );
	return SBP_UNKNOWN;
}
