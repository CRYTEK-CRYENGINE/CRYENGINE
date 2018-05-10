// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include "world.h"

#ifndef STANDALONE_PHYSICS
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>
#endif
/* */

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Physics : public ISystemEventListener
{
public:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		switch (event)
		{
		//case ESYSTEM_EVENT_RANDOM_SEED:
		//	cry_random_seed(gEnv->bNoRandomSeed ? 0 : wparam);
		//	break;
		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			break;
		case ESYSTEM_EVENT_3D_POST_RENDERING_END:
			break;
		case ESYSTEM_EVENT_FAST_SHUTDOWN:
		case ESYSTEM_EVENT_FULL_SHUTDOWN:
			cpx::g_cryPhysX.DisconnectPhysicsDebugger();
			break;
		}
	}
};

static CSystemEventListner_Physics g_system_event_listener_physics;

//////////////////////////////////////////////////////////////////////////

CRYPHYSICS_API IPhysicalWorld *CreatePhysicalWorld(ISystem *pSystem)
{
	if (pSystem)
	{
		pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_physics, "CSystemEventListner_Physics");
		return new PhysXWorld(pSystem->GetILog());
	}

	return new PhysXWorld(0);
}

//////////////////////////////////////////////////////////////////////////

#ifndef STANDALONE_PHYSICS
//////////////////////////////////////////////////////////////////////////
class CEngineModule_CryPhysics : public IPhysicsEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IPhysicsEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryPhysics, "EngineModule_CryPhysics", "526cabf3-d776-407f-aa23-38545bb6ae7f"_cry_guid)

		//////////////////////////////////////////////////////////////////////////
		virtual const char *GetName() const override { return "CryPhysics"; };
	virtual const char *GetCategory() const override { return "CryEngine"; };

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment &env, const SSystemInitParams &initParams) override
	{
		ISystem* pSystem = env.pSystem;

		if (pSystem)
			pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_physics, "CSystemEventListner_Physics");

		env.pPhysicalWorld = new PhysXWorld(pSystem ? pSystem->GetILog() : 0);

		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryPhysics)

#endif
