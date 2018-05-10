// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameLobbyData.h"

#include "Utility/CryHash.h"

#include "GameRulesModules/IGameRulesModulesManager.h"
#include "GameRulesModules/GameRulesModulesManager.h"
#include <ILevelSystem.h>
#include <CryString/NameCRCHelper.h>
#include "GameCVars.h"
#include "PlaylistManager.h"
#include "DataPatchDownloader.h"

namespace GameLobbyData
{
	char const * const g_sUnknown = "Unknown";

	const uint32 ConvertGameRulesToHash(const char* gameRules)
	{
		if (gameRules)
		{
			return CryStringUtils::HashStringLower(gameRules);
		}
		else
		{
			return 0;
		}
	}

	const char* GetGameRulesFromHash(uint32 hash, const char* unknownStr/*="Unknown"*/)
	{
		IGameRulesModulesManager *pGameRulesModulesManager = CGameRulesModulesManager::GetInstance();
		const int rulesCount = pGameRulesModulesManager->GetRulesCount();
		for(int i = 0; i < rulesCount; i++)
		{
			const char* name = pGameRulesModulesManager->GetRules(i);
			if(ConvertGameRulesToHash(name) == hash)
			{
				return name;
			}
		}

		return unknownStr;
	}

	const uint32 ConvertMapToHash(const char* mapName)
	{
		if (mapName)
		{
			return CryStringUtils::HashStringLower(mapName);
		}
		else
		{
			return 0;
		}
	}

	const char* GetMapFromHash(uint32 hash, const char *pUnknownStr)
	{
		ILevelSystem* pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
		const int levelCount = pLevelSystem->GetLevelCount();
		for(int i = 0; i < levelCount; i++)
		{
			const char* name = pLevelSystem->GetLevelInfo(i)->GetName();
			if(ConvertMapToHash(name) == hash)
			{
				return name;
			}
		}
		for(int i = 0; i < levelCount; i++)
		{
			const char* name = pLevelSystem->GetLevelInfo(i)->GetName();
			const char* pTrimmedLevelName = strrchr(name, '/');
			if(pTrimmedLevelName && ConvertMapToHash(pTrimmedLevelName+1) == hash)
			{
				return name;
			}
		}
		return pUnknownStr;
	}

	const uint32 GetVersion()
	{
#if defined(DEDICATED_SERVER)
		CDownloadMgr* pDownloadMgr = g_pGame->GetDownloadMgr();
		if (pDownloadMgr && pDownloadMgr->IsWaitingToShutdown())
		{
			// Return a bogus version number so that nobody is able to connect to the server
			return 0xDEADBEEF;
		}
#endif
		// matchmaking version defaults to build id, i've chose the bit shifts here to ensure we don't unnecessary truncate the version and matchmake against the wrong builds
		const uint32 version = (g_pGameCVars->g_MatchmakingVersion & 8191) + (g_pGameCVars->g_MatchmakingBlock * 8192);
		return version^GetGameDataPatchCRC();
	}

	const bool IsCompatibleVersion(uint32 version)
	{
		return version == GetVersion();
	}

	const uint32 GetPlaylistId()
	{
		CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
		if (pPlaylistManager)
		{
			const SPlaylist* pPlaylist = pPlaylistManager->GetCurrentPlaylist();
			if(pPlaylist)
			{
				return pPlaylist->id;
			}
		}

		return 0;
	}

	const uint32 GetVariantId()
	{
		CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
		if (pPlaylistManager)
		{
			return pPlaylistManager->GetActiveVariantIndex();
		}

		return 0;
	}

	const uint32 GetGameDataPatchCRC()
	{
		uint32		result=0;

		if (CDataPatchDownloader *pDP=g_pGame->GetDataPatchDownloader())
		{
			result=pDP->IsPatchingEnabled()?pDP->GetDataPatchHash():0;
		}

		return result;
	}

	const int32 GetSearchResultsData(SCrySessionSearchResult* session, CryLobbyUserDataID id)
	{

		for (uint32 i = 0; i < session->m_data.m_numData; i++)
		{
			if (session->m_data.m_data[i].m_id == id)
			{
				return session->m_data.m_data[i].m_int32;
			}
		}

		CRY_ASSERT(0);

		return 0;
	}

	const bool IsValidServer(SCrySessionSearchResult* session)
	{
		bool isValidServer = false;

		if(GameLobbyData::IsCompatibleVersion(GameLobbyData::GetSearchResultsData(session, LID_MATCHDATA_VERSION)))
		{
			if(GameLobbyData::GetSearchResultsData(session, LID_MATCHDATA_GAMEMODE) != 0 && GameLobbyData::GetSearchResultsData(session, LID_MATCHDATA_MAP) != 0)
			{
				isValidServer = true;
			}
		}

		return isValidServer;
	}
}
