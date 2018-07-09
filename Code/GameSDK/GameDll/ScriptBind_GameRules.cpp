// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 27:10:2004   11:29 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_GameRules.h"
#include "GameRules.h"
#include "Actor.h"
#include "Game.h"
#include "GameCVars.h"
#include "Environment/LedgeManager.h"
#include "IVehicleSystem.h"
#include "Player.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "EquipmentLoadout.h"
#include "Utility/CryWatch.h"

#include "GameRulesModules/IGameRulesDamageHandlingModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesStatsRecording.h"
#include "GameRulesModules/GameRulesObjective_PowerStruggle.h"

#include "GodMode.h"
#include "BodyDamage.h"
#include "Audio/Announcer.h"

#include "RecordingSystem.h"
#include "EnvironmentalWeapon.h"

#define SPEAR_PANEL_GRAB_CONE_SIZE DEG2RAD(45.f)

//------------------------------------------------------------------------
CScriptBind_GameRules::CScriptBind_GameRules(ISystem *pSystem, IGameFramework *pGameFramework)
: m_pSystem(pSystem),
	m_pGameFW(pGameFramework)
{
	Init(pSystem->GetIScriptSystem(), m_pSystem, 1);

	m_players.Create(m_pSS);
	m_teamplayers.Create(m_pSS);
	m_spawnlocations.Create(m_pSS);
	m_spawngroups.Create(m_pSS);
	m_spectatorlocations.Create(m_pSS);

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_GameRules::~CScriptBind_GameRules()
{
}

//------------------------------------------------------------------------
void CScriptBind_GameRules::RegisterGlobals()
{
	m_pSS->SetGlobalValue("TextMessageServer", eTextMessageServer);

	m_pSS->SetGlobalValue("eHeadShotType_None", CGameRules::eHeadShotType_None);
	m_pSS->SetGlobalValue("eHeadShotType_Head", CGameRules::eHeadShotType_Head);
	m_pSS->SetGlobalValue("eHeadShotType_Helmet", CGameRules::eHeadShotType_Helmet);

	m_pSS->SetGlobalValue("ChatToTarget", eChatToTarget);
	m_pSS->SetGlobalValue("ChatToTeam", eChatToTeam);
	m_pSS->SetGlobalValue("ChatToAll", eChatToAll);

	m_pSS->SetGlobalValue("TextMessageToAll", eRMI_ToAllClients);
	m_pSS->SetGlobalValue("TextMessageToAllRemote", eRMI_ToRemoteClients);
	m_pSS->SetGlobalValue("TextMessageToClient", eRMI_ToClientChannel);

 	m_pSS->SetGlobalValue("eTE_TurretUnderAttack", 1);
 	m_pSS->SetGlobalValue("eTE_GameOverWin", 2);
 	m_pSS->SetGlobalValue("eTE_GameOverLose", 3);
 	m_pSS->SetGlobalValue("eTE_TACTankStarted", 4);
 	m_pSS->SetGlobalValue("eTE_SingularityStarted", 5);
 	m_pSS->SetGlobalValue("eTE_TACTankCompleted", 6);
	m_pSS->SetGlobalValue("eTE_TACLauncherCompleted", 7);
 	m_pSS->SetGlobalValue("eTE_SingularityCompleted", 8);
	m_pSS->SetGlobalValue("eTE_EnemyNearBase", 9);
	m_pSS->SetGlobalValue("eTE_Promotion", 10);
	m_pSS->SetGlobalValue("eTE_Reactor50", 11);
	m_pSS->SetGlobalValue("eTE_Reactor100", 12);
	m_pSS->SetGlobalValue("eTE_ApproachEnemyHq", 13);
	m_pSS->SetGlobalValue("eTE_ApproachEnemySub", 14);
	m_pSS->SetGlobalValue("eTE_ApproachEnemyCarrier", 15);	
}

//------------------------------------------------------------------------
void CScriptBind_GameRules::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_GameRules::

	SCRIPT_REG_TEMPLFUNC(IsServer, "");
	SCRIPT_REG_TEMPLFUNC(IsClient, "");
	SCRIPT_REG_TEMPLFUNC(IsMultiplayer, "");
	SCRIPT_REG_TEMPLFUNC(CanCheat, "");

	SCRIPT_REG_TEMPLFUNC(SpawnPlayer, "channelId, name, className, pos, angles");
	SCRIPT_REG_TEMPLFUNC(Revive, "playerId");
	SCRIPT_REG_TEMPLFUNC(RevivePlayer, "playerId, pos, angles, teamId, clearInventory");
	SCRIPT_REG_TEMPLFUNC(RevivePlayerInVehicle, "playerId, vehicleId, seatId, teamId, clearInventory");
	SCRIPT_REG_TEMPLFUNC(IsPlayer, "playerId");
	SCRIPT_REG_TEMPLFUNC(IsProjectile, "entityId");
	
	SCRIPT_REG_TEMPLFUNC(AddSpawnLocation, "entityId, isInitialSpawn, doVisTest, pGroupName");
	SCRIPT_REG_TEMPLFUNC(RemoveSpawnLocation, "id, isInitialSpawn");
	SCRIPT_REG_TEMPLFUNC(EnableSpawnLocation, "entityId, isInitialSpawn, pGroupName");
	SCRIPT_REG_TEMPLFUNC(DisableSpawnLocation, "id, isInitialSpawn");
	SCRIPT_REG_TEMPLFUNC(GetFirstSpawnLocation, "teamId");

	SCRIPT_REG_TEMPLFUNC(AddSpawnGroup, "groupId");
	SCRIPT_REG_TEMPLFUNC(AddSpawnLocationToSpawnGroup, "groupId, location");
	SCRIPT_REG_TEMPLFUNC(RemoveSpawnLocationFromSpawnGroup, "groupId, location");
	SCRIPT_REG_TEMPLFUNC(RemoveSpawnGroup, "groupId");
	SCRIPT_REG_TEMPLFUNC(GetSpawnGroups, "");
	SCRIPT_REG_TEMPLFUNC(IsSpawnGroup, "entityId");

	SCRIPT_REG_TEMPLFUNC(SetPlayerSpawnGroup, "playerId, groupId");

	SCRIPT_REG_TEMPLFUNC(AddSpectatorLocation, "location");
	SCRIPT_REG_TEMPLFUNC(RemoveSpectatorLocation, "id");
	

	SCRIPT_REG_TEMPLFUNC(ServerExplosion, "shooterId, weaponId, dmg, pos, dir, radius, angle, press, holesize, [effect], [effectScale]");
	SCRIPT_REG_TEMPLFUNC(ServerHit, "targetId, shooterId, weaponId, dmg, radius, materialId, partId, typeId, [pos], [dir], [normal]");
	SCRIPT_REG_TEMPLFUNC(ClientSelfHarm, "dmg, materialId, partId, typeId, dir");
	SCRIPT_REG_TEMPLFUNC(ClientSelfHarmByEntity, "sourceEntity, dmg, materialId, partId, typeId, dir");
	SCRIPT_REG_TEMPLFUNC(ServerHarmVehicle, "vehicle, dmg, materialId, typeId, dir");

	SCRIPT_REG_TEMPLFUNC(GetTeamName, "teamId");
	SCRIPT_REG_TEMPLFUNC(GetTeamId, "teamName");
	
	SCRIPT_REG_TEMPLFUNC(SetTeam, "teamId, playerId");
	SCRIPT_REG_TEMPLFUNC(ClientSetTeam, "teamId, playerId");
	SCRIPT_REG_TEMPLFUNC(GetTeam, "playerId");

	SCRIPT_REG_TEMPLFUNC(Announce, "playerId, msg, context");

	SCRIPT_REG_TEMPLFUNC(ForbiddenAreaWarning, "active, timer, targetId");

	SCRIPT_REG_TEMPLFUNC(GetServerTime, "");

	SCRIPT_REG_TEMPLFUNC(EndGame, "");
	SCRIPT_REG_TEMPLFUNC(NextLevel, "");

	SCRIPT_REG_TEMPLFUNC(GetHitMaterialId, "materialName");
	SCRIPT_REG_TEMPLFUNC(GetHitTypeId, "type");
	SCRIPT_REG_TEMPLFUNC(GetHitType, "id");
	SCRIPT_REG_TEMPLFUNC(IsHitTypeIdMelee, "hitTypeId");

	SCRIPT_REG_TEMPLFUNC(IsDemoMode, "");

	SCRIPT_REG_TEMPLFUNC(DebugCollisionDamage, "");

	SCRIPT_REG_TEMPLFUNC(SendDamageIndicator, "targetId, shooterId, weaponId, dir, damage, projectileClassId, hitTypeId");

	SCRIPT_REG_TEMPLFUNC(EnteredGame, "");

	SCRIPT_REG_TEMPLFUNC(Watch, "text");

	SCRIPT_REG_TEMPLFUNC(DemiGodDeath, "");

	SCRIPT_REG_TEMPLFUNC(GetPrimaryTeam, "");

	SCRIPT_REG_TEMPLFUNC(AddForbiddenArea, "entityId");
	SCRIPT_REG_TEMPLFUNC(RemoveForbiddenArea, "entityId");

	SCRIPT_REG_TEMPLFUNC(MakeMovementVisibleToAI, "entityClass");
	SCRIPT_REG_TEMPLFUNC(SendGameRulesObjectiveEntitySignal, "entityId, signal");

	SCRIPT_REG_TEMPLFUNC(ReRecordEntity, "entityId");
	
	SCRIPT_REG_TEMPLFUNC(ShouldGiveLocalPlayerHitFeedback2DSound, "damage");
	SCRIPT_REG_TEMPLFUNC(CanUsePowerStruggleNode, "userId, entityId");
}

//------------------------------------------------------------------------
CGameRules *CScriptBind_GameRules::GetGameRules(IFunctionHandler *pH)
{
	return static_cast<CGameRules *>(m_pGameFW->GetIGameRulesSystem()->GetCurrentGameRules());
}

//------------------------------------------------------------------------
CActor *CScriptBind_GameRules::GetActor(EntityId id)
{
	return static_cast<CActor *>(m_pGameFW->GetIActorSystem()->GetActor(id));
}

//------------------------------------------------------------------------
void CScriptBind_GameRules::AttachTo(CGameRules *pGameRules)
{
	IScriptTable *pScriptTable = pGameRules->GetEntity()->GetScriptTable();

	if (pScriptTable)
	{
		SmartScriptTable thisTable(m_pSS);
		thisTable->Delegate(GetMethodsTable());

		pScriptTable->SetValue("game", thisTable);
	}
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::IsServer(IFunctionHandler *pH)
{
	CGameRules *pGameRules=GetGameRules(pH);
	if (!pGameRules)
		return pH->EndFunction();

	return pH->EndFunction(gEnv->bServer);
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::IsClient(IFunctionHandler *pH)
{
	CGameRules *pGameRules=GetGameRules(pH);
	if (!pGameRules)
		return pH->EndFunction();

	return pH->EndFunction(gEnv->IsClient());
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::IsMultiplayer(IFunctionHandler *pH)
{
	CGameRules *pGameRules=GetGameRules(pH);
	if (!pGameRules)
		return pH->EndFunction();

	return pH->EndFunction(gEnv->bMultiplayer);
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::CanCheat(IFunctionHandler *pH)
{
	CGameRules *pGameRules=GetGameRules(pH);
	if (!pGameRules)
		return pH->EndFunction();

	if (g_pGame->GetIGameFramework()->CanCheat())
		return pH->EndFunction(1);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::SpawnPlayer(IFunctionHandler *pH, int channelId, const char *name, const char *className, Vec3 pos, Vec3 angles)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	IActor *pActor = pGameRules->SpawnPlayer(channelId, name, className, pos, Ang3(angles));

	if (pActor)
		return pH->EndFunction(pActor->GetEntity()->GetScriptTable());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::Revive(IFunctionHandler *pH, ScriptHandle playerId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	CActor *pActor = GetActor((EntityId)playerId.n);

	if (pActor != NULL && pActor->IsDead())
	{
		IEntity *pActorEntity = pActor->GetEntity();
		pGameRules->RevivePlayer(pActor, pActorEntity->GetWorldPos(), pActorEntity->GetWorldAngles(), 0, MP_MODEL_INDEX_DEFAULT, false);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::RevivePlayer(IFunctionHandler *pH, ScriptHandle playerId, Vec3 pos, Vec3 angles, int teamId, bool clearInventory)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	CActor *pActor = GetActor((EntityId)playerId.n);

	if (pActor)
		pGameRules->RevivePlayer(pActor, pos, Ang3(angles), teamId, MP_MODEL_INDEX_DEFAULT, clearInventory);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::RevivePlayerInVehicle(IFunctionHandler *pH, ScriptHandle playerId, ScriptHandle vehicleId, int seatId, int teamId, bool clearInventory)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	CActor *pActor = GetActor((EntityId)playerId.n);

	if (pActor)
		pGameRules->RevivePlayerInVehicle(pActor, (EntityId)vehicleId.n, seatId, teamId, clearInventory);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
// returns true if the id is a player
//------------------------------------------------------------------------
int CScriptBind_GameRules::IsPlayer(IFunctionHandler *pH, ScriptHandle playerId)
{
	CGameRules *pGameRules = GetGameRules( pH );

	if (pGameRules != NULL && pGameRules->IsPlayer( (EntityId)playerId.n) )
		return pH->EndFunction( true );

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::IsProjectile(IFunctionHandler *pH, ScriptHandle entityId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	return pH->EndFunction(pGameRules->IsProjectile((EntityId)entityId.n));
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::AddSpawnLocation(IFunctionHandler *pH, ScriptHandle entityId, bool isInitialSpawn, bool doVisTest, const char *pGroupName)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
	{
		pGameRules->AddSpawnLocation((EntityId)entityId.n, isInitialSpawn, doVisTest, pGroupName);
	}
	else 
	{
		GameWarning("[SPAWN POINT]Trying to add spawn location, but GameRules does not exist yet!");
	}
	
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::RemoveSpawnLocation(IFunctionHandler *pH, ScriptHandle id, bool isInitialSpawn)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
		pGameRules->RemoveSpawnLocation((EntityId)id.n, isInitialSpawn);
	
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::EnableSpawnLocation(IFunctionHandler *pH, ScriptHandle id, bool isInitialSpawn, const char *pGroupName)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
		pGameRules->EnableSpawnLocation((EntityId)id.n, isInitialSpawn, pGroupName);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::DisableSpawnLocation(IFunctionHandler *pH, ScriptHandle id, bool isInitialSpawn)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
		pGameRules->DisableSpawnLocation((EntityId)id.n, isInitialSpawn);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::GetFirstSpawnLocation(IFunctionHandler *pH, int teamId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
	{
		EntityId id=pGameRules->GetFirstSpawnLocation(teamId);
		if (id)
			return pH->EndFunction(ScriptHandle(id));
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::AddSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
		pGameRules->AddSpawnGroup((EntityId)groupId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::AddSpawnLocationToSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId, ScriptHandle location)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
		pGameRules->AddSpawnLocationToSpawnGroup((EntityId)groupId.n, (EntityId)location.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::RemoveSpawnLocationFromSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId, ScriptHandle location)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
		pGameRules->RemoveSpawnLocationFromSpawnGroup((EntityId)groupId.n, (EntityId)location.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::RemoveSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
		pGameRules->RemoveSpawnGroup((EntityId)groupId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::GetSpawnGroups(IFunctionHandler *pH)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	int teamId=-1;
	if (pH->GetParamCount()>0 && pH->GetParamType(1)==svtNumber)
		pH->GetParam(1, teamId);

	int count=pGameRules->GetSpawnGroupCount();
	if (!count)
		return pH->EndFunction();

	int tcount=m_spawngroups->Count();

	int i=0;
	int k=0;
	while(i<count)
	{
		EntityId groupId=pGameRules->GetSpawnGroup(i);
		if (teamId==-1 || teamId==pGameRules->GetTeam(groupId))
			m_spawngroups->SetAt(k++, ScriptHandle(groupId));
		++i;
	}

	while(i<tcount)
		m_spawngroups->SetNullAt(++i);

	return pH->EndFunction(m_spawngroups);
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::IsSpawnGroup(IFunctionHandler *pH, ScriptHandle entityId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	if (pGameRules->IsSpawnGroup((EntityId)entityId.n))
		return pH->EndFunction(true);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::SetPlayerSpawnGroup(IFunctionHandler *pH, ScriptHandle playerId, ScriptHandle groupId)
{
	CGameRules *pGameRules=GetGameRules(pH);
	if (pGameRules)
		pGameRules->SetPlayerSpawnGroup((EntityId)playerId.n, (EntityId)groupId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::AddSpectatorLocation(IFunctionHandler *pH, ScriptHandle location)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules != NULL && pGameRules->GetSpectatorModule())
		pGameRules->GetSpectatorModule()->AddSpectatorLocation((EntityId)location.n);

	return pH->EndFunction();

}

//------------------------------------------------------------------------
int CScriptBind_GameRules::RemoveSpectatorLocation(IFunctionHandler *pH, ScriptHandle id)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules != NULL && pGameRules->GetSpectatorModule())
		pGameRules->GetSpectatorModule()->RemoveSpectatorLocation((EntityId)id.n);

	return pH->EndFunction();

}

//------------------------------------------------------------------------
int CScriptBind_GameRules::ServerExplosion(IFunctionHandler *pH, ScriptHandle shooterId, ScriptHandle weaponId,
																					 float dmg, Vec3 pos, Vec3 dir, float radius, float angle, float pressure, float holesize)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!gEnv->bServer)
		return pH->EndFunction();

	const char *effect="";
	float effectScale=1.0f;
	int type=0;

	const int paramCount = pH->GetParamCount();

	if (paramCount > 9 && pH->GetParamType(10)!=svtNull)
		pH->GetParam(10, effect);

	if (paramCount > 10 && pH->GetParamType(11)!=svtNull)
		pH->GetParam(11, effectScale);

	if (paramCount > 11 && pH->GetParamType(12)!=svtNull)
		pH->GetParam(12, type);

	if (type == 0)
	{
		GameWarning("A script is creating an explosion with an invalid type! Making it a frag...");
		type = CGameRules::EHitType::Frag;
	}
	assert(type);

	float minRadius = radius/2.0f;
	float minPhysRadius = radius/2.0f;
	float physRadius = radius;
	float soundRadius = 0.0f; // Game code uses a hardcoded soundRadius of explosionRadius * 3 if this is at or below 0.
	if (paramCount > 12 && pH->GetParamType(13)!=svtNull)
		pH->GetParam(13, minRadius);
	if (paramCount > 13 && pH->GetParamType(14)!=svtNull)
		pH->GetParam(14, minPhysRadius);
	if (paramCount > 14 && pH->GetParamType(15)!=svtNull)
		pH->GetParam(15, physRadius);
	if (paramCount > 15 && pH->GetParamType(16)!=svtNull)
		pH->GetParam(16, soundRadius);

	int ffType = static_cast<int>(eFriendyFireNone);
	if (paramCount > 16 && pH->GetParamType(17)!=svtNull)
		pH->GetParam(17, ffType);

	ExplosionInfo info((EntityId)shooterId.n, (EntityId)weaponId.n, 0, dmg, pos, dir.GetNormalized(), minRadius, radius, minPhysRadius, physRadius, angle, pressure, holesize, 0);
	info.SetEffect(effect, effectScale, 0.0f);
	info.type = type;
	info.soundRadius = soundRadius;
	info.friendlyfire = static_cast<EFriendyFireType>(ffType); 


	pGameRules->QueueExplosion(info);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::ServerHit(IFunctionHandler *pH, ScriptHandle targetId, ScriptHandle shooterId, ScriptHandle weaponId,
																		 float dmg, float radius, int materialId, int partId, int typeId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!gEnv->bServer)
		return pH->EndFunction();

	HitInfo info((EntityId)shooterId.n, (EntityId)targetId.n, (EntityId)weaponId.n, dmg, radius, materialId, partId, typeId);

	GetOptionalHitInfoParams(pH, info);

	pGameRules->ServerHit(info);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::ClientSelfHarm(IFunctionHandler *pH, float dmg, int materialId, int partId, int typeId, Vec3 dir)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!gEnv->IsClient())
		return pH->EndFunction();

	EntityId ownId = g_pGame->GetIGameFramework()->GetClientActorId();
	HitInfo info(ownId, ownId, ownId, dmg, 0.0f, materialId, partId, typeId);
	info.dir = dir;

	pGameRules->ClientHit(info);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::ClientSelfHarmByEntity(IFunctionHandler *pH, ScriptHandle sourceEntity, float dmg, int materialId, int partId, int typeId, Vec3 dir)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!gEnv->IsClient())
		return pH->EndFunction();

	EntityId ownId = gEnv->pGameFramework->GetClientActorId();
	HitInfo info((EntityId)sourceEntity.n, ownId, ownId, dmg, 0.0f, materialId, partId, typeId);
	info.dir = dir;

	pGameRules->ClientHit(info);	

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::ServerHarmVehicle(IFunctionHandler *pH, ScriptHandle vehicle, float dmg, int materialId, int typeId, Vec3 dir)
{
	if( gEnv->bServer )
	{
		CGameRules *pGameRules=GetGameRules(pH);

		EntityId vehicleId = (EntityId)vehicle.n;

		if( IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle( vehicleId ) )
		{
			HitInfo info( vehicleId, vehicleId, vehicleId, dmg, 0.0f, materialId, 0, typeId );
			info.dir = dir;
			info.pos = pVehicle->GetEntity()->GetWorldPos();	//vehicle hit info's need a pos to deal damage

			pGameRules->ServerHit(info);	
		}
	}
	return pH->EndFunction();
}


//------------------------------------------------------------------------
void CScriptBind_GameRules::GetOptionalHitInfoParams(IFunctionHandler *pH, HitInfo &hitInfo)
{
	if (pH->GetParamCount()>8 && pH->GetParamType(9)!=svtNull)
		pH->GetParam(9, hitInfo.pos);

	if (pH->GetParamCount()>9 && pH->GetParamType(10)!=svtNull)
		pH->GetParam(10, hitInfo.dir);

	if (pH->GetParamCount()>10 && pH->GetParamType(11)!=svtNull)
		pH->GetParam(11, hitInfo.normal);
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::GetTeamName(IFunctionHandler *pH, int teamId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	const char *name=pGameRules->GetTeamName(teamId);
	if (name)
		return pH->EndFunction(name);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::GetTeamId(IFunctionHandler *pH, const char *teamName)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	int id=pGameRules->GetTeamId(teamName);
	if (id)
		return pH->EndFunction(id);

	return pH->EndFunction();
}


//------------------------------------------------------------------------
int CScriptBind_GameRules::SetTeam(IFunctionHandler *pH, int teamId, ScriptHandle playerId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	pGameRules->SetTeam(teamId, (EntityId)playerId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::ClientSetTeam(IFunctionHandler *pH, int teamId, ScriptHandle playerId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	pGameRules->ClDoSetTeam(teamId, (EntityId)playerId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::GetTeam(IFunctionHandler *pH, ScriptHandle playerId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	return pH->EndFunction(pGameRules->GetTeam((EntityId)playerId.n));
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::ForbiddenAreaWarning(IFunctionHandler *pH, bool active, int timer, ScriptHandle targetId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	pGameRules->ForbiddenAreaWarning(active, timer, (EntityId)targetId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::GetServerTime(IFunctionHandler *pH)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (!pGameRules)
		return pH->EndFunction();

	return pH->EndFunction(pGameRules->GetServerTime());
}

//------------------------------------------------------------------------
int	CScriptBind_GameRules::EndGame(IFunctionHandler *pH)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
		pGameRules->OnEndGame();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::NextLevel(IFunctionHandler *pH)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
		pGameRules->NextLevel();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::GetHitMaterialId(IFunctionHandler *pH, const char *materialName)
{
	IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
	assert(pMaterialManager);

	return pH->EndFunction(pMaterialManager->GetSurfaceTypeIdByName(materialName));
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::GetHitTypeId(IFunctionHandler *pH, const char *type)
{
	CGameRules *pGameRules=GetGameRules(pH);

	return pH->EndFunction(pGameRules->GetHitTypeId(type));
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::GetHitType(IFunctionHandler *pH, int id)
{
	CGameRules *pGameRules=GetGameRules(pH);

	return pH->EndFunction(pGameRules->GetHitType(id));
}


//------------------------------------------------------------------------
// In:  The hit type ID. In the Lua damage handling methods, the hitTypeID 
//      value can also be obtained from the "hit" table via "hit.typeId" 
//      (see CGameRules::CreateScriptHitInfo()).
//
// Returns: True if the hit type is _any_ kind of melee; otherwise false.
//
int CScriptBind_GameRules::IsHitTypeIdMelee(IFunctionHandler *pH, int hitTypeId)
{
	CGameRules *pGameRules=GetGameRules(pH);

	const HitTypeInfo* pHitTypeInfo = pGameRules->GetHitTypeInfo(hitTypeId);
	IF_UNLIKELY (pHitTypeInfo == NULL)
	{
		return pH->EndFunction(false);
	}

	return pH->EndFunction( ((pHitTypeInfo->m_flags & CGameRules::EHitTypeFlag::IsMeleeAttack) != 0) );
}


//------------------------------------------------------------------------
int CScriptBind_GameRules::IsDemoMode(IFunctionHandler *pH)
{
	int demoMode = IsDemoPlayback();
	return pH->EndFunction(demoMode);
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::DebugCollisionDamage(IFunctionHandler *pH)
{
  return pH->EndFunction(g_pGameCVars->g_debugCollisionDamage);
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::SendDamageIndicator(IFunctionHandler* pH, ScriptHandle targetId, ScriptHandle shooterId, ScriptHandle weaponId, Vec3 dir, float damage, int projectileClassId, int hitTypeId)
{
	if(!gEnv->bServer)
		return pH->EndFunction();

	CGameRules *pGameRules = GetGameRules(pH);

	if(IGameRulesDamageHandlingModule* pDamageHandlingModule = pGameRules->GetDamageHandlingModule())
	{
		if(!pDamageHandlingModule->AllowHitIndicatorForType(hitTypeId))
		{
			return pH->EndFunction();
		}
	}

	EntityId tId = EntityId(targetId.n);
	EntityId sId= EntityId(shooterId.n);
	EntityId wId= EntityId(weaponId.n);

	if (!tId)
		return pH->EndFunction();

	IActor* pActor = pGameRules->GetActorByEntityId(tId);
	if (!pActor || !pActor->IsPlayer())
		return pH->EndFunction();

	pGameRules->SanityCheckHitData(dir, sId, tId, wId, hitTypeId, "CScriptBind_GameRules::SendDamageIndicator");
	pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClProcessHit(), CGameRules::ProcessHitParams(sId, wId, dir, damage, (uint16)projectileClassId, (uint8)hitTypeId), eRMI_ToClientChannel, tId, pGameRules->GetChannelId(tId));

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_GameRules::EnteredGame(IFunctionHandler* pH)
{
	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
		pGameRules->EnteredGame();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::Announce(IFunctionHandler *pH, ScriptHandle playerId, const char *announcement, int context)
{
	CAnnouncer::GetInstance()->Announce((EntityId)playerId.n, announcement, static_cast<CAnnouncer::EAnnounceContext>(context));
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_GameRules::Watch(IFunctionHandler *pH, const char* text)
{
	CryWatch(text);
	return pH->EndFunction();
}

int CScriptBind_GameRules::DemiGodDeath(IFunctionHandler *pH)
{
	CGameRules* pGameRules = g_pGame->GetGameRules();

	if (pGameRules)
	{
		CGodMode::GetInstance().DemiGodDeath();
	}

	return pH->EndFunction();
}

int CScriptBind_GameRules::GetPrimaryTeam(IFunctionHandler *pH)
{
	int  t = 0;

	if (CGameRules* pGameRules=g_pGame->GetGameRules())
	{
		if (IGameRulesRoundsModule* pRoundsModule=g_pGame->GetGameRules()->GetRoundsModule())
		{
			t = pRoundsModule->GetPrimaryTeam();
		}
	}

	return pH->EndFunction(t);
}

int CScriptBind_GameRules::AddForbiddenArea( IFunctionHandler *pH, ScriptHandle entityId )
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
	{
		pGameRules->AddForbiddenArea((EntityId)entityId.n);
	}
	
	return pH->EndFunction();
}

int CScriptBind_GameRules::RemoveForbiddenArea( IFunctionHandler *pH, ScriptHandle entityId )
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
	{
		pGameRules->RemoveForbiddenArea((EntityId)entityId.n);
	}
	
	return pH->EndFunction();
}

int CScriptBind_GameRules::MakeMovementVisibleToAI( IFunctionHandler *pH, const char* entityClass )
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
	{
		IGameRulesDamageHandlingModule* pDamageHandling = pGameRules->GetDamageHandlingModule();
		if (pDamageHandling)
		{
			IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(entityClass);
			if (pEntityClass)
			{
				pDamageHandling->MakeMovementVisibleToAIForEntityClass(pEntityClass);
			}
		}
	}

	return pH->EndFunction();
}

int CScriptBind_GameRules::SendGameRulesObjectiveEntitySignal(IFunctionHandler *pH, ScriptHandle entityId, int signal)
{
	CGameRules *pGameRules=GetGameRules(pH);

	if (pGameRules)
	{
		IGameRulesObjectivesModule *pObjective=pGameRules->GetObjectivesModule();
		if (pObjective)
		{
			pObjective->OnEntitySignal((EntityId)entityId.n, signal);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_GameRules::ReRecordEntity(IFunctionHandler *pH, ScriptHandle entityId)
{
	CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();

	if (pRecordingSystem)
	{
		pRecordingSystem->ReRecord((EntityId)entityId.n);
	}

	return pH->EndFunction();
}

// Query if we should give any feedback on a hit (local 2D sound feedback).
//
// In:      The amount of hit damage that was inflicted.
// 
// Returns: True if feedback should be given; otherwise false.
//
int CScriptBind_GameRules::ShouldGiveLocalPlayerHitFeedback2DSound(IFunctionHandler *pH, float damage)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IF_UNLIKELY (pGameRules == NULL)
	{
		return pH->EndFunction(false);
	}

	return pH->EndFunction(pGameRules->ShouldGiveLocalPlayerHitFeedback(CGameRules::eLocalPlayerHitFeedbackChannel_2DSound, damage));
}

int CScriptBind_GameRules::CanUsePowerStruggleNode( IFunctionHandler *pH, ScriptHandle userId, ScriptHandle entityId )
{
	//We can use the power struggle node if we are in a position to use one of the attached panels
	IEntity *pSpearEntity = gEnv->pEntitySystem->GetEntity((EntityId)entityId.n);

	int childCount = 0;
	if(pSpearEntity && (childCount = pSpearEntity->GetChildCount()))
	{
		CPlayer* pUserPlayer = static_cast<CPlayer*>(m_pGameFW->GetIActorSystem()->GetActor((EntityId)userId.n));
		if(pUserPlayer)
		{
			IEntity *pUserEntity = pUserPlayer->GetEntity();
			const Vec3 spearToUser = (pUserEntity->GetWorldPos() - pSpearEntity->GetWorldPos()).GetNormalized();

			for(int i = 0; i < childCount; ++i)
			{
				//Check we're inside its cone
				const IEntity* pChildEntity = pSpearEntity->GetChild(i);
				const Vec3 panelForwardDir = pChildEntity->GetForwardDir();

				const float theta = acos(panelForwardDir.dot(spearToUser));
				if(theta > SPEAR_PANEL_GRAB_CONE_SIZE)
				{
					continue;
				}

				const EntityId childId = pChildEntity->GetId();
				CEnvironmentalWeapon* pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(childId, "EnvironmentalWeapon"));
				if(pEnvWeapon && pEnvWeapon->CanBeUsedByPlayer(pUserPlayer))
				{
					return pH->EndFunction(childId);
				}
			}
		}
	}

	return pH->EndFunction(0);
}
