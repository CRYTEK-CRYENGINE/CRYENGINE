// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include "NodeGraphViewGraphicsWidget.h"
#include "AbstractConnectionItem.h"

#include <QPointF>
#include <QPainterPath>
#include <QWidget>

namespace CryGraphEditor {

class CNodeGraphView;
class CPinWidget;
class CConnectionWidgetStyle;

class EDITOR_COMMON_API CConnectionWidget : public CNodeGraphViewGraphicsWidget
{
	Q_OBJECT

public:
	enum : int32 { Type = eGraphViewWidgetType_ConnectionWidget };

public:
	CConnectionWidget(CAbstractConnectionItem* pItem, CNodeGraphView& view);
	~CConnectionWidget();

	// CNodeGraphViewGraphicsWidget
	virtual void                             DeleteLater() override;
	virtual int32                            GetType() const override         { return Type; }
	virtual CAbstractNodeGraphViewModelItem* GetAbstractItem() const override { return m_pItem; }
	// ~CNodeGraphViewGraphicsWidget

	CAbstractConnectionItem* GetItem() const                         { return m_pItem; }
	void                     SetItem(CAbstractConnectionItem* pItem) { m_pItem = pItem; }

	const CConnectionPoint*  GetSourceConnectionPoint() const        { return m_pSourcePoint; }
	void                     SetSourceConnectionPoint(const CConnectionPoint* pPoint);

	const CConnectionPoint*  GetTargetConnectionPoint() const { return m_pTargetPoint; }
	void                     SetTargetConnectionPoint(const CConnectionPoint* pPoint);

	void                     Update();

protected:
	virtual void         paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget) override;
	virtual QRectF       boundingRect() const override;
	virtual void         updateGeometry() override;
	virtual QPainterPath shape() const override;

	virtual void         hoverEnterEvent(QGraphicsSceneHoverEvent* pEvent) override;
	virtual void         hoverMoveEvent(QGraphicsSceneHoverEvent* pEvent) override;
	virtual void         hoverLeaveEvent(QGraphicsSceneHoverEvent* pEvent) override;

	virtual void         mousePressEvent(QGraphicsSceneMouseEvent* pEvent) override;
	virtual void         mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent) override;
	virtual void         mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent) override;

	void                 OnPointPositionChanged();
	void                 UpdatePath();

private:
	CAbstractConnectionItem*      m_pItem;

	const CConnectionPoint*       m_pSourcePoint;
	const CConnectionPoint*       m_pTargetPoint;

	const CConnectionWidgetStyle* m_pStyle;
	QPointF                       m_selectionPoint;
	QPainterPath                  m_selectionShape;
	QPainterPath                  m_path;
};

}

