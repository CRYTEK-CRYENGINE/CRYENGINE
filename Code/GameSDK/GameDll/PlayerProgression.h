// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 07:12:2009		Created by Ben Parbury
*************************************************************************/

#ifndef __PLAYERPROGRESSION_H__
#define __PLAYERPROGRESSION_H__


#include "AutoEnum.h"
#include "GameRulesTypes.h"
#include "PlayerProgressionTypes.h"
#include "GameRulesModules/IGameRulesClientScoreListener.h"
#include <CryCore/Containers/CryFixedArray.h>
#include "ProgressionUnlocks.h"
#include <IPlayerProfiles.h>
#include "Network/Lobby/GameLobbyManager.h"
#include "Audio/AudioSignalPlayer.h"

class CPlayer;
struct IFlashPlayer;

#if !defined(_RELEASE) && !CRY_PLATFORM_ORBIS
	#define DEBUG_XP_ALLOCATION 1
#else
	#define DEBUG_XP_ALLOCATION 0
#endif

#define MAX_EXPECTED_UNLOCKS_PER_RANK 5

enum EPPData
{
	EPP_Rank = 0,
	EPP_MaxRank,
	EPP_DisplayRank,
	EPP_MaxDisplayRank,
	EPP_XP,
	EPP_LifetimeXP,
	EPP_XPToNextRank,
	EPP_XPLastMatch,
	EPP_XPMatchStart,
	EPP_MatchStartRank,
	EPP_MatchStartDisplayRank,
	EPP_MatchStartXPInCurrentRank,
	EPP_MatchStartXPToNextRank,
	EPP_MatchBonus,
	EPP_MatchScore,
	EPP_MatchXP,
	EPP_XPInCurrentRank,
	EPP_NextRank,
	EPP_NextDisplayRank,
	EPP_Reincarnate,
	EPP_CanReincarnate,
	EPP_SkillRank,
	EPP_SkillKillXP
};

enum ECompletionStartValues
{
	eCSV_Weapons = 0,
	eCSV_Loadouts,
	eCSV_Attachments,
	eCSV_GameTypes,
	eCSV_Assessments,
	eCSV_NUM
};

struct IPlayerProgressionEventListener
{
	virtual	~IPlayerProgressionEventListener(){}
	virtual void OnEvent(EPPType type, bool skillKill, void *data) = 0;
};

class CPlayerProgression : public SGameRulesListener, public IGameRulesClientScoreListener, public IPlayerProfileListener,  public IPrivateGameListener
{
public:
	
	const static int k_PresaleCount = 4;

public:

	CPlayerProgression();
	virtual ~CPlayerProgression();

	static CPlayerProgression* GetInstance();

	void Init();
	void PostInit();

	void ResetUnlocks();
	void OnUserChanged();

	void Event(EPPType type, bool skillKill = false, void *data = NULL);
	void ClientScoreEvent(EGameRulesScoreType type, int points, EXPReason inReason, int currentTeamScore);

	bool SkillKillEvent(CGameRules* pGameRules, IActor* pTargetActor, IActor* pShooterActor, const HitInfo &hitInfo, bool firstBlood);
	void SkillAssistEvent(CGameRules *pGameRules, IActor* pTargetActor, IActor* pShooterActor, const HitInfo &hitInfo);
	int SkillAssessmentEvent(int points /*, EXPReason inReason*/);
	
	void Update(CPlayer* pPlayer, float deltaTime, float fHealth);
#ifndef _RELEASE
	void UpdateDebug();
#endif

	//SGameRulesListener
	virtual void EnteredGame();
	virtual void GameOver(EGameOverType localWinner, bool isClientSpectator);

	//IPlayerProfileListener
	virtual void SaveToProfile(IPlayerProfile* pProfile, bool online, unsigned int reason);
	virtual void LoadFromProfile(IPlayerProfile* pProfile, bool online, unsigned int reason);

	//IPrivateGameListener
	virtual void SetPrivateGame(const bool privateGame);

	const int GetData(EPPData dataType);
	int GetMatchBonus(EGameOverType localWinner, float fracTimePlayed) const;

	float GetProgressWithinRank(const int forRank, const int forXp);
	const int GetMaxRanks() const { return k_maxPossibleRanks; }
	static const int GetMaxDisplayableRank() { return k_maxDisplayableRank; }
	const int GetMaxReincarnations() const { return k_maxReincarnation; }


	const int GetXPForRank(const uint8 rank) const;
	uint8 GetRankForXP(const int xp) const;
	const char* GetRankName(uint8 rank, bool localize=true);
	const char* GetRankNameUpper(uint8 rank, bool localize=true);
  bool HaveUnlocked(EUnlockType type, const char* name, SPPHaveUnlockedQuery &results) const;

	//only checks rank and custom class unlocks (only needed for weapons and modules atm)
	void GetUnlockedCount( EUnlockType type, uint32* outUnlocked, uint32* outTotal ) const;

#if !defined(_RELEASE)
	static void CmdGainXP(IConsoleCmdArgs* pCmdArgs);
	static void CmdGameEnd(IConsoleCmdArgs* pCmdArgs);
	static void CmdUnlockAll(IConsoleCmdArgs* pCmdArgs);
	static void CmdUnlocksNow(IConsoleCmdArgs* pCmdArgs);
	static void CmdResetXP(IConsoleCmdArgs* pCmdArgs);
	static void CmdGainLevels( IConsoleCmdArgs* pCmdArgs );

	void DebugFakeSetProgressions(int8 fakeRank, int8 fakeDefault, int8 fakeStealth, int8 fakeArmour);
	static void CmdFakePlayerProgression(IConsoleCmdArgs *pArgs);
	static void CmdGenerateProfile(IConsoleCmdArgs *pArgs);
#endif

	void AddEventListener(IPlayerProgressionEventListener* pEventListener);
	void RemoveEventListener(IPlayerProgressionEventListener* pEventListener);

	static const char* GetEPPTypeName(EPPType type);
	static void UpdateLocalUserData();

	void AddUINewDisplayFlags(uint32 flags) { m_uiNewDisplayFlags |= flags; }
	void RemoveUINewDisplayFlags(uint32 flags) { m_uiNewDisplayFlags &= ~flags; }
	uint32 CheckUINewDisplayFlags(uint32 flags) const { return ((m_uiNewDisplayFlags&flags)!=0); }
	uint32 GetUINewDisplayFlags() const { return m_uiNewDisplayFlags; }

	void Reincarnate();

	void UnlockCustomClassUnlocks(bool rewards = true);

	// Unlocks given unlocks. Returns given XP (doesn't apply XP itself)
	int UnlockPresale( uint32 id, bool showPopup, bool isNew );

	int IncrementXPPresale(int ammount, EXPReason inReason);
	int GetXPForEvent(EPPType type) const;
	

	void OnQuit();
	void OnEndSession();
	void OnFinishMPLoading();

	static void ReserveUnlocks(XmlNodeRef xmlNode, TUnlockElements& unlocks, TUnlockElements& allowUnlocks);
	static void LoadUnlocks(XmlNodeRef xmlNode, const int rank, TUnlockElements& unlocks, TUnlockElements& allowUnlocks);

	int GetUnlockedTokenCount(EUnlockType type, const char* paramName);

	void GetRankUnlocks(int firstrank, int lastrank, int reincarnations, TUnlockElementsVector &unlocks) const;

	void HaveUnlockedByRank(EUnlockType type, const char* name, SPPHaveUnlockedQuery &results, const int rank, const int  reincarnations, const bool bResetResults) const;

	bool AllowedWriteStats() const;

	bool WillReincarnate () const;

protected:
	void Reset();
	void InitRanks(const char* filename);
	void InitEvents(const char* filename);
	void InitCustomClassUnlocks(const char* filename);
	void InitConsoleCommands();

	void InitPresaleUnlocks();
	void InitPresaleUnlocks( const char* pScriptName );
	int UnlockPresale( const char* filename, bool showPopup, bool isNew );

	void SanityCheckRanks();
	void SanityCheckSuitLevels();

	int CalculateRankFromXp(int xp);

	int EventInternal(int points, EXPReason inReason, bool bSkillKill=false);

	int IncrementXP(int amount, EXPReason inReason);

	void MatchBonus(const EGameOverType localWinner, const float totalTime);
	float WinModifier(const EGameOverType localWinner) const;

	void ReincarnateUnlocks();

	void UpdateAllUnlocks(bool fromProgress);	//from Progress as opposed to from Load
	
	void ResetXP();
	void UpdateStartRank();

	void SendEventToListeners(EPPType, bool, void*);

	const bool IsSkillAssist(const EPPType type) const;

	static int GetUnlockedTokenCount(const TUnlockElements& unlockList, EUnlockType type);

	typedef std::vector<IPlayerProgressionEventListener*> TEventListener;
	TEventListener m_eventListeners;

	struct SRank
	{
		SRank(XmlNodeRef node);

		const static int k_maxNameLength = 16;
		char m_name[k_maxNameLength];
		int m_xpRequired;
	};

  struct SUnlockedItem
  {
    SUnlockedItem(EUnlockType type = eUT_Invalid, const char * name = NULL, int rank = -1)
    {
      m_type = type;
      m_name = name;
      m_rank = rank;
    }

    EUnlockType m_type;
    const char * m_name;
    int m_rank;
  };

	static CPlayerProgression* s_playerProgression_instance;
	const static float k_queuedRankTime;
	enum { k_maxPossibleRanks=51,
	       k_maxDisplayableRank=50,
	       k_risingStarRank=20 }; //Unlock achievement when reaching this rank
	typedef CryFixedArray<SRank,k_maxPossibleRanks > TRankElements;
	TRankElements m_ranks;
	int m_maxRank;

	const static int s_nPresaleScripts = 1;
	static const char* s_presaleScriptNames[ s_nPresaleScripts ];

	const static int k_maxEvents = EPP_Max;
	int m_events[k_maxEvents];

	TUnlockElements m_unlocks;				//unlocks in the ranks.xml
	TUnlockElements m_customClassUnlocks;	//unlocks for the custom class (customclass.xml)
	TUnlockElements m_presaleUnlocks;	//unlocks from presale

	TUnlockElements m_allowUnlocks; 	//Determine when you're allowed to unlock things

	float m_time;

	CAudioSignalPlayer m_rankUpSignal;
	int m_xp;
	int m_lifetimeXp;
	int m_matchScore;
	int m_gameStartXp;
	int m_rank;
	int m_gameStartRank;
	int m_matchBonus;
	const static int k_maxReincarnation = 5;
	int m_reincarnation;
	int m_initialLoad;
	int m_skillRank;
	int m_skillKillXP;

	uint32	m_uiNewDisplayFlags;	// Flags for which buttons wants want to display a 'new' icon.

	int m_queuedRankForHUD;
	int m_queuedXPRequiredForHUD;
	float m_rankUpQueuedTimer;
	bool m_rankUpQueuedForHUD;

	bool m_hasNewStats;
	bool m_privateGame;

#if DEBUG_XP_ALLOCATION
	typedef std::vector<string> TDebugXPVector;
	TDebugXPVector m_debugXp;
#endif
};

#endif // __PLAYERPROGRESSION_H__
