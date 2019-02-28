// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
	- 11:09:2009  : Created by Thomas Houghton

*************************************************************************/

#ifndef __GAMERULESMPSPECTATOR_H__
#define __GAMERULESMPSPECTATOR_H__

#include "GameRulesModules/IGameRulesSpectatorModule.h"

class CGameRulesMPSpectator : public IGameRulesSpectatorModule
{
private:

	TChannelSpectatorModeMap  m_channelSpectatorModes;

	TSpawnLocations  m_spectatorLocations;

	CGameRules*  m_pGameRules;
	IGameFramework*  m_pGameFramework;
	IGameplayRecorder*  m_pGameplayRecorder;
	IActorSystem*  m_pActorSys; 

	uint8  m_eatsActorActions	:1;
	uint8  m_enableFree				:1;
	uint8  m_enableFollow			:1;
	uint8  m_enableFollowWhenNoLivesLeft : 1;
	uint8  m_enableKiller			:1;
	uint8  m_serverAlwaysAllowsSpectatorModeChange : 1;

	float  m_timeFromDeathTillAutoSpectate;

	struct SClientRequestInfo
	{
		SClientRequestInfo()	{	Reset();	}
		void Reset()
		{
			m_fRequestTimer = 0.0f;
			m_targetId			= 0;
			m_mode					= 0xFF;
		}

		float			m_fRequestTimer;
		EntityId	m_targetId;
		uint8			m_mode;
	} m_ClientInfo;

public:

	CGameRulesMPSpectator();
	virtual ~CGameRulesMPSpectator();

	virtual void Init(XmlNodeRef xml);
	virtual void PostInit();
	virtual void Update(float frameTime);

	virtual bool ModeIsEnabledForPlayer(const int mode, const EntityId playerId) const;
	virtual bool ModeIsAvailable(const EntityId playerId, const int mode, EntityId* outOthEnt);
	virtual int GetBestAvailableMode(const EntityId playerId, EntityId* outOthEnt);
	virtual int GetNextMode(EntityId playerId, int inc, EntityId* outOthEid);

	virtual void SvOnChangeSpectatorMode(EntityId playerId, int mode, EntityId targetId, bool resetAll);
	virtual void SvRequestSpectatorTarget( EntityId playerId, int change );

	virtual void ClOnChangeSpectatorMode(EntityId playerId, int mode, EntityId targetId, bool resetAll);

	virtual bool GetModeFromChannelSpectatorMap(int channelId, int* outMode) const;
	virtual void FindAndRemoveFromChannelSpectatorMap(int channelId);

	virtual EntityId GetNextSpectatorTarget(EntityId playerId, int change );
	virtual bool CanChangeSpectatorMode(EntityId playerId) const;
	virtual const char* GetActorSpectatorModeName(uint8 mode);

	virtual void ChangeSpectatorMode(const IActor* pActor, uint8 mode, EntityId targetId, bool resetAll, bool force = false);
	virtual void ChangeSpectatorModeBestAvailable(const IActor *pActor, bool resetAll);

	virtual void AddSpectatorLocation(EntityId location);
	virtual void RemoveSpectatorLocation(EntityId id);
	virtual int GetSpectatorLocationCount() const;
	virtual EntityId GetSpectatorLocation(int idx) const;
	virtual void GetSpectatorLocations(TSpawnLocations &locations) const;
	virtual EntityId GetRandomSpectatorLocation() const;
	virtual EntityId GetInterestingSpectatorLocation() const;
	virtual EntityId GetNextSpectatorLocation(const CActor* pPlayer) const;
	virtual EntityId GetPrevSpectatorLocation(const CActor* pPlayer) const;

	virtual float GetTimeFromDeathTillAutoSpectate() const { return m_timeFromDeathTillAutoSpectate; }
	virtual bool GetServerAlwaysAllowsSpectatorModeChange() const { return m_serverAlwaysAllowsSpectatorModeChange; }

	virtual void GetPostDeathDisplayDurations( EntityId playerId, int teamWhenKilled, bool isSuicide, bool isBulletTimeKill, float& rDeathCam, float& rKillerCam, float& rKillCam ) const;
};


#endif // __GAMERULESMPSPECTATOR_H__
