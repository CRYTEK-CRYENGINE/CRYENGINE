// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 21:07:2009		Created by Tim Furnish
*************************************************************************/

#include "StdAfx.h"
#include "Audio/Announcer.h"
#include "Audio/GameAudio.h"
#include "GameRules.h"
#include <CryString/StringUtils.h>
#include "Utility/DesignerWarning.h"
#include "Player.h"
#include <CrySystem/ILocalizationManager.h>
#include <CrySystem/ISystem.h>
#include "UI/UIManager.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "UI/HUD/HUDUtils.h"
#include <GameXmlParamReader.h>
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesStateModule.h"

#define ANNOUNCER_FILENAME "Scripts/Sounds/cw2_Annoucements.xml"

#define ANNOUNCER_MAX_LOAD_WAIT_TIMEOUT 5.0f

#define CHECK_FOR_THIS_MANY_DUPLICATED_STRINGS_WHEN_READING_ANNOUNCER_XML 128

#define AnnouncerConditionList(f)   \
	f(kAnnouncerCondition_self)       \
	f(kAnnouncerCondition_teammate)   \
	f(kAnnouncerCondition_enemy)      \
	f(kAnnouncerCondition_any)        \

AUTOENUM_BUILDFLAGS_WITHZERO(AnnouncerConditionList, kAnnouncerCondition_none);

static AUTOENUM_BUILDNAMEARRAY(s_conditionNames, AnnouncerConditionList);

CRY_TODO(26, 1, 2010, "kAnnouncerCondition_any isn't really any....");

CAnnouncer* CAnnouncer::s_announcer_instance = NULL;

//------------------------------------------------------------------------
CAnnouncer* CAnnouncer::GetInstance()
{
	CRY_ASSERT(s_announcer_instance);
	return s_announcer_instance;
}


#ifndef _RELEASE
//------------------------------------------------------------------------
struct SAnnouncerAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const { return CAnnouncer::GetInstance()->GetCount(); };
	virtual const char* GetValue( int nIndex ) const { return CAnnouncer::GetInstance()->GetName(nIndex); };
};

static SAnnouncerAutoComplete s_perkNameAutoComplete;
#endif

#ifndef _RELEASE
//------------------------------------------------------------------------
int CAnnouncer::GetCount() const
{
	return m_annoucementList.size();
}
#endif

#ifndef _RELEASE
//------------------------------------------------------------------------
const char* CAnnouncer::GetName(int i) const
{
	return m_annoucementList[i].m_pName;
}
#endif

#ifndef _RELEASE
//------------------------------------------------------------------------
const char* CAnnouncer::GetAudio(int i) const
{
	return m_annoucementList[i].m_audio;
}
#endif

//------------------------------------------------------------------------
CAnnouncer::CAnnouncer(const EGameMode gamemode)
{
	CRY_ASSERT(!s_announcer_instance);
	s_announcer_instance = this;

#ifndef _RELEASE
	IConsole * console = gEnv->pConsole;
	if (console)
	{
		REGISTER_COMMAND("announce", CAnnouncer::CmdAnnounce, VF_NULL, "Trigger a local announcement");
		REGISTER_COMMAND("announcerDump", CAnnouncer::CmdDump, VF_NULL, "Dump loaded announcements to console");
		REGISTER_COMMAND("announcerDumpPlayed", CAnnouncer::CmdDumpPlayed, VF_NULL, "Dump loaded announcements that have played to console");
		REGISTER_COMMAND("announcerDumpUnPlayed", CAnnouncer::CmdDumpUnPlayed, VF_NULL, "Dump loaded announcements that haven't played to console");
		REGISTER_COMMAND("announcerClearPlayCount", CAnnouncer::CmdClearPlayCount, VF_NULL, "Clear the playcounts of all announcements");
		REGISTER_COMMAND("announcerCheck", CAnnouncer::CmdCheckAnnouncements, VF_NULL, "Check all announcements in gamemode");
		
		console->RegisterAutoComplete("announce",				& s_perkNameAutoComplete);
	}
#endif

	m_lastPlayedTime = 0.0f;
	m_canPlayFromTime = 0.0f;
	m_repeatTime = 0.0f;

	LoadAnnouncements(gamemode);

	//m_soundsBeingListenedTo.reserve(10);
	//m_mostRecentSoundId = INVALID_SOUNDID;
	m_messageSignalID = g_pGame->GetGameAudio()->GetSignalID("Notify_announcer");
}

//------------------------------------------------------------------------
CAnnouncer::~CAnnouncer()
{
#ifndef _RELEASE
	IConsole * console = gEnv->pConsole;
	if (console)
	{
		console->RemoveCommand("announce");
		console->RemoveCommand("announcerDump");
		console->RemoveCommand("announcerDumpPlayed");
		console->RemoveCommand("announcerDumpUnPlayed");
		console->RemoveCommand("announcerClearPlayCount");
		console->RemoveCommand("announcerCheck");

		console->UnRegisterAutoComplete("announce");
	}
#endif

	CRY_ASSERT(s_announcer_instance == this);
	s_announcer_instance = NULL;

	/*ISoundSystem *pSoundSystem = gEnv->pSoundSystem;
	for (TSoundIDList::const_iterator it=m_soundsBeingListenedTo.begin(); it!=m_soundsBeingListenedTo.end(); ++it)
	{
		tSoundID soundID = *it;
		_smart_ptr<ISound> pSound = pSoundSystem->GetSound(soundID);
		if (pSound)
		{
			CryLog("[ANNOUNCER] Found a dangling sound being listened to %x; removing. This used to cause crashes", soundID);
			pSound->RemoveEventListener((ISoundEventListener*) this);
		}
		else
		{
			CRY_ASSERT_MESSAGE(0, string().Format("~CAnnouncer() has found a soundID we were listening to %x; which has failed to resolve into a valid ISound*", soundID).c_str());
			CryLog("[ANNOUNCER] Found a dangling soundID being listened to %x but failed to resolve it. Doing nothing", soundID);
		}
	}*/
}

//------------------------------------------------------------------------
void CAnnouncer::LoadAnnouncements(const EGameMode gamemode)
{
	const char* currentLanguage = gEnv->pSystem->GetLocalizationManager()->GetLanguage();
	const char* currentGamemodeName = GetGamemodeName(gamemode);

	m_annoucementList.clear();
	m_singleAllocTextBlock.Reset();

	const char* filename = ANNOUNCER_FILENAME;

	CryComment("[ANNOUNCER] LoadAnnouncements('%s')", filename);

	XmlNodeRef xmlNode = GetISystem()->LoadXmlFromFile( filename );
	DesignerWarning (xmlNode, "Announcement definitions file '%s' could not be read!", filename);
	if( xmlNode == NULL )
	{
		//This can legitimately fail to load at runtime, so return cleanly
		return;
	}

	CGameXmlParamReader xmlReader(xmlNode);

	const int childCount = xmlReader.GetUnfilteredChildCount();

	CSingleAllocTextBlock::SReuseDuplicatedStrings duplicatedStringsWorkspace[CHECK_FOR_THIS_MANY_DUPLICATED_STRINGS_WHEN_READING_ANNOUNCER_XML];
	m_singleAllocTextBlock.SetDuplicatedStringWorkspace(duplicatedStringsWorkspace, CHECK_FOR_THIS_MANY_DUPLICATED_STRINGS_WHEN_READING_ANNOUNCER_XML);

	// Calculate how much memory we'll need to store all the strings...
	for (int iChild = 0; iChild < childCount; ++iChild)
	{
		const XmlNodeRef childXML = xmlReader.GetFilteredChildAt(iChild);

		IF_UNLIKELY( childXML == NULL )
		{
			continue;
		}

		const char * tagType = childXML->getTag();
		const char * name = childXML->getAttr("Name");
		bool isAnnouncement = (stricmp("Announcement", tagType) == 0);

		DesignerWarning(isAnnouncement, "While reading '%s' found a '%s' tag when only expected to find 'Announcement' tags", filename, tagType);
		DesignerWarning(name, "While reading '%s' found a '%s' tag with no name!", filename, tagType);

		if (name && isAnnouncement)
		{
			CGameXmlParamReader childReader(childXML);

			const int subChildCount = childReader.GetUnfilteredChildCount();

			m_singleAllocTextBlock.IncreaseSizeNeeded(name);

			for (int iSubChild = 0; iSubChild < subChildCount; ++iSubChild)
			{
				const XmlNodeRef optionXML = childReader.GetFilteredChildAt(iSubChild);

				IF_UNLIKELY( optionXML == NULL)
				{
					continue;
				}

				if(0 == stricmp(optionXML->getTag(), "Gamemode"))
				{
					if(ShouldLoadNode(optionXML, currentGamemodeName, currentLanguage))
					{
						CGameXmlParamReader optionReader(optionXML);

						const int gamemodeChildCount = optionReader.GetUnfilteredChildCount();
						for (int iGamemodeChild = 0; iGamemodeChild < gamemodeChildCount; ++iGamemodeChild)
						{
							const XmlNodeRef gamemodeOptionXML = optionReader.GetFilteredChildAt(iGamemodeChild);

							if (gamemodeOptionXML)
							{
								const char * gamemodeOptionName = gamemodeOptionXML->getTag();
								
								if (0 == stricmp(gamemodeOptionName, "SoundOption"))
								{
									m_singleAllocTextBlock.IncreaseSizeNeeded(gamemodeOptionXML->getAttr("sound"));
								}
							}
						}						
					}
				}
				else if (0 == stricmp(optionXML->getTag(), "SoundOption"))
				{
					m_singleAllocTextBlock.IncreaseSizeNeeded(optionXML->getAttr("sound"));
				}
			}
		}
	}

	// Allocate memory for strings...
	m_singleAllocTextBlock.Allocate();

	// Now use allocated memory to store strings...
	for (int iChild = 0; iChild < childCount; ++iChild)
	{
		const XmlNodeRef childXML = xmlReader.GetFilteredChildAt(iChild);

		IF_UNLIKELY( childXML == NULL )
		{
			continue;
		}

		const char * tagType = childXML->getTag();
		const char * name = childXML->getAttr("Name");
		bool isAnnouncement = (stricmp("Announcement", tagType) == 0);

		if (isAnnouncement && name)
		{
			const char * storedName = m_singleAllocTextBlock.StoreText(name);
			const char * conStr = childXML->getAttr("condition");
			EAnnounceConditions conditions = (conStr && conStr[0]) ? AutoEnum_GetBitfieldFromString(conStr, s_conditionNames, AnnouncerConditionList_numBits) : kAnnouncerCondition_any;

			float noRepeatTime = 0.0f;
			childXML->getAttr("norepeat", noRepeatTime);

			SOnScreenMessageDef onScreenMessage;

			CGameXmlParamReader childReader(childXML);

			const int subChildCount = childReader.GetUnfilteredChildCount();
			DesignerWarning(childCount > 0, "While reading '%s' found an announcement (name='%s' condition='%s') with no child tags!", filename, name, conStr);
			int numAnnouncementsAdded = 0;

			for (int iSubChild = 0; iSubChild < subChildCount; ++iSubChild)
			{
				const XmlNodeRef optionXML = childReader.GetFilteredChildAt(iSubChild);

				IF_UNLIKELY( optionXML == NULL )
				{
					continue;
				}

				const char * optionName = optionXML->getTag();
				if (0 == stricmp(optionName, "Gamemode"))
				{
					if(ShouldLoadNode(optionXML, currentGamemodeName, currentLanguage))
					{
						CGameXmlParamReader optionReader(optionXML);
						const int gamemodeChildCount = optionReader.GetUnfilteredChildCount();
						for (int iGamemodeChild = 0; iGamemodeChild < gamemodeChildCount; ++iGamemodeChild)
						{
							const XmlNodeRef gamemodeOptionXML = optionReader.GetFilteredChildAt(iGamemodeChild);
							if (gamemodeOptionXML != NULL)
							{
								const char * gamemodeOptionName = gamemodeOptionXML->getTag();

								bool loaded = LoadAnnouncementOption(gamemodeOptionName, gamemodeOptionXML, conditions, storedName, onScreenMessage, noRepeatTime, numAnnouncementsAdded);
								DesignerWarning(loaded, "While reading '%s' found an unexpected tag '%s' inside an announcement (name='%s' condition='%s')", filename, optionName, name, conStr);
							}
						}
					}
				}
				else
				{
					bool loaded = LoadAnnouncementOption(optionName, optionXML, conditions, storedName, onScreenMessage, noRepeatTime, numAnnouncementsAdded);
					DesignerWarning(loaded, "While reading '%s' found an unexpected tag '%s' inside an announcement (name='%s' condition='%s')", filename, optionName, name, conStr);
				}
			}

			if (numAnnouncementsAdded == 0 && !onScreenMessage.empty())
			{
				const char * sound = NULL;
				AddAnnouncement(storedName, conditions, sound, onScreenMessage, 0.0f);
			}
		}
	}

	m_singleAllocTextBlock.Lock();
}

//------------------------------------------------------------------------
const char* CAnnouncer::GetGamemodeName(EGameMode mode)
{
	switch (mode)
	{
	case eGM_AllOrNothing:
		return "AON";
	case eGM_Assault:
		return "AS";
	case eGM_BombTheBase:
		return "BTB";
	case eGM_CaptureTheFlag:
		return "CTF";
	case eGM_CrashSite:
		return "CS";
	case eGM_Extraction:
		return "EXT";
	case eGM_InstantAction:
		return "IA";
	case eGM_PowerStruggle:
		return "PS";
	case eGM_TeamInstantAction:
		return "TIA";
	case eGM_Gladiator:
		return "GL";
	default:
		CRY_ASSERT_TRACE(false, ("Annoucner doesn't know what gamemode attribute to check for (%d needs adding to switch statement)", mode));
		return "";
	}
}

//------------------------------------------------------------------------
bool CAnnouncer::ShouldLoadNode(const XmlNodeRef gamemodeXML, const char* currentGamemodeName, const char* currentLanguage)
{
	CRY_ASSERT(0 == stricmp(gamemodeXML->getTag(), "Gamemode"));

	int shouldLoadSpecficLang = 0;
	if(!gamemodeXML->getAttr(currentLanguage, shouldLoadSpecficLang))
	{
		if (!gamemodeXML->getAttr("languagedefault", shouldLoadSpecficLang))
		{
			shouldLoadSpecficLang = 1;
		}
	}

	if(shouldLoadSpecficLang)
	{
		int shouldLoad = 0;
		if(gamemodeXML->getAttr(currentGamemodeName, shouldLoad))
		{
			return (shouldLoad == 1);
		}

		if(gamemodeXML->getAttr("default", shouldLoad))
		{
			return (shouldLoad == 1);
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CAnnouncer::LoadAnnouncementOption(const char* optionName, const XmlNodeRef optionXML, const EAnnounceConditions conditions, const char * storedName, SOnScreenMessageDef &onScreenMessage, const float noRepeat, int &numAnnouncementsAdded)
{
	if (0 == stricmp(optionName, "DisplayMessage"))
	{
		onScreenMessage.Read(optionXML);
	}
	else if (0 == stricmp(optionName, "SoundOption"))
	{
		const char * sound = m_singleAllocTextBlock.StoreText(optionXML->getAttr("sound"));
		AddAnnouncement(storedName, conditions, sound, onScreenMessage, noRepeat);
		numAnnouncementsAdded++;
	}
	else
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------------
void CAnnouncer::AddAnnouncement(const char * pName, const EAnnounceConditions condition, const char * audio, const SOnScreenMessageDef & onScreenMessage, const float noRepeat)
{
	SAnnouncementDef newAnnoucement;

	newAnnoucement.m_announcementID = CAnnouncer::NameToID(pName);
	newAnnoucement.m_pName = pName;
	newAnnoucement.m_conditions = condition;
	newAnnoucement.m_audio = audio;
	//newAnnoucement.m_audioFlags = (audio && !strstr(audio, ":")) ? FLAG_SOUND_VOICE : FLAG_SOUND_EVENT;
	newAnnoucement.m_onScreenMessage = onScreenMessage;
	newAnnoucement.m_noRepeat = noRepeat;

	m_annoucementList.push_back(newAnnoucement);
}

//------------------------------------------------------------------------
const SAnnouncementDef * CAnnouncer::FindAnnouncementWithConditions(const int announcementCRC, EAnnounceConditions conditions) const
{
	const int annoucementSize = m_annoucementList.size();
	std::vector<const SAnnouncementDef*> results;

	for(int i = 0; i < annoucementSize; i++)
	{
		if(m_annoucementList[i].m_announcementID == announcementCRC)
		{
			if(m_annoucementList[i].m_conditions & conditions)
			{
				results.push_back(&m_annoucementList[i]);
			}
		}
	}

	if(!results.empty())
	{
		return results[cry_random((size_t)0, results.size() - 1)];
	}

	return NULL;
}

//------------------------------------------------------------------------
void CAnnouncer::Announce(const char* announcement, EAnnounceContext context)
{
	AnnounceInternal(0, 0, announcement, kAnnouncerCondition_any, false, context);
}

//------------------------------------------------------------------------
void CAnnouncer::Announce(const EntityId entityID, EAnnouncementID announcementID, EAnnounceContext context)
{
	EAnnounceConditions conditions = GetConditionsFromEntityId(entityID);
	AnnounceInternal(entityID, 0, announcementID, conditions, false, context);
}

//------------------------------------------------------------------------
void CAnnouncer::Announce(const EntityId entityID, const char* announcement, EAnnounceContext context)
{
	EAnnounceConditions conditions = GetConditionsFromEntityId(entityID);
	AnnounceInternal(entityID, 0, announcement, conditions, false, context);
}

//------------------------------------------------------------------------
void CAnnouncer::AnnounceFromTeamId(const int teamId, EAnnouncementID announcementID, EAnnounceContext context) 
{
	EAnnounceConditions conditions = GetConditionsFromTeamId(teamId);
	AnnounceInternal(0, 0, announcementID, conditions, false, context);
}

//------------------------------------------------------------------------
void CAnnouncer::AnnounceFromTeamId(const int teamId, const char* announcement, EAnnounceContext context) 
{
	EAnnounceConditions conditions = GetConditionsFromTeamId(teamId);
	AnnounceInternal(0, 0, announcement, conditions, false, context);
}

//------------------------------------------------------------------------
void CAnnouncer::AnnounceAsTeam(const int teamId, EAnnouncementID announcementID, EAnnounceContext context)
{
	AnnounceInternal(0, teamId, announcementID, kAnnouncerCondition_any, false, context);
}

//------------------------------------------------------------------------
void CAnnouncer::AnnounceAsTeam(const int teamId, const char* announcement, EAnnounceContext context)
{
	AnnounceInternal(0, teamId, announcement, kAnnouncerCondition_any, false, context);
}

//------------------------------------------------------------------------
void CAnnouncer::ForceAnnounce(const char* announcement, EAnnounceContext context)
{
	AnnounceInternal(0, 0, announcement, kAnnouncerCondition_any, true, context);
}

//------------------------------------------------------------------------
void CAnnouncer::ForceAnnounce(const EAnnouncementID announcementID, EAnnounceContext context)
{
	AnnounceInternal(0, 0, announcementID, kAnnouncerCondition_any, true, context);
}

//------------------------------------------------------------------------
EAnnounceConditions CAnnouncer::GetConditionsFromTeamId(const int teamId) const
{
	EAnnounceConditions conditions = kAnnouncerCondition_any;
	CGameRules *pGameRules=g_pGame->GetGameRules();
	CRY_ASSERT(pGameRules);
	EntityId localActorEntityID = gEnv->pGameFramework->GetClientActorId();

	int localTeam = pGameRules->GetTeam(localActorEntityID);

	if (localTeam == 0 || localTeam != teamId)
	{
		conditions |= kAnnouncerCondition_enemy;
	}
	else
	{
		conditions |= kAnnouncerCondition_teammate;
	}
	return conditions;
}

//------------------------------------------------------------------------
EAnnounceConditions CAnnouncer::GetConditionsFromEntityId(const EntityId entityId) const
{
	EAnnounceConditions conditions = kAnnouncerCondition_any;
	CGameRules *pGameRules=g_pGame->GetGameRules();
	CRY_ASSERT(pGameRules);
	EntityId localActorEntityID = gEnv->pGameFramework->GetClientActorId();

	if (localActorEntityID == entityId)
	{
		conditions |= kAnnouncerCondition_self;
	}
	else
	{
		int localTeam = pGameRules->GetTeam(localActorEntityID);
		int entityTeam = pGameRules->GetTeam(entityId);

		if (localTeam == 0 || localTeam != entityTeam)
		{
			conditions |= kAnnouncerCondition_enemy;
		}
		else
		{
			conditions |= kAnnouncerCondition_teammate;
		}
	}
	return conditions;
}

//------------------------------------------------------------------------
bool CAnnouncer::AnnounceInternal(EntityId entityID, const int overrideTeamId, const char* announcement, const EAnnounceConditions conditions, const bool force, EAnnounceContext context)
{
	EAnnouncementID announcementID = CAnnouncer::NameToID(announcement);
	return AnnounceInternal(entityID, overrideTeamId, announcementID, conditions, force, context);
}

//------------------------------------------------------------------------
bool CAnnouncer::AnnounceInternal(EntityId entityID, const int overrideTeamId, const EAnnouncementID announcementID, const EAnnounceConditions conditions, const bool force, EAnnounceContext context)
{
	bool doAudio = true;
	bool okToAnnounce = false;
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesStateModule *pStateModule = pGameRules ? pGameRules->GetStateModule() : NULL;
	if (pStateModule)
	{
		IGameRulesStateModule::EGR_GameState gameState = pStateModule->GetGameState();

		switch(context)
		{
			case eAC_inGame:
				if (gameState == IGameRulesStateModule::EGRS_InGame)
					okToAnnounce = true;
				break;
			case eAC_postGame:
				if (gameState == IGameRulesStateModule::EGRS_PostGame)
					okToAnnounce = true;
				break;
			case eAC_always:
				okToAnnounce=true;
				break;
			default:
				CRY_ASSERT_MESSAGE(0, string().Format("AnnounceInternal() unhandled context=%d", context));
				break;
		}

		if (!okToAnnounce)
		{
	#ifndef _RELEASE
			const SAnnouncementDef * pAnnouncement = FindAnnouncementWithConditions(announcementID, conditions);
			CryLog("[ANNOUNCER] CAnnouncer::AnnounceInternal() rejecting announcement=%d (%s); as our gamestate %d isn't valid for our context=%d",	announcementID, pAnnouncement ? pAnnouncement->m_pName : "<ANNOUNCEMENT INVALID>", gameState, context);
	#endif
			return false;
		}
	}
	else
	{
		CryLog("[ANNOUNCER] CAnnouncer::AnnounceInternal() has no stateModule to validate against");
	}

	const float currentTime = gEnv->pTimer->GetCurrTime();

	if (!force && (m_canPlayFromTime > currentTime))
	{
		CryLog("[ANNOUNCER] CanPlay %.2f - currentTime %.2f", m_canPlayFromTime, currentTime);
		doAudio = false;
	}

	const SAnnouncementDef * doAnnouncement = FindAnnouncementWithConditions(announcementID, conditions);

	if (doAnnouncement)
	{
		bool allow = force || (AllowNoRepeat(doAnnouncement->m_noRepeat, currentTime) && AllowAnnouncement());
		if (doAudio && doAnnouncement->m_audio && allow)
		{
			string audioFilename(doAnnouncement->m_audio);
			ConvertToFinalSoundName(overrideTeamId, audioFilename);
			/*_smart_ptr<ISound> const pSound = gEnv->pSoundSystem->CreateSound(audioFilename.c_str(), doAnnouncement->m_audioFlags);

			if (pSound)
			{
				if (force && (m_mostRecentSoundId != INVALID_SOUNDID))
				{
					if (_smart_ptr<ISound> const pRecent=gEnv->pSoundSystem->GetSound(m_mostRecentSoundId))
					{
						if (pRecent->IsPlaying())
						{
							CryLog("[ANNOUNCER] attempting to FORCE play sound '%s' by stopping most recent sound '%s'", audioFilename.c_str(), pRecent->GetName());
							pRecent->Stop();
						}
					}
				}

				pSound->SetSemantic(doAnnouncement->m_audioFlags & FLAG_SOUND_VOICE ? eSoundSemantic_OnlyVoice : eSoundSemantic_SoundSpot);
				pSound->Play();

#ifndef _RELEASE
				{
					SAnnouncementDef *pNonConstAnnouncement = const_cast<SAnnouncementDef*>(doAnnouncement);
					++pNonConstAnnouncement->m_playCount;
				}
#endif

				m_lastPlayedTime = gEnv->pTimer->GetCurrTime();
				m_canPlayFromTime = m_lastPlayedTime + ANNOUNCER_MAX_LOAD_WAIT_TIMEOUT;
				pSound->AddEventListener((ISoundEventListener*) this, "CAnnouncer");
				const tSoundID  sid = pSound->GetId();
				m_soundsBeingListenedTo.push_back(sid);
				m_mostRecentSoundId = sid;
			}
			else
			{
				CryLog("[ANNOUNCER] Unable to create sound '%s' - probably invalid sound in XML file or s_soundEnable is set to 0", audioFilename.c_str());
				m_mostRecentSoundId = INVALID_SOUNDID;
			}*/
		}

		if (!doAnnouncement->m_onScreenMessage.empty() && g_pGame->GetIGameFramework()->GetClientActor())
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityID);
			SHUDEventWrapper::GameStateNotify( doAnnouncement->m_onScreenMessage.GetDisplayText(), m_messageSignalID, pEntity ? pEntity->GetName() : "??" );
		}
	}

	return doAnnouncement != NULL;
}

//------------------------------------------------------------------------
bool CAnnouncer::AllowAnnouncement() const
{
	bool allow = true;
	CGameRules* pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
	{
		IGameRulesRoundsModule* pRoundsModule=pGameRules->GetRoundsModule();
		if (pRoundsModule)
		{
			allow = allow && pRoundsModule->IsInProgress();
		}
	}

	allow = allow && !g_pGame->GetUI()->IsLoading();

	return allow;
}

//------------------------------------------------------------------------
bool CAnnouncer::AllowNoRepeat(const float noRepeat, const float currentTime)
{
	if(noRepeat > 0.0f)
	{
		if(m_repeatTime > currentTime)
		{
			CryLog("[ANNOUNCER] Repeat Time %.2f < currentTime %.2f", m_repeatTime, currentTime);
			return false;
		}

		m_repeatTime = currentTime + noRepeat;	//allowed to play - but keep a record of time
	}

	return true;
}

//static------------------------------------------------------------------------
EAnnouncementID CAnnouncer::NameToID(const char* announcement)
{
	CryStackStringT<char, 32> lower(announcement);
	lower.MakeLower();
	return CCrc32::Compute(lower.c_str());
}


#ifndef _RELEASE
//------------------------------------------------------------------------
void CAnnouncer::CmdAnnounce(IConsoleCmdArgs* pCmdArgs)
{
	// First argument is command itself
	if (pCmdArgs->GetArgCount() > 1)
	{
		const char * arg = pCmdArgs->GetArg(1);

		EAnnounceConditions conditions = (pCmdArgs->GetArgCount() > 2) ? atoi(pCmdArgs->GetArg(2)) : kAnnouncerCondition_self|kAnnouncerCondition_teammate|kAnnouncerCondition_enemy|kAnnouncerCondition_any;
		int annoucements = (pCmdArgs->GetArgCount() > 3) ? atoi(pCmdArgs->GetArg(3)) : 1;
		for(int i = 0; i < annoucements; i++)
		{
			bool done = GetInstance()->AnnounceInternal(0, 0, arg, conditions, true, eAC_always);
			if (! done)
			{
				CryLog ("[ANNOUNCER] Failed to find an appropriate announcement (%s %u)", arg, conditions);
			}
		}
	}
	else
	{
		CryLog ("[ANNOUNCER] Wrong number of arguments to console command");
	}
}
#endif

#ifndef _RELEASE
//------------------------------------------------------------------------
void CAnnouncer::CmdDump(IConsoleCmdArgs* pCmdArgs)
{
	const CAnnouncer* pAnnouncer = GetInstance();
	const int count = pAnnouncer->GetCount();

	CryLog("----------------------------------------------------------------------");
	CryLog("Dump all");
	CryLog("----------------------------------------------------------------------");
	
	for(int i = 0; i < count; i++)
	{
		CryLogAlways("Announcer: %s - %s (playCount=%d)", pAnnouncer->GetName(i), pAnnouncer->GetAudio(i), pAnnouncer->m_annoucementList[i].m_playCount);
	}
	
	CryLog("----------------------------------------------------------------------");
	CryLog("Total: %d", count);
	CryLog("----------------------------------------------------------------------");
}
#endif

#ifndef _RELEASE
//------------------------------------------------------------------------
void CAnnouncer::CmdDumpPlayed(IConsoleCmdArgs *pCmdArgs)
{
	const CAnnouncer* pAnnouncer = GetInstance();
	const int count = pAnnouncer->GetCount();

	CryLog("----------------------------------------------------------------------");
	CryLog("Dump Played");
	CryLog("----------------------------------------------------------------------");
	int numDumped=0;

	for(int i = 0; i < count; i++)
	{
		if (pAnnouncer->m_annoucementList[i].m_playCount > 0)
		{
			CryLogAlways("Announcer: %s - %s (playCount=%d)", pAnnouncer->GetName(i), pAnnouncer->GetAudio(i), pAnnouncer->m_annoucementList[i].m_playCount);
			numDumped++;
		}
	}

	CryLog("----------------------------------------------------------------------");
	CryLog("Total Played: %d", numDumped);
	CryLog("----------------------------------------------------------------------");
}
#endif

#ifndef _RELEASE
//------------------------------------------------------------------------
void CAnnouncer::CmdDumpUnPlayed(IConsoleCmdArgs *pCmdArgs)
{
	const CAnnouncer* pAnnouncer = GetInstance();
	const int count = pAnnouncer->GetCount();
	
	CryLog("----------------------------------------------------------------------");
	CryLog("Dump UnPlayed");
	CryLog("----------------------------------------------------------------------");
	int numDumped=0;

	for(int i = 0; i < count; i++)
	{
		if (pAnnouncer->m_annoucementList[i].m_playCount == 0)
		{
			CryLogAlways("%s - %s (playCount=%d)", pAnnouncer->GetName(i), pAnnouncer->GetAudio(i), pAnnouncer->m_annoucementList[i].m_playCount);
			numDumped++;
		}
	}

	CryLog("----------------------------------------------------------------------");
	CryLog("Total UnPlayed: %d", numDumped);
	CryLog("----------------------------------------------------------------------");
}
#endif

#ifndef _RELEASE
//------------------------------------------------------------------------
void CAnnouncer::CmdClearPlayCount(IConsoleCmdArgs *pCmdArgs)
{
	CAnnouncer* pAnnouncer = GetInstance();
	const int count = pAnnouncer->GetCount();
	int numCleared=0;

	for(int i = 0; i < count; i++)
	{
		if (pAnnouncer->m_annoucementList[i].m_playCount > 0)
		{
			numCleared++;
		}

		pAnnouncer->m_annoucementList[i].m_playCount = 0;
	}

	CryLog("----------------------------------------------------------------------");
	CryLog("Cleared PlayCount on %d announcements", numCleared);
	CryLog("----------------------------------------------------------------------");
}
#endif

#ifndef _RELEASE
//------------------------------------------------------------------------
void CAnnouncer::CmdCheckAnnouncements(IConsoleCmdArgs* pCmdArgs)
{
	CAnnouncer* pAnnouncer = GetInstance();
	const int annoucementSize = pAnnouncer->m_annoucementList.size();

	for(int i = 0; i < annoucementSize; i++)
	{
		for(int j = 1; j <= 2; j++)
		{
			string audioFilename(pAnnouncer->m_annoucementList[i].m_audio);
			pAnnouncer->ConvertToFinalSoundName(j, audioFilename);
			/*ISound* pSound = gEnv->pSoundSystem->CreateSound(audioFilename, pAnnouncer->m_annoucementList[i].m_audioFlags);
			if(!pSound)
			{
				CryLogAlways("[ANNOUNCER] Unable to create sound '%s'", audioFilename.c_str());
			}*/
		}
	}
}
#endif



//------------------------------------------------------------------------
//void CAnnouncer::OnSoundEvent( ESoundCallbackEvent event, ISound *pSound )
//{
//	if(event == SOUND_EVENT_ON_LOADED)
//	{
//		const int lengthMs = pSound->GetLengthMs();
//		CRY_ASSERT(lengthMs != 0);	//This should only be zero when the sound isn't loaded
//		const float length = (float) lengthMs/1000.0f;
//		m_canPlayFromTime = m_lastPlayedTime + length;
//		pSound->RemoveEventListener((ISoundEventListener*) this);
//		if (!stl::find_and_erase(m_soundsBeingListenedTo, pSound->GetId()))
//		{
//			CRY_ASSERT_MESSAGE(0, "CAnnouncer::OnSoundEvent() failed to find a sound in our own listeners vector. This should not happen");
//		}
//	}
//	else if(event == SOUND_EVENT_ON_LOAD_FAILED)
//	{
//		m_canPlayFromTime = m_lastPlayedTime;
//		pSound->RemoveEventListener((ISoundEventListener*) this);
//		if (!stl::find_and_erase(m_soundsBeingListenedTo, pSound->GetId()))
//		{
//			CRY_ASSERT_MESSAGE(0, "CAnnouncer::OnSoundEvent() failed to find a sound in our own listeners vector. This should not happen");
//		}
//	}
//}

//------------------------------------------------------------------------
void CAnnouncer::ConvertToFinalSoundName(const int overrideTeamId, string& filename)
{
	if(filename.find("[team]"))
	{
		const int teamIndex = GetTeam(overrideTeamId);

		string temp = filename.replace("[team]", "%d");
		filename.Format(temp.c_str(), teamIndex);
	}
}
//------------------------------------------------------------------------
const int CAnnouncer::GetTeam(const int overrideTeamId)
{
	int teamIndex = 1;
	if(overrideTeamId != 0)
	{
		teamIndex = overrideTeamId;
	}
	else
	{
		//get team of local client
		CGameRules* pGameRules = g_pGame->GetGameRules();
		CRY_ASSERT(pGameRules);
		EntityId localActorEntityID = gEnv->pGameFramework->GetClientActorId();
		teamIndex = pGameRules->GetTeam(localActorEntityID);
	}

	teamIndex = CLAMP(teamIndex, 1, 2);

	return teamIndex;
}
