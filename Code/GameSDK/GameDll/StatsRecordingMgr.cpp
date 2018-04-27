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

#include "StdAfx.h"
#include "StatsRecordingMgr.h"
#include <CryGame/IGameStatistics.h>
#include "StatHelpers.h"
#include "GameRules.h"
#include "Weapon.h"
#include "Player.h"
#include "Network/Lobby/GameLobbyData.h"
#include "XMLStatsSerializer.h"
#include "TelemetryCollector.h"
#include "Network/Lobby/GameLobby.h"
#include "CircularStatsStorage.h"
#include "ISaveGame.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include <CryCore/TypeInfo_impl.h>
#include "DataPatchDownloader.h"
#include "PatchPakManager.h"


static const char	*k_stats_suitMode_any="any";
static const char	*k_stats_suitMode_tactical="tactical";
static const char	*k_stats_suitMode_infiltration="infiltration";
static const char	*k_stats_suitMode_combat="combat";
static const char	*k_stats_suitMode_agility="agility";

CStatsRecordingMgr::CStatsRecordingMgr() :
	m_gameStats(NULL),
	m_statsStorage(NULL),
	m_sessionTracker(NULL),
	m_roundTracker(NULL),
	m_serializer(NULL),
	m_submitPermissions(k_submitFullPermissions),
	m_endSessionCountdown(0.f),
	m_copierThread(NULL),
	m_IsIDataListener(false),
	m_scheduleRemoveIDataListener(false),
	m_requestNewSessionId(false)
{
	memset(m_teamTrackers, 0, sizeof(m_teamTrackers));

	m_gameStats=g_pGame->GetIGameFramework()->GetIGameStatistics();
	CRY_ASSERT_MESSAGE(m_gameStats,"CStatsRecordingMgr instantiated without IGameStatistics being instantiated first, initialise IGameStatistics first - fatal");

	m_gameStats->SetStatisticsCallback(this);

	int		size;
	if (g_pGameCVars->g_multiplayerDefault)
	{
		size=g_pGameCVars->g_telemetry_memory_size_mp;
	}
	else
	{
		size=g_pGameCVars->g_telemetry_memory_size_sp;
	}

	// the stats recording mgr gets torn down when switching from mp/sp. the circular buffer may still be in use if telemetry is still streaming so we take care
	// not to delete it by using reference counting. on initialisation here, we avoid creating the circular buffer if it already exists
	// however the circular buffer initialises itself into either limited mode or unlimited mode depending on whether it's going to be used for sp or mp. reconnecting
	// to the existing circular buffer can mean that mp settings get adopted by sp or vice versa
	// however, sp won't be shipping with telemetry, so the only time this code will get hit in release is when entering mp, in which case re-adopting existing storage is fine

	m_statsStorage=CCircularBufferStatsStorage::GetDefaultStorageNoCheck();
	if (!m_statsStorage)
	{
		m_statsStorage=new CCircularBufferStatsStorage(size);
	}

	m_gameStats->SetStorageFactory(m_statsStorage); 

	if (!m_statsStorage->IsUsingCircularBuffer() || (g_pGameCVars->g_telemetry_serialize_method&2))
	{
		m_serializer=new CXMLStatsSerializer(g_pGame->GetIGameFramework()->GetIGameStatistics(), this);
		assert(m_serializer);
		m_gameStats->RegisterSerializer(m_serializer);
	}

	//m_statsDirectory.Format(".\\%s/StatsLogs", gEnv->pCryPak->GetAlias("%USER%"));

	m_statsDirectory = "%USER%/StatsLogs";
	gEnv->pCryPak->MakeDir(m_statsDirectory.c_str());

//	m_profileStatsBinder.BindCallback(functor(*this, &CMissionStatistics::OnStatsReceived));
//	m_profileStatsBinder.BindErrback(functor(*this, &CMissionStatistics::OnStatsError));

	SGameStatDesc gameSpecificEvents[eGSE_Num - eSE_Num] = {
		GAME_STAT_DESC(eGSE_Melee,				"melee"				),
		GAME_STAT_DESC(eGSE_MeleeHit,			"melee_hit"			),
		GAME_STAT_DESC(eGSE_Firemode,			"firemode"			),
		GAME_STAT_DESC(eGSE_HitReactionAnim, "hitReaction_anim" ),
		GAME_STAT_DESC(eGSE_DeathReactionAnim, "deathReaction_anim" ),
		GAME_STAT_DESC(eGSE_SpectacularKill, "spectacular_kill"),
		GAME_STAT_DESC(eGSE_Jump, "jump"),
		GAME_STAT_DESC(eGSE_Land, "land"),
		GAME_STAT_DESC(eGSE_Crouch, "crouch"),
		GAME_STAT_DESC(eGSE_Swim, "swim"),
		GAME_STAT_DESC(eGSE_Use, "use"),
		GAME_STAT_DESC(eGSE_DropItem, "drop_item"),
		GAME_STAT_DESC(eGSE_PickupItem, "pickup_item"),
		GAME_STAT_DESC(eGSE_Grab, "grab"),
		GAME_STAT_DESC(eGSE_LedgeGrab, "ledge_grab"),
		GAME_STAT_DESC(eGSE_Slide, "slide"),
		GAME_STAT_DESC(eGSE_Sprint, "sprint"),
		GAME_STAT_DESC(eGSE_Zoom, "zoom"),
		GAME_STAT_DESC(eGSE_Save, "save"),
		GAME_STAT_DESC(eGSE_NewObjective, "new_objective"),
		GAME_STAT_DESC(eGSE_PlayerAction, "player_action"),
		GAME_STAT_DESC(eGSE_Resurrection, "resurrection"),
		GAME_STAT_DESC(eGSE_XPChanged, "xp"),
		GAME_STAT_DESC(eGSE_VisionModeChanged, "vision_mode"),
		GAME_STAT_DESC(eGSE_Flashed, "flashed"),
		GAME_STAT_DESC(eGSE_Tagged, "tagged"),
		GAME_STAT_DESC(eGSE_ExchangeItem, "exchange_item"),
		GAME_STAT_DESC(eGSE_InteractiveEvent, "interactive_event"),
		GAME_STAT_DESC(eGSE_EnvironmentalWeaponEvent,"environmental_weapon"),
		GAME_STAT_DESC(eGSE_VTOL_EnterExit,"vtol_enter_exit"),
		GAME_STAT_DESC(eGSE_Hunter_ChangeToPredator,"hunter_changeToPredator"),
		GAME_STAT_DESC(eGSE_Hunter_RoundEnd,"hunter_roundEnded"),
		GAME_STAT_DESC(eGSE_Spears_SpearStateChanged,"spear_stateChanged"),
	};
	m_gameStats->RegisterGameEvents(gameSpecificEvents, eGSE_Num - eSE_Num);

	SGameStatDesc gameSpecificStates[eGSS_Num - eSS_Num] = {
		GAME_STAT_DESC(eGSS_MapPath,			"map_path"			),
		GAME_STAT_DESC(eGSS_Attachments,	"attachments"		),
		GAME_STAT_DESC(eGSS_TeamName,			"name"					),
		GAME_STAT_DESC(eGSS_TeamID,				"id"						),
		GAME_STAT_DESC(eGSS_RoundBegin,		"begin"					),
		GAME_STAT_DESC(eGSS_RoundEnd,			"end"						),
		GAME_STAT_DESC(eGSS_RoundActuallyStarted, "actually_started" ),
		GAME_STAT_DESC(eGSS_RoundNumber,	"round_number"  ),
		GAME_STAT_DESC(eGSS_PrimaryTeam,	"primary_team"  ),
		GAME_STAT_DESC(eGSS_Config,				"config"				),
		GAME_STAT_DESC(eGSS_MaxHealth,		"maxHealth"			),
		GAME_STAT_DESC(eGSS_OnlineGUID,		"online_guid"		),
		GAME_STAT_DESC(eGSS_UserName,			"userName"		),
		GAME_STAT_DESC(eGSS_LobbyVersion,	"lobby_version"		),
		GAME_STAT_DESC(eGSS_MatchMakingBlock,	"matchmaking_block"		),
		GAME_STAT_DESC(eGSS_MatchMakingVersion,	"matchmaking_version"		),
		GAME_STAT_DESC(eGSS_PatchPakVersion, "patch_pak_version" ),
		GAME_STAT_DESC(eGSS_RunOnceVersion, "run_once_version"),
		GAME_STAT_DESC(eGSS_RunOnceTrackingVersion, "run_once_tracking_version"),
		GAME_STAT_DESC(eGSS_DataTruncated,	"data_truncated"		),
		GAME_STAT_DESC(eGSS_MemRequired,	"mem_required"		),
		GAME_STAT_DESC(eGSS_MemAvailable,	"mem_available"		),
		GAME_STAT_DESC(eGSS_BuildNumber,	"build_number"		),
		GAME_STAT_DESC(eGSS_StatsFileFormatVersion,	"version"		),
		GAME_STAT_DESC(eGSS_LifeNumber,		"life_number"		),
		GAME_STAT_DESC(eGSS_AIFaction,		"ai_faction"		),
		GAME_STAT_DESC(eGSS_Difficulty,		"difficulty"		),
		GAME_STAT_DESC(eGSS_MatchBonusXp,	"match_bonus_xp"),
		GAME_STAT_DESC(eGSS_XP,						"xp"						),
		GAME_STAT_DESC(eGSS_Rank,					"rank"					),
		GAME_STAT_DESC(eGSS_DefaultRank,	"default_rank"	),
		GAME_STAT_DESC(eGSS_StealthRank,	"stealth_rank"	),
		GAME_STAT_DESC(eGSS_ArmourRank,		"armour_rank"		),
		GAME_STAT_DESC(eGSS_Reincarnations,	"reincarnations"	),
		GAME_STAT_DESC(eGSS_PatchId,			"patch_id"			),
		GAME_STAT_DESC(eGSS_SkillLevel,		"skill_level"		),
		GAME_STAT_DESC(eGSS_Interactables,	"Interactables"	),
	};
	m_gameStats->RegisterGameStates(gameSpecificStates, eGSS_Num - eSS_Num);

	SGameScopeDesc gameScopes[eGSC_Num] = {
		GAME_SCOPE_DESC(eGSC_Session,	"stats_session",	"logged_session_stats"	),
		GAME_SCOPE_DESC(eGSC_Round,		"round",			"logged_round_stats"	),
	};
	m_gameStats->RegisterGameScopes(gameScopes, eGSC_Num);


	SGameElementDesc gameElements[eGSEL_Num] = {
		GAME_ELEM_DESC(eGSEL_Team,		eGNLT_TeamID,		"team",					"logged_team_stats"		),
		GAME_ELEM_DESC(eGSEL_Player,	eGNLT_ChannelID,	"player",				"logged_stats"			),
		GAME_ELEM_DESC(eGSEL_AIActor,	eNLT_EntityID,		"ai_actor",				"logged_stats"			),
		GAME_ELEM_DESC(eGSEL_Entity,	eNLT_EntityID,		"entity",				"logged_stats"			),
	};
	m_gameStats->RegisterGameElements(gameElements, eGSEL_Num);

	SetNewSessionId(false);

	// default to always recording everything (can be overridden from configs)

	//for(int i=0; i<eGSE_Num; ++i)
	//{
	//	m_eventConfigurations[i] = eSERT_Always;
	//}

	LoadEventConfig(g_pGameCVars->g_telemetryConfig);

#if !defined(_RELEASE)
	m_pDummyEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DummyPlayer");
#endif

	m_checkpointCount = 0;
 	m_lifeCount = 0;

	g_pGame->GetIGameFramework()->RegisterListener(this, "CStatsRecordingMgr", FRAMEWORKLISTENERPRIORITY_GAME);

	CDownloadMgr		*pDL=g_pGame->GetDownloadMgr();
	CRY_ASSERT_MESSAGE(pDL || !gEnv->bMultiplayer,"CStatsRecordingMgr must be instantiated after the CDownloadMgr");		// need to get our settings from the web
	if (pDL)
	{
		CDownloadableResourcePtr		pRes=pDL->FindResourceByName("permissions");

		if (pRes.get())
		{
			m_submitPermissions=0;
			pRes->AddDataListener(this);
			m_IsIDataListener=true;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME,VALIDATOR_ERROR,"CStatsRecordingMgr failed to start downloading its online settings - will default to sending telemetry");
#if USER_marktu
			CRY_ASSERT_MESSAGE(0,"Failed to find permission resource");
#endif
		}
	}
}

CStatsRecordingMgr::~CStatsRecordingMgr()
{
	CDownloadMgr							*pDL=g_pGame->GetDownloadMgr();

	if (m_IsIDataListener && pDL)
	{
		CDownloadableResourcePtr	pRes=pDL->FindResourceByName("permissions");
	
		if (pRes.get())
		{
			pRes->RemoveDataListener(this);
			m_IsIDataListener=false;
		}
	}

	if (m_serializer)
	{
		m_gameStats->UnregisterSerializer(m_serializer);
	}

	m_gameStats->SetStorageFactory(NULL); 
	m_statsStorage=NULL;		// smart ptr release

	m_gameStats->SetStatisticsCallback(NULL);
	delete m_serializer;

	g_pGame->GetIGameFramework()->UnregisterListener(this);
}

void CStatsRecordingMgr::ProcessOnlinePermission(
	const char									*pKey,
	const char									*pValue)
{
	struct
	{
		const char					*pKey;
		TSubmitPermissions	bit;
	} map[] =
	{
		{ "post-accounts", k_submitAccounts },
		{ "post-stats-logs", k_submitStatsLogs },
		{ "post-player-stats", k_submitPlayerStats },
		{ "post-remote-player-stats", k_submitRemotePlayerStats },
		{ "post-anti-cheat-logs", k_submitAntiCheatLogs },
		{ "post-connectivity-event-logs", k_submitConnectivityEventLogs }

	};

	for (size_t i=0; i<CRY_ARRAY_COUNT(map); i++)
	{
		if (stricmp(map[i].pKey,pKey)==0 )
		{
			// If value is a simple numeral: permission = value !=  0
			// If value is of the form "1/1000" then permission is determined 
			// based on a sampling where...
			// permission = (crc(profilename) % denominator) < numerator
			// This allows us to ramp up or down the amount of users we sample while 
			// maintaining a deterministic set of users. 

			bool permission = false;

			if(const char* slashPtr = strchr(pValue,'/'))
			{
				// Sampling case...
				uint32 numerator;
				uint32 denominator;

				slashPtr++;
				denominator = atoi(slashPtr);
				if(denominator!=0)
				{
					numerator = atoi(pValue);
					
					const uint32 userIndex = g_pGame->GetExclusiveControllerDeviceIndex();

					IPlatformOS::TUserName tUserName;
					IPlatformOS* pPlatformOS = gEnv->pSystem->GetPlatformOS();
					if(pPlatformOS->UserGetOnlineName(userIndex, tUserName) == false)
					{
						pPlatformOS->UserGetName(userIndex, tUserName);
					}
					
					const char* profileName = tUserName.c_str();
#if defined( DEDICATED_SERVER )
					if( ICVar* pServer = gEnv->pConsole->GetCVar( "sv_servername" ) )
					{
						if( const char* pServerName = pServer->GetString() )
						{
							profileName = pServerName;
						}
					}
#endif					
					const uint32 profileNameCrc = CCrc32::Compute( profileName );

					const uint32 profileNameModulo = profileNameCrc % denominator;
					permission = profileNameModulo < numerator;
				}
			}
			else
			{
				// Simple numeral case...
				if (atoi(pValue)!=0)
				{
					permission = true;
				}
			}

			if (permission)
			{
				m_submitPermissions|=map[i].bit;
				CryLog("online submit permission '%s' set to 1",map[i].pKey);
			}
			else
			{
				m_submitPermissions&=~map[i].bit;
				CryLog("online submit permission '%s' set to 0",map[i].pKey);
			}

		}
	}
}


void CStatsRecordingMgr::DataDownloaded(
	CDownloadableResourcePtr		inResource)
{
	int				bufferSize=0;
	char*			buffer;

	inResource->GetRawData(&buffer,&bufferSize);

	IXmlParser *parser=GetISystem()->GetXmlUtils()->CreateXmlParser();
	XmlNodeRef root=parser->ParseBuffer(buffer, bufferSize, false);

	if (!root)
	{
		CryWarning(VALIDATOR_MODULE_GAME,VALIDATOR_ERROR,"CStatsRecordingMgr failed to parse the permissions.xml it downloaded");
	}
	
	if (root && stricmp(root->getTag(),"permissions")==0)
	{
		const int numChildren = root->getChildCount();
		for (int i=0; i<numChildren; i++)
		{
			if (XmlNodeRef xmlChild=root->getChild(i))
			{
				if (stricmp(xmlChild->getTag(),"config")==0)
				{
					const char *pName=xmlChild->getAttr("name");
					const char *pVal=xmlChild->getAttr("value");

					if (pName && pVal)
					{
						ProcessOnlinePermission(pName,pVal);
					}
				}
			}
		}
	}

	parser->Release();

	m_scheduleRemoveIDataListener=true; // permissions.xml can now be polled during multiplayer to look for updated patches. We need to stop listening to this resource once we've used it
}

void CStatsRecordingMgr::DataFailedToDownload(
	CDownloadableResourcePtr		inResource)
{
	// leave values at defaults
	CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Failed to download online data posting permissions, will use defaults");
	m_scheduleRemoveIDataListener=true;
	m_submitPermissions=0;
}

bool CStatsRecordingMgr::IsTrackingEnabled()
{
	ITelemetryCollector		*tc=static_cast<CGame*>(g_pGame)->GetITelemetryCollector();
	bool					enabled=tc != NULL && tc->ShouldSubmitTelemetry();
	return ((enabled && gEnv->bMultiplayer && gEnv->bServer && g_pGameCVars->g_telemetry_gameplay_enabled) ||
			(!gEnv->bMultiplayer && g_pGameCVars->g_telemetryEnabledSP != 0));
}

void CStatsRecordingMgr::BeginSession()
{
	if (IsTrackingEnabled())
	{
		if (m_statsStorage->IsLockedForSerialization())
		{
			CryLog("** Previous session's stats are still being serialized and uploaded, will not be able to record stats for this session");
			assert(m_sessionTracker==NULL);
		}
		else
		{
			if (CGameRules *pGameRules = g_pGame->GetGameRules())
				pGameRules->AddHitListener(this);

			m_gameStats->PushGameScope(eGSC_Session);
			CRY_ASSERT(m_sessionTracker);

			// record the initial configuration
			if(m_sessionTracker)
			{
				m_sessionTracker->StateValue(eGSS_Config, g_pGameCVars->g_telemetryConfig);
			}
		}
	}
	
	SetNewSessionId(true);
	m_matchBonusXp.clear();

	m_environmentalWeapons.clear();
	m_environmentalWeapons.reserve( MAX_ENVIRONMENTAL_WEAPONS );
	m_interactiveObjects.clear();
	m_interactiveObjects.reserve( MAX_INTERACTIVE_OBJECTS );
}

int CStatsRecordingMgr::GetIntAttributeFromProfile(IPlayerProfile *inProfile, const char *inAttributeName)
{
	TFlowInputData data;
	int retData=0;

	CRY_ASSERT(inProfile);
	CRY_ASSERT(inAttributeName);

	if (inProfile->GetAttribute(inAttributeName, data))
	{
		switch(data.GetType())
		{
			case eFDT_Int:
			{
				const int *pIntData = data.GetPtr<int>();
				CRY_ASSERT(pIntData);
				retData = *pIntData;
				break;
			}
			case eFDT_Float:
			{
				const float *pFloatData = data.GetPtr<float>();
				CRY_ASSERT(pFloatData);
				retData = static_cast<int>(*pFloatData);
				break;
			}
			case eFDT_String:
			{
				const string *pStringData = data.GetPtr<string>();
				CRY_ASSERT(pStringData);
				retData = atoi(pStringData->c_str());
				break;
			}
			default:
				CRY_ASSERT_MESSAGE(0, string().Format("unexpected attribute data type = %d", data.GetType()).c_str());
				break;
		}
	}

	return retData;
}

void CStatsRecordingMgr::StateEndOfSessionStats()
{
	if (m_sessionTracker)
	{
		if (m_statsStorage)
		{
			m_sessionTracker->StateValue(eGSS_DataTruncated,m_statsStorage->IsDataTruncated());
			m_sessionTracker->StateValue(eGSS_MemRequired,m_statsStorage->GetTotalSessionMemoryRequests());
			m_sessionTracker->StateValue(eGSS_MemAvailable,m_statsStorage->GetBufferCapacity());
			m_statsStorage->ResetUsageCounters();
		}

		SFileVersion ver = gEnv->pSystem->GetFileVersion();
		m_sessionTracker->StateValue(eGSS_BuildNumber,ver.v[0]);

		m_sessionTracker->StateValue(eGSS_StatsFileFormatVersion,1);

		// lobby version is useful for online segregation and seeing which telemetry has come from EA releases, EPD, Tech200 etc
		m_sessionTracker->StateValue(eGSS_LobbyVersion,GameLobbyData::GetVersion());
		m_sessionTracker->StateValue(eGSS_MatchMakingVersion,g_pGameCVars->g_MatchmakingVersion);
		m_sessionTracker->StateValue(eGSS_MatchMakingBlock,g_pGameCVars->g_MatchmakingBlock);

		if (CDataPatchDownloader *pDP=g_pGame->GetDataPatchDownloader())
		{
			m_sessionTracker->StateValue(eGSS_PatchId,pDP->GetPatchId());
		}

		if (CPatchPakManager *pPatchPakManager = g_pGame->GetPatchPakManager())
		{
			int patchPakVersion = pPatchPakManager->GetPatchPakVersion();
			m_sessionTracker->StateValue(eGSS_PatchPakVersion, patchPakVersion);
		}

		if( IPlayerProfileManager *pProfileMan = gEnv->pGameFramework->GetIPlayerProfileManager() )
		{
			const char *user = pProfileMan->GetCurrentUser();
			IPlayerProfile* pProfile = pProfileMan->GetCurrentProfile( user );
			if (pProfile)
			{
				int runOnceVersion=GetIntAttributeFromProfile(pProfile, "RunOnceVersion");
				int runOnceTrackingVersion=GetIntAttributeFromProfile(pProfile, "RunOnceTrackingVersion");

				m_sessionTracker->StateValue(eGSS_RunOnceVersion, runOnceVersion);
				m_sessionTracker->StateValue(eGSS_RunOnceTrackingVersion, runOnceTrackingVersion);
			}
		}

		if(!gEnv->bMultiplayer)
		{
			m_sessionTracker->StateValue(eGSS_Difficulty, g_pGameCVars->g_difficultyLevel);
			
#if !defined(_RELEASE)															// do not use in MP, TRC violation to store users names, can only store their guids
			IPlatformOS::TUserName name;
			IPlatformOS* pOS = GetISystem()->GetPlatformOS();
			pOS->UserGetName(pOS->GetFirstSignedInUser(), name);
			m_sessionTracker->StateValue(eGSS_UserName, name.c_str());		
#endif
		}
		else
		{
			if (m_matchBonusXp.size() > 0)
			{
				XmlNodeRef resNode=m_gameStats->CreateStatXMLNode();
				CryFixedArray<SBonusMatchXp, MAX_PLAYER_LIMIT>::iterator itBonusXp;
				for (itBonusXp = m_matchBonusXp.begin(); itBonusXp != m_matchBonusXp.end(); ++itBonusXp)
				{
					XmlNodeRef childNode=m_gameStats->CreateStatXMLNode("player");
					childNode->setAttr( "entity_id", itBonusXp->entityId);
					CGameLobby* pGameLobby = g_pGame->GetGameLobby();
					
					const char* szOnlineGUID = pGameLobby ? pGameLobby->GetGUIDFromActorID(itBonusXp->entityId) : "";
#if defined( _RELEASE )
					const uint32 guidHash = CCrc32::Compute( szOnlineGUID );
					stack_string clientHashStr;
					clientHashStr.Format( "%lu", guidHash );
					childNode->setAttr( "online_guid", clientHashStr.c_str() );
#else
					childNode->setAttr( "online_guid", szOnlineGUID );
#endif

					childNode->setAttr( "amount", itBonusXp->amount);
					resNode->addChild(childNode);
				}
				
				m_sessionTracker->StateValue( eGSS_MatchBonusXp, m_gameStats->WrapXMLNode(resNode));
			}

			if( m_interactiveObjects.size() > 0 || m_environmentalWeapons.size() > 0 )
			{
				XmlNodeRef interactNode = m_gameStats->CreateStatXMLNode();
				
				//iterate interactive objects
				std::vector<SIntrestEntity>::iterator itObjects;
				for( itObjects = m_interactiveObjects.begin(); itObjects != m_interactiveObjects.end(); ++itObjects )
				{
					XmlNodeRef objectNode = m_gameStats->CreateStatXMLNode( "interactObject" );

					IEntity* pEntity = gEnv->pEntitySystem->GetEntity( itObjects->entityId );
					if( pEntity )
					{
						objectNode->setAttr( "entity_id", itObjects->entityId );
						objectNode->setAttr( "type", itObjects->type );

						const char* name = pEntity->GetName();
						objectNode->setAttr( "name", name );
												
						if( pEntity->GetArchetype() )
						{
							const char* arcName = pEntity->GetArchetype()->GetName();
							objectNode->setAttr( "arcName", arcName );
						}

						interactNode->addChild( objectNode );
					}
				}
				
				//iterate environmental weapons
				std::vector<SIntrestEntity>::iterator itWeapons;
				for( itWeapons = m_environmentalWeapons.begin(); itWeapons != m_environmentalWeapons.end(); ++itWeapons )
				{
					XmlNodeRef weaponNode = m_gameStats->CreateStatXMLNode( "envWeapon" );

					IEntity* pEntity = gEnv->pEntitySystem->GetEntity( itWeapons->entityId );
					if( pEntity )
					{
						weaponNode->setAttr( "entity_id", itWeapons->entityId );
						weaponNode->setAttr( "type", itWeapons->type );

						const char* name = pEntity->GetName();
						weaponNode->setAttr( "name", name );

						if( pEntity->GetArchetype() )
						{
							const char* arcName = pEntity->GetArchetype()->GetName();
							weaponNode->setAttr( "arcName", arcName );
						}
						

						interactNode->addChild( weaponNode );
					}
				}

				//tag and add group
				m_sessionTracker->StateValue( eGSS_Interactables, m_gameStats->WrapXMLNode( interactNode ) );

			}
		}
	}
}

void CStatsRecordingMgr::Update(float frameTime)
{
	if (m_endSessionCountdown > 0.f)
	{
		m_endSessionCountdown -= frameTime;
		if (m_endSessionCountdown <= 0.f)
		{
			EndSession();
		}
	}

	if (m_scheduleRemoveIDataListener)
	{
		if (m_IsIDataListener)
		{
			CDownloadMgr							*pDL=g_pGame->GetDownloadMgr();

			if (pDL)
			{
				CDownloadableResourcePtr	pRes=pDL->FindResourceByName("permissions");

				if (pRes.get())
				{
					pRes->RemoveDataListener(this);
					m_IsIDataListener=false;
				}
			}			
		}
		else
		{
			CryLog("CStatsRecordingMgr::Update() had scheduled an IDataListener removal but ended up not actually being a listener. This shouldn't happen!");
		}

		m_scheduleRemoveIDataListener=false;
	}
}

void CStatsRecordingMgr::EndSessionNowIfQueued()
{
	if (m_endSessionCountdown > 0.f)
	{
		EndSession();
	}
}

void CStatsRecordingMgr::EndSession(float delay)
{
	if (delay > 0.f)
	{
		m_endSessionCountdown = delay;
	}
	else
	{
		m_endSessionCountdown = 0.f;
		if (m_sessionTracker)
		{
			EndRound();

			StateEndOfSessionStats();

			CCircularXMLSerializer		*pCircSer=NULL;

			// popping the session scope will trigger serialisation, register our circular buffer stats serialiser here, which will lock the circular buffer until serialisation is complete
			if (m_statsStorage->IsUsingCircularBuffer() && (g_pGameCVars->g_telemetry_serialize_method&1))
			{
				pCircSer=new CCircularXMLSerializer(m_statsStorage);
			}

			m_gameStats->PopGameScope(eGSC_Session);
			assert(m_sessionTracker==NULL);

			if (pCircSer)
			{
				SaveSessionData(pCircSer);		// adopts passed producer, serializer is now on a one way trip to deletion
				pCircSer=NULL;
			}
		}

		if (CGameRules *pGameRules = g_pGame->GetGameRules())
			pGameRules->RemoveHitListener(this);
		
		//SetNewSessionId();	// Don't do this here because otherwise the persistant stats get saved in a different session id
		m_requestNewSessionId = true;	//but let's do it later instead of not at all
	}
}

void CStatsRecordingMgr::BeginRound()
{
	if (IsTrackingEnabled() && m_sessionTracker)
	{
		CRY_ASSERT_MESSAGE(m_sessionTracker,"BeginRound() called when no session is open");
		if (!m_sessionTracker)
		{
			BeginSession();
		}
		CRY_ASSERT_MESSAGE(m_gameStats->GetScopeID()==eGSC_Session,"stats error, session scope not active as aspected");
		m_gameStats->PushGameScope(eGSC_Round);
		assert(m_roundTracker);		// tracker will have been assigned and setup by callback
									// the reason for doing it in the callback rather than here is to allow the push to work from lua

		if (!gEnv->bMultiplayer)
		{
			// add all players that are already spawned into the stats tracking system

			// AI aren't in the gamerules player list,
			//	so add all actors here instead
			IActorSystem *pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
			IActorIteratorPtr it = pActorSystem->CreateActorIterator();
			while (IActor *pActor = it->Next())
			{
				if(m_checkpointCount != 0 && pActor->GetEntity()->IsActivatedForUpdates())
				{
					CRY_ASSERT_MESSAGE(strcmp(pActor->GetEntity()->GetName(), "PoolEntity"), "Support for pooled entities has been deprecated.");
					StartTrackingStats(pActor);
				}
			}
		}
	}
}

void CStatsRecordingMgr::StateRoundWinner(
	int			inWinningTeam)
{
	if (m_roundTracker)
	{
		m_roundTracker->StateValue(eSS_Winner,inWinningTeam);
	}
}

void CStatsRecordingMgr::EndRound()
{
	if (m_roundTracker)
	{
		StopTrackingAllPlayerStats();		// players cache their IStatsTracker which are about to be popped, stop tracking
		m_roundTracker->StateValue(eGSS_RoundEnd, gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI).GetMilliSecondsAsInt64());
		m_gameStats->PopGameScope(eGSC_Round);
		m_roundTracker=NULL;
	}
}

void CStatsRecordingMgr::RoundActuallyStarted()
{
	if (m_roundTracker)
	{
		int64 currentTime = gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI).GetMilliSecondsAsInt64();
		m_roundTracker->StateValue(eGSS_RoundActuallyStarted, currentTime);
	}
}

void CStatsRecordingMgr::AddTeam(
	int			inTeamId,
	string		inTeamName)
{
	if (IsTrackingEnabled() && m_sessionTracker)
	{
		IStatsTracker* track = m_gameStats->AddGameElement(SNodeLocator(eGSEL_Team, eGSC_Session, eGNLT_TeamID, inTeamId));
		CRY_ASSERT(track);
		if(track)
		{
			track->StateValue(eGSS_TeamName, inTeamName.c_str());		// FIXME SStatAnyValue is missing constructor for CryString and will only do a shallow copy of this string
			track->StateValue(eGSS_TeamID, inTeamId);
			if (inTeamId >= 0 && inTeamId <= MAX_TEAM_INDEX)
			{
				CRY_ASSERT_MESSAGE(m_teamTrackers[inTeamId] == NULL, "Trying to add a team with the same team id");
				m_teamTrackers[inTeamId] = track;
			}
			else
			{
				CRY_ASSERT_MESSAGE(false, "Team ID is out of range, consider increasing MAX_TEAM_INDEX if necessary");
			}
		}
	}
}

void CStatsRecordingMgr::OnTeamScore(int teamId, int score, EGameRulesScoreType pointsType)
{
	if (teamId >= 0 && teamId <= MAX_TEAM_INDEX)
	{
		IStatsTracker* track = m_teamTrackers[teamId];
		CRY_ASSERT(track);
		if (track)
		{
			track->Event(eSE_Score, new CScoreIncEvent(score, pointsType));
		}
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "Team ID is out of range, consider increasing MAX_TEAM_INDEX if necessary");
	}
}

void CStatsRecordingMgr::OnMatchBonusXp(IActor* pActor, int amount)
{
	m_matchBonusXp.push_back(SBonusMatchXp(pActor->GetEntityId(), amount));
}

void CStatsRecordingMgr::InteractiveObjectEntry( EntityId entity, int type )
{
	m_interactiveObjects.push_back(SIntrestEntity( entity, type ));
}

void CStatsRecordingMgr::EnvironmentalWeaponEntry( EntityId entity, int type )
{
	m_environmentalWeapons.push_back(SIntrestEntity( entity, type ));
}

void CStatsRecordingMgr::MakeValidXmlString(stack_string &str)
{
	// check if str contains any invalid characters
	str.replace("&","&amp;");
	str.replace("\"","&quot;");
	str.replace("\'","&apos;");
	str.replace("<","&lt;");
	str.replace(">","&gt;");
}

// state the basic player values needed once a tracker is added for a player
void CStatsRecordingMgr::StateCorePlayerStats(
	IActor			*inPlayerActor)
{
	// when using modules, this could actually be moved to the OnPlayerRevived() handler in the stats module. but when using lua we still need it to run so call it here
	// if the stats tracker has been created for the player
	if ( IStatsTracker *tracker = GetStatsTracker(inPlayerActor) )
	{
		EntityId		eid=inPlayerActor->GetEntityId();

		const char* szOnlineGUID = g_pGame->GetGameLobby() ? g_pGame->GetGameLobby()->GetGUIDFromActorID(inPlayerActor->GetEntityId()) : "";

#if ! defined( _RELEASE )
		stack_string playerName = inPlayerActor->GetEntity()->GetName();
		MakeValidXmlString(playerName);
		tracker->StateValue( eSS_PlayerName,	playerName.c_str());

		tracker->StateValue( eGSS_OnlineGUID,	szOnlineGUID );
#else
		const uint32 guidHash = CCrc32::Compute( szOnlineGUID );
		stack_string guidHashStr;
		guidHashStr.Format( "%lu", guidHash );

		tracker->StateValue( eGSS_OnlineGUID,	guidHashStr.c_str() );
#endif
	
		tracker->StateValue( eSS_ProfileId,		CStatHelpers::GetProfileId(inPlayerActor->GetChannelId()) );
		tracker->StateValue( eSS_EntityId,		static_cast<int>(eid) );
		int teamId = g_pGame->GetGameRules()->GetTeam(eid);
		if(!gEnv->bMultiplayer)
		{
			IAIObject* pAI = inPlayerActor->GetEntity()->GetAI();
			if(pAI)
			{
				IFactionMap& factions = gEnv->pAISystem->GetFactionMap();
				tracker->StateValue(eGSS_AIFaction, factions.GetFactionName(pAI->GetFactionID()));
			}

			teamId = inPlayerActor->IsPlayer() ? 1 : 0;
		}
		else
		{
			if (inPlayerActor->IsPlayer())
			{
				CPlayer* pPlayer = (CPlayer*)inPlayerActor;
				int xp, rank, defaultMode, stealth, armour, reincarnations;
				pPlayer->GetPlayerProgressions(&xp, &rank, &defaultMode, &stealth, &armour, &reincarnations);
				tracker->StateValue(eGSS_XP, xp);
				tracker->StateValue(eGSS_Rank, rank);
				tracker->StateValue(eGSS_DefaultRank, defaultMode);
				tracker->StateValue(eGSS_StealthRank, stealth);
				tracker->StateValue(eGSS_ArmourRank, armour);
				tracker->StateValue(eGSS_Reincarnations, reincarnations);

				if (CGameLobby* pGameLobby=g_pGame->GetGameLobby())
				{
					uint16	skill=pGameLobby->GetSkillRanking(inPlayerActor->GetChannelId());
					tracker->StateValue(eGSS_SkillLevel,skill);
				}
			}
		}
		tracker->StateValue( eSS_Team,			teamId );

		float health = inPlayerActor->GetHealth();
		tracker->StateValue( eGSS_MaxHealth, health);

		//tracker->Event( eSE_Revive );

		StateActorWeapons(inPlayerActor);
	}
}

// saves the current set of weapons for an actor to the stats tracker
void CStatsRecordingMgr::StateActorWeapons(
	IActor				*inActor)
{
	XmlNodeRef			resNode=0;
	IStatsTracker		*tracker=GetStatsTracker(inActor);
	IItemSystem			*is=g_pGame->GetIGameFramework()->GetIItemSystem();

	CRY_ASSERT_MESSAGE(tracker,"actor not currently in stats tracker, weapons won't be recorded - call ordering problem?");

	if (tracker)
	{
		IInventory *pInv = inActor->GetInventory();

		CRY_ASSERT_MESSAGE(pInv,"expected actor to have an inventory");

		int			invCount=pInv->GetCount();

		if (invCount>0)
		{
			for (int invIndex=0; invIndex<invCount; invIndex++)
			{
				EntityId		eid=pInv->GetItem(invIndex);
				IItem			*item=is->GetItem(eid);
				IWeapon			*weap = item ? item->GetIWeapon() : NULL;
				if (weap)
				{
					// add weapon
					if (!resNode)
					{
						resNode=m_gameStats->CreateStatXMLNode();
					}

					XmlNodeRef		childNode=m_gameStats->CreateStatXMLNode("weapon");
					childNode->setAttr( "name", static_cast<CWeapon*>(weap)->GetName());
					childNode->setAttr( "amount", 1 );
					childNode->setAttr( "initial_loadout", 1);

					CItem* pItem = (CItem*)item;
					const CItem::TAccessoryArray& accessories = pItem->GetAccessories();
					CItem::TAccessoryArray::const_iterator itAccessory;
					for (itAccessory = accessories.begin(); itAccessory != accessories.end(); ++itAccessory)
					{
						const SAccessoryParams* pParams = pItem->GetAccessoryParams(itAccessory->pClass);
						if (pParams)
						{
							if (pParams->category == g_pItemStrings->bottom)
								childNode->setAttr("bottom_attachment_class", itAccessory->pClass->GetName());
							else if (pParams->category == g_pItemStrings->barrel)
								childNode->setAttr("barrel_attachment_class", itAccessory->pClass->GetName());
							else if (pParams->category == g_pItemStrings->scope)
								childNode->setAttr("scope_attachment_class", itAccessory->pClass->GetName());
							else
								CRY_ASSERT_MESSAGE(pParams->category.empty(), string().Format("Unrecognised attachment category %s", pParams->category.c_str()));
						}
						else
						{
							CRY_ASSERT_MESSAGE(0, "Unable to find accessory params");
						}
					}

					resNode->addChild(childNode);
				}
			}
		}
		else
		{
			CryLog("Stats recording - empty inv for actor '%s'",inActor->GetActorClassName());
		}

		if (resNode)
		{
			tracker->StateValue( eSS_Weapons, m_gameStats->WrapXMLNode(resNode));
		}
	}
}

// returns the stats tracker (if any) for the given actor
IStatsTracker* CStatsRecordingMgr::GetStatsTracker(
	IActor			*inActor)
{
	if( inActor )
	{
		IStatsTracker	*result=NULL;

		CActor		*pActor = static_cast<CActor*>(inActor);
		result = pActor->m_telemetry.GetStatsTracker();
	
		return result;
	}

	return NULL;
}

void CStatsRecordingMgr::StopTrackingAllPlayerStats()
{
	CGameRules::TPlayers		players;
	IActorSystem				*as=g_pGame->GetIGameFramework()->GetIActorSystem();

	// AI aren't in the gamerules player list,
	//	so use actors here instead (the previous approach didn't work in SP)
	IActorSystem *pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
	IActorIteratorPtr it = pActorSystem->CreateActorIterator();
	while (IActor *pActor = it->Next())
	{
		StopTrackingStats(pActor);
	}
}

void CStatsRecordingMgr::StartTrackingStats(
	IActor		*inActor)
{
	if (IsTrackingEnabled() && m_roundTracker)
	{
#if !defined(_RELEASE)
		if( inActor->GetEntity()->GetClass() == m_pDummyEntityClass  )
		{
			return;
		}
#endif // !defined(_RELEASE)

		CActor			*pActor=static_cast<CActor*>(inActor);

		CryLog ("[STATS TRACKER] Start tracking stats for %s '%s'%s", pActor->GetEntity()->GetClass()->GetName(), pActor->GetEntity()->GetName(), pActor->m_telemetry.GetStatsTracker() ? " (WARNING: there are already stats for this actor)" : "");

		// CE-9958: Unreproducible. The stats tracker seems not being properly removed for the killed player.
		if (pActor->m_telemetry.GetStatsTracker())
		{
			return;
		}

		if (inActor->IsPlayer())
		{
			CRY_ASSERT_TRACE(!pActor->m_telemetry.GetStatsTracker(),("already tracking stats for player %s\n",inActor->GetEntity()->GetName()));
			m_gameStats->AddGameElement(SNodeLocator(eGSEL_Player,eGSC_Round,eGNLT_ChannelID,inActor->GetChannelId()),NULL);
			CRY_ASSERT(pActor->m_telemetry.GetStatsTracker());
		}
		else
		{
			CRY_ASSERT_TRACE(!pActor->m_telemetry.GetStatsTracker(),("already tracking stats for actor %s\n",inActor->GetEntity()->GetName()));
			m_gameStats->AddGameElement(SNodeLocator(eGSEL_AIActor,eGSC_Round,eNLT_EntityID,inActor->GetEntityId()),NULL);
			CRY_ASSERT(pActor->m_telemetry.GetStatsTracker());
		}
	}
}

void CStatsRecordingMgr::StopTrackingStats(
	IActor		*inActor)
{
	CActor			*pActor=static_cast<CActor*>(inActor);
	IStatsTracker	*tracker=pActor->m_telemetry.GetStatsTracker();

	CryLog ("[STATS TRACKER] Stop tracking stats for %s '%s'%s", pActor->GetEntity()->GetClass()->GetName(), pActor->GetEntity()->GetName(), tracker ? "" : " (NB: there are currently no stats for this actor)");

	if (tracker)
	{
		m_gameStats->RemoveElement(m_gameStats->GetTrackedNode(tracker));
		CRY_ASSERT(pActor->m_telemetry.GetStatsTracker()==NULL);
	}
}

void CStatsRecordingMgr::FreeTelemetryMemBuffer(
	void						*pInUserData)
{
	IXmlStringData	*pData=static_cast<IXmlStringData*>(pInUserData);
	pData->Release();
}

// takes the passed telemetry producer and uses it to upload telemetry to the telemetry server
// also writes a copy to disk if required
// producer is adopted immediately and should not be used after this function
void CStatsRecordingMgr::SaveSessionData(
	ITelemetryProducer				*pInProducer)
{
	bool											submitted=false;
	CTelemetryCollector				*pTC=static_cast<CTelemetryCollector*>(static_cast<CGame*>(g_pGame)->GetITelemetryCollector());
	if (pTC != NULL && (GetSubmitPermissions()&k_submitStatsLogs))
	{
		string									filePath, remotePath;
		const char							*pPrefix="Game";

		// if also serializing out old style, save this with a 'New' prefix
		if (g_pGameCVars->g_telemetry_serialize_method&2)
		{
			pPrefix="New";
		}

		GetUniqueFilePath(pPrefix,&filePath,&remotePath);

		if (g_pGameCVars->g_telemetry_gameplay_save_to_disk)
		{
			ITelemetryProducer			*pSaveToFile=new CTelemetrySaveToFile(pInProducer,filePath);
			pInProducer=pSaveToFile;
		}

		if (g_pGameCVars->g_telemetry_gameplay_gzip)
		{
			ITelemetryProducer			*pComp=new CTelemetryCompressor(pInProducer);

			pInProducer=pComp;
			filePath+=".gz";
			remotePath+=".gz";
		}
		
		if (g_pGameCVars->g_telemetry_gameplay_copy_to_global_heap)
		{
			m_copierThread = new CTelemetryCopierThread(pInProducer, pTC, remotePath);
		}
		else
		{
			pTC->SubmitTelemetryProducer(pInProducer,remotePath.c_str());
		}

		submitted=true;
	}

	if (!submitted)
	{
		if (GetSubmitPermissions()&k_submitStatsLogs)
		{
			CryLog("Failed to submit game telemetry");
		}
		else
		{
			CryLog("Did not submit stats logs as online permissions forbade it");
		}
		delete pInProducer;
		GetISystem()->GetXmlUtils()->FlushStatsXmlNodePool();
	}
}

void CStatsRecordingMgr::FlushData(void)
{
	if (m_copierThread)
	{
		m_copierThread->Flush();
		SAFE_DELETE(m_copierThread);
	}

	if( m_requestNewSessionId )
	{
		SetNewSessionId(false);
		m_requestNewSessionId = false;
	}
}

void CStatsRecordingMgr::GetUniqueFilePath(
	const char							*pInPrefix,
	string									*pLocalPath,
	string									*pRemotePath)
{
	static const int MAX_SAVE_ATTEMPTS = 20;

	string timeLabel;
	time_t offsetSeconds = 0;

	while (offsetSeconds < MAX_SAVE_ATTEMPTS)
	{
		timeLabel = GetTimeLabel(offsetSeconds);
		pLocalPath->Format("%s/%s_%s.xml", m_statsDirectory.c_str(), pInPrefix, timeLabel.c_str());
		// USER folder kept for backwards compatibility...
		pRemotePath->Format("USER/StatsLogs/%s_%s.xml", pInPrefix, timeLabel.c_str());

		if ((!g_pGameCVars->g_telemetry_gameplay_save_to_disk) || (!gEnv->pCryPak->IsFileExist(pLocalPath->c_str())))
		{
			break;
		}

		++offsetSeconds;
	}

	if (MAX_SAVE_ATTEMPTS == offsetSeconds)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Too many attempts to find statistics session name. Will overwrite prev file...");
	}
}

// this method will not work in release as it requires writing to disk and then reading back
// note, rather than saving to file, it is possible to use node->getXML() then send directly from memory - however, xmlstrings (which is what is returned)
// are limited in length to 64k on consoles which is not large enough
// better to use the g_telemetry_serialize_method==2
void CStatsRecordingMgr::SaveSessionData(XmlNodeRef node)
{
	string filePath, remotePath;
	const char *pPrefix="Game";

	// if also serializing out new style, save this with an 'Old' prefix
	if (m_statsStorage->IsUsingCircularBuffer() && (g_pGameCVars->g_telemetry_serialize_method&1))
	{
		pPrefix="Old";
	}
	
	GetUniqueFilePath(pPrefix,&filePath,&remotePath);

	static const int k_chunkSize=12*1024;		// on consoles, xmlnode::savetofile won't write in chunks larger than ~15000 bytes

	FILE	*file=gEnv->pCryPak->FOpen( filePath.c_str(),"wt");
	bool	success=false;

	if (file)
	{
		success=node->saveToFile(filePath.c_str(),k_chunkSize,file);
		gEnv->pCryPak->FClose(file);
	}

	if (success)
	{
		CTelemetryCollector		*pTC=static_cast<CTelemetryCollector*>(static_cast<CGame*>(g_pGame)->GetITelemetryCollector());
		if (pTC)
		{
			ITelemetryProducer		*pProd=new CTelemetryFileReader(filePath.c_str(),0);

			if (g_pGameCVars->g_telemetry_gameplay_gzip)
			{
				CTelemetryCompressor				*pComp=new CTelemetryCompressor(pProd);

				pProd=pComp;
				pComp=NULL;
				filePath+=".gz";
				remotePath+=".gz";
			}

			pTC->SubmitTelemetryProducer(pProd,remotePath.c_str());
		}
	}
}

//////////////////////////////////////////////////////////////////////////

string CStatsRecordingMgr::GetTimeLabel(time_t offsetSeconds) const
{
	CryFixedStringT<128> timeStr;
	time_t ltime;

	time( &ltime );
	ltime += offsetSeconds;
	tm *today = localtime( &ltime );
	strftime(timeStr.m_str, timeStr.MAX_SIZE, "%y-%m-%d_%H%M%S", today);

	return string(timeStr.c_str());
}

//////////////////////////////////////////////////////////////////////////

void CStatsRecordingMgr::OnNodeAdded(const SNodeLocator& locator)
{
	IStatsTracker *tracker = g_pGame->GetIGameFramework()->GetIGameStatistics()->GetTracker(locator);
	CRY_ASSERT(tracker);
	if(!tracker) return;

	if(locator.isScope()) // Scope pushed
	{
		switch(locator.scopeID)
		{
			case eGSC_Round:
				{
					CRY_ASSERT_MESSAGE(!m_roundTracker,"BeginRound() called when a round is already in progress");
					if (m_roundTracker)
					{
						EndRound();
					}
					m_roundTracker=tracker;
					int64 currentTime = gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI).GetMilliSecondsAsInt64();
					tracker->StateValue(eGSS_RoundBegin, currentTime);

					if(!gEnv->bMultiplayer)
					{
						tracker->StateValue(eGSS_LifeNumber, m_lifeCount + 1) ;
						tracker->StateValue(eGSS_RoundNumber, m_checkpointCount + 1);
					}
					else
					{
						CGameRules* pGameRules=g_pGame->GetGameRules();
						if (pGameRules)
						{
							IGameRulesRoundsModule* pRoundsModule = pGameRules->GetRoundsModule();
							if (pRoundsModule)
							{
								tracker->StateValue(eGSS_RoundNumber, pRoundsModule->GetRoundNumber());
								tracker->StateValue(eGSS_PrimaryTeam, pRoundsModule->GetPrimaryTeam());
							}
							if (pGameRules->HasGameActuallyStarted())
							{
								tracker->StateValue(eGSS_RoundActuallyStarted, currentTime);
							}
						}
					}
				}
				break;

			case eGSC_Session:
				{
					CRY_ASSERT_MESSAGE(!m_sessionTracker,"BeginSession() called when session already active");
					if (m_sessionTracker)
					{
						EndSession();
					}
					m_sessionTracker=tracker;
				}
				break;
		}
	}
	else // Element added
	{
		switch(locator.elemID)
		{
			case eGSEL_Player:
				{
					CRY_ASSERT(locator.scopeID == eGSC_Round);
					CRY_ASSERT(locator.locatorType == eGNLT_ChannelID);

					tracker->Event( eSE_Lifetime, "begin" );

					int		channel=locator.locatorValue;
					IActor	*actor=g_pGame->GetGameRules()->GetActorByChannelId(channel);
					CRY_ASSERT_TRACE(actor,("Failed to find actor for channel %d",channel));
					if (actor)
					{
						if (actor->IsPlayer())
						{
							CPlayer	*player=static_cast<CPlayer*>(actor);
							CRY_ASSERT_MESSAGE(player->m_telemetry.GetStatsTracker()==NULL,"setting stats tracker for player, but one is already set?!");
							player->m_telemetry.SetStatsTracker(tracker);
							StateCorePlayerStats(actor);
							RecordCurrentWeapon(player);
						}
					}
				}
				break;

			case eGSEL_AIActor:
				{
					CRY_ASSERT(locator.scopeID == eGSC_Round);
					CRY_ASSERT(locator.locatorType == eNLT_EntityID);

					tracker->Event( eSE_Lifetime, "begin" );

					EntityId id = locator.locatorValue;
					CActor	*pActor=static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(id));
					CRY_ASSERT_TRACE(pActor,("Failed to find actor %d",id));
					if (pActor)
					{
						CRY_ASSERT(!pActor->IsPlayer());
						CRY_ASSERT_MESSAGE(pActor->m_telemetry.GetStatsTracker()==NULL,"setting stats tracker for AI, but one is already set?!");

						//////////////////////////////////////////////////

						const char	*pActorName = pActor->GetEntity()->GetName();

						CryLog("%s\n", pActorName);

						//////////////////////////////////////////////////

						pActor->m_telemetry.SetStatsTracker(tracker);
						StateCorePlayerStats(pActor);	// should still be ok for AI...
						RecordCurrentWeapon(pActor);
					}
				}
				break;
		}

	}
}

//////////////////////////////////////////////////////////////////////////

void CStatsRecordingMgr::RecordCurrentWeapon(CActor* pActor)
{
	IInventory* pInventory = pActor->GetInventory();

	assert(pInventory);

	if (!pInventory)
		return;

	EntityId currentItemId = pInventory->GetCurrentItem();

	pActor->m_telemetry.SubscribeToWeapon(currentItemId);
};


//////////////////////////////////////////////////////////////////////////

void CStatsRecordingMgr::OnNodeRemoved(const SNodeLocator& locator, IStatsTracker* tracker)
{
	CRY_ASSERT(tracker);
	if(!tracker) return;

	if(locator.isScope()) // Scope popped
	{
		switch(locator.scopeID)
		{
			case eGSC_Round:
				{
					CRY_ASSERT_MESSAGE(tracker==m_roundTracker,"Stats messed up, ended a round that wasn't the round being tracked?!");
					tracker->StateValue(eGSS_RoundEnd, gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI).GetMilliSecondsAsInt64());
					m_roundTracker=NULL;
				}
				break;
			case eGSC_Session:
				{
					CRY_ASSERT_MESSAGE(tracker==m_sessionTracker,"Stats messed up, ended a session that wasn't the session being tracked?!");
					m_sessionTracker=NULL;
				}
				break;
		}

	}
	else // Element removed
	{
		switch(locator.elemID)
		{
			case eGSEL_Player:
				{
					CRY_ASSERT(locator.scopeID == eGSC_Round);
					tracker->Event( eSE_Lifetime, "end" );

					int		channel=locator.locatorValue;
					IActor	*actor=g_pGame->GetGameRules()->GetActorByChannelId(channel);
					if (actor != NULL && actor->IsPlayer())
					{
						CPlayer		*player=static_cast<CPlayer*>(actor);
						CRY_ASSERT_MESSAGE(player->m_telemetry.GetStatsTracker()==tracker || player->m_telemetry.GetStatsTracker()==NULL,"stats tracker mismatch for player");
						player->m_telemetry.SetStatsTracker(NULL);
					}
				}
				break;

			case eGSEL_AIActor:
				{
					CRY_ASSERT(locator.scopeID == eGSC_Round);
					tracker->Event( eSE_Lifetime, "end" );

					EntityId id = locator.locatorValue;
					CActor	*pActor=static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(id));
					if (pActor)
					{
						//////////////////////////////////////////////////

						const char	*pActorName = pActor->GetEntity()->GetName();

						CryLog("%s\n", pActorName);

						//////////////////////////////////////////////////

						CRY_ASSERT_MESSAGE(pActor->m_telemetry.GetStatsTracker()==tracker || pActor->m_telemetry.GetStatsTracker()==NULL,"stats tracker mismatch for AI actor");
						pActor->m_telemetry.SetStatsTracker(NULL);
					}
				}
				break;

			case eGSEL_Team:
				{
					for (int i=0; i<=MAX_TEAM_INDEX; ++i)
					{
						if (m_teamTrackers[i] == tracker)
						{
							m_teamTrackers[i] = NULL;
							break;
						}
					}
				}
				break;
		}
	}
}

// the stats recording mgr changes the telemetry collectors session id when sessions begin and end
void CStatsRecordingMgr::SetNewSessionId( bool includeMatchDetails )
{
	ITelemetryCollector		*tc=static_cast<CGame*>(g_pGame)->GetITelemetryCollector();
	if (tc)
	{
		tc->SetNewSessionId(includeMatchDetails);
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsRecordingMgr::OnHit(const HitInfo& hit)
{
	// todo: record non-actor hits - eg vehicles.
	IActor		*actor=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hit.targetId);
	if (actor)
	{
		CStatsRecordingMgr* pMgr = g_pGame->GetStatsRecorder();
		if(!pMgr || !pMgr->ShouldRecordEvent(eSE_Hit, actor))
			return;

		IGameFramework* pGameFrameWork = g_pGame->GetIGameFramework();
		IGameStatistics		*gs=pGameFrameWork->GetIGameStatistics();
		if (IStatsTracker *tracker=GetStatsTracker(actor))
		{
			char weaponClassName[128];
			if (!pGameFrameWork->GetNetworkSafeClassName(weaponClassName, sizeof(weaponClassName), hit.weaponClassId))
			{
				cry_strcpy(weaponClassName, "unknown weapon");
			}
			char projectileClassName[128];
			if (!pGameFrameWork->GetNetworkSafeClassName(projectileClassName, sizeof(projectileClassName), hit.projectileClassId))
			{
				cry_strcpy(projectileClassName, "unknown projectile");
			}

			CGameRules		*gr=g_pGame->GetGameRules();
			const char		*ht=gr->GetHitType(hit.type);

#if 0		// material type doesn't ever seem to be set for c2 characters
			ISurfaceType *pSurfaceType=gr->GetHitMaterial(hit.material);
			if (pSurfaceType)
			{
				node->setAttr("material_type", pSurfaceType->GetType());
			}
			else
			{
				node->setAttr("material_type", "unknown material");
			}
#endif

			CryFixedStringT<64> hit_part("unknown part");

			if (hit.partId >= 0)
			{
				ICharacterInstance* pCharacter = actor->GetEntity()->GetCharacter(0);
				if (pCharacter)
				{
					// 1000 is a magic number in CryAnimation that establishes the number of the first attachment identifier
					// see CAttachmentManager::PhysicalizeAttachment
					const int FIRST_ATTACHMENT_PARTID = 1000;
					if (hit.partId >= FIRST_ATTACHMENT_PARTID)
					{
						IAttachmentManager* pAttchmentManager = pCharacter->GetIAttachmentManager();
						IAttachment* pAttachment = pAttchmentManager->GetInterfaceByIndex(hit.partId - FIRST_ATTACHMENT_PARTID);
						if (pAttachment)
						{
							hit_part = "attachment "; 
							hit_part += pAttachment->GetName();
						}
					}
					else
					{
						IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
						ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
						const char* szJointName = rIDefaultSkeleton.GetJointNameByID(pSkeletonPose->getBonePhysParentOrSelfIndex(hit.partId));
						if (szJointName && *szJointName)
							hit_part = szJointName;
					}
				}
			}

			tracker->Event(eSE_Hit,new CHitStats(hit.projectileId, hit.shooterId, hit.damage, ht ? ht : "unknown hit type", weaponClassName, projectileClassName, hit_part.c_str()));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
EStatisticEventRecordType GetRecordType(const XmlString& name)
{
	EStatisticEventRecordType type = eSERT_Always;
	if(name == "never")
		type = eSERT_Never;
	else if(name == "players")
		type = eSERT_Players;
	else if(name == "ai")
		type = eSERT_AI;

	return type;
}

//////////////////////////////////////////////////////////////////////////
void CStatsRecordingMgr::LoadEventConfig(const char* configName)
{
	IGameStatistics* pGS = g_pGame->GetIGameFramework()->GetIGameStatistics();
	if(!pGS)
		return;

	if(m_sessionTracker)
	{
		GameWarning("Can't change config during gameplay");
		return;
	}

	// load an xml file containing a list of event types to enable or disable
	string fileName;
	fileName.Format("Libs/Telemetry/%s.xml", configName);
	XmlNodeRef node = GetISystem()->LoadXmlFromFile(fileName.c_str());
	if(!node || strcmp("TelemetryConfig", node->getTag()))
	{
		GameWarning("Failed to load telemetry config file %s", configName);
		return;
	}

	XmlNodeRef eventsNode = node->findChild("Events");
	if(eventsNode)
	{
		// unless otherwise specified, enable everything
		XmlString defaultEnableName = "always";
		eventsNode->getAttr("default", defaultEnableName);
		EStatisticEventRecordType defaultEnable = GetRecordType(defaultEnableName);

		// set all events to this value
		for(size_t eventIndex = 0; eventIndex < eGSE_Num; ++eventIndex)
		{
			m_eventConfigurations[eventIndex] = defaultEnable;
		}

		// override specific events
		int numEvents = eventsNode->getChildCount();
		for(int i=0; i<numEvents; ++i)
		{
			XmlNodeRef eventNode = eventsNode->getChild(i);
			XmlString name;
			XmlString enableName;
			EStatisticEventRecordType enable = defaultEnable;
			if(eventNode && eventNode->getAttr("name", name) && eventNode->getAttr("enabled", enableName))
			{
				size_t eventID = pGS->GetEventIDBySerializeName(name.c_str());
				if(eventID != INVALID_STAT_ID)
				{
					assert(eventID < eGSE_Num);
					enable = GetRecordType(enableName);
					m_eventConfigurations[eventID] = enable;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CStatsRecordingMgr::ShouldRecordEvent(size_t eventID, IActor* pActor /*=NULL*/) const
{
	assert(eventID < eGSE_Num);
	EStatisticEventRecordType type = m_eventConfigurations[eventID];
	if(type == eSERT_Always)
		return true;
	else if(type == eSERT_Never)
		return false;
	else
	{
		// if config is 'ai' or 'player' but actor isn't specified,
		//	always record
		if(!pActor)
		{
			assert(false);
			return true;
		}

		return (pActor->IsPlayer() == (type == eSERT_Players));
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsRecordingMgr::OnSaveGame(ISaveGame* pSaveGame)
{
	CGameRules* pGR = g_pGame->GetGameRules();
	if(pGR && pSaveGame && pSaveGame->GetSaveGameReason() == eSGR_FlowGraph)
	{
		pGR->SaveSessionStatistics();
		m_checkpointCount++;
		m_lifeCount = 0;	// reset life count for the new checkpoint
		pGR->InitSessionStatistics();

		BeginRound();
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsRecordingMgr::OnLoadGame(ILoadGame* pLoadGame)
{
	CGameRules* pGR = g_pGame->GetGameRules();
	if(pGR)
	{
		pGR->SaveSessionStatistics();
		m_lifeCount++;
		pGR->InitSessionStatistics();

		BeginRound();
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsRecordingMgr::OnActionEvent(const SActionEvent& event)
{
	if(event.m_event == eAE_unloadLevel)
	{
		// reset checkpoint counter at the end of the level
		m_checkpointCount = 0;
		m_lifeCount = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsRecordingMgr::Serialize(TSerialize ser)
{
	assert(ser.GetSerializationTarget() == eST_SaveGame);

	ser.Value("checkpointCount", m_checkpointCount);
}

CTelemetryCopierThread::CTelemetryCopierThread(ITelemetryProducer* pInProducer, CTelemetryCollector* telemetryCollector, string remotePath)
	: m_telemetryCollector(telemetryCollector)
	, m_pInProducer(pInProducer)
	, m_pOutProducer(NULL)
	, m_remotePath(remotePath)
#ifndef _RELEASE
	, m_time(gEnv->pTimer->GetAsyncCurTime())
	, m_runTime(0.0f)
#endif
{
	if (!gEnv->pThreadManager->SpawnThread(this, "TelemetryCopier"))
	{
		CryFatalError("Error spawning \"TelemetryCopier\" thread.");
	}
}

CTelemetryCopierThread::~CTelemetryCopierThread()
{
	if (m_pInProducer)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CTelemetryCollector: Producer wasn't deleted. Did you call flush?");
		Flush();
	}
}

void CTelemetryCopierThread::ThreadEntry()
{
#ifndef _RELEASE
	m_runTime=gEnv->pTimer->GetAsyncCurTime();
#endif

	GetISystem()->GetXmlUtils()->SetStatsOwnerThread(CryGetCurrentThreadId());
	m_pOutProducer = new CTelemetryCopier(m_pInProducer);
#ifndef _RELEASE
	m_runTime=gEnv->pTimer->GetAsyncCurTime()-m_runTime;
#endif
}

void CTelemetryCopierThread::Flush()
{
#ifndef _RELEASE
	m_time=gEnv->pTimer->GetAsyncCurTime()-m_time;
#endif
#ifndef SINGLE_THREADED_COPIER
	gEnv->pThreadManager->JoinThread(this, eJM_Join);
#endif
#ifndef _RELEASE
	CryLog("CTelemetryCopierThread had %fs to do work and took %fs", m_time, m_runTime);
#endif
	GetISystem()->GetXmlUtils()->SetStatsOwnerThread(CryGetCurrentThreadId());
	SAFE_DELETE(m_pInProducer);
	GetISystem()->GetXmlUtils()->FlushStatsXmlNodePool();
	m_telemetryCollector->SubmitTelemetryProducer(m_pOutProducer,m_remotePath);
}
