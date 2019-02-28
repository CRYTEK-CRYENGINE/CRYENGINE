// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"

#include "QFullScreenWidget.h"

#include <QWidget.h>
#include <QVBoxLayout>
#include <QEvent>
#include <QChildEvent>
#include <QApplication>
#include <QDesktopWidget>
#include <QEvent.h>
#include "EditorFramework/Events.h"
#include "BoostPythonMacros.h"

bool QFullScreenWidget::eventFilter(QObject* obj, QEvent* event)
{
	// we listen to all focus events of children
	if (event->type() == QEvent::FocusIn)
	{
		s_lastfullScreenWidget = this;
	}
	else if (event->type() == QEvent::ChildAdded)
	{
		QChildEvent* evt = static_cast<QChildEvent*> (event);
		QObject* child = evt->child();

		if (child->isWidgetType())
		{
			child->installEventFilter(this);

			// add a filter to all children of widget so we know when they have focus
			const QList<QWidget*> list = child->findChildren<QWidget*>();

			for (QWidget* w : list)
			{
				w->installEventFilter(this);
			}
		}
	}
	// Don't forget to clean ourselves up!
	else if (event->type() == QEvent::ChildRemoved)
	{
		QChildEvent* evt = static_cast<QChildEvent*> (event);
		QObject* child = evt->child();

		if (child->isWidgetType())
		{
			child->removeEventFilter(this);

			// remove ourselves from all child widgets as well
			const QList<QWidget*> list = child->findChildren<QWidget*>();

			for (QWidget* w : list)
			{
				w->removeEventFilter(this);
			}
		}
	}

	return false;
}

class QFullScreenWidgetWindow : public QWidget
{
public:
	QFullScreenWidgetWindow(QFullScreenWidget* w);
	~QFullScreenWidgetWindow();

	bool event(QEvent* event) override;
	void closeEvent(QCloseEvent*) override;
	void changeEvent(QEvent* event) override;


private:
	QFullScreenWidget* m_widget;
};

QFullScreenWidgetWindow::QFullScreenWidgetWindow(QFullScreenWidget* w)
	: m_widget(w)
{
	QWidget* wmax = w->m_maximizable;
	QVBoxLayout* layout = new QVBoxLayout;
	activateWindow();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(wmax);
	setLayout(layout);
	showFullScreen();
	wmax->setFocus();
}

QFullScreenWidgetWindow::~QFullScreenWidgetWindow()
{
}

void QFullScreenWidgetWindow::closeEvent(QCloseEvent*)
{
	if (m_widget)
	{
		QWidget* wmax = m_widget->m_maximizable;
		layout()->removeWidget(wmax);
		wmax->setParent(nullptr);
		m_widget->Restore();
		m_widget = nullptr;
	}

	this->deleteLater();
}

void QFullScreenWidgetWindow::changeEvent(QEvent* event)
{
	QWidget::changeEvent(event);
	if (event->type() == QEvent::ActivationChange && !isActiveWindow())
	{
		close();
	}
}

bool QFullScreenWidgetWindow::event(QEvent* ev)
{
	switch (ev->type())
	{
	case QEvent::KeyPress:
	{
		bool res = QWidget::event(ev);
		if (!m_widget)
			// widget has been destroyed; do not forward
			return res;
		
		if(!ev->isAccepted())
		{
			//forward events not consumed are sent to the window of the original widget so local shortcuts should still work
			QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);

			const auto key = keyEvent->key();
			if (key != Qt::Key_unknown && !(key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta)) {
				MissedShortcutEvent* event = new MissedShortcutEvent(QKeySequence(key | keyEvent->modifiers()));
				return QApplication::sendEvent(m_widget->window(), keyEvent);
			}
		}
		return true;
	}

	default:
		return QWidget::event(ev);
	}
}

///////////// QFullScreenWidget ///////////////////////

QFullScreenWidgetWindow* QFullScreenWidget::s_fullScreenWindow = nullptr;
QFullScreenWidget* QFullScreenWidget::s_lastfullScreenWidget = nullptr;

QFullScreenWidget::QFullScreenWidget(QWidget* w)
	: m_maximizable(w)
{
	QVBoxLayout* layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(w);
	setLayout(layout);

	// add a filter to all children of widget so we know when they have focus
	const QList<QWidget*> list = w->findChildren<QWidget*>();

	w->installEventFilter(this);

	for (QWidget* child : list)
	{
		child->installEventFilter(this);
	}
}

QFullScreenWidget::~QFullScreenWidget()
{
	if (s_lastfullScreenWidget == this)
	{
		s_lastfullScreenWidget = nullptr;
	}
}

void QFullScreenWidget::ShowFullScreen()
{
	if (!m_maximizable || !isVisible())
	{
		return;
	}

	// First get the current widget's screen
	QDesktopWidget* w = QApplication::desktop();

	int desktopId = w->screenNumber(this);
	QRect desktopBounds = w->screenGeometry(desktopId);
	layout()->removeWidget(m_maximizable);
	m_maximizable->setParent(nullptr);
	s_fullScreenWindow = new QFullScreenWidgetWindow(this);
	s_fullScreenWindow->setGeometry(desktopBounds);
}

void QFullScreenWidget::focusInEvent(QFocusEvent* event)
{
	s_lastfullScreenWidget = this;
}

void QFullScreenWidget::Restore()
{
	s_fullScreenWindow = nullptr;
	layout()->addWidget(m_maximizable);
	m_maximizable->show();
	if (GetIEditor()->IsInGameMode())
	{
		GetIEditor()->ExecuteCommand("general.suspend_game_input");
	}
}

QFullScreenWidgetWindow* QFullScreenWidget::GetCurrentFullScreenWindow()
{
	return s_fullScreenWindow;
}

bool QFullScreenWidget::IsFullScreenWindowActive()
{
	return s_fullScreenWindow && s_fullScreenWindow->isActiveWindow();
}

namespace
{
void PyMaximize()
{
	QFullScreenWidgetWindow* w = QFullScreenWidget::GetCurrentFullScreenWindow();
	if (!w)
	{
		if (QFullScreenWidget::GetLastFullScreenWidget())
		{
			QFullScreenWidget::GetLastFullScreenWidget()->ShowFullScreen();
		}
	}
	else
	{
		w->close();
	}
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyMaximize, general, fullscreen,
                                     "Maximizes the focused widget",
                                     "general.fullscreen()");

