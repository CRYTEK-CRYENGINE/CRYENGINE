// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DependencyManager.h"
#include <CryCore/StlUtils.h>

void DependencyManager::SetDependencies(const char* user, Strings& newUsedAssets)
{
	Strings& usedAssets = m_usedAssets[user];

	for (size_t i = 0; i < usedAssets.size(); ++i)
	{
		Strings& users = m_assetUsers[usedAssets[i]];
		stl::find_and_erase_all(users, string(user));
		if (users.empty())
			m_assetUsers.erase(usedAssets[i]);
	}

	usedAssets = newUsedAssets;

	for (size_t i = 0; i < usedAssets.size(); ++i)
	{
		Strings& users = m_assetUsers[usedAssets[i]];
		stl::push_back_unique(users, string(user));
	}
}

void DependencyManager::FindUsers(Strings* outUsers, const char* asset) const
{
	AssetUsers::const_iterator it = m_assetUsers.find(asset);
	if (it == m_assetUsers.end())
		return;
	const Strings& users = it->second;
	for (size_t i = 0; i < users.size(); ++i)
	{
		if (stl::push_back_unique(*outUsers, users[i]))
			FindUsers(outUsers, users[i]);
	}
}

void DependencyManager::FindDepending(Strings* outAssets, const char* user) const
{
	AssetUsers::const_iterator it = m_usedAssets.find(user);
	if (it == m_usedAssets.end())
		return;
	const Strings& assets = it->second;
	for (size_t i = 0; i < assets.size(); ++i)
	{
		if (stl::push_back_unique(*outAssets, assets[i]))
			FindUsers(&*outAssets, assets[i]);
	}
}

