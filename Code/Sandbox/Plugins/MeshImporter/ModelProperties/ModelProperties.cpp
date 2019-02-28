// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ModelProperties.h"

#include <Serialization/QPropertyTree/QPropertyTree.h>

#include <QAbstractItemView>

CModelProperties::CModelProperties(QPropertyTree* pPropertyTree)
	: m_pInspectedModel(nullptr)
	, m_pPropertyTree(pPropertyTree)
{
	CRY_ASSERT(pPropertyTree);
}

void CModelProperties::AddCreateSerializerFunc(const CreateSerializerFunc& f)
{
	m_createSerializerFuncs.push_back(f);
}

void CModelProperties::DetachPropertyObject()
{
	if (m_pPropertyTree->attached())
	{
		m_pPropertyTree->detach();
	}
	m_pInspectedModel = nullptr;
}

void CModelProperties::AttachSelectionToPropertyTree(QAbstractItemModel* pModel, const QModelIndexList& indices)
{
	DetachPropertyObject();
	Serialization::SStructs structs;
	for (auto index : indices)
	{
		std::unique_ptr<SSerializer> pSerializer(GetSerializer(pModel, index));
		if (pSerializer)
		{
			structs.emplace_back(pSerializer->m_sstruct);
			if (pSerializer->m_pObject)
			{
				m_serializedObjects.emplace_back(pSerializer->m_pObject);
			}
		}
	}
	m_pPropertyTree->attach(structs);
	m_pPropertyTree->expandAll();
	m_pInspectedModel = pModel;
}

std::unique_ptr<CModelProperties::SSerializer> CModelProperties::GetSerializer(QAbstractItemModel* pModel, const QModelIndex& modelIndex)
{
	for (const auto& createSerializerFunc : m_createSerializerFuncs)
	{
		std::unique_ptr<SSerializer> pSerializer(createSerializerFunc(pModel, modelIndex));
		if (pSerializer && pSerializer->m_sstruct.pointer())
		{
			return pSerializer;
		}
	}
	return std::unique_ptr<SSerializer>();
}

// This function connects a view to the property tree, so that when the selection changes, the
// currently selected set of items is displayed in the property tree.
void CModelProperties::ConnectViewToPropertyObject(QAbstractItemView* pView)
{
	QAbstractItemView* const pAbstractView = pView;
	auto* const pModel = pView->model();
	QAbstractItemModel* const pAbstractModel = pModel;

	QObject::connect(pAbstractView, &QAbstractItemView::clicked, [this, pView](const QModelIndex& clicked)
	{
		auto* const pSelectionModel = pView->selectionModel();
		AttachSelectionToPropertyTree(pView->model(), pSelectionModel->selectedRows());
	});
	QObject::connect(pAbstractView->selectionModel(), &QItemSelectionModel::currentChanged, [this, pView](const QModelIndex&, const QModelIndex&)
	{
		auto* const pSelectionModel = pView->selectionModel();
		AttachSelectionToPropertyTree(pView->model(), pSelectionModel->selectedRows());
	});
	QObject::connect(pAbstractView->selectionModel(), &QItemSelectionModel::selectionChanged, [this, pView](const QItemSelection& selected, const QItemSelection&)
	{
		AttachSelectionToPropertyTree(pView->model(), selected.indexes());
	});

	// When an object is removed from the model, the property tree needs to be detached.
	// If we don't, the property tree will keep a dangling pointer that will later crash (if data is changed through the property tree)
	QObject::connect(pAbstractModel, &QAbstractItemModel::rowsRemoved, [this, pAbstractModel](const QModelIndex& parent, int first, int last)
	{
		if (pAbstractModel == m_pInspectedModel)
		{
		  DetachPropertyObject();
		}
	});
	QObject::connect(pAbstractModel, &QAbstractItemModel::modelReset, [this, pAbstractModel]()
	{
		if (pAbstractModel == m_pInspectedModel)
		{
		  DetachPropertyObject();
		}
	});
}

