/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Solutions component.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Implementation of the QWinWidget classes

#ifdef QT3_SUPPORT
	#undef QT3_SUPPORT
#endif

#ifdef UNICODE
	#undef UNICODE
#endif

#include <StdAfx.h>
#include "qmfcapp.h"

#ifdef QTWINMIGRATE_WITHMFC
	#include <afxwin.h>
#endif

#include <qevent.h>

#include "qwinwidget.h"

#include <qt_windows.h>

#if QT_VERSION >= 0x050000
	#include <QWindow>
//#include <5.2.1/QtGui/qpa/qplatformnativeinterface.h>
	#define QT_WA(unicode, ansi) unicode
#endif

/*!
    \class QWinWidget qwinwidget.h
    \brief The QWinWidget class is a Qt widget that can be child of a
    native Win32 widget.

    The QWinWidget class is the bridge between an existing application
    user interface developed using native Win32 APIs or toolkits like
    MFC, and Qt based GUI elements.

    Using QWinWidget as the parent of QDialogs will ensure that
    modality, placement and stacking works properly throughout the
    entire application. If the child widget is a top level window that
    uses the \c WDestructiveClose flag, QWinWidget will destroy itself
    when the child window closes down.

    Applications moving to Qt can use QWinWidget to add new
    functionality, and gradually replace the existing interface.
 */

/*!
    Creates an instance of QWinWidget. \a hParentWnd is the handle to
    the native Win32 parent. If a \a parent is provided the object is
    owned by that QObject. \a f is passed on to the QWidget constructor.
 */
QWinWidget::QWinWidget(HWND hParentWnd, QObject* parent, Qt::WindowFlags f)
	: QWidget(0, f), hParent(hParentWnd), prevFocus(0), reenable_parent(false)
{
	if (parent)
		QObject::setParent(parent);

	init();
}

#ifdef QTWINMIGRATE_WITHMFC
/*!
    \overload

    Creates an instance of QWinWidget. \a parentWnd is a pointer to an
    MFC window object. If a \a parent is provided the object is owned
    by that QObject. \a f is passed on to the QWidget constructor.
 */
QWinWidget::QWinWidget(CWnd* parentWnd, QObject* parent, Qt::WindowFlags f)
	: QWidget(0, f), hParent(parentWnd ? parentWnd->m_hWnd : 0), prevFocus(0), reenable_parent(false)
{
	if (parent)
		QObject::setParent(parent);

	init();
}
#endif

void QWinWidget::init()
{
	Q_ASSERT(hParent);

	if (hParent)
	{
		// make the widget window style be WS_CHILD so SetParent will work
		QT_WA({
			SetWindowLong((HWND)winId(), GWL_STYLE, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
		}, {
			SetWindowLongA((HWND)winId(), GWL_STYLE, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
		})
#if QT_VERSION >= 0x050000
		QWindow * window = windowHandle();
		window->setProperty("_q_embedded_native_parent_handle", (WId)hParent);
		//HWND h = static_cast<HWND>(QGuiApplication::platformNativeInterface()->nativeResourceForWindow("handle", window));
		HWND h = (HWND)QWidget::winId();
		SetParent(h, hParent);
		window->setFlags(Qt::FramelessWindowHint);
#else
		SetParent(winId(), hParent);
#endif
		QEvent e(QEvent::EmbeddingControl);
		QApplication::sendEvent(this, &e);
	}
}

/*!
    Destroys this object, freeing all allocated resources.
 */
QWinWidget::~QWinWidget()
{
}

/*!
    Returns the handle of the native Win32 parent window.
 */
HWND QWinWidget::parentWindow() const
{
	return hParent;
}

/*!
    \reimp
 */
void QWinWidget::childEvent(QChildEvent* e)
{
	QObject* obj = e->child();
	if (obj->isWidgetType())
	{
		if (e->added())
		{
			if (obj->isWidgetType())
			{
				obj->installEventFilter(this);
			}
		}
		else if (e->removed() && reenable_parent)
		{
			reenable_parent = false;
			EnableWindow(hParent, true);
			obj->removeEventFilter(this);
		}
	}
	QWidget::childEvent(e);
}

/*! \internal */
void QWinWidget::saveFocus()
{
	if (!prevFocus)
		prevFocus = ::GetFocus();
	if (!prevFocus)
		prevFocus = parentWindow();
}

/*!
    Shows this widget. Overrides QWidget::show().

    \sa showCentered()
 */
void QWinWidget::show()
{
	saveFocus();
	QWidget::show();
}

/*!
    Centers this widget over the native parent window. Use this
    function to have Qt toplevel windows (i.e. dialogs) positioned
    correctly over their native parent windows.

    \code
    QWinWidget qwin(hParent);
    qwin.center();

    QMessageBox::information(&qwin, "Caption", "Information Text");
    \endcode

    This will center the message box over the client area of hParent.
 */
void QWinWidget::center()
{
	const QWidget* child = findChild<QWidget*>();
	if (child && !child->isWindow())
	{
		qWarning("QWinWidget::center: Call this function only for QWinWidgets with toplevel children");
	}
	RECT r;
	GetWindowRect(hParent, &r);
	setGeometry((r.right - r.left) / 2 + r.left, (r.bottom - r.top) / 2 + r.top, 0, 0);
}

/*!
    \obsolete

    Call center() instead.
 */
void QWinWidget::showCentered()
{
	center();
	show();
}

/*!
    Sets the focus to the window that had the focus before this widget
    was shown, or if there was no previous window, sets the focus to
    the parent window.
 */
void QWinWidget::resetFocus()
{
	if (prevFocus)
		::SetFocus(prevFocus);
	else
		::SetFocus(parentWindow());
}

/*! \reimp
 */
#if QT_VERSION >= 0x050000
bool QWinWidget::nativeEvent(const QByteArray&, void* message, long*)
#else
bool QWinWidget::winEvent(MSG* msg, long*)
#endif
{
#if QT_VERSION >= 0x050000
	MSG* msg = (MSG*)message;
#endif
	if (msg->message == WM_SETFOCUS)
	{
		Qt::FocusReason reason;
		if (::GetKeyState(VK_LBUTTON) < 0 || ::GetKeyState(VK_RBUTTON) < 0)
			reason = Qt::MouseFocusReason;
		else if (::GetKeyState(VK_SHIFT) < 0)
			reason = Qt::BacktabFocusReason;
		else
			reason = Qt::TabFocusReason;
		QFocusEvent e(QEvent::FocusIn, reason);
		QApplication::sendEvent(this, &e);
	}
	else if (msg->message == WM_KILLFOCUS)
	{
		QWidget* focusedWidget = QApplication::focusWidget();
		if (focusedWidget)
			focusedWidget->clearFocus();
	}

	return false;
}

/*!
    \reimp
 */
bool QWinWidget::eventFilter(QObject* o, QEvent* e)
{
	QWidget* w = (QWidget*)o;

	switch (e->type())
	{
	case QEvent::WindowDeactivate:
		if (w->isModal() && w->isHidden())
			BringWindowToTop(hParent);
		break;

	case QEvent::Hide:
		if (reenable_parent)
		{
			EnableWindow(hParent, true);
			reenable_parent = false;
		}
		resetFocus();
		if (w->testAttribute(Qt::WA_DeleteOnClose) && w->isWindow())
			deleteLater();
		break;

	case QEvent::Show:
		if (w->isWindow())
		{
			saveFocus();
			hide();
			if (w->isModal() && !reenable_parent)
			{
				EnableWindow(hParent, false);
				reenable_parent = true;
			}
		}
		break;

	case QEvent::Close:
		::SetActiveWindow(hParent);
		if (w->testAttribute(Qt::WA_DeleteOnClose))
			deleteLater();
		break;

	default:
		break;
	}

	return QWidget::eventFilter(o, e);
}

/*! \reimp
 */
void QWinWidget::focusInEvent(QFocusEvent* e)
{
	QWidget* candidate = this;

	switch (e->reason())
	{
	case Qt::TabFocusReason:
	case Qt::BacktabFocusReason:
		while (!(candidate->focusPolicy() & Qt::TabFocus))
		{
			candidate = candidate->nextInFocusChain();
			if (candidate == this)
			{
				candidate = 0;
				break;
			}
		}
		if (candidate)
		{
			candidate->setFocus(e->reason());
			if (e->reason() == Qt::BacktabFocusReason || e->reason() == Qt::TabFocusReason)
			{
				candidate->setAttribute(Qt::WA_KeyboardFocusChange);
				candidate->window()->setAttribute(Qt::WA_KeyboardFocusChange);
			}
			if (e->reason() == Qt::BacktabFocusReason)
				QWidget::focusNextPrevChild(false);
		}
		break;
	default:
		break;
	}
}

/*! \reimp
 */
bool QWinWidget::focusNextPrevChild(bool next)
{
	QWidget* curFocus = focusWidget();
	if (!next)
	{
		if (!curFocus->isWindow())
		{
			QWidget* nextFocus = curFocus->nextInFocusChain();
			QWidget* prevFocus = 0;
			QWidget* topLevel = 0;
			while (nextFocus != curFocus)
			{
				if (nextFocus->focusPolicy() & Qt::TabFocus)
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
			{
				return QWidget::focusNextPrevChild(false);
			}
		}
	}
	else
	{
		QWidget* nextFocus = curFocus;
		while (1)
		{
			nextFocus = nextFocus->nextInFocusChain();
			if (nextFocus->isWindow())
				break;
			if (nextFocus->focusPolicy() & Qt::TabFocus)
			{
				return QWidget::focusNextPrevChild(true);
			}
		}
	}

	::SetFocus(hParent);

	return true;
}

