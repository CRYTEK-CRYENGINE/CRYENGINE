// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QToolWindowCustomWrapper.h"
#include "QToolWindowManager.h"
#include "IToolWindowArea.h"

#include <QGridLayout>
#include <QSpacerItem>
#include <QCloseEvent>
#include <QWindowStateChangeEvent>
#include <QToolButton>
#include <QApplication>
#include <QHoverEvent>
#include <QTime>
#include <QStyle>
#include <QDesktopWidget>

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>


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

QToolWindowCustomTitleBar::QToolWindowCustomTitleBar(QToolWindowCustomWrapper* parent)
	: QCustomTitleBar(parent)
{
}

QToolWindowCustomWrapper* QToolWindowCustomWrapper::wrapWidget(QWidget* w, QVariantMap config)
{
	return new QToolWindowCustomWrapper(nullptr, w, config);
}

QToolWindowCustomWrapper::QToolWindowCustomWrapper(QToolWindowManager* manager, QWidget* wrappedWidget, QVariantMap config)
	: QCustomWindowFrame()
	, m_manager(manager)	
{
	m_manager->installEventFilter(this);
	setStyleSheet(m_manager->styleSheet());

	if (manager->config().value(QTWM_WRAPPERS_ARE_CHILDREN, false).toBool())
	{
		setParent(manager);
		SetWindowLongPtr((HWND)winId(), GWLP_HWNDPARENT, (LONG_PTR)(manager->winId()));
	}

	setContents(wrappedWidget);
}

QToolWindowCustomWrapper::~QToolWindowCustomWrapper()
{
	if (m_manager)
	{
		m_manager->removeWrapper(this);
		m_manager = nullptr;
	}
}

bool QToolWindowCustomWrapper::event(QEvent* e)
{
	if (e->type() == QEvent::Show || e->type() == QEvent::Hide)
	{
		return QFrame::event(e); // Don't want QCustomWindowFrame custom handling of show and hide.
	}
	else if (e->type() == QEvent::Polish)
	{
		ensureTitleBar();
	}
	else if (e->type() == QEvent::ParentChange)
	{
		setWindowFlags(windowFlags()); // Ensure window stays as window even when reparented
		return true;
	}
	return QCustomWindowFrame::event(e);
}

void QToolWindowCustomWrapper::closeEvent(QCloseEvent* event)
{
	if (m_contents)
	{
		QList<QWidget*> toolWindows;

		for (QWidget* toolWindow : m_manager->toolWindows())
		{
			if (toolWindow->window() == this)
			{
				toolWindows << toolWindow;
			}
		}
		if (!m_manager->releaseToolWindows(toolWindows, true))
		{
			event->ignore();
		}
	}
}

bool QToolWindowCustomWrapper::eventFilter(QObject* o, QEvent* e)
{
	if (o == m_manager)
	{
		if (e->type() == QEvent::StyleChange && m_manager->styleSheet() != styleSheet())
		{
			setStyleSheet(m_manager->styleSheet());
		}
	}

	if (o == m_contents)
	{
		switch (e->type())
		{
			// Don't intercept these messages in parent class.
		case QEvent::Close:
		case QEvent::HideToParent:
		case QEvent::ShowToParent:
			return false;
		}
	}

	return QCustomWindowFrame::eventFilter(o, e);
}

Qt::WindowFlags QToolWindowCustomWrapper::calcFrameWindowFlags()
{
	Qt::WindowFlags flags = QCustomWindowFrame::calcFrameWindowFlags();

	if (parentWidget())
	{
		// Disable minimize button on children, since they won't show up on the task bar, so can't be restored.
		flags = flags & ~Qt::WindowMinimizeButtonHint;
	}
	return flags;
}

#if QT_VERSION >= 0x050000
bool QToolWindowCustomWrapper::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
	if (!m_titleBar)
		return false;

#if defined(WIN32) || defined(WIN64)
	MSG* msg = reinterpret_cast<MSG*>(message);
	if (winEvent(msg, result))
		return true;
#endif
	return QCustomWindowFrame::nativeEvent(eventType, message, result);
}
#endif

#if defined(WIN32) || defined(WIN64)
bool QToolWindowCustomWrapper::winEvent(MSG *msg, long *result)
{
	switch (msg->message)
	{
	case WM_MOVING:
		if (m_manager)
		{
			if (m_manager->draggedWrapper() == this)
			{
				m_manager->updateDragPosition();
			}
			else
			{
				m_manager->startDrag(this);
			}
		}
		return false;
		break;

	case WM_SIZING:
		if (m_manager)
		{
			if (m_manager->resizedWrapper() != this)
			{
				m_manager->startResize(this);
			}
		}
		break;

	case WM_EXITSIZEMOVE:
		if (m_manager)
		{
			if (m_manager->draggedWrapper() == this)
			{
				m_manager->finishWrapperDrag();
			}
			else if (m_manager->resizedWrapper() == this)
			{
				m_manager->finishWrapperResize();
			}
		}
		break;
	}
#if QT_VERSION < 0x050000
	return QCustomWindowFrame::winEvent(msg, result);
#else
	return false;
#endif
}
#endif

void QToolWindowCustomWrapper::startDrag()
{
	m_titleBar->onBeginDrag();
}

void QToolWindowCustomWrapper::deferDeletion()
{
	if (m_manager)
	{
		m_manager->removeWrapper(this);
		m_manager = nullptr;
	}
	setParent(nullptr);
	deleteLater();
}

