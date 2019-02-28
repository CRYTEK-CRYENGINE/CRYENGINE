// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QToolWindowWrapper.h"
#include "QToolWindowManager.h"
#include "QToolWindowArea.h"

#include <QBoxLayout>
#include <QCloseEvent>
#include <QWindowStateChangeEvent>

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#ifdef UNICODE
#define _UNICODE
#endif

#include <tchar.h>
#endif

QToolWindowWrapper::QToolWindowWrapper(QToolWindowManager* manager, Qt::WindowFlags flags)
	: QWidget(0)
	, m_manager(manager)
	, m_contents(nullptr)
{
	if (m_manager)
	{
		m_manager->installEventFilter(this);
		setStyleSheet(m_manager->styleSheet());

		if (manager->config().value(QTWM_WRAPPERS_ARE_CHILDREN, false).toBool())
		{
			setParent(manager);
		}
	}

	if (flags)
	{
		setWindowFlags(flags);
	}
	
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
}

QToolWindowWrapper::~QToolWindowWrapper()
{
	if (m_manager)
	{
		m_manager->removeWrapper(this);
		m_manager = nullptr;
	}
}

void QToolWindowWrapper::closeEvent(QCloseEvent* event)
{
	QList<QWidget*> toolWindows;
	foreach(QObject* child, children())
	{
		IToolWindowArea* tabWidget = qobject_cast<IToolWindowArea*>(child);
		if (tabWidget)
		{
			toolWindows << tabWidget->toolWindows();
		}
	}
	if (!m_manager->releaseToolWindows(toolWindows, true))
	{
		event->ignore();
	}
}

void QToolWindowWrapper::changeEvent(QEvent* e)
{
	if ((e->type() == QEvent::WindowStateChange || e->type() == QEvent::ActivationChange) && getMainWindow() && getMainWindow()->windowState() == Qt::WindowMinimized)
	{
		getMainWindow()->setWindowState(Qt::WindowNoState);
	}
	QWidget::changeEvent(e);
}

bool QToolWindowWrapper::eventFilter(QObject* o, QEvent* e)
{
	if (o == m_manager)
	{
		if (e->type() == QEvent::StyleChange && m_manager->styleSheet() != styleSheet())
		{
			setStyleSheet(m_manager->styleSheet());
		}
	}

	if (!m_manager && o == m_contents && e->type() == QEvent::StyleChange)
		return false;

	return QWidget::eventFilter(o, e);
}

#if QT_VERSION >= 0x050000
bool QToolWindowWrapper::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#if defined(WIN32) || defined(WIN64)
	MSG* msg = reinterpret_cast<MSG*>(message);
	if (winEvent(msg, result))
		return true;
#endif
	return QWidget::nativeEvent(eventType, message, result);
}
#endif

#if defined(WIN32) || defined(WIN64)
bool QToolWindowWrapper::winEvent(MSG *msg, long *result)
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
	case WM_EXITSIZEMOVE:
		if (m_manager && m_manager->draggedWrapper() == this)
		{
			m_manager->finishWrapperDrag();
		}
		return false;
		break;
	case WM_ACTIVATEAPP:
		if (!m_manager && msg->wParam)
		{
			//Restore all the Z orders
			QToolWindowManager* manager = findChild<QToolWindowManager*>();
			if (manager && !manager->config().value(QTWM_WRAPPERS_ARE_CHILDREN, false).toBool() && manager->config().value(QTWM_BRING_ALL_TO_FRONT, true).toBool())
			{
				manager->bringAllToFront();
				*result = 0;
				return true;
			}
		}
		break;
	}

#if QT_VERSION < 0x050000
	return QWidget::winEvent(msg, result);
#else
	return false;
#endif
}
#endif

QWidget* QToolWindowWrapper::getContents()
{
	return m_contents;
}

void QToolWindowWrapper::setContents(QWidget * widget)
{
	if (m_contents)
	{
		if (m_contents->parentWidget() == this)
			m_contents->setParent(nullptr);

		layout()->removeWidget(m_contents);
	}

	m_contents = widget;

	if (m_contents)
	{
		setAttribute(Qt::WA_DeleteOnClose, m_contents->testAttribute(Qt::WA_DeleteOnClose));

		if (m_contents->testAttribute(Qt::WA_QuitOnClose))
		{
			m_contents->setAttribute(Qt::WA_QuitOnClose, false);
			setAttribute(Qt::WA_QuitOnClose);
		}

		if (parentWidget())
		{
			// Disable minimize button on children, since they won't show up on the task bar, so can't be restored.
			setWindowFlags(windowFlags() & ~Qt::WindowMinimizeButtonHint);
		}

		layout()->addWidget(m_contents);
		m_contents->setParent(this);
		m_contents->show();
	}
}

void QToolWindowWrapper::startDrag()
{
	ReleaseCapture();
	SendMessage((HWND)winId(), WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void QToolWindowWrapper::deferDeletion()
{
	if (m_manager)
	{
		m_manager->removeWrapper(this);
		m_manager = nullptr;
	}
	setParent(nullptr);
	deleteLater();
}

