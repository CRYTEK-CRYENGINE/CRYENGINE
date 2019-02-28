// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PropertyTreeDockWidget.h"
#include "TrackViewComponentsManager.h"
#include "Nodes/TrackViewAnimNode.h"
#include "TrackViewPlugin.h"
#include "SequenceTabWidget.h"

#include <Serialization/QPropertyTree/QPropertyTree.h>
#include <QLayout>
#include <QString>

#include "Serialization.h"
#include <CrySerialization/IArchive.h>
#include "Serialization/PropertyTree/Serialization.h"

#include <CryMovie/AnimKey.h>
#include "Objects/EntityObject.h"
#include <CryEntitySystem/IEntity.h>
#include <CryMovie/IMovieSystem.h>

CTrackViewPropertyTreeWidget::STrackViewPropertiesRoot::STrackViewPropertiesRoot(CTrackViewCore& _core)
	: core(_core)
{}

void CTrackViewPropertyTreeWidget::STrackViewPropertiesRoot::Serialize(Serialization::IArchive& ar)
{
	if (CTrackViewSequenceTabWidget* pTabWidget = core.GetComponentsManager()->GetTrackViewSequenceTabWidget())
	{
		if (pTabWidget->GetCurrentSelectedKeys().size())
		{
			const SSelectedKey& key = pTabWidget->GetCurrentSelectedKeys()[0];
			const stack_string type = key.m_key->m_pType;
			if (type == "Character" ||
				type == "Event")
			{
				SetEntityContext(ar, key, pTabWidget);
			}

			IAnimSequence* pSequence = pTabWidget->GetActiveSequence()->GetIAnimSequence();
			if (sequenceContext.object == nullptr && pSequence)
			{
				sequenceContext.set(pSequence);
				sequenceContext.previousContext = ar.setLastContext(&sequenceContext);
			}
		}

		if (const SAnimTime::Settings* pAnimTimeContext = core.GetAnimTimeSettings())
		{
			animTimeContext.set(pAnimTimeContext);
			animTimeContext.previousContext = ar.setLastContext(&animTimeContext);
		}
	}

	CRY_ASSERT(!structs.empty());
	ar(structs[0], "properties", "+Properties");
}

void CTrackViewPropertyTreeWidget::STrackViewPropertiesRoot::SetEntityContext(Serialization::IArchive& ar, const SSelectedKey& key, CTrackViewSequenceTabWidget* pTabWidget)
{
	if (entityContext.object == nullptr)
	{
		CTrackViewNode* pNode = pTabWidget->GetNodeFromActiveSequence(key.m_pTrack);
		if (pNode)
		{
			while (pNode = pNode->GetParentNode())
			{
				if (pNode->GetNodeType() == eTVNT_AnimNode)
				{
					CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(pNode);
					CEntityObject* pEntityObject = pAnimNode ? pAnimNode->GetNodeEntity() : nullptr;
					IEntity* pEntity = pEntityObject ? pEntityObject->GetIEntity() : nullptr;
					if (pEntity)
					{
						entityContext.set(pEntity);
						entityContext.previousContext = ar.setLastContext(&entityContext);
						return;
					}
				}
			}
		}
	}
}

void CTrackViewPropertyTreeWidget::STrackViewPropertiesRoot::Clear()
{
	entityContext = yasli::Context();
	sequenceContext = yasli::Context();

	structs.clear();
}

CTrackViewPropertyTreeWidget::STrackViewPropertiesRoot::operator Serialization::SStruct()
{
	return Serialization::SStruct(*this);
}

CTrackViewPropertyTreeWidget::CTrackViewPropertyTreeWidget(CTrackViewCore* pTrackViewCore)
	: CTrackViewCoreComponent(pTrackViewCore, true)
	, m_bUndoPush(false)
{
	m_pPropertyTree = new QPropertyTree();
	m_pPropertyTree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pPropertyTree->setAutoRevert(false);
	m_pPropertyTree->setUndoEnabled(false);

	QVBoxLayout* pVBoxLayout = new QVBoxLayout();
	pVBoxLayout->setSpacing(1);
	pVBoxLayout->setContentsMargins(0, 0, 0, 0);
	pVBoxLayout->addWidget(m_pPropertyTree);

	setLayout(pVBoxLayout);

	setObjectName(QString("keyPropertiesDock"));
	setWindowTitle(QString("Properties"));

	connect(m_pPropertyTree, SIGNAL(signalPushUndo()), this, SLOT(OnPropertiesUndoPush()));
	connect(m_pPropertyTree, SIGNAL(signalChanged()), this, SLOT(OnPropertiesChanged()));
}

void CTrackViewPropertyTreeWidget::OnPropertiesUndoPush()
{
	GetIEditor()->GetIUndoManager()->Begin();
	//m_bDontUpdateProperties = true;
	m_bUndoPush = true;
}

void CTrackViewPropertyTreeWidget::OnPropertiesChanged()
{
	CTrackViewComponentsManager* pComponentsManager = GetTrackViewCore()->GetComponentsManager();
	CTrackViewSequenceTabWidget* pTabWidget = pComponentsManager->GetTrackViewSequenceTabWidget();

	if (pTabWidget)
	{
		// Note: Locking the properties is just a workaround. The actual problem is that
		//			 ApplyChangesProperties(...) triggers a chain of updates and events within events
		//			 which results in multiple property updates and in the end properties
		//			 will just disappear.
		GetTrackViewCore()->LockProperties();

		pTabWidget->ApplyChangedProperties();
		if (CTrackViewSequence* pSequence = pComponentsManager->GetTrackViewSequenceTabWidget()->GetActiveSequence())
		{
			pSequence->Deactivate();
			pSequence->Activate();
			if (CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext())
				pAnimationContext->ForceAnimation();
		}

		m_bUndoPush = false;
		GetIEditor()->GetIUndoManager()->Accept("Changed sequence properties");

		GetTrackViewCore()->UnlockProperties();
	}
}

void CTrackViewPropertyTreeWidget::OnTrackViewEditorEvent(ETrackViewEditorEvent event)
{
	switch (event)
	{
	case eTrackViewEditorEvent_OnPropertiesChanged:
		{
			Update();
			break;
		}
	}
}

void CTrackViewPropertyTreeWidget::Update()
{
	if (m_bUndoPush)
	{
		m_bUndoPush = false;
		return;
	}

	m_pPropertyTree->detach();
	m_properties.clear();

	Serialization::SStructs structs;
	GetTrackViewCore()->GetCurrentProperties(structs);
	if (!structs.empty())
	{
		yasli::Serializers serializers;

		// Serializers will hold pointers to items of the m_properties array so it is critical to 
		// pre-allocate enought memory for all the items in once to avoid memory re-allocating.
		m_properties.reserve(structs.size());
		for (auto& prop : structs)
		{
			m_properties.emplace_back(*GetTrackViewCore());
			m_properties.back().structs.emplace_back(prop);

			// Implicitly makes a reference to the m_properties item.
			serializers.push_back(m_properties.back());
		}
		m_pPropertyTree->attach(serializers);
	}
}

void CTrackViewPropertyTreeWidget::OnNodeChanged(CTrackViewNode* pNode, ENodeChangeType type)
{
	if (type == eNodeChangeType_Removed)
	{
		if (pNode->GetFirstSelectedNode() != nullptr || pNode->GetSelectedKeys().GetKeyCount() > 0)
		{
			GetTrackViewCore()->UpdateProperties(ePropertyDataSource_None);
		}
	}
}
