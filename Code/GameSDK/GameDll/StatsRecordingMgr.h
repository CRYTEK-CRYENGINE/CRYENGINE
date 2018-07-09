// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: Implements various IGameStatistics callbacks and listens to
				 various game subsystems, forwarding necessary events to stats
				 recording system.

	-------------------------------------------------------------------------
	History:
	- 02:11:2009  : Created by Mark Tully

*************************************************************************/

#ifndef __STATSRECORDINGMGR_H__
#define __STATSRECORDINGMGR_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IItemSystem.h"
#include "IWeapon.h"
#include <CryGame/IGameStatistics.h>
#include "IGameRulesSystem.h"
#include "ITelemetryCollector.h"
#include "GameRulesTypes.h"
#include "DownloadMgr.h"
#include <CryThreading/IThreadManager.h>

class CXMLStatsSerializer;
class CCircularXMLSerializer;
class CActor;
class CCircularBufferStatsStorage;
struct IPlayerProfile;

//////////////////////////////////////////////////////////////////////////

// Game-specific events
enum EGameStatisticEvent
{
	eGSE_Melee = eSE_Num,
	eGSE_MeleeHit,
	eGSE_Firemode,
	eGSE_HitReactionAnim,
	eGSE_DeathReactionAnim,
	eGSE_SpectacularKill,
	eGSE_Jump,
	eGSE_Land,
	eGSE_Crouch,
	eGSE_Swim,
	eGSE_Use,
	eGSE_DropItem,
	eGSE_PickupItem,
	eGSE_Grab,
	eGSE_LedgeGrab,
	eGSE_Slide,
	eGSE_Sprint,
	eGSE_Zoom,
	eGSE_Save,
	eGSE_NewObjective,
	eGSE_PlayerAction,
	eGSE_Resurrection,
	eGSE_XPChanged,
	eGSE_VisionModeChanged,
	eGSE_Flashed,
	eGSE_Tagged,
	eGSE_ExchangeItem,
	eGSE_InteractiveEvent,
	eGSE_EnvironmentalWeaponEvent,
	eGSE_VTOL_EnterExit,
	eGSE_Hunter_RoundEnd,
	eGSE_Hunter_ChangeToPredator,
	eGSE_Spears_SpearStateChanged,
	eGSE_Num,
};

//////////////////////////////////////////////////////////////////////////

// Game-specific states
enum EGameStatisticsState
{
	eGSS_MapPath = eSS_Num,
	eGSS_Attachments,
	eGSS_TeamName,
	eGSS_TeamID,
	eGSS_RoundBegin,
	eGSS_RoundEnd,
	eGSS_RoundNumber,
	eGSS_RoundActuallyStarted,
	eGSS_PrimaryTeam,
	eGSS_Config,
	eGSS_MaxHealth,
	eGSS_OnlineGUID,
	eGSS_UserName,			// SP only - do not use in MP - TRC issue
	eGSS_LobbyVersion,
	eGSS_MatchMakingBlock,
	eGSS_MatchMakingVersion,
	eGSS_PatchPakVersion,
	eGSS_RunOnceVersion,
	eGSS_RunOnceTrackingVersion,
	eGSS_DataTruncated,
	eGSS_MemRequired,
	eGSS_MemAvailable,
	eGSS_BuildNumber,
	eGSS_StatsFileFormatVersion,
	eGSS_LifeNumber,
	eGSS_AIFaction,
	eGSS_Difficulty,
	eGSS_MatchBonusXp,
	eGSS_XP,
	eGSS_Rank,
	eGSS_DefaultRank,
	eGSS_StealthRank,
	eGSS_ArmourRank,
	eGSS_Reincarnations,
	eGSS_PatchId,
	eGSS_SkillLevel,
	eGSS_Interactables,
	eGSS_Num,
};

//////////////////////////////////////////////////////////////////////////

enum EGameStatisticsScopes
{
	eGSC_Session,
	eGSC_Round,

	eGSC_Num,
};

//////////////////////////////////////////////////////////////////////////

enum EGameStatisticsElements
{
	eGSEL_Team,
	eGSEL_Player,
	eGSEL_AIActor,
	eGSEL_Entity,

	eGSEL_Num,
};

enum EGameNodeLocators
{
	eGNLT_TeamID = eNLT_Num,
	eGNLT_ChannelID,

	eGNLT_Num,
};

enum EStatisticEventRecordType
{
	eSERT_Never,		// never record events of this type
	eSERT_Always,		// always record
	eSERT_Players,	// only record if the actor is a player
	eSERT_AI,				// only record if the actor is an AI
};

#define MAX_TEAM_INDEX 2

struct SBonusMatchXp
{
	SBonusMatchXp(EntityId _entityId, int _amount)
		: entityId(_entityId)
		, amount(_amount)
	{}
	EntityId entityId;
	int amount;
};

#define MAX_ENVIRONMENTAL_WEAPONS 32
#define MAX_INTERACTIVE_OBJECTS 16

struct SIntrestEntity
{
	SIntrestEntity( EntityId _entityId, int _type)
		: entityId(_entityId)
		, type(_type)
	{}
	EntityId entityId;
	int type;
};

class CTelemetryCopierThread : public IThread
{
public:
	CTelemetryCopierThread(ITelemetryProducer *pInProducer, CTelemetryCollector *telemetryCollector, string remotePath);
	~CTelemetryCopierThread();
	void Flush();

public:
	void ThreadEntry();

	CTelemetryCollector *m_telemetryCollector;
	ITelemetryProducer *m_pInProducer;
	ITelemetryProducer *m_pOutProducer;
	string m_remotePath;

#ifndef _RELEASE
	double m_time;
	double m_runTime;
#endif
};

class CStatsRecordingMgr : public IGameStatisticsCallback, public IHitListener, public IGameFrameworkListener, public IDataListener
{
	public:
		typedef uint8															TSubmitPermissions;
		enum
		{
			k_submitStatsLogs				= BIT(0),
			k_submitPlayerStats			= BIT(1),
			k_submitAccounts				= BIT(2),
			k_submitRemotePlayerStats		  = BIT(3),
			k_submitAntiCheatLogs		= BIT(4),
			k_submitConnectivityEventLogs = BIT(5),

			k_submitFullPermissions	= (TSubmitPermissions)0xffffffff
		};

	protected:
		IGameStatistics														*m_gameStats;
		_smart_ptr<CCircularBufferStatsStorage>		m_statsStorage;
		IStatsTracker															*m_sessionTracker;
		IStatsTracker															*m_roundTracker;
		string																		m_statsDirectory;
		CXMLStatsSerializer												*m_serializer;
		EStatisticEventRecordType									m_eventConfigurations[eGSE_Num];
		int																				m_checkpointCount;
		int																				m_lifeCount;
		bool																			m_requestNewSessionId;
		IStatsTracker															*m_teamTrackers[MAX_TEAM_INDEX+1];
		CryFixedArray<SBonusMatchXp, MAX_PLAYER_LIMIT> m_matchBonusXp;
		std::vector<SIntrestEntity>								m_interactiveObjects;
		std::vector<SIntrestEntity>								m_environmentalWeapons;
		TSubmitPermissions												m_submitPermissions;
		float																			m_endSessionCountdown;
		CTelemetryCopierThread										*m_copierThread;
		bool																			m_IsIDataListener;
		bool																			m_scheduleRemoveIDataListener;

		void																			StateActorWeapons(
																								IActor		*inActor);
		void																			StateCorePlayerStats(
																								IActor		*inPlayerActor);
		void																			StateEndOfSessionStats();

		void																			SetNewSessionId( bool includeMatchDetails );

		void																			ProcessOnlinePermission(
																								const char	*pKey,
																								const char	*pValue);


		static void																FreeTelemetryMemBuffer(
																								void		*pInUserData);

		void																			MakeValidXmlString(
																								stack_string	&str);
	public:
																							CStatsRecordingMgr();
																							~CStatsRecordingMgr();

		TSubmitPermissions												GetSubmitPermissions()		{ return m_submitPermissions; }

		static int																GetIntAttributeFromProfile(
																								IPlayerProfile *inProfile, 
																								const char *inAttributeName);

		string																		GetTimeLabel(time_t offsetSeconds) const;
		bool																			IsTrackingEnabled();

		void																			BeginSession();
		void																			EndSessionNowIfQueued();
		void																			EndSession(float delay = 0.f);
		void																			BeginRound();
		void																			StateRoundWinner(
																								int			inTeam);
		void																			EndRound();
		void																			RoundActuallyStarted();
		IStatsTracker															*GetSessionTracker()			{ return m_sessionTracker; }
		IStatsTracker															*GetRoundTracker()				{ return m_roundTracker; }

		IStatsTracker															*GetStatsTracker(
																								IActor		*inActor);
		void																			StartTrackingStats(
																								IActor		*inActor);
		void																			StopTrackingStats(
																								IActor		*inActor);
		void																			StopTrackingAllPlayerStats();
		void																			AddTeam(
																								int				inTeamId,
																								string		inTeamName);
		void																			OnTeamScore(int teamId, int score, EGameRulesScoreType pointsType);
		void																			OnMatchBonusXp(IActor* pActor, int amount);

		void																			InteractiveObjectEntry( EntityId entity, int type );
		void																			EnvironmentalWeaponEntry( EntityId entity, int type );

		void																			GetUniqueFilePath(
																								const char							*pInPrefix,
																								string									*pLocalPath,
																								string									*pRemotePath);
		void																			SaveSessionData(
																									ITelemetryProducer		*pInProducer);
		void																			SaveSessionData(XmlNodeRef node);

		bool																			ShouldRecordEvent(size_t eventID, IActor* pActor=NULL) const;
		void																			LoadEventConfig(const char* configName);

		void																			Serialize(TSerialize ser);
		void																			Update(float frameTime);

		void																			FlushData(void);

		// IGameStatisticsCallback
		virtual void OnEvent(const SNodeLocator& locator, size_t eventID, const CTimeValue& time, const SStatAnyValue& val) {}
		virtual void OnState(const SNodeLocator& locator, size_t stateID, const SStatAnyValue& val) {}
		virtual void PreprocessScriptedEventParameter(size_t eventID, SStatAnyValue& value) {}
		virtual void PreprocessScriptedStateParameter(size_t stateID, SStatAnyValue& value) {}
		virtual void OnNodeAdded(const SNodeLocator& locator);
		void RecordCurrentWeapon(CActor* pActor);
		virtual void OnNodeRemoved(const SNodeLocator& locator, IStatsTracker* tracker);

		// IHitListener
		virtual void OnHit(const HitInfo&);
		virtual void OnExplosion(const ExplosionInfo&) {}
		virtual void OnServerExplosion(const ExplosionInfo&) {}

		// IGameFrameworkListener
		virtual void OnPostUpdate(float fDeltaTime) {}
		virtual void OnSaveGame(ISaveGame* pSaveGame);
		virtual void OnLoadGame(ILoadGame* pLoadGame);
		virtual void OnLevelEnd(const char* pNextLevel) {}
		virtual void OnActionEvent(const SActionEvent& event);
		// ~IGameFrameworkListener

		// IDataListener
		virtual void DataDownloaded(CDownloadableResourcePtr inResource);
		virtual void DataFailedToDownload(CDownloadableResourcePtr inResource);
		// ~IDataListener

		// Helper function - try to track event.
		static inline void TryTrackEvent(IActor *pActor, size_t eventID, const SStatAnyValue &value = SStatAnyValue())
		{
			if(pActor)
			{
				CStatsRecordingMgr *pStatsRecordingMgr = g_pGame->GetStatsRecorder();

				if(pStatsRecordingMgr != NULL  && pStatsRecordingMgr->ShouldRecordEvent(eventID, pActor))
				{
					IStatsTracker	*pStatsTracker = pStatsRecordingMgr->GetStatsTracker(pActor);

					if(pStatsTracker)
					{
						pStatsTracker->Event(eventID, value);
					}
				}
			}
		}

#if !defined(_RELEASE)
		IEntityClass *m_pDummyEntityClass;
#endif
};


#endif // __STATSRECORDINGMGR_H__
