// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "QPopupWidget.h"

// Qt
#include <QApplication>
#include <QCursor>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QDesktopWidget>

// EditorCommon
#include "EditorFramework/PersonalizationManager.h"
#include "QtUtil.h"

QPopupWidget::QPopupWidget(const QString& name, QWidget* pContent, QSize sizeHint, bool bFixedSize /* = false*/, QWidget* pParent /* = nullptr*/)
	: QFrame(pParent, Qt::SubWindow | Qt::FramelessWindowHint)
	, m_pContent(pContent)
	, m_pFocusShareWidget(nullptr)
	, m_mousePressPos(0, 0)
	, m_sizeHint(sizeHint)
	, m_borderOffset(4)
	, m_resizeFlags(Origin::Unknown)
	, m_origin(Origin::Unknown)
{
	setObjectName(name);
	setMouseTracking(true);

	if (bFixedSize)
		m_resizeFlags = 0;

	m_pInsetLayout = new QVBoxLayout();
	QWidget* pInsetWidget = new QWidget(this);
	pInsetWidget->setObjectName("InsetBorder");
	pInsetWidget->setLayout(m_pInsetLayout);
	m_pInsetLayout->setMargin(0);
	m_pInsetLayout->setSpacing(0);
	m_pInsetLayout->addWidget(m_pContent);

	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setMargin(0);
	pMainLayout->setSpacing(0);
	pMainLayout->addWidget(pInsetWidget);
	setLayout(pMainLayout);

	// Must enable mouse tracking and an event filter on the content and all it's children
	// So we can unset the cursor when it leaves this widget
	RecursiveEnableMouseTracking(pInsetWidget);

	QVariant lastSizeVar = GetIEditor()->GetPersonalizationManager()->GetProperty(objectName(), "size");
	if (lastSizeVar.isValid())
	{
		QSize lastSize = QtUtil::ToQSize(lastSizeVar);
		resize(lastSize);
	}
}

void QPopupWidget::ShowAt(const QPoint& globalPos, Origin origin /* = Unknown*/)
{
	// Move the Widget after calling show(). Size is otherwise incorrect
	show();

	// Use the desktop widget to get screen size and position
	QDesktopWidget* pDesktopWidget = QApplication::desktop();
	QRect screenRes = pDesktopWidget->screenGeometry(globalPos);

	// Get the screen widget so we can determine the screen's global position and move the screen rect to match
	QWidget* pScreen = pDesktopWidget->screen(pDesktopWidget->screenNumber(globalPos));
	screenRes.moveTopLeft(pScreen->pos());

	// Rects bounds go from 0 to screenRes - 1, get the real bottom-right-most pixel
	int screenRight = screenRes.x() + screenRes.width();
	int screenBottom = screenRes.y() + screenRes.height();

	QPoint bottomRight(globalPos.x() + width(), globalPos.y() + height());

	m_origin = origin;
	if (m_origin == Origin::Unknown)
	{
		m_origin = 0;
		// Make sure that the menu doesn't get shown outside the screen
		if (screenRight < bottomRight.x())
			m_origin |= Origin::Right;
		else
			m_origin |= Origin::Left;

		if (screenBottom < bottomRight.y())
			m_origin |= Origin::Bottom;
		else
			m_origin |= Origin::Top;
	}

	// This will set the correct sizing constraints
	if (m_resizeFlags != Origin::Fixed)
		SetFixedSize(false);

	move(GetSpawnPosition(globalPos));

	raise();
}

QPoint QPopupWidget::GetSpawnPosition(const QPoint& globalPos)
{
	QPoint finalPos(globalPos);
	if (m_origin & Origin::Right)
		finalPos.setX(globalPos.x() - width());

	if (m_origin & Origin::Bottom)
		finalPos.setY(globalPos.y() - height());

	return finalPos;
}

void QPopupWidget::SetContent(QWidget* pContent)
{
	if (m_pContent)
	{
		m_pInsetLayout->removeWidget(m_pContent);
		// Intentional use of delete rather than deleteLater(). If deleteLater() was used, the old content would
		// get focused after being removed from the layout. This was causing the menu to lose focus and hide.
		// Attempted to solve the issue by tracking the old content and preventing the menu hide if the old content
		// got focused, but that just lead to another event that would set focus to null (old content lost focus)
		delete m_pContent;
	}

	m_pContent = pContent;
	m_pInsetLayout->addWidget(m_pContent);
}

void QPopupWidget::OnFocusChanged(QWidget* pOld, QWidget* pNew)
{
	// If the newly focused item is the widget we share focus with, make sure to raise our widget
	// so it stays visible
	if (pNew == m_pFocusShareWidget || (m_pFocusShareWidget && pNew->isAncestorOf(m_pFocusShareWidget) &&
	                                    QApplication::widgetAt(QCursor::pos()) == m_pFocusShareWidget))
	{
		raise();
		return;
	}

	// If any other widget is focused then hide this pop up
	if (pNew != this && pNew != m_pContent && !m_pContent->isAncestorOf(pNew))
	{
		disconnect(qApp, &QApplication::focusChanged, this, &QPopupWidget::OnFocusChanged);
		hide();
	}
}

void QPopupWidget::RecursiveEnableMouseTracking(QWidget* pWidget)
{
	pWidget->setMouseTracking(true);
	pWidget->installEventFilter(this);

	QList<QWidget*> childWidgets = pWidget->findChildren<QWidget*>();
	for (QWidget* pChild : childWidgets)
	{
		RecursiveEnableMouseTracking(pChild);
	}
}

void QPopupWidget::mouseMoveEvent(QMouseEvent* pEvent)
{
	QRect contentBounds = m_pContent->contentsRect();
	QPoint mousePos = pEvent->pos();

	// This should ideally be based off the content's rect rather than a predefined offset
	// but there were issues with content widgets not following the stylesheet's rules
	if (contentBounds.topLeft().isNull())
		contentBounds.adjust(m_borderOffset, m_borderOffset, -m_borderOffset, -m_borderOffset);

	// If dragging on the widget, then resize
	if (pEvent->buttons() & Qt::LeftButton && !m_mousePressPos.isNull())
	{
		QRect currGeometry = m_initialGeometry;
		QPoint delta = m_mousePressPos - mapToGlobal(mousePos);

		// Use the desktop widget to get screen size and position
		QDesktopWidget* pDesktopWidget = QApplication::desktop();
		QRect screenRes = pDesktopWidget->screenGeometry(mapToGlobal(pos()));

		// Get the screen widget so we can determine the screen's global position and move the screen rect to match
		QWidget* pScreen = pDesktopWidget->screen(pDesktopWidget->screenNumber(pos()));
		screenRes.moveTopLeft(pScreen->pos());

		if (m_resizeFlags & Origin::Left)
		{
			int newLeft = currGeometry.left() - delta.x() * m_resizeConstraint.x();
			int newWidth = currGeometry.right() - newLeft;

			// Make sure we don't move the left side more than is allowed or we'll eventually be moving the anchor point
			if (newWidth < minimumSize().width())
				newLeft = pos().x();

			currGeometry.setLeft(newLeft);
		}
		else if (m_resizeFlags & Origin::Right)
		{
			int newRight = currGeometry.right() + delta.x() * m_resizeConstraint.x();

			if (screenRes.right() < newRight)
				newRight = screenRes.right();

			currGeometry.setRight(newRight);
		}

		if (m_resizeFlags & Origin::Top)
		{
			int newTop = currGeometry.top() + delta.y() * m_resizeConstraint.y();
			int newHeight = currGeometry.bottom() - newTop;

			// Make sure we don't move the top side more than is allowed or we'll eventually be moving the anchor point
			if (newHeight < minimumSize().height())
				newTop = pos().y();

			currGeometry.setTop(newTop);
		}
		else if (m_resizeFlags & Origin::Bottom)
		{
			int newBottom = currGeometry.bottom() - delta.y() * m_resizeConstraint.y();

			if (screenRes.bottom() < newBottom)
				newBottom = screenRes.bottom();

			currGeometry.setBottom(newBottom);
		}

		setGeometry(currGeometry);
	}
	else
	{
		// Change cursor to show that it's a resizable frame
		if (m_resizeFlags & Origin::Bottom && mousePos.y() > contentBounds.bottom())
		{
			if (m_resizeFlags & Origin::Left && mousePos.x() < contentBounds.left())
				setCursor(Qt::SizeBDiagCursor);
			else if (m_resizeFlags & Origin::Right && mousePos.x() > contentBounds.right())
				setCursor(Qt::SizeFDiagCursor);
			else
				setCursor(Qt::SizeVerCursor);
		}
		else if (m_resizeFlags & Origin::Top && mousePos.y() < contentBounds.top())
		{
			if (m_resizeFlags & Origin::Left && mousePos.x() < contentBounds.left())
				setCursor(Qt::SizeFDiagCursor);
			else if (m_resizeFlags & Origin::Right && mousePos.x() > contentBounds.right())
				setCursor(Qt::SizeBDiagCursor);
			else
				setCursor(Qt::SizeVerCursor);
		}
		else if (m_resizeFlags & Origin::Left && mousePos.x() < contentBounds.left())
		{
			setCursor(Qt::SizeHorCursor);
		}
		else if (m_resizeFlags & Origin::Right && mousePos.x() > contentBounds.right())
		{
			setCursor(Qt::SizeHorCursor);
		}
		else
		{
			unsetCursor();
		}
	}
}

void QPopupWidget::mousePressEvent(QMouseEvent* pEvent)
{
	QPoint relPos = pEvent->pos();
	m_mousePressPos = mapToGlobal(relPos);
	m_initialGeometry = geometry();
	m_resizeConstraint = QPoint(0, 0);

	// Calculate the resizing constraint based on the edge that was grabbed
	QRect contentBounds = m_pContent->contentsRect();

	// This should ideally be based off the content's rect rather than a predefined offset
	// but there were issues with content widgets not following the stylesheet's rules
	if (contentBounds.topLeft().isNull())
		contentBounds.adjust(m_borderOffset, m_borderOffset, -m_borderOffset, -m_borderOffset);

	if (m_resizeFlags & Origin::Top && relPos.y() < contentBounds.top())
		m_resizeConstraint.setY(-1);
	else if (m_resizeFlags & Origin::Bottom && relPos.y() > contentBounds.bottom())
		m_resizeConstraint.setY(1);

	if (m_resizeFlags & Origin::Left && relPos.x() < contentBounds.left())
		m_resizeConstraint.setX(1);
	else if (m_resizeFlags & Origin::Right && relPos.x() > contentBounds.right())
		m_resizeConstraint.setX(-1);
}

void QPopupWidget::mouseReleaseEvent(QMouseEvent* pEvent)
{
	if (pEvent->button() == Qt::LeftButton && !m_mousePressPos.isNull())
	{
		GetIEditor()->GetPersonalizationManager()->SetProperty(objectName(), "size", QtUtil::ToQVariant(size()));
	}
}

void QPopupWidget::showEvent(QShowEvent* pEvent)
{
	connect(qApp, &QApplication::focusChanged, this, &QPopupWidget::OnFocusChanged);
}

bool QPopupWidget::eventFilter(QObject* pWatched, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::MouseMove)
	{
		QMouseEvent* pMouseEvent = static_cast<QMouseEvent*>(pEvent);
		QWidget* pWidget = static_cast<QWidget*>(pWatched);
		// Get the position relative to this widget
		QPoint relPos = mapFromGlobal(pWidget->mapToGlobal(pMouseEvent->pos()));
		QRect contentBounds = m_pContent->contentsRect();

		// Check if the content bounds intersect the mouse position
		if (contentBounds.contains(relPos))
			setCursor(Qt::ArrowCursor);
	}
	return false;
}

void QPopupWidget::hideEvent(QHideEvent* pEvent)
{
	SignalHide();
	QWidget::hideEvent(pEvent);
}

