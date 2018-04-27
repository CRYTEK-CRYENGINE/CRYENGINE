// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:
   GameStatsConfig - configurator for GameSpy Stats&Tracking service
   -------------------------------------------------------------------------
   History:
   - 9:9:2007   15:38 : Created by Stas Spivakov

*************************************************************************/

#ifndef __GAMESTATSCONFIG_H__
#define __GAMESTATSCONFIG_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryGame/IGameFramework.h>

class CGameStatsConfig : public IGameStatsConfig
{
	struct SKeyDef
	{
		SKeyDef(int key, const char* group) : m_key(key), m_group(group){}
		int    m_key;
		string m_group;
	};
	typedef std::map<string, SKeyDef> TKeyDefMap;

	struct SCategoryEntry
	{
		int    m_code;
		string m_keyName;
		string m_display;
	};

	struct SCategory
	{
		string                      m_name;
		int                         m_mod;
		std::vector<SCategoryEntry> m_entries;
	};

	typedef std::vector<SCategory> TCategoryVector;
public:
	CGameStatsConfig();
	~CGameStatsConfig();
	void ReadConfig();
	//////////////////////////////////////////////////////////////////////////
	// IGameStatsConfig
	virtual int         GetStatsVersion();
	virtual int         GetCategoryMod(const char* cat);
	virtual const char* GetValueNameByCode(const char* cat, int id);
	virtual int         GetKeyId(const char* key) const;
	//////////////////////////////////////////////////////////////////////////
	int                 GetCodeByKeyName(const char* cat, const char* key) const;
private:
	int             m_version;
	TKeyDefMap      m_map;
	TCategoryVector m_categories;
};

#endif //__GAMESTATSCONFIG_H__
