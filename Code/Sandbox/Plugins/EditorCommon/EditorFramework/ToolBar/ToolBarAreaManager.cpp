// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ToolBarAreaManager.h"

// EditorCommon
#include "Commands/QCommandAction.h"
#include "EditorFramework/Editor.h"
#include "EditorFramework/Events.h"
#include "Menu/AbstractMenu.h"
#include "Menu/MenuWidgetBuilders.h"
#include "QtUtil.h"
#include "ToolBarArea.h"
#include "ToolBarAreaItem.h"
#include "ToolBarCustomizeDialog.h"
#include "ToolBarService.h"

// Qt
#include <QToolBar>

CToolBarAreaManager::CToolBarAreaManager(CEditor* pEditor)
	: m_pEditor(pEditor)
	, m_pFocusedArea(nullptr)
	, m_areaCount(IndexForArea(Area::Right) + 1)
	, m_isLocked(false)
{
	m_pEditor->signalAdaptiveLayoutChanged.Connect(this, &CToolBarAreaManager::OnAdaptiveLayoutChanged);
}

void CToolBarAreaManager::SetDragAndDropMode(bool isEnabled)
{
	for (CToolBarArea* pToolBarArea : m_toolBarAreas)
	{
		// Set value for styling customization of drop areas
		pToolBarArea->setProperty("dragAndDropMode", isEnabled);
		pToolBarArea->style()->unpolish(pToolBarArea);
		pToolBarArea->style()->polish(pToolBarArea);
	}
}

void CToolBarAreaManager::OnDragStart()
{
	SetDragAndDropMode(true);
}

void CToolBarAreaManager::OnDragEnd()
{
	SetDragAndDropMode(false);
}

void CToolBarAreaManager::CreateAreas()
{
	CRY_ASSERT_MESSAGE(m_toolBarAreas.size() < m_areaCount, "Areas have been already created, cannot create more");

	for (auto i = 0; i < m_areaCount; ++i)
	{
		const Area area = AreaForIndex(i);
		CToolBarArea* pToolBarArea = new CToolBarArea(m_pEditor, GetOrientation(area));
		pToolBarArea->connect(pToolBarArea, &QWidget::customContextMenuRequested, [pToolBarArea, this]()
		{
			// Force this widget to gain focus so we can handle any triggered actions
			pToolBarArea->setFocus();

			m_pFocusedArea = pToolBarArea;
			ShowContextMenu();
			m_pFocusedArea = nullptr;
		});
		m_toolBarAreas.push_back(pToolBarArea);
		pToolBarArea->signalDragStart.Connect(this, &CToolBarAreaManager::OnDragStart);
		pToolBarArea->signalDragEnd.Connect(this, &CToolBarAreaManager::OnDragEnd);
		pToolBarArea->signalItemDropped.Connect(this, &CToolBarAreaManager::MoveToolBarToArea);
	}

	m_pDefaultArea = GetWidget(Area::Default);
}

void CToolBarAreaManager::LoadToolBars()
{
	std::vector<QToolBar*> toolBars = GetIEditor()->GetToolBarService()->LoadToolBars(m_pEditor);

	for (QToolBar* pToolBar : toolBars)
	{
		m_pDefaultArea->AddToolBar(pToolBar);
	}
}

void CToolBarAreaManager::CreateMenu()
{
	// Create tool bar menu
	m_pEditor->AddToMenu({ CEditor::MenuItems::ToolBarMenu });
	CAbstractMenu* pToolBarsMenu = m_pEditor->GetMenu(CEditor::MenuItems::ToolBarMenu);
	FillMenuForArea(pToolBarsMenu, nullptr);

	// Make sure a menu with this name doesn't already exist, if so clear it
	pToolBarsMenu->signalAboutToShow.Connect(this, &CToolBarAreaManager::FillMenu);
}

void CToolBarAreaManager::Initialize()
{
	CreateAreas();
	// Make sure to create menus before loading toolbars since toolbars might have actions related to this menu
	CreateMenu();
	LoadToolBars();
}

bool CToolBarAreaManager::ToggleLock()
{
	SetLocked(!m_isLocked);
	return true;
}

void CToolBarAreaManager::SetLocked(bool isLocked)
{
	if (m_isLocked == isLocked)
		return;

	m_isLocked = isLocked;

	for (CToolBarArea* pToolBarArea : m_toolBarAreas)
	{
		pToolBarArea->SetLocked(m_isLocked);
	}
}

bool CToolBarAreaManager::AddExpandingSpacer()
{
	m_pDefaultArea->AddSpacer(CSpacerItem::SpacerType::Expanding);
	return true;
}

bool CToolBarAreaManager::AddFixedSpacer()
{
	m_pDefaultArea->AddSpacer(CSpacerItem::SpacerType::Fixed);
	return true;
}

CToolBarArea* CToolBarAreaManager::GetWidget(Area area) const
{
	return m_toolBarAreas[IndexForArea(area)];
}

void CToolBarAreaManager::ShowContextMenu()
{
	QPoint actionContextPosition(QCursor::pos());
	CToolBarArea* pCurrentArea = nullptr;
	for (CToolBarArea* pArea : m_toolBarAreas)
	{
		if (pArea->contentsRect().contains(pArea->mapFromGlobal(actionContextPosition)))
		{
			pCurrentArea = pArea;
			pArea->SetActionContextPosition(actionContextPosition);
			break;
		}
	}

	CAbstractMenu menuDesc;
	FillMenuForArea(&menuDesc, pCurrentArea);

	QMenu menu;
	menuDesc.Build(MenuWidgetBuilders::CMenuBuilder(&menu));

	menu.exec(actionContextPosition);
}

void CToolBarAreaManager::FillMenu(CAbstractMenu* pMenu)
{
	FillMenuForArea(pMenu, nullptr);
}

void CToolBarAreaManager::FillMenuForArea(CAbstractMenu* pMenu, CToolBarArea* pCurrentArea)
{
	pMenu->Clear();

	pMenu->CreateCommandAction("toolbar.customize");
	pMenu->CreateCommandAction("toolbar.toggle_lock")->setChecked(m_isLocked);

	int sec = pMenu->GetNextEmptySection();
	pMenu->SetSectionName(sec, "Spacers");
	pMenu->CreateCommandAction("toolbar.insert_expanding_spacer", sec)->setEnabled(!m_isLocked);
	pMenu->CreateCommandAction("toolbar.insert_fixed_spacer", sec)->setEnabled(!m_isLocked);
	if (pCurrentArea)
	{
		pCurrentArea->FillContextMenu(pMenu);
	}

	sec = pMenu->GetNextEmptySection();
	pMenu->SetSectionName(sec, "Toolbars");

	// Get all toolbars in alphabetical order
	std::map<string, CToolBarItem*> toolBarMap;
	for (CToolBarArea* pArea : m_toolBarAreas)
	{
		const std::vector<CToolBarItem*>& toolBars = pArea->GetToolBars();
		for (CToolBarItem* pToolBar : toolBars)
		{
			toolBarMap.insert(std::pair<string, CToolBarItem*>(QtUtil::ToString(pToolBar->GetTitle()), pToolBar));
		}
	}

	// Create actions for modifying toolbar visibility
	for (const std::pair<string, CToolBarItem*>& toolBarIte : toolBarMap)
	{
		// Toolbar name is packed as part of the window title (this is used by QMainWindow to display toolbar names in context menu)
		CToolBarItem* pToolBar = toolBarIte.second;
		QAction* pToggleVisibilityAction = new QAction(pToolBar->GetTitle());
		pToggleVisibilityAction->setCheckable(true);
		pToggleVisibilityAction->setChecked(pToolBar->isVisible());
		pToggleVisibilityAction->connect(pToggleVisibilityAction, &QAction::triggered, [pToolBar](bool isChecked)
		{
			pToolBar->setVisible(isChecked);
		});
		pMenu->AddAction(pToggleVisibilityAction, sec);
	}
}

CToolBarArea* CToolBarAreaManager::GetFocusedArea() const
{
	if (m_pFocusedArea)
		return m_pFocusedArea;

	// If there's no focused area, return default
	return m_pDefaultArea;
}

CToolBarItem* CToolBarAreaManager::FindToolBarByName(const char* szToolBarObjectName)
{
	CToolBarItem* pToolBarItem = nullptr;

	for (CToolBarArea* pArea : m_toolBarAreas)
	{
		pToolBarItem = pArea->FindToolBarByName(szToolBarObjectName);

		if (pToolBarItem)
			break;
	}

	return pToolBarItem;
}

void CToolBarAreaManager::OnNewToolBar(QToolBar* pToolBar)
{
	GetFocusedArea()->AddToolBar(pToolBar);
}

void CToolBarAreaManager::OnToolBarModified(QToolBar* pToolBar)
{
	const string toolBarName = QtUtil::ToString(pToolBar->objectName());
	CToolBarItem* pToolBarItem = FindToolBarByName(toolBarName.c_str());

	if (!pToolBarItem)
	{
		CRY_ASSERT_MESSAGE(0, "Toolbar Modified: Failed to find tool bar: %s", toolBarName);
	}

	pToolBarItem->ReplaceToolBar(pToolBar);
}

void CToolBarAreaManager::OnToolBarRenamed(const char* szOldObjectName, QToolBar* pToolBar)
{
	CToolBarItem* pToolBarItem = FindToolBarByName(szOldObjectName);

	if (!pToolBarItem)
	{
		CRY_ASSERT_MESSAGE(0, "Toolbar Renamed: Failed to find tool bar: %s", szOldObjectName);
	}

	pToolBarItem->ReplaceToolBar(pToolBar);
}

void CToolBarAreaManager::OnToolBarDeleted(const char* szToolBarObjectName)
{
	const string toolBarName(szToolBarObjectName);
	CToolBarArea* pAreaForToolBar = nullptr;
	CToolBarItem* pToolBarItem = nullptr;

	for (CToolBarArea* pArea : m_toolBarAreas)
	{
		pToolBarItem = pArea->FindToolBarByName(toolBarName.c_str());

		if (pToolBarItem)
		{
			pAreaForToolBar = pArea;
			break;
		}
	}

	if (!pAreaForToolBar)
	{
		CRY_ASSERT_MESSAGE(0, "Toolbar Modified: Failed to find tool bar: %s", toolBarName);
	}

	pAreaForToolBar->DeleteToolBar(pToolBarItem);
}

void CToolBarAreaManager::MoveToolBarToArea(CToolBarAreaItem* pAreaItem, CToolBarArea* pDestinationArea, int targetIndex)
{
	CToolBarArea* pArea = pAreaItem->GetArea();
	pArea->RemoveItem(pAreaItem);
	pDestinationArea->AddItem(pAreaItem, targetIndex);
}

CToolBarAreaManager::Area CToolBarAreaManager::GetAreaFor(const CToolBarArea* pToolBarArea) const
{
	for (unsigned i = 0; i < m_toolBarAreas.size(); ++i)
	{
		if (pToolBarArea == m_toolBarAreas[i])
		{
			return AreaForIndex(i);
		}
	}

	return Area::Default;
}

QVariantMap CToolBarAreaManager::GetState() const
{
	QVariantMap state;
	QVariantList toolBarAreas;
	QVariantList toolBars;
	QVariantMap toolBar;

	for (const CToolBarArea* pArea : m_toolBarAreas)
	{
		const std::vector<CToolBarItem*> areaItems = pArea->GetToolBars();

		for (const CToolBarAreaItem* pAreaItem : areaItems)
		{
			if (pAreaItem->GetType() != CToolBarAreaItem::Type::ToolBar)
				continue;

			const CToolBarItem* pToolBarItem = qobject_cast<const CToolBarItem*>(pAreaItem);

			toolBar["name"] = pToolBarItem->GetName();
			toolBar["area"] = IndexForArea(GetAreaFor(pArea));
			toolBars.push_back(toolBar);
		}

		toolBarAreas.push_back(pArea->GetState());
	}

	state["toolBars"] = toolBars;
	state["toolBarArea"] = toolBarAreas;
	state["isLocked"] = m_isLocked;

	return state;
}

void CToolBarAreaManager::SetState(const QVariantMap& state)
{
	if (m_toolBarAreas.empty())
		return;

	for (CToolBarArea* pArea : m_toolBarAreas)
	{
		pArea->HideAll();
	}

	QVariantList toolBars = state["toolBars"].toList();
	for (const QVariant& toolBar : toolBars)
	{
		const QVariantMap toolBarMap = toolBar.toMap();
		string toolBarName = QtUtil::ToString(toolBarMap["name"].toString());
		Area area = AreaForIndex(toolBarMap["area"].toUInt());

		// All tool bars are by default loaded into the main area
		CToolBarItem* pToolBar = m_pDefaultArea->FindToolBarByName(toolBarName.c_str());

		if (!pToolBar)
			continue;

		MoveToolBarToArea(pToolBar, GetWidget(area));
	}

	QVariantList toolBarAreas = state["toolBarArea"].toList();

	for (auto i = 0; i < toolBarAreas.size(); ++i)
	{
		m_toolBarAreas[i]->SetState(toolBarAreas[i].toMap());
	}

	SetLocked(state["isLocked"].toBool());
}

void CToolBarAreaManager::OnAdaptiveLayoutChanged(Qt::Orientation orientation)
{
	if (m_toolBarAreas.empty())
		return;

	for (auto i = 0; i < m_areaCount; ++i)
	{
		m_toolBarAreas[i]->SetOrientation(GetOrientation(AreaForIndex(i)));
	}
}

unsigned CToolBarAreaManager::IndexForArea(Area area) const
{
	return static_cast<unsigned>(area);
}

CToolBarAreaManager::Area CToolBarAreaManager::AreaForIndex(unsigned index) const
{
	return static_cast<Area>(index);
}

Qt::Orientation CToolBarAreaManager::GetOrientation(Area area) const
{
	const bool isDefaultOrientation = m_pEditor->GetOrientation() == m_pEditor->GetDefaultOrientation();

	switch (area)
	{
	// intentional fall-through
	case Area::Top:
	case Area::Bottom:
		return isDefaultOrientation ? Qt::Horizontal : Qt::Vertical;
	// intentional fall-through
	case Area::Left:
	case Area::Right:
		return isDefaultOrientation ? Qt::Vertical : Qt::Horizontal;
	}

	CRY_ASSERT_MESSAGE(0, "Requesting orientation for unknown tool bar area");
	return Qt::Horizontal;
}
