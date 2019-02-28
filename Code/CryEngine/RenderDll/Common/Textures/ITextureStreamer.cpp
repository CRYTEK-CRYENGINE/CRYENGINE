// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ITextureStreamer.h"
#include "Texture.h"

ITextureStreamer::ITextureStreamer()
{
	m_pendingRelinks.reserve(4096);
	m_pendingUnlinks.reserve(4096);
	m_textures.reserve(4096);
}

void ITextureStreamer::BeginUpdateSchedule()
{
	SyncTextureList();

	if (CTexture::s_bStatsComputeStreamPoolWanted || CTexture::s_pStatsTexWantedLists)
	{
		CTexture::s_nStatsStreamPoolWanted = StatsComputeRequiredMipMemUsage();
	}

#if !defined(_RELEASE)
	if (CRenderer::CV_r_TexturesStreamingDebug >= 3)
		CTexture::OutputDebugInfo();
#endif
}

void ITextureStreamer::ApplySchedule(EApplyScheduleFlags asf)
{
	if (asf & eASF_InOut)
		CTexture::StreamState_Update();

	if (asf & eASF_Prep)
		CTexture::StreamState_UpdatePrep();
}

void ITextureStreamer::Relink(CTexture* pTexture)
{
	CryAutoCriticalSection scopelock(m_accessLock);

	m_pendingRelinks.push_back(pTexture);
	pTexture->m_bInDistanceSortedList = true;
}

void ITextureStreamer::Unlink(CTexture* pTexture)
{
	CryAutoCriticalSection scopelock(m_accessLock);

	TStreamerTextureVec::iterator it = std::find(m_pendingRelinks.begin(), m_pendingRelinks.end(), pTexture);
	if (it == m_pendingRelinks.end())
	{
		m_pendingUnlinks.push_back(pTexture);
	}
	else
	{
		std::swap(*it, m_pendingRelinks.back());
		m_pendingRelinks.pop_back();
	}

	pTexture->m_bInDistanceSortedList = false;
}

int ITextureStreamer::GetMinStreamableMip() const
{
	return CTexture::s_bStreamingFromHDD ? 0 : CRenderer::CV_r_TexturesStreamingMipClampDVD;
}

int ITextureStreamer::GetMinStreamableMipWithSkip() const
{
	return (CTexture::s_bStreamingFromHDD ? 0 : CRenderer::CV_r_TexturesStreamingMipClampDVD);
}

size_t ITextureStreamer::StatsComputeRequiredMipMemUsage()
{
#ifdef STRIP_RENDER_THREAD
	int nThreadList = m_nCurThreadProcess;
#else
	int nThreadList = gRenDev->m_pRT->m_nCurThreadProcess;
#endif
	std::vector<CTexture::WantedStat>* pLists = CTexture::s_pStatsTexWantedLists;
	std::vector<CTexture::WantedStat>* pList = pLists ? &pLists[nThreadList] : NULL;

	if (pList)
		pList->clear();

	SyncTextureList();

	CryAutoCriticalSection scopelock(m_accessLock);

	TStreamerTextureVec& textures = GetTextures();

	size_t nSizeToLoad = 0;

	TStreamerTextureVec::iterator item = textures.begin(), end = textures.end();
	for (; item != end; ++item)
	{
		CTexture* tp = *item;

		bool bStale = StatsWouldUnload(tp);
		{
			int nPersMip = tp->m_nMips - tp->m_CacheFileHeader.m_nMipsPersistent;
			int nReqMip = tp->m_bForceStreamHighRes ? 0 : (bStale ? nPersMip : tp->GetRequiredMip());
			int nMips = tp->GetNumMips();
			nReqMip = min(nReqMip, nPersMip);

			int nWantedSize = tp->StreamComputeSysDataSize(nReqMip);

			nSizeToLoad += nWantedSize;
			if (nWantedSize && pList)
			{
				if (tp->TryAddRef())
				{
					CTexture::WantedStat ws;
					ws.pTex = tp;
					tp->Release();
					ws.nWanted = nWantedSize;
					pList->push_back(ws);
				}
			}
		}
	}

	return nSizeToLoad;
}

void ITextureStreamer::StatsFetchTextures(std::vector<CTexture*>& out)
{
	SyncTextureList();

	CryAutoCriticalSection scopelock(m_accessLock);

	out.reserve(out.size() + m_textures.size());
	std::copy(m_textures.begin(), m_textures.end(), std::back_inserter(out));
}

bool ITextureStreamer::StatsWouldUnload(const CTexture* pTexture)
{
	const int nCurrentFarZoneRoundId = gRenDev->GetStreamZoneRoundId(MAX_PREDICTION_ZONES - 1);
	const int nCurrentNearZoneRoundId = gRenDev->GetStreamZoneRoundId(0);

	return
	  (nCurrentFarZoneRoundId - pTexture->GetStreamRoundInfo(MAX_PREDICTION_ZONES - 1).nRoundUpdateId > 3) &&
	  (nCurrentNearZoneRoundId - pTexture->GetStreamRoundInfo(0).nRoundUpdateId > 3);
}

void ITextureStreamer::SyncTextureList()
{
	CryAutoCriticalSection scopelock(m_accessLock);

	if (!m_pendingUnlinks.empty())
	{
		std::sort(m_pendingUnlinks.begin(), m_pendingUnlinks.end());
		m_pendingUnlinks.resize((int)(std::unique(m_pendingUnlinks.begin(), m_pendingUnlinks.end()) - m_pendingUnlinks.begin()));

		TStreamerTextureVec::iterator it = m_textures.begin(), wrIt = it, itEnd = m_textures.end();
		for (; it != itEnd; ++it)
		{
			if (!std::binary_search(m_pendingUnlinks.begin(), m_pendingUnlinks.end(), *it))
				*wrIt++ = *it;
		}
		m_textures.erase(wrIt, itEnd);

		m_pendingUnlinks.resize(0);
	}

	if (!m_pendingRelinks.empty())
	{
		std::sort(m_pendingRelinks.begin(), m_pendingRelinks.end());
		m_pendingRelinks.resize((int)(std::unique(m_pendingRelinks.begin(), m_pendingRelinks.end()) - m_pendingRelinks.begin()));
		m_textures.append(m_pendingRelinks);

		m_pendingRelinks.resize(0);
	}
}
