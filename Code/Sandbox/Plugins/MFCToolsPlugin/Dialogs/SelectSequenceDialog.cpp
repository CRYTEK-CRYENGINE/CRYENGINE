// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SelectSequenceDialog.h"
#include <CryMovie/IMovieSystem.h>

// CSelectSequence dialog

IMPLEMENT_DYNAMIC(CSelectSequenceDialog, CGenericSelectItemDialog)

//////////////////////////////////////////////////////////////////////////
CSelectSequenceDialog::CSelectSequenceDialog(CWnd* pParent) : CGenericSelectItemDialog(pParent)
{
	m_dialogID = "Dialogs\\SelSequence";
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ BOOL
CSelectSequenceDialog::OnInitDialog()
{
	SetTitle(_T("Select Sequence"));
	SetMode(eMODE_LIST);
	return __super::OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ void
CSelectSequenceDialog::GetItems(std::vector<SItem>& outItems)
{
	IMovieSystem* pMovieSys = GetIEditor()->GetMovieSystem();
	for (int i = 0; i < pMovieSys->GetNumSequences(); ++i)
	{
		IAnimSequence* pSeq = pMovieSys->GetSequence(i);
		SItem item;
		string fullname = pSeq->GetName();
		item.name = fullname.c_str();
		outItems.push_back(item);
	}
}

