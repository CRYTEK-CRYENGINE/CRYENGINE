// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "QParentWndWidget.h"
#include <QEvent>
#include <QGuiApplication>
#include <QApplication>
#include <QFocusEvent>

#include <qt_windows.h>

#if QT_VERSION >= 0x050000
	#include <QWindow>
#endif
static HWND FindTopmostWindow(HWND child, bool considerWsChild)
{
	if (child == GetDesktopWindow())
		return 0;
	HWND current = child;
	while (GetParent(current) != 0)
	{
		if (considerWsChild && (GetWindowLongW(current, GWL_STYLE) & WS_CHILD) == 0)
			break;
		current = GetParent(current);
	}

	return current;
}

QParentWndWidget::QParentWndWidget(HWND parent)
	: m_parent(parent)
	, m_previousFocus(0)
	, m_modalityRoot(0)
	, m_parentToCenterOn(0)
{
	if (m_parent)
	{
		SetWindowLongA((HWND)winId(), GWL_STYLE, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

#if QT_VERSION >= 0x50000
		QWindow* window = windowHandle();
		window->setProperty("_q_embedded_native_parent_handle", (WId)m_parent);
		SetParent((HWND)winId(), m_parent);
		window->setFlags(Qt::FramelessWindowHint);
#else
		SetParent((HWND)winId(), m_parent);
#endif
		QEvent e(QEvent::EmbeddingControl);
		QApplication::sendEvent(this, &e);
	}

	m_parentToCenterOn = FindTopmostWindow(m_parent, true);
	m_modalityRoot = FindTopmostWindow(m_parent, false);
}

void QParentWndWidget::childEvent(QChildEvent* ev)
{
	QObject* child = ev->child();
	if (child->isWidgetType())
	{
		if (ev->added())
		{
			if (child->isWidgetType())
				child->installEventFilter(this);
		}
		else if (ev->removed() && m_parentWasDisabled)
		{
			m_parentWasDisabled = false;
			EnableWindow(m_modalityRoot, true);
			child->removeEventFilter(this);
		}
	}
	QWidget::childEvent(ev);
}

void QParentWndWidget::show()
{
	if (!m_previousFocus)
		m_previousFocus = ::GetFocus();
	if (!m_previousFocus)
		m_previousFocus = parentWindow();

	QWidget::show();
}
void QParentWndWidget::hide()
{
	QWidget::hide();
}

void QParentWndWidget::center()
{
	const QWidget* child = findChild<QWidget*>();

	RECT rect;
	GetWindowRect(m_parentToCenterOn, &rect);
	setGeometry((rect.right - rect.left) / 2 + rect.left,
	            (rect.bottom - rect.top) / 2 + rect.top, 0, 0);
}

#if QT_VERSION >= 0x50000
bool QParentWndWidget::nativeEvent(const QByteArray&, void* message, long*)
#else
bool QParentWndWidget::winEvent(MSG* msg, long*)
#endif
{

#if QT_VERSION >= 0x50000
	MSG* msg = (MSG*)message;
#endif
	if (msg->message == WM_SETFOCUS)
	{
		Qt::FocusReason reason;
		if (::GetKeyState(VK_LBUTTON) < 0 ||
		    ::GetKeyState(VK_RBUTTON) < 0)
			reason = Qt::MouseFocusReason;
		else if (::GetKeyState(VK_SHIFT) < 0)
			reason = Qt::BacktabFocusReason;
		else
			reason = Qt::TabFocusReason;
		QFocusEvent ev(QEvent::FocusIn, reason);
		QApplication::sendEvent(this, &ev);
	}
	return false;
}

bool QParentWndWidget::eventFilter(QObject* obj, QEvent* ev)
{
	QWidget* widget = (QWidget*)obj;
	switch (ev->type())
	{
	case QEvent::WindowDeactivate:
		{
			if (widget->isModal() && isHidden())
				BringWindowToTop(m_parent);
			break;
		}
	case QEvent::Show:
		{
			if (widget->isWindow())
			{
				if (!m_previousFocus)
					m_previousFocus = ::GetFocus();
				if (!m_previousFocus)
					m_previousFocus = parentWindow();
				hide();
				if (widget->isModal() && !m_parentWasDisabled)
				{
					EnableWindow(m_modalityRoot, false);
					m_parentWasDisabled = true;
				}
			}
			break;
		}
	case QEvent::Hide:
		{
			if (m_parentWasDisabled)
			{
				EnableWindow(m_modalityRoot, true);
				m_parentWasDisabled = false;
			}
			if (m_previousFocus)
				::SetFocus(m_previousFocus);
			else
				::SetFocus(parentWindow());
			if (widget->testAttribute(Qt::WA_DeleteOnClose) && widget->isWindow())
				deleteLater();
			break;
		}
	case QEvent::Close:
		{
			::SetActiveWindow(m_parent);
			if (widget->testAttribute(Qt::WA_DeleteOnClose))
				deleteLater();
			break;
		}
	default:
		break;
	}
	;

	return QWidget::eventFilter(obj, ev);
}

void QParentWndWidget::focusInEvent(QFocusEvent* ev)
{
	QWidget* candidate = this;

	if (ev->reason() == Qt::TabFocusReason || ev->reason() == Qt::BacktabFocusReason)
	{
		while (candidate && (candidate->focusPolicy() & Qt::TabFocus) == 0)
		{
			candidate = candidate->nextInFocusChain();
			if (candidate == this)
				candidate = 0;
		}
		if (candidate)
		{
			candidate->setFocus(ev->reason());
			candidate->setAttribute(Qt::WA_KeyboardFocusChange);
			candidate->window()->setAttribute(Qt::WA_KeyboardFocusChange);
			if (ev->reason() == Qt::BacktabFocusReason)
				QWidget::focusNextPrevChild(false);
		}
	}
}

bool QParentWndWidget::focusNextPrevChild(bool next)
{
	QWidget* current = focusWidget();
	if (next)
	{
		QWidget* nextFocus = current;
		while (true)
		{
			nextFocus = nextFocus->nextInFocusChain();
			if (nextFocus->isWindow())
				break;
			if (nextFocus->focusPolicy() & Qt::TabFocus)
				return QWidget::focusNextPrevChild(true);
		}
	}
	else
	{
		if (!current->isWindow())
		{
			QWidget* nextFocus = current->nextInFocusChain();
			QWidget* prevFocus = 0;
			QWidget* topLevel = 0;
			while (nextFocus != current)
			{
				if ((nextFocus->focusPolicy() & Qt::TabFocus) != 0)
				{
					prevFocus = nextFocus;
					topLevel = 0;
				}
				else if (nextFocus->isWindow())
				{
					topLevel = nextFocus;
				}
				nextFocus = nextFocus->nextInFocusChain();
			}

			if (!topLevel)
				return QWidget::focusNextPrevChild(false);
		}
	}

	::SetFocus(m_parent);
	return true;
}

