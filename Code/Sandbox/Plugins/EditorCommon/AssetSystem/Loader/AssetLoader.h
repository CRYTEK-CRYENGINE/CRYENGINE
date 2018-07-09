// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CAsset;

namespace AssetLoader
{
	//! Unary predicate which returns true for the required assets. 
	//! \param szAssetRelativePath Path relative to assets root.
	typedef std::function<bool(const char* szAssetRelativePath, uint64 timestamp)> Predicate;

	// Loads assets for which predicate returns true.
	std::vector<CAsset*> LoadAssets(const std::vector<string>& assetRootPaths, bool bIgnorePaks, Predicate predicate);
};
