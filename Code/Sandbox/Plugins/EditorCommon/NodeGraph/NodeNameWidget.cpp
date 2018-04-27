// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeNameWidget.h"

#include "NodeWidget.h"
#include "NodeGraphView.h"
#include "NodeGraphViewStyle.h"
#include "NodeHeaderWidgetStyle.h"
#include "IUndoManager.h"
#include "AbstractNodeItem.h"

#include <QtUtil.h>
#include <QMouseEvent>

namespace CryGraphEditor {

CNodeNameWidget::CNodeNameWidget(CNodeWidget& nodeWidget)
	: QLineEdit(nodeWidget.GetItem().GetName())
	, m_item(nodeWidget.GetItem())
	, m_view(nodeWidget.GetView())
	, m_clickTimer(this)
	, m_hasFocus(false)
	, m_useSelectionStyle(false)
{
	m_clickTimer.setSingleShot(true);

	setContextMenuPolicy(Qt::PreventContextMenu);
	setCursor(Qt::ArrowCursor);
	setAutoFillBackground(true);
	setAlignment(Qt::AlignVCenter);

	const CNodeHeaderWidgetStyle* pStyle = &nodeWidget.GetStyle().GetHeaderWidgetStyle();
	setFont(pStyle->GetNameFont());

	m_bgColor = palette().color(backgroundRole());
	m_textColor = pStyle->GetNameColor();
	setStyleSheet(QString("QLineEdit { background-color: rgba(0, 0, 0, 0) }"));

	m_item.SignalNameChanged.Connect(this, &CNodeNameWidget::OnNameChanged);
	QObject::connect(this, &QLineEdit::editingFinished, this, &CNodeNameWidget::OnEditingFinished);
}

CNodeNameWidget::~CNodeNameWidget()
{

}

void CNodeNameWidget::TriggerEdit()
{
	m_hasFocus = true;
	setFocus();
}

void CNodeNameWidget::SetName(const QString& name)
{
	const int32 minNameWidth = fontMetrics().width(m_item.GetName());
	setMinimumWidth(minNameWidth + 24);

	setText(name);
}

void CNodeNameWidget::mouseMoveEvent(QMouseEvent* pEvent)
{
	if (m_hasFocus)
	{
		QLineEdit::mouseMoveEvent(pEvent);
	}
	else
	{
		pEvent->setAccepted(false);
	}
}

void CNodeNameWidget::mousePressEvent(QMouseEvent* pEvent)
{
	if (m_hasFocus)
	{
		QLineEdit::mousePressEvent(pEvent);
	}
	else if (m_item.GetAcceptsRenaming())
	{
		const int remainingTime = m_clickTimer.remainingTime();
		if (remainingTime > 0)
		{
			TriggerEdit();
			m_clickTimer.stop();
			QLineEdit::mousePressEvent(pEvent);
		}
		else
		{
			m_hasFocus = false;

			m_clickTimer.start(QApplication::doubleClickInterval());
			pEvent->setAccepted(false);
		}
	}
	else
	{
		pEvent->setAccepted(false);
	}
}

void CNodeNameWidget::mouseReleaseEvent(QMouseEvent* pEvent)
{
	if (m_hasFocus)
	{
		QLineEdit::mouseReleaseEvent(pEvent);
	}
	else
	{
		pEvent->setAccepted(false);
	}
}

void CNodeNameWidget::focusInEvent(QFocusEvent* pEvent)
{
	if (m_hasFocus)
	{
		selectAll();

		const QString bgColor = QString("%1, %2, %3").arg(m_bgColor.red()).arg(m_bgColor.green()).arg(m_bgColor.blue());
		setStyleSheet(QString("QLineEdit { background-color: rgb(%1); }").arg(bgColor));

		QLineEdit::focusInEvent(pEvent);
	}
	else
	{
		clearFocus();
	}
}

void CNodeNameWidget::focusOutEvent(QFocusEvent* pEvent)
{
	if (m_hasFocus)
	{
		m_hasFocus = false;

		setStyleSheet(QString("QLineEdit { background-color: rgba(0, 0, 0, 0); }"));

		clearFocus();
		QLineEdit::focusOutEvent(pEvent);
	}
}

void CNodeNameWidget::keyPressEvent(QKeyEvent* pKeyEvent)
{
	if (m_hasFocus)
	{
		const int key = pKeyEvent->key();
		switch (key)
		{
		case Qt::Key_Escape:
			setText(m_item.GetName());
		case Qt::Key_Enter:
		case Qt::Key_Return:
			{
				QFocusEvent focusOutEvent(QEvent::FocusOut);
				QApplication::sendEvent(this, &focusOutEvent);
				update();
			}
			return;
		default:
			QLineEdit::keyPressEvent(pKeyEvent);
		}
	}
}

void CNodeNameWidget::paintEvent(QPaintEvent* pPaintEvent)
{
	if (!m_hasFocus)
	{
		if (m_view.GetZoom() >= 40)
		{
			const QRectF geom = rect();

			QPainter painter(this);
			painter.setBrush(Qt::NoBrush);
			painter.setFont(font());
			if (!m_useSelectionStyle)
				painter.setPen(m_textColor);
			else
				painter.setPen(QColor(255, 255, 255));
			painter.drawText(geom, Qt::AlignVCenter, text());
		}
	}
	else
	{
		if (m_view.GetZoom() >= 40)
		{
			QLineEdit::paintEvent(pPaintEvent);
		}
	}
}

void CNodeNameWidget::OnEditingFinished()
{
	m_hasFocus = false;

	GetIEditor()->GetIUndoManager()->Begin();

	const QString oldName = m_item.GetName();
	m_item.SetName(text());
	const QString newName = m_item.GetName();
	if (m_item.GetName() != oldName)
	{
		const string oldNameStr = QtUtil::ToString(oldName);
		const string newNameStr = QtUtil::ToString(newName);

		stack_string desc;
		desc.Format("Undo node renaming from '%s' to '%s'", oldNameStr.c_str(), newNameStr.c_str());
		GetIEditor()->GetIUndoManager()->Accept(desc.c_str());
	}
	else
		GetIEditor()->GetIUndoManager()->Cancel();
}

void CNodeNameWidget::OnNameChanged()
{
	m_hasFocus = false;

	SetName(m_item.GetName());
}

void CNodeNameWidget::SetSelectionStyle(bool isSelected)
{
	m_useSelectionStyle = isSelected;

	update();
}

}

