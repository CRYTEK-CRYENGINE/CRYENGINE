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

#ifndef _GameRulesStandardRounds_h_
#define _GameRulesStandardRounds_h_

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesTeamChangedListener.h"
#include <CryString/CryFixedString.h>
#include "GameRulesTypes.h"
#include "IGameRulesStateModule.h"

class CGameRules;

class CGameRulesStandardRounds :	public IGameRulesRoundsModule,
									public IGameRulesTeamChangedListener,
									public IGameRulesStateListener
{
public:
	CGameRulesStandardRounds();
	virtual ~CGameRulesStandardRounds();

	// IGameRulesRoundsModule
	virtual void Init(XmlNodeRef xml);
	virtual void PostInit();
	virtual void Update(float frameTime);

	virtual void OnStartGame();
	virtual void OnEnterSuddenDeath();

	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );

	virtual void OnLocalPlayerSpawned();
	virtual void OnEndGame(int teamId, EntityId playerId, EGameOverReason reason);
	virtual int GetRoundNumber();
	virtual int GetRoundsRemaining() const; // Returns 0 if currently on last round
	virtual void SetTreatCurrentRoundAsFinalRound(const bool treatAsFinal) { m_treatCurrentRoundAsFinalRound = treatAsFinal; }

	virtual int GetPrimaryTeam() const;

	virtual bool CanEnterSuddenDeath() const;
	virtual bool IsInProgress() const								{ return (m_roundState == eGRRS_InProgress || m_roundState == eGRRS_SuddenDeath); }
	virtual bool IsInSuddenDeath() const						{ return (m_roundState == eGRRS_SuddenDeath); }
	virtual bool IsRestarting() const								{ return (m_roundState == eGRRS_Restarting); }
	virtual bool IsRestartingRound(int round) const	{ return (round == eGRRS_Restarting); }
	virtual bool IsGameOver() const									{ return (m_roundState == eGRRS_GameOver); }

	virtual float GetTimeTillRoundStart() const;

	virtual int GetPreviousRoundWinnerTeamId() const { return m_previousRoundWinnerTeamId; }
	virtual const int* GetPreviousRoundTeamScores(void) const { return m_previousRoundTeamScores; }
	virtual EGameOverReason GetPreviousRoundWinReason() const { return m_previousRoundWinReason; }

	virtual ERoundEndHUDState GetRoundEndHUDState() const { return m_roundEndHUDState; }

	virtual void OnPromoteToServer();
	
#if USE_PC_PREMATCH
	virtual void OnPrematchStateEnded(bool isSkipped); 
#endif // #if USE_PC_PREMATCH

	void ShowRoundStartingMessage(bool bPlayAudio);

	virtual bool ShowKillcamAtEndOfRound() const { return m_bShowKillcamAtEndOfRound; }
	virtual void AdjustTimers(CTimeValue adjustment);
	// ~IGameRulesRoundsModule

	// IGameRulesTeamChangedListener
	virtual void OnChangedTeam(EntityId entityId, int oldTeamId, int newTeamId);
	// ~IGameRulesTeamChangedListener

	// IGameRulesStateListener
	virtual void OnIntroStart() {}
	virtual void OnGameStart();
	virtual void OnGameEnd() {}
	virtual void OnStateEntered(IGameRulesStateModule::EGR_GameState newGameState)  {}
	// ~IGameRulesStateListener

	enum ERoundType
	{
		ERT_Any,
		ERT_Odd,
		ERT_Even,
	};

protected:
	typedef CryFixedStringT<32> TFixedString;
	typedef std::vector<EntityId> TEntityIdVec;

	struct SEntityDetails
	{
		TEntityIdVec m_currentEntities;
		TFixedString m_activateFunc;
		TFixedString m_deactivateFunc;
		const IEntityClass *m_pEntityClass;
		int m_selectCount;
		bool m_doRandomSelection;
	};

	struct SOnEndRoundVictoryStrings
	{
		SOnEndRoundVictoryStrings()
		{
			m_reason = EGOR_Unknown;
			m_round = ERT_Any;
		}

		EGameOverReason m_reason;
		ERoundType m_round;

		TFixedString m_roundWinVictoryMessage;
		TFixedString m_roundLoseVictoryMessage;
	};

	struct SOnEndRoundStrings
	{
		SOnEndRoundStrings()
		{
			m_reason = EGOR_Unknown;
		}

		EGameOverReason m_reason;

		TFixedString m_roundWinMessage;
		TFixedString m_roundLoseMessage;
		TFixedString m_roundDrawMessage;
	};

	enum ERoundState
	{
		eGRRS_InProgress,		// Round is in progress
		eGRRS_Restarting,		// New round is about to start
		eGRRS_SuddenDeath,	// Sudden death mode
		eGRRS_Finished,			// All rounds finished (but still showing end of round scores)
		eGRRS_GameOver,			// Finished showing end of round scores
	};

	typedef std::vector<SEntityDetails> TEntityDetailsVec;

	static const int MAX_END_OF_ROUND_STRINGS = 3;
	static const int k_MaxEndOfRoundVictoryStrings = 4;

	void StartNewRound(bool isReset);
	void EndRound();
	void ReadEntityClasses(TEntityDetailsVec &classesVec, XmlNodeRef xml, TFixedString &startRoundString, bool &startRoundStringIsTeamMessage, TFixedString &startRoundStringExtra, bool &bShowTeamBanner, TFixedString &startRoundHeaderString, bool &bCustomHeader);
	void SetupEntities();
	void SetTeamEntities(TEntityDetailsVec &classesVec, int teamId);
	void GetTeamEntities(TEntityDetailsVec &classesVec, int teamId);
	void CallScript(EntityId entityId, const char *pFunction);
	void ActivateEntity(EntityId entityId, int teamId, const char *pActivateFunc, TEntityIdVec &pEntitiesVec);
	void DeactivateEntities(const char *pDeactivateFunc, TEntityIdVec &entitiesVec);
	void CheckForTeamEntity(IEntity *pEntity, int newTeamId, TEntityDetailsVec &entitiesVec);

	void DisplaySuddenDeathMessage();
	void ClDisplayEndOfRoundMessage();
	const char* ClGetVictoryMessage(const int localTeam) const;
	const float GetVictoryMessageTime() const;
	const bool IsCurrentRoundType(const ERoundType roundType) const;

	bool IsFinished() const { return (m_roundState == eGRRS_Finished); }
	void SetRoundState(ERoundState inRoundState);
	void EnterRoundEndHUDState(ERoundEndHUDState state);
	void UpdateRoundEndHUD(float frameTime);

	void CalculateNewRoundTimes(float serverTime);
	void CalculateEndGameTime(float serverTime);

	void OnRoundEnd();
	void NewRoundTransition();

	static void CmdNextRound(IConsoleCmdArgs *pArgs);

	SOnEndRoundVictoryStrings m_endOfRoundVictoryStrings[k_MaxEndOfRoundVictoryStrings];
	SOnEndRoundStrings m_endOfRoundStrings[MAX_END_OF_ROUND_STRINGS];
	SOnEndRoundStrings m_endOfRoundStringsDefault;

	TEntityDetailsVec m_primaryTeamEntities;
	TEntityDetailsVec m_secondaryTeamEntities;

	// strings
	TFixedString m_primaryStartRoundString;
	TFixedString m_secondaryStartRoundString;
	TFixedString m_primaryStartRoundStringExtra;
	TFixedString m_secondaryStartRoundStringExtra;
	TFixedString m_primaryCustomHeader;
	TFixedString m_secondaryCustomHeader;


	TFixedString m_victoryMessage;
	TFixedString m_victoryDescMessage;

	int m_previousRoundTeamScores[2];
	
	int m_endOfRoundStringCount;
	int m_endOfRoundVictoryStringCount;
	int m_primaryTeamOverride; 

	int m_missedLoadout;
	
	int m_roundNumber;		// Start on round 0
	int m_previousRoundWinnerTeamId;
	int m_numLocalSpawnsThisRound;
	EntityId m_previousRoundWinnerEntityId;
	EGameOverReason m_previousRoundWinReason;
	
	float m_prevServerTime;
	float m_newRoundStartTime;
	float m_newRoundShowLoadoutTime;
	float m_victoryMessageTime;
	float m_suddenDeathTime;
	float m_timeSinceRoundEndStateChange;

	ERoundState m_roundState;
	ERoundEndHUDState m_roundEndHUDState;

	bool m_resetScores;			// Reset scores on round end
	bool m_allowBestOfVictory;
	bool m_treatCurrentRoundAsFinalRound;
	bool m_primaryStartRoundStringIsTeamMessage;
	bool m_secondaryStartRoundStringIsTeamMessage;
	bool m_bShownLoadout;
	bool m_bShowPrimaryTeamBanner;
	bool m_bShowSecondaryTeamBanner;
	bool m_bShowKillcamAtEndOfRound;
	bool m_bCustomHeaderPrimary;
	bool m_bCustomHeaderSecondary;
	bool m_bShowRoundStartingMessageEverySpawn;
};

#endif // _GameRulesStandardRounds_h_
