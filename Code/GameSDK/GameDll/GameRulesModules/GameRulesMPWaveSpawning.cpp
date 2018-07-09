// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameRulesMPWaveSpawning.h"
#include "IGameRulesPlayerStatsModule.h"
#include "Player.h"

CGameRulesMPWaveSpawning::CGameRulesMPWaveSpawning():
m_fWaveSpawnInterval(0.0f)
, m_fWaveSpawnTimer(0.0f)
,m_ruleFlags(ERuleFlag_None) 
{

}

CGameRulesMPWaveSpawning::~CGameRulesMPWaveSpawning()
{

}

void CGameRulesMPWaveSpawning::Init( XmlNodeRef xml )
{
	TInherited::Init(xml); 

	m_ruleFlags = ERuleFlag_None; 

	bool bTemp = false; 

	if(xml->getAttr("team1WaveSpawnEnabled", bTemp) && bTemp)
	{
		m_ruleFlags |= ERuleFlag_team1WaveRespawningEnabled;
	}

	bTemp = false; 
	if(xml->getAttr("team2WaveSpawnEnabled", bTemp) && bTemp)
	{
		m_ruleFlags |= ERuleFlag_team2WaveRespawningEnabled;
	}

	xml->getAttr("waveSpawnInterval",  m_fWaveSpawnInterval); 
	m_fWaveSpawnTimer = m_fWaveSpawnInterval;

}

void CGameRulesMPWaveSpawning::Update( float frameTime )
{
	TInherited::Update(frameTime); 

	// Update wave respawns
	if(WaveSpawningEnabled())
	{
		//CryWatch("Next Reinforcement Wave in: %.4f", m_fMarineWaveSpawnTimer);
		m_fWaveSpawnTimer -= frameTime; 
		if(m_fWaveSpawnTimer <= 0.0f)
		{
			if(gEnv->bServer)
			{
				if(m_ruleFlags & ERuleFlag_team1WaveRespawningEnabled)
				{
					// This function only revives DEAD players on a team, which is what we want
					ReviveAllPlayersOnTeam(1);
				}
				if(m_ruleFlags & ERuleFlag_team2WaveRespawningEnabled)
				{
					// This function only revives DEAD players on a team, which is what we want
					ReviveAllPlayersOnTeam(2);
				}
			}

			ResetWaveRespawnTimer();
		}
	}
}

bool CGameRulesMPWaveSpawning::WaveSpawningEnabled() const
{
	if ( m_pGameRules->HasGameActuallyStarted() && 
		m_fWaveSpawnInterval > 0.0f)
	{
		return true;
	}
	return false; 
}

bool CGameRulesMPWaveSpawning::WaveSpawningEnabled( int teamId ) const
{
	if(WaveSpawningEnabled())
	{
		if( teamId == 1 && (m_ruleFlags & ERuleFlag_team1WaveRespawningEnabled) ||
			teamId == 2 && (m_ruleFlags & ERuleFlag_team2WaveRespawningEnabled))
		{
			return true;
		}
	}
	return false; 
}


void CGameRulesMPWaveSpawning::ResetWaveRespawnTimer()
{
	float timeVal	=  m_pGameRules->GetCurrentGameTime();	
	timeVal			= fmodf(timeVal,m_fWaveSpawnInterval); 

	// getCurrentTime is ticking UP so we need remainder to get our timer. 
	m_fWaveSpawnTimer    = m_fWaveSpawnInterval - timeVal;
}

void CGameRulesMPWaveSpawning::EntityRevived( EntityId entityId )
{
	TInherited::EntityRevived(entityId); 

	// Need to force timer sync immediately after *initial* spawn to ensure syncing
	// (game timer not valid even as late as during postStartGame)
	IGameRulesPlayerStatsModule *pStatsModule = m_pGameRules->GetPlayerStatsModule();
	if (pStatsModule)
	{
		const SGameRulesPlayerStat *pStats = pStatsModule->GetPlayerStats(entityId);
		if (pStats && pStats->deaths == 0)
		{
			ResetWaveRespawnTimer(); 
		}
	}
}

float CGameRulesMPWaveSpawning::GetTeamReviveTimeOverride( int teamId ) const
{
	if(WaveSpawningEnabled(teamId))
	{
		return m_fWaveSpawnTimer;
	}
	return -1.0f; // no override
}

float CGameRulesMPWaveSpawning::GetTimeFromDeathTillAutoReviveForTeam( int teamId ) const
{
	// Check if have an override else call default
	float timeOverride = GetTeamReviveTimeOverride(teamId);

	if(timeOverride >= 0.0f)
	{
		// Need to factor in dead time due to the way the UI handles this :(
		CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
		if(pPlayer)
		{
			const float deathTime = pPlayer->GetDeathTime();
			if (deathTime > 0.f)
			{
				const float timeBeenDeadFor = (gEnv->pTimer->GetFrameStartTime().GetSeconds() - deathTime);
				timeOverride += timeBeenDeadFor; 
			}
		}
		return timeOverride; 
	}
	else
	{
		return TInherited::GetTimeFromDeathTillAutoReviveForTeam(teamId); 
	}
}


