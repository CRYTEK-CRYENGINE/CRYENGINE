// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Entity.h"
#include "EntitySystem.h"

#include <CrySystem/CryUnitTest.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

#include "SubstitutionProxy.h"

#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

CRY_UNIT_TEST_SUITE(EntityTestsSuit)
{
	struct IUnifiedEntityComponent : public IEntityComponent
	{
		static void ReflectType(Schematyc::CTypeDesc<IUnifiedEntityComponent>& desc)
		{
			desc.SetGUID("{C89DD0DD-9850-43BA-BF52-86EFB7C9D0A5}"_cry_guid);
		}
	};

	class CUnifiedEntityComponent : public IUnifiedEntityComponent
	{
	public:
		static void ReflectType(Schematyc::CTypeDesc<CUnifiedEntityComponent>& desc)
		{
			desc.AddBase<IUnifiedEntityComponent>();
			desc.SetGUID("{C89DD0DD-9850-43BA-BF52-86EFB7C9D0A5}"_cry_guid);
			desc.SetEditorCategory("Unit Tests");
			desc.SetLabel("Unified Entity Component");
			desc.SetDescription("Does stuff");
			desc.SetIcon("icons:ObjectTypes/light.ico");
			desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

			desc.AddMember(&CUnifiedEntityComponent::m_bMyBool, 'bool', "Bool", "Bool", "Bool desc", false);
			desc.AddMember(&CUnifiedEntityComponent::m_myFloat, 'floa', "Float", "Float", "Float desc", 0.f);
		}

		bool m_bMyBool = false;
		float m_myFloat = 0.f;
	};

	IEntity* SpawnTestEntity(const char* szName)
	{
		SEntitySpawnParams params;
		params.guid = CryGUID::Create();
		params.sName = szName;
		params.nFlags = ENTITY_FLAG_CLIENT_ONLY;
		params.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

		IEntity *pEntity = gEnv->pEntitySystem->SpawnEntity(params);
		CRY_UNIT_TEST_ASSERT(pEntity != NULL);
		return pEntity;
	}

	CRY_UNIT_TEST(SpawnTest)
	{
		IEntity *pEntity = SpawnTestEntity("TestEntity");
		EntityId id = pEntity->GetId();

		CRY_UNIT_TEST_ASSERT( pEntity->GetGuid() != CryGUID::Null() );

		CRY_UNIT_TEST_CHECK_EQUAL(id, gEnv->pEntitySystem->FindEntityByGuid(pEntity->GetGuid()));
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity, gEnv->pEntitySystem->FindEntityByName("TestEntity"));

		// Test Entity components
		IEntitySubstitutionComponent *pComponent = pEntity->GetOrCreateComponent<IEntitySubstitutionComponent>();
		CRY_UNIT_TEST_ASSERT( nullptr != pComponent );
		CRY_UNIT_TEST_ASSERT( 1 == pEntity->GetComponentsCount() );
	}

	class CLegacyEntityComponent : public IEntityComponent
	{
	public:
		CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CLegacyEntityComponent, "LegacyEntityComponent", "b1febcee-be12-46a9-b69a-cc66d6c0d3e0"_cry_guid);
	};

	CRYREGISTER_CLASS(CLegacyEntityComponent);

	CRY_UNIT_TEST(CreateLegacyComponent)
	{
		IEntity *pEntity = SpawnTestEntity("LegacyComponentTestEntity");

		CLegacyEntityComponent *pComponent = pEntity->GetOrCreateComponent<CLegacyEntityComponent>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponentsCount(), 1);

		pEntity->RemoveComponent(pComponent);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponentsCount(), 0);
	}

	struct ILegacyComponentInterface : public IEntityComponent
	{
		CRY_ENTITY_COMPONENT_INTERFACE_GUID(ILegacyComponentInterface, "d9a71012-8af1-4426-8835-99c4f4eb987d"_cry_guid)

		virtual bool IsValid() const { return false; }
	};

	class CLegacyEntityComponentWithInterface final : public ILegacyComponentInterface
	{
	public:
		CRY_ENTITY_COMPONENT_CLASS_GUID(CLegacyEntityComponentWithInterface, ILegacyComponentInterface, "LegacyEntityComponentWithInterface", "34ee1b92-2154-4bad-b69a-cc66d6c0d3e0"_cry_guid);

		virtual bool IsValid() const final { return true; }
	};

	CRYREGISTER_CLASS(CLegacyEntityComponentWithInterface);

	CRY_UNIT_TEST(CreateLegacyComponentWithInterface)
	{
		IEntity *pEntity = SpawnTestEntity("LegacyComponentTestInterfaceEntity");

		ILegacyComponentInterface *pComponent = pEntity->GetOrCreateComponent<ILegacyComponentInterface>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponentsCount(), 1);
		CRY_UNIT_TEST_ASSERT(pComponent->IsValid());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, pEntity->GetComponent<CLegacyEntityComponentWithInterface>());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, pEntity->GetComponent<ILegacyComponentInterface>());

		pEntity->RemoveComponent(pComponent);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponentsCount(), 0);

		pComponent = pEntity->GetOrCreateComponent<CLegacyEntityComponentWithInterface>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponentsCount(), 1);
		CRY_UNIT_TEST_ASSERT(pComponent->IsValid());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, pEntity->GetComponent<CLegacyEntityComponentWithInterface>());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, pEntity->GetComponent<ILegacyComponentInterface>());

		DynArray<CLegacyEntityComponentWithInterface*> components;
		pEntity->GetAllComponents<CLegacyEntityComponentWithInterface>(components);
		CRY_UNIT_TEST_CHECK_EQUAL(components.size(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(components.at(0), static_cast<CLegacyEntityComponentWithInterface*>(pComponent));

		DynArray<ILegacyComponentInterface*> componentsbyInterface;
		pEntity->GetAllComponents<ILegacyComponentInterface>(componentsbyInterface);
		CRY_UNIT_TEST_CHECK_EQUAL(componentsbyInterface.size(),  1);
		CRY_UNIT_TEST_CHECK_EQUAL(componentsbyInterface.at(0), static_cast<CLegacyEntityComponentWithInterface*>(pComponent));
	}

	CRY_UNIT_TEST(CreateUnifiedComponent)
	{
		IEntity *pEntity = SpawnTestEntity("UnifiedComponentTestEntity");

		CUnifiedEntityComponent *pComponent = pEntity->GetOrCreateComponent<CUnifiedEntityComponent>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponentsCount(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_bMyBool, false);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_myFloat, 0.f);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, pEntity->GetComponent<CUnifiedEntityComponent>());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, pEntity->GetComponent<IUnifiedEntityComponent>());

		pEntity->RemoveComponent(pComponent);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponentsCount(), 0);

		pComponent = static_cast<CUnifiedEntityComponent*>(pEntity->GetOrCreateComponent<IUnifiedEntityComponent>());
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponentsCount(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_bMyBool, false);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_myFloat, 0.f);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, pEntity->GetComponent<CUnifiedEntityComponent>());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, pEntity->GetComponent<IUnifiedEntityComponent>());

		DynArray<CUnifiedEntityComponent*> components;
		pEntity->GetAllComponents<CUnifiedEntityComponent>(components);
		CRY_UNIT_TEST_CHECK_EQUAL(components.size(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(components.at(0), static_cast<CUnifiedEntityComponent*>(pComponent));

		DynArray<IUnifiedEntityComponent*> componentsbyInterface;
		pEntity->GetAllComponents<IUnifiedEntityComponent>(componentsbyInterface);
		CRY_UNIT_TEST_CHECK_EQUAL(componentsbyInterface.size(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(componentsbyInterface.at(0), static_cast<CUnifiedEntityComponent*>(pComponent));
	}

	CRY_UNIT_TEST(UnifiedComponentSerialization)
	{
		XmlNodeRef node = gEnv->pSystem->CreateXmlNode();
		CryGUID instanceGUID;

		{
			IEntity *pEntity = SpawnTestEntity("UnifiedComponentSerializationTestEntity");

			CUnifiedEntityComponent *pComponent = pEntity->GetOrCreateComponent<CUnifiedEntityComponent>();
			CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
			// Check default values
			CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_bMyBool, false);
			CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_myFloat, 0.f);

			pComponent->m_bMyBool = true;
			pComponent->m_myFloat = 1337.f;

			instanceGUID = pComponent->GetGUID();

			// Save to XML
			pEntity->SerializeXML(node, false);
		}

		// Create another entity for deserialization
		IEntity *pDeserializedEntity = SpawnTestEntity("UnifiedComponentDeserializationTestEntity");
		// Deserialize
		pDeserializedEntity->SerializeXML(node, true);

		CUnifiedEntityComponent *pComponent = pDeserializedEntity->GetComponent<CUnifiedEntityComponent>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		// Check deserialized values
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_bMyBool, true);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_myFloat, 1337.f);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->GetGUID(), instanceGUID);
	}

	CRY_UNIT_TEST(QueryInvalidGUID)
	{
		IEntity *pEntity = SpawnTestEntity("UnifiedComponentTestEntity");
		CUnifiedEntityComponent *pComponent = pEntity->GetOrCreateComponent<CUnifiedEntityComponent>();
		CLegacyEntityComponentWithInterface* pLegacyComponent = pEntity->GetOrCreateComponent<CLegacyEntityComponentWithInterface>();
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponentsCount(), 2);
		// Querying the lowest level GUIDs is disallowed
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetComponent<IEntityComponent>(), nullptr);
	}

	class CComponent2 : public IEntityComponent
	{
	public:
		static void ReflectType(Schematyc::CTypeDesc<CComponent2>& desc)
		{
			desc.SetGUID("{8B80B6EA-3A85-48F0-89DC-EDE69E72BC08}"_cry_guid);
			desc.SetLabel("CComponent2");
		}

		virtual void Initialize() override
		{
			m_bInitialized = true;
		}

		bool m_bInitialized = false;
	};

	class CComponent1 : public IEntityComponent
	{
	public:
		static void ReflectType(Schematyc::CTypeDesc<CComponent1>& desc)
		{
			desc.SetGUID("{00235E09-55EC-46AD-A6C7-606EA4A40A6B}"_cry_guid);
			desc.SetLabel("CComponent1");
			desc.AddMember(&CComponent1::m_bLoadingFromDisk, 'load', "Loading", "Loading", "", false);
		}

		virtual void Initialize() override
		{
			if (m_bLoadingFromDisk)
			{
				// Make sure that CComponent2 is already available (but uninitialized)
				CComponent2* pOtherComponent = m_pEntity->GetComponent<CComponent2>();
				CRY_UNIT_TEST_CHECK_DIFFERENT(pOtherComponent, nullptr);
				if (pOtherComponent != nullptr)
				{
					CRY_UNIT_TEST_CHECK_EQUAL(pOtherComponent->m_bInitialized, false);
				}
			}
		}

		bool m_bLoadingFromDisk = false;
	};

	// Test whether two components will be deserialized into CEntity::m_components before being Initialized
	// Initialize must never be called during loading from disk if another component (in the same entity) has yet to be loaded
	CRY_UNIT_TEST(LoadMultipleComponents)
	{
		IEntity *pEntity = SpawnTestEntity("SaveMultipleComponents");
		CComponent1 *pComponent1 = pEntity->GetOrCreateComponent<CComponent1>();
		CComponent2 *pComponent2 = pEntity->GetOrCreateComponent<CComponent2>();

		// Set boolean to true so we can assert existence of CComponent2 in CComponent1::Initialize
		pComponent1->m_bLoadingFromDisk = true;
		
		// Save
		XmlNodeRef xmlNode = gEnv->pSystem->CreateXmlNode("Entity");
		pEntity->SerializeXML(xmlNode, false);
		gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());

		// Load
		pEntity = SpawnTestEntity("LoadMultipleComponents");
		pEntity->SerializeXML(xmlNode, true);

		CRY_UNIT_TEST_CHECK_DIFFERENT(pEntity->GetComponent<CComponent1>(), nullptr);
		CRY_UNIT_TEST_CHECK_DIFFERENT(pEntity->GetComponent<CComponent2>(), nullptr);
	}
}

void RegisterUnitTestComponents(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(EntityTestsSuit::CUnifiedEntityComponent));
		scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(EntityTestsSuit::CComponent1));
		scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(EntityTestsSuit::CComponent2));
	}
}