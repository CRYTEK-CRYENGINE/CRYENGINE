// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QCustomWindowFrame.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDesktopWidget>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

typedef HRESULT(WINAPI *dwmExtendFrameIntoClientArea_t)(HWND hwnd, const MARGINS* pMarInset);

#ifdef UNICODE
#define _UNICODE
#endif
#include <tchar.h>

Q_GUI_EXPORT QPixmap qt_pixmapFromWinHICON(HICON icon);
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
	myLayout->setContentsMargins(0, 0, 0, 0);
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

	connect(parent, SIGNAL(windowTitleChanged(const QString &)), m_caption, SLOT(setText(const QString &)));
	connect(parent, SIGNAL(windowIconChanged(const QIcon &)), this, SLOT(onIconChange()));

	onFrameContentsChanged(parent);
}

void QCustomTitleBar::updateWindowStateButtons()
{
	if (m_maximizeButton)
	{
		if (parentWidget()->windowState() & Qt::WindowMaximized)
		{
			m_maximizeButton->setObjectName("restoreButton");
		}
		else
		{
			m_maximizeButton->setObjectName("maximizeButton");
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
	if (parentWidget()->windowState() & Qt::WindowMaximized)
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

	const float pixelRatio = screen->devicePixelRatioF();

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

void QCustomTitleBar::onBeginDrag()
{
	m_dragging = true;
#if defined(WIN32) || defined(WIN64)
	ReleaseCapture();
	SendMessage((HWND)parentWidget()->winId(), WM_NCLBUTTONDOWN, HTCAPTION, 0);
#endif
}

void QCustomTitleBar::onFrameContentsChanged(QWidget* newContents)
{
	const auto flags = parentWidget()->windowFlags();
	m_minimizeButton->setVisible(flags & Qt::WindowMinimizeButtonHint || flags & Qt::MSWindowsFixedSizeDialogHint);
	m_maximizeButton->setVisible(flags & Qt::WindowMaximizeButtonHint || flags & Qt::MSWindowsFixedSizeDialogHint);
	m_closeButton->setVisible(flags & Qt::WindowCloseButtonHint);

	QString winTitle = parentWidget()->windowTitle();
	if (!winTitle.size())
	{
#if (defined(WIN32) || defined(WIN64))
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
		onBeginDrag();
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

bool QCustomTitleBar::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pObject == m_sysMenuButton)
	{
		switch (pEvent->type())
		{
		case QEvent::MouseButtonPress:
			if (static_cast<QMouseEvent*>(pEvent)->button() == Qt::LeftButton && qApp->widgetAt(QCursor::pos()) == m_sysMenuButton)
			{
				showSystemMenu(mapToGlobal(rect().bottomLeft()));
				return true;
			}
		case QEvent::MouseButtonDblClick:
			if (static_cast<QMouseEvent*>(pEvent)->button() == Qt::LeftButton && qApp->widgetAt(QCursor::pos()) == m_sysMenuButton)
			{
				parentWidget()->close();
				return true;
			}
		}
	}

	return QFrame::eventFilter(pObject, pEvent);
}

bool QCustomTitleBar::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#if defined(WIN32) || defined(WIN64)
	MSG* msg = reinterpret_cast<MSG*>(message);
	if (winEvent(msg, result))
		return true;
#endif
	return QFrame::nativeEvent(eventType, message, result);
}

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
		else if (m_sysMenuButton->geometry().contains(pos))
		{
			QMouseEvent me(QEvent::MouseButtonDblClick, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
			QApplication::sendEvent(m_sysMenuButton, &me);
			return true;
		}
	}
	return false;
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
	, m_resizeMargin(4)
{
	// To provide resizing functionality we must enable mouse tracking. Mouse tracking enables us to pick up
	// mouse move events without requiring to have mouse buttons pressed
	setMouseTracking(true);
	m_layout = new QBoxLayout(QBoxLayout::TopToBottom);
	m_layout->setSpacing(0);
	m_layout->setMargin(0);
	setLayout(m_layout);

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

		m_layout->removeWidget(m_contents);
		m_layout->removeWidget(m_titleBar);
		m_contents->removeEventFilter(this);

		disconnect(m_contents, SIGNAL(windowTitleChanged(const QString &)), this, SLOT(setWindowTitle(const QString &)));
		disconnect(m_contents, SIGNAL(windowIconChanged(const QIcon &)), this, SLOT(onIconChange()));
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

		connect(m_contents, SIGNAL(windowTitleChanged(const QString &)), this, SLOT(setWindowTitle(const QString &)));
		connect(m_contents, SIGNAL(windowIconChanged(const QIcon &)), this, SLOT(onIconChange()));

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

		m_layout->addWidget(m_titleBar);
		m_layout->addWidget(m_contents);
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

#if defined(WIN32) || defined(WIN64)
bool QCustomWindowFrame::winEvent(MSG *msg, long *result)
{
	if (!m_titleBar)
		return false;

	QRect r = rect();
	QPoint newPos = mapFromGlobal(QCursor::pos());

	bool topEdge = qAbs(newPos.y() - r.top()) <= m_resizeMargin;
	bool leftEdge = qAbs(newPos.x() - r.left()) <= m_resizeMargin;
	bool rightEdge = qAbs(newPos.x() - r.right()) <= m_resizeMargin;
	bool bottomEdge = qAbs(newPos.y() - r.bottom()) <= m_resizeMargin;
	bool hor = leftEdge || rightEdge;
	bool ver = topEdge || bottomEdge;
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
			if (leftEdge == bottomEdge)
			{
				*result = leftEdge ? HTBOTTOMLEFT : HTTOPRIGHT;
			}
			else
			{
				*result = leftEdge ? HTTOPLEFT : HTBOTTOMRIGHT;
			}
		}
		else if (hor)
		{
			*result = leftEdge ? HTLEFT : HTRIGHT;
		}
		else if (ver)
		{
			*result = topEdge ? HTTOP : HTBOTTOM;
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

		const float pixelRatio = devicePixelRatioF();

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
		MARGINS shadow_on = { 0,0,0,0 };
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
	return false;
}
#endif

bool QCustomWindowFrame::event(QEvent* pEvent)
{
	switch (pEvent->type())
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
	return QFrame::event(pEvent);
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

	if (m_titleBar)
	{
		m_titleBar->updateWindowStateButtons();
	}

	QFrame::changeEvent(e);
}

bool QCustomWindowFrame::eventFilter(QObject* o, QEvent* e)
{
	if (o == m_contents)
	{
		if (m_contents->parentWidget() == this)
		{
			// Respond to some events from our contents.
			switch (e->type())
			{
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
		MARGINS shadow_on = { 0, 0, 0, 0 };
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
