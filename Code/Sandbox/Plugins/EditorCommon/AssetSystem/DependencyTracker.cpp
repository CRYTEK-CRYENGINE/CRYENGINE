// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DependencyTracker.h"
#include "AssetManager.h"

CDependencyTracker::CDependencyTracker()
{
	CAssetManager* pAssetManager = CAssetManager::GetInstance();

	pAssetManager->signalAfterAssetsInserted.Connect([this](const std::vector<CAsset*>&)
	{
		CreateIndex();
	}, (uintptr_t)this);

	pAssetManager->signalAfterAssetsUpdated.Connect([this]()
	{
		CreateIndex();
	}, (uintptr_t)this);

	pAssetManager->signalAssetChanged.Connect([this](CAsset&)
	{
		CreateIndex();
	}, (uintptr_t)this);

	pAssetManager->signalAfterAssetsRemoved.Connect([this]()
	{
		CreateIndex();
	}, (uintptr_t)this);
}

CDependencyTracker::~CDependencyTracker()
{
	CAssetManager* pAssetManager = CAssetManager::GetInstance();
	pAssetManager->signalAfterAssetsInserted.DisconnectById((uintptr_t)this);
	pAssetManager->signalAfterAssetsUpdated.DisconnectById((uintptr_t)this);
	pAssetManager->signalAssetChanged.DisconnectById((uintptr_t)this);
	pAssetManager->signalAfterAssetsRemoved.DisconnectById((uintptr_t)this);
}

std::vector<SAssetDependencyInfo> CDependencyTracker::GetReverseDependencies(const char* szAssetPath) const
{
	const string key = PathUtil::ToUnixPath(szAssetPath);
	std::vector<SAssetDependencyInfo> dependencies;

	struct SLess
	{
		bool operator() (const std::pair<string, SAssetDependencyInfo>& a, const string& b)
		{
			return stricmp(a.first.c_str(), b.c_str()) < 0;
		}

		bool operator() (const string& a, const std::pair<string, SAssetDependencyInfo>& b)
		{
			return stricmp(a.c_str(), b.first.c_str()) < 0;
		}
	};

	if (m_future.valid())
	{
		m_future.wait();
	}

	const auto lowerBound = std::lower_bound(m_index.begin(), m_index.end(), key, SLess());
	const auto upperBound = std::upper_bound(lowerBound, m_index.end(), key, SLess());

	std::transform(lowerBound, upperBound, std::back_inserter(dependencies), [](const auto& x) 
	{
		return x.second;
	});

	return dependencies;
}

void CDependencyTracker::CreateIndex()
{
	// Do nothing if the future has already referred to the deffered request.
	if (m_future.valid() && m_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
	{
		return;
	}

	m_future = std::async(std::launch::deferred, [this]()
	{
		m_index.clear();
		CAssetManager* pAssetManager = CAssetManager::GetInstance();
		pAssetManager->ForeachAsset([this](CAsset* pAsset)
		{
			MAKE_SURE(pAsset->GetFilesCount(), return);

			const std::vector<SAssetDependencyInfo>& dependencies = pAsset->GetDependencies();
			for (const auto& item : dependencies)
			{
				m_index.emplace_back(item.path, SAssetDependencyInfo( pAsset->GetFile(0), item.usageCount ));
			}
		});

		std::sort(m_index.begin(), m_index.end(), [](const auto& a, const auto& b)
		{
			return stricmp(a.first.c_str(), b.first.c_str()) < 0;
		});
	});
}

