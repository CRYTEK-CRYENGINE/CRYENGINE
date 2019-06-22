// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "TextWidget.h"
#include "TextWidgetStyle.h"

#include "NodeWidget.h"
#include "NodeGraphView.h"
#include "NodeGraphViewStyle.h"
#include "HeaderWidgetStyle.h"
#include "IUndoManager.h"
#include "AbstractNodeItem.h"

#include <QtUtil.h>
#include <QApplication>
#include <QMouseEvent>
#include <QGraphicsSceneMouseEvent>

#include <Serialization/QPropertyTree/PropertyTree.h>

namespace CryGraphEditor {

CTextWidget::CTextWidget(CNodeGraphViewGraphicsWidget& viewWidget, QGraphicsWidget& parentWidget)
	: m_item(*viewWidget.GetAbstractItem())
	, m_style(viewWidget.GetStyle().GetHeaderTextStyle())
	, m_viewWidget(viewWidget)
	, m_parentWidget(parentWidget)
	, m_useSelectionStyle(false)
	, m_clickTimer(this)	
	, m_multiline(false)
	, m_textWidth(0.0f)
	, m_textHeight(0.0f)
{
	setCursor(Qt::ArrowCursor);
	
	m_textColor = m_style.GetTextColor();
	m_clickTimer.setSingleShot(true);
}

CTextWidget::~CTextWidget()
{	
}

void CTextWidget::TriggerEdit()
{	
	QSize size = m_parentWidget.size().toSize();

	QPoint t0 = m_viewWidget.GetView().mapFromScene(m_parentWidget.sceneBoundingRect().topLeft().toPoint());
	QPoint t1 = m_viewWidget.GetView().mapFromScene(sceneBoundingRect().topLeft().toPoint());

	QPoint delta = t1 - t0;
	QPoint position = m_viewWidget.GetView().viewport()->mapToGlobal(t0);

	if (!m_multiline)
	{
		position = position + delta;
		size = QSize(boundingRect().width(), size.height() - delta.y());
	}

	setFocus();
	m_viewWidget.GetView().ShowContentEditingPopup(*this, position, size, m_text, m_style.GetTextFont(), m_multiline);
}

void CTextWidget::SetText(const QString& text)
{
	m_text = text;
	if (m_multiline)
	{
		QFontMetrics fontMetrics = QFontMetrics(m_style.GetTextFont());
		QRectF boundingBox = boundingRect();
			
		int textWidth = fontMetrics.width(text);
		
		int rows = textWidth / boundingBox.width() + 1;
		int cols = boundingBox.width() / fontMetrics.averageCharWidth() - 1;

		m_textWidth  = fontMetrics.averageCharWidth() * cols;
		m_textHeight = fontMetrics.lineSpacing() * rows;

		m_textToDisplay = text;
		for (int i = 0; i < rows; i++)
		{
			int index = (i + 1)*cols + i;
			m_textToDisplay = m_textToDisplay.insert(index, '\n');
		}
	}
	else
	{
		m_textToDisplay = m_text;
	}
}

void CTextWidget::mousePressEvent(QGraphicsSceneMouseEvent* pEvent)
{
	if (m_item.GetAcceptsRenaming() || m_item.GetAcceptsTextEditing())
	{
		const int remainingTime = m_clickTimer.remainingTime();
		if (remainingTime > 0)
		{
			TriggerEdit();
			m_clickTimer.stop();
		}
		else
		{
			m_clickTimer.start(QApplication::doubleClickInterval());
			pEvent->setAccepted(false);
		}
	}
	else
	{
		pEvent->setAccepted(false);
	}
}

void CTextWidget::paint(QPainter* pPainter, const QStyleOptionGraphicsItem* pOption, QWidget* pWidget)
{
	if (isVisible() && m_viewWidget.GetView().GetZoom() >= 40)
	{
		pPainter->save();

		if (!m_useSelectionStyle)
		{
			pPainter->setPen(m_textColor);
		}
		else
		{
			pPainter->setPen(QColor(255, 255, 255));
		}
		
		pPainter->setFont(m_style.GetTextFont());
		pPainter->drawText(boundingRect(), m_style.GetAlignment(), m_textToDisplay);

		pPainter->setBrush(Qt::NoBrush);
		pPainter->setPen(QColor(255, 0, 0));

		pPainter->restore();
	}
}

void CTextWidget::SetSelectionStyle(bool isSelected)
{
	m_useSelectionStyle = isSelected;
	update();
}

}
