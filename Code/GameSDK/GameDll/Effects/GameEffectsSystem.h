// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GAME_EFFECTS_SYSTEM_
#define _GAME_EFFECTS_SYSTEM_

#pragma once

// Includes
#include "GameEffectsSystemDefines.h"
#include "GameRulesTypes.h"

//==================================================================================================
// Name: CGameEffectsSystem
// Desc: System to handle game effects, game render nodes and game render elements
//			 Game effect: separates out effect logic from game logic
//			 Game render node: handles the render object in 3d space
//			 Game render element: handles the rendering of the object
//			 CVar activation system: system used to have data driven cvars activated in game effects
//			 Post effect activation system: system used to have data driven post effects activated in game effects
// Author: James Chilvers
//==================================================================================================
class CGameEffectsSystem : public IInputEventListener, public SGameRulesListener
{
	friend struct SGameEffectSystemStaticData;

public:
	// ---------- Singleton static functions ----------
	static inline CGameEffectsSystem&		Instance()
	{
		if(s_singletonInstance == NULL)
		{
			s_singletonInstance = new CGameEffectsSystem;
		}
		return *s_singletonInstance;
	}

	static void							Destroy();
	static ILINE bool				Exists()
	{
		return (s_singletonInstance) ? true : false;
	}
	//-------------------------------------------------

	void										Initialise();
	void										LoadData();
	void										ReleaseData();

	template<class T>
	T*											CreateEffect()  // Use if dynamic memory allocation is required for the game effect
	{																				// Using this function then allows easy changing of memory allocator for all dynamically created effects
		T* newEffect = new T;
		return newEffect;
	}

	void							RegisterEffect(IGameEffect* effect); // Each effect automatically registers and unregisters itself
	void							UnRegisterEffect(IGameEffect* effect);

	void							Update(float frameTime);

	void							RegisterGameRenderNode(IGameRenderNodePtr& pGameRenderNode);
	void							UnregisterGameRenderNode(IGameRenderNodePtr& pGameRenderNode);

	void							RegisterGameRenderElement(IGameRenderElementPtr& pGameRenderElement);
	void							UnregisterGameRenderElement(IGameRenderElementPtr& pGameRenderElement);

	bool										OnInputEvent(const SInputEvent& inputEvent);

#if DEBUG_GAME_FX_SYSTEM
	// Creating a static version of SRegisterEffectDebugData inside an effect cpp registers the effect's debug data with the game effects system
	struct SRegisterEffectDebugData
	{
		SRegisterEffectDebugData(DebugOnInputEventCallback inputEventCallback,DebugDisplayCallback debugDisplayCallback, const char* effectName)
		{
			CGameEffectsSystem::RegisterEffectDebugData(inputEventCallback,debugDisplayCallback,effectName);
		}
	};

	int								GetDebugView() const { return m_debugView; }
	IGameEffect*			GetDebugEffect(const char* pEffectName) const;
#endif

	// Creating a static version of SRegisterDataCallbacks inside an effect cpp registers the effect's data callback functions with the game effects system
	struct SRegisterDataCallbacks
	{
		SRegisterDataCallbacks(DataLoadCallback dataLoadingCallback,DataReleaseCallback dataReleaseCallback,DataReloadCallback dataReloadCallback)
		{
			CGameEffectsSystem::RegisterDataLoadCallback(dataLoadingCallback);
			CGameEffectsSystem::RegisterDataReleaseCallback(dataReleaseCallback);
			CGameEffectsSystem::RegisterDataReloadCallback(dataReloadCallback);
		}
	};

	// Creating a static version of SRegisterGameCallbacks inside an effect cpp registers the effect's game callback functions with the game effects system
	struct SRegisterGameCallbacks
	{
		SRegisterGameCallbacks(EnteredGameCallback enteredGameCallback)
		{
			CGameEffectsSystem::RegisterEnteredGameCallback(enteredGameCallback);
		}
	};

	// SGameRulesListener implementation
	void GameRulesInitialise();
	virtual void EnteredGame();

	void ReloadData();

protected:
	CGameEffectsSystem();
	virtual ~CGameEffectsSystem();

private:

	void										Reset();
	void										AutoReleaseAndDeleteFlaggedEffects(IGameEffect* effectList);
	void										AutoDeleteEffects(IGameEffect* effectList);
	void										SetPostEffectCVarCallbacks();
	static void							PostEffectCVarCallback(ICVar* cvar);

	static void							RegisterDataLoadCallback(DataLoadCallback dataLoadCallback);
	static void							RegisterDataReleaseCallback(DataReleaseCallback dataReleaseCallback);
	static void							RegisterDataReloadCallback(DataReloadCallback dataReloadCallback);

	static void							RegisterEnteredGameCallback(EnteredGameCallback enteredGameCallback);

#if DEBUG_GAME_FX_SYSTEM
	void										DrawDebugDisplay();
	void										OnActivateDebugView(int debugView);
	void										OnDeActivateDebugView(int debugView);
	static void							RegisterEffectDebugData(	DebugOnInputEventCallback inputEventCallback,
																										DebugDisplayCallback displayCallback,
																										const char* effectName);

	struct SEffectDebugData
	{
		SEffectDebugData(	DebugOnInputEventCallback paramInputCallback,
											DebugDisplayCallback paramDisplayCallback,
											const char* paramEffectName)
		{
			inputCallback = paramInputCallback;
			displayCallback = paramDisplayCallback;
			effectName = paramEffectName;
		}
		DebugOnInputEventCallback inputCallback;
		DebugDisplayCallback			displayCallback;
		const char*								effectName;
	};

	static int												s_currentDebugEffectId;

	int																m_debugView;
#endif

	static CGameEffectsSystem*	s_singletonInstance;
	static int									s_postEffectCVarNameOffset;

	IGameEffect*								m_effectsToUpdate;
	IGameEffect*								m_effectsNotToUpdate;
	IGameEffect*								m_nextEffectToUpdate; // -> If in update loop, this is the next effect to be updated
																										//    this will get changed if the effect is unregistered
	bool												m_isInitialised;
	static bool									s_hasLoadedData;
};//------------------------------------------------------------------------------------------------

#endif // _GAME_EFFECTS_SYSTEM_
