// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <future> 

class CAsset;

//! Dependency Tracker allows dependencies between assets to be tracked.
class EDITOR_COMMON_API CDependencyTracker
{
	friend class CAssetManager;
	CDependencyTracker();
public:
	virtual ~CDependencyTracker();

	//! Returns a list of assets that use the specified file.
	//! \param szAssetPath The asset to enumerate reverse dependencies.
	//! \return Returns a collection of SAssetDependencyInfo, where
	// the string part of each item is the path relative to the assets root directory, and 
	// the integer value contains the instance count for the dependency or 0 if such information is not available.
	std::vector<SAssetDependencyInfo> GetReverseDependencies(const char* szAssetPath) const;
private:
	void CreateIndex();

private:
	std::vector<std::pair<string, SAssetDependencyInfo>> m_index;
	std::future<void> m_future;
};




