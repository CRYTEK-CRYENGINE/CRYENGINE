// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectPropertyWidget.h"

#include "Objects/BaseObject.h"
#include "IObjectManager.h"
#include "QAdvancedPropertyTreeLegacy.h"

// EditorQt
#include <Objects/SelectionGroup.h>

// EditorInterface
#include <IUndoManager.h>
#include <IEditor.h>

#include <CrySerialization/IArchive.h>
#include <QBoxLayout>

struct CObjectPropertyWidget::SBaseObjectSerializer
{
	SBaseObjectSerializer(CBaseObject* const pObj, bool bMultiEdit, TSerializationFunc& serializationFunc)
		: m_object(pObj)
		, m_bMultiEdit(bMultiEdit)
		, m_serializationFunc(serializationFunc)
	{
	}

	void YASLI_SERIALIZE_METHOD(Serialization::IArchive& ar)
	{
		m_serializationFunc(m_object, ar, m_bMultiEdit);

		if (ar.isInput())
		{
			m_object->SetModified(false, false);
		}
	}

	const CBaseObjectPtr m_object;
	const bool           m_bMultiEdit;

	TSerializationFunc&  m_serializationFunc;
};

CObjectPropertyWidget::CObjectPropertyWidget(TSerializationFunc serializationFunc)
	: QWidget()
	, m_bReloadProperties(false)
	, m_bCleanupDeletedObjects(false)
	, m_serializationFunc(serializationFunc)
{
	QVBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->setMargin(0);
	mainLayout->setSizeConstraint(QLayout::SetNoConstraint);
	setLayout(mainLayout);

	GetIEditor()->RegisterNotifyListener(this);
	CreatePropertyTrees();
}

CObjectPropertyWidget::~CObjectPropertyWidget()
{
	GetIEditor()->UnregisterNotifyListener(this);
	UnregisterObjects();

	if (m_propertyTree)
	{
		m_propertyTree->deleteLater();
	}
}

void CObjectPropertyWidget::CreatePropertyTrees()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	const CSelectionGroup* pSelectionGroup = GetIEditor()->GetObjectManager()->GetSelection();
	if (pSelectionGroup->GetCount() == 0)
	{
		return;
	}

	auto selectionCount = pSelectionGroup->GetCount();
	m_objectSerializers.reserve(selectionCount);

	m_propertyTree.reset(new QAdvancedPropertyTreeLegacy("ObjectPropertyWidget"));
	m_propertyTree->setExpandLevels(2);
	m_propertyTree->setValueColumnWidth(0.6f);
	m_propertyTree->setAggregateMouseEvents(false);
	m_propertyTree->setFullRowContainers(true);
	m_propertyTree->setSizeToContent(true);

	connect(m_propertyTree.get(), &QPropertyTreeLegacy::signalBeginUndo, this, &CObjectPropertyWidget::OnBeginUndo);
	connect(m_propertyTree.get(), &QPropertyTreeLegacy::signalEndUndo, this, &CObjectPropertyWidget::OnUndoEnd);

	bool bMultiEdit = false;

	if (pSelectionGroup->GetCount() > 1)
	{
		bMultiEdit = true;

		CRuntimeClass* clstype = pSelectionGroup->GetObject(0)->GetRuntimeClass();
		for (size_t i = 0, n = pSelectionGroup->GetCount(); i < n; ++i)
		{
			if (clstype != pSelectionGroup->GetObject(i)->GetRuntimeClass())
			{
				bMultiEdit = false;
				break;
			}
		}
	}

	Serialization::SStructs structs;
	structs.reserve(pSelectionGroup->GetCount());
	for (size_t i = 0, n = pSelectionGroup->GetCount(); i < n; ++i)
	{
		CBaseObject* pObject = pSelectionGroup->GetObject(i);
		pObject->signalChanged.Connect(this, &CObjectPropertyWidget::OnObjectEvent);

		m_objectSerializers.emplace_back(pObject, bMultiEdit, m_serializationFunc);
		structs.emplace_back(m_objectSerializers.back());
	}
	m_propertyTree->attach(structs);

	layout()->addWidget(m_propertyTree.get());
	m_propertyTree.get()->show();
}

// Separate object updates are processed in a lazy manner, else we risk doing too many updates
void CObjectPropertyWidget::OnObjectEvent(const CBaseObject* pObject, const CObjectEvent& event)
{
	if (event.m_type == OBJECT_ON_TRANSFORM || event.m_type == OBJECT_ON_RENAME || event.m_type == OBJECT_ON_UI_PROPERTY_CHANGED)
	{
		m_bReloadProperties = true;
	}
	else if (event.m_type == OBJECT_ON_DELETE)
	{
		m_bCleanupDeletedObjects = true;
		m_bReloadProperties = false;
	}
}

void CObjectPropertyWidget::UnregisterObjects()
{
	// Serializers are objects times tabs, and property trees are times tabs, check CreatePropertyTrees
	if (m_propertyTree)
	{
		for (SBaseObjectSerializer& pSer : m_objectSerializers)
		{
			pSer.m_object->signalChanged.DisconnectObject(this);
		}
	}
}

void CObjectPropertyWidget::OnBeginUndo()
{
	GetIEditor()->GetIUndoManager()->Begin();
	if (m_propertyTree)
	{
		for (SBaseObjectSerializer& pSer : m_objectSerializers)
		{
			pSer.m_object->StoreUndo("Object Modify");
		}
	}
}

void CObjectPropertyWidget::OnUndoEnd(bool undoAccepted)
{
	if (!undoAccepted)
	{
		GetIEditor()->GetIUndoManager()->Cancel();
	}
	else
	{
		GetIEditor()->GetIUndoManager()->Accept("Object Modify");
	}
}

void CObjectPropertyWidget::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnIdleUpdate)
	{
		if (m_bCleanupDeletedObjects)
		{
			m_bCleanupDeletedObjects = false;
			CleanupDeletedObjects();
		}
		else if (m_bReloadProperties)
		{
			m_bReloadProperties = false;
			ReloadPropertyTrees();
		}
	}
}

void CObjectPropertyWidget::ReloadPropertyTrees()
{
	// it's important to use non-interrupting revert here because the reload might be
	// triggered by tweaking a property inside the inspector itself. Non-interrupting
	// revert has guards to ensure that only properties not currently edited will get
	// reverted. This could probably be more fine grained, but this should do for now.
	if (m_propertyTree)
		m_propertyTree->revertNoninterrupting();
}

void CObjectPropertyWidget::CleanupDeletedObjects()
{
	// check which objects are dead and throw them away
	if (m_propertyTree)
	{
		// check for deleted objects
		size_t inumobjects = m_objectSerializers.size();
		vector<CBaseObject*> newobjects;
		newobjects.reserve(inumobjects);

		for (size_t i = 0; i < inumobjects; i++)
		{
			if (!m_objectSerializers[i].m_object->CheckFlags(OBJFLAG_DELETED))
			{
				newobjects.push_back(m_objectSerializers[i].m_object.get());
			}
		}

		UnregisterObjects();

		layout()->removeWidget(m_propertyTree.get());
		m_propertyTree->deleteLater();
		m_propertyTree.release();
		m_objectSerializers.clear();

		if (newobjects.size() > 0)
		{
			CreatePropertyTrees();
		}
		else
		{
			deleteLater();
		}
	}
}
