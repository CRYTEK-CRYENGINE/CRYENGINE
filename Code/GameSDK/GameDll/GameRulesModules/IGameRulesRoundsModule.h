// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 

	-------------------------------------------------------------------------
	History:
	- 26:10:2009  : Created by Colin Gulliver

*************************************************************************/

#ifndef _GameRulesRoundsModule_h_
#define _GameRulesRoundsModule_h_

#if _MSC_VER > 1000
# pragma once
#endif

#include <CryNetwork/SerializeFwd.h>
#include "IGameObject.h"
#include "GameRulesTypes.h"

#ifndef _RELEASE
#define LOG_PRIMARY_ROUND(...)  { if (g_pGameCVars->g_logPrimaryRound == 1) CryLog(__VA_ARGS__); }
#else
#define LOG_PRIMARY_ROUND(...)
#endif

class IGameRulesRoundsModule
{
public:
	enum ERoundEndHUDState
	{
		eREHS_Unknown,
		eREHS_HUDMessage,
		eREHS_Top3,
		eREHS_WinningKill,
	};

	virtual ~IGameRulesRoundsModule() {}

	virtual void Init(XmlNodeRef xml) = 0;
	virtual void PostInit() = 0;
	virtual void Update(float frameTime) = 0;

	virtual void OnStartGame() = 0;
	virtual void OnEnterSuddenDeath() = 0;

	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) = 0;

	virtual void OnLocalPlayerSpawned() = 0;
	virtual void OnEndGame(int teamId, EntityId playerId, EGameOverReason reason) = 0;
	virtual int GetRoundNumber() = 0;
	virtual int GetRoundsRemaining() const = 0;
	virtual void SetTreatCurrentRoundAsFinalRound(const bool treatAsFinal) = 0;

	virtual int GetPrimaryTeam() const = 0;

	virtual bool CanEnterSuddenDeath() const = 0;
	virtual bool IsInProgress() const	= 0;
	virtual bool IsInSuddenDeath() const = 0;
	virtual bool IsRestarting() const = 0;
	virtual bool IsGameOver() const = 0;
	virtual bool IsRestartingRound(int round) const = 0;
	virtual float GetTimeTillRoundStart() const = 0;

	virtual int GetPreviousRoundWinnerTeamId() const = 0;
	virtual const int* GetPreviousRoundTeamScores(void) const = 0;
	virtual EGameOverReason GetPreviousRoundWinReason() const = 0;

	virtual ERoundEndHUDState GetRoundEndHUDState() const = 0;

	virtual void OnPromoteToServer() = 0;

#if USE_PC_PREMATCH
	virtual void OnPrematchStateEnded(bool isSkipped) = 0; 
#endif // #if USE_PC_PREMATCH

	virtual bool ShowKillcamAtEndOfRound() const = 0;

	virtual void AdjustTimers(CTimeValue adjustment) = 0;
};

#endif // _GameRulesRoundsModule_h_
