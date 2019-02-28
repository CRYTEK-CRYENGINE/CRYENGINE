// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetThumbnailsLoader.h"

#include <AssetSystem/AssetType.h>
#include <AssetSystem/AssetManager.h>
#include <FilePathUtil.h>
#include <ThreadingUtils.h>

#include <CrySystem/IConsole.h>

#include <QIcon>
#include <QPixmap>
#include <QTimer>

#include <algorithm>

#include <stdio.h>

namespace Private_AssetThumbnailsLoader
{

std::vector<char> LoadThumbnailData(const CAsset& asset)
{
	ICryPak* const pPak = GetISystem()->GetIPak();
	FILE* pFile = pPak->FOpen(asset.GetThumbnailPath(), "rbx");
	if (!pFile)
	{
		return{};
	}

	std::vector<char> buffer(pPak->FGetSize(pFile));
	const size_t numberOfBytesRead = pPak->FReadRawAll(buffer.data(), buffer.size(), pFile);
	pPak->FClose(pFile);

	return buffer;
}

} // namespace Private_AssetThumbnailsLoader

//! CAssetThumbnailsFinalizer signals completion of thumbnail loading for a batch of assets.
//! Batching signals reduces the latency of displaying thumbnails.
class CAssetThumbnailsLoader::CAssetThumbnailsFinalizer
{
public:
	CAssetThumbnailsFinalizer()
	{
		const size_t initialSize = 1 << 6;
		m_frontBuffer.reserve(initialSize);
		m_backBuffer.reserve(initialSize);
	}

	//! Enqueues an asset for thumbnail finalization. Thread-safe.
	void PostAsset(CAsset* pAsset, std::vector<char> thumbnailData)
	{
		bool bFinalize = false;

		{
			std::lock_guard<std::mutex> lock(m_backBufferMutex);

			//! An update is only required when the back-buffer is empty.
			//! When the back-buffer is not empty, a previous call to PostAssset() already triggered a finalize.
			bFinalize = !m_backBuffer.size();

			m_backBuffer.emplace_back(pAsset, std::move(thumbnailData));
		}

		if (bFinalize)
		{
			ThreadingUtils::PostOnMainThread([this]() { Finalize(); });
		}
	}

private:
	void Finalize()
	{
		{
			std::lock_guard<std::mutex> lock(m_backBufferMutex);
			m_frontBuffer.insert(m_frontBuffer.end(), m_backBuffer.begin(), m_backBuffer.end());
			m_backBuffer.clear();
		}

		// Assets are finalized in batches, so that the main thread is not blocked for too long.
		const size_t batchSize = 1 << 5;
		size_t i = 0;
		while (!m_frontBuffer.empty() && i < batchSize)
		{
			CAsset& asset = *m_frontBuffer.back().first;

			const std::vector<char>& thumbnailData = m_frontBuffer.back().second;

			if (!thumbnailData.empty())
			{
				QPixmap pixmap;
				pixmap.loadFromData((const uchar*)&thumbnailData[0], thumbnailData.size(), "PNG");
				asset.SetThumbnail(QIcon(pixmap));
			}

			CAssetThumbnailsLoader::GetInstance().signalAssetThumbnailLoaded(asset);

			m_frontBuffer.pop_back();
			++i;
		}

		if (!m_frontBuffer.empty())
		{
			// Note that we cannot use PostOnMainThread, since this would start executing immediately.
			// Instead, we add a timer event to the end of Qt's event queue.
			QTimer::singleShot(0, [this]() { Finalize(); });
		}
	}

private:
	std::vector<std::pair<CAssetPtr, std::vector<char>>> m_frontBuffer;
	std::vector<std::pair<CAssetPtr, std::vector<char>>> m_backBuffer;
	std::mutex m_backBufferMutex;
};

CAssetThumbnailsLoader& CAssetThumbnailsLoader::GetInstance()
{
	static CAssetThumbnailsLoader theInstance;
	return theInstance;
}

void CAssetThumbnailsLoader::PostAsset(CAsset* pAsset)
{
	{
		std::lock_guard<std::mutex> lock(m_assetsMutex);

		if (pAsset->IsThumbnailLoaded())
		{
			return;
		}
		// Mark as loaded immediately to ignore requests that have already been posted.
		// CAssetThumbnailsLoader::signalAssetThumbnailLoaded will notify when the real thumbnail loading is completed.
		pAsset->SetThumbnail(pAsset->GetType()->GetIcon());

		if (!m_assets.Full())
		{
			m_assets.Push(pAsset);
		}
		else // Delete the request with the lowest priority. Notify the caller so that it can add it again, if it is still needed.
		{
			m_assets.Front()->InvalidateThumbnail();
			signalAssetThumbnailLoaded(*m_assets.Front());
			m_assets.CyclePush(pAsset);
		}
	}
	m_conditionVariable.notify_one();
}

void CAssetThumbnailsLoader::TouchAsset(CAsset* pAsset)
{
	std::lock_guard<std::mutex> lock(m_assetsMutex);
	for (auto it = m_assets.Begin(); it != m_assets.End(); ++it)
	{
		if (it->get() == pAsset)
		{
			m_assets.Erase(it);
			m_assets.Push(pAsset);

			break;
		}
	}	
}

CAssetThumbnailsLoader::CAssetThumbnailsLoader()
	: m_pFinalizer(new CAssetThumbnailsFinalizer())
	, m_bGenerateThumbnais(false)
{
	REGISTER_INT_CB("ed_generateThumbnails", 1, 0,
		"When non-zero, enables lazy auto-generation of preview thumbnails for cryassets.", 
		CAssetThumbnailsLoader::OnCVarChanged);

	SetGenerateThumbnais(gEnv->pConsole->GetCVar("ed_generateThumbnails")->GetIVal() != 0);

	// There is not much point in running several loading threads - the operation is I/O bound.
	const size_t threadCount = 1;
	for (size_t i = 0; i < threadCount; ++i)
	{
		m_threads.emplace_back(new std::thread([this]()
		{
			ThreadFunc();
		}));
	}
}

void CAssetThumbnailsLoader::ThreadFunc()
{
	using namespace Private_AssetThumbnailsLoader;

	std::unique_lock<std::mutex> lock(m_assetsMutex);
	while (true)
	{
		m_conditionVariable.wait(lock, [this]() { return !m_assets.Empty(); });

		while (!m_assets.Empty())
		{
			CAssetPtr pAsset = m_assets.Back();
			m_assets.PopBack();

			m_assetsMutex.unlock();

			if (pAsset)
			{
				std::vector<char> thumbnailData = LoadThumbnailData(*pAsset);
				if (!thumbnailData.empty() || !GetGenerateThumbnais())
				{
					m_pFinalizer->PostAsset(pAsset, std::move(thumbnailData));
				}
				else
				{
					pAsset->GenerateThumbnail();
					m_pFinalizer->PostAsset(pAsset, LoadThumbnailData(*pAsset));
				}
			}

			m_assetsMutex.lock();
		}
	}
}

void CAssetThumbnailsLoader::OnCVarChanged(ICVar* const pGenerateThumbnais)
{
	CRY_ASSERT(pGenerateThumbnais);
	CRY_ASSERT(pGenerateThumbnais->GetType() == CVAR_INT);
	CRY_ASSERT(strcmp(pGenerateThumbnais->GetName(), "ed_generateThumbnails") == 0);

	GetInstance().SetGenerateThumbnais(pGenerateThumbnais->GetIVal() != 0);

	if (GetInstance().GetGenerateThumbnais())
	{
		CAssetManager::GetInstance()->ForeachAsset([](CAsset* pAsset) 
		{ 
			if (pAsset->GetType()->HasThumbnail() && pAsset->IsThumbnailLoaded())
			{
				pAsset->InvalidateThumbnail();
			}
		});
	}
}

