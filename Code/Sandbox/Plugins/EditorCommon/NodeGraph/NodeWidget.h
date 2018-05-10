// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include "NodeWidgetStyle.h"
#include "NodeGraphViewGraphicsWidget.h"
#include "NodeHeaderWidget.h"

#include "AbstractNodeItem.h"

class QGraphicsLinearLayout;

namespace CryGraphEditor {

class CNodeGraphView;
class CNodeInfoWidget;
class CAbstractNodeContentWidget;

class EDITOR_COMMON_API CNodeWidget : public CNodeGraphViewGraphicsWidget
{
	Q_OBJECT

public:
	enum : int32 { Type = eGraphViewWidgetType_NodeWidget };

public:
	CNodeWidget(CAbstractNodeItem& item, CNodeGraphView& view);

	// CNodeGraphViewGraphicsWidget
	virtual void                             DeleteLater() override;
	virtual int32                            GetType() const override         { return Type; }
	virtual CAbstractNodeGraphViewModelItem* GetAbstractItem() const override { return &m_item; }
	// ~CNodeGraphViewGraphicsWidget

	CAbstractNodeItem&                  GetItem() const  { return m_item; }
	const CNodeWidgetStyle&             GetStyle() const { return *m_pStyle; }

	virtual void                        SetContentWidget(CAbstractNodeContentWidget* pContent);
	virtual void                        DetachContentWidget();
	virtual CAbstractNodeContentWidget* GetContentWidget() const;
	virtual void                        UpdateContentWidget();

	QString                             GetName() const;
	void                                EditName();

	void                                AddHeaderIcon(CNodeHeaderIcon* pHeaderIcon, CNodeHeader::EIconSlot slot);
	void                                SetHeaderNameWidth(int32 width);

Q_SIGNALS:
	void SignalNodeMoved();

protected:
	~CNodeWidget();

	// QGraphicsWidget
	virtual void paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget) override;

	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* pEvent) override;
	virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* pEvent) override;
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* pEvent) override;

	virtual void mousePressEvent(QGraphicsSceneMouseEvent* pEvent) override;
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent) override;
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent) override;

	virtual void moveEvent(QGraphicsSceneMoveEvent* pEvent) override;
	// ~QGraphicsWidget

	virtual void OnItemInvalidated() override;

	void         OnSelectionChanged(bool isSelected);
	void         OnItemValidated(CAbstractNodeItem& item);

private:
	const CNodeWidgetStyle*     m_pStyle;
	CAbstractNodeItem&          m_item;
	QGraphicsLinearLayout*      m_pContentLayout;
	CNodeHeader*                m_pHeader;
	CNodeInfoWidget*            m_pInfoBar;
	CAbstractNodeContentWidget* m_pContent;
};

}

