// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ManagedPlugin.h"

#include "MonoRuntime.h"
#include "Wrappers/AppDomain.h"
#include "Wrappers/MonoLibrary.h"
#include "Wrappers/MonoClass.h"
#include "Wrappers/MonoMethod.h"
#include <CrySystem/IProjectManager.h>

std::vector<std::shared_ptr<CManagedEntityComponentFactory>>* CManagedPlugin::s_pCurrentlyRegisteringFactories = nullptr;
std::vector<std::shared_ptr<CManagedEntityComponentFactory>>* CManagedPlugin::s_pCrossPluginRegisteredFactories = new std::vector<std::shared_ptr<CManagedEntityComponentFactory>>();

CManagedPlugin::CManagedPlugin(const char* szBinaryPath)
	: m_pLibrary(nullptr)
	, m_libraryPath(szBinaryPath)
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CManagedPlugin");
}

CManagedPlugin::CManagedPlugin(CMonoLibrary* pLibrary)
	: m_pLibrary(pLibrary)
	, m_libraryPath(pLibrary != nullptr ? pLibrary->GetFilePath() : "")
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CManagedPlugin");
}

CManagedPlugin::~CManagedPlugin()
{
	if (gEnv->pGameFramework)
	{
		gEnv->pGameFramework->RemoveNetworkedClientListener(*this);
	}

	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	if (m_pMonoObject != nullptr)
	{
		if (CMonoClass* pClass = m_pMonoObject->GetClass())
		{
			if (std::shared_ptr<CMonoMethod> pShutdownMethod = pClass->FindMethod("Shutdown").lock())
			{
				pShutdownMethod->Invoke(m_pMonoObject.get());
			}
		}
	}
}

void CManagedPlugin::InitializePlugin()
{
	CreatePluginInstance();

	if (m_pLibrary->WasCompiledAtRuntime())
	{
		m_guid = gEnv->pSystem->GetIProjectManager()->GetCurrentProjectGUID();

		if (m_name.size() == 0)
		{
			m_name = gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName();
		}
	}
	else
	{
		// Load GUID from assembly, should be the same every time
		m_guid = CryGUID::FromString(mono_image_get_guid(m_pLibrary->GetImage()));

		if (m_name.size() == 0)
		{
			m_name = PathUtil::GetFileName(m_pLibrary->GetFilePath());
		}
	}

	ScanAssembly();

	if (m_pMonoObject != nullptr)
	{
		if (std::shared_ptr<CMonoMethod> pMethod = m_pMonoObject->GetClass()->FindMethod("Initialize").lock())
		{
			pMethod->Invoke(m_pMonoObject.get());
		}
	}
}

void CManagedPlugin::ScanAssembly()
{
	if (m_pLibrary != nullptr && m_pLibrary->GetAssembly() != nullptr)
	{
		CMonoLibrary* pCoreAssembly = static_cast<CMonoLibrary*>(GetMonoRuntime()->GetCryCoreLibrary());
		std::shared_ptr<CMonoClass> pEngineClass = pCoreAssembly->GetTemporaryClass("CryEngine", "Engine");

		s_pCurrentlyRegisteringFactories = &m_entityComponentFactories;

		// Mark all factories as unregistered. All existing factories will get registered again during ScaneAssembly.
		for (std::shared_ptr<CManagedEntityComponentFactory>& pFactory : *s_pCurrentlyRegisteringFactories)
		{
			pFactory->SetIsRegistered(false);
		}

		//Scan for types in the assembly
		void* pRegisterArgs[1] = { m_pLibrary->GetManagedObject() };
		if (std::shared_ptr<CMonoMethod> pMethod = pEngineClass->FindMethodWithDesc("ScanAssembly(Assembly)").lock())
		{
			pMethod->InvokeStatic(pRegisterArgs);
		}

		// If a factory is not registered again, it means the component was removed.
		for (auto it = s_pCurrentlyRegisteringFactories->begin(); it != s_pCurrentlyRegisteringFactories->end();)
		{
			if (!(*it)->IsRegistered())
			{
				(*it)->RemoveAllComponentInstances();
				it = s_pCurrentlyRegisteringFactories->erase(it);
			}
			else
			{
				s_pCrossPluginRegisteredFactories->emplace_back((*it));
				++it;
			}
		}

		s_pCurrentlyRegisteringFactories = nullptr;

		// Register any potential Schematyc types
		gEnv->pSchematyc->GetEnvRegistry().RegisterPackage(stl::make_unique<CSchematycPackage>(*this));

		// scans for console command attributes
		std::shared_ptr<CMonoClass> attributeManager = GetMonoRuntime()->GetCryCoreLibrary()->GetTemporaryClass("CryEngine.Attributes", "ConsoleCommandAttributeManager");
		CRY_ASSERT(attributeManager != nullptr);
		void* args[1];
		args[0] = m_pLibrary->GetManagedObject(); //load the plug-in assembly to scans for ConsoleCommandRegisterAttribute
		if (std::shared_ptr<CMonoMethod> pMethod = attributeManager->FindMethod("RegisterAttribute", 1).lock())
		{
			pMethod->Invoke(nullptr, args);
		}

		//scans for console variable attributes
		std::shared_ptr<CMonoClass> consoleVariableAttributeManager = GetMonoRuntime()->GetCryCoreLibrary()->GetTemporaryClass("CryEngine.Attributes", "ConsoleVariableAttributeManager");
		CRY_ASSERT(consoleVariableAttributeManager != nullptr);
		void* args2[1];
		args2[0] = m_pLibrary->GetManagedObject(); //load the plug-in assembly to scans for ConsoleVariableAttribute
		if (std::shared_ptr<CMonoMethod> pMethod = consoleVariableAttributeManager->FindMethod("RegisterAttribute", 1).lock())
		{
			pMethod->Invoke(nullptr, args2);
		}
	}
}

void CManagedPlugin::CreatePluginInstance()
{
	if (m_pLibrary != nullptr && m_pLibrary->GetAssembly() != nullptr && m_pMonoObject == nullptr)
	{
		std::shared_ptr<CMonoClass> pReflectionHelper = GetMonoRuntime()->GetCryCoreLibrary()->GetTemporaryClass("CryEngine", "ReflectionHelper");
		CRY_ASSERT(pReflectionHelper != nullptr);

		void* pPluginSearchArgs[1]{ m_pLibrary->GetManagedObject() };

		if (std::shared_ptr<CMonoMethod> pMethod = pReflectionHelper->FindMethodWithDesc("FindPluginInstance(Assembly)").lock())
		{
			std::shared_ptr<CMonoObject> pPluginInstanceName = pMethod->InvokeStatic(pPluginSearchArgs);
			if (pPluginInstanceName != nullptr)
			{
				std::shared_ptr<CMonoString> pPluginClassName = pPluginInstanceName->ToString();
				const char* szPluginClassName = pPluginClassName->GetString();

				m_name = PathUtil::GetExt(szPluginClassName);
				const string nameSpace = PathUtil::RemoveExtension(szPluginClassName);

				CMonoClass* pPluginClass = m_pLibrary->GetClass(nameSpace, m_name);
				if (pPluginClass != nullptr)
				{
					m_pMonoObject = std::static_pointer_cast<CMonoObject>(pPluginClass->CreateInstance());

					gEnv->pGameFramework->AddNetworkedClientListener(*this);
				}
			}
		}
	}
}

void CManagedPlugin::Load(CAppDomain* pPluginDomain)
{
	if (m_pLibrary == nullptr && m_libraryPath.size() > 0)
	{
		m_pLibrary = pPluginDomain->LoadLibrary(m_libraryPath, m_loadIndex);
		if (m_pLibrary == nullptr)
		{
			gEnv->pLog->LogError("[Mono] could not initialize plug-in '%s'!", m_libraryPath.c_str());
			return;
		}
	}

	InitializePlugin();
}

void CManagedPlugin::OnCoreLibrariesDeserialized()
{
	gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(GetGUID());

	CreatePluginInstance();
	ScanAssembly();
}

void CManagedPlugin::OnPluginLibrariesDeserialized()
{
	for (const std::shared_ptr<CManagedEntityComponentFactory>& pFactory : m_entityComponentFactories)
	{
		pFactory->FinalizeComponentRegistration();
	}
}

void CManagedPlugin::RegisterSchematycPackageContents(Schematyc::IEnvRegistrar& registrar) const
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		for (const std::shared_ptr<CManagedEntityComponentFactory>& pFactory : m_entityComponentFactories)
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(pFactory);
			for (const std::shared_ptr<CManagedEntityComponentFactory::CSchematycFunction>& pFunction : pFactory->m_schematycFunctions)
			{
				componentScope.Register(pFunction);
			}
			for (const std::shared_ptr<CManagedEntityComponentFactory::CSchematycSignal>& pSignal : pFactory->m_schematycSignals)
			{
				componentScope.Register(pSignal);
			}
		}
	}
}

void CManagedPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
	{
		if (m_pMonoObject != nullptr)
		{
			if (CMonoClass* pClass = m_pMonoObject->GetClass())
			{
				if (std::shared_ptr<CMonoMethod> pMethod = pClass->FindMethod("OnLevelLoaded").lock())
				{
					pMethod->Invoke(m_pMonoObject.get());
				}
			}
		}
	}
	break;
	case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
	{
		if (wparam == 1)
		{
			if (m_pMonoObject != nullptr)
			{
				if (CMonoClass* pClass = m_pMonoObject->GetClass())
				{
					if (std::shared_ptr<CMonoMethod> pMethod = pClass->FindMethod("OnGameStart").lock())
					{
						pMethod->Invoke(m_pMonoObject.get());
					}
				}
			}
		}
		else if (m_pMonoObject != nullptr)
		{
			if (CMonoClass* pClass = m_pMonoObject->GetClass())
			{
				if (std::shared_ptr<CMonoMethod> pMethod = pClass->FindMethod("OnGameStop").lock())
				{
					pMethod->Invoke(m_pMonoObject.get());
				}
			}
		}
	}
	break;
	case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
	{
		if (m_pMonoObject != nullptr)
		{
			if (CMonoClass* pClass = m_pMonoObject->GetClass())
			{
				if (std::shared_ptr<CMonoMethod> pMethod = pClass->FindMethod("OnGameStart").lock())
				{
					pMethod->Invoke(m_pMonoObject.get());
				}
			}
		}
	}
	break;
	case ESYSTEM_EVENT_LEVEL_UNLOAD:
	{
		if (m_pMonoObject != nullptr)
		{
			if (CMonoClass* pClass = m_pMonoObject->GetClass())
			{
				if (std::shared_ptr<CMonoMethod> pMethod = pClass->FindMethod("OnGameStop").lock())
				{
					pMethod->Invoke(m_pMonoObject.get());
				}
			}
		}
	}
	break;
	}
}

bool CManagedPlugin::OnClientConnectionReceived(int channelId, bool bIsReset)
{
	if (m_pMonoObject != nullptr)
	{
		if (CMonoClass* pClass = m_pMonoObject->GetClass())
		{
			if (std::shared_ptr<CMonoMethod> pMethod = pClass->FindMethod("OnClientConnectionReceived").lock())
			{
				void* pParameters[1];
				pParameters[0] = &channelId;

				pMethod->Invoke(m_pMonoObject.get(), pParameters);
			}
		}
		
	}

	return true;
}

bool CManagedPlugin::OnClientReadyForGameplay(int channelId, bool bIsReset)
{
	if (m_pMonoObject != nullptr)
	{
		if (CMonoClass* pClass = m_pMonoObject->GetClass())
		{
			if (std::shared_ptr<CMonoMethod> pMethod = pClass->FindMethod("OnClientReadyForGameplay").lock())
			{
				void* pParameters[1];
				pParameters[0] = &channelId;

				pMethod->Invoke(m_pMonoObject.get(), pParameters);
			}
		}
	}

	return true;
}

void CManagedPlugin::OnClientDisconnected(int channelId, EDisconnectionCause cause, const char* description, bool bKeepClient)
{
	if (m_pMonoObject != nullptr)
	{
		if (CMonoClass* pClass = m_pMonoObject->GetClass())
		{
			if (std::shared_ptr<CMonoMethod> pMethod = pClass->FindMethod("OnClientDisconnected").lock())
			{
				void* pParameters[1];
				pParameters[0] = &channelId;
				pMethod->Invoke(m_pMonoObject.get(), pParameters);
			}
		}
	}
}
