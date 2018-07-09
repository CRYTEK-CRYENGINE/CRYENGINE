// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Entity.h"

#include "MonoRuntime.h"
#include "ManagedPlugin.h"

#include "NativeComponents/ManagedEntityComponent.h"

#include "Wrappers/MonoDomain.h"
#include "Wrappers/MonoClass.h"

#include <CrySchematyc/CoreAPI.h>

static void RegisterComponent(MonoInternals::MonoReflectionType* pType, uint64 guidHipart, uint64 guidLopart, MonoInternals::MonoString* pName, MonoInternals::MonoString* pCategory, MonoInternals::MonoString* pDescription, MonoInternals::MonoString* pIcon)
{
	MonoInternals::MonoClass* pMonoClass = MonoInternals::mono_type_get_class(MonoInternals::mono_reflection_type_get_type(pType));
	MonoInternals::MonoImage* pImage = MonoInternals::mono_class_get_image(pMonoClass);

	CryGUID id = CryGUID::Construct(guidHipart, guidLopart);
	Schematyc::SSourceFileInfo info("Unknown .NET source file", 0);

	// Find source file and line number
	// We can't get this for the class itself, but cheat and use the first exposed method
	void* iter = nullptr;
	if (MonoInternals::MonoMethod* pMethod = mono_class_get_methods(pMonoClass, &iter))
	{
		if (MonoInternals::MonoDebugMethodJitInfo* pMethodDebugInfo = mono_debug_find_method(pMethod, MonoInternals::mono_domain_get()))
		{
			// Hack since this isn't exposed to the Mono API
			struct InternalMonoDebugLineNumberEntry 
			{
				uint32_t il_offset;
				uint32_t native_offset;
			};

			if (MonoInternals::MonoDebugSourceLocation* pSourceLocation = mono_debug_lookup_source_location(pMethod,
				((InternalMonoDebugLineNumberEntry*)(void*)pMethodDebugInfo->line_numbers)[0].native_offset,
				MonoInternals::mono_domain_get()))
			{
				info.szFileName = pSourceLocation->source_file;
				info.lineNumber = pSourceLocation->row;
			}
		}
	}

	CMonoDomain* pDomain = GetMonoRuntime()->FindDomainByHandle(MonoInternals::mono_object_get_domain((MonoInternals::MonoObject*)pType));
	CMonoLibrary& library = pDomain->GetLibraryFromMonoAssembly(MonoInternals::mono_image_get_assembly(pImage), pImage);

	std::shared_ptr<CMonoString> pClassName = pDomain->CreateString(pName);
	std::shared_ptr<CMonoString> pClassCategory = pDomain->CreateString(pCategory);
	std::shared_ptr<CMonoString> pDesc = pDomain->CreateString(pCategory);
	std::shared_ptr<CMonoString> pClassIcon = pDomain->CreateString(pIcon);

	for (std::shared_ptr<CManagedEntityComponentFactory>& pFactory : *CManagedPlugin::s_pCurrentlyRegisteringFactories)
	{
		if (pFactory->GetGUID() == id)
		{
			// Refresh the mono class, since it has changed due after deserializing
			pFactory->OnClassDeserialized(pMonoClass, info, pClassName->GetString(), pClassCategory->GetString(), pDesc->GetString(), pClassIcon->GetString());
			return;
		}
	}

	// New factory, emplace
	CManagedPlugin::s_pCurrentlyRegisteringFactories->emplace_back(std::make_shared<CManagedEntityComponentFactory>(library.GetClassFromMonoClass(pMonoClass), id, info, pClassName->GetString(), pClassCategory->GetString(), pDesc->GetString(), pClassIcon->GetString()));
}

static void AddComponentBase(MonoInternals::MonoReflectionType* pType, MonoInternals::MonoReflectionType* pBaseType)
{
	MonoInternals::MonoClass* pMonoClass = MonoInternals::mono_type_get_class(MonoInternals::mono_reflection_type_get_type(pType));
	MonoInternals::MonoClass* pBaseMonoClass = MonoInternals::mono_type_get_class(MonoInternals::mono_reflection_type_get_type(pBaseType));

	std::shared_ptr<CManagedEntityComponentFactory> pFactory;
	std::shared_ptr<CManagedEntityComponentFactory> pBaseFactory;

	for (std::shared_ptr<CManagedEntityComponentFactory>& pRegisteredFactory : *CManagedPlugin::s_pCurrentlyRegisteringFactories)
	{
		if (pRegisteredFactory->GetClass()->GetMonoClass() == pMonoClass)
		{
			pFactory = pRegisteredFactory;
		}
		else if (pRegisteredFactory->GetClass()->GetMonoClass() == pBaseMonoClass)
		{
			pBaseFactory = pRegisteredFactory;
		}
	}

	// If the component or base component was not found, it might be in another plugin.
	if (pFactory == nullptr || pBaseFactory == nullptr)
	{
		for (std::shared_ptr<CManagedEntityComponentFactory>& pRegisteredFactory : *CManagedPlugin::s_pCrossPluginRegisteredFactories)
		{
			if (!pFactory && pRegisteredFactory->GetClass()->GetMonoClass() == pMonoClass)
			{
				pFactory = pRegisteredFactory;
			}
			else if (!pBaseFactory && pRegisteredFactory->GetClass()->GetMonoClass() == pBaseMonoClass)
			{
				pBaseFactory = pRegisteredFactory;
			}
		}
	}

	if (pFactory == nullptr)
	{
		CRY_ASSERT(false);
		gEnv->pLog->LogWarning("Tried to add component base before component itself was registered!");
		return;
	}

	if (pBaseFactory == nullptr)
	{
		CRY_ASSERT(false);
		gEnv->pLog->LogWarning("Tried to add component base before base was registered!");
		return;
	}

	if (pFactory->m_classDescription.FindBaseByTypeID(pBaseFactory->GetDesc().GetGUID()) == nullptr)
	{
		pFactory->m_classDescription.AddBase(pBaseFactory->GetDesc());
	}
}

static IEntityComponent* CreateManagedComponent(IEntity *pEntity, SEntitySpawnParams& params, void* pUserData)
{
	return pEntity->CreateComponentByInterfaceID(*(CryGUID*)pUserData);
}

static void RegisterManagedEntityWithDefaultComponent(MonoInternals::MonoString* pName, MonoInternals::MonoString* pEditorCategory, MonoInternals::MonoString* pEditorHelper, MonoInternals::MonoString* pEditorIcon, bool bHide, MonoInternals::MonoReflectionType* pComponentType)
{
	MonoInternals::MonoClass* pMonoClass = MonoInternals::mono_type_get_class(MonoInternals::mono_reflection_type_get_type(pComponentType));
	std::shared_ptr<CManagedEntityComponentFactory> pFactory;

	for (std::shared_ptr<CManagedEntityComponentFactory>& pRegisteredFactory : *CManagedPlugin::s_pCurrentlyRegisteringFactories)
	{
		if (pRegisteredFactory->GetClass()->GetMonoClass() == pMonoClass)
		{
			pFactory = pRegisteredFactory;
			break;
		}
	}

	CRY_ASSERT(pFactory != nullptr);

	std::shared_ptr<CMonoString> pClassName = CMonoDomain::CreateString(pName);
	std::shared_ptr<CMonoString> pCategory = CMonoDomain::CreateString(pEditorCategory);
	std::shared_ptr<CMonoString> pHelper = CMonoDomain::CreateString(pEditorHelper);
	std::shared_ptr<CMonoString> pIcon = CMonoDomain::CreateString(pEditorIcon);

	IEntityClassRegistry::SEntityClassDesc clsDesc;
	clsDesc.sName = pClassName->GetString();

	clsDesc.editorClassInfo.sCategory = pCategory->GetString();
	clsDesc.editorClassInfo.sHelper = pHelper->GetString();
	clsDesc.editorClassInfo.sIcon = pIcon->GetString();

	clsDesc.pUserProxyCreateFunc = &CreateManagedComponent;
	clsDesc.pUserProxyData = (void*)&pFactory->m_classDescription.GetGUID();

	gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(clsDesc);
}

static MonoInternals::MonoObject* GetComponent(IEntity* pEntity, uint64 guidHipart, uint64 guidLopart)
{
	if (auto* pComponent = static_cast<CManagedEntityComponent*>(pEntity->GetComponentByTypeId(CryGUID::Construct(guidHipart, guidLopart))))
	{
		return pComponent->GetObject()->GetManagedObject();
	}

	// If the implementation wasn't found, automatically search for a base class / interface instead.
	if (auto* pComponent = static_cast<CManagedEntityComponent*>(pEntity->QueryComponentByInterfaceID(CryGUID::Construct(guidHipart, guidLopart))))
	{
		return pComponent->GetObject()->GetManagedObject();
	}

	return nullptr;
}

static void GetComponents(IEntity* pEntity, uint64 guidHipart, uint64 guidLopart, MonoInternals::MonoObject** pArrayOut)
{
	DynArray<IEntityComponent*> foundComponents;

	// Get the specific components first
	pEntity->GetComponentsByTypeId(CryGUID::Construct(guidHipart, guidLopart), foundComponents);
	
	// Search for base components next
	pEntity->QueryComponentsByInterfaceID(CryGUID::Construct(guidHipart, guidLopart), foundComponents);
	
	// Remove duplicate components
	foundComponents.erase(std::unique(foundComponents.begin(), foundComponents.end()), foundComponents.end());

	if (foundComponents.size() == 0)
	{
		pArrayOut = nullptr;
		return;
	}

	CMonoClass* pEntityComponentClass = GetMonoRuntime()->GetCryCoreLibrary()->GetClass("CryEngine", "EntityComponent");

	MonoInternals::MonoArray* pArray = MonoInternals::mono_array_new(GetMonoRuntime()->GetActiveDomain()->GetHandle(), pEntityComponentClass->GetMonoClass(), foundComponents.size());
	for(int i = 0; i < foundComponents.size(); ++i)
	{
		*(MonoInternals::MonoObject **)mono_array_addr((pArray), MonoInternals::MonoObject*, i) = static_cast<CManagedEntityComponent*>(foundComponents[i])->GetObject()->GetManagedObject();
	}

	*pArrayOut = (MonoInternals::MonoObject*)pArray;
}

static MonoInternals::MonoObject* AddComponent(IEntity* pEntity, uint64 guidHipart, uint64 guidLopart)
{
	CManagedEntityComponent* pComponent = static_cast<CManagedEntityComponent*>(pEntity->CreateComponentByInterfaceID(CryGUID::Construct(guidHipart, guidLopart)));

	if (pComponent != nullptr)
	{
		return pComponent->GetObject()->GetManagedObject();
	}

	return nullptr;
}

static MonoInternals::MonoObject* GetOrCreateComponent(IEntity* pEntity, uint64 guidHipart, uint64 guidLopart)
{
	CryGUID id = CryGUID::Construct(guidHipart, guidLopart);

	CManagedEntityComponent* pComponent = static_cast<CManagedEntityComponent*>(pEntity->GetComponentByTypeId(id));
	if (pComponent == nullptr)
	{
		pComponent = static_cast<CManagedEntityComponent*>(pEntity->CreateComponentByInterfaceID(id, false));
	}

	return pComponent->GetObject()->GetManagedObject();
}

static void RegisterComponentProperty(MonoInternals::MonoReflectionType* pComponentType, MonoInternals::MonoReflectionProperty* pProperty, MonoInternals::MonoString* pPropertyName, MonoInternals::MonoString* pPropertyLabel, MonoInternals::MonoString* pPropertyDescription, EEntityPropertyType type, MonoInternals::MonoObject* pDefaultValue)
{
	MonoInternals::MonoClass* pMonoClass = MonoInternals::mono_type_get_class(MonoInternals::mono_reflection_type_get_type(pComponentType));
	std::shared_ptr<CManagedEntityComponentFactory> pFactory;
	for (std::shared_ptr<CManagedEntityComponentFactory>& pRegisteredFactory : *CManagedPlugin::s_pCurrentlyRegisteringFactories)
	{
		if (pRegisteredFactory->GetClass()->GetMonoClass() == pMonoClass)
		{
			pFactory = pRegisteredFactory;
			break;
		}
	}

	CRY_ASSERT(pFactory != nullptr);

	std::shared_ptr<CMonoString> pName = CMonoDomain::CreateString(pPropertyName);
	std::shared_ptr<CMonoString> pLabel = CMonoDomain::CreateString(pPropertyLabel);
	std::shared_ptr<CMonoString> pDescription = CMonoDomain::CreateString(pPropertyDescription);

	pFactory->AddProperty(pProperty, pName->GetString(), pLabel->GetString(), pDescription->GetString(), type, pDefaultValue);
}

static void RegisterComponentFunction(MonoInternals::MonoReflectionType* pComponentType, MonoInternals::MonoReflectionMethod* pMethod)
{
	MonoInternals::MonoClass* pMonoClass = MonoInternals::mono_type_get_class(MonoInternals::mono_reflection_type_get_type(pComponentType));
	std::shared_ptr<CManagedEntityComponentFactory> pFactory;
	for (std::shared_ptr<CManagedEntityComponentFactory>& pRegisteredFactory : *CManagedPlugin::s_pCurrentlyRegisteringFactories)
	{
		if (pRegisteredFactory->GetClass()->GetMonoClass() == pMonoClass)
		{
			pFactory = pRegisteredFactory;
			break;
		}
	}

	CRY_ASSERT(pFactory != nullptr);
	pFactory->AddFunction(pMethod);
}

static int RegisterComponentSignal(MonoInternals::MonoReflectionType* pComponentType, MonoInternals::MonoString* pSignalName)
{
	MonoInternals::MonoClass* pMonoClass = MonoInternals::mono_type_get_class(MonoInternals::mono_reflection_type_get_type(pComponentType));
	std::shared_ptr<CManagedEntityComponentFactory> pFactory;
	for (std::shared_ptr<CManagedEntityComponentFactory>& pRegisteredFactory : *CManagedPlugin::s_pCurrentlyRegisteringFactories)
	{
		if (pRegisteredFactory->GetClass()->GetMonoClass() == pMonoClass)
		{
			pFactory = pRegisteredFactory;
			break;
		}
	}

	CRY_ASSERT(pFactory != nullptr);

	char* szSignalName = MonoInternals::mono_string_to_utf8(pSignalName);
	int signalId = pFactory->AddSignal(szSignalName);
	MonoInternals::mono_free(szSignalName);

	return signalId;
}

static void AddComponentSignalParameter(MonoInternals::MonoReflectionType* pComponentType, int signalId, MonoInternals::MonoString* pParamName, MonoInternals::MonoReflectionType* pParamType)
{
	MonoInternals::MonoClass* pMonoClass = MonoInternals::mono_type_get_class(MonoInternals::mono_reflection_type_get_type(pComponentType));
	std::shared_ptr<CManagedEntityComponentFactory> pFactory;
	for (std::shared_ptr<CManagedEntityComponentFactory>& pRegisteredFactory : *CManagedPlugin::s_pCurrentlyRegisteringFactories)
	{
		if (pRegisteredFactory->GetClass()->GetMonoClass() == pMonoClass)
		{
			pFactory = pRegisteredFactory;
			break;
		}
	}

	CRY_ASSERT(pFactory != nullptr);

	char* szParamName = MonoInternals::mono_string_to_utf8(pParamName);
	pFactory->AddSignalParameter(signalId, szParamName, pParamType);
	MonoInternals::mono_free(szParamName);
}

static void SendComponentSignal(IEntity* pEntity, uint64 guidHipart, uint64 guidLopart, int signalId, MonoInternals::MonoArray* pParameters)
{
	CryGUID id = CryGUID::Construct(guidHipart, guidLopart);
	if (CManagedEntityComponent* pComponent = static_cast<CManagedEntityComponent*>(pEntity->GetComponentByTypeId(id)))
	{
		pComponent->SendSignal(signalId, pParameters);
	}
}

void CManagedEntityInterface::RegisterFunctions(std::function<void(const void* pMethod, const char* methodName)> func)
{
	func(RegisterComponent, "RegisterComponent");
	func(AddComponentBase, "AddComponentBase");
	func(RegisterManagedEntityWithDefaultComponent, "RegisterEntityWithDefaultComponent");
	func(GetComponent, "GetComponent");
	func(GetComponents, "GetComponents");
	func(GetOrCreateComponent, "GetOrCreateComponent");
	func(AddComponent, "AddComponent");
	func(RegisterComponentProperty, "RegisterComponentProperty");
	func(RegisterComponentFunction, "RegisterComponentFunction");
	func(RegisterComponentSignal, "RegisterComponentSignal");
	func(AddComponentSignalParameter, "AddComponentSignalParameter");
	func(SendComponentSignal, "SendComponentSignal");
}