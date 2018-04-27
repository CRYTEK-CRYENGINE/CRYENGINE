// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for GameRules

-------------------------------------------------------------------------
History:
- 23:2:2006   18:30 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_GAMERULES_H__
#define __SCRIPTBIND_GAMERULES_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>


class CGameRules;
class CActor;
struct IGameFramework;
struct ISystem;
struct HitInfo;


class CScriptBind_GameRules :
	public CScriptableBase
{
public:
	CScriptBind_GameRules(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_GameRules();

	virtual void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	void AttachTo(CGameRules *pGameRules);

	int IsServer(IFunctionHandler *pH);
	int IsClient(IFunctionHandler *pH);
	int IsMultiplayer(IFunctionHandler *pH);
	int CanCheat(IFunctionHandler *pH);

	int SpawnPlayer(IFunctionHandler *pH, int channelId, const char *name, const char *className, Vec3 pos, Vec3 angles);
	int Revive(IFunctionHandler *pH, ScriptHandle playerId);
	int RevivePlayer(IFunctionHandler *pH, ScriptHandle playerId, Vec3 pos, Vec3 angles, int teamId, bool clearInventory);
	int RevivePlayerInVehicle(IFunctionHandler *pH, ScriptHandle playerId, ScriptHandle vehicleId, int seatId, int teamId, bool clearInventory);
	int IsPlayer(IFunctionHandler *pH, ScriptHandle playerId);
	int IsProjectile(IFunctionHandler *pH, ScriptHandle entityId);

	int AddSpawnLocation(IFunctionHandler *pH, ScriptHandle entityId, bool isInitialSpawn, bool doVisTest, const char *pGroupName);
	int RemoveSpawnLocation(IFunctionHandler *pH, ScriptHandle id, bool isInitialSpawn);
	int EnableSpawnLocation(IFunctionHandler *pH, ScriptHandle entityId, bool isInitialSpawn, const char *pGroupName);
	int DisableSpawnLocation(IFunctionHandler *pH, ScriptHandle id, bool isInitialSpawn);
	int GetFirstSpawnLocation(IFunctionHandler *pH, int teamId);

	int AddSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId);
	int AddSpawnLocationToSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId, ScriptHandle location);
	int RemoveSpawnLocationFromSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId, ScriptHandle location);
	int RemoveSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId);
	int GetSpawnGroups(IFunctionHandler *pH);
	int IsSpawnGroup(IFunctionHandler *pH, ScriptHandle entityId);

	int SetPlayerSpawnGroup(IFunctionHandler *pH, ScriptHandle playerId, ScriptHandle groupId);

	int AddSpectatorLocation(IFunctionHandler *pH, ScriptHandle location);
	int RemoveSpectatorLocation(IFunctionHandler *pH, ScriptHandle id);
	
	int ServerExplosion(IFunctionHandler *pH, ScriptHandle shooterId, ScriptHandle weaponId, float dmg,
		Vec3 pos, Vec3 dir, float radius, float angle, float pressure, float holesize);
	int ServerHit(IFunctionHandler *pH, ScriptHandle targetId, ScriptHandle shooterId, ScriptHandle weaponId, float dmg, float radius, int materialId, int partId, int typeId);
	int ClientSelfHarm(IFunctionHandler *pH, float dmg, int materialId, int partId, int typeId, Vec3 dir);
	int ClientSelfHarmByEntity(IFunctionHandler *pH, ScriptHandle sourceEntity, float dmg, int materialId, int partId, int typeId, Vec3 dir);
	int ServerHarmVehicle(IFunctionHandler *pH, ScriptHandle vehicle, float dmg, int materialId, int typeId, Vec3 dir);

	int GetTeamName(IFunctionHandler *pH, int teamId);
	int GetTeamId(IFunctionHandler *pH, const char *teamName);

	int SetTeam(IFunctionHandler *pH, int teamId, ScriptHandle playerId);
	int ClientSetTeam(IFunctionHandler *pH, int teamId, ScriptHandle playerId);	// Specific usage case spawn points
	int GetTeam(IFunctionHandler *pH, ScriptHandle playerId);

	int ForbiddenAreaWarning(IFunctionHandler *pH, bool active, int timer, ScriptHandle targetId);

	int Announce(IFunctionHandler *pH, ScriptHandle playerId, const char *announcement, int context);

	int GetServerTime(IFunctionHandler *pH);

	int	EndGame(IFunctionHandler *pH);
	int NextLevel(IFunctionHandler *pH);

	int GetHitMaterialId(IFunctionHandler *pH, const char *materialName);
	int GetHitTypeId(IFunctionHandler *pH, const char *type);
	int GetHitType(IFunctionHandler *pH, int id);
	int IsHitTypeIdMelee(IFunctionHandler *pH, int hitTypeId);

	int IsDemoMode(IFunctionHandler *pH);

  int DebugCollisionDamage(IFunctionHandler *pH);

	int SendDamageIndicator(IFunctionHandler* pH, ScriptHandle targetId, ScriptHandle shooterId, ScriptHandle weaponId, Vec3 dir, float damage, int projectileClassId, int hitTypeId);

	int EnteredGame(IFunctionHandler* pH);

	int Watch(IFunctionHandler *pH, const char* text);

	int DemiGodDeath(IFunctionHandler *pH);
	int GetPrimaryTeam(IFunctionHandler *pH);

	int AddForbiddenArea(IFunctionHandler *pH, ScriptHandle entityId);
	int RemoveForbiddenArea(IFunctionHandler *pH, ScriptHandle entityId);
	int MakeMovementVisibleToAI(IFunctionHandler *pH, const char* entityClass);
	int SendGameRulesObjectiveEntitySignal(IFunctionHandler *pH, ScriptHandle entityId, int signal);

	int ReRecordEntity(IFunctionHandler *pH, ScriptHandle entityId);
	
	int ShouldGiveLocalPlayerHitFeedback2DSound(IFunctionHandler *pH, float damage);
	int CanUsePowerStruggleNode(IFunctionHandler *pH, ScriptHandle userId, ScriptHandle entityId);

private:
	void RegisterGlobals();
	void RegisterMethods();

	CGameRules *GetGameRules(IFunctionHandler *pH);
	CActor *GetActor(EntityId id);

	void GetOptionalHitInfoParams(IFunctionHandler *pH, HitInfo &hitInfo);

	SmartScriptTable	m_players;
	SmartScriptTable	m_teamplayers;
	SmartScriptTable	m_spawnlocations;
	SmartScriptTable	m_spectatorlocations;
	SmartScriptTable	m_spawngroups;

	ISystem						*m_pSystem;
	IGameFramework		*m_pGameFW;
};


#endif //__SCRIPTBIND_GAMERULES_H__
