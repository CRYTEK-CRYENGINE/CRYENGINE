// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#define CRY_SAVEGAME_FILENAME "CRYENGINE"
#define CRY_SAVEGAME_FILE_EXT ".CSF"

#include <CrySystem/ICmdLine.h>
#include <CryNetwork/INetwork.h>

struct IAIActorProxy;
struct IGameFramework;
struct IGameStateRecorder;
struct IGameAudio;
struct IGameWarningsListener;
struct SGameStartParams;
struct SRenderingPassInfo;
struct IGameToEditorInterface;
struct IGameWebDebugService;
struct IGameplayListener;

// Declare legacy GameDLL as deprecated except for the legacy modules that expose functionality for it
#if !defined(eCryModule) || (eCryModule != eCryM_GameFramework && eCryModule != eCryM_LegacyGameDLL     \
	&& eCryModule != eCryM_Editor && eCryModule != eCryM_FlowGraph && eCryModule != eCryM_AudioSystem   \
	&& eCryModule != eCryM_3DEngine && eCryModule != eCryM_AISystem && eCryModule != eCryM_EntitySystem \
	&& eCryModule != eCryM_Movie && eCryModule != eCryM_System && eCryModule != eCryM_Legacy)

	#define CRY_DEPRECATED_GAME_DLL CRY_DEPRECATED("(v5.3) IGame, IEditorGame and IGameStartup have been replaced by ICryPlugin and will be removed in a future update.")

#else
	#define CRY_DEPRECATED_GAME_DLL
#endif

//! Legacy functionality for the Main interface of a game, replaced with ICryPlugin (see templates for example implementation)
//! The IGame interface should be implemented in the GameDLL.
//! Game systems residing in the GameDLL can be initialized and updated inside the Game object.
//! \see IEditorGame.
struct IGame
{
	CRY_DEPRECATED_GAME_DLL IGame() = default;

	//! Interface used to communicate what entities/entity archetypes need to be precached.
	//! Game code can further do some data mining to figure out the resources needed for the entities
	struct IResourcesPreCache
	{
		virtual void QueueEntityClass(const char* szEntityClass) = 0;
		virtual void QueueEntityArchetype(const char* szEntityArchetype) = 0;
	};

	struct ExportFilesInfo
	{
		ExportFilesInfo(const char* _baseFileName, const uint32 _fileCount)
			: m_pBaseFileName(_baseFileName)
			, m_fileCount(_fileCount)
		{
		}

		ILINE uint32      GetFileCount() const    { return m_fileCount; }
		ILINE const char* GetBaseFileName() const { return m_pBaseFileName; }

		static void       GetNameForFile(const char* baseFileName, const uint32 fileIdx, char* outputName, size_t outputNameSize)
		{
			assert(baseFileName != NULL);
			cry_sprintf(outputName, outputNameSize, "%s_%u", baseFileName, fileIdx);
		}

	private:
		const char*  m_pBaseFileName;
		const uint32 m_fileCount;
	};

	// <interfuscator:shuffle>
	virtual ~IGame(){}

	//! Initialize the MOD.
	//! The shutdown method, must be called independent of this method's return value.
	//! \param pCmdLine Pointer to the command line interface.
	//! \param pFramework Pointer to the IGameFramework interface.
	//! \return 0 if something went wrong with initialization, non-zero otherwise.
	virtual bool Init(/*IGameFramework* pFramework*/) = 0;

	//! Init editor related things.
	virtual void InitEditor(IGameToEditorInterface* pGameToEditor) = 0;

	virtual void GetMemoryStatistics(ICrySizer* s) = 0;

	//! Finish initializing the MOD.
	//! Called after the game framework has finished its CompleteInit.
	//! This is the point at which to register game flow nodes etc.
	virtual bool CompleteInit() { return true; };

	//! Shuts down the MOD and delete itself.
	virtual void Shutdown() = 0;

	//! Notify game of pre-physics update.
	virtual void PrePhysicsUpdate() {}

	//! Updates the MOD.
	//! \param haveFocus true if the game has the input focus.
	//! \return 0 to terminate the game (i.e. when quitting), non-zero to continue.
	virtual int Update(bool haveFocus, unsigned int updateFlags) = 0;

	//! Called on the game when entering/exiting game mode in editor
	//! \param bStart true if we enter game mode, false if we exit it.
	virtual void EditorResetGame(bool bStart) = 0;

	//! \return Name of the mode. (e.g. "Capture The Flag").
	virtual const char* GetLongName() = 0;

	//! \return A short description of the mode. (e.g. "dc")
	virtual const char* GetName() = 0;

	//! Loads a specified action map, used mainly for loading the default action map
	virtual void LoadActionMaps(const char* filename) = 0;

	//! \return Pointer to the game framework being used.
	virtual IGameFramework* GetIGameFramework() = 0;

	//! Mapping level filename to "official" name.
	//! \return c_str or NULL.
	virtual const char* GetMappedLevelName(const char* levelName) const = 0;

	//! Add a game warning that is shown to the player
	//! \return A unique handle to the warning or 0 for any error.
	virtual uint32 AddGameWarning(const char* stringId, const char* paramMessage, IGameWarningsListener* pListener = NULL) = 0;

	//! Called from 3DEngine in RenderScene, so polygons and meshes can be added to the scene from game
	virtual void OnRenderScene(const SRenderingPassInfo& passInfo) = 0;

	//! Render Game Warnings.
	virtual void RenderGameWarnings() = 0;

	//! Remove a game warning.
	virtual void RemoveGameWarning(const char* stringId) = 0;

	//! Callback to game for game specific actions on level end.
	//! \retval false, if the level end should continue.
	//! \retval true, if the game handles the end level action and calls ScheduleEndLevel directly.
	virtual bool GameEndLevel(const char* stringId) = 0;

	//! Creates a GameStateRecorder instance in GameDll and returns the non-owning pointer to the caller (CryAction/GamePlayRecorder).
	virtual IGameStateRecorder* CreateGameStateRecorder(IGameplayListener* pL) = 0;

	virtual void                FullSerialize(TSerialize ser) = 0;
	virtual void                PostSerialize() = 0;

	//! Editor export interface hook, to allow the game to export its own data into the level paks.
	//! \return Exported file information.
	virtual IGame::ExportFilesInfo ExportLevelData(const char* levelName, const char* missionName) const = 0;

	//! Interface hook to load all game exported data when the level is loaded.
	virtual void LoadExportedLevelData(const char* levelName, const char* missionName) = 0;

	//! Interface hook to sync game exported data from level paks when the level is loaded in editor
	virtual void LoadExportedLevelDataInEditor(const char* szLevelName, const char* szMissionName) {}

	//! Access to game interface.
	virtual void* GetGameInterface() = 0;

	//! Access game specific resource precache interface
	virtual IResourcesPreCache* GetResourceCache() { return nullptr; }

	//! Retrieves IGameWebDebugService for web-socket based remote debugging.
	virtual IGameWebDebugService* GetIWebDebugService() { return nullptr; };
	// </interfuscator:shuffle>
};

//! \endcond