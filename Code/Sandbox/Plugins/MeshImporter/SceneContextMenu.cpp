// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneContextMenu.h"
#include "SceneModelCommon.h"
#include "SceneView.h"

#include <CrySandbox/ScopedVariableSetter.h>

#include <QApplication>
#include <QAbstractItemModel>
#include <QMenu>

namespace Private_SceneContextMenu
{

static void AddExpandCollapseChildrenContextMenu(CSceneViewCommon* pSceneView, const QModelIndex& index, QMenu* pMenu)
{
	if (index.model()->rowCount(index) > 0)
	{
		QAction* const pExpand = pMenu->addAction(QCoreApplication::translate("SceneContextMenu", "Expand child nodes"));
		QObject::connect(pExpand, &QAction::triggered, [=]()
		{
			if (index.isValid())
			{
				pSceneView->ExpandRecursive(index, true);
			}
		});

		QAction* const pCollapse = pMenu->addAction(QCoreApplication::translate("SceneContextMenu", "Collapse child nodes"));
		QObject::connect(pCollapse, &QAction::triggered, [=]()
		{
			if (index.isValid())
			{
				pSceneView->ExpandRecursive(index, false);
			}
		});
	}
}

static void AddExpandCollapseAllContextMenu(CSceneViewCommon* pSceneView, QMenu* pMenu)
{
	QAction* const pExpandAll = pMenu->addAction(QCoreApplication::translate("SceneContextMenu", "Expand all nodes"));
	QObject::connect(pExpandAll, &QAction::triggered, [=]()
	{
		pSceneView->expandAll();
	});

	QAction* const pCollapseAll = pMenu->addAction(QCoreApplication::translate("SceneContextMenu", "Collapse all nodes"));
	QObject::connect(pCollapseAll, &QAction::triggered, [=]()
	{
		pSceneView->collapseAll();
	});
}

} // namespace Private_SceneContextMenu

CSceneContextMenuCommon::CSceneContextMenuCommon(CSceneViewCommon* pSceneView)
	: m_pSceneView(pSceneView)
	, m_pMenu(nullptr)
{
}

void CSceneContextMenuCommon::Attach()
{
	m_pSceneView->setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(m_pSceneView, &QTreeView::customContextMenuRequested, [this](const QPoint& p) { Show(p); });
}

void CSceneContextMenuCommon::Show(const QPoint& point)
{
	QAbstractItemModel* const pModel = m_pSceneView->model();
	QItemSelectionModel* const pSelectionModel = m_pSceneView->selectionModel();
	assert(pModel && pSelectionModel);
	const QPersistentModelIndex index = m_pSceneView->indexAt(point);

	CScopedVariableSetter<QMenu*> setMenu(m_pMenu, new QMenu(m_pSceneView));

	if (index.isValid() && index.data(CSceneModelCommon::eRole_InternalPointerRole).isValid())
	{
		AddSceneElementSectionInternal(index);

		m_pMenu->addSeparator();
	}

	AddGeneralSectionInternal();

	const QPoint popupLocation = point + QPoint(1, 1); // Otherwise double-right-click immediately executes first option
	m_pMenu->popup(m_pSceneView->viewport()->mapToGlobal(popupLocation));
}

void CSceneContextMenuCommon::AddSceneElementSectionInternal(const QModelIndex& index)
{
	using namespace Private_SceneContextMenu;

	CSceneElementCommon* const pSceneElement = (CSceneElementCommon*)index.data(CSceneModelCommon::eRole_InternalPointerRole).value<intptr_t>();
	assert(pSceneElement);
	AddSceneElementSection(pSceneElement);

	AddExpandCollapseChildrenContextMenu(m_pSceneView, index, m_pMenu);
}

void CSceneContextMenuCommon::AddGeneralSectionInternal()
{
	using namespace Private_SceneContextMenu;
	AddExpandCollapseAllContextMenu(m_pSceneView, m_pMenu);
}

