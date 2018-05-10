// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 

	-------------------------------------------------------------------------
	History:
	- 03:09:2009  : Created by Colin Gulliver

*************************************************************************/

#ifndef _GameRulesStateModule_h_
#define _GameRulesStateModule_h_

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameObject.h"
#include <CryNetwork/SerializeFwd.h>


class IGameRulesStateListener;
class CPlayer;

class IGameRulesStateModule
{
public:
	enum EGR_GameState
	{
		EGRS_Reset = 0,
		EGRS_Intro, 
		EGRS_PreGame,
		EGRS_InGame,
		EGRS_PostGame,
		EGRS_MAX		// Must remain last
	};

	enum EPostGameState
	{
		ePGS_Unknown,
		ePGS_Starting,
		ePGS_HudMessage,
		ePGS_FinalKillcam,
		ePGS_HighlightReel,
		ePGS_Top3,
		ePGS_Scoreboard,
		ePGS_LeavingGame,
	};

	virtual ~IGameRulesStateModule() {}

	virtual void Init(XmlNodeRef xml) = 0;
	virtual void PostInit() = 0;

	virtual void OnGameRestart() = 0;
	virtual void OnGameReset() = 0;
	virtual void OnGameEnd() = 0;

	virtual EGR_GameState GetGameState() const = 0;

	virtual void Update(float frameTime) = 0;

	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) = 0;

	virtual EPostGameState GetPostGameState() = 0;

	virtual bool IsInitialChannelId(int channelId) const = 0;
	virtual void OwnClientEnteredGame(const CPlayer& rPlayer) = 0;

  //listener interfaces
  virtual void AddListener(IGameRulesStateListener * pListener) = 0;
  virtual void RemoveListener(IGameRulesStateListener * pListener) = 0;

	virtual void StartCountdown(bool bStart) = 0;
	virtual bool IsInCountdown() const = 0;

	virtual void Add3DWinningTeamMember() = 0;

	virtual bool IsOkToStartRound() const = 0; 
	virtual void OnIntroCompleted() = 0; 
};

class IGameRulesStateListener
{
public:
	virtual ~IGameRulesStateListener(){}
	virtual void OnIntroStart() = 0;
	virtual void OnGameStart() = 0;
	virtual void OnGameEnd() = 0;
	virtual void OnStateEntered(IGameRulesStateModule::EGR_GameState newGameState) = 0;
};

#endif // _GameRulesStateModule_h_
