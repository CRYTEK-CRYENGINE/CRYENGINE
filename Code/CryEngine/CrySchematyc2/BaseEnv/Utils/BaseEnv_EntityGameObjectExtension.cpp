// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/Utils/BaseEnv_EntityGameObjectExtension.h"
#include "BaseEnv/Utils/BaseEnv_EntityFoundationProperties.h"

#include "BaseEnv/BaseEnv_BaseEnv.h"
#include "CrySchematyc2/IEntityAttributesProxy.h"
#include "BaseEnv/Utils/BaseEnv_EntityMap.h"
#include "BaseEnv/Utils/BaseEnv_EntityUserData.h"

namespace SchematycBaseEnv
{
	const char* CEntityGameObjectExtension::s_szExtensionName = "SchematycGameEntityGameObjectExtension";

	CEntityGameObjectExtension::CEntityGameObjectExtension()
		: m_simulationMode(Schematyc2::ESimulationMode::NotSet)
		, m_objectState(EObjectState::Uninitialized)
		, m_pObject(nullptr)
	{}

	CEntityGameObjectExtension::~CEntityGameObjectExtension()
	{
		DestroyObject();
	}

	bool CEntityGameObjectExtension::RegisterNetworkSerializer(const Schematyc2::NetworkSerializeCallbackConnection& callbackConnection, int32 aspects)
	{
		return false;
	}

	void CEntityGameObjectExtension::MarkAspectsDirty(int32 aspects) {}

	void CEntityGameObjectExtension::SetAspectDelegation(int32 aspects, const EAspectDelegation delegation) {}

	bool CEntityGameObjectExtension::ClientAuthority() const
	{
		return false;
	}

	uint16 CEntityGameObjectExtension::GetChannelId() const
	{
		return 0;
	}

	bool CEntityGameObjectExtension::ServerAuthority() const
	{
		return true;
	}

	bool CEntityGameObjectExtension::LocalAuthority() const
	{
		return gEnv->bServer;
	}

	bool CEntityGameObjectExtension::Init(IGameObject* pGameObject)
	{
		SetGameObject(pGameObject);
		
		IEntityAttributesComponent* pAttribComponent = GetEntity()->GetOrCreateComponent<IEntityAttributesComponent>();

		// NOTE pavloi 2016.11.25: CEntityClassRegistrar::RegisterEntityClass() sets a pointer to SEntityClass in the userProxyData
		const CEntityClassRegistrar::SEntityClass* pClassUserData = static_cast<CEntityClassRegistrar::SEntityClass*>(GetEntity()->GetClass()->GetUserProxyData());
		CRY_ASSERT(pClassUserData);
		if (pClassUserData)
		{
			m_classGUID = pClassUserData->libClassGUID;
			return true;
		}
		return false;
	}

	void CEntityGameObjectExtension::InitClient(int channelId) {}

	void CEntityGameObjectExtension::PostInit(IGameObject* pGameObject)
	{
		CreateObject(!gEnv->IsEditor() || gEnv->IsEditorGameMode());
	}

	void CEntityGameObjectExtension::PostInitClient(int channelId) {}

	bool CEntityGameObjectExtension::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& spawnParams)
	{
		return false;
	}

	void CEntityGameObjectExtension::PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& spawnParams) {}

	void CEntityGameObjectExtension::FullSerialize(TSerialize serialize) {}

	bool CEntityGameObjectExtension::NetSerialize(TSerialize serialize, EEntityAspects aspects, uint8 profile, int flags)
	{
		return true;
	}

	void CEntityGameObjectExtension::PostSerialize() {}

	void CEntityGameObjectExtension::SerializeSpawnInfo(TSerialize serialize) {}

	ISerializableInfoPtr CEntityGameObjectExtension::GetSpawnInfo()
	{
		return ISerializableInfoPtr();
	}

	void CEntityGameObjectExtension::Update(SEntityUpdateContext& context, int updateSlot) {}

	void CEntityGameObjectExtension::PostUpdate(float frameTime) {}

	void CEntityGameObjectExtension::PostRemoteSpawn() {}

	uint64 CEntityGameObjectExtension::GetEventMask() const
	{
		return ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_START_LEVEL) | ENTITY_EVENT_BIT(ENTITY_EVENT_DONE);
	}

	void CEntityGameObjectExtension::ProcessEvent(const SEntityEvent& event)
	{
		switch(event.event)
		{
		case ENTITY_EVENT_RESET:
			{
				// Recreate object when class is modified in editor.
				if(gEnv->IsEditor() && ((GetEntity()->GetFlags() & ENTITY_FLAG_SPAWNED) == 0))
				{
					CreateObject(event.nParam[0] == 1);
				}
				break;
			}
		case ENTITY_EVENT_START_LEVEL:
			{
				// Run object now if previously deferred.
				if(m_objectState == EObjectState::Initialized)
				{
					RunObject();
				}
				break;
			}
		case ENTITY_EVENT_DONE:
			{
				DestroyObject();
				break;
			}
		}
		if(!gEnv->IsEditing())
		{
			//g_eventSignal.Select(GetEntityId()).Send(event);
		}
	}

	void CEntityGameObjectExtension::HandleEvent(const SGameObjectEvent& event) {}

	void CEntityGameObjectExtension::SetChannelId(uint16 channelId) {}

	void CEntityGameObjectExtension::GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
	}

	void CEntityGameObjectExtension::CreateObject(bool bEnteringGame)
	{
		DestroyObject();

		Schematyc2::ILibClassConstPtr pLibClass = gEnv->pSchematyc2->GetLibRegistry().GetClass(m_classGUID);
		if(pLibClass)
		{
			IEntity*                       pEntity = GetEntity();
			const EntityId                 entityId = pEntity->GetId();
			Schematyc2::IPropertiesConstPtr pFoundationProperties = pLibClass->GetFoundationProperties();
			CRY_ASSERT(pFoundationProperties);
			if(pFoundationProperties)
			{
				const SEntityFoundationProperties* pFoundationPropertiesImpl = pFoundationProperties->ToPtr<SEntityFoundationProperties>();
				if(pFoundationPropertiesImpl->bTriggerAreas)
				{
					pEntity->AddFlags(ENTITY_FLAG_TRIGGER_AREAS);
				}
				else
				{
					pEntity->ClearFlags(ENTITY_FLAG_TRIGGER_AREAS);
				}
			}

			bool                    bIsPreview = false;
			Schematyc2::EObjectFlags objectFlags = Schematyc2::EObjectFlags::None;

			if(gEnv->IsEditor())
			{
				const SEntityUserData* pUserData = static_cast<const SEntityUserData*>(GetGameObject()->GetUserData());
				if(pUserData)
				{
					bIsPreview  = pUserData->bIsPreview;
					objectFlags = pUserData->objectFlags;
				}
			}

			m_simulationMode = bIsPreview ? Schematyc2::ESimulationMode::Preview : bEnteringGame ? Schematyc2::ESimulationMode::Game : Schematyc2::ESimulationMode::Editing;
			m_objectState    = EObjectState::Initialized;

			Schematyc2::SObjectParams objectParams;
			objectParams.pLibClass = pLibClass;

			IEntityAttributesComponent* pAttributesProxy = GetEntity()->GetComponent<IEntityAttributesComponent>();
			if(pAttributesProxy)
			{
				IEntityAttributePtr pPropertiesAttribute = EntityAttributeUtils::FindAttribute(pAttributesProxy->GetAttributes(), "SchematycGameEntityPropertiesAttribute");
				if(pPropertiesAttribute)
				{
					objectParams.pProperties = pPropertiesAttribute->GetProperties();
				}
			}

			objectParams.pNetworkObject = this;
			objectParams.entityId       = Schematyc2::ExplicitEntityId(entityId);
			objectParams.flags          = objectFlags;

			m_pObject = gEnv->pSchematyc2->GetObjectManager().CreateObject(objectParams);

			SchematycBaseEnv::CBaseEnv::GetInstance().GetGameEntityMap().AddObject(entityId, m_pObject->GetObjectId());

			// Run object now or defer?
			if(gEnv->IsEditor()/* && !gEnv->pGame->IsLevelLoading()*/)
			{
				RunObject();
			}
		}
	}

	void CEntityGameObjectExtension::DestroyObject()
	{
		if(m_pObject)
		{
			SchematycBaseEnv::CBaseEnv::GetInstance().GetGameEntityMap().RemoveObject(GetEntityId());
			gEnv->pSchematyc2->GetObjectManager().DestroyObject(m_pObject);
			m_simulationMode = Schematyc2::ESimulationMode::NotSet;
			m_objectState    = EObjectState::Uninitialized;
			m_pObject        = nullptr;
		}
	}

	void CEntityGameObjectExtension::RunObject()
	{
		CRY_ASSERT(m_objectState == EObjectState::Initialized);
		if(m_objectState == EObjectState::Initialized)
		{
			m_objectState = EObjectState::Running;
			m_pObject->Run(m_simulationMode);
		}
	}
}