// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryGraphEditor.h"

#include "NodeGraphViewGraphicsWidget.h"
#include "AbstractPinItem.h"

#include <QColor>
#include <QString>
#include <QPointF>
#include <QLabel>
#include <QPixmap>
#include <QRectF>

class QGraphicsLinearLayout;
class QLabel;
class QGraphicsPixmapItem;
class QPixmap;

class CryIcon;

namespace CryGraphEditor {

class CNodeGraphView;
class CPinWidgetStyle;
class CGraphicsSceneIcon;
class CAbstractNodeContentWidget;
class CNodeWidget;

class CPinName;
class CPinWidgetStyle;

struct SNodePinMouseEventArgs {};

class EDITOR_COMMON_API CPinWidget : public CNodeGraphViewGraphicsWidget, public IConnectionEventsListener
{
	Q_OBJECT

public:
	enum EPinIcon : int32
	{
		Icon_Default = 0,
		Icon_Selected,
		Icon_Highlighted,
		Icon_Deactivated,

		Icon_Count
	};

	enum : int32 { Type = eGraphViewWidgetType_PinWidget };

public:
	CPinWidget(CAbstractPinItem& item, CNodeWidget& nodeWidget, CNodeGraphView& view, bool displayPinName = true);

	// CNodeGraphViewGraphicsWidget
	virtual void                             DeleteLater() override;
	virtual int32                            GetType() const override         { return Type; }
	virtual CAbstractNodeGraphViewModelItem* GetAbstractItem() const override { return &m_item; }
	// ~CNodeGraphViewGraphicsWidget

	void              SetIconOffset(float x, float y);
	const QRectF&     GetRect() const       { return m_rect; }

	CAbstractPinItem& GetItem() const       { return m_item; }
	CConnectionPoint& GetConnectionPoint()  { return m_connectionPoint; }
	CNodeWidget&      GetNodeWidget() const { return m_nodeWidget; }

	void              UpdateConnectionPoint();

protected:
	virtual ~CPinWidget();

	virtual void paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget) override;

	virtual void updateGeometry() override;

	//
	void OnIconHoverEnterEvent(CGraphicsSceneIcon& sender, QGraphicsSceneHoverEvent* pEvent);
	void OnIconHoverMoveEvent(CGraphicsSceneIcon& sender, QGraphicsSceneHoverEvent* pEvent);
	void OnIconHoverLeaveEvent(CGraphicsSceneIcon& sender, QGraphicsSceneHoverEvent* pEvent);

	void OnIconMousePressEvent(CGraphicsSceneIcon& sender, QGraphicsSceneMouseEvent* pEvent);
	void OnIconMouseReleaseEvent(CGraphicsSceneIcon& sender, QGraphicsSceneMouseEvent* pEvent);

	void OnNodeSelectionChanged(bool isSelected);
	void OnInvalidated();

	// IConnectionEventsListener
	virtual void OnBeginCreateConnection(QWidget* pSender, SBeginCreateConnectionEventArgs& args) override;
	virtual void OnEndCreateConnection(QWidget* pSender, SEndCreateConnectionEventArgs& args) override;
	// ~IConnectionEventsListener

	void LoadIcons();
	void ReloadIcons();

	void OnStateChanged(bool) { updateGeometry(); }

private:
	void SetNameWidget(CPinName* pWidget);

private:
	const CNodePinWidgetStyle* m_pStyle;

	CAbstractPinItem&          m_item;
	QGraphicsLinearLayout*     m_pLayout;
	QWidget*                   m_pContent;
	QGraphicsProxyWidget*      m_pContentProxy;
	CConnectionPoint           m_connectionPoint;
	CGraphicsSceneIcon*        m_pIcons[StyleState_Count];
	CGraphicsSceneIcon*        m_pActiveIcon;
	CNodeWidget&               m_nodeWidget;
	CPinName*                  m_pName;
	QRectF                     m_rect;

	bool                       m_isConnecting : 1;
	bool                       m_isInput      : 1;
};

}

