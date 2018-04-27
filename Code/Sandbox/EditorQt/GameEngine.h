// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios
// -------------------------------------------------------------------------
//  Created:     13/5/2002 by Timur.
//  Description: The game engine for editor
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "LogFile.h"
#include "IEditorImpl.h"
#include "SandboxAPI.h"
#include <CryCore/Containers/CryListenerSet.h>

struct IEditorGame;
struct IFlowSystem;
struct IGameTokenSystem;
struct IEquipmentSystemInterface;
struct IInitializeUIInfo;

class CNavigation;

class IGameEngineListener
{
public:
	virtual void OnPreEditorUpdate() = 0;
	virtual void OnPostEditorUpdate() = 0;
};

//! Callback used by editor when initializing for info in UI dialogs
struct IInitializeUIInfo
{
	virtual void SetInfoText(const char* text) = 0;
};

//! This class serves as a high-level wrapper for CryEngine game.
class SANDBOX_API CGameEngine : public IEditorNotifyListener
{
public:
	CGameEngine();
	~CGameEngine(void);
	//! Initialize System.
	//! @return true if initialization succeeded, false otherwise
	bool Init(
	  bool bPreviewMode,
	  bool bTestMode,
	  bool bShaderCacheGen,
	  const char* sCmdLine,
	  IInitializeUIInfo* logo);

	//! Configures editor related engine components
	void InitAdditionalEngineComponents();

	//! Load new terrain level into 3d engine.
	//! Also load AI triangulation for this level.
	bool LoadLevel(
	  const string& levelPath,
	  const string& mission,
	  bool bDeleteAIGraph,
	  bool bReleaseResources);
	//!* Reload level if it was already loaded.
	bool ReloadLevel();
	//! Loads AI triangulation.
	bool LoadAI(const string& levelName, const string& missionName);
	//! Finish the editor-specific process of AI data loading
	bool FinishLoadAI();
	//! Load new mission.
	bool LoadMission(const string& mission);
	//! Reload environment settings in currently loaded level.
	bool ReloadEnvironment();
	//! Request to switch In/Out of game mode on next update.
	//! The switch will happen when no sub systems are currently being updated.
	//! @param inGame When true editor switch to game mode.
	void RequestSetGameMode(bool inGame);
	//! Switch In/Out of AI and Physics simulation mode.
	//! @param enabled When true editor switch to simulation mode.
	void           SetSimulationMode(bool enabled, bool bOnlyPhysics = false);
	//! Get current simulation mode.
	bool           GetSimulationMode() const { return m_bSimulationMode; };
	//! Returns true if level is loaded.
	bool           IsLevelLoaded() const     { return m_bLevelLoaded; };
	//! Assign new level path name.
	void           SetLevelPath(const string& path);
	//! Assign new current mission name.
	void           SetMissionName(const string& mission);
	//! Return name of currently loaded level.
	const string& GetLevelName() const               { return m_levelName; };
	//! Return name of currently active mission.
	const string& GetMissionName() const             { return m_missionName; };
	//! Get fully specified level path.
	const string& GetLevelPath() const               { return m_levelPath; };
	//! Query if engine is in game mode.
	bool           IsInGameMode() const               { return m_bInGameMode; };
	//! Force level loaded variable to true.
	void           SetLevelLoaded(bool bLoaded)       { m_bLevelLoaded = bLoaded; }
	//! Force level just created variable to true.
	void           SetLevelCreated(bool bJustCreated) { m_bJustCreated = bJustCreated; }
	//! Generate All AI data
	void           GenerateAiAll(uint32 flags);
	//! Generate AI MNM navigation of currently loaded data.
	void           GenerateAINavigationMesh();
	//! Generate AI Cover Surfaces of currently loaded data.
	void           GenerateAICoverSurfaces();
	//! Loading all the AI navigation data of current level.
	void           LoadAINavigationData();
	//! Does some basic sanity checking of the AI navigation
	void           ValidateAINavigation();
	//! Clears all AI navigation data from the current level.
	void           ClearAllAINavigation();
	//! Generates 3D volume voxels only for debugging
	void           ExportAiData(
	  const char* navFileName,
	  const char* areasFileName,
	  const char* roadsFileName,
	  const char* fileNameVerts,
	  const char* fileNameVolume,
	  const char* fileNameFlight);
	CNavigation* GetNavigation() { return m_pNavigation; }
	//! Sets equipment pack current used by player.
	void         SetPlayerEquipPack(const char* sEqipPackName);
	//! Called when Game resource file must be reloaded.
	void         ReloadResourceFile(const string& filename);
	//! Query ISystem interface.
	ISystem*     GetSystem() { return m_pISystem; };
	//! Set player position in game.
	//! @param bEyePos If set then given position is position of player eyes.
	void                       SetPlayerViewMatrix(const Matrix34& tm, bool bEyePos = true);
	//! When set, player in game will be every frame synchronized with editor camera.
	void                       SyncPlayerPosition(bool bEnable);
	bool                       IsSyncPlayerPosition() const { return m_bSyncPlayerPosition; };
	void                       HideLocalPlayer(bool bHide);
	//! Set game's current Mod name.
	void                       SetCurrentMOD(const char* sMod);
	//! Returns game's current Mod name.
	const char*                GetCurrentMOD() const;
	//! Called every frame.
	void                       Update();
	virtual void               OnEditorNotifyEvent(EEditorNotifyEvent event);
	struct IEntity*            GetPlayerEntity();
	// Retrieve pointer to the game flow system implementation.
	IFlowSystem*               GetIFlowSystem() const;
	IGameTokenSystem*          GetIGameTokenSystem() const;
	IEquipmentSystemInterface* GetIEquipmentSystemInterface() const;
	IEditorGame*               GetIEditorGame() const { return m_pEditorGame; }
	//! When enabled flow system will be updated at editing mode.
	void                       EnableFlowSystemUpdate(bool bEnable)
	{
		m_bUpdateFlowSystem = bEnable;

		const EEditorNotifyEvent event = bEnable ? eNotify_OnEnableFlowSystemUpdate : eNotify_OnDisableFlowSystemUpdate;
		GetIEditorImpl()->Notify(event);
	}

	bool IsFlowSystemUpdateEnabled() const { return m_bUpdateFlowSystem; }
	void LockResources();
	void UnlockResources();
	void ResetResources();
	bool BuildEntitySerializationList(XmlNodeRef output);
	void OnTerrainModified(const Vec2& modPosition, float modAreaRadius, bool bFullTerrain, bool bGeometryModified);

	//! mutex used by other threads to lock up the PAK modification,
	//! so only one thread can modify the PAK at once
	static CryMutex& GetPakModifyMutex()
	{
		//! mutex used to halt copy process while the export to game
		//! or other pak operation is done in the main thread
		static CryMutex s_pakModifyMutex;
		return s_pakModifyMutex;
	}
	void SetGameMode(bool inGame);

	void AddListener(IGameEngineListener* pListener);
	void RemoveListener(IGameEngineListener* pListener);

	// toggles between game input and editor action handling
	void ToggleGameInputSuspended();
	bool IsGameInputSuspended();

private:
	void SwitchToInGame();
	void SwitchToInEditor();

	void InitializeEditorGame();
	bool StartGameContext();
	void NotifyGameModeChange(bool bGameMode);

	CLogFile                      m_logFile;
	string                        m_levelName;
	string                        m_missionName;
	string                        m_levelPath;
	string                        m_MOD;
	bool                          m_bLevelLoaded;
	bool                          m_bInGameMode;
	bool                          m_bGameModeSuspended;
	bool                          m_bSimulationMode;
	bool                          m_bSimulationModeAI;
	bool                          m_bSyncPlayerPosition;
	bool                          m_bUpdateFlowSystem;
	bool                          m_bJustCreated;
	bool                          m_bIgnoreUpdates;
	ISystem*                      m_pISystem;
	IAISystem*                    m_pIAISystem;
	IEntitySystem*                m_pIEntitySystem;
	CNavigation*                  m_pNavigation;
	Matrix34                      m_playerViewTM;
	struct SSystemUserCallback*   m_pSystemUserCallback;
	IEditorGame*                  m_pEditorGame;
	class CGameToEditorInterface* m_pGameToEditorInterface;
	enum EPendingGameMode
	{
		ePGM_NotPending,
		ePGM_SwitchToInGame,
		ePGM_SwitchToInEditor,
	};
	EPendingGameMode                   m_ePendingGameMode;
	CListenerSet<IGameEngineListener*> m_listeners;

	//! Keeps the editor active even if no focus is set
	int keepEditorActive;
};

