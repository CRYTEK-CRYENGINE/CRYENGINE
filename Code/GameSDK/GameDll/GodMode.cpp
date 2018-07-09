// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GodMode.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "StatsRecordingMgr.h"
#include "Actor.h"
#include "UI/HUD/HUDEventDispatcher.h"

const float CGodMode::m_timeToWaitBeforeRespawn = 4.0f;
const char * CGodMode::m_godModeCVarName = "g_godMode";
const char * CGodMode::m_demiGodRevivesAtCheckpointCVarName = "g_demiGodRevivesAtCheckpoint";

CGodMode::CGodMode()
	: m_respawningFromDemiGodDeath(false)
	, m_elapsedTime(0.0f)
	, m_godMode(0)
	, m_demiGodRevivesAtCheckpoint(1)
{
	ClearCheckpointData();
}

void CGodMode::ClearCheckpointData()
{
	m_lastCheckpointWorldTM.SetIdentity();
	m_hasHitCheckpoint = false;
}

void CGodMode::SetNewCheckpoint(const Matrix34& rWorldMat)
{
	m_hasHitCheckpoint = true;
	m_lastCheckpointWorldTM = rWorldMat;
}

CGodMode& CGodMode::GetInstance()
{
	static CGodMode godMode;
	return godMode;
}

void CGodMode::RegisterConsoleVars(IConsole* pConsole)
{
	REGISTER_CVAR2(m_godModeCVarName, &m_godMode, 0, VF_CHEAT, "God Mode");
	REGISTER_CVAR2(m_demiGodRevivesAtCheckpointCVarName, &m_demiGodRevivesAtCheckpoint, 1, VF_CHEAT, "Demi God Mode warps you to the last checkpoint");
}

void CGodMode::UnregisterConsoleVars(IConsole* pConsole) const
{
	pConsole->UnregisterVariable(m_godModeCVarName, true);
	pConsole->UnregisterVariable(m_demiGodRevivesAtCheckpointCVarName, true);
}

EGodModeState CGodMode::GetCurrentState() const
{
	return (EGodModeState)m_godMode;
}

const char* CGodMode::GetCurrentStateString() const
{
	const char* res = "None";

	switch (GetCurrentState())
	{
	case eGMS_GodMode:
		res = "GodMode";
		break;
	case eGMS_TeamGodMode:
		res =  "TeamGodMode";
		break;
	case eGMS_DemiGodMode:
		res =  "DemiGodMode";
		break;
	case eGMS_NearDeathExperience:
		res = "NearDeathExperience";
		break;
	}

	return res;
}

void CGodMode::MoveToNextState()
{
	m_godMode = (m_godMode + 1) % eGMS_LAST;
}

void CGodMode::DemiGodDeath()
{
	if (!gEnv->bMultiplayer && eGMS_DemiGodMode == GetCurrentState())
	{
		CActor* player = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());

		player->GetEntity()->GetAI()->Event(AIEVENT_DISABLE, NULL);
		player->SetHealth(0);
		player->CreateScriptEvent("kill", 0);

		m_respawningFromDemiGodDeath = true;
		m_elapsedTime = 0.0;
	}
}

bool CGodMode::IsGod() const
{
	EGodModeState currentState = GetCurrentState();
	return currentState == eGMS_GodMode || currentState == eGMS_TeamGodMode;
}

bool CGodMode::IsDemiGod() const
{
	return GetCurrentState() == eGMS_DemiGodMode;
}

bool CGodMode::IsGodModeActive() const
{
	EGodModeState currentState = GetCurrentState();
	return currentState != eGMS_None && currentState != eGMS_LAST;
}

bool CGodMode::RespawnIfDead(CActor *pActor) const
{
	if(pActor && pActor->IsDead())
	{
		pActor->StandUp();

		pActor->Revive(CActor::kRFR_Spawn);

		pActor->SetHealth(pActor->GetMaxHealth());

		pActor->HolsterItem(true);

		pActor->HolsterItem(false);

		if( IEntity *pEntity = pActor->GetEntity() )
		{
			pEntity->GetAI()->Event(AIEVENT_ENABLE, NULL);
		}

		if(m_demiGodRevivesAtCheckpoint != 0 && pActor->IsPlayer() && m_hasHitCheckpoint)
		{
			pActor->GetEntity()->SetWorldTM(m_lastCheckpointWorldTM);
		}

		CStatsRecordingMgr *pStatsRecordingMgr = g_pGame->GetStatsRecorder();

		if(pStatsRecordingMgr)
		{
			pStatsRecordingMgr->StartTrackingStats(pActor);

			CStatsRecordingMgr::TryTrackEvent(pActor, eGSE_Resurrection);
		}

		return true;
	}

	return false;
}

bool CGodMode::RespawnPlayerIfDead() const
{
	CActor* player = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	bool result = RespawnIfDead(player);
	if (result)
	{
		SHUDEvent event;
		event.eventType = eHUDEvent_OnHUDReload;
		event.AddData(SHUDEventData(true));
		event.AddData( SHUDEventData(false)); // Dynamically loaded
		CHUDEventDispatcher::CallEvent(event);
	}
	return result;
}

void CGodMode::Update(float frameTime)
{
	if (!gEnv->bMultiplayer && m_respawningFromDemiGodDeath && eGMS_DemiGodMode == GetCurrentState())
	{
		m_elapsedTime += frameTime;

		if (m_elapsedTime > m_timeToWaitBeforeRespawn)
		{
			RespawnPlayerIfDead();
			m_respawningFromDemiGodDeath = false;
		}
	}
}
