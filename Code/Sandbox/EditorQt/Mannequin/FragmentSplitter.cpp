// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FragmentSplitter.h"

#include "MannequinDialog.h"

IMPLEMENT_DYNAMIC(CFragmentSplitter, CClampedSplitterWnd)

BEGIN_MESSAGE_MAP(CFragmentSplitter, CClampedSplitterWnd)
ON_WM_SETFOCUS()
END_MESSAGE_MAP()

void CFragmentSplitter::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);

	if (auto pMannequinDialog = CMannequinDialog::GetCurrentInstance())
	{
		if (auto pDockingPaneManager = pMannequinDialog->GetDockingPaneManager())
		{
			if (pDockingPaneManager->IsPaneSelected(CMannequinDialog::IDW_PREVIEWER_PANE) == false)
			{
				pDockingPaneManager->ShowPane(CMannequinDialog::IDW_FRAGMENT_EDITOR_PANE, FALSE);
			}
		}
	}
}

