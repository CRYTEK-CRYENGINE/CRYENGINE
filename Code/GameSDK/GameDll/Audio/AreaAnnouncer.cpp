// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

	Plays Announcements based upon AreaBox triggers placed in levels

History:
- 25:02:2010		Created by Ben Parbury
*************************************************************************/

#include "StdAfx.h"
#include "Audio/AreaAnnouncer.h"
#include "Audio/MiscAnnouncer.h"

#include "Actor.h"
#include "ActorManager.h"
#include "GameRules.h"
#include "Utility/CryWatch.h"
#include <CryString/StringUtils.h>
#include "Utility/DesignerWarning.h"
#include "Player.h"
#include "Announcer.h"


#define ANNOUNCEMENT_NOT_PLAYING_TIME -1.0f

int static aa_debug = 0;
int static aa_peopleNeeded = 2;

int static aa_enabled = 1;

//---------------------------------------
CAreaAnnouncer::CAreaAnnouncer()
{
	REGISTER_CVAR(aa_peopleNeeded, aa_peopleNeeded, VF_NULL, "Number of people needed to play area announcement");
	REGISTER_CVAR(aa_enabled, aa_enabled, VF_NULL, "Stops area announcements being played or updated");
	
#if !defined(_RELEASE)
	REGISTER_CVAR(aa_debug, aa_debug, VF_NULL, "Enable/Disables Area announcer debug messages");
	REGISTER_COMMAND("aa_play", CmdPlay, VF_CHEAT, "play area announcement");
	REGISTER_COMMAND("aa_reload", CmdReload, VF_CHEAT, "Init area announcement");
#endif
}

//---------------------------------------
CAreaAnnouncer::~CAreaAnnouncer()
{
	if(gEnv->pConsole)
	{
		gEnv->pConsole->UnregisterVariable("aa_peopleNeeded");
		gEnv->pConsole->UnregisterVariable("aa_enabled");
#if !defined(_RELEASE)
		gEnv->pConsole->UnregisterVariable("aa_debug");
		gEnv->pConsole->RemoveCommand("aa_play");
		gEnv->pConsole->RemoveCommand("aa_reload");
#endif
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
	{
		pGameRules->UnRegisterRevivedListener(this);
	}
}

//---------------------------------------
void CAreaAnnouncer::Init()
{
	Reset();

	//Scan for areas
	IEntityClass* pTargetClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AreaBox");
	CRY_ASSERT_MESSAGE(pTargetClass, "Unable to find Target class AreaBox");

	if(pTargetClass)
	{
		IEntityItPtr it = gEnv->pEntitySystem->GetEntityIterator();
		while ( !it->IsEnd() )
		{
			IEntity* pEntity = it->Next();
			if(pEntity->GetClass() == pTargetClass)
			{
				//check entityName
				IEntityAreaComponent *pArea = (IEntityAreaComponent*)pEntity->GetProxy(ENTITY_PROXY_AREA);
				if (pArea)
				{
					LoadAnnouncementArea(pEntity, pEntity->GetName());
				}
			}
		}
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	pGameRules->RegisterRevivedListener(this);
}
//---------------------------------------
void CAreaAnnouncer::LoadAnnouncementArea(const IEntity* pEntity, const char* areaName)
{
	TAudioSignalID signal[AREA_ANNOUNCERS];

	stack_string signalName;
	for(int i = 0; i < AREA_ANNOUNCERS; i++)
	{
		signalName.Format("%s_%d", areaName, i + 1);
		signal[i] = g_pGame->GetGameAudio()->GetSignalID(signalName.c_str(), false);
	}

#if !defined(_RELEASE)
	if(aa_debug)
	{
		CryLogAlways("[AA] Found area '%s' with Signals %d, %d", areaName, signal[0], signal[1]);
	}
#endif

	if(signal[0] != INVALID_AUDIOSIGNAL_ID && signal[1] != INVALID_AUDIOSIGNAL_ID)
	{
		SAnnouncementArea area;
		area.m_areaProxyId = pEntity->GetId();
		static_assert(sizeof(area.m_signal) == sizeof(signal), "Invalid type size!");
		memcpy(area.m_signal, signal, sizeof(area.m_signal));

#if !defined(_RELEASE)
		cry_strcpy(area.m_name, areaName);
#endif

		DesignerWarning(m_areaList.size() < k_maxAnnouncementAreas, "Too many AreaAnnouncer area boxes loaded");
		if(m_areaList.size() < k_maxAnnouncementAreas)
		{
			m_areaList.push_back(area);
		}
	}

#if !defined(_RELEASE)
	//Found one signal but not both
	if(signal[0] != signal[1] && (signal[0] == INVALID_AUDIOSIGNAL_ID || signal[1] == INVALID_AUDIOSIGNAL_ID))
	{
#if defined(USER_benp)
		CRY_ASSERT_MESSAGE(0, ("'%s' only has signal for 1 team!", areaName));
#endif
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "'%s' only has signal for 1 team!", areaName);
	}
#endif

}

//---------------------------------------
void CAreaAnnouncer::Reset()
{
	m_areaList.clear();
}

//---------------------------------------
void CAreaAnnouncer::Update(const float dt)
{
	if(!aa_enabled)
		return;

#if !defined(_RELEASE)
	if(aa_debug)
	{
		const EntityId clientId = gEnv->pGameFramework->GetClientActorId();
		TAudioSignalID signal = BuildAnnouncement(clientId);
		CryWatch("Signal %d", signal);
	}
#endif

}

bool CAreaAnnouncer::AnnouncerRequired()
{
	bool required=true;
	EGameMode gameMode = g_pGame->GetGameRules()->GetGameMode();

	if (gEnv->IsDedicated())
	{
		required=false;
	}

	switch(gameMode)
	{
		case eGM_Gladiator:
		case eGM_Assault:
			required=false;
			break;
	}

	return required;
}

//---------------------------------------
void CAreaAnnouncer::EntityRevived(EntityId entityId)
{
	if(!aa_enabled)
		return;
	
	if (!AnnouncerRequired())
			return;

	const EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	if(entityId == clientId)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		CMiscAnnouncer *pMiscAnnouncer = pGameRules->GetMiscAnnouncer();
	
		TAudioSignalID signal = BuildAnnouncement(clientId);
		if(signal != INVALID_AUDIOSIGNAL_ID)
		{
			CAudioSignalPlayer::JustPlay(signal);
		}
	}
}

//---------------------------------------
TAudioSignalID CAreaAnnouncer::BuildAnnouncement(const EntityId clientId)
{
	const int k_areaCount = m_areaList.size();
	if (k_areaCount > 0)
	{
		IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();

		if (CActor* pClientActor = static_cast<CActor*>(pActorSystem->GetActor(clientId)))
		{
			int actorCount[k_maxAnnouncementAreas];
			memset(&actorCount, 0, sizeof(actorCount));

			CActorManager * pActorManager = CActorManager::GetActorManager();

			pActorManager->PrepareForIteration();

			const int kNumActors		= pActorManager->GetNumActors();
			const int kPlayerTeamId = g_pGame->GetGameRules()->GetTeam(clientId);

			for (int i = 0; i < kNumActors; i++)
			{
				SActorData actorData;
				pActorManager->GetNthActorData(i, actorData);

				if(actorData.teamId != kPlayerTeamId && actorData.health > 0 && actorData.spectatorMode == CActor::eASM_None)
				{
					for (int areaIndex = 0; areaIndex < k_areaCount; areaIndex++)
					{
						IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_areaList[areaIndex].m_areaProxyId);
						if(pEntity)
						{
							IEntityAreaComponent *pArea = (IEntityAreaComponent*)pEntity->GetProxy(ENTITY_PROXY_AREA);
							if(pArea && pArea->CalcPointWithin(INVALID_ENTITYID, actorData.position, true))
							{
								actorCount[areaIndex]++;
								break;
							}
						}
					}
				}
			}

			return GenerateAnnouncement(&actorCount[0], k_areaCount, clientId);
		}
	}

	return INVALID_AUDIOSIGNAL_ID;
}

//---------------------------------------
TAudioSignalID CAreaAnnouncer::GenerateAnnouncement(const int* actorCount, const int k_areaCount, const EntityId clientId)
{
	int maxActorArea = -1;	//busiest area
	int maxActorCount = 0;

	for(int i = 0; i < k_areaCount; i++)
	{

#if !defined(_RELEASE)
		if(aa_debug)
		{
			CryWatch("%s - %d", m_areaList[i].m_name, actorCount[i]);
		}
#endif

		if(actorCount[i] > maxActorCount)
		{
			maxActorArea = i;
			maxActorCount = actorCount[i];
		}
	}

	if(maxActorCount >= aa_peopleNeeded)
	{
		return m_areaList[maxActorArea].m_signal[GetAreaAnnouncerTeamIndex(clientId)];
	}

	return INVALID_AUDIOSIGNAL_ID;
}

//---------------------------------------
//Returns signal index of 0 (for team 0 and 1) and 1 for team 2
int CAreaAnnouncer::GetAreaAnnouncerTeamIndex(const EntityId clientId)
{
	int team = 0;
	CGameRules* pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
	{
		team = pGameRules->GetTeam(clientId);
	}
	return CLAMP(team - 1, 0, AREA_ANNOUNCERS - 1);
}

#if !defined(_RELEASE)
//static-------------------------------------------------------
void CAreaAnnouncer::CmdPlay(IConsoleCmdArgs* pCmdArgs)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	CAreaAnnouncer* pAA = pGameRules->GetAreaAnnouncer();

	if(!pAA->m_areaList.empty())
	{
		int randA = cry_random(0U, pAA->m_areaList.size() - 1);

		const EntityId clientId = gEnv->pGameFramework->GetClientActorId();
		CAudioSignalPlayer::JustPlay(pAA->m_areaList[randA].m_signal[pAA->GetAreaAnnouncerTeamIndex(clientId)]);

		CryLogAlways("Playing - %s_%d", pAA->m_areaList[randA].m_name, pAA->GetAreaAnnouncerTeamIndex(clientId));
	}
	else
	{
		CryLogAlways("Unable to play because there aren't any areas!");
	}
}

//static-------------------------------------------------------
void CAreaAnnouncer::CmdReload(IConsoleCmdArgs* pCmdArgs)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	CAreaAnnouncer* pAA = pGameRules->GetAreaAnnouncer();
	pAA->Init();
}
#endif
