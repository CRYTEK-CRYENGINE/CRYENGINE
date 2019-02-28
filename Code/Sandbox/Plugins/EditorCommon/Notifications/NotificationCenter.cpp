// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "NotificationCenter.h"
#include "NotificationCenterImpl.h"

#include "CryIcon.h"

#include "IEditor.h"

CProgressNotification::CProgressNotification(const QString& title, const QString& message, bool bShowProgress /* = false*/)
	: m_id(-1)
{
	CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
	m_id = pNotificationCenter->ShowProgress(title, message, bShowProgress);
}

void CProgressNotification::SetProgress(float value)
{
	CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
	Internal::CProgressNotification* pProgress = static_cast<Internal::CProgressNotification*>(pNotificationCenter->GetNotification(m_id));
	pProgress->SetProgress(value);
}

void CProgressNotification::SetMessage(const QString& message)
{
	CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
	Internal::CProgressNotification* pProgress = static_cast<Internal::CProgressNotification*>(pNotificationCenter->GetNotification(m_id));
	pProgress->SetMessage(message);
}

CProgressNotification::~CProgressNotification()
{
	CNotificationCenter* pNotificationCenter = static_cast<CNotificationCenter*>(GetIEditor()->GetNotificationCenter());
	Internal::CProgressNotification* pProgress = static_cast<Internal::CProgressNotification*>(pNotificationCenter->GetNotification(m_id));
	pProgress->SetProgress(1.0f);
}

