// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IPakSystem.h"
#include "DBATableEnumerator.h"
#include "Plugins/EditorAnimation/Shared/DBATable.h"
#include "Plugins/EditorAnimation/Shared/AnimSettings.h"
#include "FileUtil.h"
#include "PathHelpers.h"

string UnifiedPath(const string& path);

using std::vector;

DBATableEnumerator::DBATableEnumerator()
{
}

DBATableEnumerator::~DBATableEnumerator()
{
}

bool DBATableEnumerator::LoadDBATable(const string& animConfigFolder, const string& sourceFolder, IPakSystem* pak, ICryXML* xml)
{
	m_table.reset(new SDBATable());
	if (!m_table->Load(animConfigFolder + "\\DBATable.json"))
	{
		RCLogError("Failed to load DBATable.json");
		return false;
	}

	vector<SAnimationFilterItem> filterItems;
	{
		vector<string> cafFileList;
		FileUtil::ScanDirectory(sourceFolder, "*.i_caf", cafFileList, true, "");

		for (size_t i = 0; i < cafFileList.size(); ++i)
		{
			const string& path = cafFileList[i];

			string animSettingsPath = PathUtil::ReplaceExtension(path, "animsettings");
			string animSettingsFullPath = sourceFolder + "\\" + animSettingsPath;
			SAnimSettings animSettings;
			if (PakSystemFile* f = pak->Open(animSettingsFullPath.c_str(), "rb"))
			{
				pak->Close(f);
			}
			else
			{
				continue;
			}

			if (!animSettings.Load(animSettingsFullPath, std::vector<string>(), xml, pak))
			{
				RCLogWarning("AnimationFilter: failed to load animsettings: %s", animSettingsFullPath.c_str());
			}

			SAnimationFilterItem item;
			item.path = UnifiedPath(PathUtil::ReplaceExtension(path, "caf"));
			item.path.replace('\\', '/');
			item.tags = animSettings.build.tags;
			item.skeletonAlias = animSettings.build.skeletonAlias;
			filterItems.push_back(item);
		}
	}

	m_dbas.clear();
	m_dbas.resize(m_table->entries.size());
	for (size_t i = 0; i < m_dbas.size(); ++i)
		m_dbas[i].path = m_table->entries[i].path;

	for (size_t i = 0; i < filterItems.size(); ++i)
	{
		const SAnimationFilterItem& f = filterItems[i];
		int dbaIndex = m_table->FindDBAForAnimation(f);
		if (dbaIndex == -1)
			continue;

		m_dbas[dbaIndex].animations.push_back(f.path);
	}

	for (size_t i = 0; i < m_dbas.size(); ++i)
		std::sort(m_dbas[i].animations.begin(), m_dbas[i].animations.end());
	return true;
}

void DBATableEnumerator::GetDBA(EnumeratedDBA* dba, int index) const
{
	if (size_t(index) >= m_dbas.size())
	{
		*dba = EnumeratedDBA();
		return;
	}
	const DBATableEntry& e = m_dbas[index];
	dba->innerPath = e.path;
	dba->animationCount = e.animations.size();
}

bool DBATableEnumerator::GetCAF(EnumeratedCAF* caf, int dbaIndex, int animationIndex) const
{
	if (size_t(dbaIndex) >= m_dbas.size())
		return false;

	const DBATableEntry& dba = m_dbas[dbaIndex];
	if (size_t(animationIndex) >= dba.animations.size())
		return false;

	caf->path = dba.animations[animationIndex];
	caf->skipDBA = false;
	return true;
}

const char* DBATableEnumerator::FindDBAPath(const char* animationPath, const char* skeleton, const std::vector<string>& tags) const
{
	if (!m_table.get())
		return 0;
	SAnimationFilterItem item;
	item.path = animationPath;
	item.skeletonAlias = skeleton;
	item.tags = tags;
	int index = m_table->FindDBAForAnimation(item);
	if (index < 0 || index >= m_table->entries.size())
		return 0;
	return m_table->entries[index].path.c_str();
}
