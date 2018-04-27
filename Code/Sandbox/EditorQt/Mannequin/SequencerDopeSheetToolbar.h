// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SequencerDialogToolbar_h__
#define __SequencerDialogToolbar_h__
#pragma once

class CSequencerDopeSheetToolbar;

#include "Controls/DlgBars.h"
#include "SequencerDopeSheetBase.h"

class CSequencerDopeSheetToolbar : public CDlgToolBar
{

public:
	CSequencerDopeSheetToolbar();
	virtual ~CSequencerDopeSheetToolbar();
	virtual void InitToolbar();
	virtual void SetTime(float fTime, float fFps);

protected:
	CStatic m_timeWindow;
	float   m_lastTime;

};

#endif // __SequencerDialogToolbar_h__

