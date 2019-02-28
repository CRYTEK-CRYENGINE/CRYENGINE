// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include <QVBoxLayout>
#include "QScrollableBox.h"
#include "QCollapsibleFrame.h"

#include "QRollupBar.h"
#include <QSizePolicy>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QStylePainter>

QRollupBar::QRollupBar(QWidget *parent)
	: QWidget(parent)
	, m_pDragWidget(0)
	, m_canReorder(false)
	, m_DragInProgress(false)
	, m_rollupsClosable(false)
{
	setLayout(new QVBoxLayout(this));
	layout()->setContentsMargins(0, 0, 0, 0);
	layout()->setSpacing(0);
	m_pScrollBox = new QScrollableBox(this);
	m_pDelimeter = new QWidget(m_pScrollBox);
	m_pDelimeter->setObjectName("DropDelimeter");
	m_pDelimeter->setStyleSheet("background-color:blue;");
	m_pDelimeter->setVisible(false);
	layout()->addWidget(m_pScrollBox);
	
}

QRollupBar::~QRollupBar()
{
}

int QRollupBar::addWidget(QWidget * widget)
{
	return attachNewWidget(widget);
}

int QRollupBar::attachNewWidget(QWidget * widget, int index /*= -1*/)
{
	QCollapsibleFrame* newFrame = new QCollapsibleFrame(widget->windowTitle(), m_pScrollBox);
	newFrame->SetWidget(widget);
	newFrame->SetClosable(m_rollupsClosable);
	int newIndex = index;
	if (index < 0)
	{
		m_pScrollBox->addWidget(newFrame);
		m_subFrames.append(newFrame);
		newIndex = m_subFrames.length() - 1;
	}
	else
	{
		m_subFrames.insert(index, newFrame);
		m_pScrollBox->insertWidget(index, newFrame);
	}

	QWidget* dragHandler = newFrame->GetDragHandler();
	m_childRollups.insert(dragHandler, newFrame);
	dragHandler->installEventFilter(this);
	connect(newFrame, SIGNAL(CloseRequested(QCollapsibleFrame*)), this, SLOT(OnRollupCloseRequested(QCollapsibleFrame*)));
	return newIndex;
}


void QRollupBar::insertWidget(QWidget * widget, int index)
{
	attachNewWidget(widget, index);
}

void QRollupBar::removeWidget(QWidget* widget)
{
	int index = indexOf(widget);
	removeAt(index);
}

void QRollupBar::removeAt(int index)
{
	if (index >= 0 && m_subFrames.count() > index)
	{
		m_pScrollBox->removeWidget(m_subFrames[index]);
		m_childRollups.remove(m_subFrames[index]->GetDragHandler());
		m_subFrames[index]->close();
		m_subFrames.removeAt(index);

	}
		
}

QCollapsibleFrame* QRollupBar::rollupAt(int index) const
{
	if (index >= m_subFrames.count())
		return nullptr;
	return m_subFrames[index];
}

QCollapsibleFrame* QRollupBar::rollupAt(QPoint pos) const
{
	int index = rollupIndexAt(pos);
	if (index > -1)
	{
		return m_subFrames[index];
	}
	else
	{
		return m_subFrames.last();
	}
}

int QRollupBar::rollupIndexAt(QPoint pos) const
{
	for (int i = 0; i < m_subFrames.length(); i++)
	{
		QCollapsibleFrame* var = m_subFrames[i];
		QRollupBar* current = const_cast<QRollupBar*>(this);
		int framePos = var->mapTo(current, QPoint(0, 0)).y() + var->height();
		if (pos.y() < framePos)
		{
			return i;
		}
	}
	return -1;
}


void QRollupBar::OnRollupCloseRequested(QCollapsibleFrame* frame)
{
	RollupCloseRequested(m_subFrames.indexOf(frame));
}

int QRollupBar::indexOf(QWidget* widget) const
{
	for (int i = 0; i < m_subFrames.length(); i++)
	{
		QCollapsibleFrame* current = m_subFrames[i];
		if (current->GetWidget() == widget)
		{
			return i;
		}
	}
	return -1;
}

int QRollupBar::count() const
{
	return m_subFrames.count();
}

void QRollupBar::clear()
{
	while (count())
		removeAt(0);
}

bool QRollupBar::isDragHandle(QWidget* w)
{
	return m_childRollups.contains(w);
}

QWidget* QRollupBar::getDragHandleAt(int index)
{
	return m_subFrames[index]->GetDragHandler();
}

QWidget* QRollupBar::getDragHandle(QWidget* w)
{
	int index = indexOf(w);
	if (index >= 0)
		return m_subFrames[index]->GetDragHandler();
	return nullptr;
}

QWidget* QRollupBar::widget(int index) const
{
	if (index >= m_subFrames.count())
		return nullptr;
	return m_subFrames[index]->GetWidget();
}

void QRollupBar::SetRollupsClosable(bool closable)
{
	m_rollupsClosable = closable;
	for each (auto var in m_subFrames)
	{
		var->SetClosable(closable);
	}
}

void QRollupBar::SetRollupsReorderable(bool reorderable)
{
	m_canReorder = reorderable;
}

QWidget* QRollupBar::getDropTarget() const
{
	return m_pScrollBox;
}

bool QRollupBar::eventFilter(QObject * o, QEvent * e)
{
	if (e->type() == QEvent::MouseButtonPress)
	{	
		if (m_canReorder)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(e);
			if (me->button() == Qt::LeftButton)
			{
				if (m_childRollups.contains(static_cast<QWidget*>(o)))
				{
					m_dragStartPosition = me->pos();
				}
			}
		}
		
		
	}
	else if (e->type() == QEvent::MouseMove)
	{
		QMouseEvent* me = static_cast<QMouseEvent*>(e);
		if (me->buttons() & Qt::LeftButton)
		{
			if (m_canReorder)
			{
				if (!m_DragInProgress && !m_dragStartPosition.isNull() && (me->pos() - m_dragStartPosition).manhattanLength() > QApplication::startDragDistance())
				{
					if (!m_pDragWidget)
					{
						m_pDragWidget = new QWidget();
						m_pDragWidget->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
					}
					QWidget* dragWidget = static_cast<QWidget*>(o);

					QRect grabRect(dragWidget->rect());

					QPixmap grabImage(grabRect.size());
					grabImage.fill(Qt::transparent);
					dragWidget->render(&grabImage);

					QPalette pal;
					pal.setBrush(QPalette::All, QPalette::Window, grabImage);
					m_pDragWidget->setPalette(pal);
					m_pDragWidget->setGeometry(grabRect);
					m_pDragWidget->setAutoFillBackground(true);
					m_pDragWidget->raise();
					m_pDragWidget->setVisible(true);
					m_dragStartPosition = dragWidget->mapToGlobal(dragWidget->pos()) - me->globalPos();

					m_pDelimeter->setGeometry(0, 0, rect().width(), 2);
					m_pDelimeter->setVisible(true);
					m_DragInProgress = true;
					m_draggedId = m_subFrames.indexOf(m_childRollups.find(dragWidget).value());
				}

				if (m_DragInProgress)
				{
					m_pDragWidget->move(me->globalPos() + m_dragStartPosition);
					int currentTarget = rollupIndexAt(m_pScrollBox->mapFromGlobal(me->globalPos()));
					if (currentTarget > -1)
					{
						m_pDelimeter->move(QPoint(0, m_subFrames[currentTarget]->pos().y()));
					}
					else
					{
						QWidget* lastItem = m_subFrames.last();
						m_pDelimeter->move(QPoint(0, lastItem->pos().y() + lastItem->height()));
					}
				}
			}
			
		}
	}
	else if (e->type() == QEvent::MouseButtonRelease)
	{
		if (m_DragInProgress)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(e);
			m_pDragWidget->setVisible(false);
			m_pDelimeter->setVisible(false);
			m_dragStartPosition = QPoint();
			int targetPos = rollupIndexAt(m_pScrollBox->mapFromGlobal(me->globalPos()));
			if (targetPos == -1)
				targetPos = m_subFrames.count();
			if (targetPos != m_draggedId && targetPos != m_draggedId + 1)
			{
				if (targetPos > m_draggedId)
				{
					targetPos -= 1;
				}
		
				QCollapsibleFrame* itemToMove = m_subFrames[m_draggedId];
				m_subFrames.removeAt(m_draggedId);
				m_subFrames.insert(targetPos, itemToMove);
				m_pScrollBox->removeWidget(itemToMove);
				m_pScrollBox->insertWidget(targetPos, itemToMove);
			}
			m_DragInProgress = false;
			return true;

		}
		
	}
	return QWidget::eventFilter(o, e);
}



