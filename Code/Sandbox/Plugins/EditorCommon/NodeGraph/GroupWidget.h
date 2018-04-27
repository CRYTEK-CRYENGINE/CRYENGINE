// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "NodeGraphViewGraphicsWidget.h"
#include "EditorCommonAPI.h"

#include "AbstractGroupItem.h"

#include <QString>
#include <QColor>
#include <QRectF>
#include <QPointF>

namespace CryGraphEditor {

class CNodeGraphView;

class EDITOR_COMMON_API CGroupWidget : public CNodeGraphViewGraphicsWidget
{
	Q_OBJECT

public:
	friend class CNodeGraphView;
	enum : int32 { Type = eGraphViewWidgetType_GroupWidget };

public:
	CGroupWidget(CNodeGraphView& view);
	~CGroupWidget();

	// CNodeGraphViewGraphicsWidget
	virtual void                             DeleteLater() override;
	virtual int32                            GetType() const override { return Type; }
	virtual CAbstractNodeGraphViewModelItem* GetAbstractItem() const  { return nullptr; }
	// ~CNodeGraphViewGraphicsWidget

	const QString&            GetText() const               { return m_text; }
	void                      SetText(const QString& text)  { m_text = text; }

	const QColor&             GetColor() const              { return m_color; }
	void                      SetColor(const QColor& color) { m_color = color; }

	const QPointF&            GetAA() const                 { return m_aa; }
	void                      SetAA(const QPointF& positon);

	const QPointF&            GetBB() const { return m_bb; }
	void                      SetBB(const QPointF& positon);

	const QRectF&             GetRect() const  { return m_rect; }
	const GraphViewWidgetSet& GetItems() const { return m_items; }

protected:
	virtual void   paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget) override;
	virtual QRectF boundingRect() const override;
	virtual void   updateGeometry() override;

	virtual void   mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent) override;
	virtual void   mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent) override;

	virtual void   moveEvent(QGraphicsSceneMoveEvent* pEvent) override;

	void           UpdateContainingSceneItems();

private:
	QString            m_text;
	QColor             m_color;

	QPointF            m_aa;
	QPointF            m_bb;

	QRectF             m_rect;
	GraphViewWidgetSet m_items;
};

}

