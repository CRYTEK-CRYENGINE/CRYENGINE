// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Manager for playlists

-------------------------------------------------------------------------
History:
- 06:03:2010 : Created by Tom Houghton

*************************************************************************/

#include "StdAfx.h"
#include "PlaylistManager.h"

#include <CrySystem/ILocalizationManager.h>
#include "GameCVars.h"
#include "IGameRulesSystem.h"
#include "UI/ProfileOptions.h"
#include "Network/Lobby/GameLobby.h"
#include "GameRules.h"
#include "PlayerProgression.h"

#include "UI/UICVars.h"
#include "UI/UIManager.h"

#define LOCAL_WARNING(cond, msg)  do { if (!(cond)) { CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "%s", msg); } } while(0)
//#define LOCAL_WARNING(cond, msg)  CRY_ASSERT_MESSAGE(cond, msg)

static CPlaylistManager*  s_pPlaylistManager = NULL;

#define MAX_ALLOWED_NEGATIVE_OPTION_AMOUNT 50

#define OPTION_RESTRICTIONS_FILENAME "Scripts/Playlists/OptionRestrictions.xml"

//=======================================
// SPlaylistVariantAutoComplete

//---------------------------------------
// Used by console auto completion.
struct SPlaylistVariantAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const 
	{
		int count = 0;
		CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
		if (pPlaylistManager)
		{
			const int numPlaylists = pPlaylistManager->GetNumPlaylists();
			for (int i = 0; i < numPlaylists; ++ i)
			{
				const SPlaylist *pPlaylist = pPlaylistManager->GetPlaylist(i);
				if (pPlaylist->IsEnabled())
				{
					const SPlaylistRotationExtension::TVariantsVec &supportedVariants = pPlaylist->GetSupportedVariants();
					count += supportedVariants.size();
				}
			}
		}
		return count;
	}

	bool GetPlaylistAndVariant(int nIndex, const SPlaylist **pPlaylistOut, const SGameVariant **pVariantOut) const
	{
		CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
		if (pPlaylistManager)
		{
			const int numPlaylists = pPlaylistManager->GetNumPlaylists();
			for (int i = 0; i < numPlaylists; ++ i)
			{
				const SPlaylist *pPlaylist = pPlaylistManager->GetPlaylist(i);
				if (pPlaylist->IsEnabled())
				{
					const SPlaylistRotationExtension::TVariantsVec &supportedVariants = pPlaylist->GetSupportedVariants();
					const int numVariants = supportedVariants.size();
					if (nIndex < numVariants)
					{
						const SGameVariant *pVariant = pPlaylistManager->GetVariant(supportedVariants[nIndex].m_id);
						if (pVariant)
						{
							*pPlaylistOut = pPlaylist;
							*pVariantOut = pVariant;
							return true;
						}
					}
					else
					{
						nIndex -= numVariants;
					}
				}
			}
		}
		return false;
	}

	const char *GetOptionName(const SPlaylist *pPlaylist, const SGameVariant *pVariant) const
	{
		static CryFixedStringT<128> combinedName;
		combinedName.Format("%s  %s", pPlaylist->GetUniqueName(), pVariant->m_name.c_str());
		combinedName.replace(' ', '_');
		return combinedName.c_str();
	}

	virtual const char* GetValue( int nIndex ) const 
	{ 
		const SPlaylist *pPlaylist = NULL;
		const SGameVariant *pVariant = NULL;

		if (GetPlaylistAndVariant(nIndex, &pPlaylist, &pVariant))
		{
			return GetOptionName(pPlaylist, pVariant);
		}
		return "UNKNOWN";
	};
};
// definition and declaration must be separated for devirtualization
SPlaylistVariantAutoComplete gl_PlaylistVariantAutoComplete;

//=======================================
// SPlaylist

//---------------------------------------
void SPlaylist::ResolveVariants()
{
	if (!rotExtInfo.m_resolvedVariants)
	{
		// First time in, find all the variant ids
		CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
		if (pPlaylistManager)
		{
			bool hasDefault = false;
			int defaultIndex = pPlaylistManager->GetDefaultVariant();

			int numVariants = rotExtInfo.m_supportedVariants.size();
			for (int i = 0; i < numVariants; ++ i)
			{
				int variantIndex = pPlaylistManager->GetVariantIndex(rotExtInfo.m_supportedVariants[i].m_name.c_str());
				if (variantIndex >= 0)
				{
					rotExtInfo.m_supportedVariants[i].m_id = variantIndex;
					if (variantIndex == defaultIndex)
					{
						hasDefault = true;
					}
				}
				else
				{
					// Unknown variant, throw it away
					SPlaylistRotationExtension::TVariantsVec::iterator it = rotExtInfo.m_supportedVariants.begin() + i;
					rotExtInfo.m_supportedVariants.erase(it);
					-- i;
					-- numVariants;
				}
			}

			if (rotExtInfo.m_allowDefaultVariant && (!hasDefault) && (defaultIndex != -1))
			{
				const SGameVariant *pVariant = pPlaylistManager->GetVariant(defaultIndex);
				SSupportedVariantInfo defaultVariant;
				defaultVariant.m_name = pVariant->m_name;
				defaultVariant.m_id = pVariant->m_id;

				rotExtInfo.m_supportedVariants.insert(rotExtInfo.m_supportedVariants.begin(), 1, defaultVariant);
			}
		}
		rotExtInfo.m_resolvedVariants = true;
	}
}

//=======================================
// SPlaylistRotationExtension

//---------------------------------------
// Suppress passedByValue for smart pointers like XmlNodeRef
// cppcheck-suppress passedByValue
bool SPlaylistRotationExtension::LoadFromXmlNode(const XmlNodeRef xmlNode)
{
	bool  loaded = false;

	if (const XmlNodeRef infoNode=xmlNode->findChild("ExtendedInfo"))
	{
		if (!infoNode->getAttr("enabled", m_enabled))
		{
			LOCAL_WARNING(0,"Level Rotation is a Playlist but its \"PlaylistInfo\" entry doesn't have an \"enabled\" attribute");
		}

		infoNode->getAttr("requiresUnlocking", m_requiresUnlocking);

		infoNode->getAttr("hidden", m_hidden);

		const char * pPath;
		infoNode->getAttr("image", &pPath);
		imagePath = pPath;

		uniqueName = infoNode->getAttr("uniquename");
		LOCAL_WARNING(uniqueName, "Level Rotation is a Playlist but its \"PlaylistInfo\" entry doesn't have a \"uniqueName\" attribute");

		const char*  pCurrentLang = gEnv->pSystem->GetLocalizationManager()->GetLanguage();
		CRY_ASSERT(pCurrentLang);

		CPlaylistManager::GetLocalisationSpecificAttr<TLocaliNameStr>(infoNode, "localname", pCurrentLang, &localName);

		CPlaylistManager::GetLocalisationSpecificAttr<TLocaliDescStr>(infoNode, "localdescription", pCurrentLang, &localDescription);

		const char*  typeStr = NULL;
		if (infoNode->getAttr("supportedvariants", &typeStr))
		{
			int foundAtIndex = 0;
			const char *startFrom = typeStr;

			do
			{
				char variant[32];
				foundAtIndex = cry_copyStringUntilFindChar(variant, startFrom, sizeof(variant), '|');
				startFrom += foundAtIndex;

				m_supportedVariants.push_back(SSupportedVariantInfo(variant));
			} while (foundAtIndex);
		}

		infoNode->getAttr("useDefaultVariant", m_allowDefaultVariant);

		int maxPlayers = MAX_PLAYER_LIMIT;
		if (infoNode->getAttr("maxPlayers", maxPlayers))
		{
			maxPlayers = CLAMP(maxPlayers, 2, MAX_PLAYER_LIMIT);
		}
		m_maxPlayers = maxPlayers;

		loaded = true;
	}
	else
	{
		LOCAL_WARNING(0, "Level Rotation xmlNode doesn't have a \"ExtendedInfo\" entry");
	}

	return loaded;
}

//=======================================
// SGameModeOption

//---------------------------------------
void SGameModeOption::CopyToCVar( CProfileOptions *pProfileOptions, bool useDefault, const char *pGameModeName )
{
	if (m_pCVar)
	{
#ifdef _DEBUG
		if (m_pCVar->GetFlags() & VF_WASINCONFIG)
		{
			// Allow gamemode options to be overridden in user.cfg when in debug mode
			return;
		}
#endif

		TFixedString optionName;
		if (m_gameModeSpecific)
		{
			optionName.Format(m_profileOption.c_str(), pGameModeName);
		}
		else
		{
			optionName = m_profileOption.c_str();
		}

		if ((!optionName.empty()) && pProfileOptions->IsOption(optionName.c_str()))
		{
			if (m_pCVar->GetType() == CVAR_INT)
			{
				m_pCVar->Set(pProfileOptions->GetOptionValueAsInt(optionName.c_str(), useDefault));
			}
			else if (m_pCVar->GetType() == CVAR_FLOAT)
			{
				m_pCVar->Set(pProfileOptions->GetOptionValueAsFloat(optionName.c_str(), useDefault) * m_profileMultiplyer);
			}
			else if (m_pCVar->GetType() == CVAR_STRING)
			{
				m_pCVar->Set(pProfileOptions->GetOptionValue(optionName.c_str(), useDefault));
			}
		}
		else
		{
			if (m_pCVar->GetType() == CVAR_INT)
			{
				m_pCVar->Set(m_iDefault);
			}
			else if (m_pCVar->GetType() == CVAR_FLOAT)
			{
				m_pCVar->Set(m_fDefault);
			}
			else if (m_pCVar->GetType() == CVAR_STRING)
			{
				m_pCVar->Set("");
			}
		}
	}
}

//---------------------------------------
void SGameModeOption::CopyToProfile( CProfileOptions *pProfileOptions, const char *pGameModeName )
{
	if (m_pCVar && !m_profileOption.empty())
	{
		TFixedString optionName;
		if (m_gameModeSpecific)
		{
			optionName.Format(m_profileOption.c_str(), pGameModeName);
		}
		else
		{
			optionName = m_profileOption.c_str();
		}

		if (m_pCVar->GetType() == CVAR_INT)
		{
			pProfileOptions->SetOptionValue(optionName.c_str(), m_pCVar->GetIVal());
		}
		else if (m_pCVar->GetType() == CVAR_FLOAT)
		{
			pProfileOptions->SetOptionValue(optionName.c_str(), int_round(m_pCVar->GetFVal() / m_profileMultiplyer));
		}
	}
}

//---------------------------------------
void SGameModeOption::SetInt( ICVar *pCvar, const char *pProfileOption, bool gameModeSpecific, int iDefault )
{
	SetCommon(pCvar, pProfileOption, gameModeSpecific, 1.f, 1.f);
	m_iDefault = iDefault;
}

//---------------------------------------
void SGameModeOption::SetFloat( ICVar *pCvar, const char *pProfileOption, bool gameModeSpecific, float netMultiplyer, float profileMultiplyer, float fDefault, int floatPrecision )
{
	SetCommon(pCvar, pProfileOption, gameModeSpecific, netMultiplyer, profileMultiplyer);
	m_fDefault = fDefault;
	m_floatPrecision = floatPrecision;
}

//---------------------------------------
void SGameModeOption::SetString( ICVar *pCvar, const char *pProfileOption, bool gameModeSpecific )
{
	SetCommon(pCvar, pProfileOption, gameModeSpecific, 1.f, 1.f);
}


//---------------------------------------
void SGameModeOption::SetCommon( ICVar *pCvar, const char *pProfileOption, bool gameModeSpecific, float netMultiplyer, float profileMultiplyer )
{
	m_pCVar = pCvar;
	m_profileOption = pProfileOption;
	m_gameModeSpecific = gameModeSpecific;
	m_netMultiplyer = netMultiplyer;
	m_profileMultiplyer = profileMultiplyer;
}

//=======================================
// CPlaylistManager

//---------------------------------------
CPlaylistManager::CPlaylistManager()
{
	CRY_ASSERT_MESSAGE(!s_pPlaylistManager, "There should only ever be one Playlist Manager - and there's one already!");
	s_pPlaylistManager = this;

	m_playlists.resize(0);
	SetCurrentPlaylist(0);
	m_inited = false;
	m_defaultVariantIndex = -1;
	m_customVariantIndex = -1;
	m_activeVariantIndex = -1;
	m_bIsSettingOptions = false;

	if (gEnv->IsDedicated())
	{
		gEnv->pConsole->AddConsoleVarSink(this);
	}
}

//---------------------------------------
CPlaylistManager::~CPlaylistManager()
{
	LOCAL_WARNING(!m_inited, "PlaylistManager is being deleted before being de-initialised - unless this is a proper game shutdown, cvars (etc.) will be being leaked");
	s_pPlaylistManager = NULL;

	if (gEnv->IsDedicated())
	{
		gEnv->pConsole->RemoveConsoleVarSink(this);
	}

	int numRestrictions = m_optionRestrictions.size();
	for (int i = 0; i < numRestrictions; ++ i)
	{
		if (m_optionRestrictions[i].m_operand1.m_pVar)
		{
			m_optionRestrictions[i].m_operand1.m_pVar->SetOnChangeCallback(NULL);
		}
		if (m_optionRestrictions[i].m_operand2.m_pVar)
		{
			m_optionRestrictions[i].m_operand2.m_pVar->SetOnChangeCallback(NULL);
		}
	}

	Deinit();
}

//---------------------------------------
void CPlaylistManager::Init(const char* pPath)
{
	CRY_ASSERT(!m_inited);

	AddPlaylistsFromPath(pPath);
	AddVariantsFromPath(pPath);

	// Add the custom variant
	SGameVariant customVariant;
	customVariant.m_allowInCustomGames = true;
	customVariant.m_supportsAllModes = true;
	customVariant.m_name = PLAYLIST_MANAGER_CUSTOM_NAME;
	customVariant.m_localName = "@ui_variant_Custom";
	customVariant.m_localDescription = "@ui_tooltip_variant_Custom";
	customVariant.m_localDescriptionUpper = "@ui_tooltip_variant_Custom_upper";
	customVariant.m_id = m_variants.size();
	customVariant.m_enabled = true;
	customVariant.m_requiresUnlock = false;

	m_variants.push_back(customVariant);
	m_customVariantIndex = customVariant.m_id;

#if !defined(_RELEASE)
		REGISTER_COMMAND("playlists_list", CmdPlaylistsList, VF_CHEAT, "List all available playlists (and show current)");
		REGISTER_COMMAND("playlists_choose", CmdPlaylistsChoose, VF_CHEAT, "Choose a listed playlist to be the current playlist");
		REGISTER_COMMAND("playlists_unchoose", CmdPlaylistsUnchoose, VF_CHEAT, "Unchoose the current playlist (ie. leaves none chosen)");
		REGISTER_COMMAND("playlists_show_variants", CmdPlaylistsShowVariants, VF_CHEAT, "Show all the variants available for current playlist (and show the active)");
		REGISTER_COMMAND("playlists_select_variant", CmdPlaylistsSelectVariant, VF_CHEAT, "Select a variant to be active for the current playlist");
#endif

	m_inited = true;

	// Add gamemode specific options
	AddGameModeOptionInt("g_scoreLimit", "MP/Options/%s/ScoreLimit", true, 75 );
	AddGameModeOptionFloat("g_timelimit", "MP/Options/%s/TimeLimit", true, 0.1f, 1.f, 10.f, 1);
	AddGameModeOptionInt("g_minplayerlimit", "MP/Options/%s/MinPlayers", true, DEFAULT_MINPLAYERLIMIT);	
	AddGameModeOptionFloat("g_autoReviveTime", "MP/Options/%s/RespawnDelay", true, 1.f, 1.f, 9.f, 0);
	AddGameModeOptionInt("g_roundlimit", "MP/Options/%s/NumberOfRounds", true, 2);
	AddGameModeOptionInt("g_numLives", "MP/Options/%s/NumberOfLives", true, 0);

	// Add global options
	AddGameModeOptionFloat("g_maxHealthMultiplier", "MP/Options/MaximumHealth", false, 0.01f, 0.01f, 1.f, 0);
	AddGameModeOptionInt("g_mpRegenerationRate", "MP/Options/HealthRegen", false, 1);
	AddGameModeOptionFloat("g_friendlyfireratio", "MP/Options/FriendlyFire", false, 0.01f, 0.01f, 0.f, 0);
	AddGameModeOptionInt("g_mpHeadshotsOnly", "MP/Options/HeadshotsOnly", false, 0);
	AddGameModeOptionInt("g_mpNoVTOL", "MP/Options/NoVTOL", false, 0);
	AddGameModeOptionInt("g_mpNoEnvironmentalWeapons", "MP/Options/NoEnvWeapons", false, 0);
	AddGameModeOptionInt("g_allowCustomLoadouts", "MP/Options/AllowCustomClasses", false, 1);
	AddGameModeOptionInt("g_allowFatalityBonus", "MP/Options/AllowFatalityBonus", false, 1);
	AddGameModeOptionInt("g_modevarivar_proHud", "MP/Options/ProHUD", false, 0);
	AddGameModeOptionInt("g_modevarivar_disableKillCam", "MP/Options/DisableKillcam", false, 0);
	AddGameModeOptionInt("g_modevarivar_disableSpectatorCam", "MP/Options/DisableSpectatorCam", false, 0);
	//AddGameModeOptionFloat("g_xpMultiplyer", "", false, 1.f, 1.f, 1.f, 0);		// Don't link this to a profile attribute to prevent it being used in custom variants
	AddGameModeOptionInt("g_allowExplosives", "MP/Options/AllowExplosives", false, 1);
	AddGameModeOptionInt("g_forceWeapon", "MP/Options/ForceWeapon", false, -1);
	AddGameModeOptionInt("g_infiniteAmmo", "MP/Options/InfiniteAmmo", false, 0);
	AddGameModeOptionInt("g_infiniteCloak", "MP/Options/InfiniteCloak", false, 0);
	AddGameModeOptionInt("g_allowWeaponCustomisation", "MP/Options/AllowWeaponCustomise", false, 1);
	AddGameModeOptionString("g_forceHeavyWeapon", "MP/Options/ForceHeavyWeapon", false);
	AddGameModeOptionString("g_forceLoadoutPackage", "MP/Options/ForceLoadoutPackage", false);

	AddConfigVar("g_autoAssignTeams", true);
	AddConfigVar("gl_initialTime", true);
	AddConfigVar("gl_time", true);
	AddConfigVar("g_gameRules_startTimerLength", true);
	AddConfigVar("sv_maxPlayers", false);
	AddConfigVar("g_forceHeavyWeapon", true);
	AddConfigVar("g_forceLoadoutPackage", true);
	AddConfigVar("g_switchTeamAllowed", true);
	AddConfigVar("g_switchTeamRequiredPlayerDifference", true);
	AddConfigVar("g_switchTeamUnbalancedWarningDifference", true);
	AddConfigVar("g_switchTeamUnbalancedWarningTimer", true);

	// First time through here, register startPlaylist command and check to see if we need to start straight away
	static bool bOnlyOnce = true;
	if (bOnlyOnce)
	{
		CRY_ASSERT(gEnv);
		IConsole *pConsole = gEnv->pConsole;
		if (pConsole)
		{
			bOnlyOnce = false;

			// Create a temporary console variable so that the startPlaylist command can be used in a config
			ICVar *pInitialPlaylist = REGISTER_STRING("startPlaylist", "", VF_NULL, "");
			if (pInitialPlaylist->GetFlags() & VF_MODIFIED)
			{
				DoStartPlaylistCommand(pInitialPlaylist->GetString());
			}
			pConsole->UnregisterVariable("startPlaylist", true);

			// Now register the actual command
			REGISTER_COMMAND("startPlaylist", CmdStartPlaylist, VF_NULL, "Start the specified playlist/variant");
			pConsole->RegisterAutoComplete("startPlaylist", &gl_PlaylistVariantAutoComplete);
		}
	}

	LoadOptionRestrictions();

#if USE_DEDICATED_LEVELROTATION
	if (gEnv->IsDedicated())
	{
		LoadLevelRotation();
	}
#endif
}

//---------------------------------------
void CPlaylistManager::AddGameModeOptionInt( const char *pCVarName, const char *pProfileOption, bool gameModeSpecific, int iDefault )
{
	CRY_ASSERT(gEnv);
	m_options.push_back(SGameModeOption());
	SGameModeOption &newOption = m_options[m_options.size() - 1];
	ICVar *pCVar = gEnv->pConsole->GetCVar(pCVarName); 
	CRY_ASSERT(pCVar); 
	newOption.SetInt(pCVar, pProfileOption, gameModeSpecific, iDefault); 
	if (!pCVar)
	{
		GameWarning("PlaylistManager: Failed to find cvar %s", pCVarName);
	}
}

//---------------------------------------
void CPlaylistManager::AddGameModeOptionFloat( const char *pCVarName, const char *pProfileOption, bool gameModeSpecific, float netMultiplyer, float profileMultiplyer, float fDefault, int floatPrecision )
{
	CRY_ASSERT(gEnv);
	m_options.push_back(SGameModeOption());
	SGameModeOption &newOption = m_options[m_options.size() - 1];
	ICVar *pCVar = gEnv->pConsole->GetCVar(pCVarName); 
	CRY_ASSERT(pCVar); 
	newOption.SetFloat(pCVar, pProfileOption, gameModeSpecific, netMultiplyer, profileMultiplyer, fDefault, floatPrecision); 
	if (!pCVar)
	{
		GameWarning("PlaylistManager: Failed to find cvar %s", pCVarName);
	}
}

//---------------------------------------
void CPlaylistManager::AddGameModeOptionString( const char *pCVarName, const char *pProfileOption, bool gameModeSpecific )
{
	CRY_ASSERT(gEnv);
	m_options.push_back(SGameModeOption());
	SGameModeOption &newOption = m_options[m_options.size() - 1];
	ICVar *pCVar = gEnv->pConsole->GetCVar(pCVarName); 
	CRY_ASSERT(pCVar); 
	newOption.SetString(pCVar, pProfileOption, gameModeSpecific); 
	if (!pCVar)
	{
		GameWarning("PlaylistManager: Failed to find cvar %s", pCVarName);
	}
}

//---------------------------------------
SGameModeOption *CPlaylistManager::GetGameModeOptionStruct( const char *pOptionName )
{
	CRY_ASSERT(gEnv);
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	IGameRulesSystem *pGameRulesSystem = gEnv->pGameFramework->GetIGameRulesSystem();
	const char * sv_gamerules = gEnv->pConsole->GetCVar("sv_gamerules")->GetString();

	const char * fullGameRulesName = (pGameLobby && pGameLobby->IsCurrentlyInSession()) ? pGameLobby->GetCurrentGameModeName() : pGameRulesSystem->GetGameRulesName(sv_gamerules);

	SGameModeOption *pResult = NULL;

	if (fullGameRulesName)
	{
		CryFixedStringT<32> gameModeOptionName;
		const int numOptions = m_options.size();
		for (int i = 0; i < numOptions; ++ i)
		{
			SGameModeOption &option = m_options[i];
			if (option.m_gameModeSpecific)
			{
				gameModeOptionName.Format(option.m_profileOption.c_str(), fullGameRulesName);
				if (!stricmp(gameModeOptionName.c_str(), pOptionName))
				{
					pResult = &option;
					break;
				}
			}
			else
			{
				if (!stricmp(option.m_profileOption.c_str(), pOptionName))
				{
					pResult = &option;
					break;
				}
			}
		}
	}
	return pResult;
}

//---------------------------------------
void CPlaylistManager::AddConfigVar(const char *pCVarName, bool bNetSynched)
{
	ICVar *pCVar = gEnv->pConsole->GetCVar(pCVarName);
	if (pCVar)
	{
		SConfigVar var;
		var.m_pCVar = pCVar;
		var.m_bNetSynched = bNetSynched;
		m_configVars.push_back(var);
	}
}

//---------------------------------------
void CPlaylistManager::Deinit()
{
	if (m_inited)
	{
#if !defined(_RELEASE)
		if (gEnv && gEnv->pConsole)
		{
			gEnv->pConsole->RemoveCommand("playlists_list");
			gEnv->pConsole->RemoveCommand("playlists_choose");
			gEnv->pConsole->RemoveCommand("playlists_unchoose");
			gEnv->pConsole->RemoveCommand("playlists_show_variants");
			gEnv->pConsole->RemoveCommand("playlists_select_variant");
		}
#endif

		if (ILevelSystem* pLevelSystem=g_pGame->GetIGameFramework()->GetILevelSystem())
		{
			pLevelSystem->ClearExtendedLevelRotations();
		}

		ClearAllVariantCVars();

		m_inited = false;
	}
}

//---------------------------------------
void CPlaylistManager::ClearAllVariantCVars()
{
	const int numOptions = m_options.size();
	for (int i = 0; i < numOptions; ++ i)
	{
		SGameModeOption &option = m_options[i];
		if (option.m_pCVar)
		{
#ifdef _DEBUG
			if (option.m_pCVar->GetFlags() & VF_WASINCONFIG)
			{
				// Allow gamemode options to be overridden in user.cfg when in debug mode
				continue;
			}
#endif
			if (option.m_pCVar->GetType() == CVAR_INT)
			{
				option.m_pCVar->Set(option.m_iDefault);
			}
			else if (option.m_pCVar->GetType() == CVAR_FLOAT)
			{
				option.m_pCVar->Set(option.m_fDefault);
			}
			else if (option.m_pCVar->GetType() == CVAR_STRING)
			{
				option.m_pCVar->Set("");
			}
		}
	}
}

//---------------------------------------
void CPlaylistManager::SaveCurrentSettingsToProfile()
{
	CProfileOptions *pProfileOptions = g_pGame->GetProfileOptions();
	CRY_ASSERT(pProfileOptions);
	if (pProfileOptions)
	{
		CGameLobby *pGameLobby = g_pGame->GetGameLobby();
		CRY_ASSERT(gEnv);
		IGameRulesSystem *pGameRulesSystem = gEnv->pGameFramework->GetIGameRulesSystem();
		const char * sv_gamerules = gEnv->pConsole->GetCVar("sv_gamerules")->GetString();

		const char * fullGameRulesName = (pGameLobby && pGameLobby->IsCurrentlyInSession()) ? pGameLobby->GetCurrentGameModeName() : pGameRulesSystem->GetGameRulesName(sv_gamerules);

		CRY_ASSERT(fullGameRulesName);
		if (fullGameRulesName)
		{
			const int numOptions = m_options.size();
			for (int i = 0; i < numOptions; ++ i)
			{
				SGameModeOption &option = m_options[i];
				option.CopyToProfile(pProfileOptions, fullGameRulesName);
			}
		}
	}
}

//---------------------------------------
void CPlaylistManager::OnPromoteToServer()
{
	// If we take ownership of a game using a custom variant, we overwrite our settings with the game settings
	// so that they aren't lost
	if (m_activeVariantIndex == m_customVariantIndex)
	{
		SaveCurrentSettingsToProfile();
	}
}

//---------------------------------------
bool CPlaylistManager::HaveUnlockedPlaylist(const SPlaylist *pPlaylist, CryFixedStringT<128>* pUnlockString/*=NULL*/)
{
	bool result = true;
	if (pPlaylist && pPlaylist->RequiresUnlocking())
	{
		CPlayerProgression *pPlayerProgression = CPlayerProgression::GetInstance();
		if (pPlayerProgression)
		{
			SPPHaveUnlockedQuery results;
			pPlayerProgression->HaveUnlocked(eUT_Playlist, pPlaylist->GetUniqueName(), results);

			if (results.exists && results.unlocked == false)
			{
				if (pUnlockString)
				{
					*pUnlockString = results.unlockString.c_str();
				}
				result = false;
			}
		}
	}

	return result;
}

//---------------------------------------
bool CPlaylistManager::HaveUnlockedPlaylistVariant(const SPlaylist *pPlaylist, const int variantId, CryFixedStringT<128>* pUnlockString/*=NULL*/)
{
	bool result = true;

	const SGameVariant* pVariant = GetVariant(variantId);
	if (pPlaylist && pVariant && pVariant->m_requiresUnlock)
	{
		CryFixedStringT<32> variantUnlockName;
		variantUnlockName.Format("%s", pVariant->m_name.c_str());

		CPlayerProgression *pPlayerProgression = CPlayerProgression::GetInstance();
		if (pPlayerProgression)
		{
			SPPHaveUnlockedQuery results;
			pPlayerProgression->HaveUnlocked(eUT_Playlist, variantUnlockName.c_str(), results);

			if (results.exists && results.unlocked == false)
			{
				if (pUnlockString)
				{
					*pUnlockString = results.unlockString.c_str();
				}
				result = false;
			}
		}
	}

	return result;
}

//---------------------------------------
void CPlaylistManager::SetCurrentPlaylist(const ILevelRotation::TExtInfoId id)
{
	m_currentPlaylistId = id;
	if (!m_variants.empty())
	{
		CRY_ASSERT_MESSAGE((m_defaultVariantIndex != -1), "Failed to find a default variant");
		SetActiveVariant(m_defaultVariantIndex);
	}

	int maxPlayers = MAX_PLAYER_LIMIT;

	const SPlaylist *pPlaylist = GetCurrentPlaylist();
	if (pPlaylist)
	{
		maxPlayers = pPlaylist->rotExtInfo.m_maxPlayers;
	}

	ICVar* pMaxPlayersCVar = gEnv->pConsole->GetCVar("sv_maxplayers");
	if (pMaxPlayersCVar)
	{
		pMaxPlayersCVar->Set(maxPlayers);
	}
}

//---------------------------------------
void  CPlaylistManager::SetActiveVariant(int variantIndex)
{
	CRY_ASSERT((variantIndex >= 0) && (variantIndex < (int)m_variants.size()));
	m_activeVariantIndex = variantIndex;

	SetModeOptions();
}

//---------------------------------------
ILevelRotation* CPlaylistManager::GetRotationForCurrentPlaylist()
{
	CryLog("CPlaylistManager::GetRotationForCurrentPlaylist()");

	ILevelRotation*  pRot = NULL;

	ILevelSystem*  pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
	CRY_ASSERT(pLevelSystem);

	if (HavePlaylistSet())
	{
		if (SPlaylist* pList=FindPlaylistById(m_currentPlaylistId))
		{
			pRot = pLevelSystem->FindLevelRotationForExtInfoId(pList->id);
		}
		else
		{
			CryLog("  Error: couldn't find playlist matching m_currentPlaylistId '%u', so returning NULL", m_currentPlaylistId);
		}
	}
	else
	{
		CryLog("  m_currentPlaylistId is not set, so returning NULL");
	}

	return pRot;
}

//---------------------------------------
ILevelRotation* CPlaylistManager::GetLevelRotation()
{
	ILevelRotation*  lrot = NULL;
	if (HavePlaylistSet())
	{
		lrot = GetRotationForCurrentPlaylist();
		CRY_ASSERT(lrot);
	}
	else
	{
		ILevelSystem*  pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
		CRY_ASSERT(pLevelSystem);
		lrot = pLevelSystem->GetLevelRotation();
	}
	return lrot;
}

//---------------------------------------
bool CPlaylistManager::DisablePlaylist(const char* uniqueName)
{
	bool  disabled = false;

	if (SPlaylist* pList=FindPlaylistByUniqueName(uniqueName))
	{
		pList->SetEnabled(false);
		CryLog("CPlaylistManager::DisablePlaylist: disabled playlist '%s'", uniqueName);

		if (m_currentPlaylistId == pList->id)
		{
			CryLog("... playlist being disabled was set as current, so un-setting current");
			SetCurrentPlaylist(0);
		}

		disabled = true;
	}

	return disabled;
}

//---------------------------------------
void CPlaylistManager::AddPlaylistsFromPath(const char* pPath)
{
	CryLog ("Adding playlists from path '%s'", pPath);
	INDENT_LOG_DURING_SCOPE();

	CryStringLocal  indexPath;
	indexPath.Format("%s/PlaylistIndex.xml", pPath);

	if (XmlNodeRef indexRoot=GetISystem()->LoadXmlFromFile(indexPath))
	{
		CRY_ASSERT(gEnv);
		int  n = indexRoot->getChildCount();
		for (int i=0; i<n; i++)
		{
			if (XmlNodeRef child=indexRoot->getChild(i))
			{
				const char*  pTag = child->getTag();
				if (stricmp(pTag, "PlaylistFile") == 0)
				{
					bool allow = true;
					int allowVal = 1;

					if (child->getAttr("EPD", allowVal))
					{
						//allow = ((allowVal&g_pGameCVars->g_EPD)!=0);
						allow = (allowVal == g_pGameCVars->g_EPD);
						CryLog ("Playlist '%s' EPD=%u so %s", child->getAttr("name"), allowVal, allow ? "valid" : "invalid");
					}
					else
					{
						allow = (g_pGameCVars->g_EPD==0);
					}

					if (const char* pName = allow ? child->getAttr("name") : NULL)
					{
						CryStringLocal  filePath;
						filePath.Format("%s/%s", pPath, pName);
						
						if (XmlNodeRef fileNode=gEnv->pSystem->LoadXmlFromFile(filePath))
						{
							AddPlaylistFromXmlNode(fileNode);
						}
						else
						{
							LOCAL_WARNING(0, string().Format("Failed to load '%s'!", filePath.c_str()).c_str());
						}
					}
					else
					{
						LOCAL_WARNING(!allow, string().Format("PlaylistFile element '%s' in '%s' didn't have a \"name\" attribute", pTag, indexPath.c_str()).c_str());
					}
				}
				else
				{
					LOCAL_WARNING(0, string().Format("Unexpected xml child '%s' in '%s'", pTag, indexPath.c_str()).c_str());
				}

			}

		}

	}
	else
	{
		LOCAL_WARNING(0, string().Format("Couldn't open playlists index file '%s'", indexPath.c_str()).c_str());
	}

}

//---------------------------------------
ILevelRotation::TExtInfoId CPlaylistManager::GetPlaylistId( const char *pUniqueName ) const
{
	const ILevelRotation::TExtInfoId  genId = CCrc32::ComputeLowercase(pUniqueName);
	return genId;
}

//---------------------------------------
void CPlaylistManager::AddPlaylistFromXmlNode(XmlNodeRef xmlNode)
{
	m_playlists.push_back(SPlaylist());
	SPlaylist*  p = &m_playlists.back();

	p->Reset();

	if (p->rotExtInfo.LoadFromXmlNode(xmlNode))
	{
		CRY_ASSERT(gEnv);
		const ILevelRotation::TExtInfoId  genId = GetPlaylistId(p->rotExtInfo.uniqueName.c_str());

		if (!FindPlaylistById(genId))
		{
			ILevelSystem*  pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
			CRY_ASSERT(pLevelSystem);

			if (pLevelSystem->AddExtendedLevelRotationFromXmlRootNode(xmlNode, "playlist", genId))
			{
				p->id = genId;
				CRY_ASSERT(FindPlaylistById(genId));
				CryLog("CPlaylistManager::AddPlaylistFromXmlNode: added new playlist '%s' (enabled: %d) and gave it an id of '%u'", p->rotExtInfo.uniqueName.c_str(), p->IsEnabled(), p->id);

				ILevelRotation*  pRot = pLevelSystem->FindLevelRotationForExtInfoId(genId);
				CRY_ASSERT_TRACE(pRot, ("Cannot find newly added extended rotation '%s' with id '%u'", p->rotExtInfo.uniqueName.c_str(), genId));
			}
			else
			{
				LOCAL_WARNING(0, "Failed to add extended level rotation!");
				m_playlists.pop_back();
			}
		}
		else
		{
			LOCAL_WARNING(0, string().Format("Error adding loaded playlist as a playlist with id '%u' has already been added!", genId).c_str());
			m_playlists.pop_back();
		}
	}
	else
	{
		LOCAL_WARNING(0, "Failed to load extended level rotation info from xml");
		m_playlists.pop_back();
	}
}

//---------------------------------------
SPlaylist* CPlaylistManager::FindPlaylistById(ILevelRotation::TExtInfoId findId)
{
	SPlaylist*  pList = NULL;

	TPlaylists::iterator  begin = m_playlists.begin();
	TPlaylists::iterator  end = m_playlists.end();
	for (TPlaylists::iterator i = begin; i != end; ++i)
	{
		SPlaylist*  pIterList = &(*i);
		if (pIterList->id == findId)
		{
			pList = pIterList;
			break;
		}
	}

	return pList;
}

//---------------------------------------
SPlaylist* CPlaylistManager::FindPlaylistByUniqueName(const char* uniqueName)
{
	CRY_ASSERT(gEnv);
	const ILevelRotation::TExtInfoId  findId = GetPlaylistId(uniqueName);
	return FindPlaylistById(findId);
}

//---------------------------------------
const int CPlaylistManager::GetNumPlaylists() const
{
	return m_playlists.size();
}

//---------------------------------------
const SPlaylist* CPlaylistManager::GetPlaylist(const int index)
{
	if ((index >= 0) && (index < (int)m_playlists.size()))
	{
		SPlaylist *pList = &m_playlists[index];
		pList->ResolveVariants();
		return pList;
	}

	return NULL;
}

//---------------------------------------
const SPlaylist* CPlaylistManager::GetCurrentPlaylist()
{
	if (HavePlaylistSet())
	{
		SPlaylist *pList = FindPlaylistById(m_currentPlaylistId);
		if (pList)
		{
			pList->ResolveVariants();
			return pList;
	}
	}

	return NULL;
}


//---------------------------------------
int CPlaylistManager::GetPlaylistIndex( ILevelRotation::TExtInfoId findId ) const
{
	int iFound = -1;
	for( uint iPlaylist = 0; iPlaylist < m_playlists.size(); ++iPlaylist )
	{
		if( m_playlists[ iPlaylist ].id == findId )
		{
			iFound = iPlaylist;
			break;
		}
	}
	return iFound;
}

//---------------------------------------
const char *CPlaylistManager::GetActiveVariant()
{
	const char *pResult = "";

	if (m_activeVariantIndex >= 0)
	{
		pResult = m_variants[m_activeVariantIndex].m_name.c_str();
	}

	return pResult;
}

//---------------------------------------
const int CPlaylistManager::GetActiveVariantIndex() const
{
	return m_activeVariantIndex;
}

//---------------------------------------
const char* CPlaylistManager::GetVariantName(const int index)
{
	const char *pResult = NULL;

	if ((index >= 0) && (index < (int)m_variants.size()))
	{
		pResult = m_variants[index].m_name.c_str();
	}

	return pResult;
}

//---------------------------------------
int CPlaylistManager::GetVariantIndex(const char *pName)
{
	int result = -1;

	const int numVariants = m_variants.size();
	for (int i = 0; i < numVariants; ++ i)
	{
		if (!stricmp(m_variants[i].m_name.c_str(), pName))
		{
			result = i;
			break;
		}
	}

	return result;
}

//---------------------------------------
const int CPlaylistManager::GetNumVariants() const
{
	return m_variants.size();
}

//---------------------------------------
bool CPlaylistManager::ChoosePlaylist(const int chooseIdx)
{
	const int  n = m_playlists.size();
	if ((chooseIdx >= 0) && (chooseIdx < n))
	{
		SPlaylist*  pList = &m_playlists[chooseIdx];
		const SPlaylistRotationExtension*  pInfo = &pList->rotExtInfo;

		if (pInfo->m_enabled)
		{
			SetCurrentPlaylist(pList->id);

			// Make sure the gamerules are set before the variant so we get correct player limit
			ILevelRotation *pRotation = GetRotationForCurrentPlaylist();
			if (pRotation)
			{
				ICVar *pCVar = gEnv->pConsole->GetCVar("sv_gamerules");
				pCVar->Set(pRotation->GetNextGameRules());
			}

			return true;
		}
		else
		{
			CryLog("  Error: that playlist cannot be chosen as the current because it is NOT ENABLED");
		}
	}
	else
	{
		CryLog("  Error: playlist index '%d' is OUT OF RANGE. It should be >= 0 and < %d", chooseIdx, n);
	}

	return false;
}

//---------------------------------------
bool CPlaylistManager::ChoosePlaylistById( const ILevelRotation::TExtInfoId playlistId )
{
	SPlaylist *pPlaylist = FindPlaylistById(playlistId);
	if (pPlaylist && pPlaylist->IsEnabled())
	{
		SetCurrentPlaylist(playlistId);
		return true;
	}
	else
	{
		// TODO: Replace this assert with a "you can't join this game" message
		CRY_ASSERT(!"Failed to choose playlist by Id, playlist not found or not enabled");
		SetCurrentPlaylist(0);
		return false;
	}
}

//---------------------------------------
bool CPlaylistManager::ChooseVariant(int selectIdx)
{
	if (HavePlaylistSet())
	{
		if (SPlaylist* pList=FindPlaylistById(m_currentPlaylistId))
		{
			if (selectIdx >= 0)
				{
					SetActiveVariant(selectIdx);
					return true;
				}
				else
				{
				CryLog("  Error: variant index '%d' is OUT OF RANGE. It should be >= 0 and < %" PRISIZE_T, selectIdx, m_variants.size());
			}
		}
		else
		{
			CryLog("  Error: couldn't find playlist matching m_currentPlaylistId '%u', so cannot select a variant.", m_currentPlaylistId);
		}
	}
	else
	{
		// No playlist set, must be in a private match, all variants are allowed
		SetActiveVariant(selectIdx);
		return true;
	}

	return false;
}

//---------------------------------------
void CPlaylistManager::SetModeOptions()
{
	CGameLobby *pLobby = g_pGame->GetGameLobby();
	if (pLobby)
	{
		if ((m_activeVariantIndex == m_customVariantIndex) && (pLobby->IsServer() == false))
		{
			// If we're in the custom variant, bail so we don't modify the settings (otherwise we wouldn't know what to set them back to!)
			// Note: Server still needs to set the cvars since it collects them from the profile - just clients that can't
			return;
		}

		m_bIsSettingOptions = true;

		if (CProfileOptions *pProfileOptions = g_pGame->GetProfileOptions())
		{
			// Use default settings if we're on an actual variant, use profile settings for custom variant
			bool useDefaults = (m_activeVariantIndex != m_customVariantIndex);

			IGameRulesSystem *pGameRulesSystem = gEnv->pGameFramework->GetIGameRulesSystem();
			const char * sv_gamerules = gEnv->pConsole->GetCVar("sv_gamerules")->GetString();

			const char * fullGameRulesName = (pLobby && pLobby->IsCurrentlyInSession()) ? pLobby->GetCurrentGameModeName(sv_gamerules) : pGameRulesSystem->GetGameRulesName(sv_gamerules);

			if (fullGameRulesName)
			{
				const int numOptions = m_options.size();
				for (int i = 0; i < numOptions; ++ i)
				{
					SGameModeOption &option = m_options[i];
					option.CopyToCVar(pProfileOptions, useDefaults, fullGameRulesName);
				}
			}

		}

		IConsole *pConsole = gEnv->pConsole;
		if (pConsole && (m_activeVariantIndex >= 0))
		{
			SGameVariant &activeVariant = m_variants[m_activeVariantIndex];

			const int numOptions = activeVariant.m_options.size();
			for (int i = 0; i < numOptions; ++ i)
			{
				const char *pOption = activeVariant.m_options[i].c_str();
				pConsole->ExecuteString(pOption);
			}
		}

		pLobby->OnOptionsChanged();

		m_bIsSettingOptions = false;
	}
}

//---------------------------------------
void CPlaylistManager::AddVariantsFromPath(const char* pPath)
{
	CryStringLocal  indexPath;
	indexPath.Format("%s/VariantsIndex.xml", pPath);

	if (XmlNodeRef indexRoot=GetISystem()->LoadXmlFromFile(indexPath))
	{
		int  n = indexRoot->getChildCount();
		for (int i=0; i<n; i++)
		{
			if (XmlNodeRef child=indexRoot->getChild(i))
			{
				const char*  pTag = child->getTag();
				if (stricmp(pTag, "VariantsFile") == 0)
				{
					if (const char* pName=child->getAttr("name"))
					{
						CryStringLocal  filePath;
						filePath.Format("%s/%s", pPath, pName);
						
						LoadVariantsFile(filePath.c_str());
					}
					else
					{
						LOCAL_WARNING(0, string().Format("VariantsFile element '%s' in '%s' didn't have a \"name\" attribute", pTag, indexPath.c_str()).c_str());
					}
				}
				else
				{
					LOCAL_WARNING(0, string().Format("Unexpected xml child '%s' in '%s'", pTag, indexPath.c_str()).c_str());
				}
			}
		}
	}
	else
	{
		LOCAL_WARNING(0, string().Format("Couldn't open playlists index file '%s'", indexPath.c_str()).c_str());
	}

}

//---------------------------------------
void CPlaylistManager::LoadVariantsFile(const char *pPath)
{
	CRY_ASSERT(gEnv);
	if (XmlNodeRef xmlRoot = gEnv->pSystem->LoadXmlFromFile(pPath))
	{
		const int numChildren = xmlRoot->getChildCount();
		for (int i = 0; i < numChildren; ++ i)
		{
			XmlNodeRef xmlChild = xmlRoot->getChild(i);
			AddVariantFromXmlNode(xmlChild);
		}
	}
	else
	{
		LOCAL_WARNING(0, string().Format("Failed to load '%s'!", "%s", pPath).c_str());
	}
}

//---------------------------------------
void CPlaylistManager::AddVariantFromXmlNode( XmlNodeRef xmlNode )
{
	if (!stricmp(xmlNode->getTag(), "Variant"))
	{
		const char *pName = 0;
		if (xmlNode->getAttr("name", &pName))
		{
			SGameVariant newVariant;

			newVariant.m_name = pName;

			const char*  pCurrentLang = gEnv->pSystem->GetLocalizationManager()->GetLanguage();
			CRY_ASSERT(pCurrentLang);

			CPlaylistManager::GetLocalisationSpecificAttr<SGameVariant::TFixedString>(xmlNode, "localName", pCurrentLang, &newVariant.m_localName);

			CPlaylistManager::GetLocalisationSpecificAttr<SGameVariant::TLongFixedString>(xmlNode, "localDescription", pCurrentLang, &newVariant.m_localDescription);

			CPlaylistManager::GetLocalisationSpecificAttr<SGameVariant::TLongFixedString>(xmlNode, "localDescriptionUpper", pCurrentLang, &newVariant.m_localDescriptionUpper);
			if (newVariant.m_localDescriptionUpper.empty())
			{
				if (!newVariant.m_localDescription.empty() && (newVariant.m_localDescription.at(0) == '@'))
				{
					newVariant.m_localDescriptionUpper.Format("%s_upper", newVariant.m_localDescription.c_str());
				}
				else
				{
					LOCAL_WARNING(0, string().Format("AddVariantFromXmlNode: no \"localDescriptionUpper\" attribute inside variant '%s', and can't auto-reference a string-table entry based on the \"localDescription\" attribute because that attribute is set explicitly as opposed to being a \"@xxxxx\"-style string-table reference. Will copy the lowercase attribute, as that's probably better than nothing!", newVariant.m_name.c_str()).c_str());
					newVariant.m_localDescriptionUpper = newVariant.m_localDescription;
				}
			}

			int iValue = 0;
			if (xmlNode->getAttr("enabled", iValue))
			{
				newVariant.m_enabled = (iValue != 0);
			}

			if (xmlNode->getAttr("requiresUnlocking", iValue))
			{
				newVariant.m_requiresUnlock = (iValue != 0);
			}

			if (xmlNode->getAttr("allowInCustomGames", iValue))
			{
				newVariant.m_allowInCustomGames = (iValue != 0);
			}

			if (xmlNode->getAttr("restrictRank", iValue))
			{
				newVariant.m_restrictRank = iValue;
			}

			if (xmlNode->getAttr("requireRank", iValue))
			{
				newVariant.m_requireRank = iValue;
			}

			if (xmlNode->getAttr("allowSquads", iValue))
			{
				newVariant.m_allowSquads = (iValue != 0);
			}

			if (xmlNode->getAttr("allowClans", iValue))
			{
				newVariant.m_allowClans = (iValue != 0);
			}

			XmlString stringValue;
			if (xmlNode->getAttr("imgsuffix", stringValue))
			{
				newVariant.m_suffix = stringValue.c_str();
			}

			bool isDefault = false;
			if (xmlNode->getAttr("default", iValue))
			{
				isDefault = (iValue != 0);
			}

			bool bOk = true;

			const int numChildren = xmlNode->getChildCount();
			for (int i = 0; i < numChildren; ++ i)
			{
				XmlNodeRef xmlChild = xmlNode->getChild(i);

				if (!stricmp(xmlChild->getTag(), "Options"))
				{
					const int numOptions = xmlChild->getChildCount();
					for (int j = 0; j < numOptions; ++ j)
					{
						XmlNodeRef xmlOption = xmlChild->getChild(j);
						if (!stricmp(xmlOption->getTag(), "Option"))
						{
							const char *pSetting = 0;
							if (xmlOption->getAttr("setting", &pSetting))
							{
								newVariant.m_options.push_back(pSetting);
							}
							else
							{
								LOCAL_WARNING(0, string().Format("AddVariantFromXmlNode: Expected 'setting' attribute, inside variant '%s'", newVariant.m_name.c_str()).c_str());
								bOk = false;
							}
						}
						else
						{
							LOCAL_WARNING(0, string().Format("AddVariantFromXmlNode: Expected 'Option' tag, got '%s' instead, inside variant '%s'", xmlOption->getTag(), newVariant.m_name.c_str()).c_str());
							bOk = false;
						}
					}
				}
				else if (!stricmp(xmlChild->getTag(), "Modes"))
				{
					newVariant.m_supportsAllModes = false;
					const int numModes = xmlChild->getChildCount();
					for (int j = 0; j < numModes; ++ j)
					{
						XmlNodeRef xmlMode = xmlChild->getChild(j);
						if (!stricmp(xmlMode->getTag(), "Mode"))
						{
							const char *pSetting = 0;
							if (xmlMode->getAttr("name", &pSetting))
							{
								int rulesId = 0;
								if (AutoEnum_GetEnumValFromString(pSetting, CGameRules::S_GetGameModeNamesArray(), eGM_NUM_GAMEMODES, &rulesId))
								{
									newVariant.m_supportedModes.push_back(rulesId);
								}
								else
								{
									LOCAL_WARNING(0, string().Format("AddVariantFromXmlNode: Unknown mode '%s' found, inside variant '%s'", pSetting, newVariant.m_name.c_str()).c_str());
									bOk = false;
								}
							}
						}
						else
						{
							LOCAL_WARNING(0, string().Format("AddVariantFromXmlNode: Expected 'Mode' tag, got '%s' instead, inside variant '%s'", xmlMode->getTag(), newVariant.m_name.c_str()).c_str());
							bOk = false;
						}
					}
				}
				else
				{
					LOCAL_WARNING(0, string().Format("AddVariantFromXmlNode: Unexpected '%s' tag, inside variant '%s'", xmlChild->getTag(), newVariant.m_name.c_str()).c_str());
					bOk = false;
				}
			}
			if (bOk)
			{
				newVariant.m_id = m_variants.size();
				m_variants.push_back(newVariant);

				if (isDefault)
				{
					m_defaultVariantIndex = newVariant.m_id;
				}
			}
		}
		else
		{
			LOCAL_WARNING(0, "AddVariantFromXmlNode: No name attribute specified");
		}
	}
	else
	{
		LOCAL_WARNING(0, string().Format("AddVariantFromXmlNode: Expected node tag to be 'Variant', got %s instead", xmlNode->getTag()).c_str());
	}
}

//---------------------------------------
SGameVariant *CPlaylistManager::FindVariantByName(const char *pName)
{
	SGameVariant *pResult = NULL;

	const int numVariants = m_variants.size();
	for (int i = 0; i < numVariants; ++ i)
	{
		SGameVariant *pVariant = &m_variants[i];
		if (!stricmp(pVariant->m_name.c_str(), pName))
		{
			pResult = pVariant;
		break;
		}
	}

	return pResult;
}

//---------------------------------------
bool CPlaylistManager::DisableVariant(const char *pName)
{
	bool result = false;
	if (SGameVariant *pVariant = FindVariantByName(pName))
	{
		pVariant->m_enabled = false;
		result = true;
	}
	return result;
}

//---------------------------------------
void CPlaylistManager::GetVariantsForGameMode( const char *pGameMode, TVariantsPtrVec &result )
{
	if ((!pGameMode) || !(pGameMode[0]))
	{
		return;
	}

	int gamemodeId = 0;
	if (!AutoEnum_GetEnumValFromString(pGameMode, CGameRules::S_GetGameModeNamesArray(), eGM_NUM_GAMEMODES, &gamemodeId))
	{
		return;
	}

	const int numVariants = m_variants.size();

	result.clear();
	result.reserve(numVariants);

	for (int i = 0; i < numVariants; ++ i)
	{
		SGameVariant *pVariant = &m_variants[i];
		if (pVariant->m_enabled && pVariant->m_allowInCustomGames && (pVariant->m_id != m_customVariantIndex))
		{
			if (pVariant->m_supportsAllModes)
			{
				result.push_back(pVariant);
			}
			else
			{
				const int numSupportedModes = pVariant->m_supportedModes.size();
				for (int j = 0; j < numSupportedModes; ++ j)
				{
					if (pVariant->m_supportedModes[j] == gamemodeId)
					{
						result.push_back(pVariant);
						break;
					}
				}
			}
		}
	}

	// Add custom variant last
	if (m_customVariantIndex != -1)
	{
		SGameVariant *pCustomVariant = &m_variants[m_customVariantIndex];
		result.push_back(pCustomVariant);
	}
}

//---------------------------------------
void CPlaylistManager::GetTotalPlaylistCounts(uint32 *outUnlocked, uint32 *outTotal)
{
	uint32 total = *outTotal;
	uint32 unlocked = *outUnlocked;

	const int num = m_playlists.size();
	if (num>0)
	{
		// Playlists
		for (int i=0; i<num; ++i)
		{
			const SPlaylist *pPlaylist = &m_playlists[i];
			if (pPlaylist && pPlaylist->IsEnabled() && pPlaylist->IsSelectable() && !pPlaylist->IsHidden())
			{
				// Variants
				const int numVariants = pPlaylist->rotExtInfo.m_supportedVariants.size();
				for (int j = 0; j < numVariants; ++j)
				{
					const char *pVariantName = pPlaylist->rotExtInfo.m_supportedVariants[j].m_name.c_str();
					int variantIndex = GetVariantIndex(pVariantName);
					
					if (const SGameVariant *pVariant = GetVariant(variantIndex))
					{
						++total;
						if (HaveUnlockedPlaylist(pPlaylist) && HaveUnlockedPlaylistVariant(pPlaylist, variantIndex))
						{
							++unlocked;
						}
					}
				}
			}
		}
	}

	*outTotal = total;
	*outUnlocked = unlocked;
}

//---------------------------------------
void CPlaylistManager::SetGameModeOption( const char *pOption, const char *pValue )
{
	if (m_activeVariantIndex != m_customVariantIndex)
	{
		CryLog("CPlaylistManager::SetGameModeOption() option '%s' has been changed, switching to custom variant", pOption);
		// Copy current default options into profile
		SaveCurrentSettingsToProfile();

		SetActiveVariant(m_customVariantIndex);
	}
	CProfileOptions *pProfileOptions = g_pGame->GetProfileOptions();
	pProfileOptions->SetOptionValue(pOption, pValue);

	// Need to set options immediately in order for the restrictions to activate
	SGameModeOption *pOptionStruct = GetGameModeOptionStruct(pOption);
	if (pOptionStruct)
	{
		IGameRulesSystem *pGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();
		CGameLobby *pLobby = g_pGame->GetGameLobby();
		const char *pGameRulesCVarString = gEnv->pConsole->GetCVar("sv_gamerules")->GetString();
		const char *pActualRulesName = (pLobby && pLobby->IsCurrentlyInSession()) ? pLobby->GetCurrentGameModeName(pGameRulesCVarString) : pGameRulesSystem->GetGameRulesName(pGameRulesCVarString);
		if (pActualRulesName)
		{
			pOptionStruct->CopyToCVar(pProfileOptions, false, pActualRulesName);
		}
	}
}

//---------------------------------------
void CPlaylistManager::GetGameModeOption(const char *pOption, CryFixedStringT<32> &result)
{
	if (m_activeVariantIndex != m_customVariantIndex)
	{
		// Normal variant (i.e. not the custom one), take the current cvar value (defaults get changed by the variants)
		SGameModeOption *pOptionStruct = GetGameModeOptionStruct(pOption);
		CRY_ASSERT(pOptionStruct && pOptionStruct->m_pCVar);
		if (pOptionStruct && pOptionStruct->m_pCVar)
		{
			if (pOptionStruct->m_pCVar->GetType() == CVAR_INT)
			{
				result.Format("%d", pOptionStruct->m_pCVar->GetIVal());
			}
			else if (pOptionStruct->m_pCVar->GetType() == CVAR_FLOAT)
			{
				const float scaledFloat = pOptionStruct->m_pCVar->GetFVal() / pOptionStruct->m_profileMultiplyer;
				if ((pOptionStruct->m_floatPrecision == 0) || (floorf(scaledFloat) == scaledFloat))
				{
					result.Format("%d", int_round(scaledFloat));
				}
				else
				{
					CryFixedStringT<8> formatString;
					formatString.Format("%%.%df", pOptionStruct->m_floatPrecision);

					result.Format(formatString.c_str(), scaledFloat);
				}
			}
		}
	}
	else
	{
		CProfileOptions *pProfileOptions = g_pGame->GetProfileOptions();
		CRY_ASSERT(pProfileOptions->IsOption(pOption));
		if (pProfileOptions->IsOption(pOption))
		{
			// Custom variant, use whatever value is in the profile
			string optionValue = pProfileOptions->GetOptionValue(pOption, false);
			result = optionValue.c_str();
		}
	}
}

//---------------------------------------
void CPlaylistManager::GetGameModeProfileOptionName(const uint32 index, CryFixedStringT<32> &result)
{
	if (index < m_options.size())
	{
		SGameModeOption &option = m_options[index];
		result = option.m_profileOption.c_str();
	}
}

uint16 CPlaylistManager::PackCustomVariantOption(uint32 index)
{
	if (index < m_options.size())
	{
		SGameModeOption &option = m_options[index];
		CRY_ASSERT(option.m_pCVar);
		int iValue = 0;
		int cvarType = option.m_pCVar->GetType();
		
		if (option.m_pCVar)
		{
			if (cvarType == CVAR_INT)
			{
				iValue = option.m_pCVar->GetIVal();
			}
			else if (cvarType == CVAR_FLOAT)
			{
				// Convert into int (same as if saving to the profile)
				iValue = int_round((option.m_pCVar->GetFVal() / option.m_netMultiplyer));
			}

			//CryLog("Write %d %d %f %d", index, iValue, option.m_pCVar->GetFVal(), cvarType);

			// Add a constant amount (to allow for negative values)
			uint16 valueToSend = (uint16) (iValue + MAX_ALLOWED_NEGATIVE_OPTION_AMOUNT);
			return valueToSend;
		}
	}

	return 0;
}

// Unpacks both int and float as some Gamemode options have both as options e.g. timelimit 2.5f. The option will know which it wants to use.
int CPlaylistManager::UnpackCustomVariantOptionProfileValues(uint16 value, uint32 index, int* pIntValue, float* pFloatValue, int* pFloatPrecision)
{
	if (index < m_options.size())
	{
		SGameModeOption &option = m_options[index];
		CRY_ASSERT(option.m_pCVar);
		if (option.m_pCVar)
		{
			// Read the value and convert back into an int
			int iValue = ((int)value) - MAX_ALLOWED_NEGATIVE_OPTION_AMOUNT;
			if (option.m_pCVar->GetType() == CVAR_INT) 
			{
				if (pIntValue)
				{
					*pIntValue = iValue;
				}

				if (pFloatValue)
				{
					*pFloatValue = (float)iValue;
				}
			}
			else if (option.m_pCVar->GetType() == CVAR_FLOAT)
			{
				float fValue = ((float)iValue * option.m_netMultiplyer) / option.m_profileMultiplyer;

				if ((option.m_floatPrecision == 0) || (floorf(fValue) == fValue))
				{
					const int roundedIntValue = int_round(fValue);
					if (pIntValue)
					{
						*pIntValue = roundedIntValue;
					}

					if (pFloatValue)
					{
						*pFloatValue = (float)roundedIntValue;
					}
				}
				else
				{
					if (pFloatValue)
					{
						*pFloatValue = fValue;
					}

					if (pIntValue)
					{
						*pIntValue = iValue;
					}
				}

				if (pFloatPrecision)
				{
					*pFloatPrecision = option.m_floatPrecision;
				}
			}

			return option.m_pCVar->GetType();
		}
	}	

	return 0;
}

int CPlaylistManager::UnpackCustomVariantOption(uint16 value, uint32 index, int* pIntValue, float* pFloatValue)
{
	if (index < m_options.size())
	{
		SGameModeOption &option = m_options[index];
		CRY_ASSERT(option.m_pCVar);
		if (option.m_pCVar)
		{
			// Read the value and convert back into an int
			int iValue = ((int)value) - MAX_ALLOWED_NEGATIVE_OPTION_AMOUNT;

			if (option.m_pCVar->GetType() == CVAR_INT) 
			{
				if (pIntValue)
				{
					*pIntValue = iValue;
					return CVAR_INT;
				}
			}
			else if (option.m_pCVar->GetType() == CVAR_FLOAT)
			{
				// Convert into float (same as if reading from the profile)
				float fValue = ((float)iValue) * option.m_netMultiplyer;

				if (pFloatValue)
				{
					*pFloatValue = fValue;
					return CVAR_FLOAT;
				}
			}
		}
	}	

	return 0;
}

void CPlaylistManager::ReadDetailedServerInfo(uint16 *pOptions, uint32 numOptions)
{
	CryLog("CPlaylistManager::ReadDetailedServerInfo numOptions %d", numOptions);

	for (uint32 i = 0; i < numOptions; ++ i)
	{
		SGameModeOption &option = m_options[i];
		CRY_ASSERT(option.m_pCVar);
		if (option.m_pCVar)
		{
			// Read the value and convert back into an int
			int iValue = 0;
			float fValue = 0.f;
			int cvarType = 0;
			
			m_bIsSettingOptions = true;

			cvarType = UnpackCustomVariantOption(pOptions[i], i, &iValue, &fValue);
			if (cvarType == CVAR_INT)
			{
				option.m_pCVar->Set(iValue);
			}
			else if (cvarType == CVAR_FLOAT)
			{
				option.m_pCVar->Set(fValue);
			}

			m_bIsSettingOptions = false;
			//CryLog("Read %d %d %f %d", i, iValue, fValue, cvarType);
		}
	}
}


void CPlaylistManager::WriteSetCustomVariantOptions(CCryLobbyPacket* pPacket, CPlaylistManager::TOptionsVec pOptions, uint32 numOptions)
{
	for (uint32 i = 0; i < numOptions; ++ i)
	{
		SGameModeOption &option = pOptions[i];
		CRY_ASSERT(option.m_pCVar);
		if (option.m_pCVar)
		{
			uint16 valueToSend = PackCustomVariantOption(i);
			pPacket->WriteUINT16(valueToSend);
		}
		else
		{
			// Have to write something because the other end is going to read something!
			pPacket->WriteUINT16(0);
		}
	}
}

//---------------------------------------
void CPlaylistManager::WriteSetVariantPacket( CCryLobbyPacket *pPacket )
{
	const int variantNum = m_activeVariantIndex;
	CRY_ASSERT(variantNum > -1);
	if (variantNum > -1)
	{
		const bool bIsCustomVariant = (m_activeVariantIndex == m_customVariantIndex);

		if (!bIsCustomVariant)
		{
			const uint32 bufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size + GetSynchedVarsSize();
			if (pPacket->CreateWriteBuffer(bufferSize))
			{
				pPacket->StartWrite(eGUPD_SetGameVariant, true);
				pPacket->WriteUINT8((uint8) variantNum);
				WriteSynchedVars(pPacket);
			}
		}
		else
		{
			// Custom variant, need to send all the options as well :-(
			const int numOptions = m_options.size();

			// This is horribly large at the moment, lots of scope for packing it properly at a later date
			const uint32 bufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size + (CryLobbyPacketUINT16Size * numOptions) + GetSynchedVarsSize();
			if (pPacket->CreateWriteBuffer(bufferSize))
			{
				pPacket->StartWrite(eGUPD_SetGameVariant, true);
				pPacket->WriteUINT8((uint8) variantNum);
				WriteSynchedVars(pPacket);
				WriteSetCustomVariantOptions(pPacket, m_options, numOptions);
			}
		}
	}
}

//---------------------------------------
int CPlaylistManager::GetSynchedVarsSize()
{
	int size = 0;
	int numVars = m_configVars.size();
	for (int i = 0; i < numVars; ++ i)
	{
		if (m_configVars[i].m_bNetSynched)
		{
			if(m_configVars[i].m_pCVar->GetType() != CVAR_STRING)
			{
				size += CryLobbyPacketUINT16Size;				
			}
			else
			{
				size += CryLobbyPacketUINT8Size;
				size += strlen(m_configVars[i].m_pCVar->GetString());
			}
		}
	}
	return size;
}

//---------------------------------------
void CPlaylistManager::WriteSynchedVars(CCryLobbyPacket* pPacket)
{
	int numVars = m_configVars.size();
	for (int i = 0; i < numVars; ++ i)
	{
		if (m_configVars[i].m_bNetSynched)
		{
			switch (m_configVars[i].m_pCVar->GetType())
			{
			case CVAR_INT:
				pPacket->WriteUINT16((uint16)m_configVars[i].m_pCVar->GetIVal());
				break;
			case CVAR_FLOAT:
				pPacket->WriteUINT16((uint16)m_configVars[i].m_pCVar->GetFVal());
				break;
			case CVAR_STRING:
				CryFixedStringT<24> temp;
				temp.Format("%s", m_configVars[i].m_pCVar->GetString());
				const uint8 length = static_cast<uint8> (strlen(m_configVars[i].m_pCVar->GetString()) + 1); //+1 == escape character
				pPacket->WriteUINT8(length);
				pPacket->WriteString(temp.c_str(), length);
				break;
			}
		}
	}
}

//---------------------------------------
void CPlaylistManager::ReadSynchedVars(CCryLobbyPacket* pPacket)
{
	int numVars = m_configVars.size();
	for (int i = 0; i < numVars; ++ i)
	{
		if (m_configVars[i].m_bNetSynched)
		{
			switch (m_configVars[i].m_pCVar->GetType())
			{
			case CVAR_INT:
				{
					const uint16 intVal = pPacket->ReadUINT16();
					m_configVars[i].m_pCVar->Set((int) intVal);
					break;
				}
			case CVAR_FLOAT:
				{
					const uint16 floatVal = pPacket->ReadUINT16();
					m_configVars[i].m_pCVar->Set((float) floatVal);
					break;
				}
			case CVAR_STRING:
				{
					const uint8 length = pPacket->ReadUINT8();
					char strVal[24];
					pPacket->ReadString(strVal, length);
					m_configVars[i].m_pCVar->Set(strVal);
					break;
				}
			}
		}
	}
}

//---------------------------------------
void CPlaylistManager::ReadSetCustomVariantOptions(CCryLobbyPacket* pPacket, CPlaylistManager::TOptionsVec pOptions, uint32 numOptions)
{
	m_bIsSettingOptions = true;
	for (uint32 i = 0; i < numOptions; ++ i)
	{
		SGameModeOption &option = m_options[i];
		CRY_ASSERT(option.m_pCVar);
		if (option.m_pCVar)
		{
			// Read the value and convert back into an int
			int iValue = 0;
			float fValue = 0.f;
			int cvarType = 0;
			
			cvarType = UnpackCustomVariantOption(pPacket->ReadUINT16(), i, &iValue, &fValue);
			if (cvarType == CVAR_INT)
			{
				option.m_pCVar->Set(iValue);
			}
			else if (cvarType == CVAR_FLOAT)
			{
				option.m_pCVar->Set(fValue);
			}
		}
		else
		{
			// Have to read something because the other end is going to send something!
			pPacket->ReadUINT16();
		}
	}
	m_bIsSettingOptions = false;
}

void CPlaylistManager::ReadSetVariantPacket( CCryLobbyPacket *pPacket )
{
	const int variantNum = (int) pPacket->ReadUINT8();
	CRY_ASSERT((variantNum >= 0) && (variantNum < (int)m_variants.size()));

	if ((variantNum >= 0) && (variantNum < (int)m_variants.size()))
	{
		SetActiveVariant(variantNum);
		ReadSynchedVars(pPacket);

		const bool bIsCustomVariant = (variantNum == m_customVariantIndex);
		if (bIsCustomVariant)
		{
			// Custom variant, need to read all the options as well :-(
			const int numOptions = m_options.size();

			ReadSetCustomVariantOptions(pPacket, m_options, numOptions);
		}
	}
}

//---------------------------------------
bool CPlaylistManager::AdvanceRotationUntil(const int newNextIdx)
{
	bool  ok = false;

	if (HavePlaylistSet())
	{
		if (ILevelRotation* pLevelRotation=GetLevelRotation())
		{
			if (pLevelRotation->GetLength() > 0)
			{
				CRY_ASSERT(newNextIdx < pLevelRotation->GetLength());

				ok = true;

				const int  oldNext = pLevelRotation->GetNext();

				while (pLevelRotation->GetNext() != newNextIdx)
				{
					if (!pLevelRotation->Advance())
					{
						pLevelRotation->First();
					}

					if (pLevelRotation->GetNext() == oldNext)
					{
						CRY_ASSERT_TRACE(0, ("Caught potential infinite loop whilst looking (and failing to find) newNextIdx '%d' in playlist rotation. Breaking loop.", newNextIdx));
						ok = false;
						break;
					}
				}
			}
		}
	}

	return ok;
}


#if !defined(_RELEASE)  /////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------
void CPlaylistManager::DbgPlaylistsList()
{
	const int  n = m_playlists.size();
	if (n > 0)
	{
		CryLogAlways("  The currently loaded playlists are:");
		for (int i=0; i<n; i++)
		{
			SPlaylist*  pIterList = &m_playlists[i];
			const SPlaylistRotationExtension*  pInfo = &pIterList->rotExtInfo;
			const bool  chosen = (pIterList->id == m_currentPlaylistId);
			// [tlh] TODO would be nice to print the localized name here too...
			CryLogAlways("    %s: %u \"%s\"", (pInfo->m_enabled ? (chosen ? "*" : string().Format("%d", i).c_str()) : "X"), pIterList->id, pInfo->uniqueName.c_str());
		}
		CryLogAlways("  An \"*\" in the first column denotes the CURRENT chosen playlist (can be none).");
		CryLogAlways("  An \"X\" in the first column denotes a DISABLED playlist (this playlist cannot be chosen).");
		CryLogAlways("  Use the \"playlists_choose #\" cmd to CHOOSE one of the above playlists,");
		CryLogAlways("  or \"playlists_unchoose\" to UNSELECT the currently chosen playlist.");
	}
	else
	{
		CryLogAlways("  There are currently no playlists loaded, so nothing to show.");
	}
}

//---------------------------------------
void CPlaylistManager::DbgPlaylistsChoose(const int chooseIdx)
{
	const int  n = m_playlists.size();
	if ((chooseIdx >= 0) && (chooseIdx < n))
	{
		SPlaylist*  pList = &m_playlists[chooseIdx];
		const SPlaylistRotationExtension*  pInfo = &pList->rotExtInfo;

		if (pInfo->m_enabled)
		{
			SetCurrentPlaylist(pList->id);

			CryLogAlways("  You have successfully CHOSEN the following playlist to be the current:");
			// [tlh] TODO would be nice to print the localized name here too...
			CryLogAlways("    %d: %u \"%s\"", chooseIdx, pList->id, pInfo->uniqueName.c_str());
		}
		else
		{
			CryLogAlways("  Error: that playlist cannot be chosen as the current because it is NOT ENABLED");
		}
	}
	else
	{
		CryLogAlways("  Error: playlist index '%d' is OUT OF RANGE. It should be >= 0 and < %d", chooseIdx, n);
	}
}

//---------------------------------------
void CPlaylistManager::DbgPlaylistsUnchoose()
{
	SetCurrentPlaylist(0);
	CryLogAlways("  You have successfully UNCHOSEN the current playlist. There is now no current playlist");
}

//---------------------------------------
void CPlaylistManager::DbgPlaylistsShowVariants()
{
	if (HavePlaylistSet())
	{
		if (SPlaylist* pList=FindPlaylistById(m_currentPlaylistId))
		{
			CryLogAlways("  The variants supported by the current playlist are:");
			const int numVariants = pList->rotExtInfo.m_supportedVariants.size();
			for (int i = 0; i < numVariants; ++ i)
			{
				const char *pVariantName = pList->rotExtInfo.m_supportedVariants[i].m_name.c_str();
				{
					int variantIndex = GetVariantIndex(pVariantName);
					const bool  chosen = (variantIndex == m_activeVariantIndex);
					CryLogAlways("    %s: \"%s\"", (chosen ? "*" : string().Format("%d", i).c_str()), pVariantName);
				}
			}
			CryLogAlways("  An \"*\" in the first column denotes the CURRENT active variant.");
			CryLogAlways("  Use the \"playlists_select_variant #\" cmd to SELECT a different active variant.");
		}
		else
		{
			CryLog("  Error: couldn't find playlist matching m_currentPlaylistId '%u', so nothing to show.", m_currentPlaylistId);
		}
	}
	else
	{
		CryLogAlways("  There's currently no playlist selected as current, so nothing to show.");
	}
}

//---------------------------------------
void CPlaylistManager::DbgPlaylistsSelectVariant(int selectIdx)
{
	if (HavePlaylistSet())
	{
		if (SPlaylist* pList=FindPlaylistById(m_currentPlaylistId))
		{
			if ((selectIdx >= 0) && (selectIdx < (int)m_variants.size()))
			{
				const SPlaylistRotationExtension::TVariantsVec &supportedVariants = pList->GetSupportedVariants();
				bool supported = false;
				const int numSupportedVariants = supportedVariants.size();
				for (int i = 0; i < numSupportedVariants; ++ i)
				{
					if (supportedVariants[i].m_id == selectIdx)
				{
						supported = true;
						break;
					}
				}
				if (supported)
				{
					SetActiveVariant(selectIdx);

					CryLogAlways("  You have successfully SELECTED the following variant to be active:");
					CryLogAlways("    %d: \"%s\"", selectIdx, GetVariantName(selectIdx));
				}
				else
				{
					CryLogAlways("  Error: that variant cannot be selected to be active because it is NOT SUPPORTED by the current playlist");
				}
			}
			else
			{
				CryLogAlways("  Error: variant index '%d' is OUT OF RANGE. It should be >= 0 and < %" PRISIZE_T, selectIdx, m_variants.size());
			}
		}
		else
		{
			CryLog("  Error: couldn't find playlist matching m_currentPlaylistId '%u', so cannot select a variant.", m_currentPlaylistId);
		}
	}
	else
	{
		// No playlist set, must be in a private match, all variants are allowed
		SetActiveVariant(selectIdx);
	}
}

//---------------------------------------
// static
void CPlaylistManager::CmdPlaylistsList(IConsoleCmdArgs* pCmdArgs)
{
	CRY_ASSERT(s_pPlaylistManager);
	s_pPlaylistManager->DbgPlaylistsList();
}

//---------------------------------------
// static
void CPlaylistManager::CmdPlaylistsChoose(IConsoleCmdArgs* pCmdArgs)
{
	if (pCmdArgs->GetArgCount() == 2)
	{
		int  chooseIdx = atoi(pCmdArgs->GetArg(1));
		CRY_ASSERT(s_pPlaylistManager);
		s_pPlaylistManager->DbgPlaylistsChoose(chooseIdx);
	}
	else
	{
		CryLogAlways("  Usage: \"playlists_choose [index_of_playlist_to_choose]\"");
	}
}

//---------------------------------------
// static
void CPlaylistManager::CmdPlaylistsUnchoose(IConsoleCmdArgs* pCmdArgs)
{
	CRY_ASSERT(s_pPlaylistManager);
	s_pPlaylistManager->DbgPlaylistsUnchoose();
}

//---------------------------------------
// static
void CPlaylistManager::CmdPlaylistsShowVariants(IConsoleCmdArgs* pCmdArgs)
{
	CRY_ASSERT(s_pPlaylistManager);
	s_pPlaylistManager->DbgPlaylistsShowVariants();
}

//---------------------------------------
// static
void CPlaylistManager::CmdPlaylistsSelectVariant(IConsoleCmdArgs* pCmdArgs)
{
	if (pCmdArgs->GetArgCount() == 2)
	{
		const int selectIdx = atoi(pCmdArgs->GetArg(1));
		CRY_ASSERT(s_pPlaylistManager);
		s_pPlaylistManager->DbgPlaylistsSelectVariant(selectIdx);
	}
	else
	{
		CryLogAlways("  Usage: \"playlists_select_variant [index_of_variant_to_select]\"");
	}
}

#endif  // !_RELEASE ////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//---------------------------------------
// static
void CPlaylistManager::CmdStartPlaylist(IConsoleCmdArgs* pCmdArgs)
{
	if (pCmdArgs->GetArgCount() == 2)
	{
		const char *pConsoleArg = pCmdArgs->GetArg(1);
		if (!DoStartPlaylistCommand(pConsoleArg))
		{
			CryLogAlways("Invalid option");
		}
	}
	else
	{
		CryLogAlways("Invalid format, use 'startPlaylist <playlist>'");
	}
}

//---------------------------------------
// static
bool CPlaylistManager::DoStartPlaylistCommand( const char *pPlaylistArg )
{
	CRY_ASSERT(gEnv);
	// Use the auto complete struct so that we compare against the same strings that are used in the auto-complete
	bool success = false;
	const int numPossibleOptions = gl_PlaylistVariantAutoComplete.GetCount();
	for (int i = 0; i < numPossibleOptions; ++ i)
	{
		const SPlaylist *pPlaylist = NULL;
		const SGameVariant *pVariant = NULL;
		if (gl_PlaylistVariantAutoComplete.GetPlaylistAndVariant(i, &pPlaylist, &pVariant))
		{
			const char *pOptionName = gl_PlaylistVariantAutoComplete.GetOptionName(pPlaylist, pVariant);
			if (!stricmp(pPlaylistArg, pOptionName))
			{
				CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
				if (pPlaylistManager)
				{
					// Activate the playlist and variant
					pPlaylistManager->SetCurrentPlaylist(pPlaylist->id);
					pPlaylistManager->SetActiveVariant(pVariant->m_id);

					int numOptions = pPlaylistManager->m_options.size();
					int numConfigOptions = pPlaylistManager->m_configVars.size();

					CGame::TStringStringMap *pConfigOptions = g_pGame->GetVariantOptions();
					CGame::TStringStringMap::iterator end = pConfigOptions->end();
					for (CGame::TStringStringMap::iterator it = pConfigOptions->begin(); it != end; ++ it)
					{
						const char *pOption = it->first.c_str();
						const char *pValue = it->second.c_str();
						for (int j = 0; j < numOptions; ++ j)
						{
							ICVar *pOptionVar = pPlaylistManager->m_options[j].m_pCVar;
							if (pOptionVar && !stricmp(pOptionVar->GetName(), pOption))
							{
								CryLog("CPlaylistManager::DoStartPlaylistCommand() setting config option %s from %s to %s", pOption, pOptionVar->GetString(), pValue);
								pOptionVar->Set(pValue);
								break;
							}
						}
						for (int k = 0; k < numConfigOptions; ++ k)
						{
							ICVar *pConfigVar = pPlaylistManager->m_configVars[k].m_pCVar;
							if (pConfigVar && !stricmp(pConfigVar->GetName(), pOption))
							{
								CryLog("CPlaylistManager::DoStartPlaylistCommand() setting config option %s from %s to %s", pOption, pConfigVar->GetString(), pValue);
								pConfigVar->Set(pValue);
							}
						}
					}

					CGameLobby *pGameLobby = g_pGame->GetGameLobby();
					if (pGameLobby)
					{
						CryLogAlways("Starting playlist %s...", pPlaylistArg);
						pGameLobby->OnStartPlaylistCommandIssued();
					}
				}
				else
				{
					CryLog("DoStartPlaylist called with %s, but we have no playlist manager", pPlaylistArg ? pPlaylistArg : "<unknown arg>");
				}

				success = true;
				break;
			}
		}
	}
	return success;
}

//---------------------------------------
// [static]
template <class T>
void CPlaylistManager::GetLocalisationSpecificAttr(const XmlNodeRef& rNode, const char* pAttrKey, const char* pLang, T* pOutVal)
{
	CRY_ASSERT(rNode!=0);
	CRY_ASSERT(pAttrKey);
	CRY_ASSERT(pOutVal);

	bool  useEnglish = false;

	CryFixedStringT<32>  attrKeyWithLang = pAttrKey;
	if (pLang && (stricmp(pLang, "english") != 0))
	{
		attrKeyWithLang	+= "_";
		attrKeyWithLang	+= pLang;
	}
	else
	{
		useEnglish = true;
	}

	const char* pAttrVal = NULL;

	if (rNode)
	{
		if (!useEnglish)
		{
			if (!rNode->getAttr(attrKeyWithLang, &pAttrVal))
			{
				useEnglish = true;
				LOCAL_WARNING(0, string().Format("Could not get attribute '%s' for language '%s', will try falling-back to English...", pAttrKey, pLang).c_str());
			}
		}

		if (useEnglish)
		{
			const bool  gotEng = rNode->getAttr(pAttrKey, &pAttrVal);
			LOCAL_WARNING(gotEng, string().Format("Could not get attribute '%s' for English language! Will not have a valid value for this attribute.", pAttrKey).c_str());
		}
	}

	if (pAttrVal && (pAttrVal[0] != '\0'))
	{
		if (pOutVal)
		{
			(*pOutVal) = pAttrVal;
		}
	}
	else
	{
		LOCAL_WARNING(0, string().Format("Could not get an attribute named '%s' in node '%s' (when pAttrKey='%s' and pLang='%s')", attrKeyWithLang.c_str(), (rNode ? rNode->getTag() : NULL), pAttrKey, pLang).c_str());
		if (pOutVal)
		{
			(*pOutVal).clear();
		}
	}
}

//---------------------------------------
ILevelRotation::TExtInfoId CPlaylistManager::CreateCustomPlaylist(const char *pName)
{
	const ILevelRotation::TExtInfoId playlistId = GetPlaylistId(pName);

	SPlaylist* pList = FindPlaylistById(playlistId);
	if (!pList)
	{
		m_playlists.push_back(SPlaylist());
		pList = &m_playlists.back();
	}

	pList->Reset();
	pList->rotExtInfo.uniqueName = pName;
	pList->id = playlistId;
	pList->bSynchedFromServer = true;
	pList->SetEnabled(true);

	return playlistId;
}

//---------------------------------------
void CPlaylistManager::OnAfterVarChange( ICVar *pVar )
{
	CRY_ASSERT(gEnv->IsDedicated());
	
	
#ifndef _RELEASE
	if(g_pGameCVars->g_disableSwitchVariantOnSettingChanges)
	{
		return; 
	}
#endif //#ifndef _RELEASE

	if (!m_bIsSettingOptions)
	{
		const uint32 numOptions = m_options.size();
		for (uint32 i = 0; i < numOptions; ++ i)
		{
			SGameModeOption &option = m_options[i];
			if (option.m_pCVar == pVar)
			{
				CProfileOptions *pProfileOptions = g_pGame->GetProfileOptions();
				CGameLobby *pGameLobby = g_pGame->GetGameLobby();
				if (pProfileOptions && pGameLobby)
				{
					CryLog("Saving current settings");
					SaveCurrentSettingsToProfile();  // these settings will get read back out of the profile in the OnVarianteChanged() function below, so this essentially copies the current variant options (with the var changes that triggered this call included) onto the custom variant

					if (m_activeVariantIndex != m_customVariantIndex)
					{
						CryLog("CPlaylistManager::OnAfterVarChange() change detected to cvar %s, switching to custom variant", pVar->GetName());
						ChooseVariant(m_customVariantIndex);
					}

					pGameLobby->QueueSessionUpdate();
				}

				break;
			}
		}
	}
}

#if USE_DEDICATED_LEVELROTATION
//---------------------------------------
void CPlaylistManager::LoadLevelRotation()
{
	// Check for and load levelrotation.xml
	string path = PathUtil::GetGameFolder();

	CryFixedStringT<128> levelRotationFileName;
	levelRotationFileName.Format("./%s/LevelRotation.xml", path.c_str());

	CryLog("[dedicated] checking for LevelRotation.xml at %s", levelRotationFileName.c_str());

	CCryFile file;
	if (file.Open( levelRotationFileName.c_str(), "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FOPEN_ONDISK ))
	{
		const size_t fileSize = file.GetLength();
		char *pBuffer = new char [fileSize];

		file.ReadRaw(pBuffer, fileSize);

		XmlNodeRef root = gEnv->pSystem->LoadXmlFromBuffer(pBuffer, fileSize);

		if (root)
		{
			if (!stricmp(root->getTag(), "levelRotation"))
			{
				const char *pRotationName = NULL;
				if (!root->getAttr("name", &pRotationName))
				{
					pRotationName = PLAYLIST_MANAGER_CUSTOM_NAME;
				}

				
				bool bUsingCustomRotation = false;
				ILevelRotation *pRotation = NULL;
				bool success = true;
				SPlaylist *pPlaylist = FindPlaylistByUniqueName(pRotationName);
				if (!pPlaylist)
				{
					const ILevelRotation::TExtInfoId playlistId = CreateCustomPlaylist(pRotationName);
					pPlaylist = FindPlaylistById( playlistId );
					if (pPlaylist)
					{
						pPlaylist->m_bIsCustomPlaylist = true;
					}
					
					bUsingCustomRotation = true;

					ILevelSystem*  pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
					
					if( pLevelSystem->AddExtendedLevelRotationFromXmlRootNode( root, NULL, playlistId ) )
					{						
						pRotation = pLevelSystem->FindLevelRotationForExtInfoId(  playlistId);
					}
					else
					{
						LOCAL_WARNING(0, "Failed to add extended level rotation!");
						m_playlists.pop_back();

						success = false;
					}

				}


				if( success )
				{
					bool bReadServerInfo = false;
					int variantIndex = -1;

					IGameRulesSystem *pGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();
					const int numChildren = root->getChildCount();
					for (int i = 0; i < numChildren; ++ i)
					{
						XmlNodeRef childXml = root->getChild(i);
						const char *pTag = childXml->getTag();
						if (!stricmp(pTag, "ServerInfo"))
						{
							if (bReadServerInfo == false)
							{
								bReadServerInfo = true;

								int maxPlayers = MAX_PLAYER_LIMIT;

								ReadServerInfo(childXml, variantIndex, maxPlayers);

								if (pPlaylist)
								{
									if (bUsingCustomRotation)
									{
										pPlaylist->rotExtInfo.m_maxPlayers = maxPlayers;
									}
								}
							}
						}
					}

					if (bReadServerInfo == false)
					{
						variantIndex = m_defaultVariantIndex;
					}

					if (variantIndex != -1)
					{
						const SGameVariant *pVariant = GetVariant(variantIndex);
						CRY_ASSERT(pVariant);
						if (pVariant)
						{
							if (pPlaylist && ((variantIndex == m_customVariantIndex) || bUsingCustomRotation))
							{
								pPlaylist->rotExtInfo.m_supportedVariants.push_back(SSupportedVariantInfo(pVariant->m_name.c_str(), pVariant->m_id));
							}
							CryFixedStringT<128> startCommand;
							startCommand.Format("startPlaylist %s__%s", pRotationName, pVariant->m_name.c_str());
							gEnv->pConsole->ExecuteString(startCommand);
						}
					}

				}
				else
				{
					CryLog( "Error in Creating Level Rotation for playlist" );
				}
			}
		}
		delete[] pBuffer;
	}
}

//---------------------------------------
void CPlaylistManager::ReadServerInfo(XmlNodeRef xmlNode, int &outVariantIndex, int &outMaxPlayers)
{
	const char *pString = NULL;

	outMaxPlayers = MAX_PLAYER_LIMIT;

	const int numChildren = xmlNode->getChildCount();
	for (int i = 0; i < numChildren; ++ i)
	{
		XmlNodeRef xmlChild = xmlNode->getChild(i);
		const char *pTag = xmlChild->getTag();
		if (!stricmp(pTag, "Details"))
		{
			if (xmlChild->getAttr("name", &pString))
			{
				ICVar *pCVar = gEnv->pConsole->GetCVar("sv_servername");
				CRY_ASSERT(pCVar);
				if (pCVar)
				{
					pCVar->Set(pString);
				}
			}
			if (xmlChild->getAttr("password", &pString))
			{
				ICVar *pCVar = gEnv->pConsole->GetCVar("sv_password");
				CRY_ASSERT(pCVar);
				if (pCVar)
				{
					pCVar->Set(pString);
				}
			}
			if (xmlChild->getAttr("motd", &pString))
			{
				g_pGameCVars->g_messageOfTheDay->Set(pString);
			}
			if (xmlChild->getAttr("imageURL", &pString))
			{
				g_pGameCVars->g_serverImageUrl->Set(pString);
			}
			if (xmlChild->getAttr("maxPlayers", outMaxPlayers))
			{
				outMaxPlayers = CLAMP(outMaxPlayers, 2, MAX_PLAYER_LIMIT);
			}
		}
		else if (!stricmp(pTag, "Variant"))
		{
			if (!xmlChild->getAttr("name", &pString))
			{
				pString = PLAYLIST_MANAGER_CUSTOM_NAME;
			}

			outVariantIndex = -1;
			const int numVariants = m_variants.size();
			for (int j = 0; j < numVariants; ++ j)
			{
				if (!stricmp(m_variants[j].m_name.c_str(), pString))
				{
					outVariantIndex = j;
					break;
				}
			}

			if ((outVariantIndex == -1) || (outVariantIndex == m_customVariantIndex))
			{
				if ((m_customVariantIndex >= 0) && (m_customVariantIndex < m_variants.size()))
				{
					outVariantIndex = m_customVariantIndex;
					SGameVariant *pVariant = &m_variants[m_customVariantIndex];
					SetOptionsFromXml(xmlChild, pVariant);
				}
			}
		}
	}
}

//---------------------------------------
void CPlaylistManager::SetOptionsFromXml(XmlNodeRef xmlNode, SGameVariant *pVariant)
{
	const int numOptions = xmlNode->getChildCount();
	for (int i = 0; i < numOptions; ++ i)
	{
		XmlNodeRef xmlChild = xmlNode->getChild(i);
		if (!stricmp(xmlChild->getTag(), "Option"))
		{
			const char *pOption = NULL;
			if (xmlChild->getAttr("setting", &pOption))
			{
				pVariant->m_options.push_back(pOption);
			}
		}
	}
}

//---------------------------------------
bool CPlaylistManager::IsUsingCustomRotation()
{
	SPlaylist *pPlaylist = FindPlaylistById(m_currentPlaylistId);
	
	bool bResult = false;
	if (pPlaylist)
	{
		bResult = (pPlaylist->m_bIsCustomPlaylist || (m_activeVariantIndex == m_customVariantIndex));
	}
	return bResult;
}
#endif

//---------------------------------------
void CPlaylistManager::LoadOptionRestrictions()
{
	CryLog("CPlaylistManager::LoadOptionRestrictions()");
	if (XmlNodeRef xmlRoot = gEnv->pSystem->LoadXmlFromFile(OPTION_RESTRICTIONS_FILENAME))
	{
		const int numChildren = xmlRoot->getChildCount();
		m_optionRestrictions.reserve(numChildren);
		for (int i = 0; i < numChildren; ++ i)
		{
			XmlNodeRef xmlChild = xmlRoot->getChild(i);
			if (!stricmp(xmlChild->getTag(), "Restriction"))
			{
				const int numOperands = xmlChild->getChildCount();
				if (numOperands == 2)
				{
					XmlNodeRef xmlOperand1 = xmlChild->getChild(0);
					XmlNodeRef xmlOperand2 = xmlChild->getChild(1);

					SOptionRestriction restriction;
					if (LoadOperand(xmlOperand1, restriction.m_operand1) && LoadOperand(xmlOperand2, restriction.m_operand2))
					{
#ifndef _RELEASE
						ConsoleVarFunc pCallbackFunc = restriction.m_operand1.m_pVar->GetOnChangeCallback();
						if (pCallbackFunc && pCallbackFunc != CPlaylistManager::OnCustomOptionCVarChanged)
						{
							CryFatalError("Can't add callback on operand 1 (%s - %p) - one already exists", restriction.m_operand1.m_pVar->GetName(), restriction.m_operand1.m_pVar);
						}
						pCallbackFunc = restriction.m_operand2.m_pVar->GetOnChangeCallback();
						if (pCallbackFunc && pCallbackFunc != CPlaylistManager::OnCustomOptionCVarChanged)
						{
							CryFatalError("Can't add callback on operand 2 (%s - %p) - one already exists", restriction.m_operand2.m_pVar->GetName(), restriction.m_operand2.m_pVar);
						}
#endif

						restriction.m_operand1.m_pVar->SetOnChangeCallback(CPlaylistManager::OnCustomOptionCVarChanged);
						restriction.m_operand2.m_pVar->SetOnChangeCallback(CPlaylistManager::OnCustomOptionCVarChanged);

						m_optionRestrictions.push_back(restriction);
					}
					else
					{
						CryLog("  badly formed file - failed to read operand in restriction %d", i);
					}
				}
				else
				{
					CryLog("  badly formed file - expected 2 operands in restriction %d", i);
				}
			}
		}
	}
}

//---------------------------------------
bool CPlaylistManager::LoadOperand( XmlNodeRef operandXml, SOptionRestriction::SOperand &outResult )
{
	if (!stricmp(operandXml->getTag(), "Operand"))
	{
		const char *pVarName;
		if (operandXml->getAttr("var", &pVarName))
		{
			outResult.m_pVar = gEnv->pConsole->GetCVar(pVarName);
			if (outResult.m_pVar)
			{
				const char *pValue;
				if (operandXml->getAttr("equal", &pValue))
				{
					outResult.m_type = SOptionRestriction::eOT_Equal;
				}
				else if (operandXml->getAttr("notEqual", &pValue))
				{
					outResult.m_type = SOptionRestriction::eOT_NotEqual;
				}
				else if (operandXml->getAttr("lessThan", &pValue))
				{
					outResult.m_type = SOptionRestriction::eOT_LessThan;
				}
				else if (operandXml->getAttr("greaterThan", &pValue))
				{
					outResult.m_type = SOptionRestriction::eOT_GreaterThan;
				}
				else
				{
					return false;
				}

				const char *pFallbackValue;
				if (operandXml->getAttr("fallback", &pFallbackValue))
				{
					int varType = outResult.m_pVar->GetType();
					switch (varType)
					{
					case CVAR_INT:
						outResult.m_comparisionValue.m_int = atoi(pValue);
						outResult.m_fallbackValue.m_int = atoi(pFallbackValue);
						break;
					case CVAR_FLOAT:
						outResult.m_comparisionValue.m_float = (float) atof(pValue);
						outResult.m_fallbackValue.m_float = (float) atof(pFallbackValue);
						break;
					case CVAR_STRING:
						outResult.m_comparisionValue.m_string = pValue;
						outResult.m_fallbackValue.m_string = pFallbackValue;
						break;
					}

					return true;
				}
				else
				{
					return false;
				}
			}
		}
	}

	return false;
}

//---------------------------------------
void CPlaylistManager::CheckRestrictions( ICVar *pChangedVar )
{
	int numRestrictions = m_optionRestrictions.size();
	for (int i = 0; i < numRestrictions; ++ i)
	{
		SOptionRestriction &restriction = m_optionRestrictions[i];
		if (restriction.m_operand1.m_pVar == pChangedVar)
		{
			ApplyRestriction(restriction.m_operand1, restriction.m_operand2);
		}
		else if (restriction.m_operand2.m_pVar == pChangedVar)
		{
			ApplyRestriction(restriction.m_operand2, restriction.m_operand1);
		}
	}
	SaveCurrentSettingsToProfile();
}

//---------------------------------------
void CPlaylistManager::ApplyRestriction( SOptionRestriction::SOperand &operand1, SOptionRestriction::SOperand &operand2 )
{
	if (CheckOperation(operand1, false))
	{
		if (CheckOperation(operand2, true))
		{
			CryLog("CPlaylistManager::ApplyRestriction() found violated restriction, '%s' set to fallback value", operand2.m_pVar->GetName());
		}
	}
}

//---------------------------------------
bool CPlaylistManager::CheckOperation( SOptionRestriction::SOperand &operand, bool bEnsureFalse )
{
	bool bComparisonResult = false;

	int varType = operand.m_pVar->GetType();
	switch (varType)
	{
	case CVAR_INT:
		{
			switch (operand.m_type)
			{
			case SOptionRestriction::eOT_Equal:
				bComparisonResult = (operand.m_pVar->GetIVal() == operand.m_comparisionValue.m_int);
				break;
			case SOptionRestriction::eOT_NotEqual:
				bComparisonResult = (operand.m_pVar->GetIVal() != operand.m_comparisionValue.m_int);
				break;
			case SOptionRestriction::eOT_LessThan:
				bComparisonResult = (operand.m_pVar->GetIVal() < operand.m_comparisionValue.m_int);
				break;
			case SOptionRestriction::eOT_GreaterThan:
				bComparisonResult = (operand.m_pVar->GetIVal() > operand.m_comparisionValue.m_int);
				break;
			}
			if (bComparisonResult && bEnsureFalse)
			{
				operand.m_pVar->Set(operand.m_fallbackValue.m_int);
			}
		}
		break;
	case CVAR_FLOAT:
		{
			switch (operand.m_type)
			{
			case SOptionRestriction::eOT_Equal:
				bComparisonResult = (operand.m_pVar->GetFVal() == operand.m_comparisionValue.m_float);
				break;
			case SOptionRestriction::eOT_NotEqual:
				bComparisonResult = (operand.m_pVar->GetFVal() != operand.m_comparisionValue.m_float);
				break;
			case SOptionRestriction::eOT_LessThan:
				bComparisonResult = (operand.m_pVar->GetFVal() < operand.m_comparisionValue.m_float);
				break;
			case SOptionRestriction::eOT_GreaterThan:
				bComparisonResult = (operand.m_pVar->GetFVal() > operand.m_comparisionValue.m_float);
				break;
			}
			if (bComparisonResult && bEnsureFalse)
			{
				operand.m_pVar->Set(operand.m_fallbackValue.m_float);
			}
		}
		break;
	case CVAR_STRING:
		{
			switch (operand.m_type)
			{
			case SOptionRestriction::eOT_Equal:
				bComparisonResult = (stricmp(operand.m_pVar->GetString(), operand.m_comparisionValue.m_string.c_str()) == 0);
				break;
			case SOptionRestriction::eOT_NotEqual:
				bComparisonResult = (stricmp(operand.m_pVar->GetString(), operand.m_comparisionValue.m_string.c_str()) != 0);
				break;
			}
			if (bComparisonResult && bEnsureFalse)
			{
				operand.m_pVar->Set(operand.m_fallbackValue.m_string.c_str());
			}
		}
		break;
	}

	return bComparisonResult;
}

//---------------------------------------
void CPlaylistManager::OnCustomOptionCVarChanged( ICVar *pCVar )
{
	CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
	if (pPlaylistManager)
	{
		pPlaylistManager->CheckRestrictions(pCVar);
	}
}

#undef LOCAL_WARNING
#undef MAX_ALLOWED_NEGATIVE_OPTION_AMOUNT
