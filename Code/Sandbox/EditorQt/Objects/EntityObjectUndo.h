// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ObjectManager.h"
#include "IEditorImpl.h"
#include <CrySchematyc/Utils/ClassProperties.h>

// Helper to store changes done to entity components, such as property modifications etc
class CUndoEntityObject final : public CUndoBaseObject
{
	struct SCachedComponent
	{
		CryGUID componentInstanceGUID;
		std::unique_ptr<Schematyc::CClassProperties> pProperties;
	};

public:
	CUndoEntityObject(CEntityObject* pObject, const char* undoDescription)
		: CUndoBaseObject(pObject, undoDescription)
	{
		IEntity* pEntity = pObject->GetIEntity();
		if (pEntity == nullptr)
			return;

		CacheProperties(*pEntity, m_cachedProperties);

		pObject->SetLayerModified();
	}

	virtual int GetSize() override { return sizeof(*this); }

	virtual void Undo(bool bUndo) override
	{
		CUndoBaseObject::Undo(bUndo);

		CEntityObject* pObject = static_cast<CEntityObject*>(GetIEditor()->GetObjectManager()->FindObject(m_guid));
		if (pObject)
		{
			IEntity* pEntity = pObject->GetIEntity();
			if (pEntity == nullptr)
				return;

			CacheProperties(*pEntity, m_cachedPropertiesRedo);

			for (const SCachedComponent& cachedComponent : m_cachedProperties)
			{
				IEntityComponent* pComponent = pEntity->GetComponentByGUID(cachedComponent.componentInstanceGUID);
				if (pComponent == nullptr)
					continue;

				RestoreComponentProperties(pComponent, *cachedComponent.pProperties.get());
			}

			pObject->SetLayerModified();
		}
	}
	virtual void Redo() override
	{
		CUndoBaseObject::Redo();

		CEntityObject* pObject = static_cast<CEntityObject*>(GetIEditor()->GetObjectManager()->FindObject(m_guid));
		IEntity* pEntity = pObject->GetIEntity();
		if (pEntity == nullptr)
			return;

		for (const SCachedComponent& cachedComponent : m_cachedPropertiesRedo)
		{
			IEntityComponent* pComponent = pEntity->GetComponentByGUID(cachedComponent.componentInstanceGUID);
			if (pComponent == nullptr)
				continue;

			RestoreComponentProperties(pComponent, *cachedComponent.pProperties.get());
		}

		pObject->SetLayerModified();
	}

protected:
	void RestoreComponentProperties(IEntityComponent* pComponent, const Schematyc::CClassProperties& properties)
	{
		bool membersChanged = !properties.Compare(pComponent->GetClassDesc(), pComponent);
		if (membersChanged)
		{
			// Restore members from the previously recorded class properties
			properties.Apply(pComponent->GetClassDesc(), pComponent);
			// Notify this component about changed property.
			SEntityEvent entityEvent(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
			pComponent->SendEvent(entityEvent);
		}
	}

	void CacheProperties(IEntity& entity, std::vector<SCachedComponent>& destinationVector)
	{
		destinationVector.reserve(entity.GetComponentsCount());

		entity.VisitComponents([&destinationVector](IEntityComponent* pComponent)
		{
			std::unique_ptr<Schematyc::CClassProperties> pClassProperties(new Schematyc::CClassProperties());
			pClassProperties->Read(pComponent->GetClassDesc(), pComponent);

			destinationVector.emplace_back(SCachedComponent{ pComponent->GetGUID(), std::move(pClassProperties) });
		});
	}

protected:
	std::vector<SCachedComponent> m_cachedProperties;
	std::vector<SCachedComponent> m_cachedPropertiesRedo;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for adding an entity component
class CUndoAddComponent : public IUndoObject
{
	struct SComponentInstanceInfo
	{
		CryGUID owningEntityGUID;
		CryGUID instanceGUID;
	};

public:
	CUndoAddComponent(const CEntityComponentClassDesc* pClassDescription, size_t numExpectedEntities)
		: m_pClassDescription(pClassDescription)
	{
		m_affectedEntityInfo.reserve(numExpectedEntities);
	}

	virtual void Undo(bool bUndo) override
	{
		for (const SComponentInstanceInfo& componentInstanceInfo : m_affectedEntityInfo)
		{
			CEntityObject* pEntityObject = static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(componentInstanceInfo.owningEntityGUID));
			if (pEntityObject == nullptr)
				continue;

			IEntity* pEntity = pEntityObject->GetIEntity();
			if (pEntity == nullptr)
				continue;

			IEntityComponent* pComponent = pEntity->GetComponentByGUID(componentInstanceInfo.instanceGUID);
			if (pComponent == nullptr)
				continue;

			pEntity->RemoveComponent(pComponent);
		
			pEntityObject->SetLayerModified();
		}

		GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
	}

	virtual void Redo() override
	{
		for (SComponentInstanceInfo& componentInstanceInfo : m_affectedEntityInfo)
		{
			CEntityObject* pEntityObject = static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(componentInstanceInfo.owningEntityGUID));
			if (pEntityObject == nullptr)
				continue;

			IEntity* pEntity = pEntityObject->GetIEntity();
			if (pEntity == nullptr)
				continue;

			componentInstanceInfo.instanceGUID = AddComponentInternal(pEntity);
		
			pEntityObject->SetLayerModified();
		}

		GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
	}

	void AddComponent(IEntity* pEntity)
	{
		const CryGUID& componentInstanceGUID = AddComponentInternal(pEntity);
		if (!componentInstanceGUID.IsNull())
		{
			const CryGUID& guid = pEntity->GetGuid();
			m_affectedEntityInfo.emplace_back(SComponentInstanceInfo{ guid, componentInstanceGUID });
			
			if (CEntityObject* pEntityObject = static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(guid)))
			{
				pEntityObject->SetLayerModified();
			}
		}
	}

	bool IsValid() const { return m_affectedEntityInfo.size() > 0; }

private:
	const CryGUID& AddComponentInternal(IEntity* pEntity)
	{
		EntityComponentFlags flags = m_pClassDescription->GetComponentFlags();
		flags.Add(EEntityComponentFlags::UserAdded);
		IEntityComponent::SInitParams initParams(pEntity, CryGUID::Create(), "", m_pClassDescription, flags, nullptr, nullptr);
		IF_LIKELY(IEntityComponent* pComponent = pEntity->CreateComponentByInterfaceID(m_pClassDescription->GetGUID(), &initParams))
		{
			return pComponent->GetGUID();
		}

		static CryGUID invalidGUID = CryGUID::Null();
		return invalidGUID;
	}

	virtual const char* GetDescription() { return "Added Entity Component"; }

	const CEntityComponentClassDesc*    m_pClassDescription;
	std::vector<SComponentInstanceInfo> m_affectedEntityInfo;
};

class CUndoEntityLink : public IUndoObject
{
public:
	CUndoEntityLink(const std::vector<CBaseObject*>& objects)
	{
		m_Links.reserve(objects.size());
		for (CBaseObject* pObj : objects)
		{
			if (pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				SLink link;
				link.entityID = pObj->GetId();
				link.linkXmlNode = XmlHelpers::CreateXmlNode("undo");
				((CEntityObject*)pObj)->SaveLink(link.linkXmlNode);
				m_Links.push_back(link);
			}
		}
	}

protected:
	virtual void        Release() { delete this; }
	virtual const char* GetDescription() { return "Entity Link"; }
	virtual const char* GetObjectName() { return ""; }

	virtual void        Undo(bool bUndo)
	{
		for (int i = 0, iLinkSize(m_Links.size()); i < iLinkSize; ++i)
		{
			SLink& link = m_Links[i];
			CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(link.entityID);
			if (pObj == NULL)
				continue;
			if (!pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
				continue;
			CEntityObject* pEntity = (CEntityObject*)pObj;
			if (link.linkXmlNode->getChildCount() == 0)
				continue;
			pEntity->LoadLink(link.linkXmlNode->getChild(0));
		}
	}
	virtual void Redo() {}

private:

	struct SLink
	{
		CryGUID    entityID;
		XmlNodeRef linkXmlNode;
	};

	std::vector<SLink> m_Links;
};

class CUndoCreateFlowGraph : public IUndoObject
{
public:
	CUndoCreateFlowGraph(CEntityObject* entity, const char* groupName)
		: m_entityGuid(entity->GetId())
		, m_groupName(groupName)
	{
	}

protected:
	virtual void Undo(bool bUndo) override
	{
		auto pEntity = static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(m_entityGuid));
		if (pEntity)
		{
			pEntity->RemoveFlowGraph();
		}
	}

	virtual void Redo() override
	{
		auto pEntity = static_cast<CEntityObject*>(GetIEditorImpl()->GetObjectManager()->FindObject(m_entityGuid));
		if (pEntity)
		{
			pEntity->OpenFlowGraph(m_groupName);
		}
	}

	virtual const char* GetDescription() override { return "Create Flow Graph"; }

private:
	CryGUID     m_entityGuid;
	const char* m_groupName;
};