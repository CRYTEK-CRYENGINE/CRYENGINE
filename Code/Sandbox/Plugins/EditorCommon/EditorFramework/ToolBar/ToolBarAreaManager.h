// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//EditorCommon
#include "EditorFramework/StateSerializable.h"

//EditorCommon
class CAbstractMenu;
class CEditor;
class CToolBarArea;
class CToolBarAreaItem;
class CToolBarItem;

// Qt
class QToolBar;

class CToolBarAreaManager : public IStateSerializable
{
public:
	enum class Area
	{
		Default,
		Top = Default,
		Bottom,
		Left,
		Right,
	};

	CToolBarAreaManager(CEditor* pEditor);

	void          Initialize();
	bool          ToggleLock();
	bool          AddExpandingSpacer();
	bool          AddFixedSpacer();

	CToolBarArea* GetWidget(Area area) const;
	Area          GetAreaFor(const CToolBarArea* pToolBarArea) const;

	// Layout management
	QVariantMap GetState() const override;
	void        SetState(const QVariantMap& state) override;
	void        OnAdaptiveLayoutChanged(Qt::Orientation orientation);

	// Toolbar management
	void OnNewToolBar(QToolBar* pToolBar);
	void OnToolBarModified(QToolBar* pToolBar);
	void OnToolBarRenamed(const char* szOldObjectName, QToolBar* pToolBar);
	void OnToolBarDeleted(const char* szToolBarObjectName);

private:
	CToolBarArea* GetFocusedArea() const;
	CToolBarItem* FindToolBarByName(const char* szToolBarObjectName);
	void          SetLocked(bool isLocked);
	void          ShowContextMenu();
	void          FillMenu(CAbstractMenu* pMenu);
	void          FillMenuForArea(CAbstractMenu* pMenu, CToolBarArea* pCurrentArea);

	// Initialization
	void CreateAreas();
	void LoadToolBars();
	void CreateMenu();

	void MoveToolBarToArea(CToolBarAreaItem* pAreaItem, CToolBarArea* pDestinationArea, int targetIndex = -1);

	// Drag and drop handling
	void            SetDragAndDropMode(bool isEnabled);
	void            OnDragStart();
	void            OnDragEnd();

	Qt::Orientation GetOrientation(Area area) const;
	unsigned        IndexForArea(Area area) const;
	Area            AreaForIndex(unsigned index) const;

private:
	CEditor*                   m_pEditor;
	CToolBarArea*              m_pDefaultArea;
	CToolBarArea*              m_pFocusedArea;

	std::vector<CToolBarArea*> m_toolBarAreas;
	const unsigned             m_areaCount;
	bool                       m_isLocked;
};