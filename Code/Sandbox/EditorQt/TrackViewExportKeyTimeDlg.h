// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002-2013.
// -------------------------------------------------------------------------
//  File name:   TrackViewExportKeyTimeDlg.h
//  Version:     v1.00
//  Created:     25/6/2013 by Konrad.
//  Compilers:   Visual Studio 2010
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TRACKVIEW_EXPORT_KEY_TIME_DIALOG_H__
#define __TRACKVIEW_EXPORT_KEY_TIME_DIALOG_H__
#pragma once

class CTrackViewExportKeyTimeDlg : public CDialog
{
	DECLARE_DYNAMIC(CTrackViewExportKeyTimeDlg)

public:
	CTrackViewExportKeyTimeDlg();
	virtual ~CTrackViewExportKeyTimeDlg(){}

	bool IsAnimationExportChecked() { return m_bAnimationTimeExport; };
	bool IsSoundExportChecked()     { return m_bSoundTimeExport; };

private:

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	void         SetChecks();

	DECLARE_MESSAGE_MAP()

private:
	CButton m_animationTimeExport;
	CButton m_soundTimeExport;
	bool    m_bAnimationTimeExport;
	bool    m_bSoundTimeExport;

};
#endif

