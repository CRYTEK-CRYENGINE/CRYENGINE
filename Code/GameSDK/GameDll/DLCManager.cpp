// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Responsible for loading downloadable content into the game
and informing the game of what content is loaded.

-------------------------------------------------------------------------
History:
- 18:05:2010  : Created by Martin Sherburn
- 21:08:2012  : Maintained by Andrew Blackwell

*************************************************************************/

#include "StdAfx.h"
#include "UI/WarningsManager.h"
#include "GameCVars.h"
#include "Network/Lobby/GameLobby.h"
#include "Network/Squad/SquadManager.h"

#include "PlayerProgression.h"

#include <ILevelSystem.h>
#include <CryCore/TypeInfo_impl.h>
#include <GameXmlParamReader.h>

#include "DLCManager.h"
#include "PlaylistManager.h"
#include <CryString/UnicodeFunctions.h>

static AUTOENUM_BUILDNAMEARRAY(s_standardLevelNames, NOTDLCLevelList);
static AUTOENUM_BUILDNAMEARRAY(s_standardGameModes, NOTDLCGameModeList);

#define MAX_VERSION_STRING 64

#define PRESALE_ID_OFFSET 8
#define CLIENT_SET_ENTITLEMENT_OFFSET 5

//Presale and digi pack flags (reference shipped xmls for contents)
#define PRESALE_DLC_FLAGS 0xFF00 // second 8 bits

#define STANDARD_DLC_FLAGS 0xFF	// lowest 8 bits

#define DLC_WARN_LOWEST_PRIO 10


const int k_optin_ent_index = 5;
const int k_demo_ent_index = 6;
const int k_alpha_ent_index = 7;

int nInitedPresales = 8;
static const char* k_entitlementTags[] =
{
	"C3:Preorder_Stalker",
	"C3:Preorder_Overkill",
	"C3:Preorder_Predator",
	"C3:Hunter",
	"C3_EGC_Unlocks",
	"C3:OPTIN:REWARD",
};

//not sure whether our actual dlc will end up being id 0 or 1 yet
static const char* k_dlcEntitlementTags[] =
{
	"C3:DLC1",
	"C3:DLC1"
};

CDLCManager::CDLCManager()
	: m_loadedDLCs(0)
	, m_allowedDLCs(0)
	, m_entitledDLCs(0)
	, m_warningPriority(DLC_WARN_LOWEST_PRIO)
	, m_requiredDLCs(0)
	, m_dlcLoaded(false)
	, m_allowedDLCUpToDate(false)
	, m_allowedDLCCheckFailed(false)
	, m_onlineAttributesRead(false)
	, m_bContentRemoved(false)
	, m_bContentAvailable(true)
	, m_appliedDLCStat(0)
	, m_DLCXPToAward(0)
	, m_bNewDLCAdded(false)
	, m_queueEventEntitlement(false)
	, m_entitlementTask(CryLobbyInvalidTaskID)
{
	gEnv->pSystem->GetPlatformOS()->AddListener(this, "CDLCManager");

#if defined(DLC_LOAD_ON_CONSTRUCTION)
	LoadDownloadableContent( 0 );
#endif
}

CDLCManager::~CDLCManager()
{
	if (IPlatformOS* pPlatformOS = gEnv->pSystem->GetPlatformOS())
	{
		gEnv->pSystem->GetPlatformOS()->RemoveListener(this);
	}
}

void CDLCManager::LoadDownloadableContent( uint32 userIdOveride /*= INVALID_CONTROLLER_INDEX*/ )
{
	if( m_dlcLoaded || !m_bContentAvailable )
	{
		return;
	}

	const ICmdLineArg *pNoDLCArg = gEnv->pSystem->GetICmdLine()->FindArg(eCLAT_Pre, "nodlc");
	if (pNoDLCArg != NULL)
	{
		return;
	}

// SECRET
	uint8 keyData[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
// SECRET

	uint32 userIndex;
	
	if( userIdOveride != INVALID_CONTROLLER_INDEX )
	{
		userIndex = userIdOveride;
	}
	else
	{
		IPlayerProfileManager *pPlayerProfileManager = gEnv->pGameFramework->GetIPlayerProfileManager();
		userIndex = pPlayerProfileManager ? pPlayerProfileManager->GetExclusiveControllerDeviceIndex() : INVALID_CONTROLLER_INDEX;
	}

	if( userIndex != INVALID_CONTROLLER_INDEX )
	{
		gEnv->pSystem->GetPlatformOS()->MountDLCContent(this, userIndex, keyData);
		m_dlcLoaded = true;
	}
}

void CDLCManager::DisableDownloadableContent()
{
	m_dlcLoaded = false;
	m_allowedDLCs = 0;
	m_allowedDLCUpToDate = false;
	m_allowedDLCCheckFailed = false;
	m_onlineAttributesRead = false;

	m_dlcWarning.clear();
	m_warningPriority = DLC_WARN_LOWEST_PRIO;
}

void CDLCManager::OnPlatformEvent(const IPlatformOS::SPlatformEvent& event)
{
	switch(event.m_eEventType)
	{
		case IPlatformOS::SPlatformEvent::eET_ContentInstalled:
		{
			break;
		}

		case IPlatformOS::SPlatformEvent::eET_ContentRemoved:
		{
			OnDLCRemoved(event.m_uParams.m_contentRemoved.m_root);
			m_bContentAvailable = false;

			CErrorHandling *pErrorHandling = CErrorHandling::GetInstance();
			if (pErrorHandling)
			{
				pErrorHandling->OnFatalError(CErrorHandling::eFE_ContentRemoved);
			}
			break;
		}
	}
}

void CDLCManager::OnDLCMounted(const XmlNodeRef &rootNode, const char* sDLCRootFolder)
{
	CryLog( "OnDLCMounted: '%s'", sDLCRootFolder);
	XmlString minVersion;
	XmlString sName;
	int dlcId;
	if (rootNode->getAttr("minversion", minVersion) &&
			rootNode->getAttr("name", sName) &&
			rootNode->getAttr("id", dlcId))
	{
		CryLog( "DLC Name = %s, ID = %d", sName.c_str(), dlcId );

		if (dlcId	>= 0 && dlcId < MAX_DLC_COUNT)
		{
#if (! ENTITLEMENTS_AUTHORATIVE) || defined(DEDICATED_SERVER)
			//whenever we load a dlc, it is automatically allowed
			m_allowedDLCs |= BIT(dlcId);
#endif
			if (!IsDLCReallyLoaded(dlcId))
			{
				SFileVersion currentVersion = gEnv->pSystem->GetProductVersion();
				SFileVersion minimumVersion = SFileVersion(minVersion.c_str());
				if (currentVersion < minimumVersion)
				{
					char currentVersionString[MAX_VERSION_STRING];
					currentVersion.ToString(currentVersionString);
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Unable to load DLC \"%s\" because it requires version %s and current version is %s", sName.c_str(), minVersion.c_str(), currentVersionString);
					RequestDLCWarning("DLCVersionMismatch",2, true);
				}
				else
				{
					XmlNodeRef crcNode = rootNode->findChild("crcs");

					PopulateDLCContents(rootNode, dlcId, sName.c_str() );

					//insist that CRCs are present and level folders match listed contents
					if ( !crcNode || !VerifyCRCs(crcNode, sDLCRootFolder) || !CheckLevels( dlcId, sDLCRootFolder ) )
					{
						ClearDLCContents( dlcId );
						RequestDLCWarning("DLCFileCorrupt",4, true);
						CryLog("DLC \"%s\" not loaded successfully", sName.c_str());
					}
					else
					{
						CryLog("DLC \"%s\" loaded successfully", sName.c_str());
						m_loadedDLCs |= BIT(dlcId);

						m_dlcContents[ dlcId ].root.Format( "%s", sDLCRootFolder );
				
						XmlNodeRef unlocksXml = rootNode->findChild("Unlocks");
						if(unlocksXml)
						{
							DoDLCUnlocks( unlocksXml, dlcId);
						}						

						CryFixedStringT<ICryPak::g_nMaxPath> path;

						//Level Extras pak contains things which need to be accessed relative to the Level Path
						//eg. Level meta data, icons and mini maps
						//also contains Level Names and Rich Presence mappings
						path.Format("%s/dlcLevelExtras.pak", sDLCRootFolder);
						CryLog( "DLC: Opening %s as %s", path.c_str(), sDLCRootFolder );
						bool success = gEnv->pCryPak->OpenPack( sDLCRootFolder, path );

						//Data pak contains things which need to be accessed relative to the Game Root
						//eg. Objects and Textures for new entities
						path.Format("%s/dlcData.pak", sDLCRootFolder);
						string gamePath = PathUtil::GetGameFolder();
						CryLog( "DLC: Opening %s as %s", path.c_str(), gamePath.c_str() );
						success &= gEnv->pCryPak->OpenPack( gamePath.c_str(), path );

						if (success == false)
						{
							CRY_ASSERT_MESSAGE(success, "Failed to open DLC packs");
							CryLog("Failed to open DLC packs '%s'",path.c_str());
						}
						else
						{
							//Only DLCs with data paks can have strings or levels

							path.Format("%s/", sDLCRootFolder);
							CryLog( "DLCManager: Adding %s to Mod paths", path.c_str() );
							gEnv->pCryPak->AddMod(path.c_str());

							//load string mappings for level names in this DLC
							path.Format( "%s/scripts/dlc%dnames.xml", sDLCRootFolder, dlcId );
							g_pGame->LoadMappedLevelNames( path.c_str() );

							//and load the actual localized strings
							ILocalizationManager *pLocMan = GetISystem()->GetLocalizationManager();
							path.Format( "%s/scripts/dlc%d%s.xml", sDLCRootFolder, dlcId, pLocMan->GetLanguage() );
							pLocMan->LoadExcelXmlSpreadsheet( path );

							//see if the pack has a description
							CryFixedStringT<32> descriptionKey;
							descriptionKey.Format( "dlc%d_pack_description", dlcId );
							SLocalizedInfoGame		tempInfo;
							if( pLocMan->GetLocalizedInfoByKey( descriptionKey.c_str(), tempInfo ) )
							{
								m_dlcContents[ dlcId ].descriptionStr.Format( "@%s", descriptionKey.c_str() );
							}

							//and load the Rich Presence mappings
							path.Format( "%s/scripts/dlc%dpresence.xml", sDLCRootFolder, dlcId );
							g_pGame->AddRichPresence( path.c_str() );

							//and get the Score Rewards Path
							m_dlcContents[ dlcId ].scoreRewardsPath.Format( "%s/scripts/dlc%drewards.xml", sDLCRootFolder, dlcId );

							//and the Playlists Path
							m_dlcContents[ dlcId ].playlistsPath.Format( "%s/scripts/dlc%dplaylists", sDLCRootFolder, dlcId );

							ILevelSystem *pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
							path.Format("%s/levels", sDLCRootFolder);
							CryLog("DLC Levelsystem rescan '%s'", path.c_str());
							const uint32 dlcTag = 'DLC0';
							pLevelSystem->Rescan(path.c_str(), dlcTag);
						}
					}
				}
			}
			else
			{
				CryLog("DLC %d already loaded, OK if from re-sign in", dlcId );
			}
		}
		else
		{
			CRY_ASSERT_MESSAGE(false, "DLC id is not within range");
		}
	}
	else
	{
		RequestDLCWarning("DLCXmlError",4, true);
	}
}



void CDLCManager::OnDLCMountFailed(IPlatformOS::EDLCMountFail reason)
{
	CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Unable to load DLC, error code: %d", reason);
	const char* sWarning = NULL;
	switch (reason)
	{
	case IPlatformOS::eDMF_FileCorrupt:
		sWarning = "DLCFileCorrupt";
		break;
	case IPlatformOS::eDMF_DiskCorrupt:
		sWarning = "DLCDiskCorrupt";
		break;
	case IPlatformOS::eDMF_XmlError:
		sWarning = "DLCXmlError";
		break;
	}
	if (sWarning)
	{
		RequestDLCWarning(sWarning, 4, true);
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "Unrecognised DLC error");
	}
}

void CDLCManager::OnDLCMountFinished(int nPacksFound)
{
	CryLog( "OnDLCMountFinished nPacksFound:%d", nPacksFound);

	CryLog( "DLC: Loaded DLCs flags are 0x%x", GetLoadedDLCs() );

	if( nPacksFound > 0 )
	{
		//we should rescan for any levels added by the DLCs
		ILevelSystem *pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
		pLevelSystem->Rescan("levels", ILevelSystem::TAG_MAIN);
	}
	
#if ! ENTITLEMENTS_AUTHORATIVE
	//on consoles, after DLC is loaded, we know about what DLC is allowed
	m_allowedDLCUpToDate = true;
#endif

}

void CDLCManager::OnDLCRemoved(const char* sDLCRootFolder)
{
	//clear all the data
	for( int iDLC = 0; iDLC < MAX_DLC_COUNT; iDLC++ )
	{
		if( IsDLCLoaded( iDLC ) )
		{
			if( strcmpi( m_dlcContents[iDLC].root.c_str(), sDLCRootFolder ) == 0 )
			{
				m_loadedDLCs &= ~BIT(iDLC);
				m_allowedDLCs &= ~BIT(iDLC);

				//close the paks
				CryFixedStringT<ICryPak::g_nMaxPath> path;

				path.Format("%s/dlcLevelExtras.pak", sDLCRootFolder);
				CryLog( "DLC: Closing %s", path.c_str() );
				gEnv->pCryPak->ClosePack( path.c_str() );

				path.Format("%s/dlcData.pak", sDLCRootFolder);
				CryLog( "DLC: Closing %s", path.c_str() );
				gEnv->pCryPak->ClosePack( path.c_str() );
			}

		}
	}
}

bool CDLCManager::VerifyCRCs(const XmlNodeRef &crcNode, const char* sDLCRootFolder)
{
	bool success = true;
	CryFixedStringT<ICryPak::g_nMaxPath> path;
	int numFiles = crcNode->getChildCount();
	XmlString fileName;
	uint32 storedCrc;
	for (int i=0; i<numFiles; ++i)
	{
		XmlNodeRef fileNode = crcNode->getChild(i);
		if (fileNode->getAttr("name", fileName) &&
			fileNode->getAttr("crc", storedCrc))
		{
			
#if CRY_PLATFORM_WINDOWS
			path.Format("%s/%s", sDLCRootFolder, fileName.c_str());
			bool useCryFile = true;
#else
			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "No Platform defined in DLCManager" );
			bool useCryFile = false;
#endif
			CryLog( "CRC: Checking CRC of %s", path.c_str() );

			success = gEnv->pCryPak->OpenPack( path.c_str() );

			if( !success )
			{
					CryLog( "CRC: Failed to open pack" );
			}

			uint32 computedCrc = gEnv->pCryPak->ComputeCachedPakCDR_CRC( path.c_str(), useCryFile );
			if (computedCrc != storedCrc)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CRC on file %s (%u) does not match stored value (%u)", path.c_str(), computedCrc, storedCrc);
				success = false;
			}
			gEnv->pCryPak->ClosePack( path.c_str() );
		}
	}
	return success;
}

void CDLCManager::PatchMenu(CMenuData& menu)
{
#if 0 // old frontend
	// This function is used to add additional elements to the menus that should be only available in the DLC
	// e.g. used to add additional levels and game modes
	for (int i=0; i<MAX_DLC_COUNT; ++i)
	{
		if (IsDLCLoaded(i))
		{
			// e.g. Replace file.xml with file_dlc0.xml
			CryFixedStringT<32> newExtension;
			newExtension.Format("_dlc%d.xml", i);
			stack_string path = menu.GetPath();
			PathUtil::RemoveExtension(path);
			path.append(newExtension);
			XmlNodeRef xml = GetISystem()->LoadXmlFromFile(path);
			if(xml)
			{
				CGameXmlParamReader xmlReader(xml);

				const int childXmlNodeCount = xmlReader.GetUnfilteredChildCount();
				for(int childIndex = 0; childIndex < childXmlNodeCount; ++childIndex)
				{
					const XmlNodeRef childNode = xmlReader.GetFilteredChildAt(childIndex);
					if ((childNode != NULL) && !stricmp(childNode->getTag(), "Screen"))
					{
						const char* name = childNode->getAttr("name");
						int screenIndex = menu.GetScreen(name);
						if (screenIndex >= 0)
						{
							CMenuScreen *pScreen = menu.GetScreen(screenIndex);
							pScreen->Initialize(childNode);
						}
					}
				}
			}
		}
	}
#endif
}

void CDLCManager::PopulateDLCContents(const XmlNodeRef &rootNode, int dlcId, const char* name )
{
	Unicode::Convert(m_dlcContents[dlcId].name, name);

	XmlNodeRef levelsNode = rootNode->findChild("levels");
	if (levelsNode)
	{
		XmlString levelName;
		int numLevels = levelsNode->getChildCount();

		CryLog( "Found %d levels in the DLC", numLevels );
		
		m_dlcContents[dlcId].levels.reserve(numLevels);
		for (int i=0; i<numLevels; ++i)
		{
			XmlNodeRef levelNode = levelsNode->getChild(i);
			if (levelNode->getAttr("name", levelName))
			{
				CryLog( "Found level %s and added to the DLC manager", levelName.c_str() );
				m_dlcContents[dlcId].levels.push_back(levelName);
			}
		}
	}

	XmlNodeRef bonusNode = rootNode->findChild("bonus");
	if( bonusNode )
	{
		CryLog( "DLC pak includes a pre-sale bonus" );
		uint32 bonusID = 0;
		bonusNode->getAttr("id", bonusID );
		m_dlcContents[dlcId].bonusID = bonusID;
	}

	XmlNodeRef uniqueIdNode = rootNode->findChild("uniqueId");
	if( uniqueIdNode )
	{
		uint32 uniqueID = 0;
		uniqueIdNode->getAttr("id", uniqueID );
		m_dlcContents[dlcId].uniqueID = uniqueID;
	}

	XmlNodeRef uniqueTagNode = rootNode->findChild("uniqueTag");
	if( uniqueTagNode )
	{
		const char* str =	uniqueTagNode->getAttr( "tag" );
		m_dlcContents[dlcId].uniqueTag.Format( str );
	}
}

void CDLCManager::ClearDLCContents( int dlcId )
{
	wcscpy( m_dlcContents[dlcId].name, L"" );
	m_dlcContents[dlcId].levels.clear();
	m_dlcContents[dlcId].bonusID = 0;
}

uint32 CDLCManager::GetRequiredDLCs()
{
	stack_string sCurrentLevelName;
	CGameLobby* pGameLobby = g_pGame->GetGameLobby();
	if (pGameLobby)
	{
		// This is the map currently selected in the game lobby
		sCurrentLevelName = pGameLobby->GetCurrentLevelName();
	}
	else
	{
		// This is the fallback if the lobby can't be found
		sCurrentLevelName = gEnv->pConsole->GetCVar("sv_map")->GetString();
	}
	if (strcmp(sCurrentLevelName.c_str(), m_prevLevelName.c_str()) != 0)
	{
		m_prevLevelName = sCurrentLevelName.c_str();
		m_requiredDLCs = GetRequiredDLCsForLevel(sCurrentLevelName.c_str());
	}
	return m_requiredDLCs;
}

uint32 CDLCManager::GetRequiredDLCsForLevel(const char* pLevelName)
{
	if (pLevelName[0] == 0)
	{
		// Server hasn't set a level yet, no dlc required as yet
		return 0;
	}

	int req = 0;

	// Strip off the first part from the level name (e.g. "Wars/")
	const char* pTrimmedLevelName = strrchr(pLevelName, '/');
	if (pTrimmedLevelName)
	{
		pTrimmedLevelName++;
	}
	else
	{
		pTrimmedLevelName = pLevelName;
	}

	for (int i=0; i<MAX_DLC_COUNT; ++i)
	{
		for (size_t j=0; j<m_dlcContents[i].levels.size(); ++j)
		{
			if (stricmp(m_dlcContents[i].levels[j].c_str(), pTrimmedLevelName) == 0)
			{
				req |= BIT(i);
			}
		}
	}

	//TODO: Add explioit prevention DLC level list here if needed


	if (req == 0 && !LevelExists(pLevelName))
	{
		// this means we know the level's DLC but we can't work out *which* DLC it needs
		// (probably due to this being called on a client without the requisite DLC) - so for sanity lets say it needs all of them!
		req = 0xFFffFFff;
	}

	return req;
}

bool CDLCManager::LevelExists(const char* pLevelName)
{
	ILevelSystem *pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
	return pLevelSystem && pLevelSystem->GetLevelInfo(pLevelName);
}

bool CDLCManager::IsLevelStandard(const char * levelname)
{
	// This function is currently only used for achievements and shouldn't be used as a robust way
	// to check if a level is DLC or not
	for ( int level = 0; level < eNOTDLC_NUM_LEVELS; level++ )
	{
		if ( stricmp(&s_standardLevelNames[level][8],levelname) == 0 )
		{
			return true;
		}
	}
	return false;
}

bool CDLCManager::IsGameModeStandard(const char * gamemode)
{
	// This function is currently only used for achievements and shouldn't be used as a robust way
	// to check if a game mode is DLC or not
	for ( int gmidx = 0; gmidx < eNOTDLC_NUM_GAMEMODES; gmidx++ )
	{
		if ( stricmp(&s_standardGameModes[gmidx][8],gamemode) == 0 )
		{
			return true;
		}
	}
	return false;
}

uint32 CDLCManager::GetSquadCommonDLCs()
{
	/* Squad User data is not yet functional
	CSquadManager* pSquadManager = g_pGame->GetSquadManager();
	if (pSquadManager)
	{
		uint32 commonDLCs;		
		if (pSquadManager->GetSquadCommonDLCs(commonDLCs))
		{
			return commonDLCs;
		}
	}
	*/
	// Fall back to just the local loaded and allowed DLCs if it can't be obtained from the squad manager;
	return GetLoadedDLCs();
}

int CDLCManager::GetNamesStringOfPlayersMissingDLCsForLevel(const char* pLevelName, stack_string* pPlayersString)
{
	CGameLobby*  pGameLobby = g_pGame->GetGameLobby();
	CRY_ASSERT(pGameLobby);
	CRY_ASSERT(pGameLobby->IsServer());

	int  count = 0;

	uint32  requiredDLCs = GetRequiredDLCsForLevel(pLevelName);

	const SSessionNames&  lobbySessNames = pGameLobby->GetSessionNames();

	const int  nameSize = lobbySessNames.Size();
	for (int i=0; i<nameSize; ++i)
	{
		const SSessionNames::SSessionName&  player = lobbySessNames.m_sessionNames[i];
		const uint32  loadedDLC = (uint32) player.m_userData[eLUD_LoadedDLCs];
		if (!MeetsDLCRequirements(requiredDLCs, loadedDLC))
		{
			CryLog("CDLCManager::GetNamesStringOfPlayersMissingDLCsForLevel: '%s' does not meet DLC requirements for level '%s'", player.m_name, pLevelName);
			count++;
			if (!pPlayersString->empty())
			{
				pPlayersString->append(", ");
			}
			pPlayersString->append(player.m_name);
		}
	}

	return count;
}

bool CDLCManager::OnWarningReturn(THUDWarningId id, const char* returnValue)
{
	return true;
}

void CDLCManager::OnlineAttributesRead()
{
	m_onlineAttributesRead = true;
}

bool CDLCManager::IsNewDLC(const int index) const
{
	return ! (m_appliedDLCStat & ( BIT(index) ) );
}

void CDLCManager::AddNewDLCApplied(const int index)
{
	CRY_ASSERT(IsNewDLC(index));
	m_appliedDLCStat |= ( BIT(index) );
}

void CDLCManager::ActivatePreSaleBonuses( bool showPopup, bool fromSuitReboot /*= false*/ )
{
	m_DLCXPToAward = 0;

	bool anyNew = false;

	if( m_onlineAttributesRead && m_allowedDLCUpToDate )
	{
		if( CPlayerProgression *pPlayerProgression = CPlayerProgression::GetInstance() )
		{
			for( int iDLC = 0; iDLC < MAX_DLC_COUNT; ++iDLC )
			{
				if( IsDLCLoaded( iDLC ) )
				{
					bool isNew = IsNewDLC( iDLC );

					anyNew |= isNew;
					
					if( m_dlcContents[ iDLC].bonusID != 0 )
					{
						int xpFromThis = pPlayerProgression->UnlockPresale( m_dlcContents[ iDLC ].bonusID, showPopup, isNew || fromSuitReboot );

						if( isNew || fromSuitReboot )
						{
							m_DLCXPToAward += xpFromThis;
						}
					}

					if( isNew )
					{
						AddNewDLCApplied( iDLC );

						if( ! m_dlcContents[ iDLC ].descriptionStr.empty()  )
						{
							//show new unlocked DLC description
							const char* pDescription = CHUDUtils::LocalizeString( m_dlcContents[ iDLC ].descriptionStr.c_str() );

							g_pGame->GetWarnings()->AddGameWarning( "RedeemNewDLC", pDescription, this );

						}
					}
				}
			}
		}
	}
	else
	{
		CryLog( "DLCWarning: Calling activate presale bonuses too soon" );
	}

}

bool CDLCManager::CheckLevels( int dlcId, const char* sDLCRootFolder )
{
	//TODO: Enable on all platforms

	return true;
}

void CDLCManager::RequestDLCWarning( const char* warningName, uint32 priority, bool alwaysDelay )
{
	if( gEnv->bMultiplayer && !alwaysDelay )
	{
		g_pGame->GetWarnings()->AddGameWarning( warningName, NULL );
	}
	else
	{
		//Check we don't already have a warning
		if( m_dlcWarning.empty() || priority < m_warningPriority )
		{
			m_dlcWarning.Format( warningName );
			m_warningPriority = priority;
		}
		else
		{
			CryLog( "DLC: reporting warning %s but already have warning %s waiting, ignoring", warningName, m_dlcWarning.c_str() );
		}

	}
}

void CDLCManager::ProcessDelayedWarnings()
{
	if( !m_dlcWarning.empty() )
	{
		g_pGame->GetWarnings()->AddGameWarning( m_dlcWarning.c_str(), NULL );

		m_dlcWarning.clear();
		m_warningPriority = DLC_WARN_LOWEST_PRIO;
	}
}

const char* CDLCManager::ScoreRewardsFilename( const char* pLevelName )
{
	int dlcID = DlcIdForLevel( pLevelName );

	if( dlcID != -1 )
	{
		return m_dlcContents[ dlcID ].scoreRewardsPath.c_str();
	}
	
	return NULL;
}

int CDLCManager::DlcIdForLevel( const char* pLevelName )
{
	int retVal = -1;
	// Strip off all directories from the level name (e.g. "Wars/")
	const char* pTrimmedLevelName = pLevelName;
	const char* pSlashPoint;
	while( (pSlashPoint = strrchr(pTrimmedLevelName, '/') ) != NULL )
	{
		pTrimmedLevelName = pSlashPoint+1;
	}

	for (int i=0; i<MAX_DLC_COUNT; ++i)
	{
		if (IsDLCReallyLoaded(i))	//probably ok to find out if a DLC we have installed is required, even if we don't own it
		{
			for (size_t j=0; j<m_dlcContents[i].levels.size(); ++j)
			{
				if (stricmp(m_dlcContents[i].levels[j].c_str(), pTrimmedLevelName) == 0)
				{
					CRY_ASSERT_MESSAGE( retVal == -1, "DLC level in multiple DLC packages" );
					retVal = i;
				}
			}
		}
	}

	return retVal;
}

void CDLCManager::DoDLCUnlocks( XmlNodeRef unlocksXml, int dlcId )
{
	//handle any direct unlocks
	const int unlockCount = unlocksXml->getChildCount();

	for (int iUnlock = 0; iUnlock < unlockCount; ++iUnlock)
	{
		XmlNodeRef unlockNode = unlocksXml->getChild(iUnlock);
		SUnlock unlock(unlockNode, 0);
		unlock.Unlocked(true);
		m_itemUnlocks.push_back( unlock );
		m_itemUnlockDLCids.push_back( dlcId );

		CryLog( "DLC: Found a dlc item unlock, %s", unlockNode->getAttr("name") );
	}
}

bool CDLCManager::HaveUnlocked( EUnlockType type, const char* name, SPPHaveUnlockedQuery & results )
{
	results.exists = false;
	results.unlocked = false;

	for(unsigned int i = 0; i < m_itemUnlocks.size(); i++)
	{
		if( IsDLCLoaded(m_itemUnlockDLCids[i]) )
		{
			const SUnlock& unlock = m_itemUnlocks[i];
			if(unlock.m_type == type )
			{
				if(strcmpi(name, unlock.m_name) == 0)
				{
					CryLog( "DLC: dlc has a node to unlock %s", name );
					results.available = true;
					results.exists = true;
					results.reason = eUR_None;
					if (unlock.m_unlocked)
					{
						results.unlocked = true;
						return true;
					}
				}
			}
		}
	}

	return false;
}

void CDLCManager::AddPlaylists()
{
	CPlaylistManager* pPlaylistManager = g_pGame->GetPlaylistManager();

	if( pPlaylistManager )
	{
		for( int dlcId = 0; dlcId < MAX_DLC_COUNT; dlcId++ )
		{
			if( IsDLCLoaded( dlcId) )
			{
				pPlaylistManager->AddPlaylistsFromPath( m_dlcContents[ dlcId ].playlistsPath );
			}
		}
	}
}

bool CDLCManager::IsPresaleEntitlementSet(int entitlementIndex) const
{
	CRY_ASSERT(0 <= entitlementIndex && entitlementIndex < CRY_ARRAY_COUNT(k_entitlementTags));
	return IsEntitlementSet(k_entitlementTags[entitlementIndex]);
}

bool CDLCManager::IsDLCEntitlementSet(int entitlementIndex) const
{
	CRY_ASSERT(0 <= entitlementIndex && entitlementIndex < CRY_ARRAY_COUNT(k_dlcEntitlementTags));
	return IsEntitlementSet(k_dlcEntitlementTags[entitlementIndex]);
}

bool CDLCManager::IsEntitlementSet (const char* pTag) const
{
	CRY_ASSERT(pTag != nullptr);

	for( int dlcId = 0; dlcId < MAX_DLC_COUNT; dlcId++ )
	{
		if( ! m_dlcContents[ dlcId ].uniqueTag.empty() )
		{
			if( strcmp( m_dlcContents[ dlcId ].uniqueTag, pTag ) == 0 )
			{
				//this is the entitlement we are looking for
#if ENTITLEMENTS_AUTHORATIVE
				return IsDLCLoaded( dlcId );
#else
				return (m_entitledDLCs & BIT(dlcId)) != 0;
#endif
			}
		}
	}

	CryLog( "CDLCManager: No record of entitlement %s", pTag );
	return false;
}

int CDLCManager::SetEntitlement( int entitlementIndex, bool presale )
{
	int dlcID = -1;

	// TODO: michiel

	return dlcID;
}

void CDLCManager::Update()
{
	// TODO: michiel
}

bool CDLCManager::IsPIIEntitlementSet() const
{
	return IsPresaleEntitlementSet(k_optin_ent_index);
}
