// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <stdafx.h>
#include "AutoLogTime.h"

CAutoLogTime::CAutoLogTime(const char* what)
{
	m_what = what;
	CryLog("---- Start: %s", m_what);
	m_t0 = GetTickCount();
}

CAutoLogTime::~CAutoLogTime()
{
	m_t1 = GetTickCount();
	CryLog("---- End: %s (%d seconds)", m_what, (m_t1 - m_t0) / 1000);
}

