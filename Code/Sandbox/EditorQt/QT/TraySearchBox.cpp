// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

// Qt
#include <QMenuBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyleOption>
#include <QPainter>
#include <QAction>
#include <QHeaderView>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QStandardItemModel>
#include <QToolButton>

// Editor
#include "TraySearchBox.h"
#include "QtMainFrame.h"
#include "Commands/CommandManager.h"
#include "Commands/QCommandAction.h"

// EditorCommon
#include <Controls/QPopupWidget.h>
#include <ProxyModels/DeepFilterProxyModel.h>
#include <Notifications/NotificationCenter.h>

void CTraySearchBox::QMenuTreeView::mouseMoveEvent(QMouseEvent* pEvent)
{
	QModelIndex newIdx = indexAt(pEvent->pos());
	if (newIdx.isValid())
	{
		setCurrentIndex(newIdx);
	}
}

CTraySearchBox::CTraySearchBox(QWidget* pParent /* = nullptr*/)
	: QSearchBox(pParent)
	, m_pPopupWidget(nullptr)
	, m_pTreeView(nullptr)
	, m_pFilterProxy(nullptr)
{
	EnableContinuousSearch(true);
	connect(this, &QLineEdit::textChanged, this, &CTraySearchBox::ShowSearchResults);
	signalOnFiltered.Connect(this, &CTraySearchBox::OnFilter);

	m_pFilterProxy = new QDeepFilterProxyModel();
	m_pFilterProxy->setSourceModel(CreateMainMenuModel());
	SetModel(m_pFilterProxy);

	m_pTreeView = new QMenuTreeView();
	m_pTreeView->setModel(m_pFilterProxy);
	m_pTreeView->setDragEnabled(true);
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setAllColumnsShowFocus(true);
	m_pTreeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_pTreeView->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_pTreeView->header()->setDefaultSectionSize(300);
	m_pTreeView->setHeaderHidden(true);

	m_pPopupWidget = new QPopupWidget("TraySearchPopupWidget", m_pTreeView);
	m_pPopupWidget->setAttribute(Qt::WA_ShowWithoutActivating);
	m_pPopupWidget->SetFocusShareWidget(this);
	m_pPopupWidget->hide();

	// Execute actions on click or enter/return key press
	connect(m_pTreeView, &QAdvancedTreeView::activated, this, &CTraySearchBox::OnItemSelected);
	connect(m_pTreeView, &QAdvancedTreeView::clicked, this, &CTraySearchBox::OnItemSelected);

	QCommandAction* pAction = GetIEditorImpl()->GetCommandManager()->GetCommandAction("search.focus");
	if (pAction)
	{
		connect(pAction, &QAction::changed, this, &CTraySearchBox::UpdatePlaceHolderText);
	}

	UpdatePlaceHolderText();
}

CTraySearchBox::~CTraySearchBox()
{
	m_pPopupWidget->deleteLater();
}

void CTraySearchBox::UpdatePlaceHolderText()
{
	// Check if there's a shortcut action to focus the search box
	QCommandAction* pAction = GetIEditorImpl()->GetCommandManager()->GetCommandAction("search.focus");

	if (!pAction)
		return;

	QKeySequence shortcut = pAction->shortcut();

	if (shortcut.isEmpty())
	{
		setPlaceholderText(tr("Search"));
	}
	else
	{
		setPlaceholderText(tr("Search (%1)").arg(shortcut.toString()));
	}
}

void CTraySearchBox::OnItemSelected(const QModelIndex& index)
{
	// Make sure we don't try to execute a module/category
	if (!m_pFilterProxy->hasChildren(index))
	{
		QModelIndex mappedIdx = m_pFilterProxy->mapToSource(index);
		QVariant ptrVariant = mappedIdx.data(Qt::UserRole + 1);
		QAction* pAction = ptrVariant.value<QAction*>();
		if (pAction)
			pAction->trigger();

		clear();
		m_pPopupWidget->hide();
	}
}

void CTraySearchBox::ShowSearchResults()
{
	if (text().isEmpty())
	{
		m_pPopupWidget->hide();
		return;
	}

	if (m_pPopupWidget && m_pPopupWidget->isVisible())
		return;

	m_pTreeView->expandAll();
	m_pPopupWidget->ShowAt(mapToGlobal(QPoint(width(), height())));
}

void CTraySearchBox::OnFilter()
{
	m_pTreeView->setCurrentIndex(m_pFilterProxy->index(0, 0));
}

QStandardItemModel* CTraySearchBox::CreateMainMenuModel()
{
	QStandardItemModel* pModel = new QStandardItemModel();
	QStringList headerLabels;
	headerLabels.push_back("Name");
	headerLabels.push_back("Path");
	pModel->setHorizontalHeaderLabels(headerLabels);

	if (CEditorMainFrame::GetInstance())
	{
		QList<QMenu*> menus = CEditorMainFrame::GetInstance()->menuBar()->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);

		// Must go through menus because actions may have been created with a parent other than the menu that holds it.
		// In fact it may be attached to several different widgets we don't really care about, so the only
		// way is to traverse the menus in the main menu bar.
		for (QMenu* pMenu : menus)
		{
			AddActionsToModel(pMenu, pModel);
		}
	}

	return pModel;
}

void CTraySearchBox::AddActionsToModel(QMenu* pParentMenu, QStandardItemModel* pModel)
{
	QList<QMenu*> menus = pParentMenu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);
	QList<QAction*> actions = pParentMenu->actions();

	// Go through all child menus as well
	for (QMenu* pMenu : menus)
	{
		AddActionsToModel(pMenu, pModel);
	}

	for (QAction* pAction : actions)
	{
		// Don't add actions that are actually menus, or if they have empty text
		if (pAction->menu() || pAction->text().isEmpty())
			continue;

		QMenu* pParent = pParentMenu;
		QString path(pParent->title()); // Figure out the path to the action
		while (pParent = qobject_cast<QMenu*>(pParent->parent()))
		{
			path.prepend(pParent->title() + QChar(8594)); // Unicode character for arrow right
		}

		QStandardItem* pNameItem = new QStandardItem(pAction->text().remove("&"));
		pNameItem->setEditable(false);
		pNameItem->setToolTip(pAction->text().remove("&"));
		pNameItem->setIcon(QIcon(pAction->icon().pixmap(QSize(16, 16), QIcon::Normal, QIcon::Off)));
		pNameItem->setData(QVariant::fromValue(pAction));
		pModel->appendRow(pNameItem);

		QStandardItem* pPathItem = new QStandardItem(path.remove("&"));
		pPathItem->setData(QVariant::fromValue(pAction));
		pModel->setItem(pModel->rowCount() - 1, 1, pPathItem);
	}
}

void CTraySearchBox::keyPressEvent(QKeyEvent* pKeyEvent)
{
	int key = pKeyEvent->key();
	switch (pKeyEvent->key())
	{
	case Qt::Key_Escape:
		clear();
		break;
	case Qt::Key_Up:
	case Qt::Key_Down:
	case Qt::Key_Enter:
	case Qt::Key_Return:
		QCoreApplication::sendEvent(m_pTreeView, pKeyEvent);
		break;
	default:
		break;
	}

	QSearchBox::keyPressEvent(pKeyEvent);
}

namespace Private_SearchCommands
{
static void PyFocus()
{
	CTraySearchBox* pSearch = GetIEditorImpl()->GetTrayArea()->GetTrayWidget<CTraySearchBox>();
	if (pSearch)
		pSearch->setFocus();
}
}

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_SearchCommands::PyFocus, search, focus,
                                   CCommandDescription("Focus Editor global search"));
REGISTER_EDITOR_COMMAND_SHORTCUT(search, focus, "Ctrl+Alt+F");

