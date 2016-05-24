// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SoundCVars.h"
#include "AudioSystem.h"
#include <CryAudio/IAudioSystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryMemory/BucketAllocatorImpl.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
#include <../CryAction/IViewSystem.h>
#include <CryGame/IGameFramework.h>
#include <CryEntitySystem/IEntitySystem.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

#if CRY_PLATFORM_DURANGO
#include <apu.h>
#endif // CRY_PLATFORM_DURANGO

// Define global objects.
CAudioCVars  g_audioCVars;
CAudioLogger g_audioLogger;
CSoundAllocator g_audioMemoryPoolPrimary;
CTimeValue g_lastMainThreadFrameStartTime;

#define MAX_MODULE_NAME_LENGTH 256

//////////////////////////////////////////////////////////////////////////
class CSystemEventListner_Sound:public ISystemEventListener
{
public:

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_LOAD_START:
		{
			g_audioMemoryPoolPrimary.Cleanup();
			break;
		}
		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
		{
			g_audioMemoryPoolPrimary.Cleanup();
			break;
		}
		case ESYSTEM_EVENT_RANDOM_SEED:
		{
			cry_random_seed(gEnv->bNoRandomSeed ? 0 : (uint32)wparam);
			break;
		}
		case ESYSTEM_EVENT_ACTIVATE:
		{
			// When Alt+Tabbing out of the application while it's in fullscreen mode
			// ESYSTEM_EVENT_ACTIVATE is sent instead of ESYSTEM_EVENT_CHANGE_FOCUS.

			// wparam != 0 is active, wparam == 0 is inactive
			// lparam != 0 is minimized, lparam == 0 is not minimized

			if (wparam == 0 || lparam != 0)
			{
				//lost focus
				if (gEnv->pAudioSystem != nullptr)
				{
					gEnv->pAudioSystem->PushRequest(m_loseFocusRequest);
				}
			}
			else
			{
				// got focus
				if (gEnv->pAudioSystem != nullptr)
				{
					gEnv->pAudioSystem->PushRequest(m_getFocusRequest);
				}
			}

			break;
		}
		case ESYSTEM_EVENT_CHANGE_FOCUS:
		{
			// wparam != 0 is focused, wparam == 0 is not focused
			if (wparam == 0)
			{
				// lost focus
				if (gEnv->pAudioSystem != nullptr)
				{
					gEnv->pAudioSystem->PushRequest(m_loseFocusRequest);
				}
			}
			else
			{
				// got focus
				if (gEnv->pAudioSystem != nullptr)
				{
					gEnv->pAudioSystem->PushRequest(m_getFocusRequest);
				}
			}
			break;
		}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void InitRequestData()
	{
		if (gEnv->pAudioSystem != nullptr)
		{
			m_loseFocusRequest.flags = eAudioRequestFlags_PriorityHigh;
			m_loseFocusRequest.pData = &m_loseFocusRequestData;
			m_getFocusRequest.flags  = eAudioRequestFlags_PriorityHigh;
			m_getFocusRequest.pData  = &m_getFocusRequestData;
		}
	}

private:

	SAudioRequest m_loseFocusRequest;
	SAudioRequest m_getFocusRequest;
	SAudioManagerRequestData<eAudioManagerRequestType_LoseFocus> m_loseFocusRequestData;
	SAudioManagerRequestData<eAudioManagerRequestType_GetFocus>  m_getFocusRequestData;
};

static CSystemEventListner_Sound g_system_event_listener_sound;

///////////////////////////////////////////////////////////////////////////
bool CreateAudioSystem(SSystemGlobalEnvironment& env)
{
	bool bSuccess = false;

	// The AudioSystem must be the first object that is allocated from the audio memory pool after it has been created and the last that is freed from it!
	POOL_NEW_CREATE(CAudioSystem, pAudioSystem);

	if (pAudioSystem != nullptr)
	{
		//release the old AudioSystem
		if (env.pAudioSystem != nullptr)
		{
			env.pAudioSystem->Release();
			env.pAudioSystem = nullptr;
		}

		env.pAudioSystem = static_cast<IAudioSystem*>(pAudioSystem);
		bSuccess         = env.pAudioSystem->Initialize();
	}
	else
	{
		g_audioLogger.Log(eAudioLogType_Error, "Could not create an instance of CAudioSystem! Keeping the default AudioSystem!\n");
	}

	return bSuccess;
}

///////////////////////////////////////////////////////////////////////////
void PrepareAudioSystem(IAudioSystem* const pAudioSystem)
{
	CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> const temp(pAudioSystem->GetConfigPath());

	// Must be blocking requests.
	SAudioRequest request;
	request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking;

	SAudioManagerRequestData<eAudioManagerRequestType_ParseControlsData> requestData1(temp.c_str(), eAudioDataScope_Global);
	request.pData = &requestData1;
	pAudioSystem->PushRequest(request);

	SAudioManagerRequestData<eAudioManagerRequestType_ParsePreloadsData> requestData2(temp.c_str(), eAudioDataScope_Global);
	request.pData = &requestData2;
	pAudioSystem->PushRequest(request);

	SAudioManagerRequestData<eAudioManagerRequestType_PreloadSingleRequest> requestData3(SATLInternalControlIDs::globalPreloadRequestId, false);
	request.pData = &requestData3;
	gEnv->pAudioSystem->PushRequest(request);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// dll interface

void AddPhysicalBlock(long size)
{
#ifndef _LIB
	_CryMemoryManagerPoolHelper::FakeAllocation(size);
#else
	GetISystem()->GetIMemoryManager()->FakeAllocation(size);
#endif
}

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryAudioSystem:public IEngineModule
{
	CRYINTERFACE_SIMPLE(IEngineModule)
	CRYGENERATE_SINGLETONCLASS(CEngineModule_CryAudioSystem, "EngineModule_CryAudioSystem", 0xec73cf4362ca4a7f, 0x8b451076dc6fdb8b)

	virtual const char* GetName() override {return "CryAudioSystem"; }
	virtual const char* GetCategory() override {return "CryEngine"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		bool bSuccess = false;

		// initialize memory pools
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "ATL Memory Pool");
		size_t const primaryPoolSize    = g_audioCVars.m_audioPrimaryPoolSize << 10;
		uint8* const pPrimaryPoolMemory = new uint8[primaryPoolSize];
		g_audioMemoryPoolPrimary.InitMem(primaryPoolSize, pPrimaryPoolMemory, "Audio Primary Memory Pool");

		if (CreateAudioSystem(env))
		{
#if CRY_PLATFORM_DURANGO
			// Do this before initializing the audio middleware!
			HRESULT const result = ApuCreateHeap(static_cast<UINT32>(g_audioCVars.m_fileCacheManagerSize << 10));

			if (result != S_OK)
			{
				CryFatalError("<Audio>: AudioSystem failed to allocate APU heap! (%d byte)", g_audioCVars.m_fileCacheManagerSize << 10);
			}
#endif      // CRY_PLATFORM_DURANGO

			s_currentModuleName = m_pAudioImplNameCVar->GetString();

			if (env.pSystem->InitializeEngineModule(s_currentModuleName.c_str(), s_currentModuleName.c_str(), false))
			{
				PrepareAudioSystem(env.pAudioSystem);
			}
			else
			{
				// In case of a failure always set NULL implementation
				SAudioRequest request;
				request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking;

				SAudioManagerRequestData<eAudioManagerRequestType_SetAudioImpl> requestData(nullptr);
				request.pData = &requestData;
				env.pAudioSystem->PushRequest(request);
			}

			g_system_event_listener_sound.InitRequestData();
			env.pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_sound);

			// As soon as the audio system was created we consider this a success (even if the NULL implementation was used)
			bSuccess = true;
		}
		else
		{
			g_audioLogger.Log(eAudioLogType_Error, "Could not create AudioSystem!");
		}

		return bSuccess;
	}

	//////////////////////////////////////////////////////////////////////////
	static void OnAudioImplChanged(ICVar* pAudioImplNameCvar)
	{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		CryFixedStringT<MAX_MODULE_NAME_LENGTH> const previousModuleName(s_currentModuleName);

		SSystemInitParams systemInitParams;
		s_currentModuleName = pAudioImplNameCvar->GetString();

		// First try to load and initialize the new engine module.
		// This will release the currently running implementation but only if the library loaded successfully.
		if (gEnv->pSystem->InitializeEngineModule(s_currentModuleName.c_str(), s_currentModuleName.c_str(), false))
		{
			SAudioRequest request;
			request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking;

			// Unload the previous module if the new one initialized successfully..
			if (!previousModuleName.empty())
			{
				gEnv->pSystem->UnloadEngineModule(previousModuleName.c_str(), previousModuleName.c_str());
			}

			// Then load global controls data and preloads.
			PrepareAudioSystem(gEnv->pAudioSystem);

			// Then load level specific controls data and preloads.
			string const levelName = PathUtil::GetFileName(gEnv->pGame->GetIGameFramework()->GetLevelName());

			if (!levelName.empty() && levelName.compareNoCase("Untitled") != 0)
			{
				string levelPath(gEnv->pAudioSystem->GetConfigPath());
				levelPath += "levels" CRY_NATIVE_PATH_SEPSTR;
				levelPath += levelName;

				// Needs to be blocking so data is available for next preloading request!
				SAudioManagerRequestData<eAudioManagerRequestType_ParseControlsData> requestData1(levelPath, eAudioDataScope_LevelSpecific);
				request.pData = &requestData1;
				gEnv->pAudioSystem->PushRequest(request);

				SAudioManagerRequestData<eAudioManagerRequestType_ParsePreloadsData> requestData2(levelPath, eAudioDataScope_LevelSpecific);
				request.pData = &requestData2;
				gEnv->pAudioSystem->PushRequest(request);

				AudioPreloadRequestId audioPreloadRequestId = INVALID_AUDIO_PRELOAD_REQUEST_ID;

				if (gEnv->pAudioSystem->GetAudioPreloadRequestId(levelName.c_str(), audioPreloadRequestId))
				{
					SAudioManagerRequestData<eAudioManagerRequestType_PreloadSingleRequest> requestData3(audioPreloadRequestId, true);
					request.pData = &requestData3;
					gEnv->pAudioSystem->PushRequest(request);
				}
			}

			// Then adjust the listener transformation to the active view's transformation.
			IGame* const pIGame = gEnv->pGame;

			if (pIGame != nullptr)
			{
				IGameFramework* const pIGameFramework = pIGame->GetIGameFramework();

				if (pIGameFramework != nullptr)
				{
					IViewSystem* const pIViewSystem = pIGameFramework->GetIViewSystem();

					if (pIViewSystem != nullptr)
					{
						IView* const pActiveView = pIViewSystem->GetActiveView();

						if (pActiveView != nullptr)
						{
							EntityId const id       = pActiveView->GetLinkedId();
							IEntity const* pIEntity = gEnv->pEntitySystem->GetEntity(id);

							if (pIEntity != nullptr)
							{
								SAudioListenerRequestData<eAudioListenerRequestType_SetTransformation> requestData4(pIEntity->GetWorldTM());
								request.pData = &requestData4;

								gEnv->pAudioSystem->PushRequest(request);
							}
						}
					}
				}
			}

			// And finally re-trigger all active audio controls to restart previously playing sounds.
			SAudioManagerRequestData<eAudioManagerRequestType_RetriggerAudioControls> requestData5;
			request.pData = &requestData5;
			gEnv->pAudioSystem->PushRequest(request);
		}
		else
		{
			// We could fail in two ways.
			// Either the module did not load in which case unloading of s_currentModuleName is redundant
			// or the module did not initialize in which case setting the null implementation is redundant.
			// As we currently do not know here how exactly the module failed we play safe and always set the null implementation and unload the modules.
			SAudioRequest request;
			SAudioManagerRequestData<eAudioManagerRequestType_SetAudioImpl> requestData(nullptr);
			request.flags = eAudioRequestFlags_PriorityHigh | eAudioRequestFlags_ExecuteBlocking;
			request.pData = &requestData;
			gEnv->pAudioSystem->PushRequest(request);

			// The module failed to initialize, unload both as we are running the null implementation now.
			gEnv->pSystem->UnloadEngineModule(previousModuleName.c_str(), previousModuleName.c_str());
			gEnv->pSystem->UnloadEngineModule(s_currentModuleName.c_str(), s_currentModuleName.c_str());
			s_currentModuleName.clear();
		}

		// In any case send the event as we always loaded some implementation (either the proper or the NULL one).
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_AUDIO_IMPLEMENTATION_LOADED, 0, 0);
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE
	}

private:

	ICVar* m_pAudioImplNameCVar;
	static CryFixedStringT<MAX_MODULE_NAME_LENGTH> s_currentModuleName;
};

CryFixedStringT<MAX_MODULE_NAME_LENGTH> CEngineModule_CryAudioSystem::s_currentModuleName;
CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioSystem)

//////////////////////////////////////////////////////////////////////////
CEngineModule_CryAudioSystem::CEngineModule_CryAudioSystem()
{
	// Register audio cvars
	m_pAudioImplNameCVar = REGISTER_STRING_CB("s_AudioImplName", "CryAudioImplSDLMixer", 0,
		"Holds the name of the audio implementation library to be used.\n"
		"Usage: s_AudioImplName <name of the library without extension>\n"
		"Default: CryAudioImplSDLMixer\n",
		CEngineModule_CryAudioSystem::OnAudioImplChanged);

	g_audioCVars.RegisterVariables();
}

//////////////////////////////////////////////////////////////////////////
CEngineModule_CryAudioSystem::~CEngineModule_CryAudioSystem()
{
}

#include <CryCore/CrtDebugStats.h>
