// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../Common/Textures/TextureStreamPool.h"

#include "DriverD3D.h"

#if !defined(CHK_RENDTH)
	#define CHK_RENDTH assert(gRenDev->m_pRT->IsRenderThread(true))
#endif
#if !defined(CHK_MAINTH)
	#define CHK_MAINTH assert(gRenDev->m_pRT->IsMainThread(true))
#endif
#if !defined(CHK_MAINORRENDTH)
	#define CHK_MAINORRENDTH assert(gRenDev->m_pRT->IsMainThread(true) || gRenDev->m_pRT->IsRenderThread(true) || gRenDev->m_pRT->IsLevelLoadingThread(true))
#endif

CryCriticalSection STexPoolItemHdr::s_sSyncLock;

bool STexPoolItem::IsStillUsedByGPU(uint32 nTick)
{
	CDeviceTexture* pDeviceTexture = m_pDevTexture;
	if (pDeviceTexture)
	{
		CHK_MAINORRENDTH;
		D3DBaseTexture* pD3DTex = pDeviceTexture->GetBaseTexture();
	}
	return (nTick - m_nFreeTick) < 4;
}

STexPoolItem::STexPoolItem(STexPool* pOwner, CDeviceTexture* pDevTexture, size_t devTextureSize)
	: m_pOwner(pOwner)
	, m_pTex(NULL)
	, m_pDevTexture(pDevTexture)
	, m_nDevTextureSize(devTextureSize)
	, m_nFreeTick(0)
	, m_nActiveLod(0)
{
	assert(pDevTexture);
	++pOwner->m_nItems;
}

STexPoolItem::~STexPoolItem()
{
	assert(m_pDevTexture);

	--m_pOwner->m_nItems;
	if (IsFree())
		--m_pOwner->m_nItemsFree;

	Unlink();
	UnlinkFree();

	m_pDevTexture->Release();
}

CTextureStreamPoolMgr::CTextureStreamPoolMgr()
{
	m_FreeTexPoolItems.m_NextFree = &m_FreeTexPoolItems;
	m_FreeTexPoolItems.m_PrevFree = &m_FreeTexPoolItems;

	m_nDeviceMemReserved = 0;
	m_nDeviceMemInUse = 0;

#ifdef TEXSTRM_USE_FREEPOOL
	m_nFreePoolBegin = 0;
	m_nFreePoolEnd = 0;
#endif

	m_nTick = 0;

#if !defined(_RELEASE)
	m_bComputeStats = false;
#endif
}

CTextureStreamPoolMgr::~CTextureStreamPoolMgr()
{
	Flush();
}

void CTextureStreamPoolMgr::Flush()
{
	AUTO_LOCK(STexPoolItem::s_sSyncLock);

	FlushFree();

	// Free pools first, as that will attempt to remove streamed textures in release, which will end up
	// in the free list.

	for (TexturePoolMap::iterator it = m_TexturesPools.begin(), itEnd = m_TexturesPools.end(); it != itEnd; ++it)
		SAFE_DELETE(it->second);
	stl::free_container(m_TexturesPools);

	// Now nuke the free items.

	FlushFree();

#ifdef TEXSTRM_USE_FREEPOOL
	while (m_nFreePoolBegin != m_nFreePoolEnd)
	{
		assert(m_nFreePoolEnd < CRY_ARRAY_COUNT(m_FreePool));
		void* p = m_FreePool[m_nFreePoolBegin];
		m_nFreePoolBegin = (m_nFreePoolBegin + 1) % MaxFreePool ;
		::operator delete(p);
	}
#endif
}

STexPool* CTextureStreamPoolMgr::GetPool(const STextureLayout& pLayout)
{
	D3DFormat d3dFmt = DeviceFormats::ConvertFromTexFormat(pLayout.m_eDstFormat);
	if (pLayout.m_bIsSRGB)
		d3dFmt = DeviceFormats::ConvertToSRGB(d3dFmt);

	TexturePoolKey poolKey((uint16)pLayout.m_nWidth, (uint16)pLayout.m_nHeight, d3dFmt, (uint8)pLayout.m_eTT, (uint8)pLayout.m_nMips, (uint16)pLayout.m_nArraySize);

	TexturePoolMap::iterator it = m_TexturesPools.find(poolKey);
	if (it != m_TexturesPools.end())
		return it->second;

	return NULL;
}

STexPoolItem* CTextureStreamPoolMgr::GetItem(const STextureLayout& pLayout, bool bShouldBeCreated, const char* sName, const STexturePayload* pPayload, bool bCanCreate, bool bWaitForIdle)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Texture, 0, "Stream pool item %ix%ix%i", pLayout.m_nWidth, pLayout.m_nHeight, pLayout.m_nMips);

	D3DFormat d3dFmt = DeviceFormats::ConvertFromTexFormat(pLayout.m_eDstFormat);
	if (pLayout.m_bIsSRGB)
		d3dFmt = DeviceFormats::ConvertToSRGB(d3dFmt);

	// TODO: use pLayout
	STexPool* pPool = GetOrCreatePool(pLayout.m_nWidth, pLayout.m_nHeight, pLayout.m_nMips, pLayout.m_nArraySize, d3dFmt, pLayout.m_eTT);
	if (!pPool)
		return NULL;

	AUTO_LOCK(STexPoolItem::s_sSyncLock);

	STexPoolItemHdr* pITH = &m_FreeTexPoolItems;
	STexPoolItem* pIT = static_cast<STexPoolItem*>(pITH);

	bool bFoundACoolingMatch = false;

#ifndef TSP_GC_ALL_ITEMS
	if (!pPayload)
	{
		pITH = m_FreeTexPoolItems.m_PrevFree;
		pIT = static_cast<STexPoolItem*>(pITH);

		// Try to find empty item in the pool
		while (pITH != &m_FreeTexPoolItems)
		{
			if (pIT->m_pOwner == pPool)
			{
				if (!bWaitForIdle || !pIT->IsStillUsedByGPU(m_nTick))
					break;

				bFoundACoolingMatch = true;
			}

			pITH = pITH->m_PrevFree;
			pIT = static_cast<STexPoolItem*>(pITH);
		}
	}

#endif

	if (pITH != &m_FreeTexPoolItems)
	{
		pIT->UnlinkFree();
#if defined(DO_RENDERLOG)
		if (CRenderer::CV_r_logTexStreaming == 2)
			gRenDev->LogStrv("Remove from FreePool '%s', [%d x %d], Size: %d\n", sName, pIT->m_pOwner->m_Width, pIT->m_pOwner->m_Height, pIT->m_pOwner->m_nDevTextureSize);
#endif

#if !defined(_RELEASE)
		++m_frameStats.nSoftCreates;
#endif

		--pIT->m_pOwner->m_nItemsFree;
	}
	else
	{
		if (!bCanCreate)
			return NULL;

#if defined(TEXSTRM_TEXTURECENTRIC_MEMORY)
		if (bFoundACoolingMatch)
		{
			// Really, really, don't want to create a texture if one will be available soon
			if (!bShouldBeCreated)
				return NULL;
		}
#endif

		// Create API texture for the item in DEFAULT pool
		CDeviceTexture* pDevTex = CDeviceTexture::Create(pLayout, pPayload);
		if (!pDevTex)
		{
			return nullptr;
		}

#ifdef TEXSTRM_USE_FREEPOOL
		if (m_nFreePoolBegin != m_nFreePoolEnd)
		{
			assert(m_nFreePoolBegin < (CRY_ARRAY_COUNT(m_FreePool)));

			void* p = m_FreePool[m_nFreePoolBegin];
			m_nFreePoolBegin = (m_nFreePoolBegin + 1) % MaxFreePool;
			pIT = new(p) STexPoolItem(pPool, pDevTex, pPool->m_nDevTextureSize);
		}
		else
#endif
		{
			pIT = new STexPoolItem(pPool, pDevTex, pPool->m_nDevTextureSize);
		}

		pIT->Link(&pPool->m_ItemsList);
		CryInterlockedAdd(&m_nDeviceMemReserved, pPool->m_nDevTextureSize);

#if !defined(_RELEASE)
		++m_frameStats.nHardCreates;
#endif
	}

	CryInterlockedAdd(&m_nDeviceMemInUse, pIT->m_nDevTextureSize);

	return pIT;
}

void CTextureStreamPoolMgr::ReleaseItem(STexPoolItem* pItem)
{
	assert(!pItem->m_NextFree);

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_logTexStreaming == 2)
	{
		const char* name = pItem->m_pTex ? pItem->m_pTex->GetSourceName() : "";
		gRenDev->LogStrv("Add to FreePool '%s', [%d x %d], Size: %d\n", name, pItem->m_pOwner->m_Width, pItem->m_pOwner->m_Height, pItem->m_pOwner->m_nDevTextureSize);
	}
#endif

	CryInterlockedAdd(&m_nDeviceMemInUse, -(intptr_t)pItem->m_nDevTextureSize);
	pItem->m_pTex = NULL;
	pItem->m_nFreeTick = m_nTick;
	pItem->LinkFree(&m_FreeTexPoolItems);

	++pItem->m_pOwner->m_nItemsFree;

#if !defined(_RELEASE)
	++m_frameStats.nSoftFrees;
#endif
}

STexPool* CTextureStreamPoolMgr::GetOrCreatePool(int nWidth, int nHeight, int nMips, int nArraySize, D3DFormat eTF, ETEX_Type eTT)
{
	STexPool* pPool = NULL;

	assert((eTT != eTT_Cube && eTT != eTT_CubeArray) || !(nArraySize % 6));
	TexturePoolKey poolKey((uint16)nWidth, (uint16)nHeight, eTF, (uint8)eTT, (uint8)nMips, (uint16)nArraySize);

	TexturePoolMap::iterator it = m_TexturesPools.find(poolKey);
	if (it == m_TexturesPools.end())
	{
		// Create new pool
		pPool = new STexPool;
		pPool->m_eTT = eTT;
		pPool->m_eFormat = eTF;
		pPool->m_Width = nWidth;
		pPool->m_Height = nHeight;
		pPool->m_nArraySize = nArraySize;
		pPool->m_nMips = nMips;
		pPool->m_nDevTextureSize = CDeviceTexture::TextureDataSize(nWidth, nHeight, 1, nMips, nArraySize, DeviceFormats::ConvertToTexFormat(eTF), eTM_Optimal, CDeviceObjectFactory::BIND_SHADER_RESOURCE);

		pPool->m_nItems = 0;
		pPool->m_nItemsFree = 0;

		pPool->m_ItemsList.m_Next = &pPool->m_ItemsList;
		pPool->m_ItemsList.m_Prev = &pPool->m_ItemsList;

		it = m_TexturesPools.insert(std::make_pair(poolKey, pPool)).first;
	}

	return it->second;
}

void CTextureStreamPoolMgr::FlushFree()
{
	STexPoolItemHdr* pITH = m_FreeTexPoolItems.m_PrevFree;
	while (pITH != &m_FreeTexPoolItems)
	{
		STexPoolItemHdr* pITHNext = pITH->m_PrevFree;
		STexPoolItem* pIT = static_cast<STexPoolItem*>(pITH);

		assert(!pIT->m_pTex);
		CryInterlockedAdd(&m_nDeviceMemReserved, -(intptr_t)pIT->m_nDevTextureSize);

#ifdef TEXSTRM_USE_FREEPOOL
		unsigned int nFreePoolSize = (m_nFreePoolEnd - m_nFreePoolBegin) % MaxFreePool;
		if (nFreePoolSize < MaxFreePool - 1) // -1 to avoid needing a separate count
		{
			pIT->~STexPoolItem();
			m_FreePool[m_nFreePoolEnd] = pIT;
			m_nFreePoolEnd = (m_nFreePoolEnd + 1) % MaxFreePool;
		}
		else
#endif
		{
			delete pIT;
		}

		pITH = pITHNext;
	}
}

void CTextureStreamPoolMgr::GarbageCollect(size_t* nCurTexPoolSize, size_t nLowerPoolLimit, int nMaxItemsToFree)
{
	size_t nSize = nCurTexPoolSize ? *nCurTexPoolSize : 1024 * 1024 * 1024;
	ptrdiff_t nFreed = 0;

	AUTO_LOCK(STexPoolItem::s_sSyncLock);

	STexPoolItemHdr* pITH = m_FreeTexPoolItems.m_PrevFree;
	while (pITH != &m_FreeTexPoolItems)
	{
		STexPoolItemHdr* pITHNext = pITH->m_PrevFree;

		STexPoolItem* pIT = static_cast<STexPoolItem*>(pITH);

		assert(!pIT->m_pTex);

#if defined(TEXSTRM_TEXTURECENTRIC_MEMORY)
		if (pIT->m_pOwner->m_nItemsFree > 20 || pIT->m_pOwner->m_Width > 64 || pIT->m_pOwner->m_Height > 64)
#endif
		{
			if (!pIT->IsStillUsedByGPU(m_nTick))
			{
				nSize -= pIT->m_nDevTextureSize;
				nFreed += (ptrdiff_t)pIT->m_nDevTextureSize;

#ifdef TEXSTRM_USE_FREEPOOL
				unsigned int nFreePoolSize = (m_nFreePoolEnd - m_nFreePoolBegin) % MaxFreePool;
				if (nFreePoolSize < MaxFreePool - 1) // -1 to avoid needing a separate count
				{
					pIT->~STexPoolItem();
					m_FreePool[m_nFreePoolEnd] = pIT;
					m_nFreePoolEnd = (m_nFreePoolEnd + 1) % MaxFreePool;
				}
				else
#endif
				{
					delete pIT;
				}

				--nMaxItemsToFree;

#if !defined(_RELEASE)
				++m_frameStats.nHardFrees;
#endif
			}
		}

		pITH = pITHNext;

		// we release all textures immediately on consoles
#ifndef TSP_GC_ALL_ITEMS
		if (nMaxItemsToFree <= 0 || nSize < nLowerPoolLimit)
			break;
#endif
	}

	CryInterlockedAdd(&m_nDeviceMemReserved, -nFreed);

	CTexture::StreamValidateTexSize();

	if (nCurTexPoolSize)
		*nCurTexPoolSize = nSize;

#if !defined(_RELEASE)
	if (m_bComputeStats)
	{
		CryAutoLock<CryCriticalSection> lock(m_statsLock);

		m_poolStats.clear();

		for (TexturePoolMap::iterator it = m_TexturesPools.begin(), itEnd = m_TexturesPools.end(); it != itEnd; ++it)
		{
			STexPool* pPool = it->second;
			SPoolStats ps;
			ps.nWidth = pPool->m_Width;
			ps.nHeight = pPool->m_Height;
			ps.nMips = pPool->m_nMips;
			ps.nFormat = pPool->m_eFormat;
			ps.eTT = pPool->m_eTT;

			ps.nInUse = pPool->m_nItems - pPool->m_nItemsFree;
			ps.nFree = pPool->m_nItemsFree;
			ps.nHardCreatesPerFrame = 0;
			ps.nSoftCreatesPerFrame = 0;

			m_poolStats.push_back(ps);
		}
	}
#endif

	++m_nTick;
}

void CTextureStreamPoolMgr::GetMemoryUsage(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "Texture Pools");
	pSizer->AddObject(m_TexturesPools);
}
