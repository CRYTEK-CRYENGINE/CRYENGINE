// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CAsset;

namespace AssetLoader
{

	//! Unary predicate which returns true for the required assets. 
	//! \param szAssetRelativePath Path relative to assets root.
	typedef std::function<bool(const char* szAssetRelativePath, uint64 timestamp)> Predicate;

	// Should load assets for which predicate returns true.
	typedef std::function<std::vector<CAsset*>(Predicate predicate)> AssetLoaderFunction;

	// Loads assets, using the specified cache file to speed up the loading time.
	std::vector<CAsset*> LoadAssetsCached(const char* szCacheFilename, AssetLoaderFunction assetLoader);

	// Saves collection of assets as a cache file.
	bool SaveCache(const char* szCacheFilename, std::vector<CAsset*>& assets);
};
