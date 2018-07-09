// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "Events.h"

#include <QObject>
#include <QWidget>
#include <QApplication>

#include "BroadcastManager.h"

SandboxEvent::SandboxEvent(EventType type, bool fallbackToParent)
	: QEvent(static_cast<QEvent::Type>(type))
	, m_beingHandled(false)
	, m_fallbackToParent(fallbackToParent)
{
}

void SandboxEvent::SendToKeyboardFocus()
{
	QObject* const pFocused = QApplication::focusWidget();
	if (pFocused)
	{
		QApplication::sendEvent(pFocused, this);
	}
}

void SandboxEvent::SendToMouseFocus()
{
	//TODO : this should handle grab focus cases, the mouse focus is not always the widget under cursor
	//Grabbing widget seems difficult to extract. Only way I can imagine would work would be to intercept GrabFocus events at application level
	QObject* const pFocused = QApplication::widgetAt(QCursor::pos());
	if (pFocused)
	{
		QApplication::sendEvent(pFocused, this);
	}
}

bool SandboxEvent::IsSandboxEvent(QEvent::Type type)
{
	return type > SandboxEvent::First && type < SandboxEvent::Max;
}

//////////////////////////////////////////////////////////////////////////

CommandEvent::CommandEvent(const char* command)
	: SandboxEvent(EventType::Command, true)
	, m_command(command)
{
}

GetBroadcastManagerEvent::GetBroadcastManagerEvent()
	: SandboxEvent(EventType::GetBroadcastManager, true)
	, m_broadcastManager(nullptr)
{
}

CBroadcastManager* GetBroadcastManagerEvent::GetManager() const
{
	return m_broadcastManager;
}

void GetBroadcastManagerEvent::SetManager(CBroadcastManager* const pManager)
{
	m_broadcastManager = pManager;
}

//
// Broadcast events
//

BroadcastEvent::BroadcastEvent(EventType type)
	: QEvent(static_cast<QEvent::Type>(type))
{
}

bool BroadcastEvent::IsBroadcastEvent(const QEvent::Type type)
{
	return type > BroadcastEvent::All && type < BroadcastEvent::Max;
}

void BroadcastEvent::Broadcast(CBroadcastManager* pBroadcastManager)
{
	CRY_ASSERT(pBroadcastManager);
	pBroadcastManager->Broadcast(*this);
}

void BroadcastEvent::Broadcast(QWidget* pContextWidget)
{
	CRY_ASSERT(pContextWidget);
	CBroadcastManager* const pBroadcastManager = CBroadcastManager::Get(pContextWidget);
	CRY_ASSERT(pBroadcastManager);
	pBroadcastManager->Broadcast(*this);
}

//////////////////////////////////////////////////////////////////////////


ClearInspectorEvent::ClearInspectorEvent()
	: PopulateInspectorEvent([](CInspector&){ /*Do nothing*/})
{
	
}


//////////////////////////////////////////////////////////////////////////

EditCurveEvent::EditCurveEvent(const TSetupCallback& setup)
	: BroadcastEvent(BroadcastEvent::EditCurve)
	, m_setup(setup)
{
}

void EditCurveEvent::SetupEditor(CCurveEditorPanel& editorPanel) const
{
	if (m_setup)
		m_setup(editorPanel);
}

CameraTransformEvent::CameraTransformEvent()
	: SandboxEvent(EventType::CameraMovement, true)
{
	m_translate.Set(0, 0, 0);
	m_rotate.Set(0, 0, 0);
}

void CameraTransformEvent::SetTranslation(Vec3& translation)
{
	m_translate = translation;
}

void CameraTransformEvent::SetRotation(Vec3& rotation)
{
	m_rotate = rotation;
}

CryInputEvent::CryInputEvent(SInputEvent* pInput)
	: SandboxEvent(EventType::CryInput, true)
	, m_pInput(pInput)
{
}

MissedShortcutEvent::MissedShortcutEvent(QKeySequence sequence)
	: SandboxEvent(EventType::MissedShortcut, false)
	, m_sequence(sequence)
{
}

