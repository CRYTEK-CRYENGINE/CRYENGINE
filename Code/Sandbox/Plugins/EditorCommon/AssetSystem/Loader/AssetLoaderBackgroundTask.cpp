// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetLoaderBackgroundTask.h"
#include "AssetLoader.h"
#include "AssetCache.h"
#include "AssetSystem/Asset.h"
#include "IBackgroundTaskManager.h"
#include "FilePathUtil.h"
#include <CryString\CryPath.h>
#include <future>

namespace AssetLoaderBackgroundTask_Private
{

static string GetCacheFilepath()
{
	return PathUtil::Make(PathUtil::GetUserSandboxFolder(), "assets.json");
}

class CSaveCacheBackgroundTask : public IBackgroundTask
{
public:
	static void Run(std::vector<CAsset*>&& assets)
	{
		GetIEditor()->GetBackgroundTaskManager()->AddTask(new CSaveCacheBackgroundTask(std::move(assets)), eTaskPriority_FileUpdate, eTaskThreadMask_IO);
	}

	virtual void Delete() override
	{
	}

	virtual ETaskResult Work() override
	{
		const string cacheFilename = GetCacheFilepath();
		return AssetLoader::SaveCache(cacheFilename, m_assets) ? eTaskResult_Completed : eTaskResult_Failed;
	}

	virtual void Finalize() override
	{
	}
private:
	CSaveCacheBackgroundTask(std::vector<CAsset*>&& assets)
		: m_assets(std::move(assets))
	{
	}

private:
	std::vector<CAsset*> m_assets;
};

class CAssetLoaderBackgroundTask : public IBackgroundTask
{
public:
	typedef std::function<void(const std::vector<CAsset*>&)> FinalizeFunc;

public:
	static void Run(const std::vector<string>& assetRootPaths, FinalizeFunc finalize)
	{
		GetIEditor()->GetBackgroundTaskManager()->AddTask(new CAssetLoaderBackgroundTask(assetRootPaths, std::move(finalize)), eTaskPriority_FileUpdate, eTaskThreadMask_IO);
	}

	virtual void Delete() override
	{
	}

	virtual ETaskResult Work() override
	{

		const string cacheFile = GetCacheFilepath();
		m_assets = AssetLoader::LoadAssetsCached(cacheFile, [this](AssetLoader::Predicate& predicate)
		{
			const bool bIgnorePaks = false;
			return AssetLoader::LoadAssets(m_assetRootPaths, bIgnorePaks, predicate);
		});

		return eTaskResult_Completed;
	}

	virtual void Finalize() override
	{
		if (m_finalize)
		{
			m_finalize(m_assets);
		}

		CSaveCacheBackgroundTask::Run(std::move(m_assets));
	}
private:
	CAssetLoaderBackgroundTask(const std::vector<string>& assetRootPaths, FinalizeFunc finalize)
		: m_assetRootPaths(assetRootPaths)
		, m_finalize(std::move(finalize))
	{
	}

private:
	std::vector<CAsset*> m_assets;
	std::vector<string> m_assetRootPaths;
	FinalizeFunc m_finalize;
};

}

namespace AssetLoaderBackgroundTask
{

void Run(const std::vector<string>& assetRootPaths, std::function<void(const std::vector<CAsset*>&)> finalize)
{
	using namespace AssetLoaderBackgroundTask_Private;
	CAssetLoaderBackgroundTask::Run(assetRootPaths, std::move(finalize));
}

};

