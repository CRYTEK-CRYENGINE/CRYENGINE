// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 21:07:2009		Created by Tim Furnish
*************************************************************************/

#ifndef __ANNOUNCER_H__
#define __ANNOUNCER_H__

#include "Audio/AudioTypes.h"
#include "UI/HUD/HUDOnScreenMessageDef.h"
#include "Utility/SingleAllocTextBlock.h"
#include "GameRulesTypes.h"

#define INVALID_ANNOUNCEMENT_ID (-1)

struct SAnnouncementDef
{
	EAnnouncementID					m_announcementID;
	EAnnounceConditions			m_conditions;
	const char *						m_pName;
	const char *						m_audio;
	uint32									m_audioFlags;
	SOnScreenMessageDef			m_onScreenMessage;
	float										m_noRepeat;

#ifndef _RELEASE
	uint32									m_playCount;
#endif

	SAnnouncementDef()
		: m_announcementID(0)
		, m_conditions(0)
		, m_pName("")
		, m_audio("")
		, m_audioFlags(0)
		, m_noRepeat(0.0f)
#ifndef _RELEASE
		, m_playCount(0)
#endif
	{}
};

class CAnnouncer /*: public ISoundEventListener*/
{
	public:
		CAnnouncer(const EGameMode gamemode);
		~CAnnouncer();

		static EAnnouncementID NameToID(const char* announcement);

		enum EAnnounceContext
		{
			eAC_inGame = 0,
			eAC_postGame,
			eAC_always
		};

		void Announce(const char* announcement, EAnnounceContext context);

		//Works out play conditions around entity id e.g self, teammate or enemy
		void Announce(const EntityId entityID, const char* announcement, EAnnounceContext context);
		void Announce(const EntityId entityID, EAnnouncementID announcementID, EAnnounceContext context);

		//FromTeamId works out play conditions, e.g teammate or enemy
		void AnnounceFromTeamId(const int teamId, EAnnouncementID announcementID, EAnnounceContext context);
		void AnnounceFromTeamId(const int teamId, const char* announcement, EAnnounceContext context);

		//AsTeam plays with specific teamId
		void AnnounceAsTeam(const int teamId, EAnnouncementID announcementID, EAnnounceContext context);
		void AnnounceAsTeam(const int teamId, const char* announcement, EAnnounceContext context);

		//Force announce overrides currently playing
		void ForceAnnounce(const char* announcement, EAnnounceContext context);
		void ForceAnnounce(const EAnnouncementID announcementID, EAnnounceContext context);

		static CAnnouncer* GetInstance();

#ifndef _RELEASE
		static void CmdAnnounce(IConsoleCmdArgs* pArgs);
		static void CmdDump(IConsoleCmdArgs* pArgs);
		static void CmdDumpPlayed(IConsoleCmdArgs *pCmdArgs);
		static void CmdDumpUnPlayed(IConsoleCmdArgs *pCmdArgs);
		static void CmdClearPlayCount(IConsoleCmdArgs *pCmdArgs);
		static void CmdCheckAnnouncements(IConsoleCmdArgs* pArgs);

		int GetCount() const;
		const char* GetName(int i) const;
		const char* GetAudio(int i) const;
#endif

		//void OnSoundEvent( ESoundCallbackEvent event, ISound *pSound );

	private:
		void LoadAnnouncements(const EGameMode gamemode);

		bool AnnounceInternal(EntityId entityID, const int overrideTeamId, const char* announcement, const EAnnounceConditions conditions, const bool force, EAnnounceContext context);
		bool AnnounceInternal(EntityId entityID, const int overrideTeamId, const EAnnouncementID announcementID, const EAnnounceConditions conditions, const bool force, EAnnounceContext context);

		bool AllowAnnouncement() const;
		bool AllowNoRepeat(const float noRepeat, const float currentTime);

		const char* GetGamemodeName(EGameMode mode);
		
		// Suppress passedByValue for smart pointers like XmlNodeRef
		// cppcheck-suppress passedByValue
		bool ShouldLoadNode(const XmlNodeRef gamemodeXML, const char* currentGamemode, const char* currentLanguage);
		// cppcheck-suppress passedByValue
		bool LoadAnnouncementOption(const char* optionName, const XmlNodeRef optionXML, const EAnnounceConditions conditions, const char * storedName, SOnScreenMessageDef &onScreenMessage, const float noRepeat, int &numAnnouncementsAdded);

		void AddAnnouncement(const char * pName, const EAnnounceConditions condition, const char * audio, const SOnScreenMessageDef & onScreenMessage, const float noRepeat);

		const SAnnouncementDef * FindAnnouncementWithConditions(int announcementCRC, EAnnounceConditions conditions) const;

		EAnnounceConditions GetConditionsFromEntityId(const EntityId entityId) const;
		EAnnounceConditions GetConditionsFromTeamId(const int teamId) const;

		void ConvertToFinalSoundName(const int overrideTeamId, string& filename);
		const int GetTeam(const int overrideTeamId);

		static CAnnouncer* s_announcer_instance;
		std::vector<SAnnouncementDef> m_annoucementList;
		//typedef std::vector<tSoundID> TSoundIDList;
		//TSoundIDList m_soundsBeingListenedTo;

		//tSoundID m_mostRecentSoundId;

		mutable float m_lastPlayedTime;
		mutable float m_canPlayFromTime;
		mutable float m_repeatTime;

		TAudioSignalID m_messageSignalID;
		
		CSingleAllocTextBlock m_singleAllocTextBlock;
};

#endif // __ANNOUNCER_H__