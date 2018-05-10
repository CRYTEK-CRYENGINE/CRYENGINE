// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "CryLiveCreate.h"
#include "LiveCreateManager.h"
#include "LiveCreateCommands.h"
#include "LiveCreateHost.h"
#include <CryCore/CrtDebugStats.h>

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

#ifndef NO_LIVECREATE

	#if CRY_PLATFORM_WINDOWS
		#ifndef _LIB
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}
		#endif
	#endif

namespace LiveCreate
{
void RegisterCommandClasses()
{
	// Please put a line for all of the remote commands you want to call on the LiveCreate Hosts
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetCameraPosition);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetCameraFOV);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_EnableLiveCreate);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_DisableLiveCreate);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_EnableCameraSync);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_DisableCameraSync);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetEntityTransform);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_EntityUpdate);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_EntityDelete);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetCVar);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetTimeOfDayValue);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetTimeOfDay);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetTimeOfDayFull);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetEnvironmentFull);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_ObjectAreaUpdate);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetParticlesFull);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SetArchetypesFull);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_FileSynced);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_EntityPropertyChange);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_UpdateLightParams);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SyncSelection);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_EntitySetMaterial);
	REGISTER_LIVECREATE_COMMAND(CLiveCreateCmd_SyncLayerVisibility);
}
}

//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryLiveCreate : public ILiveCreateEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(ILiveCreateEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryLiveCreate, "EngineModule_CryLiveCreate", "dc126bee-bdc6-411f-a111-b42839f2dd1b"_cry_guid)

	virtual ~CEngineModule_CryLiveCreate() {}

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName() const override { return "CryLiveCreate"; };
	virtual const char* GetCategory() const override { return "CryEngine"; };

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
	{
		ISystem* pSystem = env.pSystem;

		pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_livecreate, "CEngineModule_CryLiveCreate");

		// new implementation
	#if CRY_PLATFORM_WINDOWS
		// Create the manager only on PC editor configuration
		if (env.IsEditor())
		{
			env.pLiveCreateManager = new LiveCreate::CManager();
		}
	#endif

		// create the host implementation only if not in editor
	#if CRY_PLATFORM_WINDOWS
		if (!env.IsEditor())
		{
			env.pLiveCreateHost = new LiveCreate::CHost();
		}
	#else
		env.pLiveCreateHost = new LiveCreate::CHost();
	#endif

		// always register LiveCreate commands (weather on host or on manager side)
		LiveCreate::RegisterCommandClasses();
		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryLiveCreate)

LiveCreate::IManager* CreateLiveCreate(ISystem* pSystem)
{
	return NULL;
}

void DeleteLiveCreate(LiveCreate::IManager* pLC)
{
}

#endif
