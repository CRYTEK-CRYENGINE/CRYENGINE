// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

		bool  m_bMyBool = false;
		float m_myFloat = 0.f;
	};

	struct SScopedSpawnEntity
	{
		SScopedSpawnEntity(const char* szName)
		{
			SEntitySpawnParams params;
			params.guid = CryGUID::Create();
			params.sName = szName;
			params.nFlags = ENTITY_FLAG_CLIENT_ONLY;

			pEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(params));
			CRY_UNIT_TEST_ASSERT(pEntity != nullptr);
		}

		~SScopedSpawnEntity()
		{
			g_pIEntitySystem->RemoveEntity(pEntity->GetId());
		}

		CEntity* pEntity;
	};

	CRY_UNIT_TEST(SpawnTest)
	{
		SScopedSpawnEntity entity("TestEntity");
		EntityId id = entity.pEntity->GetId();

		CRY_UNIT_TEST_ASSERT(entity.pEntity->GetGuid() != CryGUID::Null());

		CRY_UNIT_TEST_CHECK_EQUAL(id, g_pIEntitySystem->FindEntityByGuid(entity.pEntity->GetGuid()));
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity, g_pIEntitySystem->FindEntityByName("TestEntity"));

		// Test Entity components
		IEntitySubstitutionComponent* pComponent = entity.pEntity->GetOrCreateComponent<IEntitySubstitutionComponent>();
		CRY_UNIT_TEST_ASSERT(nullptr != pComponent);
		CRY_UNIT_TEST_ASSERT(1 == entity.pEntity->GetComponentsCount());
	}

	class CLegacyEntityComponent : public IEntityComponent
	{
	public:
		CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CLegacyEntityComponent, "LegacyEntityComponent", "b1febcee-be12-46a9-b69a-cc66d6c0d3e0"_cry_guid);
	};

	CRYREGISTER_CLASS(CLegacyEntityComponent);

	CRY_UNIT_TEST(CreateLegacyComponent)
	{
		SScopedSpawnEntity entity("LegacyComponentTestEntity");

		CLegacyEntityComponent* pComponent = entity.pEntity->GetOrCreateComponent<CLegacyEntityComponent>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponentsCount(), 1);

		entity.pEntity->RemoveComponent(pComponent);
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponentsCount(), 0);
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
		SScopedSpawnEntity entity("LegacyComponentTestInterfaceEntity");

		ILegacyComponentInterface* pComponent = entity.pEntity->GetOrCreateComponent<ILegacyComponentInterface>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponentsCount(), 1);
		CRY_UNIT_TEST_ASSERT(pComponent->IsValid());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, entity.pEntity->GetComponent<CLegacyEntityComponentWithInterface>());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, entity.pEntity->GetComponent<ILegacyComponentInterface>());

		entity.pEntity->RemoveComponent(pComponent);
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponentsCount(), 0);

		pComponent = entity.pEntity->GetOrCreateComponent<CLegacyEntityComponentWithInterface>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponentsCount(), 1);
		CRY_UNIT_TEST_ASSERT(pComponent->IsValid());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, entity.pEntity->GetComponent<CLegacyEntityComponentWithInterface>());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, entity.pEntity->GetComponent<ILegacyComponentInterface>());

		DynArray<CLegacyEntityComponentWithInterface*> components;
		entity.pEntity->GetAllComponents<CLegacyEntityComponentWithInterface>(components);
		CRY_UNIT_TEST_CHECK_EQUAL(components.size(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(components.at(0), static_cast<CLegacyEntityComponentWithInterface*>(pComponent));

		DynArray<ILegacyComponentInterface*> componentsbyInterface;
		entity.pEntity->GetAllComponents<ILegacyComponentInterface>(componentsbyInterface);
		CRY_UNIT_TEST_CHECK_EQUAL(componentsbyInterface.size(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(componentsbyInterface.at(0), static_cast<CLegacyEntityComponentWithInterface*>(pComponent));
	}

	CRY_UNIT_TEST(CreateUnifiedComponent)
	{
		SScopedSpawnEntity entity("UnifiedComponentTestEntity");

		CUnifiedEntityComponent* pComponent = entity.pEntity->GetOrCreateComponent<CUnifiedEntityComponent>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponentsCount(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_bMyBool, false);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_myFloat, 0.f);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, entity.pEntity->GetComponent<CUnifiedEntityComponent>());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, entity.pEntity->GetComponent<IUnifiedEntityComponent>());

		entity.pEntity->RemoveComponent(pComponent);
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponentsCount(), 0);

		pComponent = static_cast<CUnifiedEntityComponent*>(entity.pEntity->GetOrCreateComponent<IUnifiedEntityComponent>());
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponentsCount(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_bMyBool, false);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_myFloat, 0.f);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, entity.pEntity->GetComponent<CUnifiedEntityComponent>());
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent, entity.pEntity->GetComponent<IUnifiedEntityComponent>());

		DynArray<CUnifiedEntityComponent*> components;
		entity.pEntity->GetAllComponents<CUnifiedEntityComponent>(components);
		CRY_UNIT_TEST_CHECK_EQUAL(components.size(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(components.at(0), static_cast<CUnifiedEntityComponent*>(pComponent));

		DynArray<IUnifiedEntityComponent*> componentsbyInterface;
		entity.pEntity->GetAllComponents<IUnifiedEntityComponent>(componentsbyInterface);
		CRY_UNIT_TEST_CHECK_EQUAL(componentsbyInterface.size(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(componentsbyInterface.at(0), static_cast<CUnifiedEntityComponent*>(pComponent));
	}

	CRY_UNIT_TEST(UnifiedComponentSerialization)
	{
		XmlNodeRef node = gEnv->pSystem->CreateXmlNode();
		CryGUID instanceGUID;

		{
			SScopedSpawnEntity entity("UnifiedComponentSerializationTestEntity");

			CUnifiedEntityComponent* pComponent = entity.pEntity->GetOrCreateComponent<CUnifiedEntityComponent>();
			CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
			// Check default values
			CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_bMyBool, false);
			CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_myFloat, 0.f);

			pComponent->m_bMyBool = true;
			pComponent->m_myFloat = 1337.f;

			instanceGUID = pComponent->GetGUID();

			// Save to XML
			entity.pEntity->SerializeXML(node, false);
		}

		// Create another entity for deserialization
		SScopedSpawnEntity entity("UnifiedComponentDeserializationTestEntity");
		// Deserialize
		entity.pEntity->SerializeXML(node, true);

		CUnifiedEntityComponent* pComponent = entity.pEntity->GetComponent<CUnifiedEntityComponent>();
		CRY_UNIT_TEST_CHECK_DIFFERENT(pComponent, nullptr);
		// Check deserialized values
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_bMyBool, true);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_myFloat, 1337.f);
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->GetGUID(), instanceGUID);
	}

	CRY_UNIT_TEST(QueryInvalidGUID)
	{
		SScopedSpawnEntity entity("UnifiedComponentTestEntity");
		CUnifiedEntityComponent* pComponent = entity.pEntity->GetOrCreateComponent<CUnifiedEntityComponent>();
		CLegacyEntityComponentWithInterface* pLegacyComponent = entity.pEntity->GetOrCreateComponent<CLegacyEntityComponentWithInterface>();
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponentsCount(), 2);
		// Querying the lowest level GUIDs is disallowed
		CRY_UNIT_TEST_CHECK_EQUAL(entity.pEntity->GetComponent<IEntityComponent>(), nullptr);
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
		XmlNodeRef xmlNode;

		{
			SScopedSpawnEntity entity("SaveMultipleComponents");
			CComponent1* pComponent1 = entity.pEntity->GetOrCreateComponent<CComponent1>();
			CComponent2* pComponent2 = entity.pEntity->GetOrCreateComponent<CComponent2>();

			// Set boolean to true so we can assert existence of CComponent2 in CComponent1::Initialize
			pComponent1->m_bLoadingFromDisk = true;

			// Save
			xmlNode = gEnv->pSystem->CreateXmlNode("Entity");
			entity.pEntity->SerializeXML(xmlNode, false);
		}

		// Load
		SScopedSpawnEntity entity("LoadMultipleComponents");
		entity.pEntity->SerializeXML(xmlNode, true);

		CRY_UNIT_TEST_CHECK_DIFFERENT(entity.pEntity->GetComponent<CComponent1>(), nullptr);
		CRY_UNIT_TEST_CHECK_DIFFERENT(entity.pEntity->GetComponent<CComponent2>(), nullptr);
	}

	CRY_UNIT_TEST(TestComponentEvent)
	{
		class CComponentWithEvent : public IEntityComponent
		{
		public:
			virtual void ProcessEvent(const SEntityEvent& event) override
			{
				CRY_UNIT_TEST_CHECK_EQUAL(event.event, ENTITY_EVENT_PHYSICS_CHANGE_STATE);
				m_wasHit = true;
			}

			virtual uint64 GetEventMask() const override { return ENTITY_EVENT_BIT(ENTITY_EVENT_PHYSICS_CHANGE_STATE); }

			bool m_wasHit = false;
		};

		class CComponentWithoutEvent : public IEntityComponent
		{
		public:
			virtual void ProcessEvent(const SEntityEvent& event) override
			{
				CRY_UNIT_TEST_ASSERT(false);
			}
		};

		SScopedSpawnEntity entity("TestComponentEvent");
		CComponentWithEvent* pComponent = entity.pEntity->CreateComponentClass<CComponentWithEvent>();
		entity.pEntity->CreateComponentClass<CComponentWithoutEvent>();
		pComponent->m_wasHit = false;
		pComponent->SendEvent(SEntityEvent(ENTITY_EVENT_PHYSICS_CHANGE_STATE));
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_wasHit, true);

		pComponent->m_wasHit = false;
		entity.pEntity->SendEvent(SEntityEvent(ENTITY_EVENT_PHYSICS_CHANGE_STATE));
		CRY_UNIT_TEST_CHECK_EQUAL(pComponent->m_wasHit, true);

		// Ensure that we can send the events without any issues
		entity.pEntity->SendEvent(SEntityEvent(ENTITY_EVENT_PHYSICS_CHANGE_STATE));
	}

	CRY_UNIT_TEST(TestNestedComponentEvent)
	{
		class CComponentWithEvent : public IEntityComponent
		{
		public:
			virtual void ProcessEvent(const SEntityEvent& event) override
			{
				CRY_UNIT_TEST_ASSERT(event.event == ENTITY_EVENT_PHYSICS_CHANGE_STATE || event.event == ENTITY_EVENT_SCRIPT_EVENT);

				m_receivedEvents++;

				if (event.event == ENTITY_EVENT_PHYSICS_CHANGE_STATE)
				{
					CRY_UNIT_TEST_CHECK_EQUAL(m_receivedEvents, 1);
					m_pEntity->SendEvent(SEntityEvent(ENTITY_EVENT_SCRIPT_EVENT));
					CRY_UNIT_TEST_CHECK_EQUAL(m_receivedEvents, 2);
				}
				else if (event.event == ENTITY_EVENT_SCRIPT_EVENT)
				{
					CRY_UNIT_TEST_CHECK_EQUAL(m_receivedEvents, 2);
				}
			}

			virtual uint64 GetEventMask() const override { return ENTITY_EVENT_BIT(ENTITY_EVENT_PHYSICS_CHANGE_STATE) | ENTITY_EVENT_BIT(ENTITY_EVENT_SCRIPT_EVENT); }

			uint8 m_receivedEvents = 0;
		};

		SScopedSpawnEntity entity("TestComponentEvent");
		CComponentWithEvent* pComponent = entity.pEntity->CreateComponentClass<CComponentWithEvent>();
		entity.pEntity->SendEvent(SEntityEvent(ENTITY_EVENT_PHYSICS_CHANGE_STATE));
		entity.pEntity->RemoveComponent(pComponent);

		// Ensure that we can send the events without any issues
		entity.pEntity->SendEvent(SEntityEvent(ENTITY_EVENT_PHYSICS_CHANGE_STATE));
		entity.pEntity->SendEvent(SEntityEvent(ENTITY_EVENT_SCRIPT_EVENT));
	}

	CRY_UNIT_TEST(TestSpawnedTransformation)
	{
		SEntitySpawnParams params;
		params.guid = CryGUID::Create();
		params.sName = __FUNCTION__;
		params.nFlags = ENTITY_FLAG_CLIENT_ONLY;

		params.vPosition = Vec3(1, 2, 3);
		const Ang3 angles = Ang3(gf_PI, 0, gf_PI * 0.5f);
		params.qRotation = Quat::CreateRotationXYZ(angles);
		params.vScale = Vec3(1.5f, 1.f, 0.8f);

		CEntity* pEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(params));
		CRY_UNIT_TEST_ASSERT(pEntity != nullptr);
		CRY_UNIT_TEST_ASSERT(pEntity->GetParent() == nullptr);

		// Check world-space position
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetWorldPos(), params.vPosition);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetWorldTM().GetTranslation(), params.vPosition);

		// Check world-space rotation
		CRY_UNIT_TEST_CHECK_CLOSE(pEntity->GetWorldRotation(), params.qRotation, VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(Quat(pEntity->GetWorldTM()), params.qRotation, VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pEntity->GetRightDir(), params.qRotation.GetColumn0(), VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pEntity->GetForwardDir(), params.qRotation.GetColumn1(), VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pEntity->GetUpDir(), params.qRotation.GetColumn2(), VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pEntity->GetWorldAngles(), Ang3(params.qRotation), VEC_EPSILON);

		// Check world-space scale
		CRY_UNIT_TEST_CHECK_CLOSE(pEntity->GetWorldScale(), params.vScale, VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pEntity->GetWorldTM().GetScale(), params.vScale, VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pEntity->GetWorldTM(), Matrix34::Create(params.vScale, params.qRotation, params.vPosition), VEC_EPSILON);

		// Entities with parents currently return the world space position for the local Get functions
		// Ensure that this still works, while it remains intended
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetPos(), params.vPosition);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetRotation(), params.qRotation);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetScale(), params.vScale);
		CRY_UNIT_TEST_CHECK_EQUAL(pEntity->GetLocalTM(), Matrix34::Create(params.vScale, params.qRotation, params.vPosition));

		g_pIEntitySystem->RemoveEntity(pEntity->GetId());
	}

	CRY_UNIT_TEST(TestSpawnedChildTransformation)
	{
		SEntitySpawnParams parentParams;
		parentParams.guid = CryGUID::Create();
		parentParams.sName = __FUNCTION__;
		parentParams.nFlags = ENTITY_FLAG_CLIENT_ONLY;

		parentParams.vPosition = Vec3(1, 2, 3);
		const Ang3 angles = Ang3(gf_PI, 0, gf_PI * 0.5f);
		parentParams.qRotation = Quat::CreateRotationXYZ(angles);
		parentParams.vScale = Vec3(1.5f, 1.f, 0.8f);

		CEntity* const pParentEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(parentParams));
		CRY_UNIT_TEST_ASSERT(pParentEntity != nullptr);
		CRY_UNIT_TEST_ASSERT(pParentEntity->GetParent() == nullptr);

		SEntitySpawnParams childParams = parentParams;
		childParams.id = INVALID_ENTITYID;
		childParams.guid = CryGUID::Create();
		childParams.vPosition = Vec3(2, 2, 2);
		childParams.qRotation = Quat::CreateRotationZ(0.2f);
		childParams.vScale = Vec3(1.f, 1.f, 1.2f);
		childParams.pParent = pParentEntity;

		const CEntity* const pChildEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(childParams));
		CRY_UNIT_TEST_ASSERT(pChildEntity != nullptr);
		CRY_UNIT_TEST_ASSERT(pChildEntity->GetParent() == pParentEntity);

		// Ensure that parent world transform is correct
		CRY_UNIT_TEST_CHECK_CLOSE(pParentEntity->GetWorldTM(), Matrix34::Create(parentParams.vScale, parentParams.qRotation, parentParams.vPosition), VEC_EPSILON);

		// Check world-space position
		CRY_UNIT_TEST_CHECK_EQUAL(pChildEntity->GetWorldPos(), pParentEntity->GetWorldTM().TransformPoint(childParams.vPosition));
		CRY_UNIT_TEST_CHECK_EQUAL(pChildEntity->GetWorldTM().GetTranslation(), pParentEntity->GetWorldTM().TransformPoint(childParams.vPosition));

		// Check world-space rotation
		CRY_UNIT_TEST_CHECK_CLOSE(pChildEntity->GetWorldRotation(), pParentEntity->GetWorldRotation() * childParams.qRotation, VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(Quat(pChildEntity->GetWorldTM()), pParentEntity->GetWorldRotation() * childParams.qRotation, VEC_EPSILON);

		Matrix34 childWorldTransform = pParentEntity->GetWorldTM() * Matrix34::Create(childParams.vScale, childParams.qRotation, childParams.vPosition);
		childWorldTransform.OrthonormalizeFast();

		CRY_UNIT_TEST_CHECK_CLOSE(pChildEntity->GetRightDir(), childWorldTransform.GetColumn0(), VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pChildEntity->GetForwardDir(), childWorldTransform.GetColumn1(), VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pChildEntity->GetUpDir(), childWorldTransform.GetColumn2(), VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pChildEntity->GetWorldAngles(), Ang3(pParentEntity->GetWorldRotation() * childParams.qRotation), VEC_EPSILON);

		// Check world-space scale
		CRY_UNIT_TEST_CHECK_CLOSE(pChildEntity->GetWorldScale(), pParentEntity->GetWorldScale().CompMul(childParams.vScale), VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pChildEntity->GetWorldTM().GetScale(), pParentEntity->GetWorldScale().CompMul(childParams.vScale), VEC_EPSILON);
		CRY_UNIT_TEST_CHECK_CLOSE(pChildEntity->GetWorldTM(), pParentEntity->GetWorldTM() * Matrix34::Create(childParams.vScale, childParams.qRotation, childParams.vPosition), VEC_EPSILON);

		// Check local-space
		CRY_UNIT_TEST_CHECK_EQUAL(pChildEntity->GetPos(), childParams.vPosition);
		CRY_UNIT_TEST_CHECK_EQUAL(pChildEntity->GetRotation(), childParams.qRotation);
		CRY_UNIT_TEST_CHECK_EQUAL(pChildEntity->GetScale(), childParams.vScale);
		CRY_UNIT_TEST_CHECK_EQUAL(pChildEntity->GetLocalTM(), Matrix34::Create(childParams.vScale, childParams.qRotation, childParams.vPosition));

		g_pIEntitySystem->RemoveEntity(pParentEntity->GetId());
		g_pIEntitySystem->RemoveEntity(pChildEntity->GetId());
	}

	CRY_UNIT_TEST(TestDefaultEntitySpawnClass)
	{
		SEntitySpawnParams spawnParams;
		spawnParams.guid = CryGUID::Create();
		spawnParams.sName = __FUNCTION__;
		spawnParams.nFlags = ENTITY_FLAG_CLIENT_ONLY;

		CEntity* const pEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(spawnParams));
		CRY_UNIT_TEST_ASSERT(pEntity != nullptr);
		CRY_UNIT_TEST_ASSERT(pEntity->GetClass() != nullptr);
		CRY_UNIT_TEST_ASSERT(pEntity->GetClass() == g_pIEntitySystem->GetClassRegistry()->GetDefaultClass());
	}

	CRY_UNIT_TEST(TestComponentShutdownOrder)
	{
		bool firstComponentDestroyed = false;
		bool secondComponentDestroyed = false;

		class CFirstComponent : public IEntityComponent
		{
			bool& wasDestroyed;
			const bool& wasOtherDestroyed;

		public:
			CFirstComponent(bool& destroyed, bool& otherDestroyed) : wasDestroyed(destroyed), wasOtherDestroyed(otherDestroyed) {}
			virtual ~CFirstComponent()
			{
				CRY_UNIT_TEST_ASSERT(!wasDestroyed);
				CRY_UNIT_TEST_ASSERT(wasOtherDestroyed);
				wasDestroyed = true;
			}
		};

		class CSecondComponent : public IEntityComponent
		{
			bool& wasDestroyed;
			const bool& wasOtherDestroyed;

		public:
			CSecondComponent(bool& destroyed, bool& otherDestroyed) : wasDestroyed(destroyed), wasOtherDestroyed(otherDestroyed) {}
			virtual ~CSecondComponent() 
			{
				CRY_UNIT_TEST_ASSERT(!wasDestroyed);
				CRY_UNIT_TEST_ASSERT(!wasOtherDestroyed);
				wasDestroyed = true;
			}
		};

		SEntitySpawnParams spawnParams;
		spawnParams.guid = CryGUID::Create();
		spawnParams.sName = __FUNCTION__;
		spawnParams.nFlags = ENTITY_FLAG_CLIENT_ONLY;

		CEntity* const pEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(spawnParams));
		CRY_UNIT_TEST_ASSERT(pEntity != nullptr);

		pEntity->CreateComponentClass<CFirstComponent>(firstComponentDestroyed, secondComponentDestroyed);
		CRY_UNIT_TEST_ASSERT(!firstComponentDestroyed && !secondComponentDestroyed);
		pEntity->CreateComponentClass<CSecondComponent>(secondComponentDestroyed, firstComponentDestroyed);
		CRY_UNIT_TEST_ASSERT(!firstComponentDestroyed && !secondComponentDestroyed);

		g_pIEntitySystem->RemoveEntity(pEntity->GetId(), true);
		CRY_UNIT_TEST_ASSERT(firstComponentDestroyed && secondComponentDestroyed);
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
