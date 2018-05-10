// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GAME_SERIALIZE_H__
#define __GAME_SERIALIZE_H__

#pragma once

#include "ISaveGame.h"
#include "ILoadGame.h"
#include "ILevelSystem.h"
#include "GameSerializeHelpers.h"

class CCryAction;
class CSaveGameHolder;
class CLoadGameHolder;
struct Checkpoint;
struct IGameFramework;
struct SGameStartParams;
struct ISaveGame;
struct SBasicEntityData;
struct STempAutoResourcesLock;

typedef DynArray<SBasicEntityData, int, NArray::SmallDynStorage<NAlloc::GeneralHeapAlloc>> TBasicEntityDatas;
typedef std::vector<IEntity*>                                                              TEntitiesToSerialize;
typedef ISaveGame* (*                                                                      SaveGameFactory)();
typedef ILoadGame* (*                                                                      LoadGameFactory)();

//struct to save the state during a save game process
struct SSaveEnvironment
{
	CCryAction*     m_pCryAction;
	const char*     m_checkPointName;
	Checkpoint&     m_checkpoint;
	CSaveGameHolder m_pSaveGame;
	const char*     m_saveMethod;

	SSaveEnvironment(CCryAction* pCryAction, const char* checkPointName, const char* method, Checkpoint& checkpoint, const std::map<string, SaveGameFactory>& saveGameFactories) :
		m_pCryAction(pCryAction),
		m_checkPointName(checkPointName),
		m_checkpoint(checkpoint),
		m_pSaveGame(stl::find_in_map(saveGameFactories, CONST_TEMP_STRING(method), CSaveGameHolder::ReturnNull)()),
		m_saveMethod(method)
	{
	}

	bool InitSave(const char* name, ESaveGameReason reason);
};

//struct to save the state during a load game process
struct SLoadEnvironment
{
	CCryAction*                 m_pCryAction;
	CLoadGameHolder             m_pLoadGame;
	std::unique_ptr<TSerialize> m_pSer;
	Checkpoint&                 m_checkpoint;
	TBasicEntityDatas           m_basicEntityData;

	bool                        m_bLoadingErrors;
	bool                        m_bHaveReserved;
	ELoadGameResult             m_failure;

	SLoadEnvironment(CCryAction* pCryAction, Checkpoint& checkpoint, const char* method, const std::map<string, LoadGameFactory>& loadGameFactories) :
		m_pCryAction(pCryAction),
		m_pLoadGame(stl::find_in_map(loadGameFactories, method, CLoadGameHolder::ReturnNull)()),
		m_pSer(nullptr),
		m_checkpoint(checkpoint),
		m_basicEntityData(NAlloc::GeneralHeapAlloc(m_pLoadGame->GetHeap())),
		m_bLoadingErrors(false),
		m_bHaveReserved(false),
		m_failure(eLGR_Failed)
	{}

	bool InitLoad(bool requireQuickLoad, const char* path);
};

class CGameSerialize : public IEntitySystemSink, ILevelSystemListener
{
public:
	CGameSerialize();
	~CGameSerialize();

	void RegisterSaveGameFactory(const char* name, SaveGameFactory factory);
	void RegisterLoadGameFactory(const char* name, LoadGameFactory factory);
	void GetMemoryStatistics(ICrySizer* s)
	{
		//[AlexMcC|19.04.10]: Avoid infinite recursion caused by passing function pointers to AddObject()
		//s->AddObject(m_saveGameFactories);
		//s->AddObject(m_loadGameFactories);
	}

	bool            SaveGame(CCryAction* pCryAction, const char* method, const char* name, ESaveGameReason reason = eSGR_QuickSave, const char* checkPointName = NULL);
	ELoadGameResult LoadGame(CCryAction* pCryAction, const char* method, const char* path, SGameStartParams& params, bool requireQuickLoad);

	void            RegisterFactories(IGameFramework* pFW);

	// IEntitySystemSink
	virtual bool OnBeforeSpawn(SEntitySpawnParams&) { return true; }
	virtual void OnSpawn(IEntity* pEntity, SEntitySpawnParams&);
	virtual bool OnRemove(IEntity* pEntity);
	virtual void OnReused(IEntity* pEntity, SEntitySpawnParams& entitySpawnParams) {}
	// ~IEntitySystemSink

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char* levelName)                    {}
	virtual void OnLoadingStart(ILevelInfo* pLevel);
	virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel)           {};
	virtual void OnLoadingComplete(ILevelInfo* pLevel)                     {}
	virtual void OnLoadingError(ILevelInfo* pLevel, const char* error)     {}
	virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) {}
	virtual void OnUnloadComplete(ILevelInfo* pLevel);
	// ~ILevelSystemListener

private:

	//save process steps
	bool SaveMetaData(SSaveEnvironment& savEnv);
	void SaveEngineSystems(SSaveEnvironment& savEnv);
	bool SaveEntities(SSaveEnvironment& savEnv);
	bool SaveGameData(SSaveEnvironment& savEnv, TSerialize& gameState, TEntitiesToSerialize& entities);

	//load process steps
	void LoadEngineSystems(SLoadEnvironment& loadEnv);
	bool LoadLevel(SLoadEnvironment& loadEnv, SGameStartParams& startParams, STempAutoResourcesLock& autoResourcesLock, bool bIsQuickLoading, bool requireQuickLoad);
	bool LoadEntities(SLoadEnvironment& loadEnv, std::unique_ptr<TSerialize> pGameStateSer);
	void LoadBasicEntityData(SLoadEnvironment& loadEnv);
	void LoadGameData(SLoadEnvironment& loadEnv);

	//helper functions
	void Clean();
	void ReserveEntityIds(const TBasicEntityDatas& basicEntityData);
	void FlushActivatableGameObjectExtensions();
	bool RepositionEntities(const TBasicEntityDatas& basicEntityData, bool insistOnAllEntitiesBeingThere);
	void DeleteDynamicEntities(const TBasicEntityDatas& basicEntityData);
	void DumpEntities();
	bool IsUserSignedIn(CCryAction* pCryAction) const;

#ifndef _RELEASE
	struct DebugSaveParts;

	void DebugPrintMemoryUsage(const DebugSaveParts& debugSaveParts) const;
#endif

private:
	std::map<string, SaveGameFactory> m_saveGameFactories;
	std::map<string, LoadGameFactory> m_loadGameFactories;

	typedef std::vector<EntityId> TEntityVector;
	typedef std::set<EntityId>    TEntitySet;
	TEntityVector m_serializeEntities;
	TEntitySet    m_dynamicEntities;
};

#endif
