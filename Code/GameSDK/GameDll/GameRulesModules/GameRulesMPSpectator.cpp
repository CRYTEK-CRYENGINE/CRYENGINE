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

#include "StdAfx.h"

#include "GameRulesMPSpectator.h"
#include "Game.h"
#include "UI/UIManager.h"
#include "GameCVars.h"
#include "Actor.h"
#include "Player.h"
#include "GameRulesMPSpawning.h"
#include "IGameRulesPlayerStatsModule.h"

#include "Utility/CryWatch.h"
#include <IUIDraw.h>
#include "RecordingSystem.h"
#include "Network/Lobby/GameLobby.h"

// -----------------------------------------------------------------------

#define ASSERT_IS_CLIENT if (!gEnv->IsClient()) { assert(gEnv->IsClient()); return; }
#define ASSERT_IS_SERVER if (!gEnv->bServer) { assert(gEnv->bServer); return; }
#define ASSERT_IS_SERVER_RETURN_FALSE if (!gEnv->bServer) { assert(gEnv->bServer); return false; }

// -----------------------------------------------------------------------

static const float  kTagTime = 15.f;

// -----------------------------------------------------------------------
CGameRulesMPSpectator::CGameRulesMPSpectator()
{
	m_pGameRules = NULL;
	m_pGameFramework = NULL;
	m_pGameplayRecorder = NULL;
	m_pActorSys = NULL;

	m_eatsActorActions = 0;
	m_enableFree = 0;
	m_enableFollow = 0;
	m_enableFollowWhenNoLivesLeft = 0;
	m_enableKiller = 0;
	m_serverAlwaysAllowsSpectatorModeChange = 0;

	m_timeFromDeathTillAutoSpectate = 0.f;

	m_ClientInfo.Reset();
}

// -----------------------------------------------------------------------
CGameRulesMPSpectator::~CGameRulesMPSpectator()
{
}

// -----------------------------------------------------------------------
void CGameRulesMPSpectator::Init(XmlNodeRef xml)
{
	CryLog("CGameRulesMPSpectator::Init()");

	m_pGameRules = g_pGame->GetGameRules();
	assert(m_pGameRules);
	m_pGameFramework = g_pGame->GetIGameFramework();
	assert(m_pGameFramework);
	m_pGameplayRecorder = m_pGameFramework->GetIGameplayRecorder();
	assert(m_pGameplayRecorder);
	m_pActorSys = gEnv->pGameFramework->GetIActorSystem();
	assert(m_pActorSys);

	bool  boo;
	float  f;

	if (xml->getAttr("eatsActorActions", boo))
	{
		m_eatsActorActions = boo;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpectator failed to find valid eatsActorActions param");
	}

	if (xml->getAttr("enableFree", boo))
	{
		m_enableFree = boo;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpectator failed to find valid enableFree param");
	}

	if (xml->getAttr("enableFollow", boo))
	{
		m_enableFollow = boo;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpectator failed to find valid enableFollow param");
	}

	if (xml->getAttr("enableKiller", boo))
	{
		m_enableKiller = boo;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpectator failed to find valid enableFollow param");
	}

	if (xml->getAttr("enableFollowWhenNoLivesLeft", boo))
	{
		m_enableFollowWhenNoLivesLeft	= boo;
	}

	if (xml->getAttr("serverAlwaysAllowsSpectatorModeChange", boo))
	{
		m_serverAlwaysAllowsSpectatorModeChange = boo;
	}

	if (xml->getAttr("timeFromDeathTillAutoSpectate", f))
	{
		m_timeFromDeathTillAutoSpectate = f;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpectator failed to find valid timeFromDeathTillAutoSpectate param");
	}

	CryLog("Init() set params to eatsActorActions=%d; enableFree=%d; enableFollow=%d; enableKiller=%d, timeFromDeathTillAutoSpectate=%.1f", m_eatsActorActions, m_enableFree, m_enableFollow, m_enableKiller, m_timeFromDeathTillAutoSpectate);
}

// -----------------------------------------------------------------------
void CGameRulesMPSpectator::PostInit()
{
	CryLog("CGameRulesMPSpectator::PostInit()");
}

// -----------------------------------------------------------------------
void CGameRulesMPSpectator::Update(float frameTime)
{
	CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	if(pActor && pActor->GetSpectatorState() == CActor::eASS_SpectatorMode)
	{
		CActor::EActorSpectatorMode spectatorMode = (CActor::EActorSpectatorMode)pActor->GetSpectatorMode();
		if(spectatorMode != CActor::eASM_Follow)
		{
			ChangeSpectatorModeBestAvailable(pActor, true);
		}

		if(pActor->GetSpectatorMode()==CActor::eASM_Follow)
		{
			EntityId target = static_cast<CPlayer*>(pActor)->GetSpectatorTarget();
			CActor* pTargetActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(target));
			if(!pTargetActor || pTargetActor->GetSpectatorState() == CActor::eASS_SpectatorMode) //Our current target has left the game (literally, or to spectator only mode)
			{
				ChangeSpectatorModeBestAvailable(pActor, true);
			}
		}
	}

	if(m_ClientInfo.m_fRequestTimer > 0.f)
	{
		m_ClientInfo.m_fRequestTimer -= gEnv->pTimer->GetFrameTime();
	}
}

// -----------------------------------------------------------------------
bool CGameRulesMPSpectator::ModeIsEnabledForPlayer(const int mode, const EntityId playerId) const
{
	bool  is = false;
	switch (mode)
	{
		case CActor::eASM_Fixed:	is = true; break;
		case CActor::eASM_Free:		is = (m_enableFree   && !g_pGame->GetCVars()->g_spectate_DisableFree);   break;
		case CActor::eASM_Follow:	
		{
			if (m_enableFollow)
			{
				is = !g_pGame->GetCVars()->g_spectate_DisableFollow && !g_pGameCVars->g_modevarivar_disableSpectatorCam;
			}
			else if (m_enableFollowWhenNoLivesLeft)
			{
				bool hasNotSpawned=false;

				if (IGameRulesPlayerStatsModule* pPlayStatsMod=m_pGameRules->GetPlayerStatsModule())
				{
					if (const SGameRulesPlayerStat* s=pPlayStatsMod->GetPlayerStats(playerId))
					{
						if (!(s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND))
						{
							hasNotSpawned=true;
						}
					}
				}

				IGameRulesSpawningModule*  pSpawnMo = m_pGameRules->GetSpawningModule();
				if (pSpawnMo->GetNumLives() > 0 && (pSpawnMo->GetRemainingLives(playerId) == 0 || hasNotSpawned))
				{
					is = !g_pGame->GetCVars()->g_spectate_DisableFollow && !g_pGameCVars->g_modevarivar_disableSpectatorCam;
				}
			}
			else if (CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId)))
			{
				is = pActor->GetSpectatorState() == CActor::eASS_SpectatorMode;
			}
			break;
		}
		case CActor::eASM_Killer:
			is = ( m_enableKiller && !g_pGame->GetCVars()->g_killercam_disable );
			break;
		default: assert(0);
	}
	return is;
}

// -----------------------------------------------------------------------
bool CGameRulesMPSpectator::ModeIsAvailable(const EntityId playerId, const int mode, EntityId* outOthEnt)
{
	bool  is = false;
	EntityId  othEnt = 0;

	if (ModeIsEnabledForPlayer(mode, playerId))
	{
		switch (mode)
		{
			case CActor::eASM_Fixed:
				othEnt = GetInterestingSpectatorLocation();
				if (othEnt)
				{
					is = true;
				}
				break;
			case CActor::eASM_Free:
				is = true;
				break;
			case CActor::eASM_Follow:
				if(!g_pGameCVars->g_modevarivar_disableSpectatorCam)
				{
					othEnt = GetNextSpectatorTarget(playerId, 1);
					if (othEnt)
						is = true;
				}
				break;
			case CActor::eASM_Killer:
				is = false;
				break;
			default:
				assert(0);
		}
	}

	if (is && outOthEnt)
		(*outOthEnt) = othEnt;
	return is;
}

// -----------------------------------------------------------------------
// TODO it'd be cool if the priority order of the modes could be specified in the xml somehow?
int CGameRulesMPSpectator::GetBestAvailableMode(const EntityId playerId, EntityId* outOthEnt)
{
	int teamId = m_pGameRules->GetTeam(playerId);
	if (teamId)
	{
		-- teamId;		// index should be 0 or 1 but teams are 1 and 2
	}

	int mode = CActor::eASM_None;
	assert(teamId>= 0 && teamId<2);
	if (ModeIsAvailable(playerId, CActor::eASM_Follow, outOthEnt))
	{
		mode = CActor::eASM_Follow;
	}
	else if (ModeIsAvailable(playerId, CActor::eASM_Fixed, outOthEnt))
	{
		mode = CActor::eASM_Fixed;
	}
	else
	{
		for (int newMode = CActor::eASM_FirstMPMode; newMode < CActor::eASM_LastMPMode; ++newMode)
		{
			if (ModeIsAvailable(playerId, newMode, outOthEnt))
			{
				mode = newMode;
				break;
			}
		}

		if (mode == CActor::eASM_None)
		{
			string warningMessage;
			warningMessage.Format("No suitable spectator mode found! Valid spectator locations = %i. Please check your level setup!", GetSpectatorLocationCount());
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, warningMessage.c_str());
		}
	}

	return mode;
}

// -----------------------------------------------------------------------
int CGameRulesMPSpectator::GetNextMode(EntityId playerId, int inc, EntityId* outOthEid)
{
	CRY_ASSERT(inc==-1 || inc==1);
	//inc = ((inc < 0) ? -1 : 1);
	int  mode = (int) CActor::eASM_None;
	EntityId  othEntId = 0;
	if (IActor* pActor=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId))
	{
		CActor*  pActorImpl = static_cast< CActor* >( pActor );
		const CActor::EActorSpectatorMode  curMode = (CActor::EActorSpectatorMode) pActorImpl->GetSpectatorMode();
		mode = (int) curMode;
		bool  modeok;
		do
		{
			if (inc >= 0)
			{
				mode++;
				if (mode > CActor::eASM_LastMPMode)
				{
					mode = CActor::eASM_FirstMPMode;
				}
			}
			else
			{
				mode--;
				if(mode < CActor::eASM_FirstMPMode)
				{
					mode = CActor::eASM_LastMPMode;
				}
			}
			othEntId = 0;
			modeok = ModeIsAvailable(playerId, mode, &othEntId);
		}
		while (!modeok && (mode != curMode));
	}
	if (outOthEid)
		(*outOthEid) = othEntId;
	return mode;
}

// -----------------------------------------------------------------------
void CGameRulesMPSpectator::SvOnChangeSpectatorMode( EntityId playerId, int mode, EntityId targetId, bool resetAll )
{
	CryLog("CGameRulesMPSpectator::SvOnChangeSpectatorMode");
	ASSERT_IS_SERVER;

	CActor* pActor = static_cast<CActor*>(m_pActorSys->GetActor(playerId));
	if (!pActor)
		return;

	if (mode>0)
	{
		if(resetAll)
		{
			//TODO: Destroy inventory
		}
		
		if (mode == CActor::eASM_Free)
		{
			Vec3 pos(0.f, 0.f, 0.f);
			Ang3 angles(0.f, 0.f, 0.f);

			pActor->SetSpectatorModeAndOtherEntId(CActor::eASM_Free, 0);

			EntityId locationId = GetInterestingSpectatorLocation();
			if (locationId)
			{
				IEntity *location = gEnv->pEntitySystem->GetEntity(locationId);
				if (location)
				{
					pos = location->GetWorldPos();
					angles = location->GetWorldAngles();
					
					m_pGameRules->MovePlayer(pActor, pos, Quat(angles));
				}
			}
		}
		else if (mode == CActor::eASM_Fixed)
		{
			if (targetId)
			{
				pActor->SetSpectatorModeAndOtherEntId(CActor::eASM_Fixed, targetId);
			}
			else
			{
				EntityId newLoc = GetInterestingSpectatorLocation();
				if(newLoc)
				{
					pActor->SetSpectatorModeAndOtherEntId(CActor::eASM_Fixed, newLoc);
				}
			}
		}
		else if (mode == CActor::eASM_Follow)
		{
			if (targetId)
			{
				pActor->SetSpectatorModeAndOtherEntId(CActor::eASM_Follow, targetId);
			}
			else
			{
				EntityId newTargetId = GetNextSpectatorTarget(playerId, 1);
				if(newTargetId)
				{
					pActor->SetSpectatorModeAndOtherEntId(CActor::eASM_Follow, newTargetId);
				}
			}
		}
		else if (mode == CActor::eASM_Killer)
		{
			if (targetId)
			{
				pActor->SetSpectatorModeAndOtherEntId(CActor::eASM_Killer, targetId);
			}
			else
			{
				// If there is no killer. then revert back to regular follow.
				EntityId newTargetId = GetNextSpectatorTarget(playerId, 1);
				if(newTargetId)
				{
					pActor->SetSpectatorModeAndOtherEntId(CActor::eASM_Follow, newTargetId);
				}
			}
		}
	}
	else
	{
	//	if (self:CanRevive(playerId)) then	
		pActor->SetSpectatorModeAndOtherEntId(CActor::eASM_None, 0);
	//	end
	}
	
	TChannelSpectatorModeMap::iterator it = m_channelSpectatorModes.find(pActor->GetChannelId());
	if (it != m_channelSpectatorModes.end())
	{
		it->second = mode;
	}
	else
	{
		m_channelSpectatorModes.insert(TChannelSpectatorModeMap::value_type(pActor->GetChannelId(), mode));
	}
}

// -----------------------------------------------------------------------
void CGameRulesMPSpectator::SvRequestSpectatorTarget( EntityId playerId, int change )
{
	CryLog("CGameRulesMPSpectator::SvRequestSpectatorTarget");
	ASSERT_IS_SERVER;

}

// -----------------------------------------------------------------------
void CGameRulesMPSpectator::ClOnChangeSpectatorMode( EntityId playerId, int mode, EntityId targetId, bool resetAll )
{
	CryLog("CGameRulesMPSpectator::ClOnChangeSpectatorMode");
	ASSERT_IS_CLIENT;

}

// -----------------------------------------------------------------------
EntityId CGameRulesMPSpectator::GetNextSpectatorTarget( EntityId playerId, int change )
{
	if(change >= 1) change = 1;
	if(change <= 0) change = -1;

	CActor* pActor = static_cast<CActor*>(m_pActorSys->GetActor(playerId));
	if(pActor)
	{
		CGameRules::TPlayers  players;
		players.reserve(m_pGameRules->GetNumChannels() + 1);

		int  teamCount = m_pGameRules->GetTeamCount();
		int  friendlyTeam = m_pGameRules->GetTeam(playerId);

		int teamIndex = max(friendlyTeam - 1, 0);
		if ((teamCount == 0) || (friendlyTeam == 0))
		{
			IGameRulesPlayerStatsModule*  pPlayStatsMod = m_pGameRules->GetPlayerStatsModule();
			assert(pPlayStatsMod);
			int  numStats = pPlayStatsMod->GetNumPlayerStats();
			for (int i=0; i<numStats; i++)
			{
				const SGameRulesPlayerStat*  s = pPlayStatsMod->GetNthPlayerStats(i);
				CPlayer*  loopPlr = static_cast< CPlayer* >( m_pActorSys->GetActor(s->playerId) );
				if (!loopPlr)
				{
					CryLog(" > skipping entity because its actor is NULL");
					continue;
				}
				IEntity  *loopEnt = loopPlr->GetEntity();
				assert(loopEnt);
				players.push_back(loopEnt->GetId());
			}
		}
		else
		{
			for (int i=1; i<=teamCount; i++)
			{
				if (g_pGame->GetCVars()->g_spectate_TeamOnly && (i != friendlyTeam))
				{
					continue;
				}
				CGameRules::TPlayers  teamPlayers;
				m_pGameRules->GetTeamPlayers(i, teamPlayers);
				CGameRules::TPlayers::const_iterator  it = teamPlayers.begin();
				CGameRules::TPlayers::const_iterator  end = teamPlayers.end();
				for (; it!=end; ++it)
				{
					CPlayer  *loopPlr = static_cast<CPlayer*>(m_pActorSys->GetActor(*it));
					if (!loopPlr)
					{
						CryLog(" > skipping entity because its actor is NULL");
						continue;
					}
					IEntity  *loopEnt = loopPlr->GetEntity();
					assert(loopEnt);
					players.push_back(loopEnt->GetId());
				}
			}
		}
		
		int  numPlayers = players.size();

		// work out which one we are currently watching
		EntityId  specTarg = ((pActor->GetSpectatorMode() == CActor::eASM_Follow) ? pActor->GetSpectatorTarget() : 0);
		int index = (specTarg ? 0 : numPlayers);  // (only do the for-loop if we've actually got something to find)
		for(; index < numPlayers; ++index)
		{
			if(players[index] == specTarg)
			{
				break;
			}
		}

		// loop through the players to find a valid one.
		bool found = false;
		if(numPlayers > 0)
		{
			int newTargetIndex = index;
			int numAttempts = numPlayers;
			do
			{
				newTargetIndex += change;
				--numAttempts;

				// wrap around
				if(newTargetIndex < 0)
					newTargetIndex = numPlayers-1;
				if(newTargetIndex >= numPlayers)
					newTargetIndex = 0;

				const EntityId targetId = players[newTargetIndex];

				// skip ourself
				if (targetId == playerId)
					continue;

				// skip dead players and spectating players
				CActor* pTarget = static_cast<CActor*>(m_pActorSys->GetActor(targetId));
				if (!pTarget || pTarget->IsDead() || (pTarget->GetSpectatorMode() != CActor::eASM_None))
				{
					continue;
				}

				// otherwise this one will do.
				found = true;
			} while(!found && numAttempts > 0);

			if(found)
			{
				return players[newTargetIndex];
			}
		}
	}
	return 0;
}

//------------------------------------------------------------------------
// [tlh] Gets localisable string label that represents name of the spectator mode 'mode'
const char* CGameRulesMPSpectator::GetActorSpectatorModeName(uint8 mode)
{
	static const char*  modenames[] = {
		"@ui_hud_spec_mode_fixed",
		"@ui_hud_spec_mode_free",
		"@ui_hud_spec_mode_follow"};

	const char*  name = NULL;

	if (mode>=CActor::eASM_FirstMPMode && mode <=CActor::eASM_LastMPMode)
	{
		name = modenames[mode-CActor::eASM_FirstMPMode];
	}

	return name;
}

//------------------------------------------------------------------------
// TODO this might eventually need to consider current HUD state (map, scoreboard, etc.) etc.
bool CGameRulesMPSpectator::CanChangeSpectatorMode(EntityId playerId) const
{
	bool  canChange = true;

	if (IActor* pActor=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId))
	{
		CActor*  pActorImpl = static_cast< CActor* >( pActor );

		CActor::EActorSpectatorState spectatorState = pActorImpl->GetSpectatorState();

		canChange = ((spectatorState != CActor::eASS_None) && (spectatorState != CActor::eASS_ForcedEquipmentChange));

		if (!canChange && !gEnv->bServer)
		{
			CRY_ASSERT(playerId == g_pGame->GetIGameFramework()->GetClientActorId());

			CGameRulesMPSpawningBase*  pSpawningMo = (CGameRulesMPSpawningBase*) g_pGame->GetGameRules()->GetSpawningModule();

			if (pSpawningMo != NULL && (pSpawningMo->CanPlayerSpawnThisRound(playerId) == false))  // in games where mid-round joining isn't allowed, players need something to do whilst they wait for the round to end
			{
				canChange = g_pGame->GetUI()->IsPreGameDone();
			}
		}
	}

	return canChange;
}

//------------------------------------------------------------------------
// resetAll param seems to currently be unused
void CGameRulesMPSpectator::ChangeSpectatorMode(const IActor *pActor, uint8 mode, EntityId targetId, bool resetAll, bool force)
{
	if (!pActor)
		return;
		
	CryLog("[tlh] @ CGameRulesMPSpectator::ChangeSpectatorMode(), actor='%s', mode=%u, target=%u, resetAll=%s, force=%s, server=%s", pActor->GetEntity()->GetName(), mode, targetId, resetAll ? "true" : "false", force ? "true" : "false", gEnv->bServer ? "true" : "false");

	if (pActor->GetSpectatorMode()==mode && !(mode==CActor::eASM_Follow||mode==CActor::eASM_Killer))  // [tlh] the Follow and Killer mode must be allowed thru here because they uses the ChangeSpectatorMode mechanism to also change the target it is following
		return;

	if (gEnv->IsEditor())		// Editor doesn't use spectator mode
	{
		return;
	}

	EntityId actorId = pActor->GetEntityId();
	if (gEnv->bServer)
	{
		SvOnChangeSpectatorMode(actorId, mode, targetId, resetAll);
		m_pGameplayRecorder->Event(pActor->GetEntity(), GameplayEvent(eGE_Spectator, 0, (float)mode));
	}
	else if (pActor->IsClient())
	{
		if(m_ClientInfo.m_mode != mode || m_ClientInfo.m_targetId != targetId || m_ClientInfo.m_fRequestTimer < 0.0f)
		{
			float fNetLag = 0.f;

			if(ICryLobby * pLobby = gEnv->pNetwork->GetLobby())
			{
				CryPing ping = 0;
				if(CGameLobby * pGameLobby = g_pGame->GetGameLobby())
				{
					SCryMatchMakingConnectionUID mmCxUID = pGameLobby->GetConnectionUIDFromChannelID(pActor->GetChannelId());
					pLobby->GetMatchMaking()->GetSessionPlayerPing( mmCxUID, &ping);
				}
				
				fNetLag = ((float)ping) * 0.001f;
			}

			m_ClientInfo.m_fRequestTimer	= (fNetLag * 1.5f) + 0.1f;	//Assume the possibility of a ping spike and 2 frames processing time on the server
			m_ClientInfo.m_mode						= mode;
			m_ClientInfo.m_targetId				= targetId;

			ClOnChangeSpectatorMode(actorId, mode, targetId, resetAll);
			m_pGameRules->SendRMI_SvRequestSpectatorMode(actorId, mode, targetId, resetAll, eRMI_ToServer, force);

			if (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating)
			{
				static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId))->SetSpectatorModeAndOtherEntId(mode, targetId);
			}
		}
	}
}

// -----------------------------------------------------------------------
// The initial entry point for spectator mode
void CGameRulesMPSpectator::ChangeSpectatorModeBestAvailable(const IActor *pActor, bool resetAll)
{
	EntityId  othEnt;	
	int  mode = GetBestAvailableMode(pActor->GetEntityId(), &othEnt);

	if(mode != m_ClientInfo.m_mode || m_ClientInfo.m_fRequestTimer <= 0.f || gEnv->pEntitySystem->GetEntity(m_ClientInfo.m_targetId) == NULL)
		ChangeSpectatorMode(pActor, mode, othEnt, resetAll);
}

//------------------------------------------------------------------------
void CGameRulesMPSpectator::AddSpectatorLocation(EntityId location)
{
	IEntity* ent = gEnv->pEntitySystem->GetEntity(location);
	assert (ent);

	ent->SetFlags(ent->GetFlags() | ENTITY_FLAG_NO_PROXIMITY);

	stl::push_back_unique(m_spectatorLocations, location);
}

//------------------------------------------------------------------------
void CGameRulesMPSpectator::RemoveSpectatorLocation(EntityId id)
{
	stl::find_and_erase(m_spectatorLocations, id);
}

//------------------------------------------------------------------------
int CGameRulesMPSpectator::GetSpectatorLocationCount() const
{
	return (int)m_spectatorLocations.size();
}

//------------------------------------------------------------------------
EntityId CGameRulesMPSpectator::GetSpectatorLocation(int idx) const
{
	if (idx >= 0 && idx < (int)m_spectatorLocations.size())
		return m_spectatorLocations[idx];
	return 0;
}

//------------------------------------------------------------------------
void CGameRulesMPSpectator::GetSpectatorLocations(TSpawnLocations &locations) const
{
	locations.resize(0);
	locations = m_spectatorLocations;
}

//------------------------------------------------------------------------
EntityId CGameRulesMPSpectator::GetRandomSpectatorLocation() const
{
	EntityId  e = 0;
	if (int count=GetSpectatorLocationCount())
		e = GetSpectatorLocation(cry_random(0, count - 1));
	return e;
}

//------------------------------------------------------------------------
EntityId CGameRulesMPSpectator::GetInterestingSpectatorLocation() const
{
	return GetRandomSpectatorLocation();
}

//------------------------------------------------------------------------
EntityId CGameRulesMPSpectator::GetNextSpectatorLocation(const CActor* pPlayer) const
{
	CryLog("[tlh] @ CGameRulesMPSpectator::GetNextSpectatorLocation()");
	const int  sz = m_spectatorLocations.size();
	if (sz > 0)
	{
		EntityId  cur = pPlayer->GetSpectatorFixedLocation();
		CryLog("[tlh] > has cur loc entity %d...", cur);
		int  idx = (cur ? 0 : sz);  // no need to search for cur if haven't even got one
		for (; idx<sz; idx++)
		{
			if (m_spectatorLocations[idx] == cur)
			{
				break;
			}
		}
		if (idx < sz)  // ie. found, so increment to next
		{
			CryLog("[tlh] > cur %d found at idx %d...", cur, idx);
			idx++;
		}
		if (idx == sz)
		{
			EntityId  ret = m_spectatorLocations[0];
			CryLog("[tlh] > cur at end, returning first entry, %d", ret);
			return ret;
		}
		else
		{
			EntityId  ret = m_spectatorLocations[idx];
			CryLog("[tlh] > cur NOT at end, returning next entry, %d", ret);
			return ret;
		}
	}
	CryLog("[tlh] > no spectator locs in level or gamemode, returning 0");
	return 0;
}

//------------------------------------------------------------------------
EntityId CGameRulesMPSpectator::GetPrevSpectatorLocation(const CActor* pPlayer) const
{
	CryLog("[tlh] @ CGameRulesMPSpectator::GetPrevSpectatorLocation()");
	const int  sz = m_spectatorLocations.size();
	if (sz > 0)
	{
		EntityId  cur = pPlayer->GetSpectatorFixedLocation();
		CryLog("[tlh] > has cur loc entity %d...", cur);
		int  idx = (cur ? 0 : sz);  // no need to search for cur if haven't even got one
		for (; idx<sz; idx++)
		{
			if (m_spectatorLocations[idx] == cur)
			{
				break;
			}
		}
		idx--;  // (can do this regardless of whether cur was found or not, because if it wasn't then idx will be at one past the end, so decrementing it will put it at the end)
		if (idx < 0)
		{
			EntityId  ret = m_spectatorLocations[sz - 1];
			CryLog("[tlh] > cur at beginning, returning last entry, %d", ret);
			return ret;
		}
		else
		{
			EntityId  ret = m_spectatorLocations[idx];
			CryLog("[tlh] > cur NOT at beginning, returning previous entry, %d", ret);
			return ret;
		}
	}
	CryLog("[tlh] > no spectator locs in level or gamemode, returning 0");
	return 0;
}

// -----------------------------------------------------------------------
bool CGameRulesMPSpectator::GetModeFromChannelSpectatorMap(int channelId, int* outMode) const
{
	bool  found = false;
	TChannelSpectatorModeMap::const_iterator  it = m_channelSpectatorModes.find(channelId);
	if (it != m_channelSpectatorModes.end())
	{
		if (outMode)
			(*outMode) = it->second;
		found = true;
	}
	return true;
}

// -----------------------------------------------------------------------
void CGameRulesMPSpectator::FindAndRemoveFromChannelSpectatorMap(int channelId)
{
	TChannelSpectatorModeMap::iterator  it = m_channelSpectatorModes.find(channelId);
	if (it != m_channelSpectatorModes.end())
	{
		m_channelSpectatorModes.erase(it);
	}
}

void CGameRulesMPSpectator::GetPostDeathDisplayDurations( EntityId playerId, int teamWhenKilled, bool isSuicide, bool isBulletTimeKill, float& rDeathCam, float& rKillerCam, float& rKillCam ) const
{
	const int team = teamWhenKilled;
	const IGameRulesSpawningModule* pSpawningModule = m_pGameRules->GetSpawningModule();
	CRY_ASSERT(pSpawningModule);

	float reviveTime = pSpawningModule->GetTimeFromDeathTillAutoReviveForTeam(team);
	if( IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId) )
	{
		reviveTime += pSpawningModule->GetPlayerAutoReviveAdditionalTime(pActor);
	}

	if( isSuicide )
	{
		// Just show DeathCam.
		rDeathCam = (float)__fsel( reviveTime - g_pGameCVars->g_tpdeathcam_timeOutSuicide, g_pGameCVars->g_tpdeathcam_timeOutSuicide, reviveTime );
		rKillerCam = 0.f;
		rKillCam = 0.f;
	}
	else
	{
		// KillCam timings.
		const float killCamLength = isBulletTimeKill ? g_pGameCVars->kc_skillKillLength : g_pGameCVars->kc_length;
		const float killCamMinimumNeeded = killCamLength+g_pGameCVars->kc_kickInTime;

		// Work out scales for disabled modes.
		const float killCamScale = CRecordingSystem::KillCamEnabled() && reviveTime>=killCamMinimumNeeded ? 1.f : 0.f;
		const float deathCamScale = g_pGameCVars->g_deathCam ? 1.f : 0.f;
		const float killerCamScale = ModeIsEnabledForPlayer(CActor::eASM_Killer,playerId) ? 1.f : 0.f;

		// Actual duration of KillCam
		const float killCamTime = killCamLength*killCamScale;
		rKillCam = killCamTime;

		// KillCam time which we can use for other cams.
		const float killCamViableUseTime = (g_pGameCVars->kc_kickInTime)*killCamScale;

		// This should be as big as the kick in time as there's no way the kill cam can start til after that, and otherwise you will get a blank section.
		const float remainingTime = (float)__fsel((reviveTime-killCamTime)-killCamViableUseTime, reviveTime-killCamTime, killCamViableUseTime );

		// Minimum Time in which we will squish the KillerCam and DeathCam. Anything less will just use DeathCam.
		const float shortestTimeForBoth = g_pGameCVars->g_postkill_minTimeForDeathCamAndKillerCam;

		// Preferred Times, taking into account disabled modes.
		const float idealDeathCamTime = (g_pGameCVars->g_tpdeathcam_timeOutKilled*deathCamScale);
		const float idealKillerCamTime = (g_pGameCVars->g_killercam_displayDuration*killerCamScale);

		// Ideal total time to display enabled cameras.
		const float idealTotalTime = (idealDeathCamTime+idealKillerCamTime);

		if( remainingTime >= idealTotalTime )
		{
			// Plenty of time available. Use preferred times.
			rDeathCam = idealDeathCamTime;
			rKillerCam = idealKillerCamTime;
		}
		else if( remainingTime >= shortestTimeForBoth && idealTotalTime>FLT_EPSILON )
		{
			// Split the available time between the two.
			const float deathSplitAmount = (deathCamScale*g_pGameCVars->g_postkill_splitScaleDeathCam);
			const float killerSplitAmount = (killerCamScale*(1.f-g_pGameCVars->g_postkill_splitScaleDeathCam));
			const float invTotalSplitAmount = __fres(deathSplitAmount+killerSplitAmount);

			rDeathCam = remainingTime*deathSplitAmount*invTotalSplitAmount;
			rKillerCam = remainingTime*killerSplitAmount*invTotalSplitAmount;
		}
		else
		{
			// Small amount of time. Use it all for DeathCam; drop KillerCam.
			rDeathCam = remainingTime*deathCamScale;
			rKillerCam = 0.f;
		}
	}
}

