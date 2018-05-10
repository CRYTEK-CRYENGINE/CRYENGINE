// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SandboxWindowing.h"
#include <QKeyEvent>

#include <EditorFramework/Events.h>

namespace Private_SandboxWindowing
{
	bool SendMissedShortcutEvent(QKeyEvent* keyEvent)
	{
		const auto key = keyEvent->key();
		if (key != Qt::Key_unknown && !(key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta)) {
			MissedShortcutEvent* event = new MissedShortcutEvent(QKeySequence(key | keyEvent->modifiers()));
			return QApplication::sendEvent(getMainWindow(), event);
		}
		return false;
	}
}


QSandboxTitleBar::QSandboxTitleBar(QWidget* parent, const QVariantMap& config) : QCustomTitleBar(parent), m_config(config)
{
	if (m_minimizeButton)
	{
		m_minimizeButton->setIcon(getIcon(m_config, SANDBOX_WRAPPER_MINIMIZE_ICON, CryIcon()));
	}
	if (m_maximizeButton)
	{
		if (parentWidget()->windowState().testFlag(Qt::WindowMaximized))
		{
			m_maximizeButton->setIcon(getIcon(m_config, SANDBOX_WRAPPER_RESTORE_ICON, CryIcon()));
		}
		else
		{
			m_maximizeButton->setIcon(getIcon(m_config, SANDBOX_WRAPPER_MAXIMIZE_ICON, CryIcon()));
		}
	}
	m_closeButton->setIcon(getIcon(m_config, SANDBOX_WRAPPER_CLOSE_ICON, CryIcon()));
}

void QSandboxTitleBar::updateWindowStateButtons()
{
	QCustomTitleBar::updateWindowStateButtons();
	if (parentWidget()->windowState().testFlag(Qt::WindowMaximized))
	{
		m_maximizeButton->setIcon(getIcon(m_config, SANDBOX_WRAPPER_RESTORE_ICON, CryIcon()));
	}
	else
	{
		m_maximizeButton->setIcon(getIcon(m_config, SANDBOX_WRAPPER_MAXIMIZE_ICON, CryIcon()));
	}
}

QSandboxWindow* QSandboxWindow::wrapWidget(QWidget* w, QToolWindowManager* manager)
{
	QSandboxWindow* windowFrame = new QSandboxWindow(manager);
	windowFrame->internalSetContents(w);
	return windowFrame;
}

void QSandboxWindow::ensureTitleBar()
{
	if (!m_titleBar)
	{
		m_titleBar = new QSandboxTitleBar(this, m_manager ? m_manager->config() : m_config);
	}
}

void QSandboxWindow::keyPressEvent(QKeyEvent* keyEvent)
{
	Private_SandboxWindowing::SendMissedShortcutEvent(keyEvent);
}

bool QSandboxWindow::eventFilter(QObject* o, QEvent* e)
{
	if (!m_manager && o == m_contents && e->type() == QEvent::KeyPress)
	{
		// broadcast not consumed shortcuts to sandbox main window
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
		if (Private_SandboxWindowing::SendMissedShortcutEvent(keyEvent))
			return true;
	}

	return QCustomWindowFrame::eventFilter(o, e);
}



void QSandboxWrapper::ensureTitleBar()
{
	if (!m_titleBar)
	{
		m_titleBar = new QSandboxTitleBar(this, m_manager->config());
	}
}

void QSandboxWrapper::keyPressEvent(QKeyEvent* keyEvent)
{
	Private_SandboxWindowing::SendMissedShortcutEvent(keyEvent);
}

bool QSandboxWrapper::eventFilter(QObject* o, QEvent* e)
{
	if (o == m_contents && e->type() == QEvent::KeyPress)
	{
		// broadcast not consumed shortcuts to sandbox main window
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
		if (Private_SandboxWindowing::SendMissedShortcutEvent(keyEvent))
			return true;
	}

	return QToolWindowCustomWrapper::eventFilter(o, e);
}

QNotifierSplitterHandle::QNotifierSplitterHandle(Qt::Orientation orientation, QSplitter* parent) : QSplitterHandle(orientation, parent)
{
}

void QNotifierSplitterHandle::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton)
	{
		// Notify editors that we have begun resizing
		GetIEditor()->Notify(eNotify_OnBeginLayoutResize);
	}

	QSplitterHandle::mousePressEvent(e);
}

void QNotifierSplitterHandle::mouseReleaseEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton)
	{
		// Notify editors that we have stopped resizing
		GetIEditor()->Notify(eNotify_OnEndLayoutResize);
	}

	QSplitterHandle::mouseReleaseEvent(e);
}

void QNotifierSplitterHandle::mouseMoveEvent(QMouseEvent *e)
{
	//QT 5.6: Screen position is incorrect in HiDPI contexts; so we fix it
	QMouseEvent e2(e->type(), e->localPos(), e->windowPos(), QCursor::pos(), e->button(), e->buttons(), e->modifiers());
	QSplitterHandle::mouseMoveEvent(&e2);
}

QNotifierSplitter::QNotifierSplitter(QWidget* parent /*= 0*/) : QSizePreservingSplitter(parent)
{
	setChildrenCollapsible(false);
}

QSplitterHandle* QNotifierSplitter::createHandle()
{
	return new QNotifierSplitterHandle(orientation(), this);
}

