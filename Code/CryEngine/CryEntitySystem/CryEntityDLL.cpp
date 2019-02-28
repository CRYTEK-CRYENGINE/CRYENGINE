// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryEntitySystem/IEntitySystem.h>
#include "EntitySystem.h"
#include "EntityUnitTests.h"
#include "EntityClassRegistry.h"

#include "Schematyc/EntitySchematycActions.h"
#include "Schematyc/EntitySchematycUtilFunctions.h"
#include "Schematyc/EntityUtilsComponent.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

#include  <CrySchematyc/Env/IEnvRegistry.h>
#include  <CrySchematyc/Env/Elements/EnvComponent.h>
#include  <CrySchematyc/Env/EnvPackage.h>
#include <CryCore/StaticInstanceList.h>

CEntitySystem* g_pIEntitySystem = nullptr;
constexpr CryGUID SchematyEntityComponentsPackageGUID = "A37D36D5-2AB1-4B48-9353-3DEC93A4236A"_cry_guid;

struct CSystemEventListener_Entity : public ISystemEventListener
{
public:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		switch (event)
		{
		case ESYSTEM_EVENT_GAME_POST_INIT:
			{
				static_cast<CEntityClassRegistry*>(g_pIEntitySystem->GetClassRegistry())->OnGameFrameworkInitialized();
			}
			break;
		case ESYSTEM_EVENT_REGISTER_SCHEMATYC_ENV:
			{

				auto entitySchematycRegistration = [](Schematyc::IEnvRegistrar& registrar)
				{
					Schematyc::CEntityTimerAction::Register(registrar);
					Schematyc::CEntityDebugTextAction::Register(registrar);
					Schematyc::Entity::RegisterUtilFunctions(registrar);
					Schematyc::CEntityUtilsComponent::Register(registrar);

					if (gEnv->bTesting)
					{
						RegisterUnitTestComponents(registrar);
					}
				};

				gEnv->pSchematyc->GetEnvRegistry().RegisterPackage(
				  stl::make_unique<Schematyc::CEnvPackage>(
				    SchematyEntityComponentsPackageGUID,
				    "EntityComponents",
				    "Crytek GmbH",
				    "CRYENGINE Default Entity Components",
				    entitySchematycRegistration
				    )
				  );
			}
			break;
		case ESYSTEM_EVENT_FULL_SHUTDOWN:
		case ESYSTEM_EVENT_FAST_SHUTDOWN:
			{
				// Deregister the Schematyc packages which were registered in the entity system.
				if (gEnv->pSchematyc)
				{
					gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(SchematyEntityComponentsPackageGUID);

					g_pIEntitySystem->GetClassRegistry()->UnregisterSchematycEntityClass();
				}
			}
			break;
		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			{
				if (g_pIEntitySystem)
				{
					if (!gEnv->pSystem->IsSerializingFile())
					{
						// activate the default layers
						g_pIEntitySystem->EnableDefaultLayers();
					}

					g_pIEntitySystem->OnLevelLoaded();
				}
			}
			break;
		case ESYSTEM_EVENT_3D_POST_RENDERING_END:
			if (g_pIEntitySystem)
			{
				g_pIEntitySystem->Unload();
			}
			break;
		}
	}
};
static CSystemEventListener_Entity g_system_event_listener_entity;

//////////////////////////////////////////////////////////////////////////
class CEngineModule_EntitySystem : public IEntitySystemEngineModule
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(IEntitySystemEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_EntitySystem, "EngineModule_CryEntitySystem", "88565507-2f01-4c03-820c-5a1a9b4d623b"_cry_guid)

	virtual ~CEngineModule_EntitySystem()
	{
		GetISystem()->GetISystemEventDispatcher()->RemoveListener(&g_system_event_listener_entity);
		SAFE_RELEASE(g_pIEntitySystem);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual const char* GetName() const override     { return "CryEntitySystem"; };
	virtual const char* GetCategory() const override { return "CryEngine"; };

	//////////////////////////////////////////////////////////////////////////
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;

		CEntitySystem* pEntitySystem = new CEntitySystem(pSystem);
		g_pIEntitySystem = pEntitySystem;
		if (!pEntitySystem->Init(pSystem))
		{
			pEntitySystem->Release();
			return false;
		}

		pSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_entity, "CSystemEventListener_Entity");

		env.pEntitySystem = pEntitySystem;
		return true;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_EntitySystem)

#include <CryCore/CrtDebugStats.h>
