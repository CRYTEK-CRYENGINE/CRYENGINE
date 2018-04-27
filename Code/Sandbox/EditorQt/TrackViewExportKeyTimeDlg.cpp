// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002-2013.
// -------------------------------------------------------------------------
//  File name:   TrackViewExportKeyTimeDlg.cpp
//  Version:     v1.00
//  Created:     25/6/2013 by Konrad.
//  Compilers:   Visual Studio 2010
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TrackViewExportKeyTimeDlg.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CTrackViewExportKeyTimeDlg, CDialog)
CTrackViewExportKeyTimeDlg::CTrackViewExportKeyTimeDlg()
	: CDialog(IDD_TV_EXPORT_KEY_TIME_DLG, NULL)
{
	m_bAnimationTimeExport = true;
	m_bSoundTimeExport = true;
}

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CTrackViewExportKeyTimeDlg, CDialog)
ON_BN_CLICKED(IDC_TV_EXPORT_KEYS_TIME_ANIM_CHK, SetChecks)
ON_BN_CLICKED(IDC_TV_EXPORT_KEYS_TIME_SOUND_CHK, SetChecks)
END_MESSAGE_MAP()

void CTrackViewExportKeyTimeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TV_EXPORT_KEYS_TIME_ANIM_CHK, m_animationTimeExport);
	DDX_Control(pDX, IDC_TV_EXPORT_KEYS_TIME_SOUND_CHK, m_soundTimeExport);
}

//////////////////////////////////////////////////////////////////////////
BOOL CTrackViewExportKeyTimeDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_animationTimeExport.SetCheck(BST_CHECKED);
	m_soundTimeExport.SetCheck(BST_CHECKED);

	return FALSE;
}

void CTrackViewExportKeyTimeDlg::SetChecks()
{
	m_bAnimationTimeExport = m_animationTimeExport.GetCheck();
	m_bSoundTimeExport = m_soundTimeExport.GetCheck();
}

