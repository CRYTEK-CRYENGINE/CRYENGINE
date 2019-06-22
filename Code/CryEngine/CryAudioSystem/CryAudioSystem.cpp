// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Common.h"
#include "CVars.h"
#include "System.h"
#include <CrySystem/ISystem.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>
#include <CrySystem/ConsoleRegistration.h>

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include <CryGame/IGameFramework.h>
#endif // CRY_AUDIO_USE_DEBUG_CODE

#if CRY_PLATFORM_DURANGO
	#include <apu.h>
#endif // CRY_PLATFORM_DURANGO

#define MAX_MODULE_NAME_LENGTH 256

namespace CryAudio
{
// Define global objects.
CCVars g_cvars;

///////////////////////////////////////////////////////////////////////////
bool CreateAudioSystem(SSystemGlobalEnvironment& env)
{
	bool bSuccess = false;

	if (env.pAudioSystem != nullptr)
	{
		env.pAudioSystem->Release();
		env.pAudioSystem = nullptr;
	}

	env.pAudioSystem = static_cast<IAudioSystem*>(&g_system);
	bSuccess = g_system.Initialize();

	return bSuccess;
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
class CEngineModule_CryAudioSystem : public ISystemModule
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(ISystemModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryAudioSystem, "EngineModule_CryAudioSystem", "ec73cf43-62ca-4a7f-8b45-1076dc6fdb8b"_cry_guid)

	CEngineModule_CryAudioSystem();
	virtual ~CEngineModule_CryAudioSystem() override;

	virtual const char* GetName() const override     { return "CryAudioSystem"; }
	virtual const char* GetCategory() const override { return "CryEngine"; }

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		s_pInitParameters = &initParams;

		bool bSuccess = false;

		if (CreateAudioSystem(env))
		{
#if CRY_PLATFORM_DURANGO
			{
				// Do this before initializing the audio middleware!
				MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "AudioSystem ApuCreateHeap");
				HRESULT const result = ApuCreateHeap(static_cast<UINT32>(g_cvars.m_fileCacheManagerSize << 10));

				if (result != S_OK)
				{
					CryFatalError("<Audio>: AudioSystem failed to allocate APU heap! (%d byte)", g_cvars.m_fileCacheManagerSize << 10);
				}
			}
#endif  // CRY_PLATFORM_DURANGO

			s_currentModuleName = m_pImplNameCVar->GetString();

			// Get the first CryAudio::IImplModule factory available in the module and create an instance of it
			auto pModule = env.pSystem->LoadModuleAndCreateFactoryInstance<IImplModule>(s_currentModuleName.c_str(), initParams);

			if (pModule != nullptr)
			{
				auto const pSystem = static_cast<CSystem*>(env.pAudioSystem);
				CryFixedStringT<MaxFilePathLength> const temp(pSystem->GetConfigPath());
				pSystem->ParseControlsData(temp.c_str(), g_szGlobalContextName, GlobalContextId);
				pSystem->ParsePreloadsData(temp.c_str(), GlobalContextId);
				pSystem->PreloadSingleRequest(GlobalPreloadRequestId, false);
				pSystem->AutoLoadSetting(GlobalContextId);
			}
			else
			{
				// In case of a failure set the null-implementation.
				SRequestUserData const data(ERequestFlags::ExecuteBlocking);
				static_cast<CSystem*>(env.pAudioSystem)->SetImpl(nullptr, data);
			}

			// As soon as the audio system was created we consider this a success (even if the NULL implementation was used)
			bSuccess = true;
		}

		return bSuccess;
	}

	//////////////////////////////////////////////////////////////////////////
	static void OnImplChanged(ICVar* pImplNameCvar)
	{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		CryFixedStringT<MAX_MODULE_NAME_LENGTH> const previousModuleName(s_currentModuleName);
		auto const pSystem = static_cast<CSystem*>(gEnv->pAudioSystem);
		SSystemInitParams systemInitParams;
		s_currentModuleName = pImplNameCvar->GetString();

		if (!previousModuleName.empty())
		{
			SRequestUserData const data(ERequestFlags::ExecuteBlocking);
			pSystem->SetImpl(nullptr, data);

			gEnv->pSystem->UnloadEngineModule(previousModuleName.c_str());
		}

		auto pModule = gEnv->pSystem->LoadModuleAndCreateFactoryInstance<IImplModule>(s_currentModuleName.c_str(), *s_pInitParameters);

		SRequestUserData const data(ERequestFlags::ExecuteBlocking);

		// Trying to load and initialize the new engine module will release the currently
		// running implementation but only if the library loaded successfully.
		if (pModule != nullptr)
		{
			CryFixedStringT<MaxFilePathLength> const temp(pSystem->GetConfigPath());
			pSystem->ParseControlsData(temp.c_str(), g_szGlobalContextName, GlobalContextId);
			pSystem->ParsePreloadsData(temp.c_str(), GlobalContextId);

			// Needs to be blocking to avoid executing triggers while data is loading.
			pSystem->PreloadSingleRequest(GlobalPreloadRequestId, false, data);
			pSystem->AutoLoadSetting(GlobalContextId);

			string const contextName = PathUtil::GetFileName(gEnv->pGameFramework->GetLevelName());

			if (!contextName.empty() && contextName.compareNoCase("Untitled") != 0)
			{
				ContextId const contextId = StringToId(contextName.c_str());

				string contextPath(gEnv->pAudioSystem->GetConfigPath());
				contextPath += g_szContextsFolderName;
				contextPath += "/";
				contextPath += contextName;

				pSystem->ParseControlsData(contextPath.c_str(), contextName.c_str(), contextId);
				pSystem->ParsePreloadsData(contextPath.c_str(), contextId);

				auto const preloadRequestId = static_cast<PreloadRequestId>(contextId);

				// Needs to be blocking to avoid executing triggers while data is loading.
				pSystem->PreloadSingleRequest(preloadRequestId, true, data);
			}

			pSystem->RetriggerControls();
		}
		else
		{
			// We could fail in two ways.
			// Either the module did not load in which case unloading of s_currentModuleName is redundant
			// or the module did not initialize in which case setting the null implementation is redundant.
			// As we currently do not know here how exactly the module failed we play safe and always set the null implementation and unload the modules.
			pSystem->SetImpl(nullptr, data);

			// The module failed to initialize, unload both as we are running the null implementation now.
			gEnv->pSystem->UnloadEngineModule(s_currentModuleName.c_str());
			s_currentModuleName.clear();
		}

		// In any case send the event as we always loaded some implementation (either the proper or the NULL one).
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_AUDIO_IMPLEMENTATION_LOADED, 0, 0);
#endif  // CRY_AUDIO_USE_DEBUG_CODE
	}

private:

	ICVar*                                         m_pImplNameCVar;
	static const SSystemInitParams*                s_pInitParameters;
	static CryFixedStringT<MAX_MODULE_NAME_LENGTH> s_currentModuleName;
};

const SSystemInitParams* CEngineModule_CryAudioSystem::s_pInitParameters = nullptr;
CryFixedStringT<MAX_MODULE_NAME_LENGTH> CEngineModule_CryAudioSystem::s_currentModuleName;
CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryAudioSystem)

//////////////////////////////////////////////////////////////////////////
CEngineModule_CryAudioSystem::CEngineModule_CryAudioSystem()
{
	if (gEnv->pSystem != nullptr)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system, "CryAudio::CSystem");
	}

	m_pImplNameCVar = REGISTER_STRING_CB(g_szImplCVarName, "CryAudioImplSDLMixer", 0,
	                                     "Holds the name of the audio implementation library to be used.\n"
	                                     "Usage: s_ImplName <name of the library without extension>\n"
	                                     "Default: CryAudioImplSDLMixer\n",
	                                     CEngineModule_CryAudioSystem::OnImplChanged);
}

//////////////////////////////////////////////////////////////////////////
CEngineModule_CryAudioSystem::~CEngineModule_CryAudioSystem()
{
	if (gEnv->pConsole != nullptr)
	{
		gEnv->pConsole->UnregisterVariable(g_szImplCVarName);
	}

	if (gEnv->pSystem != nullptr)
	{
		gEnv->pSystem->UnloadEngineModule(s_currentModuleName.c_str());
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(&g_system);
	}
}
} // namespace CryAudio
#include <CryCore/CrtDebugStats.h>
