// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileMonitorBase.h"

#include "AudioControlsEditorPlugin.h"

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CFileMonitorBase::CFileMonitorBase(int const delay, QObject* const pParent)
	: QTimer(pParent)
	, m_delay(delay)
{
	setSingleShot(true);
	connect(this, &CFileMonitorBase::timeout, this, &CFileMonitorBase::SignalReloadData);
}

//////////////////////////////////////////////////////////////////////////
CFileMonitorBase::~CFileMonitorBase()
{
	stop();
	GetIEditor()->GetFileMonitor()->UnregisterListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CFileMonitorBase::OnFileChange(char const* szFileName, EChangeType type)
{
	start(m_delay);
}

//////////////////////////////////////////////////////////////////////////
void CFileMonitorBase::Disable()
{
	stop();
	GetIEditor()->GetFileMonitor()->UnregisterListener(this);
}
} // namespace ACE
