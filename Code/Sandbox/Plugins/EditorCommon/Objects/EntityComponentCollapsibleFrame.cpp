// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityComponentCollapsibleFrame.h"

#include "Controls/DynamicPopupMenu.h"

#include "IObjectManager.h"
#include "Objects/EntityObject.h"
#include "IUndoObject.h" 

#include <CryEntitySystem/IEntityComponent.h>
#include <CryEntitySystem/IEntity.h>
#include "ISelectionGroup.h"

class CUndoRemoveComponent : public IUndoObject
{
public:
	CUndoRemoveComponent(IEntityComponent* pAddedComponent)
		: m_owningEntityGUID(pAddedComponent->GetEntity()->GetGuid())
		, m_classDescription(pAddedComponent->GetClassDesc())
		, m_flags(pAddedComponent->GetComponentFlags())
		, m_name(pAddedComponent->GetName())
		, m_transform(pAddedComponent->GetTransform())
		, m_componentInstanceGUID(pAddedComponent->GetGUID())
		, m_parentComponentInstanceGUID(pAddedComponent->GetParent() != nullptr ? pAddedComponent->GetParent()->GetGUID() : CryGUID::Null())
	{
		// Serialize component properties to buffer
		gEnv->pSystem->GetArchiveHost()->SaveBinaryBuffer(m_propertyBuffer, Serialization::SStruct(IEntityComponent::SPropertySerializer{ pAddedComponent }));
	}

	virtual void Undo(bool bUndo) override
	{
		CEntityObject* pEntityObject = static_cast<CEntityObject*>(GetIEditor()->GetObjectManager()->FindObject(m_owningEntityGUID));
		if (pEntityObject == nullptr)
			return;

		IEntity* pEntity = pEntityObject->GetIEntity();
		if (pEntity == nullptr)
			return;

		IEntityComponent::SInitParams initParams(pEntity, m_componentInstanceGUID, m_name, &m_classDescription, m_flags, nullptr, m_transform);
		if (!m_parentComponentInstanceGUID.IsNull())
		{
			initParams.pParent = pEntity->GetComponentByGUID(m_parentComponentInstanceGUID);
		}

		IF_LIKELY(IEntityComponent* pComponent = pEntity->CreateComponentByInterfaceID(m_classDescription.GetGUID(), &initParams))
		{
			// Deserialize to the target
			gEnv->pSystem->GetArchiveHost()->LoadBinaryBuffer(Serialization::SStruct(IEntityComponent::SPropertySerializer{ pComponent }), m_propertyBuffer.data(), m_propertyBuffer.size());

			GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
		}
		else
		{
			m_componentInstanceGUID = CryGUID::Null();
		}
	}

	virtual void Redo() override
	{
		CEntityObject* pEntityObject = static_cast<CEntityObject*>(GetIEditor()->GetObjectManager()->FindObject(m_owningEntityGUID));
		if (pEntityObject == nullptr)
			return;

		IEntity* pEntity = pEntityObject->GetIEntity();
		if (pEntity == nullptr)
			return;

		if (IEntityComponent* pComponent = pEntity->GetComponentByGUID(m_componentInstanceGUID))
		{
			pEntity->RemoveComponent(pComponent);

			GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
		}
	}

private:
	virtual int         GetSize() { return sizeof(CUndoRemoveComponent); }
	virtual const char* GetDescription() { return "Removed Entity Component"; }

	const CEntityComponentClassDesc& m_classDescription;
	const CryGUID& m_owningEntityGUID;
	CryGUID m_componentInstanceGUID;
	EntityComponentFlags m_flags;
	string m_name;
	CryTransform::CTransformPtr m_transform;
	CryGUID m_parentComponentInstanceGUID;
	DynArray<char> m_propertyBuffer;
};

CEntityComponentCollapsibleFrameHeader::CEntityComponentCollapsibleFrameHeader(const QString& title, QCollapsibleFrame* pParentCollapsible, const CEntityComponentClassDesc& typeDesc, const size_t typeInstanceIndex, const bool isComponentUserAdded)
	: CCollapsibleFrameHeader(title, pParentCollapsible, QString(typeDesc.GetIcon()))
	, m_title(title)
{
	// Add the options icon
	QToolButton* pOptionsIcon = new QToolButton();
	pOptionsIcon->setIcon(CryIcon("icons:General/edit-select-object.ico"));
	pOptionsIcon->setLayoutDirection(Qt::RightToLeft);
	connect(pOptionsIcon, &QToolButton::clicked, [&typeDesc, typeInstanceIndex, isComponentUserAdded]
	{
		CDynamicPopupMenu menu;
		CPopupMenuItem& root = menu.GetRoot();

		if (isComponentUserAdded)
		{
			root.Add("Remove", "icons:General/Remove_Negative.ico", [&typeDesc, typeInstanceIndex]()
			{
				CUndo undo("Remove Component");

				const ISelectionGroup* pGroup = GetIEditor()->GetISelectionGroup();

				for (size_t i = 0; i < pGroup->GetCount(); ++i)
				{
					CRY_ASSERT(pGroup->GetObject(i) != nullptr);
					IEntity* pEntity = pGroup->GetObject(i)->GetIEntity();
					CRY_ASSERT(pEntity != nullptr);

					DynArray<IEntityComponent*> components;
					pEntity->GetComponentsByTypeId(typeDesc.GetGUID(), components);
					
					// Skip this entity since the component index is different
					if (components.size() <= typeInstanceIndex)
						continue;

					IEntityComponent* pComponent = components[typeInstanceIndex];
					if (pComponent->GetComponentFlags().Check(EEntityComponentFlags::UserAdded))
					{
						CUndo::Record(new CUndoRemoveComponent(pComponent));
						pEntity->RemoveComponent(pComponent);
					}
				}

				GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
			});
		}

		menu.SpawnAtCursor();
	});

	layout()->addWidget(pOptionsIcon);
}

CEntityComponentCollapsibleFrame::CEntityComponentCollapsibleFrame(const QString& title, const CEntityComponentClassDesc& typeDesc, const size_t typeInstanceIndex, const bool isComponentUserAdded)
	: QCollapsibleFrame(nullptr)
{
	SetHeaderWidget(new CEntityComponentCollapsibleFrameHeader(title, this, typeDesc, typeInstanceIndex, isComponentUserAdded));
}
