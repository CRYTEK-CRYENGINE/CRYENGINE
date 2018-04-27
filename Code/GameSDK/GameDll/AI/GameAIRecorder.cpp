// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Helper for Game-specific items dealing with AI Recorder
  
 -------------------------------------------------------------------------
  History:
  - 17:05:2010: Created by Kevin Kirst, with integrated changes from GameF07

*************************************************************************/

#include "StdAfx.h"
#include "GameAIRecorder.h"
#include "GameAISystem.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "Agent.h"

//////////////////////////////////////////////////////////////////////////
void RecordAIComment(EntityId entityId, const char* szComment, ...)
{
#ifdef INCLUDE_GAME_AI_RECORDER

	Agent agent(entityId);
	if (agent.IsValid())
	{
		va_list args;
		va_start(args, szComment);

		g_pGame->GetGameAISystem()->GetGameAIRecorder().RecordLuaComment(agent, szComment, args);
		
		va_end(args);
	}

#endif //INCLUDE_GAME_AI_RECORDER
}

#ifdef INCLUDE_GAME_AI_RECORDER

static char const* g_szRemoteTempArchive = "Recordings\\RemoteTempArchive.zip";

const char *CGameAIRecorderCVars::ai_remoteRecorder_serverDir	= "";
int CGameAIRecorderCVars::ai_remoteRecorder_enabled				= 0;
int CGameAIRecorderCVars::ai_remoteRecorder_onlyBookmarked		= 1;

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorderCVars::RegisterCommands()
{
	REGISTER_COMMAND("comment", CmdAIRecorderAddComment, 0, "Add a comment to the AI Recorder");
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorderCVars::RegisterVariables()
{
	REGISTER_CVAR(ai_remoteRecorder_serverDir, "", 0, "Server directory where to copy the remote archive");
	REGISTER_CVAR(ai_remoteRecorder_enabled, 0, 0, "Enable remote archive copying for AI recorder information");
	REGISTER_CVAR(ai_remoteRecorder_onlyBookmarked, 1, 0, "Only copy the archive over if it contains a bookmark");
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorderCVars::UnregisterCommands(IConsole *pConsole)
{
	if (pConsole)
	{
		pConsole->RemoveCommand("comment");
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorderCVars::UnregisterVariables(IConsole *pConsole)
{
	if (pConsole)
	{
		pConsole->UnregisterVariable("ai_remoteRecorder_serverDir");
		pConsole->UnregisterVariable("ai_remoteRecorder_enabled");
		pConsole->UnregisterVariable("ai_remoteRecorder_onlyBookmarked");
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorderCVars::CmdAIRecorderAddComment(IConsoleCmdArgs* cmdArgs)
{
	if (cmdArgs->GetArgCount() > 0)
	{
		g_pGame->GetGameAISystem()->GetGameAIRecorder().RequestAIRecordComment(cmdArgs->GetCommandLine());
	}
	else
	{
		CryLogAlways("AIRecorderAddComment: No comment specified! Ignoring");
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CGameAIRecorder::CGameAIRecorder()
: m_bIsRecording(false)
, m_bBookmarkAdded(false)
{

}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::Init()
{
	assert(gEnv);

	IAIRecorder *pRecorder = gEnv->pAISystem->GetIAIRecorder();
	if (pRecorder)
	{
		pRecorder->AddListener(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::Shutdown()
{
	assert(gEnv);

	IAIRecorder *pRecorder = gEnv->pAISystem->GetIAIRecorder();
	if (pRecorder)
	{
		pRecorder->RemoveListener(this);
	}

	CleanupRemoteArchive();
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::RequestAIRecordBookmark()
{
	EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();

	if (gEnv->bServer)
	{
		AddRecordBookmark(clientId);
	}
#ifdef INCLUDE_GAME_AI_RECORDER_NET
	else if (CGameRules *pGameRules = g_pGame->GetGameRules())
	{
		pGameRules->GetGameObject()->InvokeRMI(CGameRules::SvRequestRecorderBookmark(), CGameRules::NoParams(), eRMI_ToServer, clientId);
	}
#endif //INCLUDE_GAME_AI_RECORDER_NET
	else
	{
		GameWarning("RequestAIRecordBookmark Cannot add bookmark, because we're a client and there are no Gamerules to facilitate the remote command");
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::RequestAIRecordComment(const char* szComment)
{
	EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();

	if (gEnv->bServer)
	{
		AddRecordComment(clientId, szComment);
	}
#ifdef INCLUDE_GAME_AI_RECORDER_NET
	else if (CGameRules *pGameRules = g_pGame->GetGameRules())
	{
		pGameRules->GetGameObject()->InvokeRMI(CGameRules::SvRequestRecorderComment(), CGameRules::StringParams(szComment), eRMI_ToServer, clientId);
	}
#endif //INCLUDE_GAME_AI_RECORDER_NET
	else
	{
		GameWarning("RequestAIRecordComment Cannot add comment, because we're a client and there are no Gamerules to facilitate the remote command");
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::AddRecordBookmark(EntityId requesterId)
{
	assert(requesterId > 0);
	assert(gEnv->bServer);

	if (m_bIsRecording)
	{
		if (!gEnv->bServer)
		{
			CryLogAlways("[AI] Recorder bookmark requested on Client. Only the Server can do this!");
			return;
		}

		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(requesterId);
		IAIObject *pAI = pEntity ? pEntity->GetAI() : NULL;
		if (!pAI)
		{
			CryLogAlways("[AI] Attempting to add recorder bookmark, but the requester does not have an AI!");
			return;
		}

		static int g_iBookmarkCounter = 0;
		int iBookmark = ++g_iBookmarkCounter;

		// (Kevin) We need to unify the timestamp for this, which should be set when the Recorder itself starts.
		//	This way, all screenshots will match up with the recorder per date/time/build. Then we need to
		//	move these into subfolders that contain the date/time/build as the name. (10/08/2009)

		time_t ltime;
		time(&ltime);
		tm *pTm = localtime(&ltime);
		char szDate[128];
		strftime(szDate, 128, "Date(%d %b %Y) Time(%H %M %S)", pTm);

		// Get current version line
		const SFileVersion& fileVersion = gEnv->pSystem->GetFileVersion();

		const bool bTakeScreenshot = (requesterId == g_pGame->GetIGameFramework()->GetClientActorId());

		// Output to log
		CryLogAlways("[AI] --- RECORDER BOOKMARK ADDED ---");
		CryLogAlways("[AI] Id: %d", iBookmark);
		CryLogAlways("[AI] %s", szDate);
		CryLogAlways("[AI] By: %s", pEntity->GetName());

		if (!bTakeScreenshot)
		{
			CryLogAlways("[AI] No Screenshot was made for this bookmark, because requester is not the server!");
		}
		else
		{
			string sScreenShotFile;
			sScreenShotFile.Format("Recorder_Bookmark(%d) Build(%d) %s", iBookmark, fileVersion[0], szDate);

			const string sScreenShotPath = PathUtil::Make("Recordings", sScreenShotFile.c_str(), "jpg");

			// Take screenshot
			CryLogAlways("[AI] Screenshot: \'%s\'", sScreenShotPath.c_str());
			gEnv->pRenderer->ScreenShot(sScreenShotPath.c_str());

			OnAddBookmark(sScreenShotPath);
		}

		// Add bookmark to stream
		IAIRecordable::RecorderEventData data((float)iBookmark);
		pAI->RecordEvent(IAIRecordable::E_BOOKMARK, &data);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::AddRecordComment(EntityId requesterId, const char* szComment)
{
	assert(szComment && szComment[0]);
	assert(requesterId > 0);
	assert(gEnv->bServer);

	if (m_bIsRecording && szComment && szComment[0])
	{
		if (!gEnv->bServer)
		{
			CryLogAlways("[AI] Recorder comment requested on Client. Only the Server can do this!");
			return;
		}

		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(requesterId);
		IAIObject *pAI = pEntity ? pEntity->GetAI() : NULL;
		if (!pAI)
		{
			CryLogAlways("[AI] Attempting to add recorder comment, but the requester does not have an AI!");
			return;
		}

		// Output to log
		CryLogAlways("[AI] --- RECORDER COMMENT ADDED ---");
		CryLogAlways("[AI] By: %s", pEntity->GetName());
		CryLogAlways("[AI] Comment: %s", szComment);

		// Add comment to stream
		RecordLuaComment(pAI, "Comment: %s", szComment);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::RecordLuaComment(const Agent &agent, const char* szComment, ...) const
{
	assert(agent.IsValid());

	if (m_bIsRecording && agent.IsValid())
	{
		char szBuffer[1024];

		va_list args;
		va_start(args, szComment);
		cry_vsprintf(szBuffer, szComment, args);
		va_end(args);

		IAIRecordable::RecorderEventData data(szBuffer);
		agent.GetAIObject()->RecordEvent(IAIRecordable::E_LUACOMMENT, &data);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::OnRecordingStart(EAIRecorderMode mode, const char *filename)
{
	m_bIsRecording = true;

	m_bBookmarkAdded = false;
	CleanupRemoteArchive();

	// Create a new temp archive
	if (CGameAIRecorderCVars::ai_remoteRecorder_enabled != 0)
	{
		ICryPak *pCryPak = gEnv->pSystem->GetIPak();
		if (pCryPak)
		{
			m_pRemoteArchive = pCryPak->OpenArchive(g_szRemoteTempArchive);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::OnRecordingStop(const char *filename)
{
	m_bIsRecording = false;
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::OnAddBookmark(const string& sScreenShotPath)
{
	m_bBookmarkAdded = true;

	if (m_pRemoteArchive)
	{
		// Add the screenshot file to the archive
		AddFileToRemoteArchive(sScreenShotPath.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::OnRecordingSaved(const char *filename)
{
	if (m_pRemoteArchive && !gEnv->IsEditor() && gEnv->bServer)
	{
		// Only send when bookmark is added?
		if (m_bBookmarkAdded || CGameAIRecorderCVars::ai_remoteRecorder_onlyBookmarked == 0)
		{
			FinalizeRemoteArchive(filename);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CGameAIRecorder::AddFileToRemoteArchive(const char* szFile)
{
	bool bResult = false;

	ICryPak *pCryPak = gEnv->pSystem->GetIPak();
	assert(pCryPak);

	if (m_pRemoteArchive && pCryPak)
	{
		string sLocalPath = PathUtil::Make("..", szFile);

		FILE *pFile = pCryPak->FOpen(sLocalPath.c_str(), "rb");
		if (pFile)
		{
			// Add the file to the PAK
			size_t iFileSize = pCryPak->FGetSize(pFile);
			BYTE* pBuffer = (BYTE*)pCryPak->PoolMalloc(iFileSize);
			if (!pBuffer)
			{
				CryLogAlways("[Warning] Failed when packing file to remote archive: Out of memory. (\'%s\')", szFile);
			}
			else
			{
				pCryPak->FReadRaw(pBuffer, iFileSize, 1, pFile);
				int iResult = m_pRemoteArchive->UpdateFile(PathUtil::GetFile(szFile), pBuffer, iFileSize, ICryArchive::METHOD_DEFLATE, ICryArchive::LEVEL_BETTER);
				if (0 != iResult)
				{
				CryLogAlways("[Warning] Failed when packing file to remote archive: File update failed. (\'%s\')", szFile);
				}
				else
				{
					bResult = true;
				}

				pCryPak->PoolFree(pBuffer);
			}

			pCryPak->FClose(pFile);
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CGameAIRecorder::FinalizeRemoteArchive(const char* szRecordingFile)
{
	ICryPak *pCryPak = gEnv->pSystem->GetIPak();
	assert(pCryPak);

	bool bPAKCreated = false;
	bool bPAKSent = false;

	CryLogAlways("Attempting to pack AI Recording \'%s\'...", szRecordingFile);

	if (!m_pRemoteArchive)
	{
		CryLogAlways("[Warning] Remote recording temp archive does not exist! Aborting");
		return false;
	}
	else
	{
		bPAKCreated = AddFileToRemoteArchive(szRecordingFile);

		// Also attempt to add game log
		AddFileToRemoteArchive(gEnv->pSystem->GetILog()->GetFileName());

		// Close the archive so it can be sent
		m_pRemoteArchive = NULL;
	}

	if (bPAKCreated)
	{
		// Commit changes
		_flushall();

		// Send the archive
		bPAKSent = SendRemoteArchive(szRecordingFile);
		if (!bPAKSent)
		{
			CryLogAlways("[Warning] AI Recording pack failed to copy.");
		}
		else
		{
			CryLogAlways("AI Recording pack successfully copied!");
		}

		CleanupRemoteArchive();
	}

	return (bPAKCreated && bPAKSent);
}

//////////////////////////////////////////////////////////////////////////
bool CGameAIRecorder::SendRemoteArchive(const char* szRecordingFile)
{
	ICryPak *pCryPak = gEnv->pSystem->GetIPak();
	assert(pCryPak);

	bool bResult = false;

	string sPAKFileName;
	sPAKFileName.Format("%s_%s", gEnv->pSystem->GetUserName(), PathUtil::GetFileName(szRecordingFile).c_str());
	string sDestFile = PathUtil::Make(CGameAIRecorderCVars::ai_remoteRecorder_serverDir, sPAKFileName.c_str(), "zip");

	CryLogAlways("AI Recording packed successfully! Copying to remote directory \'%s\'...", sDestFile.c_str());

	// Remote copy the file
	string sPAKFileLocalPath = PathUtil::Make("..", g_szRemoteTempArchive);
	FILE *pPAKFile = pCryPak->FOpen(sPAKFileLocalPath.c_str(), "rb");
	if (pPAKFile)
	{
		FILE *pDestFile = pCryPak->FOpen(sDestFile.c_str(), "wb");
		if (pDestFile)
		{
			BYTE pBuffer[512];
			while (true)
			{
				const int iReadAmount = pCryPak->FReadRaw(pBuffer, 1, 512, pPAKFile);
				if (iReadAmount <= 0)
					break;

				if (pCryPak->FWrite(pBuffer, iReadAmount, 1, pDestFile) > 0)
					bResult = true;
			}

			pCryPak->FClose(pDestFile);
			pCryPak->FClose(pPAKFile);
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CGameAIRecorder::CleanupRemoteArchive()
{
	ICryPak *pCryPak = gEnv->pSystem->GetIPak();
	assert(pCryPak);

	m_pRemoteArchive = NULL;
	
	string sPAKFileLocalPath = PathUtil::Make("..", g_szRemoteTempArchive);
	pCryPak->RemoveFile(sPAKFileLocalPath.c_str());
}

#endif //INCLUDE_GAME_AI_RECORDER
