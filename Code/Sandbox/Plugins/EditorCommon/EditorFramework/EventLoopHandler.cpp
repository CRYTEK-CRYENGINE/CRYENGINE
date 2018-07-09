// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "EventLoopHandler.h"

#include <QApplication>
#include <QWidget>

#include "Events.h"

CEventLoopHandler::CEventLoopHandler()
	: m_pDefaultHandler(nullptr)
{
	qApp->installEventFilter(this);
	qApp->installNativeEventFilter(this);
}

CEventLoopHandler::~CEventLoopHandler()
{
	qApp->removeEventFilter(this);
	qApp->removeNativeEventFilter(this);
}

bool CEventLoopHandler::eventFilter(QObject* object, QEvent* event)
{
	//Custom events are only sent to the target widget and not propagated to parent.
	//This is an acceptable way to reimplement the event loop for all of our custom events

	const int type = event->type();
	if (type > SandboxEvent::First && type < SandboxEvent::Max)
	{
		SandboxEvent* sandboxEvent = static_cast<SandboxEvent*>(event);
		if (sandboxEvent->m_fallbackToParent && !sandboxEvent->m_beingHandled && object->isWidgetType())
		{
			sandboxEvent->m_beingHandled = true;
			sandboxEvent->setAccepted(false);

			QWidget* w = qobject_cast<QWidget*>(object);
			while (w)
			{
				//QApplication::sendEvent will trigger a recursive call of this (eventFilter) method, hence the m_beingHandled guard
				//res is almost always true as QObject always returns true for handling custom events, we have to rely on isAccepted instead to know if the event has been handled
				bool res = QApplication::sendEvent(w, event);
				if ((res && event->isAccepted()))
					return true;
				if (w->isWindow())
				{
					if (m_pDefaultHandler)
					{
						return QApplication::sendEvent(m_pDefaultHandler, event);
					}
					return true;
				}
				w = w->parentWidget();
			}
		}
	}

	return false;//Defer to processing by Qt event loop
}

void CEventLoopHandler::AddNativeHandler(uintptr_t id, std::function <bool(void*, long*)> callback)
{
	// careful not to register twice!
	RemoveNativeHandler(id);

	m_nativeListeners.emplace_back(id, callback);
}

void CEventLoopHandler::RemoveNativeHandler(uintptr_t id)
{
	for (auto iter = m_nativeListeners.begin(); iter != m_nativeListeners.end(); ++iter)
	{
		if (iter->m_id == id)
		{
			m_nativeListeners.erase(iter);
			return;
		}
	}
}


bool CEventLoopHandler::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
	if (eventType == "windows_generic_MSG")
	{
		for (auto listener : m_nativeListeners)
		{
			// first handler who succeeds returns.
			if (listener.m_cb(message, result))
			{
				return true;
			}
		}
	}

	return false;
}

