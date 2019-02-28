// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include <QGraphicsWidget>

class QGraphicsLinearLayout;

namespace CryGraphEditor {

class CNodeWidget;
class CNodeGraphView;
class CNodeNameWidget;
class CNodeHeaderIcon;
class CNodeHeaderWidgetStyle;

class CNodeHeader : public QGraphicsWidget
{
public:
	enum class EIconSlot
	{
		Left,
		Right,
	};

public:
	CNodeHeader(CNodeWidget& nodeWidget);

	QString GetName() const;
	void    SetName(QString name);
	void    EditName();

	void    AddIcon(CNodeHeaderIcon* pHeaderIcon, EIconSlot slot);
	void    SetNameWidth(int32 width);
	void    SetNameColor(QColor color);

	void    OnSelectionChanged(bool isSelected);

protected:
	virtual void paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget) override;

private:
	const CNodeHeaderWidgetStyle* m_pStyle;
	CNodeWidget&                  m_nodeWidget;
	CNodeGraphView&               m_view;
	QGraphicsLinearLayout*        m_pLayout;
	CNodeNameWidget*              m_pName;
};

}

