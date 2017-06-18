// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PluginDll.h"

#include <CryEntitySystem/IEntityClass.h>

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

IEntityRegistrator* IEntityRegistrator::g_pFirst = nullptr;
IEntityRegistrator* IEntityRegistrator::g_pLast = nullptr;

class CSystemEventListener : public ISystemEventListener
{
public:
	~CSystemEventListener()
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override
	{
		switch (event)
		{
		case ESYSTEM_EVENT_GAME_POST_INIT:
			{
				// Register entities
				if (IEntityRegistrator::g_pFirst != nullptr)
				{
					do
					{
						IEntityRegistrator::g_pFirst->Register();

						IEntityRegistrator::g_pFirst = IEntityRegistrator::g_pFirst->m_pNext;
					}
					while (IEntityRegistrator::g_pFirst != nullptr);
				}

				// Register dummy entities
				IEntityClassRegistry::SEntityClassDesc stdClass;
				stdClass.flags |= ECLF_INVISIBLE | ECLF_DEFAULT;

				stdClass.sName = "AreaBox";
				gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(stdClass);

				stdClass.sName = "AreaSphere";
				gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(stdClass);

				stdClass.sName = "AreaSolid";
				gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(stdClass);

				stdClass.sName = "ClipVolume";
				gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(stdClass);

				stdClass.sName = "AreaShape";
				gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(stdClass);
			}
			break;
		}
	}
};

CSystemEventListener g_listener;

CPlugin_CryDefaultEntities::CPlugin_CryDefaultEntities()
{
	m_loadPriority = EPluginPriority_CryLoad;
	m_unloadPriority = EPluginPriority_CryUnload;
};

bool CPlugin_CryDefaultEntities::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	env.pSystem->GetISystemEventDispatcher()->RegisterListener(&g_listener,"CCryPluginManager::CSystemEventListener");

	return true;
}

CRYREGISTER_SINGLETON_CLASS(CPlugin_CryDefaultEntities)

#include <CryCore/CrtDebugStats.h>
