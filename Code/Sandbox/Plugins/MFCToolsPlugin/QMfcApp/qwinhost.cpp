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

// Implementation of the QWinHost classes

#ifdef QT3_SUPPORT
	#undef QT3_SUPPORT
#endif

#include <StdAfx.h>
#include "qwinhost.h"

#include <QEvent>
#include <qt_windows.h>
#include <QApplication>

#if QT_VERSION >= 0x050000
	#define QT_WA(unicode, ansi) unicode
#endif

/*!
    \class QWinHost qwinhost.h
    \brief The QWinHost class provides an API to use native Win32
    windows in Qt applications.

    QWinHost exists to provide a QWidget that can act as a parent for
    any native Win32 control. Since QWinHost is a proper QWidget, it
    can be used as a toplevel widget (e.g. 0 parent) or as a child of
    any other QWidget.

    QWinHost integrates the native control into the Qt user interface,
    e.g. handles focus switches and laying out.

    Applications moving to Qt may have custom Win32 controls that will
    take time to rewrite with Qt. Such applications can use these
    custom controls as children of QWinHost widgets. This allows the
    application's user interface to be replaced gradually.

    When the QWinHost is destroyed, and the Win32 window hasn't been
    set with setWindow(), the window will also be destroyed.
 */

/*!
    Creates an instance of QWinHost. \a parent and \a f are
    passed on to the QWidget constructor. The widget has by default
    no background.

    \warning You cannot change the parent widget of the QWinHost instance
    after the native window has been created, i.e. do not call
    QWidget::setParent or move the QWinHost into a different layout.
 */
QWinHost::QWinHost(QWidget* parent, Qt::WindowFlags f)
	: QWidget(parent, f), wndproc(0), own_hwnd(false), hwnd(0)
{
	setAttribute(Qt::WA_NoBackground);
	setAttribute(Qt::WA_NoSystemBackground);
	setDisabled(true);
}

/*!
    Destroys the QWinHost object. If the hosted Win32 window has not
    been set explicitly using setWindow() the window will be
    destroyed.
 */
QWinHost::~QWinHost()
{
	if (wndproc)
	{
#if defined(GWLP_WNDPROC)
		QT_WA({
			SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)wndproc);
		}, {
			SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)wndproc);
		})
#else
		QT_WA({
			SetWindowLong(hwnd, GWL_WNDPROC, (LONG)wndproc);
		}, {
			SetWindowLongA(hwnd, GWL_WNDPROC, (LONG)wndproc);
		})
#endif
	}

	if (hwnd && own_hwnd)
		DestroyWindow(hwnd);
}

/*!
    Reimplement this virtual function to create and return the native
    Win32 window. \a parent is the handle to this widget, and \a
    instance is the handle to the application instance. The returned HWND
    must be a child of the \a parent HWND.

    The default implementation returns null. The window returned by a
    reimplementation of this function is owned by this QWinHost
    instance and will be destroyed in the destructor.

    This function is called by the implementation of polish() if no
    window has been set explicitly using setWindow(). Call polish() to
    force this function to be called.

    \sa setWindow()
 */
HWND QWinHost::createWindow(HWND parent, HINSTANCE instance)
{
	Q_UNUSED(parent);
	Q_UNUSED(instance);
	return 0;
}

/*!
    Ensures that the window provided a child of this widget, unless
    it is a WS_OVERLAPPED window.
 */
void QWinHost::fixParent()
{
	if (!hwnd)
		return;
	if (!::IsWindow(hwnd))
	{
		hwnd = 0;
		return;
	}
	if (::GetParent(hwnd) == (HWND)winId())
		return;
	long style = GetWindowLong(hwnd, GWL_STYLE);
	if (style & WS_OVERLAPPED)
		return;
	::SetParent(hwnd, (HWND)winId());
}

/*!
    Sets the native Win32 window to \a window. If \a window is not a child
    window of this widget, then it is reparented to become one. If \a window
    is not a child window (i.e. WS_OVERLAPPED is set), then this function does nothing.

    The lifetime of the window handle will be managed by Windows, QWinHost does not
    call DestroyWindow. To verify that the handle is destroyed when expected, handle
    WM_DESTROY in the window procedure.

    \sa window(), createWindow()
 */
void QWinHost::setWindow(HWND window, bool bOwnWindow)
{
	QWinHost* widget = qobject_cast<QWinHost*>(QWidget::find((WId)::GetParent(window)));
	if (widget)
		wndproc = widget->wndproc;

	if (hwnd && own_hwnd)
		DestroyWindow(hwnd);

	hwnd = window;
	fixParent();

	own_hwnd = bOwnWindow;
}

/*!
    Returns the handle to the native Win32 window, or null if no
    window has been set or created yet.

    \sa setWindow(), createWindow()
 */
HWND QWinHost::window() const
{
	return hwnd;
}

void* getWindowProc(QWinHost* host)
{
	return host ? host->wndproc : 0;
}

LRESULT CALLBACK WinHostProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	QWinHost* widget = qobject_cast<QWinHost*>(QWidget::find((WId)::GetParent(hwnd)));
	WNDPROC oldproc = (WNDPROC)getWindowProc(widget);
	if (widget)
	{
		switch (msg)
		{
		case WM_LBUTTONDOWN:
			if (::GetFocus() != hwnd && (widget->focusPolicy() & Qt::ClickFocus))
			{
				widget->setFocus(Qt::MouseFocusReason);
			}
			break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			QT_WA({
				SendMessage((HWND)widget->winId(), msg, wParam, lParam);
			}, {
				SendMessageA((HWND)widget->winId(), msg, wParam, lParam);
			})
			break;

		case WM_KEYDOWN:
			if (wParam == VK_TAB)
			{
				QT_WA({
					SendMessage((HWND)widget->winId(), msg, wParam, lParam);
				}, {
					SendMessageA((HWND)widget->winId(), msg, wParam, lParam);
				})
			}
			break;

		case WM_MOUSEACTIVATE:
			{
				QWidget* focusedWidget = QApplication::focusWidget();
				if (focusedWidget)
					focusedWidget->clearFocus();
			}
			break;

		default:
			break;
		}
	}

	QT_WA({
		if (oldproc)
			return CallWindowProc(oldproc, hwnd, msg, wParam, lParam);
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}, {
		if (oldproc)
			return CallWindowProcA(oldproc, hwnd, msg, wParam, lParam);
		return DefWindowProcA(hwnd, msg, wParam, lParam);
	})
}

/*!
    \reimp
 */
bool QWinHost::event(QEvent* e)
{
	switch (e->type())
	{
	case QEvent::Polish:
		if (!hwnd)
		{
			hwnd = createWindow((HWND)winId(), qWinAppInst());
			fixParent();
			own_hwnd = hwnd != 0;
		}
		if (hwnd && !wndproc && GetParent(hwnd) == (HWND)winId())
		{
#if defined(GWLP_WNDPROC)
			QT_WA({
				void* oldProc = (void*)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
				if (oldProc != WinHostProc)
					wndproc = (void*)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WinHostProc);
			}, {
				void* oldProc = (void*)GetWindowLongPtrA(hwnd, GWLP_WNDPROC);
				if (oldProc != WinHostProc)
					wndproc = (void*)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)WinHostProc);
			})
#else
			QT_WA({
				void* oldProc = (void*)GetWindowLong(hwnd, GWLP_WNDPROC);
				if (oldProc != WinHostProc)
					wndproc = (void*)SetWindowLong(hwnd, GWL_WNDPROC, (LONG)WinHostProc);
			}, {
				void* oldProc = (void*)GetWindowLongA(hwnd, GWL_WNDPROC);
				if (oldProc != WinHostProc)
					wndproc = (void*)SetWindowLongA(hwnd, GWL_WNDPROC, (LONG)WinHostProc);
			})
#endif

			LONG style;
			QT_WA({
				style = GetWindowLong(hwnd, GWL_STYLE);
			}, {
				style = GetWindowLongA(hwnd, GWL_STYLE);
			})
			if (style & WS_TABSTOP)
				setFocusPolicy(Qt::FocusPolicy(focusPolicy() | Qt::StrongFocus));
		}
		break;
	case QEvent::WindowBlocked:
		if (hwnd)
			EnableWindow(hwnd, false);
		break;
	case QEvent::WindowUnblocked:
		if (hwnd)
			EnableWindow(hwnd, true);
		break;
	}
	return QWidget::event(e);
}

/*!
    \reimp
 */
void QWinHost::showEvent(QShowEvent* e)
{
	QWidget::showEvent(e);

	if (hwnd)
		SetWindowPos(hwnd, HWND_TOP, 0, 0, width() * QWidget::window()->devicePixelRatioF(), height() * QWidget::window()->devicePixelRatioF(), SWP_SHOWWINDOW);
}

/*!
    \reimp
 */
void QWinHost::focusInEvent(QFocusEvent* e)
{
	QWidget::focusInEvent(e);

	if (hwnd)
		::SetFocus(hwnd);
}

/*!
    \reimp
 */
void QWinHost::resizeEvent(QResizeEvent* e)
{
	QWidget::resizeEvent(e);

	if (hwnd)
		SetWindowPos(hwnd, HWND_TOP, 0, 0, width() * QWidget::window()->devicePixelRatioF(), height() * QWidget::window()->devicePixelRatioF(), 0);
}

/*!
    \reimp
 */
#if QT_VERSION >= 0x050000
bool QWinHost::nativeEvent(const QByteArray& eventType, void* message, long* result)
#else
bool QWinHost::winEvent(MSG* msg, long* result)
#endif
{
#if QT_VERSION >= 0x050000
	MSG* msg = (MSG*)message;
#endif
	switch (msg->message)
	{
	case WM_SETFOCUS:
		if (hwnd)
		{
			::SetFocus(hwnd);
			return true;
		}
	default:
		break;
	}
#if QT_VERSION >= 0x050000
	return QWidget::nativeEvent(eventType, message, result);
#else
	return QWidget::winEvent(msg, result);
#endif
}

