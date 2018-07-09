// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MannequinEditorPage.h"
#include "../MannequinModelViewport.h"
#include "../MannequinNodes.h"
#include "MannDopeSheet.h"

IMPLEMENT_DYNAMIC(CMannequinEditorPage, CToolbarDialog)

CMannequinEditorPage::CMannequinEditorPage(UINT nIDTemplate, CWnd* pParentWnd)
	: CToolbarDialog(nIDTemplate, pParentWnd)
{
}

CMannequinEditorPage::~CMannequinEditorPage()
{
}

//////////////////////////////////////////////////////////////////////////
BOOL CMannequinEditorPage::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == (int)'F' && IsWindowVisible())
		{
			CMannequinModelViewport* pViewport = ModelViewport();
			CMannDopeSheet* pTrackPanel = TrackPanel();
			CMannNodesCtrl* pNodeCtrl = Nodes();

			if (pViewport == NULL)
				return FALSE;

			// Get the nodes from dope sheet and node control
			std::vector<CSequencerNode*> pNodes;
			std::vector<CSequencerNode*> pSelectedNodes;

			if (pTrackPanel)
			{
				pTrackPanel->GetSelectedNodes(pSelectedNodes);
			}

			if (pNodeCtrl)
			{
				pNodeCtrl->GetSelectedNodes(pNodes);
			}

			// If selected keys are from non-expanded tracks, then ignore them (causes confusion for users)
			if (pNodeCtrl)
			{
				std::vector<CSequencerNode*>::reverse_iterator itEnd = pSelectedNodes.rend();
				for (std::vector<CSequencerNode*>::reverse_iterator it = pSelectedNodes.rbegin(); it != itEnd; it++)
				{
					if (!pNodeCtrl->IsNodeExpanded(*it))
					{
						pSelectedNodes.erase(--(it.base()));
					}
				}
			}

			// Merge list, ensure not empty
			stl::push_back_range_unique(pSelectedNodes, pNodes.begin(), pNodes.end());
			if (pSelectedNodes.empty())
			{
				return FALSE;
			}

			AABB selectionBB, entityBB;
			bool bInitialised = false;
			for (std::vector<CSequencerNode*>::iterator it = pSelectedNodes.begin(); it != pSelectedNodes.end(); ++it)
			{
				IEntity* pEntity = (*it)->GetEntity();

				if (pEntity == NULL)
					continue;

				pEntity->GetWorldBounds(entityBB);

				if (!bInitialised)
				{
					selectionBB = entityBB;
					bInitialised = true;
				}
				else
				{
					selectionBB.Add(entityBB);
				}
			}

			if (bInitialised)
			{
				pViewport->Focus(selectionBB);

				// Focus can turn off first person cam, if this happens the button state needs updating
				ValidateToolbarButtonsState();
			}

			// We don't want to consume in case of data entry
			return FALSE;
		}
	}

	return FALSE;
}

