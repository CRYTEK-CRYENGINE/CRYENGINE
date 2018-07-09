// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 

	-------------------------------------------------------------------------
	History:
	- 02:07:2009   13:16 : Created by Frank Harrison

*************************************************************************/
#ifndef __GAMERULESTYPES_H__
#define __GAMERULESTYPES_H__

#include "AutoEnum.h"

class CActor;

#define AEGameModeList(f) \
	f(eGM_SinglePlayer) \
	f(eGM_AllOrNothing) \
	f(eGM_Assault) \
	f(eGM_BombTheBase) \
	f(eGM_CaptureTheFlag) \
	f(eGM_Countdown) \
	f(eGM_CrashSite) \
	f(eGM_Extraction) \
	f(eGM_InstantAction) \
	f(eGM_TeamInstantAction) \
	f(eGM_PowerStruggle) \
	f(eGM_Gladiator) \
	f(eGM_DeathMatch) \

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EGameMode, AEGameModeList, eGM_INVALID_GAMEMODE, eGM_NUM_GAMEMODES);
AUTOENUM_BUILDFLAGS_WITHZERO_WITHBITSUFFIX(AEGameModeList, eGM_NULL_bit);

typedef int8 EGameRulesScoreType; // if this type changes update compression policy in ScoreChangeParams
typedef int16 TGameRulesScoreInt; // if this type changes update compression policy and score limits in ScoreChangeParams

#define EGRSTList(f) \
	f(EGRST_PlayerKill)										/*WARNING: If you change the first item in this list you need to update the SvOnXPChanged RMI in Player.cpp*/ \
	f(EGRST_PlayerKillAssist)							\
	f(EGRST_PlayerKillAssistTeam1)				\
	f(EGRST_PlayerKillAssistTeam2)				\
	f(EGRST_PlayerTeamKill)								\
	f(EGRST_Suicide)											\
	f(EGRST_Tagged_PlayerKillAssist)			\
	f(EGRST_SingleHanded)									\
	f(EGRST_FlagAssist)										\
	f(EGRST_CaptureObjectiveHeld)					\
	f(EGRST_CaptureObjectiveTaken)				\
	f(EGRST_CaptureObjectiveCompleted)		\
	f(EGRST_CaptureObjectivesDefended)		\
	f(EGRST_AON_Win)											\
	f(EGRST_AON_Draw)											\
	f(EGRST_CarryObjectiveTaken)					\
	f(EGRST_CarryObjectiveRetrieved)			\
	f(EGRST_CarryObjectiveCarrierKilled)	\
	f(EGRST_CarryObjectiveDefended)				\
	f(EGRST_CarryObjectiveCompleted)			\
	f(EGRST_BombTheBaseCompleted)					\
	f(EGRST_KingOfTheHillObjectiveHeld)		\
	f(EGRST_HoldObj_OffensiveKill)				\
	f(EGRST_HoldObj_DefensiveKill)				\
	f(EGRST_CombiCapObj_Att_PlayerKill)		\
	f(EGRST_CombiCapObj_Def_PlayerKill)		\
	f(EGRST_CombiCapObj_Win)							\
	f(EGRST_CombiCapObj_Def_Win_IntelRemainBonus_Max) \
	f(EGRST_CombiCapObj_Att_Lost_IntelDownloadedBonus_Max) \
	f(EGRST_CombiCapObj_Def_Lost_TimeRemainBonus_Max) \
	f(EGRST_CombiCapObj_Capturing_PerSec) \
	f(EGRST_Extraction_AttackingKill)			\
	f(EGRST_Extraction_DefendingKill)			\
	f(EGRST_Extraction_ObjectiveReturnDefend) \
	f(EGRST_Gladiator_KillAsSoldier)			\
	f(EGRST_Gladiator_KillAsGladiator)		\
	f(EGRST_Gladiator_ActivateDisruptor)	\
	f(EGRST_Predator_KillAsSoldier)				\
	f(EGRST_Predator_KillAsPredator)			\
	f(EGRST_Predator_FinalKillAsPredator)	\
	f(EGRST_Predator_LastMarineStanding)	\
	f(EGRST_Predator_TwoMarinesRemaining)	\
	f(EGRST_Predator_ThreeMarinesRemaining)	\
	f(EGRST_Predator_SurviveTimePeriod)		\
	f(EGRST_Predator_SurviveToEnd)				\
	f(EGRST_PowerStruggle_CaptureSpear)		\
	f(EGRST_PowerStruggle_CaptureGiantSpear)		\
/* Any scoring types which want to be counted towards the gamemode points column on the scoreboard must be added to CGameRules::IsGamemodeScoringEvent(). */

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EGRST, EGRSTList, EGRST_Unknown, EGRST_Num);

//Non-score events that give you xp
// The order determines the points associated for the event within the highlight reel (higher is more likely to be shown)
#define PlayerProgressionType(f)	\
	f(EPP_FirstBlood)								/*WARNING: If you change the first item in this list you need to update the SvOnXPChanged RMI in Player.cpp*/ \
	f(EPP_KickedCar)								\
	f(EPP_QuinKill)									\
	f(EPP_QuadKill)									\
	f(EPP_TripleKill)								\
	f(EPP_DoubleKill)								\
	f(EPP_StealthKill)							\
	f(EPP_StealthKillWithSPModule)   \
	f(EPP_Recovery)									\
	f(EPP_BlindKill)								\
	f(EPP_Blinding)									\
	f(EPP_Flushed)									\
	f(EPP_DualWeapon)								\
	f(EPP_NearDeathExperience)			\
	f(EPP_KillJoy)									\
	f(EPP_Intervention)							\
	f(EPP_GotYourBack)							\
	f(EPP_Guardian)									\
	f(EPP_Retaliation)							\
	f(EPP_Piercing)									\
	f(EPP_Rumbled)									\
	f(EPP_MeleeTakedown)						\
	f(EPP_Headshot)									\
	f(EPP_AirDeath)									\
	f(EPP_TeamRadar)								\
	f(EPP_MicrowaveBeam)						\
	f(EPP_SuitBoost)								\
	f(EPP_Swarmer)									\
	f(EPP_EMPBlast)									\
	f(EPP_BlindAssist)							\
	f(EPP_FlushedAssist)						\
	f(EPP_SuitSuperChargedKill)			\
	f(EPP_Incoming)									\
	f(EPP_Pang)											\
	f(EPP_AntiAirSupport)						\


AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EPPType, PlayerProgressionType, EPP_Invalid, EPP_Max);

#define XPIncReasons(f) \
	f(MatchBonus) \
	f(Cheat) \
	f(PreOrder) \
	f(Unknown) \
	f(SkillAssessment) /*TODO: remove and replace with more detailed reasons */

#define XPRSN_AUTOENUM(f)		k_XPRsn_ ## f,

enum EXPReason
{
	PlayerProgressionType(XPRSN_AUTOENUM)
	EGRSTList(XPRSN_AUTOENUM)
	XPIncReasons(XPRSN_AUTOENUM)
	k_XPRsn_Num
};

// mapping macros from one enum to another, note, the values chosen below (eg EGRST_PlayerKill) are arbitrary, they just need to be the same
// value from both enums
#define EXPReasonFromEGRST(egrstEnumValue)			EXPReason(int(egrstEnumValue-EGRST_PlayerKill)+int(k_XPRsn_EGRST_PlayerKill))
#define EXPReasonFromEPP(eppEnumValue)					EXPReason(int(eppEnumValue-EPP_TeamRadar)+int(k_XPRsn_EPP_TeamRadar))

#undef XPRSN_AUTOENUM

union UGameRulesScoreData
{
	struct
	{
		EntityId  victim;
	}
	PlayerKill;
};

struct SGameRulesScoreInfo
{
	static const TGameRulesScoreInt SCORE_MAX = SHRT_MAX;
	static const TGameRulesScoreInt SCORE_MIN = SHRT_MIN;

	UGameRulesScoreData data;
	EGameRulesScoreType type;
	TGameRulesScoreInt score;
	TGameRulesScoreInt xp;
	EXPReason					xpRsn;

	explicit SGameRulesScoreInfo(EGameRulesScoreType typeIn, TGameRulesScoreInt scoreIn)
	{
		Set(typeIn, scoreIn, scoreIn, k_XPRsn_Unknown);
	}

	explicit SGameRulesScoreInfo(EGameRulesScoreType typeIn, TGameRulesScoreInt scoreIn, TGameRulesScoreInt xpIn, EXPReason inXPIncReason)
	{
		Set(typeIn, scoreIn, xpIn, inXPIncReason);
	}

	void Set(EGameRulesScoreType typeIn, TGameRulesScoreInt scoreIn, TGameRulesScoreInt xpIn, EXPReason inXPIncReason)
	{
		memset (& data, 0, sizeof(data));
		type = typeIn;

		score = scoreIn;
		xp = xpIn;
		xpRsn=inXPIncReason;

	}

	explicit SGameRulesScoreInfo( const SGameRulesScoreInfo& _score )
	: data(_score.data)
	, type(_score.type)
	, score(_score.score)
	, xp(_score.xp)
	, xpRsn(_score.xpRsn)
	{
		// ...
	}

	ILINE SGameRulesScoreInfo & AttachVictim(EntityId id)
	{
		data.PlayerKill.victim = id;
		return *this;
	}
};

enum EGameRulesTargetType
{
	EGRTT_Unknown = -1,
	EGRTT_Neutral = 0,
	EGRTT_Friendly = 1,
	EGRTT_Hostile = 2,
	EGRTT_NeutralVehicle = 3,
	EGRTT_ShieldedEntity = 4
};

struct SGameRulesNotificationInfo
{
	SGameRulesNotificationInfo()
						: message(0), arg1(0), arg2(0), type(EGRTT_Neutral) {};

	SGameRulesNotificationInfo(EGameRulesTargetType _type, const char *_message, const char *_arg1, const char *_arg2)
						: message(_message), arg1(_arg1), arg2(_arg2), type(_type) {};

	const char *message;
	const char *arg1;
	const char *arg2;

	EGameRulesTargetType type;
};

enum EGameRulesWinner
{
	EGRW_Player = 1,
	EGRW_OtherPlayer = 2,
	EGRW_PlayersTeam = 3,
	EGRW_OpponentsTeam = 4,
	EGRW_NoWinner = 5,
	EGRW_Unknown // no more after me.
};

enum EGameOverReason
{
	EGOR_TimeLimitReached = 1,
	EGOR_ScoreLimitReached = 2,
	EGOR_ObjectivesCompleted = 3,
	EGOR_NoLivesRemaining = 4,
	EGOR_OpponentsDisconnected = 5,
	EGOR_DrawRoundsResolvedWithMessage = 6,
	EGOR_Unknown // no more after me.
};

enum EGameOverType
{
	EGOT_Lose = -1,
	EGOT_Draw = 0,
	EGOT_Win = 1,
	EGOT_Unknown
};


struct SDrawResolutionData
{
	typedef CryFixedStringT<32> TFixedString;

	float m_floatDataForTeams[2];
	int m_intDataForTeams[2];

	enum EDataType
	{
		eDataType_float,
		eDataType_int,
	};
	EDataType m_dataType;

	enum EWinningDataTest
	{
		eWinningData_greater_than=0,
		eWinningData_less_than,
	};
	EWinningDataTest m_dataTest;

	TFixedString m_winningMessage;
	bool m_active;

	SDrawResolutionData()
	{
		Clear();
	}

	void Clear()
	{
		m_floatDataForTeams[0] = 0.f;
		m_floatDataForTeams[1] = 0.f;
		m_intDataForTeams[0] = 0;
		m_intDataForTeams[1] = 0;
		m_active=false;
		m_dataType = eDataType_int;
		m_dataTest = eWinningData_greater_than;
	}

	void SerializeWith(TSerialize ser)
	{
		ser.Value("float0", m_floatDataForTeams[0], 'fsec');
		ser.Value("float1", m_floatDataForTeams[1], 'fsec');
		ser.Value("int0", m_intDataForTeams[0], 'i32');
		ser.Value("int1", m_intDataForTeams[1], 'i32');
		int dataType = (int) m_dataType;
		ser.Value("dataType", dataType, 'ui2');
		if(ser.IsReading())
		{
			m_dataType = (EDataType) dataType;
		}
	}
};

#define ESVC_DrawResolutionList(f) \
	f(ESVC_DrawResolution_level_1)					/*0*/ \
	f(ESVC_DrawResolution_level_2)					/*1*/ \

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(ESVC_DrawResolution, ESVC_DrawResolutionList, ESVC_DrawResolution_invalid, ESVC_DrawResolution_num);

struct GameRulesGameOverInfo{
	EGameRulesWinner gamewinner;
	int showhud;
};

// TODO: Remove this enum, use the ID values specified in the XML
#define MissionObjectiveList(f)		        \
	f(EGRMO_DONOTUSE)                          /*00*/ \
	f(EGRMO_CTF_Base_with_flag_blue)                  \
	f(EGRMO_CTF_Base_with_flag_red)                   \
	f(EGRMO_CTF_Base_no_flag_blue)                    \
	f(EGRMO_CTF_Base_no_flag_red)                     \
	f(EGRMO_CTF_Flag_carried_blue)                    \
	f(EGRMO_CTF_Flag_carried_red)                     \
	f(EGRMO_CTF_Flag_dropped_blue)                    \
	f(EGRMO_CTF_Flag_dropped_red)                     \
	f(EGRMO_ExplosiveThreat_blue)                     \
	f(EGRMO_ExplosiveThreat_red)                      \
                                                      \
	f(EGRMO_AS_IntelPoint_blue)                       \
	f(EGRMO_AS_IntelPoint_red)                        \
	f(EGRMO_AS_IntelPoint_blueToRed)                  \
	f(ERGMO_EXT_StealthAtBase_blue)										\
	f(ERGMO_EXT_StealthAtBase_red)										\
	f(ERGMO_EXT_ArmourAtBase_blue)										\
	f(ERGMO_EXT_ArmourAtBase_red)											\
	f(ERGMO_EXT_StealthDropped_blue)									\
	f(ERGMO_EXT_StealthDropped_red)										\
	f(ERGMO_EXT_ArmourDropped_blue)										\
	f(ERGMO_EXT_ArmourDropped_red)										\
	f(ERGMO_EXT_StealthCarried_blue)									\
	f(ERGMO_EXT_StealthCarried_red)										\
	f(ERGMO_EXT_ArmourCarried_blue)										\
	f(ERGMO_EXT_ArmourCarried_red)										\
	f(ERGMO_EXT_ExtractionPoint)											\
	/* capture site*/                                 \
	f(EGRMO_CS_CapturePoint_neutral)                  \
	f(EGRMO_CS_CapturePoint_contention)               \
	f(EGRMO_CS_CapturePoint_enemyOwned)               \
	f(EGRMO_CS_CapturePoint_teamOwned)                \
	/*TODO : Remove Me */ f(EGRMO_CS_CapturePoint_neutralToBlue)          \
	/*TODO : Remove Me */ f(EGRMO_CS_CapturePoint_neutralToRed)           \
	/*TODO : Remove Me */ f(EGRMO_CS_CapturePoint_redToBlue)              \
	/*TODO : Remove Me */ f(EGRMO_CS_CapturePoint_blueToRed)              \
	/*f(EGRMO_CS_Base_blue) not in XML?*/             \
	/*f(EGRMO_CS_Base_red) not in XML?*/              \
	f(EGRMO_FriendlyHolo)                             \
	f(EGRMO_AS_IntelPoint_redToBlue)                  \
	f(EGRMO_Mission_Objective)                        \
	f(EGRMO_Sec_Mission_Objective)                    \
	f(EGRMO_Ammo_Cache)                               \
	f(EGRMO_PS_Node_Neutral)                          \
	f(EGRMO_PS_Node_Friendly)                         \
	f(EGRMO_PS_Node_Hostile)                          \
	f(EGRMO_PS_Node_FriendlyCapturing)                \
	f(EGRMO_PS_Node_HostileCapturing)                 \
	f(EGRMO_PS_Node_FriendlyCapturingFromHostile)     \
	f(EGRMO_PS_Node_HostileCapturingFromFriendly)     \
	f(ERGMO_PS_Node_Contention)												\
	f(EGRMO_PS_Node_Neutral_Secondary)                   \
	f(EGRMO_PS_Node_Friendly_Secondary)                  \
	f(EGRMO_PS_Node_Hostile_Secondary)                   \
	f(EGRMO_PS_Node_FriendlyCapturing_Secondary)         \
	f(EGRMO_PS_Node_HostileCapturing_Secondary)          \
	f(EGRMO_PS_Chargee_Neutral)                          \
	f(EGRMO_PS_Chargee_Friendly)                         \
	f(EGRMO_PS_Chargee_Hostile)                          \
	f(EGRMO_PS_Chargee_FriendlyCapturing)                \
	f(EGRMO_PS_Chargee_HostileCapturing)                 \
	f(EGRMO_GL_Disruptor_Neutral)                        \
	f(EGRMO_GL_Disruptor_Charging)                       \
	f(EGRMO_GL_Disruptor_Friendly)                       \
	f(EGRMO_GL_Disruptor_Hostile)                        \
	f(EGRMO_GL_Hunter)                                   \
	f(EGRMO_GL_Hunter_Dead)                              \

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EGameRulesMissionObjectives, MissionObjectiveList, EGRMO_Unknown, EGRMO_Max);

struct SGameRulesMissionObjectiveInfo
{
	EntityId targetId;
	EGameRulesMissionObjectives objectiveType;

	static EGameRulesMissionObjectives GetIconId(const char* nameOrNumber);
};

enum EGoodBadNeutralForLocalPlayer
{
	eGBNFLP_Neutral=0,
	eGBNFLP_Good,
	eGBNFLP_Bad,
};

enum ESpawnEntityState
{
	ESES_AtBase,
	ESES_Carried,
	ESES_Dropped,
	eSES_Unknown,
};

struct HitInfo;

struct SBattleLogMessageInfo
{
	const HitInfo *hitInfo;
	CryFixedStringT<35> weaponClassName;
	int      hitType;
	EntityId shooterId;
	EntityId targetId;
	EntityId shooterWeaponId;
	uint16	projectileClassId;

	SBattleLogMessageInfo()
	{
		hitInfo=NULL;
		hitType=0;
		shooterId=0;
		targetId=0;
		shooterWeaponId=0;
		projectileClassId=0;
	}
};

struct SChatLogMessageInfo
{
	EntityId  senderId;

	SChatLogMessageInfo()
	{
		senderId=0;
	}
};

struct STeamChangeInfo
{
	int oldTeamNum;
	int newTeamNum;
};

struct SGameRulesListener
{
	virtual	~SGameRulesListener(){}

	virtual void GameOver(EGameOverType localWinner, bool isClientSpectator) {}
	virtual void EnteredGame() {}
	virtual void EndGameNear(EntityId id) {}
	virtual void ClientEnteredGame( EntityId clientId ) {}
	virtual void ClientDisconnect( EntityId clientId ) {}
	virtual void OnActorDeath( CActor* pActor ) {}
	virtual void SvOnTimeLimitExpired() {}
	virtual void ClTeamScoreFeedback(int teamId, int prevScore, int newScore) {}

	void GetMemoryUsage(ICrySizer *pSizer ) const {}
};

#if PC_CONSOLE_NET_COMPATIBLE
#define USE_PC_PREMATCH 0
#define PC_PREMATCH_RMI_COUNT 0
#else
#define USE_PC_PREMATCH 1
#define PC_PREMATCH_RMI_COUNT 1
#endif

#define MP_MODEL_INDEX_NET_POLICY		'ui4'
// Default is highest value that we can serialise in the above policy
#define MP_MODEL_INDEX_DEFAULT			15

struct SVoteData
{
	SVoteData(EntityId _voterId, bool _bVotedToKick) : voterId(_voterId), bVotedToKick(_bVotedToKick) {}

	EntityId	voterId;
	bool			bVotedToKick;
};

typedef std::vector<SVoteData> TVoteDataList;

typedef std::vector<EntityId> TEntityIdVec;

#endif //__GAMERULESTYPES_H__
