// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HotTrackingTreeCtrl.h"

BEGIN_MESSAGE_MAP(CHotTrackingTreeCtrl, CTreeCtrl)
ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

CHotTrackingTreeCtrl::CHotTrackingTreeCtrl()
	: CTreeCtrl()
{
	m_hHoverItem = NULL;
}

BOOL CHotTrackingTreeCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (nFlags)
	{
		HTREEITEM hItem = GetFirstVisibleItem();

		if (hItem)
		{
			HTREEITEM hTmpItem = 0;
			int d = abs(zDelta / WHEEL_DELTA) * 2;
			for (int i = 0; i < d; i++)
			{
				if (zDelta < 0)
					hTmpItem = GetNextVisibleItem(hItem);
				else
					hTmpItem = GetPrevVisibleItem(hItem);
				if (hTmpItem)
					hItem = hTmpItem;
			}
			if (hItem)
				SelectSetFirstVisible(hItem);
		}
	}
	return CTreeCtrl::OnMouseWheel(nFlags, zDelta, pt);
}

