// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QTrackingTooltip.h"
#include <QToolTip>
#include <QApplication>
#include <QDesktopWidget>

QSharedPointer<QTrackingTooltip> QTrackingTooltip::m_instance;

QTrackingTooltip::QTrackingTooltip(QWidget* parent)
	: QFrame(parent)
	, m_isTracking(false)
	, m_cursorOffset(0, 0)
	, m_autoHide(true)
{
	setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	setFocusPolicy(Qt::FocusPolicy::NoFocus);
	setAttribute(Qt::WA_ShowWithoutActivating);
	setAttribute(Qt::WA_TransparentForMouseEvents);
}

QTrackingTooltip::~QTrackingTooltip(void)
{
}

void QTrackingTooltip::Show()
{
	raise();
	show();
}

void QTrackingTooltip::ShowTextTooltip(const QString& str, const QPoint& p)
{
	if (m_instance)
	{
		if (!str.isEmpty() && m_instance->SetText(str))
		{
			m_instance->SetPos(p);
			return;
		}
		else
			QTrackingTooltip::HideTooltip();
	}

	if (!str.isEmpty())
	{
		m_instance = QSharedPointer<QTrackingTooltip>(new QTrackingTooltip(), &QObject::deleteLater);
		m_instance->SetText(str);
		m_instance->Show();
		m_instance->SetPos(p);
	}
}

void QTrackingTooltip::ShowTrackingTooltip(QSharedPointer<QTrackingTooltip>& tooltip, const QPoint& cursorOffset)
{
	if (m_instance && m_instance != tooltip)
	{
		QTrackingTooltip::HideTooltip();
	}

	if (tooltip)
	{
		m_instance = tooltip;
		m_instance->Show();

		if (!cursorOffset.isNull())
			m_instance->SetPosCursorOffset(cursorOffset);
		else if (m_instance->GetCursorOffset().isNull())
			m_instance->SetPosCursorOffset(QPoint(25, 25));
		else
			m_instance->SetPosCursorOffset();

		m_instance->m_isTracking = true;
		qApp->installEventFilter(m_instance.data());
	}
}

void QTrackingTooltip::HideTooltip()
{
	if (m_instance)
	{
		qApp->removeEventFilter(m_instance.data());
		m_instance->m_isTracking = false;
		m_instance->setVisible(false);
		m_instance.clear();
	}
}

void QTrackingTooltip::HideTooltip(QTrackingTooltip* tooltip)
{
	if (m_instance == tooltip)
		HideTooltip();
}

bool QTrackingTooltip::SetText(const QString& str)
{
	QLabel* label = findChild<QLabel*>("QTrackingTooltipLabel");
	if (label)
	{
		label->setText(str);
		adjustSize();
		return true;
	}

	if (layout())
		return false;

	label = new QLabel(this, Qt::WindowStaysOnTopHint);
	label->setFrameStyle(QFrame::Panel);
	label->setFont(QToolTip::font());
	label->setPalette(QToolTip::palette());
	label->setText(str);
	label->setObjectName("QTrackingTooltipLabel");
	label->adjustSize();

	auto layout = new QVBoxLayout();
	layout->setMargin(1);
	layout->addWidget(label);
	setLayout(layout);
	return true;
}

bool QTrackingTooltip::SetPixmap(const QPixmap& pixmap)
{
	QLabel* label = findChild<QLabel*>("QTrackingTooltipLabel");
	if (label)
	{
		label->setPixmap(pixmap);
		adjustSize();
		return true;
	}

	if (layout())
		return false;

	label = new QLabel();
	label->setFrameStyle(QFrame::Panel);
	label->setPixmap(pixmap);
	label->setObjectName("QTrackingTooltipLabel");
	label->adjustSize();

	auto layout = new QVBoxLayout();
	layout->setMargin(1);
	layout->addWidget(label);
	setLayout(layout);
	return true;
}

void QTrackingTooltip::SetPos(const QPoint& p)
{
	move(AdjustPosition(p));
}

void QTrackingTooltip::SetPosCursorOffset(const QPoint& p)
{
	if(!p.isNull())
		m_cursorOffset = p;

	SetPos(QCursor::pos() + m_cursorOffset);
}

QPoint QTrackingTooltip::AdjustPosition(QPoint p) const
{
	//The tooltip position may never overlap the cursor (otherwise we have a leave event and the tooltip disappears)
	
	QRect rect = this->rect();
	QRect screen = QApplication::desktop()->screenGeometry(GetToolTipScreen());

	//see qtooltip.cpp
	if (p.x() + rect.width() > screen.x() + screen.width())
		p.rx() -= 4 + rect.width();
	if (p.y() + rect.height() > screen.y() + screen.height())
		p.ry() -= 24 + rect.height();
	if (p.y() < screen.y())
		p.setY(screen.y());
	if (p.x() + rect.width() > screen.x() + screen.width())
		p.setX(screen.x() + screen.width() - rect.width());
	if (p.x() < screen.x())
		p.setX(screen.x());
	if (p.y() + rect.height() > screen.y() + screen.height())
		p.setY(screen.y() + screen.height() - rect.height());

	QRect futureRect(p.x(), p.y(), rect.width(), rect.height());
	const auto cursor = QCursor::pos();
	if (futureRect.contains(cursor))
	{
		//find closest edge
		QPoint offset;

		int diff = cursor.x() - futureRect.left();
		int minDiff = diff;
		offset = QPoint(diff + abs(m_cursorOffset.x()) + 1, 0);

		diff = futureRect.right() - cursor.x();
		if (diff < minDiff)
		{
			minDiff = diff;
			offset = QPoint(-diff - abs(m_cursorOffset.x()) - 1, 0);
		}

		diff = cursor.y() - futureRect.top();
		if (diff < minDiff)
		{
			minDiff = diff;
			offset = QPoint(0, diff + abs(m_cursorOffset.y()) + 1);
		}

		diff = futureRect.bottom() - cursor.y();
		if (diff < minDiff)
		{
			minDiff = diff;
			offset = QPoint(0, -diff - abs(m_cursorOffset.y()) - 1);
		}

		p += offset;

		CRY_ASSERT(!QRect(p.x(), p.y(), rect.width(), rect.height()).contains(cursor));
	}

	return p;
}

int QTrackingTooltip::GetToolTipScreen() const
{
	if (QApplication::desktop()->isVirtualDesktop())
		return QApplication::desktop()->screenNumber(QCursor::pos());
	else
		return QApplication::desktop()->screenNumber(this);
}

//Inspired from qtooltip.cpp
//Note that this code originally had special cases for macOS and QNX.
bool QTrackingTooltip::eventFilter(QObject* object, QEvent* event)
{
	switch (event->type())
	{
	case QEvent::Leave:
	//Note: here we may actually want to start a hide timer instead,
	//as we could still be wanting to display the same tooltip.
	//Not useful yet but will be.
	case QEvent::WindowActivate:
	case QEvent::WindowDeactivate:
	case QEvent::FocusIn:
	case QEvent::FocusOut:
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonRelease:
	case QEvent::MouseButtonDblClick:
	case QEvent::Wheel:
		if (m_autoHide)
		{
			QTrackingTooltip::HideTooltip();
		}
		break;
	case QEvent::MouseMove:
		if (m_isTracking)
		{
			SetPosCursorOffset(m_cursorOffset);
		}
	default:
		break;
	}
	return false;
}

void QTrackingTooltip::resizeEvent(QResizeEvent* resizeEvent)
{
	if (m_isTracking)
	{
		SetPosCursorOffset(m_cursorOffset);
	}
}

