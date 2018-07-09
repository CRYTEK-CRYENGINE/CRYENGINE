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

#include "StdAfx.h"
#include "GameRulesModulesManager.h"
#include "GameRules.h"

#define GAMERULES_DEFINITIONS_XML_PATH		"Scripts/GameRules/GameModes.xml"

//------------------------------------------------------------------------
CGameRulesModulesManager* CGameRulesModulesManager::s_pInstance = NULL;

//------------------------------------------------------------------------
CGameRulesModulesManager * CGameRulesModulesManager::GetInstance( bool create /*= true*/ )
{
	if (create && !s_pInstance)
	{
		s_pInstance = new CGameRulesModulesManager();
	}
	return s_pInstance;
}

//------------------------------------------------------------------------
CGameRulesModulesManager::CGameRulesModulesManager()
{
	assert(!s_pInstance);
	s_pInstance = this;
}

//------------------------------------------------------------------------
CGameRulesModulesManager::~CGameRulesModulesManager()
{
	assert(s_pInstance == this);
	s_pInstance = NULL;
}

// Implement register and create functions for each module type
#define GAMERULES_MODULE_LIST_FUNC(type, name, lowerCase, useInEditor)	\
	void CGameRulesModulesManager::Register##name##Module( const char *moduleName, type *(*func)(), bool isAI )	\
	{	\
		m_##name##Classes.insert(TModuleClassMap_##name::value_type(moduleName, func));	\
	}	\
	type *CGameRulesModulesManager::Create##name##Module(const char *moduleName)	\
	{	\
		TModuleClassMap_##name::iterator ite = m_##name##Classes.find(moduleName);	\
		if (ite == m_##name##Classes.end())	\
		{	\
			GameWarning("Unknown GameRules module: <%s>", moduleName);	\
			return NULL;	\
		}	\
		return (*ite->second)();	\
	}

GAMERULES_MODULE_TYPES_LIST

#undef GAMERULES_MODULE_LIST_FUNC

//------------------------------------------------------------------------
void CGameRulesModulesManager::Init()
{
	XmlNodeRef root = gEnv->pSystem->LoadXmlFromFile( GAMERULES_DEFINITIONS_XML_PATH );
	if (root)
	{
		if (!stricmp(root->getTag(), "Modes"))
		{
			IGameRulesSystem *pGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();

			int numModes = root->getChildCount();

			for (int i = 0; i < numModes; ++ i)
			{
				XmlNodeRef modeXml = root->getChild(i);

				if (!stricmp(modeXml->getTag(), "GameMode"))
				{
					const char *modeName;

					if (modeXml->getAttr("name", &modeName))
					{
						pGameRulesSystem->RegisterGameRules(modeName, "GameRules");

						SGameRulesData gameRulesData;

						int numModeChildren = modeXml->getChildCount();
						for (int j = 0; j < numModeChildren; ++ j)
						{
							XmlNodeRef modeChildXml = modeXml->getChild(j);

							const char *nodeTag = modeChildXml->getTag();
							if (!stricmp(nodeTag, "Alias"))
							{
								const char *alias;
								if (modeChildXml->getAttr("name", &alias))
								{
									pGameRulesSystem->AddGameRulesAlias(modeName, alias);
								}
							}
							else if (!stricmp(nodeTag, "LevelLocation"))
							{
								const char *path;
								if (modeChildXml->getAttr("path", &path))
								{
									pGameRulesSystem->AddGameRulesLevelLocation(modeName, path);
								}
							}
							else if (!stricmp(nodeTag, "Rules"))
							{
								const char *path;
								if (modeChildXml->getAttr("path", &path))
								{
									gameRulesData.m_rulesXMLPath = path;
								}
							}
							else if( !stricmp(nodeTag, "DefaultHudState"))
							{
								const char *name;
								if (modeChildXml->getAttr("name", &name))
								{
									gameRulesData.m_defaultHud = name;
								}
							}
						}

						// Check if we're a team game
						int teamBased = 0;
						modeXml->getAttr("teamBased", teamBased);
						gameRulesData.m_bIsTeamGame = (teamBased != 0);

						// Check if this mode uses the gamelobby for team balancing
						int useLobbyTeamBalancing = 0;
						modeXml->getAttr("useLobbyTeamBalancing", useLobbyTeamBalancing);
						gameRulesData.m_bUseLobbyTeamBalancing = (useLobbyTeamBalancing != 0);

						// Check if this mode uses the player team visualization to change player models
						int usePlayerTeamVisualization = 1;
						modeXml->getAttr("usePlayerTeamVisualization", usePlayerTeamVisualization);
						gameRulesData.m_bUsePlayerTeamVisualization = (usePlayerTeamVisualization != 0);

						// Insert gamerule specific data
						m_rulesData.insert(TDataMap::value_type(modeName, gameRulesData));
					}
					else
					{
						CryLogAlways("CGameRulesModulesManager::Init(), invalid 'GameMode' node, requires 'name' attribute");
					}
				}
				else
				{
					CryLogAlways("CGameRulesModulesManager::Init(), invalid xml, expected 'GameMode' node, got '%s'", modeXml->getTag());
				}
			}
		}
		else
		{
			CryLogAlways("CGameRulesModulesManager::Init(), invalid xml, expected 'Modes' node, got '%s'", root->getTag());
		}
	}
}

//------------------------------------------------------------------------
const char * CGameRulesModulesManager::GetXmlPath( const char *gameRulesName ) const
{
	TDataMap::const_iterator it = m_rulesData.find(gameRulesName);
	if (it == m_rulesData.end())
	{
		return NULL;
	}
	return it->second.m_rulesXMLPath.c_str();
}

//------------------------------------------------------------------------
const char * CGameRulesModulesManager::GetDefaultHud( const char *gameRulesName ) const
{
	TDataMap::const_iterator it = m_rulesData.find(gameRulesName);
	if (it == m_rulesData.end())
	{
		return NULL;
	}
	return it->second.m_defaultHud.c_str();
}

//------------------------------------------------------------------------
int CGameRulesModulesManager::GetRulesCount()
{
	return m_rulesData.size();
}

//------------------------------------------------------------------------
const char* CGameRulesModulesManager::GetRules(int index)
{
	assert (index >= 0 && index < (int)m_rulesData.size());
	TDataMap::const_iterator iter = m_rulesData.begin();
	std::advance(iter, index);
	return iter->first.c_str();
}

//------------------------------------------------------------------------
bool CGameRulesModulesManager::IsTeamGame( const char *gameRulesName) const
{
	TDataMap::const_iterator it = m_rulesData.find(gameRulesName);
	if (it == m_rulesData.end())
	{
		return false;
	}
	return it->second.m_bIsTeamGame;
}

//------------------------------------------------------------------------
bool CGameRulesModulesManager::UsesLobbyTeamBalancing( const char *gameRulesName ) const
{
	TDataMap::const_iterator it = m_rulesData.find(gameRulesName);
	if (it == m_rulesData.end())
	{
		return false;
	}
	return it->second.m_bUseLobbyTeamBalancing;
}

//------------------------------------------------------------------------
bool CGameRulesModulesManager::UsesPlayerTeamVisualization( const char *gameRulesName ) const
{
	TDataMap::const_iterator it = m_rulesData.find(gameRulesName);
	if (it == m_rulesData.end())
	{
		return false;
	}
	return it->second.m_bUsePlayerTeamVisualization;
}

//------------------------------------------------------------------------
bool CGameRulesModulesManager::IsValidGameRules(const char *gameRulesName) const
{
	TDataMap::const_iterator it = m_rulesData.find(gameRulesName);
	if (it == m_rulesData.end())
	{
		return false;
	}
	return true;
}
