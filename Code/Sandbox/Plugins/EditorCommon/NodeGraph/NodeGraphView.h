// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryGraphEditor.h"
#include "EditorCommonAPI.h"

#include "NodeGraphViewGraphicsWidget.h"

#include <CrySandbox/CrySignal.h>

#include <QGraphicsView>
#include <QPointF>
#include <QTimer>

class QGraphicsScene;
class QPopupWidget;
class QTrackingTooltip;

class CDictionaryWidget;
class CAbstractDictionaryEntry;

namespace CryGraphEditor {

class CNodeGraphViewModel;
class CNodeGraphViewBackground;

class CAbstractNodeGraphViewModelItem;
class CAbstractNodeItem;
class CAbstractConnectionItem;

class CConnectionWidget;
class CGroupWidget;
class CNodeWidget;
class CPinWidget;

class CNodeItem;
class CParticleGraphModel;
class CNodeGraphStyle;

class EDITOR_COMMON_API CNodeGraphView : public QGraphicsView
{
	Q_OBJECT

	typedef std::map<const CAbstractNodeItem*, CNodeWidget*>             NodeWidgetByItemInstance;
	typedef std::map<const CAbstractConnectionItem*, CConnectionWidget*> ConnectionWidgetByItemInstance;

	enum EOperation : uint16
	{
		eOperation_None = 0,
		eOperation_DeleteSelected
	};

public:
	enum EAction : uint32
	{
		eAction_None = 0,
		eAction_SceneNavigation,
		eAction_ConnectionCreation,
		eAction_ItemBoxSelection,
		eAction_ItemMovement,
		eAction_AddNodeMenu,
		eAction_ConnectionCreationAddNodeMenu,

		eAction_UserAction
	};

public:
	CNodeGraphView();
	~CNodeGraphView();

	CNodeGraphViewModel*       GetModel() const { return m_pModel; }
	void                       SetModel(CNodeGraphViewModel* pModel);

	const CNodeGraphViewStyle& GetStyle() const { return *m_pStyle; }
	void                       SetStyle(const CNodeGraphViewStyle* pStyle);

	bool                       AbortAction();

	void                       OnNodeMouseEvent(QGraphicsItem* pSender, SNodeMouseEventArgs& args);
	void                       OnPinMouseEvent(QGraphicsItem* pSender, SPinMouseEventArgs& args);
	void                       OnConnectionMouseEvent(QGraphicsItem* pSender, SConnectionMouseEventArgs& args);
	void                       OnGroupMouseEvent(QGraphicsItem* pSender, SGroupMouseEventArgs& args);

	void                       SetPosition(QPoint sceenPos);
	QPoint                     GetPosition() const;

	void                       SetZoom(int32 percentZoom);
	int32                      GetZoom() const { return m_currentScalePercent; }

	void                       FitSceneInView();

	const GraphItemSet& GetSelectedItems() const { return m_selectedItems; }
	void                DeselectAllItems();
	void                DeleteSelectedItems();

	void                SelectItem(CAbstractNodeGraphViewModelItem& item);
	void                DeselectItem(CAbstractNodeGraphViewModelItem& item);

	void                SelectItems(const GraphItemSet& items);
	void                DeselectItems(const GraphItemSet& items);
	void                SelectItems(const GraphItemIds& itemIds);
	void                DeselectItems(const GraphItemIds& itemIds);

Q_SIGNALS:
	void SignalItemsReloaded(CNodeGraphView& view);

protected:
	void                                  SelectWidget(CNodeGraphViewGraphicsWidget& itemWidget);
	void                                  DeselectWidget(CNodeGraphViewGraphicsWidget& itemWidget);
	void                                  SelectWidgets(const GraphViewWidgetSet& widgets);
	void                                  DeselectWidgets(const GraphViewWidgetSet& widgets);

	bool                                  SetItemSelectionState(CAbstractNodeGraphViewModelItem& item, bool setSelected);
	bool                                  SetWidgetSelectionState(CNodeGraphViewGraphicsWidget& itemWidget, bool setSelected);

	void                                  SetAction(uint32 action, bool abortCurrent = true);
	uint32                                GetAction() const { return m_action; }

	void                                  ShowItemToolTip(CNodeGraphViewGraphicsWidget* pItemWidget);

	virtual QWidget*                      CreatePropertiesWidget(GraphItemSet& selectedItems);

	virtual void                          ShowNodeContextMenu(CNodeWidget* pNodeWidget, QPointF screenPos);
	virtual void                          ShowGraphContextMenu(QPointF screenPos);
	virtual void                          ShowSelectionContextMenu(QPointF screenPos);

	virtual bool                          PopulateNodeContextMenu(CAbstractNodeItem& node, QMenu& menu)                { return false; }
	virtual bool                          PopulateSelectionContextMenu(const GraphItemSet& selectedItems, QMenu& menu) { return false; }

	virtual bool                          DeleteCustomItem(CAbstractNodeGraphViewModelItem& item)                      { return true; }
	virtual CNodeGraphViewGraphicsWidget* GetCustomWidget(CAbstractNodeGraphViewModelItem& item)                       { return nullptr;  }
	virtual void                          OnRemoveCustomItem(CAbstractNodeGraphViewModelItem& item)                    {}

	QPointF                               GetContextMenuPosition() const                                               { return m_contextMenuScenePos; }
	void                                  SetContextMenuPosition(QPointF sceenPos)                                     { m_contextMenuScenePos = sceenPos; }

	CPinWidget*                           GetPossibleConnectionTarget(QPointF scenePos);

	// Helpers
	CNodeWidget*       GetNodeWidget(const CAbstractNodeItem& node) const;
	CConnectionWidget* GetConnectionWidget(const CAbstractConnectionItem& connection) const;
	CPinWidget*        GetPinWidget(const CAbstractPinItem& pin) const;

protected Q_SLOTS:
	void OnCreateNode(CAbstractNodeItem& node);
	void OnCreateConnection(CAbstractConnectionItem& connection);
	void OnRemoveNode(CAbstractNodeItem& node);
	void OnRemoveConnection(CAbstractConnectionItem& connection);

	void OnContextMenuAddNode(CAbstractDictionaryEntry& entry);
	void OnContextMenuAbort();

private:
	// QGraphicsView
	virtual void customEvent(QEvent* pEvent) final;

	virtual void keyPressEvent(QKeyEvent* pEvent) final;
	virtual void wheelEvent(QWheelEvent* pEvent) final;
	virtual void mouseMoveEvent(QMouseEvent* pEvent) final;
	virtual void mousePressEvent(QMouseEvent* pEvent) final;
	virtual void mouseReleaseEvent(QMouseEvent* pEvent) final;
	// ~QGraphicsView

	// Custom events
	// TODO: Remove protected as soon as we don't need to support MFC anymore.
public:
	bool OnCopyEvent();
	bool OnPasteEvent();
	bool OnDeleteEvent();
	bool OnUndoEvent();
	bool OnRedoEvent();
private:
	// ~TODO

	//
	void ReloadItems();

	void AddNodeItem(CAbstractNodeItem& node);
	void AddConnectionItem(CAbstractConnectionItem& connection);

	void BroadcastSelectionChange(bool forceClear = false);

	void MoveSelection(const QPointF& delta);

	void RemoveNodeItem(CAbstractNodeItem& node);
	void RemoveConnectionItem(CAbstractConnectionItem& connection);

	void BeginCreateConnection(CPinWidget* pPinWidget);
	void EndCreateConnection(CPinWidget* pPinWidget);

	// Scene navigation
	void        BeginSceneDragging(QMouseEvent* pEvent);
	void        EndSceneDragging(QMouseEvent* pEvent);
	bool        RequestContextMenu(QMouseEvent* pEvent);

	static bool IsDraggingButton(Qt::MouseButtons buttons)  { return (buttons& Qt::RightButton) != 0; }
	static bool IsSelectionButton(Qt::MouseButtons buttons) { return (buttons& Qt::LeftButton) != 0; }

	bool        IsSelected(const CNodeGraphViewGraphicsWidget* pItem) const;
	bool        IsNodeSelected(const CAbstractNodeItem& node) const;

	bool        IsScreenPositionPresented(const QPoint& screenPos) const;

	// TODO: Get rid of this!
public:
	void RegisterConnectionEventListener(CryGraphEditor::IConnectionEventsListener* pListener);
	void RemoveConnectionEventListener(CryGraphEditor::IConnectionEventsListener* pListener);

	void SendBeginCreateConnection(QWidget* pSender, CryGraphEditor::SBeginCreateConnectionEventArgs& args);
	void SendEndCreateConnection(QWidget* pSender, CryGraphEditor::SEndCreateConnectionEventArgs& args);

private:
	std::set<CryGraphEditor::IConnectionEventsListener*> m_connectionEventListeners;
	// ~TODO

private:
	QString                          m_copyPasteMimeFormat;

	NodeWidgetByItemInstance         m_nodeWidgetByItemInstance;
	ConnectionWidgetByItemInstance   m_connectionWidgetByItemInstance;

	CNodeGraphViewModel*             m_pModel;
	const CNodeGraphViewStyle*       m_pStyle;
	QGraphicsScene*                  m_pScene;
	CNodeGraphViewBackground*        m_pBackground;
	GraphViewWidgetSet               m_selectedWidgets;
	GraphItemSet                     m_selectedItems;

	QPoint                           m_lastMouseActionPressPos;
	CConnectionPoint                 m_mouseConnectionPoint;
	CConnectionWidget*               m_pNewConnectionLine;
	CPinWidget*                      m_pNewConnectionBeginPin;
	CPinWidget*                      m_pNewConnectionPossibleTargetPin;
	CGroupWidget*                    m_pSelectionBox;

	QSharedPointer<QTrackingTooltip> m_pTooltip;
	QTimer                           m_toolTipTimer;

	QPointF                          m_contextMenuScenePos;
	QPopupWidget*                    m_pContextMenu;
	CDictionaryWidget*               m_pContextMenuContent;
	bool                             m_isRecodringUndo;
	bool                             m_isTooltipActive;

	EOperation                       m_operation;
	uint32                           m_action;
	qreal                            m_currentScaleFactor;
	int32                            m_currentScalePercent;
};

EDITOR_COMMON_API QGraphicsProxyWidget* AddWidget(QGraphicsScene* pScene, QWidget* pWidget);

}

