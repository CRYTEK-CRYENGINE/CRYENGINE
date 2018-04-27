// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include <QLineEdit>
#include <QTimer>
#include <QColor>

namespace CryGraphEditor {

class CAbstractNodeItem;
class CNodeWidget;
class CNodeGraphView;

// TODO: Make the base a QGraphicsWidget.
class CNodeNameWidget : public QLineEdit
{
	Q_OBJECT

public:
	CNodeNameWidget(CNodeWidget& nodeWidget);
	~CNodeNameWidget();

	void    TriggerEdit();

	void    SetName(const QString& name);
	QString GetName() const                { text(); }
	void    SetTextColor(QColor textColor) { m_textColor = textColor; }
	void    SetSelectionStyle(bool isSelected);

protected:
	virtual void mouseMoveEvent(QMouseEvent* pEvent) override;
	virtual void mousePressEvent(QMouseEvent* pEvent) override;
	virtual void mouseReleaseEvent(QMouseEvent* pEvent) override;

	virtual void focusInEvent(QFocusEvent* pEvent) override;
	virtual void focusOutEvent(QFocusEvent* pEvent) override;
	virtual void keyPressEvent(QKeyEvent* pKeyEvent) override;

	virtual void paintEvent(QPaintEvent* pPaintEvent) override;

	void         OnEditingFinished();
	void         OnNameChanged();

private:
	CAbstractNodeItem& m_item;
	CNodeGraphView&    m_view;
	QTimer             m_clickTimer;
	QColor             m_bgColor;
	QColor             m_textColor;
	bool               m_hasFocus;
	bool               m_useSelectionStyle;
};

}

