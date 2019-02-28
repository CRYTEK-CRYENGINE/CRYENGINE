// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include <QGraphicsWidget>

class QGraphicsLinearLayout;

namespace CryGraphEditor {

class CNodeWidget;
class CNodeInfoWidgetStyle;
class CNodeGraphView;

class CNodeInfoWidget : public QGraphicsWidget
{
public:
	CNodeInfoWidget(CNodeWidget& nodeWidget);

	void Update();

protected:
	virtual void paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget) override;

private:
	const CNodeInfoWidgetStyle* m_pStyle;
	CNodeWidget&                m_nodeWidget;
	CNodeGraphView&             m_view;

	bool                        m_showError;
	bool                        m_showWarning;
};

}

