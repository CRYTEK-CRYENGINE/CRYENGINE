// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/platform.h>
#include "EditorCommonAPI.h"
#include <vector>
#include <map>

class EDITOR_COMMON_API DependencyManager
{
public:
	typedef std::vector<string> Strings;

	void SetDependencies(const char* asset, Strings& usedAssets);
	void FindUsers(Strings* users, const char* asset) const;
	void FindDepending(Strings* assets, const char* user) const;

private:
	typedef std::map<string, Strings, stl::less_stricmp<string>> UsedAssets;
	UsedAssets m_usedAssets;

	typedef std::map<string, Strings, stl::less_stricmp<string>> AssetUsers;
	AssetUsers m_assetUsers;
};

