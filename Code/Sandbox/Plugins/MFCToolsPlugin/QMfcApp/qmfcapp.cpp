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

// Implementation of the QMfcApp classes

#ifdef QT3_SUPPORT
	#undef QT3_SUPPORT
#endif

#ifdef UNICODE
	#undef UNICODE
#endif

#include <StdAfx.h>
#include "qmfcapp.h"

#include <QEventLoop>
#include <QAbstractEventDispatcher>
#include <QWidget>

#ifdef QTWINMIGRATE_WITHMFC
	#include <afxwin.h>
#else
	#include <qt_windows.h>
#endif

#ifdef QTWINMIGRATE_WITHMFC
CWinApp* QMfcApp::mfc_app = 0;
char** QMfcApp::mfc_argv = 0;
int QMfcApp::mfc_argc = 0;
#endif

#if QT_VERSION >= 0x050000
	#define QT_WA(unicode, ansi) unicode

QMfcAppEventFilter::QMfcAppEventFilter() : QAbstractNativeEventFilter()
{
}

bool QMfcAppEventFilter::nativeEventFilter(const QByteArray&, void* message, long* result)
{
	return static_cast<QMfcApp*>(qApp)->winEventFilter((MSG*)message, result);
}
#endif

/*! \class QMfcApp qmfcapp.h
    \brief The QMfcApp class provides merging of the MFC and Qt event loops.

    QMfcApp is responsible for driving both the Qt and MFC event loop.
    It replaces the standard MFC event loop provided by
    CWinApp::Run(), and is used instead of the QApplication parent
    class.

    To replace the MFC event loop reimplement the CWinApp::Run()
    function in the CWinApp subclass usually created by the MFC
    Application Wizard, and use either the static run() function, or
    an instance of QMfcApp created earlier through the static
    instance() function or the constructor.

    The QMfcApp class also provides a static API pluginInstance() that
    drives the Qt event loop when loaded into an MFC or Win32 application.
    This is useful for developing Qt based DLLs or plugins, or if the
    MFC application's event handling can not be modified.
 */

static int modalLoopCount = 0;

HHOOK hhook;
LRESULT CALLBACK QtFilterProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (qApp)
	{
		// don't process deferred-deletes while in a modal loop
		if (modalLoopCount)
			qApp->sendPostedEvents();
		else
			qApp->sendPostedEvents(0, -1);
	}

	return CallNextHookEx(hhook, nCode, wParam, lParam);
}

/*!
    Inform Qt that a modal loop is about to be entered, and that DeferredDelete
    events should not be processed. Call this function before calling Win32
    or MFC functions that enter a modal event loop (i.e. MessageBox).

    This is only required if the Qt UI code hooks into an existing Win32
    event loop using QMfcApp::pluginInstance.

    \sa exitModalLoop()
 */
void QMfcApp::enterModalLoop()
{
	++modalLoopCount;
}

/*!
    Inform Qt that a modal loop has been exited, and that DeferredDelete
    events should not be processed. Call this function after the blocking
    Win32 or MFC function (i.e. MessageBox) returned.

    This is only required if the Qt UI code hooks into an existing Win32
    event loop using QMfcApp::pluginInstance.

    \sa enterModalLoop()
 */
void QMfcApp::exitModalLoop()
{
	--modalLoopCount;
	Q_ASSERT(modalLoopCount >= 0);
}

/*!
    If there is no global QApplication object (i.e. qApp is null) this
    function creates a QApplication instance and returns true;
    otherwise it does nothing and returns false.

    The application installs an event filter that drives the Qt event
    loop while the MFC or Win32 application continues to own the event
    loop.

    Use this static function if the application event loop code can not be
    easily modified, or when developing a plugin or DLL that will be loaded
    into an existing Win32 or MFC application. If \a plugin is non-null then
    the function loads the respective DLL explicitly to avoid unloading from
    memory.

    \code
    BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpvReserved)
    {
   if (dwReason == DLL_PROCESS_ATTACH)
      QMfcApp::pluginInstance(hInstance);

   return TRUE;
    }
    \endcode

    Set \a plugin to 0 when calling this function from within the same executable
    module.

    If this function is used, call enterModalLoop and exitModalLoop whenever you
    call a Win32 or MFC function that opens a local event loop.

    \code
    void Dialog::someSlot()
    {
        QMfcApp::enterModalLoop();
        MessageBox(...);
        QMfcApp::exitModalLoop();
    }
    \endcode
 */
bool QMfcApp::pluginInstance(Qt::HANDLE plugin)
{
	if (qApp)
		return FALSE;

	QT_WA({
		hhook = SetWindowsHookExW(WH_GETMESSAGE, QtFilterProc, 0, GetCurrentThreadId());
	}, {
		hhook = SetWindowsHookExA(WH_GETMESSAGE, QtFilterProc, 0, GetCurrentThreadId());
	});

	int argc = 0;
	(void)new QApplication(argc, 0);

	if (plugin)
	{
		char filename[256];
		if (GetModuleFileNameA((HINSTANCE)plugin, filename, 255))
			LoadLibraryA(filename);
	}

	return TRUE;
}

#if QT_VERSION >= 0x050000
Q_GLOBAL_STATIC(QMfcAppEventFilter, qmfcEventFilter);
#endif

#ifdef QTWINMIGRATE_WITHMFC
/*!
    Runs the event loop for both Qt and the MFC application object \a
    mfcApp, and returns the result. This function calls \c instance()
    if no QApplication object exists and deletes the object it
    created.

    Calling this static function in a reimplementation of
    CWinApp::Run() is the simpliest way to use the QMfcApp class:

    \code
    int MyMfcApp::Run()
    {
        return QMfcApp::run(this);
    }
    \endcode

    Since a QApplication object must exist before Qt widgets can be
    created you cannot use this function if you want to use Qt-based
    user interface elements in, for example, the InitInstance()
    function of CWinApp. In such cases, create an instance of
    QApplication explicitly using instance() or the constructor.

    \sa instance()
 */
int QMfcApp::run(CWinApp* mfcApp)
{
	bool ownInstance = !qApp;
	if (ownInstance)
		instance(mfcApp);
	int result = qApp->exec();

	if (mfcApp)
	{
		int mfcRes = mfcApp->ExitInstance();
		if (mfcRes && !result)
			result = mfcRes;
	}

	if (ownInstance)
		delete qApp;

	return result;
}

/*!
    Creates an instance of QApplication, passing the command line of
    \a mfcApp to the QApplication constructor, and returns the new
    object. The returned object must be destroyed by the caller.

    Use this static function if you want to perform additional
    initializations after creating the application object, or if you
    want to create Qt GUI elements in the InitInstance()
    reimplementation of CWinApp:

    \code
    BOOL MyMfcApp::InitInstance()
    {
   // standard MFC initialization
   // ...

   // This sets the global qApp pointer
   QMfcApp::instance(this);

   // Qt GUI initialization
    }

    BOOL MyMfcApp::Run()
    {
   int result = QMfcApp::run(this);
   delete qApp;
   return result;
    }
    \endcode

    \sa run()
 */
QApplication* QMfcApp::instance(CWinApp* mfcApp)
{
	mfc_app = mfcApp;
	return new QMfcApp(mfcApp, mfc_argc, mfc_argv);
}

static bool qmfc_eventFilter(void* message)
{
	long result = 0;
	return static_cast<QMfcApp*>(qApp)->winEventFilter((MSG*)message, &result);
}

/*!
    Creates an instance of QMfcApp. \a mfcApp must point to the
    existing instance of CWinApp. \a argc and \a argv are passed on
    to the QApplication constructor.

    Use the static function instance() to automatically use the
    command line passed to the CWinApp.

    \code
    QMfcApp *qtApp;

    BOOL MyMfcApp::InitInstance()
    {
   // standard MFC initialization

   int argc = ...
   char **argv = ...

   qtApp = new QMfcApp(this, argc, argv);

   // Qt GUI initialization
    }

    BOOL MyMfcApp::Run()
    {
   int result = qtApp->exec();
   delete qtApp;
   qtApp = 0;

   return result;
    }
    \endcode

    \sa instance() run()
 */
QMfcApp::QMfcApp(CWinApp* mfcApp, int& argc, char** argv)
	: QApplication(argc, argv), idleCount(0), doIdle(FALSE)
{
	mfc_app = mfcApp;
	#if QT_VERSION >= 0x050000
	QAbstractEventDispatcher::instance()->installNativeEventFilter(qmfcEventFilter());
	#else
	QAbstractEventDispatcher::instance()->setEventFilter(qmfc_eventFilter);
	#endif
	setQuitOnLastWindowClosed(false);
}
#endif

QMfcApp::QMfcApp(int& argc, char** argv) : QApplication(argc, argv)
{
#if QT_VERSION >= 0x050000
	QAbstractEventDispatcher::instance()->installNativeEventFilter(qmfcEventFilter());
#endif
}
/*!
    Destroys the QMfcApp object, freeing all allocated resources.
 */
QMfcApp::~QMfcApp()
{
	if (hhook)
	{
		UnhookWindowsHookEx(hhook);
		hhook = 0;
	}

#ifdef QTWINMIGRATE_WITHMFC
	mfc_argc = 0;
	mfc_argv = 0;
	mfc_app = 0;
#endif
}

/*!
    \reimp
 */
bool QMfcApp::winEventFilter(MSG* msg, long* result)
{
	static bool recursion = false;
	if (recursion)
		return false;

	recursion = true;

	QWidget* widget = QWidget::find((WId)msg->hwnd);
	HWND toplevel = 0;
	if (widget)
	{
		HWND parent = msg->hwnd;
		while (parent)
		{
			toplevel = parent;
			parent = GetParent(parent);
		}
		HMENU menu = toplevel ? GetMenu(toplevel) : 0;
		if (menu && GetFocus() == msg->hwnd)
		{
			if (msg->message == WM_SYSKEYUP && msg->wParam == VK_MENU)
			{
				// activate menubar on Alt-up and move focus away
				SetFocus(toplevel);
				SendMessage(toplevel, msg->message, msg->wParam, msg->lParam);
				widget->setFocus();
				recursion = false;
				return TRUE;
			}
			else if (msg->message == WM_SYSKEYDOWN && msg->wParam != VK_MENU)
			{
				SendMessage(toplevel, msg->message, msg->wParam, msg->lParam);
				SendMessage(toplevel, WM_SYSKEYUP, VK_MENU, msg->lParam);
				recursion = false;
				return TRUE;
			}
		}
	}
#ifdef QTWINMIGRATE_WITHMFC
	else if (mfc_app)
	{
		MSG tmp;
		while (doIdle && !PeekMessage(&tmp, 0, 0, 0, PM_NOREMOVE))
		{
			// DIRTY HACK for DEV-1148:
			// We prevent Qt from calling OnIdle(...) which cleans up temporary resources. But
			// XTP caches CDC pointers which would get released while the user is dragging a tool
			// window. So we only allow to release it if the left mouse button is not pressed.
			if (GetKeyState(VK_LBUTTON) != 0 || !mfc_app->OnIdle(idleCount++))
				doIdle = FALSE;
		}
		if (mfc_app->IsIdleMessage(msg))
		{
			doIdle = TRUE;
			idleCount = 0;
		}
	}
	if (mfc_app && mfc_app->PreTranslateMessage(msg))
	{
		recursion = false;
		return TRUE;
	}
#endif

	recursion = false;
#if QT_VERSION < 0x050000
	return QApplication::winEventFilter(msg, result);
#else
	Q_UNUSED(result);
	return false;
#endif
}

