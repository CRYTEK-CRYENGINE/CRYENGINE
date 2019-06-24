// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//EditorCommon
#include "EditorFramework/StateSerializable.h"
#include "ToolBarAreaItem.h"

// CryCommon
#include <CrySandbox/CrySignal.h>

// Qt
#include <QWidget>

//EditorCommon
class CAbstractMenu;
class CDragHandle;
class CEditor;

// Qt
class QBoxLayout;
class QSpacerItem;
class QToolBar;

// In charge of creating and managing tool bar areas for all editors. It also serves as mediation between editors
// and editor toolbar service
class CToolBarArea : public QWidget, public IStateSerializable
{
	Q_OBJECT
	Q_INTERFACES(IStateSerializable)

public:
	CToolBarArea(CEditor* pParent, Qt::Orientation orientation);

	Qt::Orientation            GetOrientation() const { return m_orientation; }
	std::vector<CToolBarItem*> GetToolBars() const;

	// Provides the minimum size of the largest item in this area
	QSize                      GetLargestItemMinimumSize() const;
	void                       SetOrientation(Qt::Orientation orientation);
	void                       SetActionContextPosition(const QPoint& actionContextPosition) { m_actionContextPosition = actionContextPosition; }
	void                       FillContextMenu(CAbstractMenu* pMenu);

	// Item management
	void          AddItem(CToolBarAreaItem* pItem, int targetIndex = -1);
	void          AddToolBar(QToolBar* pToolBar, int targetIndex = -1);
	void          AddSpacer(CSpacerItem::SpacerType spacerType, int targetIndex = -1);
	void          RemoveItem(CToolBarAreaItem* pItem);
	void          DeleteToolBar(CToolBarAreaItem* pToolBarItem);

	void          HideAll();

	CToolBarItem* FindToolBarByName(const char* szToolBarName) const;

	// Area management
	void SetLocked(bool isLocked);

	// Layout saving/loading
	QVariantMap GetState() const override;
	void        SetState(const QVariantMap& state) override;

protected:
	// Drag and drop handling
	void dragEnterEvent(QDragEnterEvent* pEvent) override;
	void dragMoveEvent(QDragMoveEvent* pEvent) override;
	void dragLeaveEvent(QDragLeaveEvent* pEvent) override;
	void dropEvent(QDropEvent* pEvent) override;

	void customEvent(QEvent* event) override;
	void paintEvent(QPaintEvent* pEvent) override;

private:
	// Drag and drop helpers
	void OnDragStart(CToolBarAreaItem* pItem);
	void OnDragEnd(CToolBarAreaItem* pItem);
	void RemoveDropTargetSpacer();

	// Tool bar container management
	int               IndexForItem(CToolBarAreaItem* pItem) const;
	void              MoveItem(CToolBarAreaItem* pItem, int destinationIndex);

	void              UpdateLayoutAlignment(CToolBarAreaItem* pItemToBeRemoved = nullptr);
	CToolBarAreaItem* GetItemAtPosition(const QPoint& globalPos);
	int               GetPlacementIndexFromPosition(const QPoint& globalPos);

public:
	CCrySignal<void()> signalDragStart;
	CCrySignal<void()> signalDragEnd;
	CCrySignal<void(CToolBarAreaItem*, CToolBarArea*, int)> signalItemDropped;

private:
	CEditor*        m_pEditor;
	QBoxLayout*     m_pLayout;
	QSpacerItem*    m_pDropTargetSpacer;
	Qt::Orientation m_orientation;
	QPoint          m_actionContextPosition;
	bool            m_isLocked;
};
