// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QCustomWindowFrame.h"

#include <QPushButton>
#include <QToolButton>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QStyle>
#include <QMouseEvent>
#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

typedef HRESULT(WINAPI *dwmExtendFrameIntoClientArea_t)(HWND hwnd, const MARGINS* pMarInset);

#ifdef UNICODE
#define _UNICODE
#endif
#include <tchar.h>

#if QT_VERSION >= 0x050000
Q_GUI_EXPORT QPixmap qt_pixmapFromWinHICON(HICON icon);
#else
#define qt_pixmapFromWinHICON(hIcon)QPixmap::fromWinHICON(hIcon)
#endif
#endif

QCustomTitleBar::QCustomTitleBar(QWidget* parent)
	: QFrame(parent)
	, m_dragging(false)
	, m_caption(nullptr)
	, m_sysMenuButton(nullptr)
	, m_minimizeButton(nullptr)
	, m_maximizeButton(nullptr)
	, m_closeButton(nullptr)
{
	if (parent->metaObject()->indexOfSignal(QMetaObject::normalizedSignature(SIGNAL("contentsChanged(QWidget*)"))) != -1)
	{
		connect(parent, SIGNAL(contentsChanged(QWidget*)), this, SLOT(onFrameContentsChanged(QWidget*)));
	}

	QHBoxLayout* myLayout = new QHBoxLayout(this);
	myLayout->setContentsMargins(4, 4, 4, 0);
	myLayout->setSpacing(0);
	m_caption = new QLabel(this);
	
	m_caption->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_sysMenuButton = new QToolButton(this);
	onIconChange();
	m_sysMenuButton->setObjectName("sysMenu");
	m_sysMenuButton->setFocusPolicy(Qt::NoFocus);
	m_sysMenuButton->installEventFilter(this);
	myLayout->addWidget(m_sysMenuButton);
	myLayout->addWidget(m_caption, 1);

	m_minimizeButton = new QToolButton(this);
	m_minimizeButton->setObjectName("minimizeButton");
	m_minimizeButton->setFocusPolicy(Qt::NoFocus);
	connect(m_minimizeButton, SIGNAL(clicked()), parent, SLOT(showMinimized()));
	myLayout->addWidget(m_minimizeButton);

	m_maximizeButton = new QToolButton(this);
	m_maximizeButton->setObjectName("maximizeButton");
	m_maximizeButton->setFocusPolicy(Qt::NoFocus);
	connect(m_maximizeButton, SIGNAL(clicked()), this, SLOT(toggleMaximizedParent()));
	myLayout->addWidget(m_maximizeButton);

	m_closeButton = new QToolButton(this);
	m_closeButton->setObjectName("closeButton");
	m_closeButton->setFocusPolicy(Qt::NoFocus);
	connect(m_closeButton, SIGNAL(clicked()), parent, SLOT(close()));
	myLayout->addWidget(m_closeButton);

#if QT_VERSION < 0x050000 // Qt 4 does not have signals for icon and title changes, so need to use the event filter.
	parent->installEventFilter(this);
#else
	connect(parent, SIGNAL(windowTitleChanged(const QString &)), m_caption, SLOT(setText(const QString &)));
	connect(parent, SIGNAL(windowIconChanged(const QIcon &)), this, SLOT(onIconChange()));
#endif

	onFrameContentsChanged(parent);
}

QCustomTitleBar::~QCustomTitleBar()
{
}

void QCustomTitleBar::updateWindowStateButtons()
{
	if (parentWidget()->windowState() != Qt::WindowMaximized)
	{
		layout()->setContentsMargins(4, 4, 4, 0);
	}
	else
	{
		layout()->setContentsMargins(0, 0, 0, 0);
	}

	if (m_maximizeButton)
	{
		if (parentWidget()->windowState() != Qt::WindowMaximized)
		{
			m_maximizeButton->setObjectName("maximizeButton");
		}
		else
		{
			m_maximizeButton->setObjectName("restoreButton");
		}
		style()->unpolish(m_maximizeButton);
		style()->polish(m_maximizeButton);
	}
}

void QCustomTitleBar::setActive(bool active)
{
	m_caption->setObjectName(active ? "active" : "inactive");
	style()->unpolish(m_caption);
	style()->polish(m_caption);
}

void QCustomTitleBar::toggleMaximizedParent()
{
	if (parentWidget()->windowState() == Qt::WindowMaximized)
	{
		parentWidget()->showNormal();
	}
	else
	{
		parentWidget()->showMaximized();
	}
	updateWindowStateButtons();
}

void QCustomTitleBar::showSystemMenu(QPoint p)
{
#if defined(WIN32) || defined(WIN64)

	QWidget* screen = qApp->desktop()->screen(qApp->desktop()->screenNumber(p));
	p = screen->mapFromGlobal(p);

#if QT_VERSION < 0x050600
	const float pixelRatio = 1;
#else
	const float pixelRatio = screen->devicePixelRatioF();
#endif

	p.setX(p.x() * pixelRatio);
	p.setY(p.y() * pixelRatio);
	p = screen->mapToGlobal(p);

	HWND hWnd = (HWND)parentWidget()->winId();
	HMENU hMenu = GetSystemMenu(hWnd, FALSE);

	if (hMenu != NULL)
	{
		if (hMenu)
		{
			MENUITEMINFO mii;
			mii.cbSize = sizeof(MENUITEMINFO);
			mii.fMask = MIIM_STATE;
			mii.fType = 0;

			mii.fState = MF_ENABLED;
			SetMenuItemInfo(hMenu, SC_RESTORE, FALSE, &mii);
			SetMenuItemInfo(hMenu, SC_SIZE, FALSE, &mii);
			SetMenuItemInfo(hMenu, SC_MOVE, FALSE, &mii);
			SetMenuItemInfo(hMenu, SC_MAXIMIZE, FALSE, &mii);
			SetMenuItemInfo(hMenu, SC_MINIMIZE, FALSE, &mii);

			mii.fState = MF_GRAYED;
			switch (parentWidget()->windowState())
			{
			case Qt::WindowMaximized:
				SetMenuItemInfo(hMenu, SC_SIZE, FALSE, &mii);
				SetMenuItemInfo(hMenu, SC_MOVE, FALSE, &mii);
				SetMenuItemInfo(hMenu, SC_MAXIMIZE, FALSE, &mii);
				SetMenuDefaultItem(hMenu, SC_CLOSE, FALSE);
				break;
			case Qt::WindowMinimized:
				SetMenuItemInfo(hMenu, SC_MINIMIZE, FALSE, &mii);
				SetMenuDefaultItem(hMenu, SC_RESTORE, FALSE);
				break;
			case Qt::WindowNoState:
				SetMenuItemInfo(hMenu, SC_RESTORE, FALSE, &mii);
				SetMenuDefaultItem(hMenu, SC_CLOSE, FALSE);
				break;
			}

			if (!(parentWidget()->windowFlags() & Qt::WindowMinimizeButtonHint))
			{
				SetMenuItemInfo(hMenu, SC_MINIMIZE, FALSE, &mii);
			}

			LPARAM cmd = TrackPopupMenu(hMenu, (TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD), p.x(), p.y(), NULL, hWnd, NULL);

			if (cmd)
			{
				PostMessage(hWnd, WM_SYSCOMMAND, cmd, 0);
			}
		}
	}
#endif
}

void QCustomTitleBar::onIconChange()
{
	QIcon icon = parentWidget()->windowIcon();
	if (!icon.data_ptr())
	{
		icon = qApp->windowIcon();
		if (!icon.data_ptr())
		{
#if defined(WIN32) || defined(WIN64)
			HICON hIcon = (HICON)GetClassLongPtr((HWND)winId(), GCLP_HICON);
			icon = qt_pixmapFromWinHICON(hIcon);
#endif
		}
	}
	m_sysMenuButton->setIcon(icon);
}

#if QT_VERSION < 0x050000 && (defined(WIN32) || defined(WIN64)) // Hack, because Qt4 doesn't seem to send the mouseReleaseEvent if release capture has been called.
QCustomTitleBar* draggedTitleBar = nullptr;
extern HHOOK hHook;

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (wParam == WM_LBUTTONUP && draggedTitleBar)
	{
		QMouseEvent* releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, QPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), Qt::LeftButton, Qt::LeftButton, 0);
		CALL_PROTECTED_VOID_METHOD_1ARG(QCustomTitleBar, draggedTitleBar, mouseReleaseEvent, QMouseEvent*, releaseEvent);
		draggedTitleBar = nullptr;
		UnhookWindowsHookEx(hHook);
		return false;
	}

	return CallNextHookEx(hHook, nCode, wParam, lParam);
}

HHOOK hHook = nullptr;
#endif

void QCustomTitleBar::onBeginDrag()
{
	m_dragging = true;
#if defined(WIN32) || defined(WIN64)
	ReleaseCapture();
#if QT_VERSION < 0x050000
	draggedTitleBar = this;
	hHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, 0);
#endif
	SendMessage((HWND)parentWidget()->winId(), WM_NCLBUTTONDOWN, HTCAPTION, 0);
#endif
}

void QCustomTitleBar::onFrameContentsChanged(QWidget* newContents)
{
	m_minimizeButton->setVisible(parentWidget()->windowFlags() & Qt::WindowMinimizeButtonHint || parentWidget()->windowFlags() & Qt::MSWindowsFixedSizeDialogHint);
	m_maximizeButton->setVisible(parentWidget()->windowFlags() & Qt::WindowMaximizeButtonHint || parentWidget()->windowFlags() & Qt::MSWindowsFixedSizeDialogHint);
	m_closeButton->setVisible(parentWidget()->windowFlags() & Qt::WindowCloseButtonHint);

	QString winTitle = parentWidget()->windowTitle();
	if (!winTitle.size())
	{
#if (defined(WIN32) || defined(WIN64)) && QT_VERSION >= 0x050000
		TCHAR buffer[1000];
		GetWindowText((HWND)parentWidget()->winId(), buffer, 1000);
#ifdef UNICODE
		winTitle = QString::fromWCharArray(buffer);
#else
		winTitle = QString::fromLocal8Bit(buffer);
#endif
#endif
	}
	m_caption->setText(winTitle);
}

void QCustomTitleBar::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton && qApp->widgetAt(QCursor::pos()) == m_caption)
	{
#if QT_VERSION < 0x050000 // Qt 4 doesn't like having ReleaseCapture() called in the mouse press event, so use a single shot timer to call it after.
		QTimer::singleShot(0, this, SLOT(onBeginDrag()));
#else
		onBeginDrag();
#endif
	}

	QFrame::mousePressEvent(event);
}

void QCustomTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
	if (!m_dragging)
	{
		if (event->button() == Qt::RightButton && rect().contains(mapFromGlobal(QCursor::pos())))
		{
			event->accept();
			showSystemMenu(QCursor::pos());
		}
		else
		{
			event->ignore();
		}
		return;
	}
	m_dragging = false;

	QFrame::mouseReleaseEvent(event);
}

bool QCustomTitleBar::eventFilter(QObject* o, QEvent* e)
{
#if QT_VERSION < 0x050000 // Qt 4 does not have signals for icon and title changes, so need to use the event filter.
	if (o == parentWidget())
	{
		if (e->type() == QEvent::WindowIconChange || e->type() == QEvent::ApplicationWindowIconChange)
		{
			onIconChange();
		}
		else if (e->type() == QEvent::WindowTitleChange)
		{
			m_caption->setText(parentWidget()->windowTitle());
		}
	}
#endif

	if (o == m_sysMenuButton)
	{
		switch (e->type())
		{
		case QEvent::MouseButtonPress:
			if (static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton && qApp->widgetAt(QCursor::pos()) == m_sysMenuButton)
			{
				showSystemMenu(mapToGlobal(rect().bottomLeft()));
				return true;
			}
		case QEvent::MouseButtonDblClick:
			if (static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton && qApp->widgetAt(QCursor::pos()) == m_sysMenuButton)
			{
				parentWidget()->close();
				return true;
			}
		}
	}

	return QFrame::eventFilter(o, e);
}

#if QT_VERSION >= 0x050000
bool QCustomTitleBar::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#if defined(WIN32) || defined(WIN64)
	MSG* msg = reinterpret_cast<MSG*>(message);
	if (winEvent(msg, result))
		return true;
#endif
	return QFrame::nativeEvent(eventType, message, result);
}
#endif

#if defined(WIN32) || defined(WIN64)
bool QCustomTitleBar::winEvent(MSG *msg, long *result)
{
	if (msg->message == WM_NCHITTEST)
	{
		QPoint pos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
		QRegion children = childrenRegion();
		if (!children.contains(mapFromGlobal(pos)))
		{
			*result = HTTRANSPARENT;
			return true;
		}
	}
	if (msg->message == WM_LBUTTONDBLCLK)
	{
		QPoint pos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
		QRegion children = childrenRegion() - QRegion(m_caption->geometry());
		if (!children.contains(pos))
		{
			toggleMaximizedParent();
			*result = 0;
			return true;
		}
		// Qt 5 does not send a MouseButtonDblClick event through the event filter for the system menu button, so send it manually
#if QT_VERSION >= 0x050000
		else if (m_sysMenuButton->geometry().contains(pos))
		{
			QMouseEvent me(QEvent::MouseButtonDblClick, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
			QApplication::sendEvent(m_sysMenuButton, &me);
			return true;
		}
#endif
	}
#if QT_VERSION < 0x050000
	return QFrame::winEvent(msg, result);
#else
	return false;
#endif
}
#endif

QCustomWindowFrame* QCustomWindowFrame::wrapWidget(QWidget* w)
{
	QCustomWindowFrame* windowFrame = new QCustomWindowFrame();
	windowFrame->internalSetContents(w);
	return windowFrame;
}

QCustomWindowFrame::QCustomWindowFrame()
	: QFrame(nullptr)
	, m_titleBar(nullptr)
	, m_contents(nullptr)
{
	setMouseTracking(true);
	m_grid = new QGridLayout(this);
	m_grid->setSpacing(0);		
	m_grid->setMargin(0);
	m_grid->setRowMinimumHeight(0, 4);
	m_grid->setRowMinimumHeight(2, 4);
	m_grid->setColumnMinimumWidth(0, 4);
	m_grid->setColumnMinimumWidth(2, 4);
	setLayout(m_grid);

#if defined(WIN32) || defined(WIN64)
	SetWindowLong((HWND)winId(), GWL_STYLE, WS_OVERLAPPED | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SYSMENU);
	m_dwm = (void*)LoadLibraryEx(_T("dwmapi.dll"), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (m_dwm)
	{
		dwmExtendFrameIntoClientArea = (void*)GetProcAddress((HMODULE)m_dwm, "DwmExtendFrameIntoClientArea");
	}
	else
	{
		dwmExtendFrameIntoClientArea = nullptr;
	}

	createWinId();
#endif

#if QT_VERSION < 0x050000
	// HACK: Moving the window slightly seems to fix the frame on Qt4
	QTimer::singleShot(0, this, SLOT(nudgeWindow()));
#endif
}

QCustomWindowFrame::~QCustomWindowFrame()
{
#if (defined(_WIN32) || defined(_WIN64))
	dwmExtendFrameIntoClientArea = nullptr;
	if (m_dwm)
	{
		FreeLibrary((HMODULE)m_dwm);
	}
#endif
}

void QCustomWindowFrame::internalSetContents(QWidget* widget, bool useContentsGeometry)
{
	if (m_contents)
	{
		if (m_contents->parentWidget() == this)
			m_contents->setParent(nullptr);

		m_grid->removeWidget(m_contents);
		m_grid->removeWidget(m_titleBar);
		m_contents->removeEventFilter(this);
#if QT_VERSION >= 0x050000
		disconnect(m_contents, SIGNAL(windowTitleChanged(const QString &)), this, SLOT(setWindowTitle(const QString &)));
		disconnect(m_contents, SIGNAL(windowIconChanged(const QIcon &)), this, SLOT(onIconChange()));
#endif
	}

	m_contents = widget;

	if (m_contents)
	{
		m_contents->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

		if (m_contents->testAttribute(Qt::WA_QuitOnClose))
		{
			m_contents->setAttribute(Qt::WA_QuitOnClose, false);
			setAttribute(Qt::WA_QuitOnClose);
		}

		setAttribute(Qt::WA_DeleteOnClose, m_contents->testAttribute(Qt::WA_DeleteOnClose));

		setWindowTitle(m_contents->windowTitle());
		setWindowIcon(m_contents->windowIcon());

		m_contents->installEventFilter(this);

#if QT_VERSION >= 0x050000 // Qt 4 does not have signals for icon and title changes, so need to use the event filter.
		connect(m_contents, SIGNAL(windowTitleChanged(const QString &)), this, SLOT(setWindowTitle(const QString &)));
		connect(m_contents, SIGNAL(windowIconChanged(const QIcon &)), this, SLOT(onIconChange()));
#endif
		if (useContentsGeometry)
		{
			m_contents->show();
			setGeometry(m_contents->geometry());
			m_contents->setParent(this);
		}
		else
		{
			m_contents->setParent(this);
			m_contents->show();
		}

		updateWindowFlags();
		ensureTitleBar();

		m_grid->addWidget(m_titleBar, 0, 0, 1, 3);
		m_grid->addWidget(m_contents, 1, 1);
	}
	
	contentsChanged(m_contents);
}

void QCustomWindowFrame::ensureTitleBar()
{
	if (!m_titleBar)
	{
		m_titleBar = new QCustomTitleBar(this);
	}
}

#if QT_VERSION >= 0x050000
bool QCustomWindowFrame::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
	if (!m_titleBar)
		return false;

#if defined(WIN32) || defined(WIN64)
	MSG* msg = reinterpret_cast<MSG*>(message);
	if (winEvent(msg, result))
		return true;
#endif
	return QFrame::nativeEvent(eventType, message, result);
}
#endif

#if defined(WIN32) || defined(WIN64)
bool QCustomWindowFrame::winEvent(MSG *msg, long *result)
{
	if (!m_titleBar)
		return false;

	QRect r = rect();
	static const int SIZE_MARGIN = 4;
	QPoint newPos = QCursor::pos();
#if QT_VERSION < 0x050000
	RECT rcWind;
	GetWindowRect((HWND)winId(), &rcWind);
	r.moveTo(QPoint(rcWind.left, rcWind.top));
#else
	r.moveTo(pos());
#endif
	bool m_topEdge, m_leftEdge, m_rightEdge, m_bottomEdge;
	m_topEdge = qAbs(newPos.y() - r.top()) <= SIZE_MARGIN;
	m_leftEdge = qAbs(newPos.x() - r.left()) <= SIZE_MARGIN;
	m_rightEdge = qAbs(newPos.x() - r.right()) <= SIZE_MARGIN;
	m_bottomEdge = qAbs(newPos.y() - r.bottom()) <= SIZE_MARGIN;
	bool hor = m_leftEdge || m_rightEdge;
	bool ver = m_topEdge || m_bottomEdge;
	switch (msg->message)
	{
	case WM_SIZE:
	{
		RECT r;
		GetWindowRect((HWND)winId(), &r);
		OffsetRect(&r, -r.left, -r.top);
		HRGN rgn = CreateRectRgnIndirect(&r);
		SetWindowRgn((HWND)winId(), rgn, TRUE);
	}
	return false;
	break;
	case WM_NCACTIVATE:
		if (m_titleBar)
		{
			m_titleBar->setActive(msg->wParam);
		}
		*result = TRUE;
		return true;
		break;
	case WM_NCPAINT:
		*result = 0;
		return true;
		break;
	case WM_NCCALCSIZE:
		if (msg->wParam)
		{
			*result = 0;
			return true;
		}
		break;
	case WM_NCHITTEST:
		if (windowState() == Qt::WindowMaximized)
		{
			return false;
		}
		else if (hor && ver)
		{
			if (m_leftEdge == m_bottomEdge)
			{
				*result = m_leftEdge ? HTBOTTOMLEFT : HTTOPRIGHT;
			}
			else
			{
				*result = m_leftEdge ? HTTOPLEFT : HTBOTTOMRIGHT;
			}
		}
		else if (hor)
		{
			*result = m_leftEdge ? HTLEFT : HTRIGHT;
		}
		else if (ver)
		{
			*result = m_topEdge ? HTTOP : HTBOTTOM;
		}
		else
		{
			*result = HTCLIENT;
		}
		return true;
		break;
	case WM_GETMINMAXINFO:
	{
		PMINMAXINFO mmi = reinterpret_cast<PMINMAXINFO>(msg->lParam);
		const POINT ptZero = { 0, 0 };
		HMONITOR hmon = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
		HMONITOR hwndmon = MonitorFromWindow((HWND)winId(), MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi, mi2;
		mi.cbSize = sizeof(mi);
		mi2.cbSize = sizeof(mi2);
		GetMonitorInfo(hmon, &mi);
		GetMonitorInfo(hwndmon, &mi2);
		int miWidth = mi.rcWork.right - mi.rcWork.left;
		int miHeight = mi.rcWork.bottom - mi.rcWork.top;
		int mi2Width = mi2.rcWork.right - mi2.rcWork.left;
		int mi2Height = mi2.rcWork.bottom - mi2.rcWork.top;
		
		mmi->ptMaxPosition.x = mi2.rcWork.left - mi2.rcMonitor.left;
		mmi->ptMaxPosition.y = mi2.rcWork.top - mi2.rcMonitor.top;
		if (mi2Width > miWidth || mi2Height > miHeight)
		{
			mmi->ptMaxSize.x = mi.rcMonitor.right;
			mmi->ptMaxSize.y = mi.rcMonitor.bottom;
		}
		else
		{
			mmi->ptMaxSize.x = mi2Width;
			mmi->ptMaxSize.y = mi2Height;
		}

#if QT_VERSION < 0x050600
		const float pixelRatio = 1;
#else
		const float pixelRatio = devicePixelRatioF();
#endif

		setMaximumSize(mi2Width / pixelRatio, mi2Height / pixelRatio); //Ensure window will not overlap taskbar
		mmi->ptMaxTrackSize.x = min(this->maximumWidth() * pixelRatio, GetSystemMetrics(SM_CXMAXTRACK));
		mmi->ptMaxTrackSize.y = min(this->maximumHeight() * pixelRatio, GetSystemMetrics(SM_CYMAXTRACK));
		mmi->ptMinTrackSize.x = this->minimumWidth() * pixelRatio;
		mmi->ptMinTrackSize.y = this->minimumHeight() * pixelRatio;
		*result = 0;
		return true;
	}
	break;
	case WM_DWMCOMPOSITIONCHANGED:
	case WM_ACTIVATE:
	{
		// Enable shadow
		MARGINS shadow_on = { 1, 1, 1, 1 };
		if (dwmExtendFrameIntoClientArea)
		{
			auto pFunc = (dwmExtendFrameIntoClientArea_t)dwmExtendFrameIntoClientArea;
			pFunc((HWND)winId(), &shadow_on);
		}
		*result = 0;
		// Don't intercept the message, so default functionality still happens.
		return false;
	}
	break;
	}
#if QT_VERSION < 0x050000
	return QFrame::winEvent(msg, result);
#else
	return false;
#endif
}
#endif

bool QCustomWindowFrame::event(QEvent* e)
{
	switch (e->type())
	{
	case QEvent::Show:
		if (!m_contents->isVisibleTo(this))
			m_contents->show();
		break;
	case QEvent::Hide:
		if (m_contents->isVisibleTo(this) && !windowState().testFlag(Qt::WindowMinimized))
			m_contents->hide();
		break;
	}
	return QFrame::event(e);
}

void QCustomWindowFrame::closeEvent(QCloseEvent* e)
{
	if (m_contents && m_contents->isVisibleTo(this))
	{
		if (!m_contents->close())
			e->ignore();
	}
}

void QCustomWindowFrame::changeEvent(QEvent* e)
{
	if ((e->type() == QEvent::WindowStateChange || e->type() == QEvent::ActivationChange) && getMainWindow() && getMainWindow()->windowState() == Qt::WindowMinimized)
	{
		getMainWindow()->setWindowState(Qt::WindowNoState);
	}
	if (isWindow() && e->type() == QEvent::WindowStateChange)
	{
		if (windowState() == Qt::WindowMaximized)
		{
			m_grid->setRowMinimumHeight(0, 0);
			m_grid->setRowMinimumHeight(2, 0);
			m_grid->setColumnMinimumWidth(0, 0);
			m_grid->setColumnMinimumWidth(2, 0);
		}
		else
		{
			m_grid->setRowMinimumHeight(0, 4);
			m_grid->setRowMinimumHeight(2, 4);
			m_grid->setColumnMinimumWidth(0, 4);
			m_grid->setColumnMinimumWidth(2, 4);
		}
	}
	if (m_titleBar)
	{
		m_titleBar->updateWindowStateButtons();
	}

#if QT_VERSION < 0x050000 // Nudging window seems to keep some UI from becoming unresponsive after restore
	if (e->type() == QEvent::WindowStateChange)
	{
		if (windowState() == Qt::WindowNoState || windowState() == Qt::WindowActive)
		{
			QTimer::singleShot(0, this, SLOT(nudgeWindow()));
		}
	}
#endif

	QFrame::changeEvent(e);
}

bool QCustomWindowFrame::eventFilter(QObject* o, QEvent* e)
{
	if (o == m_contents)
	{
#if QT_VERSION < 0x050000 // Qt 4 does not have signals for icon and title changes, so need to use the event filter.
		if (e->type() == QEvent::WindowIconChange || e->type() == QEvent::ApplicationWindowIconChange)
		{
			onIconChange();
		}
		else if (e->type() == QEvent::WindowTitleChange)
		{
			setWindowTitle(m_contents->windowTitle());
		}
#endif
		if (m_contents->parentWidget() == this)
		{

			// Respond to some events from our contents.
			switch (e->type())
			{
				//case QEvent::ParentChange:
				//	if (!m_contents->parentWidget())
				//	{
				//		m_contents->setParent(this);
				//		m_grid->addWidget(m_contents, 1, 1);
				//		setVisible(m_contents->isVisible());
				//	}
				//	else if (m_contents->parentWidget() != this)
				//	{
				//		hide(); // Hide if contents gets assigned to another parent.
				//	}
				//	break;
			case QEvent::Close:
				close();
				break;
			case QEvent::HideToParent:
				if (isVisible())
					hide();
				break;
			case QEvent::ShowToParent:
				if (!isVisible())
					show();
				break;
				//case QEvent::StyleChange:
				//	if (m_contents->styleSheet() != styleSheet())
				//		setStyleSheet(m_contents->styleSheet());
				//	break;
			}
		}
	}
	return QFrame::eventFilter(o, e);
}

void QCustomWindowFrame::mouseReleaseEvent(QMouseEvent* event)
{
	if (!m_titleBar)
	{
		event->ignore();
		return;
	}
	if (event->button() == Qt::LeftButton)
	{
		m_titleBar->m_dragging = false;
	}

	QFrame::mouseReleaseEvent(event);
}

void QCustomWindowFrame::nudgeWindow()
{
	RECT rcWind;
	GetWindowRect((HWND)winId(), &rcWind);

	MoveWindow((HWND)winId(), rcWind.left + 1, rcWind.top + 1, rcWind.right - rcWind.left - 1, rcWind.bottom - rcWind.top - 1, true);
	MoveWindow((HWND)winId(), rcWind.left, rcWind.top, rcWind.right - rcWind.left, rcWind.bottom - rcWind.top, true);
}

Qt::WindowFlags QCustomWindowFrame::calcFrameWindowFlags()
{
	Qt::WindowFlags flags = windowFlags();

	if (m_contents)
	{
		// Get all window flags from contents, excluding WindowType flags.
		flags = (flags & Qt::WindowType_Mask) | (m_contents->windowFlags() & ~Qt::WindowType_Mask);
	}

	// Make sure frameless window hint is always set.
	return flags | Qt::FramelessWindowHint;
}

void QCustomWindowFrame::updateWindowFlags()
{
	if (m_contents)
	{
		Qt::WindowFlags contentsWindowType = m_contents->windowFlags() & Qt::WindowType_Mask;
		// Contents has window type set, assign them it to us.
		if (contentsWindowType != Qt::Widget)
		{
			// Clear window type flags
			setWindowFlags(windowFlags() & ~Qt::WindowType_Mask);
			// Set window type from contents' flags, then clear contents' window type flags.
			setWindowFlags(windowFlags() | contentsWindowType);
			m_contents->setWindowFlags(windowFlags() & ~Qt::WindowType_Mask);
		}
		// No window type has been set on the contents, and we don't have any window flags
		else if ((this->windowFlags() & Qt::WindowType_Mask) == Qt::Widget)
		{
			// Default to regular window.
			setWindowFlags(windowFlags() | Qt::Window);
		}
	}

	setWindowFlags(calcFrameWindowFlags());

#if defined(WIN32) || defined(WIN64)
	// Windows flags can get reset, which makes the window lose resize and aero-snap functionality, as well as shadow.
	// Manually set them again.
	const unsigned long currentStyle = GetWindowLong((HWND)winId(), GWL_STYLE);
	SetWindowLong((HWND)winId(), GWL_STYLE, (currentStyle | WS_THICKFRAME) & ~(WS_POPUP | WS_BORDER));

	if (dwmExtendFrameIntoClientArea)
	{
		MARGINS shadow_on = { 1, 1, 1, 1 };
		auto pFunc = (dwmExtendFrameIntoClientArea_t)dwmExtendFrameIntoClientArea;
		pFunc((HWND)winId(), &shadow_on);
	}
#endif

}

void QCustomWindowFrame::onIconChange()
{
	// Check that the icon is not the same as already set, which prevents a stack overflow (if this gets called recursively)
	if (m_contents && windowIcon().cacheKey() != m_contents->windowIcon().cacheKey())
	{
		setWindowIcon(m_contents->windowIcon());
	}
}
