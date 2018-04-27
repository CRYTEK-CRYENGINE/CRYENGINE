// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 07:12:2009		Created by Ben Parbury
*************************************************************************/
 
#include "StdAfx.h"
#include "PlayerProgression.h"
#include "PersistantStats.h"

#include "SkillKill.h"

#include "Player.h"

#include "GameRules.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"

#include <IPlayerProfiles.h>
#include <CrySystem/Scaleform/IFlashPlayer.h>

#include "UI/ProfileOptions.h"

#include "UI/UIManager.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/UICVars.h"
#include "UI/WarningsManager.h"
#include "UI/HUD/HUDUtils.h"

#include "Utility/StringUtils.h"
#include "Utility/CryWatch.h"
#include "Utility/DesignerWarning.h"
#include "Utility/CryDebugLog.h"
#include <CryString/StringUtils.h>

#include "Audio/AudioSignalPlayer.h"

#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Network/Lobby/GameAchievements.h"
#include "Network/Lobby/GameLobbyData.h"
#include "Network/Lobby/GameLobby.h"
#include "Network/Squad/SquadManager.h"

#include "PlaylistManager.h"
#include "EquipmentLoadout.h"

#include <CryCore/TypeInfo_impl.h>

#include "RecordingSystem.h"
#include "DLCManager.h"


CPlayerProgression* CPlayerProgression::s_playerProgression_instance = NULL;
const float CPlayerProgression::k_queuedRankTime = 1.0f;


static AUTOENUM_BUILDNAMEARRAY(s_eventName, PlayerProgressionType);
static int pp_debug = 0;
static int pp_xpDebug = 0;
static int pp_defaultUnlockAll = 0;
static float pp_suitmodeAveraging = 0.0f;


#if DEBUG_XP_ALLOCATION
	#define DEBUG_XP(...) \
	if(pp_xpDebug) \
	{ \
		m_debugXp.push_back(string().Format(__VA_ARGS__)); \
        CryLog((string().Format("[XP]" __VA_ARGS__)).c_str()); \
	}

	static AUTOENUM_BUILDNAMEARRAY(s_scoreName, EGRSTList);
#else
	#define DEBUG_XP(...) (void)0
#endif

CPlayerProgression::CPlayerProgression()
{
	CryLog( "CPlayerProgression::Constructor" );

	CRY_ASSERT(s_playerProgression_instance == NULL);
	s_playerProgression_instance = this;

	Reset();
}


CPlayerProgression::~CPlayerProgression()
{
	if(gEnv->pConsole)
	{
		gEnv->pConsole->UnregisterVariable("pp_debug");
		gEnv->pConsole->UnregisterVariable("pp_xpDebug");
		gEnv->pConsole->UnregisterVariable("pp_defaultUnlockAll");
		gEnv->pConsole->UnregisterVariable("pp_suitmodeAveraging");
	}

	if(IPlayerProfileManager *pProfileMan = gEnv->pGameFramework->GetIPlayerProfileManager())
	{
		pProfileMan->RemoveListener(this);
	}

	CGameLobbyManager *pGameLobbyManager = g_pGame->GetGameLobbyManager();
	if(pGameLobbyManager)
	{
		pGameLobbyManager->RemovePrivateGameListener(this);
	}

	CRY_ASSERT(s_playerProgression_instance == this);
	s_playerProgression_instance = NULL;
}

//static
CPlayerProgression* CPlayerProgression::GetInstance()
{
	CRY_ASSERT(s_playerProgression_instance);
	return s_playerProgression_instance;	
}

void CPlayerProgression::Reset()
{
	m_ranks.clear();
  m_unlocks.clear();
	m_allowUnlocks.clear();
	m_customClassUnlocks.clear();
	m_presaleUnlocks.clear();
	
	memset(m_events, 0, sizeof(m_events));

	m_time = 0.0f;
	m_rankUpSignal.SetSignal("RankUp");
	m_xp = 0;
	m_lifetimeXp = 0;
	m_matchScore = 0;
	m_gameStartXp = 0;
	m_rank = 0;
	m_gameStartRank = 0;
	m_matchBonus = 0;
	m_maxRank = 0;
	m_reincarnation = 0;
	m_initialLoad = 1;
	m_skillRank = 1500;
	m_uiNewDisplayFlags = eMBF_None;
	m_hasNewStats = false;
	m_privateGame = false;
	
	m_queuedRankForHUD = 0;
	m_queuedXPRequiredForHUD = 0;
	m_rankUpQueuedTimer = 0.f;
	m_rankUpQueuedForHUD = false;

	m_skillKillXP = 0;

	Init();
}

void CPlayerProgression::Init()
{
	string prefix = "";
	if(g_pGameCVars->g_EPD == 1)
	{
		prefix = "EPD";
	}
	else if(g_pGameCVars->g_EPD == 2)
	{
		prefix = "MPReveal";
	}
	else if(g_pGameCVars->g_EPD == 3)
	{
		prefix = "Demo";
	}

	string progressionFileName;
	progressionFileName.Format("Scripts/Progression/%sRanks.xml", prefix.c_str());

	InitRanks(progressionFileName.c_str());
	
	InitEvents("Scripts/Progression/Events.xml");

	InitCustomClassUnlocks("Scripts/Progression/CustomClass.xml");

	InitPresaleUnlocks();

	InitConsoleCommands();

	CryLog( "CPlayerProgression::Init() Done" );
}

//Post init is so all sub systems exist so you can UpdateUnlocks correctly
void CPlayerProgression::PostInit()
{
	IPlayerProfileManager *pProfileMan = gEnv->pGameFramework->GetIPlayerProfileManager();
	CRY_ASSERT(pProfileMan);
	if(pProfileMan)
	{
		pProfileMan->RemoveListener(this);
		pProfileMan->AddListener(this, true);
	}

	if (!gEnv->IsEditor())
	{
		CGameLobbyManager *pGameLobbyManager = g_pGame->GetGameLobbyManager();
		CRY_ASSERT(pGameLobbyManager);
		if(pGameLobbyManager)
		{
			pGameLobbyManager->AddPrivateGameListener(this);
		}
	}
}

void CPlayerProgression::ResetUnlocks()
{
	//reset unlocks so it is like we are clean

	TUnlockElements::iterator current;
	TUnlockElements::iterator end;

	current = m_unlocks.begin();
	end = m_unlocks.end();

	for( ; current < end; ++current )
	{
		current->m_unlocked = false;
	}


	current = m_customClassUnlocks.begin();
	end = m_customClassUnlocks.end();

	for( ; current < end; ++current )
	{
		current->m_unlocked = false;
	}


	current = m_presaleUnlocks.begin();
	end = m_presaleUnlocks.end();

	for( ; current < end; ++current )
	{
		current->m_unlocked = false;
	}


	current = m_allowUnlocks.begin();
	end = m_allowUnlocks.end();

	for( ; current < end; ++current )
	{
		current->m_unlocked = false;
	}

}

void CPlayerProgression::OnUserChanged()
{
	m_queuedRankForHUD = 0;
	m_queuedXPRequiredForHUD = 0;
	m_rankUpQueuedTimer = 0.f;
	m_rankUpQueuedForHUD = false;
}

void CPlayerProgression::SaveToProfile(IPlayerProfile* pProfile, bool online, unsigned int reason)
{
	if(reason & ePR_Game)
	{
	  if(gEnv->bMultiplayer)
	  {
#if USE_STEAM
				// Steam has its own persistant stats online
				return;
#endif
	  }
		else
		{
			pProfile->SetAttribute("SP/Difficulty", g_pGameCVars->g_difficultyLevel);
		}
	}
}

void CPlayerProgression::LoadFromProfile(IPlayerProfile* pProfile, bool online, unsigned int reason)
{
	if(reason & ePR_Game)
	{
		if(gEnv->bMultiplayer)
		{
#if USE_STEAM
			// Steam has its own persistant stats online
			return;
#endif
		}
		else
		{
			int	difficultyLevel = eDifficulty_Default;
			if(pProfile->GetAttribute("SP/Difficulty", difficultyLevel))
			{
				g_pGame->SetDifficultyLevel(static_cast<EDifficulty>(difficultyLevel));
			}
		}
	}
}

int CPlayerProgression::CalculateRankFromXp(int xp)
{
	CRY_ASSERT(m_maxRank >= k_risingStarRank);
	if(xp >= m_ranks[k_risingStarRank - 1].m_xpRequired)
	{
		g_pGame->GetGameAchievements()->GiveAchievement(eC3A_MP_Rising_Star);
	}

	for(int i = 0; i < m_maxRank - 1; i++)
	{
		if(xp >= m_ranks[i].m_xpRequired && xp < m_ranks[i + 1].m_xpRequired)
		{
			return i;
		}
	}

	if(xp >= m_ranks[m_maxRank - 1].m_xpRequired)
	{
		return m_maxRank - 1;
	}

	CRY_ASSERT_MESSAGE(false, "Failed to CalculateRankFromXp");
	return 0;
}

void CPlayerProgression::InitRanks(const char* filename)
{
	XmlNodeRef xml = GetISystem()->LoadXmlFromFile( filename );

	if(xml)
	{
		const int childCount = xml->getChildCount();

		DesignerWarning(childCount <= k_maxPossibleRanks, "There should be at maximum '%d' ranks, not '%d'", k_maxPossibleRanks, childCount);
		m_maxRank = std::min((int)childCount, (int)k_maxPossibleRanks);

		ReserveUnlocks(xml, m_unlocks, m_allowUnlocks);

		for (int iRank = 0; iRank < childCount; ++iRank)
		{
			XmlNodeRef childXML = xml->getChild(iRank);

			if (childXML)
			{
				SRank rank(childXML);
				m_ranks.push_back(rank);

				LoadUnlocks(childXML, iRank, m_unlocks, m_allowUnlocks);
			}
		}

		SanityCheckRanks();
	}
}

//static
void CPlayerProgression::ReserveUnlocks( XmlNodeRef xmlNode, TUnlockElements& unlocks, TUnlockElements& allowUnlocks )
{
	int unlockReserveCount = 0;
	int allowReserveCount = 0;

	const int childCount = xmlNode->getChildCount();
	for (int iRank = 0; iRank < childCount; ++iRank)
	{
		XmlNodeRef childXML = xmlNode->getChild(iRank);

		const int unlockCount = childXML->getChildCount();
		for (int iUnlock = 0; iUnlock < unlockCount; ++iUnlock)
		{
			XmlNodeRef unlockNode = childXML->getChild(iUnlock);

			const int allowCount = unlockNode->getChildCount();
			if(allowCount)
			{
				allowReserveCount += allowCount;
			}
			else
			{
				unlockReserveCount++;
			}
		}
	}

	unlocks.reserve(unlockReserveCount);
	allowUnlocks.reserve(allowReserveCount);
}

//static
void CPlayerProgression::LoadUnlocks(XmlNodeRef xmlNode, const int rank, TUnlockElements& unlocks, TUnlockElements& allowUnlocks)
{
	const int unlockCount = xmlNode->getChildCount();
	for (int iUnlock = 0; iUnlock < unlockCount; ++iUnlock)
	{
		XmlNodeRef unlockNode = xmlNode->getChild(iUnlock);

		const int allowCount = unlockNode->getChildCount();
		if(allowCount)
		{
			DesignerWarning( strcmpi(unlockNode->getTag(), "allow") == 0, "expected allow tag at line %d", unlockNode->getLine() );

			for (int iAllow = 0; iAllow < allowCount; ++iAllow)
			{
				XmlNodeRef allowNode = unlockNode->getChild(iAllow);
				DesignerWarning( strcmpi(allowNode->getTag(), "allow") == 0, "expected allow tag at line %d", unlockNode->getLine() );
				CRY_ASSERT(allowUnlocks.size() + 1 <= allowUnlocks.capacity());	//should have been reserved already
				SUnlock unlock(allowNode, rank);
				allowUnlocks.push_back(unlock);
			}
		}
		else
		{
			DesignerWarning( strcmpi(unlockNode->getTag(), "unlock") == 0, "expected unlock tag at line %d", unlockNode->getLine() );
			CRY_ASSERT(unlocks.size() + 1 <= unlocks.capacity());	//should have been reserved already
			SUnlock unlock(unlockNode, rank);
			unlocks.push_back(unlock);
		}
	}
}

void CPlayerProgression::InitCustomClassUnlocks(const char* filename)
{
	XmlNodeRef xml = GetISystem()->LoadXmlFromFile( filename );

	if(xml)
	{
		const int unlockCount = xml->getChildCount();

		m_customClassUnlocks.reserve(unlockCount);

		for (int iUnlock = 0; iUnlock < unlockCount; ++iUnlock)
		{
			XmlNodeRef unlockNode = xml->getChild(iUnlock);

			SUnlock unlock(unlockNode, 0);
			m_customClassUnlocks.push_back(unlock);
		}
	}
}

void CPlayerProgression::UnlockCustomClassUnlocks(bool reward)
{
	const int unlockSize = m_customClassUnlocks.size();
	for(int i = 0; i < unlockSize; i++)
	{
		m_customClassUnlocks[i].Unlocked(reward);
	}
}


const char* CPlayerProgression::s_presaleScriptNames[] = 
{
	"Scripts/Progression/Presale/Presale1.xml",
};

void CPlayerProgression::InitPresaleUnlocks()
{
	for( int iPresale = 0; iPresale < s_nPresaleScripts; iPresale++ )
	{
		InitPresaleUnlocks( s_presaleScriptNames[ iPresale ] );
	}
}

void CPlayerProgression::InitPresaleUnlocks( const char* pScriptName )
{
	//add all the unlocks to the presale unlocks vector (currently locked)
	//so if we ask for something that is presale content which is not unlocked by any other means
	//and we don't have the entitlement to, HaveUnlocked will return exists true and unlocked false.

	int returnXP = 0;
	XmlNodeRef xml = GetISystem()->LoadXmlFromFile( pScriptName );

	if(xml)
	{
		int index = 0;
		xml->getAttr("index", index);

		XmlNodeRef unlocksXml = xml->findChild("Unlocks");
		if(unlocksXml)
		{
			const int unlockCount = unlocksXml->getChildCount();

			m_presaleUnlocks.reserve(unlockCount);

			for (int iUnlock = 0; iUnlock < unlockCount; ++iUnlock)
			{
				XmlNodeRef unlockNode = unlocksXml->getChild(iUnlock);

				SUnlock unlock(unlockNode, 0);
				stl::push_back_unique(m_presaleUnlocks, unlock);
			}
		}
	}
}

int CPlayerProgression::UnlockPresale( uint32 id, bool showPopup, bool isNew )
{
	int returnXp = 0;


	if( id > 0 )
	{
		int index = id - 1;

		if( index < s_nPresaleScripts )
		{
			returnXp = UnlockPresale( s_presaleScriptNames[ index ], showPopup, isNew );
		}
		else
		{
			CryLog( "Tried to unlock invalid pre-sale item. value was %d, valid range is 1-%d", id, s_nPresaleScripts );
		}
	}
	
	return returnXp;
}

int CPlayerProgression::UnlockPresale( const char* filename, bool showPopup, bool isNew )
{
	int returnXP = 0;
	XmlNodeRef xml = GetISystem()->LoadXmlFromFile( filename );

	if(xml)
	{
		int index = 0;
		xml->getAttr("index", index);

		XmlNodeRef unlocksXml = xml->findChild("Unlocks");
		if(unlocksXml)
		{
			const int unlockCount = unlocksXml->getChildCount();

			//m_presaleUnlocks.reserve(unlockCount);

			for (int iUnlock = 0; iUnlock < unlockCount; ++iUnlock)
			{
				XmlNodeRef unlockNode = unlocksXml->getChild(iUnlock);

				SUnlock newUnlock(unlockNode, 0);

				TUnlockElements::iterator foundUnlock = std::find(m_presaleUnlocks.begin(), m_presaleUnlocks.end(), newUnlock );

				if( foundUnlock != m_presaleUnlocks.end() )
				{
					foundUnlock->Unlocked( isNew );
				}
				else
				{
					//quite unexpected error
					CryLog( "Presale Error: Missing unlock %s from initial load", newUnlock.m_name );
				}			
			}
		}

		if( showPopup && isNew )
		{
			//UI message when first loaded
			const char* presaleName;
			xml->getAttr("uiMsg", &presaleName);
			if(presaleName)
			{
				g_pGame->GetWarnings()->AddGameWarning( presaleName, NULL );
			}
		}

		//Can potentially give you bonus XP
		XmlNodeRef bonusXml = xml->findChild("Bonus");
		if(bonusXml)
		{
			XmlNodeRef bonusXPXml = bonusXml->findChild("XP");
			if(bonusXPXml)
			{
				bonusXPXml->getAttr("value", returnXP);
			}
		}
	}

	return returnXP;
}

void CPlayerProgression::SanityCheckRanks()
{
	for(int i = 1; i < m_maxRank; i++)
	{
		DesignerWarning(m_ranks[i - 1].m_xpRequired < m_ranks[i].m_xpRequired, "Rank %d needs more xp than rank %d", i - 1, i);
	}
}

CPlayerProgression::SRank::SRank(XmlNodeRef node)
{
	CRY_ASSERT_MESSAGE(strcmp(node->getTag(), "Rank") == 0, "Invalid tag found in rank xml");
	CRY_ASSERT_MESSAGE(node->haveAttr("name"), "Missing name attribute in rank xml");
	CRY_ASSERT_MESSAGE(node->haveAttr("xpRequired"), "Missing xpRequired attribute in rank xml");

	cry_strcpy(m_name, node->getAttr("name"));
	node->getAttr("xpRequired", m_xpRequired);
}

void CPlayerProgression::InitEvents(const char* filename)
{
	XmlNodeRef xml = GetISystem()->LoadXmlFromFile( filename );

	if(xml)
	{
		const int childCount = min(xml->getChildCount(), (int)EPP_Max);
		for (int iChild = 0; iChild < childCount; ++iChild)
		{
			XmlNodeRef childXML = xml->getChild(iChild);

			CRY_ASSERT_MESSAGE(childXML->haveAttr("name"), "Missing name attribute in event xml");
			CRY_ASSERT_MESSAGE(childXML->haveAttr("reward"), "Missing reward attribute in event xml");

			int index = 0;
			const bool eventFound = AutoEnum_GetEnumValFromString(childXML->getAttr("name"), s_eventName, EPP_Max, &index);
			DesignerWarning(eventFound, "Failed to find event '%s'", childXML->getAttr("name"));
			if (eventFound)
			{
				childXML->getAttr("reward", m_events[index]);
			}
		}
	}
}
void CPlayerProgression::Event(EPPType type, bool skillKill, void *data)
{
	CRY_ASSERT_MESSAGE(type >= 0 && type < EPP_Max, "Invalid event type");

	DEBUG_XP("Event %s", s_eventName[type], m_events[type]);

	// these two enums must stay in sync for the cast below to be valid
	static_assert(static_cast<int>(k_XPRsn_EPP_TeamRadar) == static_cast<int>(EPP_TeamRadar), "Unexpected enum value!");
	static_assert(static_cast<int>(k_XPRsn_EPP_FlushedAssist) == static_cast<int>(EPP_FlushedAssist), "Unexpected enum value!");

	EXPReason		reason;
	if (type==EPP_Invalid)
	{
		reason=k_XPRsn_Unknown;
	}
	else
	{
		reason=EXPReasonFromEPP(type);
	}

	PREFAST_ASSUME(type >= 0 && type < EPP_Max);
	int xpPointsToAward = m_events[type]; // XP Points

	// Apply any active score/xp modifiers
	IActor* pActor = gEnv->pGameFramework->GetClientActor();
	if(pActor && pActor->IsPlayer())
	{
		xpPointsToAward = static_cast<CPlayer*>(pActor)->GetXPBonusModifiedXP(xpPointsToAward);
	}

	if (xpPointsToAward > 0)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules && !pGameRules->HasGameActuallyStarted())
		{
			xpPointsToAward = 0;		// Still do the message but don't show any points
		}

		if(skillKill || IsSkillAssist(type))
		{
			SHUDEventWrapper::OnSkillKillMessage( type, xpPointsToAward );
		}
		else
		{
			SHUDEventWrapper::OnSupportBonusXPMessage( type, xpPointsToAward );
		}
	}

	SendEventToListeners(type, skillKill, data);
	
	EventInternal(xpPointsToAward, reason, (skillKill || IsSkillAssist(type)));
}


const bool CPlayerProgression::IsSkillAssist(const EPPType type) const
{
	return type == EPP_BlindAssist || type == EPP_FlushedAssist;
}


int CPlayerProgression::SkillAssessmentEvent(int points /*, EXPReason inReason*/)
{
	return EventInternal(points,k_XPRsn_SkillAssessment);		// TODO: need to provide more detailed reason for specific skill assessment
}

int CPlayerProgression::EventInternal(int points, EXPReason inReason, bool bSkillKill)
{
	int awardedXP = IncrementXP(points,inReason);

	if (bSkillKill)
	{
		m_skillKillXP += awardedXP;
	}

	return awardedXP;
}

bool CPlayerProgression::SkillKillEvent(CGameRules* pGameRules, IActor* pTargetActor, IActor* pShooterActor, const HitInfo &hitInfo, bool firstBlood)
{
	CRY_ASSERT(pGameRules);
	if(pTargetActor && pShooterActor &&	pTargetActor != pShooterActor)
	{
		CPlayer* pShooterPlayer = static_cast<CPlayer*>(pShooterActor);

		CPlayer* pTargetPlayer = static_cast<CPlayer*>(pTargetActor);

		if(!pShooterPlayer->IsFriendlyEntity(pTargetActor->GetEntityId())) // not friendly target
		{
			bool skillKillAwarded = false;

#define CheckEvent(check, event, allowMultiEvent) \
		if(check) \
			{ \
				Event(event, true); \
				if (allowMultiEvent) \
				{ \
					skillKillAwarded = true; \
				} \
				else \
				{ \
					return true; \
				} \
			}

#define CheckUniqueEvent(check, event) \
		CheckEvent(check, event, false)

#define CheckMultiEventSP(check, event) \
		CheckEvent(check, event, (gEnv->bMultiplayer == false))

			CheckMultiEventSP(firstBlood, EPP_FirstBlood); // first blood has been worked out by the server, we just need to know if we should show the event
			CheckMultiEventSP(SkillKill::IsKickedCarKill(pShooterPlayer, pTargetPlayer, hitInfo.weaponId), EPP_KickedCar); // Most difficult to achieve, so should be tested earliest
			EPPType multikillEvent = SkillKill::IsMultiKill(pShooterPlayer);
			CheckMultiEventSP(multikillEvent != EPP_Invalid, multikillEvent);
			CheckMultiEventSP(SkillKill::IsSuperChargedKill(pShooterPlayer), EPP_SuitSuperChargedKill);

			CheckUniqueEvent(SkillKill::IsStealthKill(pTargetPlayer), EPP_StealthKill);
			CheckUniqueEvent(SkillKill::IsRecoveryKill(pShooterPlayer), EPP_Recovery);
			CheckUniqueEvent(SkillKill::IsBlindKill(pShooterPlayer), EPP_BlindKill);
			CheckUniqueEvent(SkillKill::IsBlinding(pShooterPlayer, pTargetPlayer, hitInfo.weaponClassId), EPP_Blinding);
			CheckUniqueEvent(SkillKill::IsFlushed(pShooterPlayer, pTargetPlayer, pGameRules, hitInfo.type), EPP_Flushed);
			CheckUniqueEvent(SkillKill::IsDualWeapon(pShooterPlayer, pTargetPlayer), EPP_DualWeapon);
			CheckUniqueEvent(SkillKill::IsDefiantKill(pShooterPlayer), EPP_NearDeathExperience);
			CheckUniqueEvent(SkillKill::IsKillJoy(pTargetPlayer), EPP_KillJoy);
			CheckUniqueEvent(SkillKill::IsGuardianKill(pShooterPlayer, pTargetPlayer), EPP_Guardian);
			CheckUniqueEvent(SkillKill::IsInterventionKill(pShooterPlayer, pTargetPlayer), EPP_Intervention);
			CheckUniqueEvent(SkillKill::IsGotYourBackKill(pShooterPlayer, pTargetPlayer), EPP_GotYourBack);
			CheckUniqueEvent(SkillKill::IsRetaliationKill(pShooterPlayer, pTargetPlayer), EPP_Retaliation);
			CheckUniqueEvent(SkillKill::IsPiercing(hitInfo.penetrationCount), EPP_Piercing);
			CheckUniqueEvent(SkillKill::IsRumbled(pShooterPlayer, pTargetPlayer), EPP_Rumbled);
			CheckUniqueEvent(SkillKill::IsMeleeTakedown(pGameRules, hitInfo.type), EPP_MeleeTakedown);
			CheckUniqueEvent(SkillKill::IsHeadshot(pTargetPlayer, hitInfo.partId, hitInfo.material), EPP_Headshot);
			CheckUniqueEvent(SkillKill::IsAirDeath(pTargetPlayer), EPP_AirDeath);
			CheckUniqueEvent(SkillKill::IsIncomingKill(hitInfo.type), EPP_Incoming); //Stamp
			CheckUniqueEvent(SkillKill::IsPangKill(hitInfo.type), EPP_Pang); //Pick & Throw
			CheckUniqueEvent(SkillKill::IsAntiAirSupportKill(hitInfo.type), EPP_AntiAirSupport); //Explosion from VTOL being destroyed

			return skillKillAwarded;

#undef CheckEvent
#undef CheckUniqueEvent
#undef CheckMultiEventSP
		}
	}

	return false;
}

void CPlayerProgression::SkillAssistEvent(CGameRules *pGameRules, IActor* pTargetActor, IActor* pShooterActor, const HitInfo &hitInfo)
{
	CRY_ASSERT(pShooterActor == NULL || (pShooterActor && !pShooterActor->IsClient()));	//your client shouldn't be the shooter - this is just for assists

	IActor* pClientActor = gEnv->pGameFramework->GetClientActor();
	if(pClientActor && pTargetActor && pShooterActor)
	{
		CPlayer* pClientPlayer = static_cast<CPlayer*>(pClientActor);
		
		if(pClientPlayer->IsFriendlyEntity(pShooterActor->GetEntityId()) && !pClientPlayer->IsFriendlyEntity(pTargetActor->GetEntityId())) // not friendly target
		{
			CPlayer* pTargetPlayer = static_cast<CPlayer*>(pTargetActor);

			//Reused Skill Kill logic but passes in you instead of the killer
			if(SkillKill::IsBlinding(pClientPlayer, pTargetPlayer, hitInfo.weaponClassId))
			{
				Event(EPP_BlindAssist, false);
			}

			if(SkillKill::IsFlushed(pClientPlayer, pTargetPlayer, pGameRules, hitInfo.type))
			{
				Event(EPP_FlushedAssist, false);
			}
		}
	}
}

void CPlayerProgression::Update(CPlayer *pPlayer, float deltaTime, float fHealth)
{
	CGameRules *pGameRules=g_pGame->GetGameRules();
	if(!pGameRules)
		return;

	IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
	if (pStateModule && pStateModule->GetGameState() != IGameRulesStateModule::EGRS_InGame)
		return;

	IGameRulesRoundsModule *pRoundsMo = pGameRules->GetRoundsModule();
	if (pRoundsMo && !pRoundsMo->IsInProgress())
		return;

	CRY_ASSERT_MESSAGE(pPlayer->GetEntityId() == gEnv->pGameFramework->GetClientActorId() || g_pGame->IsGameSessionHostMigrating(), "CPlayerProgression::Update is happening on the wrong player entity!");

	if(fHealth > 0.0f)
	{
		m_time += deltaTime;
	}

	if (m_rankUpQueuedForHUD)
	{
		CRY_ASSERT(pPlayer == gEnv->pGameFramework->GetClientActor());

		float newTime = m_rankUpQueuedTimer = m_rankUpQueuedTimer - deltaTime;

		if (newTime <= 0.f)
		{
			const char* rankName = GetRankName(m_queuedRankForHUD, false);
			SHUDEventWrapper::OnPromotionMessage( rankName, m_queuedRankForHUD, m_queuedXPRequiredForHUD );

			//m_rankUpSignal.Play(pPlayer->GetEntityId(), "xprank", (float)m_queuedRankForHUD);	
			
			m_rankUpQueuedForHUD = false;
		}
	}
}


#ifndef _RELEASE
void CPlayerProgression::UpdateDebug()
{
	if(pp_debug > 0)
	{
		if(pp_debug == 1)
		{
			CryWatch("%s", !m_privateGame ? "Updating" : "Not Updating - Should be a private game");
			CryWatch("XP %d", m_xp);
			CryWatch("game start XP %d and rank %d", m_gameStartXp, m_gameStartRank);
			CryWatch("Rank %d - %s", m_rank, GetRankName(m_rank + 1));
			if(m_rank < m_maxRank - 1)
			{
				CryWatch("Next Rank %s in %d xp", GetRankName(m_rank + 2), m_ranks[m_rank+1].m_xpRequired - m_xp);
			}
			CryWatch("rankUpQueuedForHUD=%d; timer=%f; queuedRank=%d; queuedXPRequired=%d",	m_rankUpQueuedForHUD, m_rankUpQueuedTimer, m_queuedRankForHUD, m_queuedXPRequiredForHUD);
		}
		else if(pp_debug == 3)
		{
			const int unlockSize = m_unlocks.size();
			CryWatch("Unlocks %d", unlockSize);
			for(int i = 0; i < unlockSize; i++)
			{
				const SUnlock unlock = m_unlocks[i];
				CryWatch("Unlock %s on rank %d type %d - %s", unlock.m_name, unlock.m_rank, unlock.m_type, unlock.m_unlocked ? "Unlocked" : "Locked");
			}
		}
		else if(pp_debug == 4)
		{
			const int unlockSize = m_allowUnlocks.size();
			CryWatch("Unlocks %d", unlockSize);
			for(int i = 0; i < unlockSize; i++)
			{
				const SUnlock unlock = m_allowUnlocks[i];
				CryWatch("Unlock %s on rank %d type %d - %s", unlock.m_name, unlock.m_rank, unlock.m_type, unlock.m_unlocked ? "Unlocked" : "Locked");
			}
		}
		else if(pp_debug == 5)
		{
			const int unlockSize = m_customClassUnlocks.size();
			CryWatch("Custom Class Unlocks %d", unlockSize);
			for(int i = 0; i < unlockSize; i++)
			{
				const SUnlock unlock = m_customClassUnlocks[i];
				CryWatch("Unlock %s of type %d - %s", unlock.m_name, unlock.m_type, unlock.m_unlocked ? "Unlocked" : "Locked");
			}
		}
		else if(pp_debug == 6)
		{
			const int unlockSize = m_presaleUnlocks.size();
			CryWatch("Presale Unlocks %d", unlockSize);
			for(int i = 0; i < unlockSize; i++)
			{
				const SUnlock unlock = m_presaleUnlocks[i];
				CryWatch("Unlock %s of type %d - %s", unlock.m_name, unlock.m_type, unlock.m_unlocked ? "Unlocked" : "Locked");
			}
		}
	}

#if DEBUG_XP_ALLOCATION
	if(pp_xpDebug)
	{
		const int count = m_debugXp.size();
		const int start = max(count - pp_xpDebug, 0);
		for(int i = start; i < count; i++)
		{
			CryWatch("[%d] - %s", i, m_debugXp[i].c_str());
		}
	}
#endif //#if DEBUG_XP_ALLOCATION
}
#endif //#ifndef _RELEASE

int CPlayerProgression::IncrementXP(int amount, EXPReason inReason)
{
	if( !m_privateGame )
	{
		CRY_ASSERT(amount >= 0);

		if(gEnv->bMultiplayer == false)
		{
			return 0;
		}

		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules && !pGameRules->HasGameActuallyStarted())
		{
			return 0;
		}

		if (g_pGameCVars->g_xpMultiplyer != 1.f)
		{
			float fAmount = (float) amount;
			fAmount *= g_pGameCVars->g_xpMultiplyer;
			amount = int_round(fAmount);
		}

		const int newXp = m_xp + amount;
		DEBUG_XP("	Incrementing %d + %d = %d", m_xp, amount, newXp);
		m_xp = newXp;

		IActor*		pActor = gEnv->pGameFramework->GetClientActor();
		CPlayer*	pPlayer = NULL;

		if (pActor && pActor->IsPlayer())
		{
			pPlayer = static_cast<CPlayer*>(pActor);

			pPlayer->LogXPChangeToTelemetry(amount,inReason);
		}

		if (m_rank < m_maxRank -1 )
		{
			int xpRequired = m_ranks[m_rank + 1].m_xpRequired;

			if(newXp >= xpRequired)
			{// Rank up
				const int newRank = CalculateRankFromXp(newXp);
				m_rank = newRank;
				const int rankValueToUse = newRank+1;
				
				UpdateLocalUserData(); // static

				if (pPlayer)
				{
					CHANGED_NETWORK_STATE(pPlayer, CPlayer::ASPECT_RANK_CLIENT);

					if( rankValueToUse <= k_maxDisplayableRank )	//Hide additional top level rank(s)
					{
						if (m_rankUpQueuedForHUD)
						{
							// already got something queued play it now
							const char* rankName = GetRankName(m_queuedRankForHUD, false);
							SHUDEventWrapper::OnPromotionMessage( rankName, m_queuedRankForHUD, m_queuedXPRequiredForHUD );

							REINST("needs verification!");
							//m_rankUpSignal.Play(pActor->GetEntityId(), "xprank", (float)m_queuedRankForHUD);	
							// don't unset m_rankUpQueuedForHUD as we're about to set it again and keep Frank happy with LHS
						}

						m_queuedRankForHUD = rankValueToUse;
						m_queuedXPRequiredForHUD = xpRequired;
						m_rankUpQueuedForHUD = true;
						m_rankUpQueuedTimer = k_queuedRankTime;
					}
					else if(rankValueToUse == k_maxPossibleRanks) //Special case for max rank
					{
						m_queuedRankForHUD = rankValueToUse;
						m_queuedXPRequiredForHUD = xpRequired;
						m_rankUpQueuedForHUD = true;
						m_rankUpQueuedTimer = k_queuedRankTime;
					}
				}

				CryLog("CPlayerProgression - Rank up to %s!", GetRankName(rankValueToUse));

			}
		}
	}

	return amount;
}

int CPlayerProgression::IncrementXPPresale( int amount, EXPReason inReason )
{
	//other systems are in place to apply the presale XP to the online stats
	//this just updates the local profile, so is safe.
	if( inReason == k_XPRsn_PreOrder )
	{
		CRY_ASSERT(amount >= 0);

		const int newXp = m_xp + amount;
		DEBUG_XP("	Incrementing %d + %d = %d", m_xp, amount, newXp);
		m_xp = newXp;

		if (m_rank < m_maxRank -1 )
		{
			int xpRequired = m_ranks[m_rank + 1].m_xpRequired;

			if(newXp >= xpRequired)
			{
				// Rank up
				const int newRank = CalculateRankFromXp(newXp);
				m_rank = newRank;
				const int rankValueToUse = newRank+1;

				UpdateAllUnlocks(true);
				UpdateLocalUserData(); // static

				CryLog("CPlayerProgression - Rank up to %s Offline", GetRankName(rankValueToUse));
			}
		}
	}

	return amount;
}

void CPlayerProgression::ClientScoreEvent(EGameRulesScoreType type, int points, EXPReason inReason, int currentTeamScore)
{
	//don't take away xp
	if(points > 0)
	{
		DEBUG_XP("Score %s", s_scoreName[type]);

		//hud event should already have been handled
		EventInternal(points,inReason);
	}
}

void CPlayerProgression::EnteredGame()
{
#if DEBUG_XP_ALLOCATION
	m_debugXp.clear();
#endif

	m_skillKillXP = 0;
	m_gameStartXp = m_xp;
	m_gameStartRank = m_rank;
	m_matchBonus = 0;
	m_matchScore = 0;
	m_hasNewStats = false;

	CGameRules *pGameRules=g_pGame->GetGameRules();
	pGameRules->RegisterClientScoreListener(this);

	if( gEnv->bMultiplayer )
	{
		if( CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout() )
		{
			pEquipmentLoadout->InitUnlockedAttachmentFlags();
		}
	}
}

void CPlayerProgression::GameOver(EGameOverType localWinner, bool isClientSpectator)
{
	if(isClientSpectator)
	{
		return;
	}

	CryLog ("[PLAYER PROGRESSION] Game over! localWinner=%d", localWinner);
	INDENT_LOG_DURING_SCOPE();

	DEBUG_XP("  totalTime = %f", m_time);

	CCCPOINT(PlayerProgression_GameOver);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();
	IGameRulesPlayerStatsModule *pPlayerStatsModule = pGameRules ? pGameRules->GetPlayerStatsModule() : NULL;
	const SGameRulesPlayerStat *pClientPlayerStats = pPlayerStatsModule ? pPlayerStatsModule->GetPlayerStats(clientId) : NULL;
	if (pClientPlayerStats)
	{
		m_matchScore = std::max(0, pClientPlayerStats->points);

		CGameLobby *pGameLobby = g_pGame->GetGameLobby();
		if (pGameLobby && pGameLobby->IsRankedGame())		// Only update skill ranking if we're in a ranked game
		{
			const uint16 newSkillPoints = pClientPlayerStats->skillPoints;
			CryLog("CPlayerProgression::GameOver() updating skill rank, was %d, now %d (diff=%d)", m_skillRank, newSkillPoints, (int)newSkillPoints - (int)m_skillRank);
			m_skillRank = newSkillPoints;
		}
	}

	MatchBonus(localWinner, m_time);

	m_hasNewStats = true;

	m_time = 0.0f;
	
	UpdateAllUnlocks(true);
}

void CPlayerProgression::ReincarnateUnlocks()
{
	const int unlockSize = m_unlocks.size();
	for(int i = 0; i < unlockSize; i++)
	{
		SUnlock& unlock = m_unlocks[i];
		unlock.m_unlocked = false;
	}

	const int customUnlockSize = m_customClassUnlocks.size();
	for(int i = 0; i < customUnlockSize; i++)
	{
		SUnlock& unlock = m_customClassUnlocks[i];
		unlock.m_unlocked = false;
	}


	UpdateAllUnlocks(true);
}

void CPlayerProgression::UpdateAllUnlocks(bool fromProgress)
{
	CryLog ("[PLAYER PROGRESSION] Updating unlocks (reward=%s) rank = %d reincarnation = %d", fromProgress ? "true" : "false", m_rank, m_reincarnation );
	INDENT_LOG_DURING_SCOPE();

	const int unlockSize = m_unlocks.size();
	for(int i = 0; i < unlockSize; i++)
	{
		const SUnlock& unlock = m_unlocks[i];
		if((unlock.m_rank <= m_rank) && (unlock.m_reincarnation <= m_reincarnation))
		{
     	m_unlocks[i].Unlocked(fromProgress);
		}
	}
}
void CPlayerProgression::MatchBonus(const EGameOverType localWinner, const float totalTime)
{
	//length in game * win/draw/lose modifier * rank
	if(!gEnv->IsDedicated())
	{
		CRY_ASSERT_TRACE (localWinner == EGOT_Lose || localWinner == EGOT_Draw || localWinner == EGOT_Win, ("Unexpected 'local winner' value = %d", localWinner));

		CCCPOINT_IF(localWinner == EGOT_Lose, PlayerProgression_MatchBonusLose);
		CCCPOINT_IF(localWinner == EGOT_Draw, PlayerProgression_MatchBonusDraw);
		CCCPOINT_IF(localWinner == EGOT_Win,  PlayerProgression_MatchBonusWin);

		CGameRules *pGameRules=g_pGame->GetGameRules();
		if(pGameRules)
		{
			if((m_xp - m_gameStartXp) > 0)
			{
				float totalGameTime = pGameRules->GetCurrentGameTime();
				float fractionOfGamePlayed = totalGameTime > 0.0f ? totalTime/totalGameTime : 0.0f;

				m_matchBonus = GetMatchBonus(localWinner, fractionOfGamePlayed);
				if(m_matchBonus > 0)
				{
					DEBUG_XP("Match Bonus - %d", m_matchBonus);
					IncrementXP(m_matchBonus,k_XPRsn_MatchBonus);
				}
			}
			else
			{
				m_matchBonus = 0;	//if you don't get any xp during a game you don't get the bonus
			}
		}
	}
}

int CPlayerProgression::GetMatchBonus(EGameOverType localWinner, float fracTimePlayed) const
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	float totalGameTime = pGameRules ? pGameRules->GetCurrentGameTime() : 600.f;

	float winModifier = WinModifier(localWinner);
	float rankModifier = ((m_rank + 1)/4.0f) + 1.0f;
	float lengthBonus = totalGameTime/60.0f;

	float matchBonus = (fracTimePlayed * winModifier * rankModifier * lengthBonus);

	CryLog("CPlayerProgression::GetMatchBonus() rank=%d, result=%d, fracTimePlayed=%.2f, bonus=%d", m_rank, (int) localWinner, fracTimePlayed, (int) matchBonus);

	return (int) matchBonus;
}

float CPlayerProgression::WinModifier(const EGameOverType localWinner) const
{
	const static float k_WinModifier = 12.5f;
	const static float k_DrawModifier = 10.0f;
	const static float k_LoseModifier = 7.5f;

	switch(localWinner)
	{
	case EGOT_Win:
		return k_WinModifier;
	case EGOT_Draw:
		return k_DrawModifier;
	case EGOT_Lose:
		return k_LoseModifier;
	default:
		CRY_ASSERT_MESSAGE(false, "Unable to determine Win Modifier");
		return k_DrawModifier;
	}
}

void CPlayerProgression::InitConsoleCommands()
{
#if !defined(_RELEASE)
		REGISTER_COMMAND("pp_GainXP", CmdGainXP, VF_CHEAT, "increments your xp");
		REGISTER_COMMAND("pp_GameEnd", CmdGameEnd, VF_CHEAT, "increments your xp and applies end of round bonus");
		REGISTER_COMMAND("pp_UnlockAll", CmdUnlockAll, VF_CHEAT, "applies any unlocks gained immediately");
		REGISTER_COMMAND("pp_UnlocksNow", CmdUnlocksNow, VF_CHEAT, "applies any unlocks gained immediately");
		REGISTER_COMMAND("pp_ResetXP", CmdResetXP, VF_CHEAT, "resets xp to 0");
		REGISTER_COMMAND("pp_GainLevels", CmdGainLevels, VF_CHEAT, "individually increments your xp by enough to gain passed no. of levels");

		REGISTER_COMMAND("pp_FakeProgression", CmdFakePlayerProgression, VF_CHEAT, "(<rank>,<tactical>,<stealth>,<armour>,<power>) Fake player progression values for local player.");
		REGISTER_COMMAND("pp_GenerateProfile", CmdGenerateProfile, VF_CHEAT, "Generate a random profile");

		REGISTER_CVAR(pp_debug, 0, VF_NULL, "Enable/Disables player progression debug messages");
		REGISTER_CVAR(pp_xpDebug, pp_xpDebug, VF_NULL, "Enable/Disables xp debugging");
		REGISTER_CVAR(pp_defaultUnlockAll, 0, VF_NULL, "Can be used in cfg file to unlock everything");
		REGISTER_CVAR(pp_suitmodeAveraging, pp_suitmodeAveraging, VF_CHEAT, "fraction of avg xp that is shared, 1 being all average, 0 being time in xp gained/ time in suit mode (must be between 0.0f and 1.0f)");
#endif
}

bool CPlayerProgression::WillReincarnate () const 
{
	return (m_reincarnation < k_maxReincarnation);
}

const int CPlayerProgression::GetData(EPPData dataType)
{
	switch(dataType)
	{
		//Ranks values go from 0 to m_maxRank - 1
		//However outside world should see 1 to m_maxRank
		case EPP_Rank:
		return m_rank + 1;

		case EPP_MaxRank:
		return m_maxRank;

		case EPP_DisplayRank:
		  return min(m_rank + 1, static_cast<int>(k_maxDisplayableRank));

		case EPP_MaxDisplayRank:
		  return min(static_cast<int>(k_maxDisplayableRank), m_maxRank);

		case EPP_XP:
		return m_xp;

		case EPP_LifetimeXP:
		return m_lifetimeXp + m_xp;

		case EPP_XPToNextRank:
		return (m_rank < m_maxRank - 1) ? m_ranks[m_rank + 1].m_xpRequired - m_xp : 0;

		case EPP_XPLastMatch:
		return m_xp - m_gameStartXp;

		case EPP_XPMatchStart:
		return m_gameStartXp;

		case EPP_MatchStartRank:
		return m_gameStartRank + 1;

		case EPP_MatchStartDisplayRank:
		  return min( m_gameStartRank + 1, static_cast<int>(k_maxDisplayableRank) );

		case EPP_MatchStartXPInCurrentRank:
		return m_gameStartXp - m_ranks[m_gameStartRank].m_xpRequired;

		case EPP_MatchStartXPToNextRank:
		return (m_gameStartRank < m_maxRank - 1) ? m_ranks[m_gameStartRank + 1].m_xpRequired - m_gameStartXp : 0;

		case EPP_MatchBonus:
		return m_matchBonus;

		case EPP_MatchScore:
		return m_matchScore;

		case EPP_MatchXP:
		return GetData(EPP_XPLastMatch) - m_matchScore - m_matchBonus;

		case EPP_XPInCurrentRank:
		return m_xp - m_ranks[m_rank].m_xpRequired;

		case EPP_NextRank:
		return (m_rank < m_maxRank - 1) ? (m_rank + 2) : m_maxRank;

		case EPP_NextDisplayRank:
		  return min(m_rank + 2, static_cast<int>(k_maxDisplayableRank));

		case EPP_Reincarnate:
		return m_reincarnation;

		case EPP_CanReincarnate:
		return (m_rank == m_maxRank - 1) && (m_reincarnation < k_maxReincarnation) ? 1 : 0;

		case EPP_SkillRank:
		return m_skillRank;

		case EPP_SkillKillXP:
		return  m_skillKillXP;

	default:
		CRY_ASSERT_MESSAGE(false, "Unable to find data type in CPlayerProgression::GetData");
		return -1;
	}
}

// NOTE forRank should be in the "external rank" range - ie. 1<=forRank<=m_maxRank
float CPlayerProgression::GetProgressWithinRank(const int forRank, const int forXp)
{
	float  pro;
	if (forRank <= 0)
		pro = 0.f;
	else if (forRank >= m_maxRank)
		pro = 1.f;
	else
	{
		const float  remain = (float) (m_ranks[forRank].m_xpRequired - forXp);
		const float  range = (float) (m_ranks[forRank].m_xpRequired - m_ranks[forRank - 1].m_xpRequired);
		const float  fracrem = (remain / range);
		pro = (1.f - fracrem);
	}
	return pro;
}

// NOTE rank should be in the "external rank" range - ie. 1<=rank<=m_maxRank
const int CPlayerProgression::GetXPForRank(const uint8 rank) const
{
	if ((rank > 0) && (rank <= m_maxRank))
	{
		return m_ranks[rank-1].m_xpRequired;
	}

	return 0;
}

uint8 CPlayerProgression::GetRankForXP( const int xp ) const
{
	for (uint8 i=1; (i<m_maxRank); ++i)
	{
		if (xp < m_ranks[i].m_xpRequired)
		{
			return i;
		}
	}

	return m_maxRank + 1;
}

const char* CPlayerProgression::GetRankName(uint8 rank, bool localize)
{
	CRY_ASSERT(rank > 0 && rank <= m_maxRank);
	return localize ? CHUDUtils::LocalizeString(m_ranks[rank - 1].m_name) : m_ranks[rank - 1].m_name;
}

const char* CPlayerProgression::GetRankNameUpper(uint8 rank, bool localize)
{
	CRY_ASSERT(rank > 0 && rank <= m_maxRank);
	string upperName;
	upperName.Format("%s_upper", m_ranks[rank - 1].m_name);
	//upperName.Format("%s", m_ranks[rank - 1].m_name);

	return localize ? CHUDUtils::LocalizeString(upperName.c_str()) : upperName.c_str();
}


bool CPlayerProgression::HaveUnlocked(EUnlockType type, const char* name, SPPHaveUnlockedQuery &results) const
{
	results.available = false;
	results.exists = false;
	results.unlocked = false;

	int useRank = m_rank;
	int useReincarnation = m_reincarnation;

	if (g_pGame->IsGameActive())
	{
		useRank = m_gameStartRank;
	}

	const int customUnlockSize = m_customClassUnlocks.size();
	for(int i = 0; i < customUnlockSize; i++)
	{
		const SUnlock& unlock = m_customClassUnlocks[i];
		
		if(unlock.m_type == type)
		{
			if(strcmpi(unlock.m_name, name) == 0)
			{
				results.exists = true;
				results.reason = eUR_None;

				if(unlock.m_unlocked)
				{
					results.unlocked = true;
					return true;
				}
			}
		}
	}

	const int presaleUnlockSize = m_presaleUnlocks.size();
	for(int i = 0; i < presaleUnlockSize; i++)
	{
		const SUnlock& unlock = m_presaleUnlocks[i];
	
		if(unlock.m_type == type)
		{
			if(strcmpi(unlock.m_name, name) == 0)
			{
				results.exists = true;
				results.reason = eUR_None;

				if(unlock.m_unlocked)
				{
					results.unlocked = true;
					return true;
				}
			}
		}
	}

	// Check DLC unlocks
	SPPHaveUnlockedQuery dlcResults;
	g_pGame->GetDLCManager()->HaveUnlocked( type, name, dlcResults );
	if (dlcResults.exists && (type!=eUT_CreateCustomClass || dlcResults.unlocked))
	{
		results = dlcResults;
		return results.unlocked;
	}

	const int allowUnlockSize = m_allowUnlocks.size();
	for(int i = 0; i < allowUnlockSize; i++)
	{
		const SUnlock& unlock = m_allowUnlocks[i];
		if(unlock.m_type == type)
		{
			if(strcmpi(unlock.m_name, name) == 0)
			{
				results.exists = true;
				results.reason = eUR_Rank;

				if (unlock.m_rank <= useRank && (unlock.m_reincarnation <= useReincarnation))
				{
					//todo - find out if still used
					results.available = true;
				}
			}
		}
	}

	HaveUnlockedByRank(type, name, results, useRank, useReincarnation, false);

	bool bFound = results.exists;


	if (pp_defaultUnlockAll > 0)
	{
		results.unlocked = true;
	}
	return bFound;
}

//only checks rank and custom class unlocks (only needed for weapons and modules atm)
void CPlayerProgression::GetUnlockedCount( EUnlockType type, uint32* outUnlocked, uint32* outTotal ) const
{
	uint32 unlocked = 0;
	uint32 total = 0;

	const int customUnlockSize = m_customClassUnlocks.size();
	for( int i = 0; i < customUnlockSize; i++ )
	{
		const SUnlock& unlock = m_customClassUnlocks[ i ];

		if(unlock.m_type == type)
		{
			total++;

			if(unlock.m_unlocked)
			{
				unlocked++;
			}
		}
	}

	const int unlockSize = m_unlocks.size();
	for( int i = 0; i < unlockSize; i++ )
	{
		const SUnlock& unlock = m_unlocks[ i ];
		if( unlock.m_type == type )
		{
			total++;

			if(unlock.m_unlocked)
			{
				unlocked++;
			}
		}
	}

	*outUnlocked = unlocked;
	*outTotal = total;
}


void CPlayerProgression::HaveUnlockedByRank(EUnlockType type, const char* name, SPPHaveUnlockedQuery &results, const int rank, const int reincarnations, const bool bResetResults) const
{
	if(bResetResults)
	{
		results.available = false;
		results.exists = false;
		results.unlocked = false;
	}

	int useRank = rank;
	int useReincarnation = reincarnations;

	const int unlockSize = m_unlocks.size();
	for(int i = 0; i < unlockSize; i++)
	{
		const SUnlock& unlock = m_unlocks[i];
		if(unlock.m_type == type)
		{
			if(strcmpi(unlock.m_name, name) == 0)
			{
				results.exists = true;
				results.reason = eUR_Rank;

				if (unlock.m_rank <= useRank && unlock.m_reincarnation <= useReincarnation)
				{
					results.available = true;
					results.unlocked = unlock.m_unlocked;
				}
				else
				{
					if (unlock.m_reincarnation > useReincarnation)
					{
						results.unlockString = CHUDUtils::LocalizeString("@ui_loadout_unlock_reincarnation_generic"); // Generic 'item uplocked after recincarnation'. Add specific ones when required.
					}
					else
					{
						switch(type)
						{
						case eUT_Weapon:
							results.unlockString = CHUDUtils::LocalizeString("@ui_loadout_unlock_weapon", CryStringUtils::toString(unlock.m_rank + 1).c_str());
							break;
						case eUT_Loadout:
							results.unlockString = CHUDUtils::LocalizeString("@ui_loadout_unlock_loadout", CryStringUtils::toString(unlock.m_rank + 1).c_str());
							break;
						case eUT_Playlist:
							results.unlockString = CHUDUtils::LocalizeString("@ui_loadout_unlock_playlist", CryStringUtils::toString(unlock.m_rank + 1).c_str());
							break;
						case eUT_CreateCustomClass:
							results.unlockString = CHUDUtils::LocalizeString("@ui_loadout_unlock_custom_class2", CryStringUtils::toString(unlock.m_rank + 1).c_str());
							break;
						}
					}
				}
				if( results.unlocked == true )
				{
					//Unlock found so break. Otherwise continue as there may be multiple versions of an unlock with different parameters
					break;
				}
			}
		}
	}
}

void CPlayerProgression::ResetXP()
{
	m_lifetimeXp = 0;
	m_xp = 0;
	m_gameStartXp = 0;
	m_rank = 0;
	m_gameStartRank = 0;
	m_matchBonus = 0;
	m_reincarnation = 0;

	
	UpdateLocalUserData(); // static

	IActor* pActor = gEnv->pGameFramework->GetClientActor();
	if(pActor && pActor->IsPlayer())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		CHANGED_NETWORK_STATE(pPlayer, CPlayer::ASPECT_RANK_CLIENT);
	}
}

void CPlayerProgression::UpdateStartRank()
{
	m_gameStartRank = m_rank;
}

void CPlayerProgression::AddEventListener(IPlayerProgressionEventListener* pEventListener)
{
	stl::push_back_unique(m_eventListeners, pEventListener);
}

void CPlayerProgression::RemoveEventListener(IPlayerProgressionEventListener* pEventListener)
{
	stl::find_and_erase(m_eventListeners, pEventListener);
}

void CPlayerProgression::SendEventToListeners(EPPType type, bool skillKill, void *data)
{
	if (!m_eventListeners.empty())
	{
		TEventListener::iterator iter = m_eventListeners.begin();
		TEventListener::iterator end = m_eventListeners.end();
		for(; iter != end; ++iter)
		{
			(*iter)->OnEvent(type, skillKill, data);
		}
	}
}

#if !defined(_RELEASE)
//static
void CPlayerProgression::CmdGainXP(IConsoleCmdArgs* pCmdArgs)
{
	if(pCmdArgs->GetArgCount() == 2)
	{
		CPlayerProgression* pThis = CPlayerProgression::GetInstance();
		

		if( pThis->AllowedWriteStats() )
		{
			const char* number = pCmdArgs->GetArg(1);
			int xp = atoi(number);
			CryLogAlways("Incrementing %d", xp);
			pThis->IncrementXP(xp,k_XPRsn_Cheat);
		}
		else
		{
			CryLog( "pp_gainXP can only be used in MP while in a Ranked Game" );
		}
		
	
	}
	else
	{
		CryLogAlways("[Usage] pp_GainXP <amountOfXp>");
	}
}


//static
void CPlayerProgression::CmdGameEnd(IConsoleCmdArgs* pCmdArgs)
{
	if(pCmdArgs->GetArgCount() == 2)
	{
		const CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
		const bool isSpectator = pActor ? pActor->GetSpectatorState() == CActor::eASS_SpectatorMode : false;

		const char* number = pCmdArgs->GetArg(1);
		int xp = atoi(number);
		CryLogAlways("Incrementing %d", xp);
		CPlayerProgression::GetInstance()->IncrementXP(xp,k_XPRsn_Cheat);
		CPlayerProgression::GetInstance()->GameOver(EGOT_Win, isSpectator);
	}
	else
	{
		CryLogAlways("[Usage] pp_GameEnd <amountOfXp>");
	}
}

//static
bool g_suppressUserDataUpdate = false;

void CPlayerProgression::CmdGainLevels( IConsoleCmdArgs* pCmdArgs )
{
	if(pCmdArgs->GetArgCount() == 2)
	{
		const char* number = pCmdArgs->GetArg(1);
		int levels = atoi(number);

		CPlayerProgression* pProgression = CPlayerProgression::GetInstance();

		if( pProgression->AllowedWriteStats() )
		{
			g_suppressUserDataUpdate = true;

			for( int i = 0; i < levels; i++ )
			{
				if( pProgression->m_rank >= pProgression->m_maxRank )
				{
					CryLog( "PP: GainLevels level %d and already max rank", i );
					break;
				}

				CryLog( "PP: GainLevels %d", i );
				int currentStart = pProgression->GetXPForRank( pProgression->m_rank+1 );
				int nextStart = pProgression->GetXPForRank( pProgression->m_rank+2 );
				int xpToGain = nextStart - currentStart;

				CryLog( "PP: For Rank %d to %d Gain %d - %d = %d XP", pProgression->m_rank+1, pProgression->m_rank+2, nextStart, currentStart, xpToGain );
				pProgression->IncrementXP( xpToGain, k_XPRsn_Cheat );
				pProgression->GameOver(EGOT_Win, false);
			}

			g_suppressUserDataUpdate = false;

			UpdateLocalUserData();
		}
		else
		{
			CryLog( "pp_gainLevels can only be used in MP while in a Ranked Game" );
		}

		
	}
}

//static
void CPlayerProgression::CmdUnlockAll(IConsoleCmdArgs* pCmdArgs)
{
	CPlayerProgression* pPlayerProgression = CPlayerProgression::GetInstance();
	pPlayerProgression->IncrementXP(pPlayerProgression->GetXPForRank(pPlayerProgression->m_maxRank), k_XPRsn_Cheat);
	if(pCmdArgs->GetArgCount() == 1)
	{
		pPlayerProgression->m_reincarnation = k_maxReincarnation;
	}


	pPlayerProgression->UpdateAllUnlocks(true);

	CPersistantStats* pPersistantStats = CPersistantStats::GetInstance();
	if (pPersistantStats)
	{
		pPersistantStats->UnlockAll();
	}

	g_pGame->GetProfileOptions()->SaveProfile(ePR_All);
}

//static
void CPlayerProgression::CmdUnlocksNow(IConsoleCmdArgs* pCmdArgs)
{
	CryLogAlways("Mid-game unlocked unlocks for rank %s", CPlayerProgression::GetInstance()->GetRankName(CPlayerProgression::GetInstance()->GetData(EPP_Rank)));

	CPlayerProgression::GetInstance()->UpdateStartRank();
}

//static
void CPlayerProgression::CmdResetXP(IConsoleCmdArgs* pCmdArgs)
{
	CryLogAlways("Reset player xp");

	CPlayerProgression::GetInstance()->ResetXP();
}

void CPlayerProgression::DebugFakeSetProgressions(int8 fakeRank, int8 fakeDefault, int8 fakeStealth, int8 fakeArmour)
{
	m_rank = (fakeRank - 1);

	UpdateLocalUserData(); // static

	IActor*  pActor = gEnv->pGameFramework->GetClientActor();
	if (pActor && pActor->IsPlayer())
	{
		CPlayer*  pPlayer = static_cast<CPlayer*>(pActor);
		CHANGED_NETWORK_STATE(pPlayer, CPlayer::ASPECT_RANK_CLIENT);
	}
}

void CPlayerProgression::CmdFakePlayerProgression(IConsoleCmdArgs *pArgs)
{
	// "pp_FakeProgression"
	// "(<rank>,<tactical>,<stealth>,<armour>,<power>)

	if (IActor* pClientActor=g_pGame->GetIGameFramework()->GetClientActor())
	{
		int  argCount = pArgs->GetArgCount();

		if (argCount == 5)
		{
			int  r = atoi(pArgs->GetArg(1));
			int  d = atoi(pArgs->GetArg(2));
			int  s = atoi(pArgs->GetArg(3));
			int  a = atoi(pArgs->GetArg(4));

			CryLogAlways("CmdFakePlayerProgression() faking local player's progression with: rank=%d, default=%d, stealth=%d, armour=%d", r, d, s, a);

			CPlayerProgression*  pp = CPlayerProgression::GetInstance();
			CRY_ASSERT(pp);
			pp->DebugFakeSetProgressions(r, d, s, a);
		}
		else
		{
			CryLogAlways("CmdFakePlayerProgression() unhandled number of arguments %d (expecting 5)", argCount);
		}
	}
	else
	{
		CryLogAlways("CmdFakePlayerProgression() needs a local actor to work");
	}
}

void CPlayerProgression::CmdGenerateProfile(IConsoleCmdArgs *pCmdArgs)
{
	if(pCmdArgs->GetArgCount() == 2)
	{
		const char* number = pCmdArgs->GetArg(1);
		int rank = atoi(number);
		if(rank > 0 && rank <= k_maxPossibleRanks)
		{
			CPlayerProgression* pProgression = CPlayerProgression::GetInstance();
			int xpToGain = pProgression->m_ranks[rank].m_xpRequired - pProgression->m_xp;
			if(xpToGain > 0)
			{
				//skip to rank
				gEnv->pConsole->ExecuteString(string().Format("pp_gameEnd %d", xpToGain).c_str());
			}
			else
			{
				CryLogAlways("Already at rank %d or higher", rank);
			}

			CryLogAlways("Spending tokens");
		}
		else
		{
			CryLogAlways("Invalid rank %d, should be between 0 and %d", rank, k_maxPossibleRanks);
		}
	}
	else
	{
		CryLogAlways("Need a rank to generate profile for");
	}
}

#endif //#if !defined(_RELEASE)

//static 
const char* CPlayerProgression::GetEPPTypeName(EPPType type)
{
	CRY_ASSERT(type >= 0 && type < EPP_Max);
	return s_eventName[type];
}

//static 
void CPlayerProgression::UpdateLocalUserData()
{
#if ! defined(_RELEASE)
	if( g_suppressUserDataUpdate )
		return;
#endif

	if(CGameLobby *pGameLobby = g_pGame->GetGameLobby())
	{
		pGameLobby->LocalUserDataUpdated();
	}
	if (CSquadManager *pSquadManager = g_pGame->GetSquadManager())
	{
		pSquadManager->LocalUserDataUpdated();
	}
}

void CPlayerProgression::Reincarnate()
{
	CRY_ASSERT(GetData(EPP_CanReincarnate));

	m_lifetimeXp += m_xp;
	m_reincarnation++;
	m_xp = 0;
	m_rank = 0;
	m_gameStartXp = 0;
	m_gameStartRank = 0;

	//Unlocks defaults again (and sets skill assessments starting value)
	ReincarnateUnlocks();

	CEquipmentLoadout* pLoadouts = g_pGame->GetEquipmentLoadout();
	pLoadouts->ResetLoadoutsToDefault();
	
	//should re-lock presale unlocks then run unlock logic again
	TUnlockElements::iterator presaleEnd = m_presaleUnlocks.end();
	for( TUnlockElements::iterator presale = m_presaleUnlocks.begin(); presale < presaleEnd; ++presale )
	{
		presale->m_unlocked = false;
	}

	g_pGame->GetDLCManager()->ActivatePreSaleBonuses(false, true);	//don't show popups, do give everything


	//Notify your squad and the local squad display (or it'll keep old info)
	UpdateLocalUserData(); // static
}

void CPlayerProgression::SetPrivateGame(const bool privateGame)
{
	m_privateGame = privateGame;
}

void CPlayerProgression::OnEndSession()
{
	CryLog("CPlayerProgression::OnEndSession %d", m_privateGame);

	if (!m_privateGame)
	{
		UpdateAllUnlocks(true);
	}
}

int CPlayerProgression::GetXPForEvent( EPPType type ) const
{
	return m_events[type];
}

void CPlayerProgression::OnQuit()
{
	if (!m_privateGame)
	{
		UpdateAllUnlocks(true);
	}
}

void CPlayerProgression::OnFinishMPLoading()
{
	UpdateAllUnlocks(false);
	g_pGame->GetDLCManager()->ActivatePreSaleBonuses( true );
	//remove any items that are used (in custom loadout etc.) but we don't have access to because of missing presale
	g_pGame->GetEquipmentLoadout()->CheckPresale();	
}

void CPlayerProgression::GetRankUnlocks(int firstrank, int lastrank, int reincarnations, TUnlockElementsVector &unlocks) const
{
	TUnlockElements::const_iterator it = m_unlocks.begin();
	TUnlockElements::const_iterator endit = m_unlocks.end();

	while (it!=endit)
	{
		const int unlock_rank = it->m_rank + 1;
		if((unlock_rank >= firstrank) && 
			(unlock_rank <= lastrank) &&
			(it->m_reincarnation <= reincarnations) )
		{
			unlocks[unlock_rank - firstrank].push_back(*it);
		}

		++it;
	}
}

bool CPlayerProgression::AllowedWriteStats() const
{
	bool retval = false;
	if( gEnv->bMultiplayer )
	{
		if( CGameRules* pRules = g_pGame->GetGameRules() )
		{
			if( !m_privateGame )
			{
				retval = true;
			}
		}
	}
	else
	{
		retval = true;
	}

	return retval;
}
