// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 9:9:2007   15:38 : Created by Stas Spivakov

*************************************************************************/

#include "StdAfx.h"
#include "CryAction.h"
#include "GameStats.h"
#include "GameStatsConfig.h"

static const char* gConfigFileName = "Scripts/Network/StatsConfig.xml";

CGameStatsConfig::CGameStatsConfig() :
	m_version(1)//defaults to 1
{

}

CGameStatsConfig::~CGameStatsConfig()
{

}

void CGameStatsConfig::ReadConfig()
{
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(gConfigFileName);
	if (!root)
	{
		return;
	}

	if (!root->isTag("StatsConfig"))
	{
		return;
	}

	if (!root->getChildCount())
		return;

	if (root->haveAttr("version"))
	{
		m_version = atoi(root->getAttr("version"));
	}

	for (int i = 0; i < root->getChildCount(); ++i)
	{
		XmlNodeRef ch = root->getChild(i);
		if (!ch || !ch->getChildCount())
			continue;

		if (ch->isTag("group"))
		{
			string groupName = ch->getAttr("name");
			for (int j = 0; j < ch->getChildCount(); ++j)
			{
				XmlNodeRef st = ch->getChild(j);
				if (st->haveAttr("keyName") && st->haveAttr("keyId"))
				{
					int keyId = max(0, atoi(st->getAttr("keyId")));
					string keyName = st->getAttr("keyName");
					std::pair<TKeyDefMap::iterator, bool> res = m_map.insert(std::make_pair(keyName, SKeyDef(keyId, keyName)));
					if (!res.second)
					{
						GameWarning("Duplicate stats def in group %s : stat \'%s\' id %d", groupName.c_str(), keyName.c_str(), keyId);
					}
				}
			}
		}

		if (ch->isTag("category"))
		{
			SCategory cat;
			cat.m_name = ch->getAttr("name");
			cat.m_mod = atoi(ch->getAttr("mod"));
			for (int j = 0; j < ch->getChildCount(); ++j)
			{
				XmlNodeRef st = ch->getChild(j);
				SCategoryEntry ent;
				if (st->haveAttr("code") && st->haveAttr("display"))
				{
					ent.m_code = max(0, atoi(st->getAttr("code")));
					ent.m_display = st->getAttr("display");

					//key
					if (st->haveAttr("keyName") && st->haveAttr("keyId"))
					{
						int keyId = max(0, atoi(st->getAttr("keyId")));
						string keyName = st->getAttr("keyName");

						ent.m_keyName = keyName;

						std::pair<TKeyDefMap::iterator, bool> res = m_map.insert(std::make_pair(keyName, SKeyDef(keyId, keyName)));
						if (!res.second)
						{
							GameWarning("Duplicate stats def in category %s : stat \'%s\' id %d", cat.m_name.c_str(), keyName.c_str(), keyId);
						}
					}

					cat.m_entries.push_back(ent);
				}
			}
			m_categories.push_back(cat);
		}
	}
}

int CGameStatsConfig::GetStatsVersion()
{
	return m_version;
}

int CGameStatsConfig::GetCategoryMod(const char* cat)
{
	for (int i = 0; i < m_categories.size(); ++i)
	{
		if (m_categories[i].m_name == cat)
		{
			return m_categories[i].m_mod;
		}
	}
	return 0;
}

const char* CGameStatsConfig::GetValueNameByCode(const char* cat, int id)
{
	for (int i = 0; i < m_categories.size(); ++i)
	{
		if (m_categories[i].m_name == cat)
		{
			const SCategory& c = m_categories[i];
			for (int j = 0; j < c.m_entries.size(); ++j)
			{
				if (c.m_entries[j].m_code == id)
					return c.m_entries[j].m_display;
			}
		}
	}
	return "";
}

int CGameStatsConfig::GetKeyId(const char* key) const
{
	TKeyDefMap::const_iterator it = m_map.find(CONST_TEMP_STRING(key));
	if (it != m_map.end())
		return it->second.m_key;
	else
		return -1;
}

int CGameStatsConfig::GetCodeByKeyName(const char* cat, const char* key) const
{
	for (int i = 0; i < m_categories.size(); ++i)
	{
		if (m_categories[i].m_name == cat)
		{
			const SCategory& c = m_categories[i];
			for (int j = 0; j < c.m_entries.size(); ++j)
			{
				if (c.m_entries[j].m_keyName == key)
					return c.m_entries[j].m_code;
			}
		}
	}
	return -1;
}
