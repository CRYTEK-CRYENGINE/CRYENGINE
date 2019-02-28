// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef GameAISystem_h
#define GameAISystem_h

#include "IGameAIModule.h"
#include "AdvantagePointOccupancyControl.h"
#include "DeathManager.h"
#include "VisibleObjectsHelper.h"
#include "TargetTrackThreatModifier.h"
#include "GameAIRecorder.h"
#include "AIAwarenessToPlayerHelper.h"
#include "AICounters.h"
#include "AISquadManager.h"
#include "EnvironmentDisturbanceManager.h"
#include "AICorpse.h"

class CGameAISystem
{
public:
	CGameAISystem();
	~CGameAISystem();
	IGameAIModule* FindModule(const char* moduleName) const;
	void EnterModule(EntityId entityID, const char* moduleName);
	void LeaveModule(EntityId entityID, const char* moduleName);
	void LeaveAllModules(EntityId entityID);
	void PauseModule(EntityId entityID, const char* moduleName);
	void PauseAllModules(EntityId entityID);
	void ResumeModule(EntityId entityID, const char* moduleName);
	void ResumeAllModules(EntityId entityID);
	void Update(float frameTime);
	void Reset(bool bUnload);
	void Serialize(TSerialize ser);
	void PostSerialize();

	CAdvantagePointOccupancyControl& GetAdvantagePointOccupancyControl() { return m_advantagePointOccupancyControl; }
	GameAI::DeathManager* GetDeathManager() { return m_pDeathManager; }
	CVisibleObjectsHelper& GetVisibleObjectsHelper() { return m_visibleObjectsHelper; }
	CAIAwarenessToPlayerHelper& GetAIAwarenessToPlayerHelper() { return m_AIAwarenessToPlayerHelper; }
	CAICounters& GetAICounters() { return m_AICounters; }
	AISquadManager& GetAISquadManager() { return m_AISquadManager; }
	GameAI::EnvironmentDisturbanceManager& GetEnvironmentDisturbanceManager() { return m_environmentDisturbanceManager; }

#ifdef INCLUDE_GAME_AI_RECORDER
	CGameAIRecorder &GetGameAIRecorder() { return m_gameAIRecorder; }
	const CGameAIRecorder &GetGameAIRecorder() const { return m_gameAIRecorder; }
#endif //INCLUDE_GAME_AI_RECORDER

	enum State
	{
		Idle,
		UpdatingModules
	};

private:
	void UpdateModules(float frameTime);
	void UpdateSubSystems(float frameTime);
	void ResetModules(bool bUnload);
	void ResetSubSystems(bool bUnload);
	void Error(const char* functionName) const;
	void InformContentCreatorOfError(string logMessage) const;
	void GetCallStack(string& callstack) const;
	typedef std::vector<IGameAIModule*> Modules;
	Modules m_modules;

	CAdvantagePointOccupancyControl m_advantagePointOccupancyControl;
	GameAI::DeathManager* m_pDeathManager;
	CVisibleObjectsHelper m_visibleObjectsHelper;
	CAIAwarenessToPlayerHelper m_AIAwarenessToPlayerHelper;
	CAICounters m_AICounters;
	CTargetTrackThreatModifier m_targetTrackThreatModifier;
	AISquadManager m_AISquadManager;
	GameAI::EnvironmentDisturbanceManager m_environmentDisturbanceManager;
	State m_state;

	CAICorpseManager* m_pCorpsesManager;

#ifdef INCLUDE_GAME_AI_RECORDER
	CGameAIRecorder m_gameAIRecorder;
#endif //INCLUDE_GAME_AI_RECORDER
};

#endif // GameAISystem_h
