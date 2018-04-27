// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 25/05/2010		Created by Jim Bamford
*************************************************************************/

#ifndef __AFTERMATCHAWARDS_H__
#define __AFTERMATCHAWARDS_H__

#include "AutoEnum.h"
#include <CryCore/Containers/CryFixedArray.h>
#include "GameRules.h"

enum AwardsFlags
{
	eAF_LocalClients		= BIT(0),				//awards that are calculated by the clients for themselves
	eAF_Server					= BIT(1),				//awards that are calculated by the server
	eAF_StatHasFloatMinLimit = BIT(2),				//award has a stat limit set in the float data that needs to be satisfied to earn the award
	eAF_PlayerAliveTimeFloatMinLimit = BIT(3),		//award is only awarded if player was alive for longer than the time specified in the float data in seconds
};

#define AfterMatchAwards(g)																																										\
	g(EAMA_MostValuable, eAF_Server|eAF_StatHasFloatMinLimit, 2.0f)										/* Highest Kill death ratio	*/															\
	g(EAMA_MostMotivated, eAF_Server|eAF_StatHasFloatMinLimit, 100.0f)								/* Highest objective score */																\
	g(EAMA_MostSneaky, eAF_LocalClients|eAF_StatHasFloatMinLimit, 5.0f)								/* Most enemies shot in the back */													\
	g(EAMA_Mostcowardly, eAF_LocalClients|eAF_StatHasFloatMinLimit, 5.0f)							/* Get shot in the back most */															\
	g(EAMA_Ninja, eAF_LocalClients|eAF_StatHasFloatMinLimit, 20.0f)										/* Longest time cloaked */																	\
	g(EAMA_SpeedDemon, eAF_LocalClients|eAF_StatHasFloatMinLimit, 30.0f)							/* Most time power sprinting */															\
	g(EAMA_ArmourPlating, eAF_LocalClients|eAF_StatHasFloatMinLimit, 5.0f)						/* Most times shot while in armour mode */									\
	g(EAMA_AdvancedRecon, eAF_LocalClients|eAF_StatHasFloatMinLimit, 20.0f)						/* Most time spent in tactical mode */											\
	g(EAMA_Assassin, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)									/* Most stealth kills */																		\
	g(EAMA_Adaptable, eAF_LocalClients|eAF_StatHasFloatMinLimit, 5.0f)								/* Most suit mode changes */																\
	g(EAMA_Murderiser, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)								/* Most Melee kills */																			\
	g(EAMA_GlassJaw, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)									/* Be melee killed the most */															\
	g(EAMA_Untouchable, eAF_LocalClients|eAF_PlayerAliveTimeFloatMinLimit, 120.0f)		/* Fewest Deaths */																					\
	g(EAMA_Rampage, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)									/* Longest Kill Streak */																		\
	g(EAMA_DemolitionMan, eAF_LocalClients|eAF_StatHasFloatMinLimit, 2.0f)						/* Most kills using C4 */																		\
	g(EAMA_KeepYourHeadDown, eAF_LocalClients|eAF_StatHasFloatMinLimit, 30.0f)				/* Most time spent crouched */															\
	g(EAMA_LeapOfFaith, eAF_LocalClients|eAF_StatHasFloatMinLimit, 8.0f)							/* Highest fall without dying */														\
	g(EAMA_AidingRadar, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)							/* Most radars called in */																	\
	g(EAMA_IsItABird, eAF_LocalClients|eAF_StatHasFloatMinLimit, 20.0f)								/* Most time spent in the air - stat is distance not time */ \
	g(EAMA_Relentless, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)								/* Most kills of the same player - changed to vendetta?*/		\
	g(EAMA_Genocide, eAF_LocalClients|eAF_StatHasFloatMinLimit, 1.0f)									/* Kill the entire enemy team */														\
	g(EAMA_Exhibitionist, eAF_LocalClients|eAF_StatHasFloatMinLimit, 5.0f)						/* Highest number of skill kills */													\
	g(EAMA_Magpie, eAF_LocalClients|eAF_StatHasFloatMinLimit, 5.0f)										/* Most number of dog tags collected */											\
	g(EAMA_ClayPigeon, eAF_LocalClients|eAF_StatHasFloatMinLimit, 2.0f)								/* Most times killed in the air */													\
	g(EAMA_XRayVision, eAF_LocalClients|eAF_StatHasFloatMinLimit, 2.0f)								/* Most bullet penetration kills */													\
	g(EAMA_StandardBearer, eAF_Server|eAF_StatHasFloatMinLimit, 15.0f)								/* Most time carrying a flag */															\
	g(EAMA_DugIn, eAF_LocalClients|eAF_StatHasFloatMinLimit, 15.0f)										/* Hold a crash site for the longest time */								\
	g(EAMA_Technomancer, eAF_LocalClients|eAF_StatHasFloatMinLimit, 15.0f)						/* Download the most intel - measured by time*/							\
	g(EAMA_Bing, eAF_LocalClients|eAF_StatHasFloatMinLimit, 5.0f)											/* Most headshot kills */																		\
	g(EAMA_BangOn, eAF_LocalClients|eAF_StatHasFloatMinLimit, 0.01f)									/* Most accurate */																					\
	g(EAMA_BulletsCostMoney, eAF_LocalClients|eAF_PlayerAliveTimeFloatMinLimit, 120.0f)		/* Least shots fired */																	\
	g(EAMA_BattlefieldSurgery, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)				/* Most times health restored to full */										\
	g(EAMA_MostSelfish, eAF_Server|eAF_StatHasFloatMinLimit, 3.0f)										/* Most kills without assists earned by teammates */				\
	g(EAMA_ToolsOfTheTrade, eAF_LocalClients|eAF_StatHasFloatMinLimit, 4.0f)					/* At least 3 kills with 4 different weapons */							\
	g(EAMA_SprayAndPray, eAF_LocalClients|eAF_StatHasFloatMinLimit, 50.0f)						/* Most shots fired */																			\
	g(EAMA_BigBanger, eAF_LocalClients|eAF_StatHasFloatMinLimit, 2.0f)								/* Most killls with a single explosive */										\
	g(EAMA_Robbed, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)										/* Most kill assists */																			\
	g(EAMA_MoneyShot, eAF_LocalClients|eAF_StatHasFloatMinLimit, 1.0f)								/* Get the winning kill */											            \
	g(EAMA_SingleMinded, eAF_LocalClients|eAF_StatHasFloatMinLimit, 1.0f)							/* Most kills */																						\
	g(EAMA_DontPanic, eAF_LocalClients|eAF_StatHasFloatMinLimit, 7.0f)								/* Most friendlies shot (number of hits) */									\
	g(EAMA_LoneWolf, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)									/* Most kills with no friendly player within 15m */					\
	g(EAMA_NeverSayDie, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)							/* Longest deathstreak */																		\
	g(EAMA_EnergeticBunny, eAF_LocalClients|eAF_StatHasFloatMinLimit, 100.0f)					/* Most energy used up */																		\
	g(EAMA_Warbird, eAF_LocalClients|eAF_StatHasFloatMinLimit, 2.0f)									/* Most kills within 5 seconds of decloaking */							\
	g(EAMA_Impregnable, eAF_LocalClients|eAF_StatHasFloatMinLimit, 4.0f)							/* Most fights won in armour mode */												\
	g(EAMA_TargetLocked, eAF_LocalClients|eAF_StatHasFloatMinLimit, 2.0f) 						/* Most enemies spotted */																	\
	g(EAMA_SafetyInNumbers, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)					/* Most kills with a friendly within 15m */									\
	g(EAMA_Spanked, eAF_LocalClients|eAF_PlayerAliveTimeFloatMinLimit, 120.0f)				/* no Kills */																							\
	g(EAMA_Pardon, eAF_LocalClients|eAF_StatHasFloatMinLimit, 1.0f)										/* Most grenades survived */																\
	/* Break between social1 and social2 */ \
	g(EAMA_DirtyDozen, eAF_LocalClients, 0.0f)																				/* 12 kills without dying	*/																\
	g(EAMA_Codpiece, eAF_LocalClients|eAF_StatHasFloatMinLimit, 1.0f)									/* Score the most groin hits */															\
	g(EAMA_MountieScore, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)							/* the most kills with mounted weapons */										\
	g(EAMA_RipOff, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)										/* Score the most kills with ripped off mounted weapons */	\
	g(EAMA_ProTips, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)									/* Most cloaked reloads */																	\
	g(EAMA_WrongPlaceWrongTime, eAF_LocalClients|eAF_StatHasFloatMinLimit, 1.0f)			/* be the victim of the winning kill */											\
	g(EAMA_Observer, eAF_LocalClients|eAF_StatHasFloatMinLimit, 10.0f)								/* Most time watching the killcam */												\
	g(EAMA_Invincible, eAF_LocalClients|eAF_PlayerAliveTimeFloatMinLimit, 120.0f)			/* No deaths */																							\
	g(EAMA_Untouchable2, eAF_LocalClients|eAF_PlayerAliveTimeFloatMinLimit, 120.0f)		/* Least damage received - called Good Condition now*/			\
	g(EAMA_Punisher, eAF_LocalClients|eAF_StatHasFloatMinLimit, 200.0f)								/* Most damage dealt */																			\
	g(EAMA_BulletMagnet, eAF_LocalClients|eAF_StatHasFloatMinLimit, 200.0f)						/* Most damage received */																	\
	g(EAMA_Pacifist, eAF_LocalClients|eAF_PlayerAliveTimeFloatMinLimit, 120.0f)				/* Least damage dealt */																		\
	g(EAMA_Icy, eAF_LocalClients|eAF_StatHasFloatMinLimit, 15.0f)											/* Most distance covered while sliding */										\
	g(EAMA_Boing, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)										/* Most stamp kills */																			\
	g(EAMA_NYF, eAF_LocalClients|eAF_StatHasFloatMinLimit, 10.0f)										/* Receive feed items from x different friends */						\
	g(EAMA_MaxSuit, eAF_LocalClients|eAF_StatHasFloatMinLimit, 1.0f)				/* Obtain the maximum suit support bonus */																		\
	g(EAMA_OrbitalStrike, eAF_LocalClients|eAF_StatHasFloatMinLimit, 2.0f)						/* Most Orbital Strike kills */										\
	g(EAMA_EMP, eAF_LocalClients|eAF_StatHasFloatMinLimit, 2.0f)											/* Most EMP's called in */																			\
	g(EAMA_WallOfSteel, eAF_LocalClients|eAF_StatHasFloatMinLimit, 50.0f)							/* Most shots absorbed with a shield */											\
	g(EAMA_PoleDancer, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)								/* Most kills with a pole */																\
	g(EAMA_HighFlyer, eAF_LocalClients|eAF_StatHasFloatMinLimit, 15.0f)								/* Most time spent in a VTOL */															\
	g(EAMA_DeathFromAbove, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)						/* Most kills with mounted VTOL HMGs */											\
	g(EAMA_TripleA, eAF_LocalClients|eAF_StatHasFloatMinLimit, 500.0f)								/* Most damage to airborne/flying vehicles */								\
	g(EAMA_Dolphin, eAF_LocalClients|eAF_StatHasFloatMinLimit, 15.0f)									/* Most time spent in water */															\
	g(EAMA_LowFlyingObject, eAF_LocalClients|eAF_StatHasFloatMinLimit, 3.0f)					/* Most kills with thrown items */													\
	g(EAMA_HideAndSeek, eAF_LocalClients, 0.0f)																				/* Survive all five rounds of hunter as a marine */					\
	g(EAMA_SoloSpear, eAF_Server, 0.0f)																								/* Completely capture a spear by yourself */								\
	g(EAMA_TheHunter, eAF_LocalClients, 0.0f)																					/* Kill all the Marines in a round of hunter (by yourself?) */	\


AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EAfterMatchAwards, AfterMatchAwards, EAMA_Invalid, EAMA_Max);

struct IFlashPlayer;
class CPersistantStats;

class CAfterMatchAwards 
{
public:

	CAfterMatchAwards();
	virtual ~CAfterMatchAwards();

	void Init();
	void Update(const float dt);
	void EnteredGame();
	void StartCalculatingAwards();

	int GetNumClientAwards();
	EAfterMatchAwards GetNthClientAward(const int Nth);

	void ClientReceivedAwards(int numAwards, const uint8 awardsArray[]);
	void ServerReceivedAwardsWorkingFromPlayer(EntityId playerEntityId, int numWorkingValues, const CGameRules::SAfterMatchAwardWorkingsParams::SWorkingValue workingValues[]);

	const char *GetNameForAward(EAfterMatchAwards inAward);
	
	void Clear();

protected:

	static const int kMaxLocalAwardsGiven = EAMA_Max;
	static const int kMaxLocalAwardsActuallyWon = 3;
	
	struct SAwardsForPlayer
	{
		union UFloatInt
		{
			float m_float;
			int m_int;		
		};

		struct SWorkingEle
		{
			UFloatInt m_data;
			bool m_calculated;
		};

		CryFixedArray<EAfterMatchAwards, CAfterMatchAwards::kMaxLocalAwardsGiven> m_awards;	// stores awards actually awarded
		SWorkingEle m_working[EAMA_Max];									// stores ongoing counts for each award

		EntityId m_playerEntityId;
		bool m_awardsReceivedFromClient;								// server only
		
		SAwardsForPlayer(EntityId inPlayerId);
		void Clear();
	};

	enum EState
	{
		// both clients and servers
		k_state_waiting_for_game_to_end=0,
		k_state_game_ended_have_awards,

		// clients
		k_state_client_game_ended_waiting_for_awards,
		
		// servers
		k_state_server_game_ended_waiting_for_all_client_results,
	};

	EState m_state;

	typedef std::vector<EntityId> ActorsVector;

	//Sessions are stored for every player for the current session
	typedef std::map<EntityId, SAwardsForPlayer, std::less<EntityId>, stl::STLGlobalAllocator<std::pair<const EntityId, SAwardsForPlayer> > > ActorAwardsMap;
	ActorAwardsMap m_actorAwards;

	CPersistantStats *m_persistantStats;
	IEntityClass* m_playerClass;

	EntityId m_localClientEntityIdWas;		// this class will be used when there are no local clients so cache so we are able to get the relevant awards out to the frontend
	static const float k_timeOutWaitingForClients;
	float m_timeOutLeftWaitingForClients;

	void CalculateAwards();
	void ClSendAwardsToServer();
	void SvSendAwardsToClients();
	SAwardsForPlayer *GetAwardsForActor(EntityId actorId);
	void CalculateAwardForActor(EAfterMatchAwards inAward, EntityId inActorId);
	EntityId GetMaximumFloatFromWorking(EAfterMatchAwards inAward, float *outMaxFloat);
	EntityId GetMinimumFloatFromWorking(EAfterMatchAwards inAward);
	void GetAllFromWorkingWithFloat(EAfterMatchAwards inAward, float inVal, ActorsVector &results);
	void GetAllFromWorkingGreaterThanFloat(EAfterMatchAwards inAward, float inVal, ActorsVector &results);
	void ClCalculateAward(EAfterMatchAwards inAward);
	int SvCalculateAward(EAfterMatchAwards inAward);
	int CalculateAward(EAfterMatchAwards inAward);

	void FilterWinningAwards();
	void HaveGotAwards();
	void DebugMe(bool useCryLog=false);

	int GetFlagsForAward(EAfterMatchAwards inAward);
	float GetFloatDataForAward(EAfterMatchAwards inAward);

	const char *GetEntityName(EntityId inEntityId) const;

	bool IsAwardProhibited(EAfterMatchAwards inAward) const; 
};

#endif //  __AFTERMATCHAWARDS_H__
