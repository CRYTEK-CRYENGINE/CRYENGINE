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

#ifndef __GAMEAIRECORDER_H__
#define __GAMEAIRECORDER_H__

class Agent;


// INCLUDE_GAME_AI_RECORDER - Define to include the game-specific utilities for AI recorder
// INCLUDE_GAME_AI_RECORDER_NET - Define to include the network portion of the Game AI Recorder

#if !defined(_RELEASE) && !defined(DEDICATED_SERVER)
	#define INCLUDE_GAME_AI_RECORDER 1
	// #define INCLUDE_GAME_AI_RECORDER_NET 1
#endif // _RELEASE


// Helper to record a comment inside the AI's recorder
// In:
//	entityId - EntityId of AI whose recorder receives the comment
//	szComment - Comment to add, with additional arguments
void RecordAIComment(EntityId entityId, const char* szComment, ...) PRINTF_PARAMS(2,3);


#ifdef INCLUDE_GAME_AI_RECORDER

#include <CryAISystem/IAIRecorder.h>

class CGameAIRecorder : public IAIRecorderListener
{
	// For network RMI usage
	friend class CGameRules;

public:
	CGameAIRecorder();

	void Init();
	void Shutdown();
	bool IsRecording() const { return m_bIsRecording; }

	// Use these methods to add bookmarks/comments to AI recorder
	void RequestAIRecordBookmark();
	void RequestAIRecordComment(const char* szComment);

	void RecordLuaComment(const Agent &agent, const char* szComment, ...) const PRINTF_PARAMS(3,4);

	// IAIRecorderListener
	virtual void OnRecordingStart(EAIRecorderMode mode, const char *filename);
	virtual void OnRecordingStop(const char *filename);
	virtual void OnRecordingSaved(const char *filename);
	//~IAIRecorderListener

private:
	void AddRecordBookmark(EntityId requesterId);
	void AddRecordComment(EntityId requesterId, const char* szComment);

	// Remote recording copy
	void OnAddBookmark(const string& sScreenShotPath);
	void CleanupRemoteArchive();
	bool FinalizeRemoteArchive(const char* szRecordingFile);
	bool SendRemoteArchive(const char* szRecordingFile);
	bool AddFileToRemoteArchive(const char* szFile);

private:
	bool m_bIsRecording;
	bool m_bBookmarkAdded;
	ICryArchive_AutoPtr m_pRemoteArchive;
};

class CGameAIRecorderCVars
{
	friend class CGameAIRecorder;

public:
	static void RegisterCommands();
	static void RegisterVariables();
	static void UnregisterCommands(IConsole* pConsole);
	static void UnregisterVariables(IConsole* pConsole);

	// Console command
	static void CmdAIRecorderAddComment(IConsoleCmdArgs* cmdArgs);

public:
	static const char *ai_remoteRecorder_serverDir;
	static int ai_remoteRecorder_enabled;
	static int ai_remoteRecorder_onlyBookmarked;
};

#endif //INCLUDE_GAME_AI_RECORDER


#endif //__GAMEAIRECORDER_H__
