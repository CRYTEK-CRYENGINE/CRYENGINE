// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 

	-------------------------------------------------------------------------
	History:
	- 02:09:2009  : Created by Colin Gulliver

*************************************************************************/

#ifndef _GameRulesModulesManager_h_
#define _GameRulesModulesManager_h_

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesModulesManager.h"

class CGameRulesModulesManager : public IGameRulesModulesManager
{
private:
	static CGameRulesModulesManager *s_pInstance;

	typedef CryFixedStringT<32>	TFixedString_32;
	typedef CryFixedStringT<64>	TFixedString_64;

// Create class map and register/create functions for each module type
#define GAMERULES_MODULE_LIST_FUNC(type, name, lowerCase, useInEditor)	\
	private:	\
		typedef std::map<TFixedString_32, type*(*)()> TModuleClassMap_##name;	\
		TModuleClassMap_##name m_##name##Classes;	\
	public:		\
		void RegisterFactory(const char *moduleName, type*(*func)(), bool isAI) { Register##name##Module(moduleName, func, isAI); };	\
		void Register##name##Module(const char *moduleName, type*(*func)(), bool isAI);	\
		type *Create##name##Module(const char *moduleName);

	GAMERULES_MODULE_TYPES_LIST

#undef GAMERULES_MODULE_LIST_FUNC

	struct SGameRulesData{
		TFixedString_64 m_rulesXMLPath;
		TFixedString_64 m_defaultHud;
		bool m_bIsTeamGame;
		bool m_bUseLobbyTeamBalancing;
		bool m_bUsePlayerTeamVisualization;


		//ctor
		SGameRulesData() : m_rulesXMLPath(""), m_defaultHud(""), m_bIsTeamGame(false), m_bUseLobbyTeamBalancing(false), m_bUsePlayerTeamVisualization(true) {}
	};

	typedef std::map<TFixedString_32, SGameRulesData> TDataMap;
	TDataMap m_rulesData;

public:
	static CGameRulesModulesManager *GetInstance(bool create = true);

	CGameRulesModulesManager();
	virtual ~CGameRulesModulesManager();

	void Init();

	// Summary
	//	 Returns the path for the gamerules XML description.
	const char *GetXmlPath(const char *gameRulesName) const;

	// Summary
	//	 Returns the default HUDState name path for the given gamerules.
	const char *GetDefaultHud(const char *gameRulesName) const;

	// Summary
	//	Returns the number of game rules
	int GetRulesCount();

	// Summary
	//	Returns the name of the nth GameRules
	const char* GetRules(int index);

	// Summary
	//	Determines if the specified gameRules is a team game
	bool IsTeamGame(const char *gameRulesName) const;

	// Summary
	//	Determines if the specified gameRules uses team balancing
	bool UsesLobbyTeamBalancing(const char *gameRulesName) const;

	// Summary
	//	Determines if the specified gameRules uses team visualization
	bool UsesPlayerTeamVisualization(const char *gameRulesName) const;

	// Summary
	// Determines if game rules are valid
	bool IsValidGameRules(const char *gameRulesName) const;
};

#endif // _GameRulesModulesManager_h_
