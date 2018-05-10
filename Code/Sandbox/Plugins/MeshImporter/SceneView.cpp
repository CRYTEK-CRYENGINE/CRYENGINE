// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "SceneView.h"
#include "SceneModel.h"
#include "MainDialog.h"
#include "ComboBoxDelegate.h"

#include <ProxyModels/AttributeFilterProxyModel.h>
#include <QFilteringPanel.h>

#include <QHeaderView>
#include <QVBoxLayout>

////////////////////////////////////////////////////
// CSceneViewCommon

CSceneViewCommon::CSceneViewCommon(QWidget* pParent)
	: QAdvancedTreeView(BehaviorFlags(PreserveExpandedAfterReset | PreserveSelectionAfterReset), pParent)
{
	setSelectionMode(QAbstractItemView::ExtendedSelection);
	setSortingEnabled(true);

	/*
	   header()->setStretchLastSection(false);
	   header()->setSectionResizeMode(0, QHeaderView::Stretch);
	 */
}

CSceneViewCommon::~CSceneViewCommon()
{}

void CSceneViewCommon::ExpandRecursive(const QModelIndex& index, bool bExpand)
{
	if (index.isValid())
	{
		if (bExpand)
		{
			expand(index);
		}
		else
		{
			collapse(index);
		}
		const int numChildren = QAbstractItemView::model()->rowCount(index);
		for (int i = 0; i < numChildren; ++i)
		{
			ExpandRecursive(QAbstractItemView::model()->index(i, 0, index), bExpand);
		}
	}
}

void CSceneViewCommon::ExpandParents(const QModelIndex& index)
{
	assert(index.isValid());
	const QModelIndex parent = QAbstractItemView::model()->parent(index);
	if (parent.isValid())
	{
		expand(parent);
		ExpandParents(parent);
	}
}

CSceneViewCgf::CSceneViewCgf(QWidget* pParent)
	: CSceneViewCommon(pParent)
{
	m_pComboBoxDelegate.reset(new CComboBoxDelegate(this));
	m_pComboBoxDelegate->SetFillEditorFunction([](QMenuComboBox* pEditor)
	{
		for (int i = 0; i < MAX_STATOBJ_LODS_NUM; ++i)
		{
		  pEditor->AddItem(QObject::tr("LOD %1").arg(i));
		}
		pEditor->AddItem(QObject::tr("Physics Proxy"));
	});
	setItemDelegate(m_pComboBoxDelegate.get());

	setEditTriggers(AllEditTriggers);

	header()->setSectionResizeMode(QHeaderView::Interactive);
}

CSceneViewCgf::~CSceneViewCgf()
{}

////////////////////////////////////////////////////
// CSceneViewContainer

CSceneViewContainer::CSceneViewContainer(QAbstractItemModel* pModel, QAdvancedTreeView* pView, QWidget* pParent)
	: QWidget(pParent)
	, m_pFilterModel()
	, m_pModel(pModel)
	, m_pView(pView)
	, m_pFilteringPanel(nullptr)
{
	m_pFilterModel.reset(new QAttributeFilterProxyModel(QAttributeFilterProxyModel::AcceptIfChildMatches));
	m_pFilterModel->setSourceModel(m_pModel);
	m_pFilterModel->setFilterKeyColumn(0);

	m_pView->setModel(m_pFilterModel.get());
	m_pView->setParent(this);

	m_pFilteringPanel = new QFilteringPanel("Mesh Importer Scene", m_pFilterModel.get());
	m_pFilteringPanel->SetContent(m_pView);

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->addWidget(m_pFilteringPanel);
	setLayout(pLayout);

	setContentsMargins(0, 0, 0, 0);
	layout()->setContentsMargins(0, 0, 0, 0);
}

const QAbstractItemModel* CSceneViewContainer::GetModel() const
{
	return m_pModel;
}

const QAdvancedTreeView* CSceneViewContainer::GetView() const
{
	return m_pView;
}

QAbstractItemModel* CSceneViewContainer::GetModel()
{
	return m_pModel;
}

QAbstractItemView* CSceneViewContainer::GetView()
{
	return m_pView;
}

const QAttributeFilterProxyModel* CSceneViewContainer::GetFilter() const
{
	return m_pFilterModel.get();
}

