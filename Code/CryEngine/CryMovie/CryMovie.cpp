// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryMovie.h"
#include "Movie.h"
#include <CryCore/CrtDebugStats.h>

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

#undef GetClassName

struct CSystemEventListener_Movie : public ISystemEventListener
{
public:
	virtual ~CSystemEventListener_Movie()
	{
		if (gEnv->pSystem)
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
		}
	}

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			{
				CLightAnimWrapper::ReconstructCache();
				break;
			}
		}
	}
};

static CSystemEventListener_Movie g_system_event_listener_movie;

class CEngineModule_CryMovie : public IMovieEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IMovieEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryMovie, "EngineModule_CryMovie", "dce26bee-bdc6-400f-a0e9-b42839f2dd5b"_cry_guid)

	virtual ~CEngineModule_CryMovie()
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(&g_system_event_listener_movie);
		SAFE_RELEASE(gEnv->pMovieSystem);
	}

	virtual const char* GetName() const override { return "CryMovie"; };
	virtual const char* GetCategory() const override { return "CryEngine"; };

	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;
		pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_movie, "CEngineModule_CryMovie");

		env.pMovieSystem = new CMovieSystem(pSystem);
		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryMovie)