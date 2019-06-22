// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "System.h"
#include <CryCore/Platform/platform_impl.inl>
#include "DebugCallStack.h"
#include "MemReplay.h"

#if CRY_PLATFORM_DURANGO
	#include "DurangoDebugCallstack.h"
#endif

#ifdef CRY_USE_CRASHRPT
	#include <CrashRpt.h>
extern bool g_bCrashRptInstalled;
#endif // CRY_USE_CRASHRPT

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	#include <CrySystem/Scaleform/IScaleformHelper.h>
#endif

HMODULE gDLLHandle = nullptr;

#if !defined(_LIB) && !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ORBIS
	#pragma warning( push )
	#pragma warning( disable : 4447 )
BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved
                      )
{
	gDLLHandle = (HMODULE)hModule;
	switch (ul_reason_for_call)
	{
	case DLL_THREAD_ATTACH:
	#ifdef CRY_USE_CRASHRPT
		if (g_bCrashRptInstalled)
		{
			crInstallToCurrentThread2(0);
		}
	#endif //CRY_USE_CRASHRPT
		break;

	//////////////////////////////////////////////////////////////////////////
	case DLL_THREAD_DETACH:
	#ifdef CRY_USE_CRASHRPT
		if (g_bCrashRptInstalled)
		{
			crUninstallFromCurrentThread();
		}
	#endif //CRY_USE_CRASHRPT
		break;
	}

	return TRUE;
}
	#pragma warning( pop )
#endif

#if CAPTURE_REPLAY_LOG
static struct SMemReplayInit
{
	SMemReplayInit() { CMemReplay::Init(); }
	~SMemReplayInit() { CMemReplay::Shutdown(); }
} s_memReplayInit;
#endif

#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
	#include <CryMemory/CryMemoryAllocator.h>
	#include <CryMemory/BucketAllocator.h>
extern void EnableDynamicBucketCleanups(bool enable);
#endif

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListener_System : public ISystemEventListener
{
public:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override
	{
#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_UNLOAD:
		case ESYSTEM_EVENT_LEVEL_LOAD_START:
		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			EnableDynamicBucketCleanups(true);
			break;

		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			EnableDynamicBucketCleanups(false);
			break;
		}
#endif

		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_UNLOAD:
			gEnv->pSystem->SetThreadState(ESubsys_Physics, false);
			break;

		case ESYSTEM_EVENT_LEVEL_LOAD_START:
		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			{
#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
				if (!gEnv->IsDedicated() && gEnv->pScaleformHelper)
				{
					gEnv->pScaleformHelper->ResetMeshCache();
				}
#endif
				CryCleanup();
				break;
			}

		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			{
				CryCleanup();
				gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
				break;
			}
		case ESYSTEM_EVENT_FULL_SHUTDOWN:
		case ESYSTEM_EVENT_FAST_SHUTDOWN:
			{
				if (gEnv && gEnv->pSystem)
				{
					ISystemEventDispatcher* pSystemEventDispatcher = gEnv->pSystem->GetISystemEventDispatcher();
					if (pSystemEventDispatcher)
					{
						pSystemEventDispatcher->RemoveListener(this);
					}
				}
			}
			break;
		}
	}
};

static CSystemEventListener_System g_system_event_listener_system;

extern "C"
{
	CRYSYSTEM_API ISystem* CreateSystemInterface(SSystemInitParams& startupParams, bool bManualEngineLoop)
	{
#if CAPTURE_REPLAY_LOG
		CMemReplay::GetInstance()->StartOnCommandLine(startupParams.szSystemCmdLine);
#endif
		MEMSTAT_CONTEXT(EMemStatContextType::Other, "Mainthread");

		std::unique_ptr<CSystem> pSystem;
		{
			MEMSTAT_CONTEXT(EMemStatContextType::Other, "System: Engine Startup");

#if CRY_PLATFORM_DURANGO
			DurangoDebugCallStack::InstallExceptionHandler();
#endif

			pSystem = stl::make_unique<CSystem>(startupParams);
			startupParams.pSystem = pSystem.get();

			CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "CreateSystemInterface");
			ModuleInitISystem(pSystem.get(), "CrySystem");
#if CRY_PLATFORM_DURANGO
#if !defined(_LIB)
			gEnv = pSystem->GetGlobalEnvironment();
#endif
			gEnv->pWindow = startupParams.hWnd;
#endif

#if CRY_PLATFORM_WINDOWS
			// Install exception handler in Release modes.
			((DebugCallStack*)IDebugCallStack::instance())->installErrorHandler(pSystem.get());
#endif

			ICryFactoryRegistryImpl* pCryFactoryImpl = static_cast<ICryFactoryRegistryImpl*>(pSystem->GetCryFactoryRegistry());
			pCryFactoryImpl->RegisterFactories(g_pHeadToRegFactories);

			// the earliest point the system exists - w2e tell the callback
			if (startupParams.pUserCallback)
				startupParams.pUserCallback->OnSystemConnect(pSystem.get());

			if (!pSystem->Initialize(startupParams))
			{
				CryMessageBox("CrySystem initialization failed!", "Engine initialization failed!");
				pSystem.release();
				startupParams.pSystem = nullptr;
				gEnv->pSystem = nullptr;

				return nullptr;
			}

			pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE, 0, 0);
			pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_system, "CSystemEventListener_System");
		}

		// run main loop
		if (!bManualEngineLoop)
		{
			pSystem->RunMainLoop();
			return nullptr;
		}
		return pSystem.release();
	}

#if defined(ENABLE_PROFILING_CODE) && !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE
	CRYSYSTEM_API void CryInstallPostExceptionHandler(void (* PostExceptionHandlerCallback)())
	{
		return IDebugCallStack::instance()->FileCreationCallback(PostExceptionHandlerCallback);
	}
#endif

};
