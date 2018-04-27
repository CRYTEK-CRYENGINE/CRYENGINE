// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 15:9:2004   10:30 : Created by Mathieu Pinard
   - 20:10:2004   10:30 : Added IGameRulesSystem interface (Craig Tiller)

*************************************************************************/
#ifndef __GAMERULESYSTEM_H__
#define __GAMERULESYSTEM_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "IGameRulesSystem.h"

class CGameServerNub;

class CGameRulesSystem
	: public IGameRulesSystem
	  , public INetworkedClientListener
{
	typedef struct SGameRulesDef
	{
		string              extension;
		std::vector<string> aliases;
		std::vector<string> maplocs;

		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(extension);
			pSizer->AddObject(aliases);
			pSizer->AddObject(maplocs);
		}
	}SGameRulesDef;

	typedef std::map<string, SGameRulesDef> TGameRulesMap;
public:
	CGameRulesSystem(ISystem* pSystem, IGameFramework* pGameFW);
	~CGameRulesSystem();

	void Release() { delete this; };

	// IGameRulesSystem
	virtual bool        RegisterGameRules(const char* rulesName, const char* extensionName, bool bUseScript) override;
	virtual bool        CreateGameRules(const char* rulesName) override;
	virtual bool        DestroyGameRules() override;

	virtual void        AddGameRulesAlias(const char* gamerules, const char* alias) override;
	virtual void        AddGameRulesLevelLocation(const char* gamerules, const char* mapLocation) override;
	virtual const char* GetGameRulesLevelLocation(const char* gamerules, int i) override;

	virtual const char* GetGameRulesName(const char* alias) const override;

	virtual bool        HaveGameRules(const char* rulesName) override;

	virtual void        SetCurrentGameRules(IGameRules* pGameRules) override;
	virtual IGameRules* GetCurrentGameRules() const override;
	// ~IGameRulesSystem

	// INetworkedClientListener
	virtual void OnLocalClientDisconnected(EDisconnectionCause cause, const char* description) override;

	virtual bool OnClientConnectionReceived(int channelId, bool bIsReset) override;
	virtual bool OnClientReadyForGameplay(int channelId, bool bIsReset) override;
	virtual void OnClientDisconnected(int channelId, EDisconnectionCause cause, const char* description, bool bKeepClient) override;
	virtual bool OnClientTimingOut(int channelId, EDisconnectionCause cause, const char* description) override;
	// ~INetworkedClientListener

	void        GetMemoryStatistics(ICrySizer* s);

private:
	SGameRulesDef*           GetGameRulesDef(const char* name);
	static IEntityComponent* CreateGameObject(
	  IEntity* pEntity, SEntitySpawnParams& params, void* pUserData);

	IGameFramework* m_pGameFW;

	EntityId        m_currentGameRules;
	IGameRules*     m_pGameRules;

	TGameRulesMap   m_GameRules;
};

#endif // __GAMERULESYSTEM_H__
