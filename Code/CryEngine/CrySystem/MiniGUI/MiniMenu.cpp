// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MiniMenu.cpp
//  Created:     26/08/2009 by Timur.
//  Description: Button implementation in the MiniGUI
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "MiniMenu.h"
#include "DrawContext.h"

MINIGUI_BEGIN

CMiniMenu::CMiniMenu()
{
	m_bVisible = false;
	m_bSubMenu = false;
	m_menuWidth = 0.f;
	m_selectionIndex = -1;
}

//////////////////////////////////////////////////////////////////////////
void CMiniMenu::OnPaint(CDrawContext& dc)
{
	CMiniButton::OnPaint(dc);

	if (m_bSubMenu)
	{
		// Draw checked mark.
		float x1 = m_rect.right - 12;
		float y1 = m_rect.top + 3;

		float x2 = m_rect.right - 3;
		float y2 = (m_rect.bottom + m_rect.top) / 2;

		float x3 = m_rect.right - 12;
		float y3 = m_rect.bottom - 3;

		dc.SetColor(ColorB(0, 0, 0, 255));

		dc.DrawLine(x1, y1, x2, y2, 2.f);
		dc.DrawLine(x2, y2, x3, y3, 2.f);

		x1 -= 1;
		x2 -= 1;
		x3 -= 1;
		y1 -= 1;
		y2 -= 1;
		y3 -= 1;

		dc.SetColor(ColorB(255, 255, 255, 255));

		dc.DrawLine(x1, y1, x2, y2, 2.f);
		dc.DrawLine(x2, y2, x3, y3, 2.f);
	}

	//dc.DrawFrame( m_rect,dc.Metrics().clrFrameBorder,dc.Metrics().clrBackground );
}

//////////////////////////////////////////////////////////////////////////
void CMiniMenu::SetRect(const Rect& rc)
{
	Rect newrc = rc;
	newrc.bottom = newrc.top + m_pGUI->Metrics().fTitleSize + 2;
	CMiniCtrl::SetRect(newrc);
}

//////////////////////////////////////////////////////////////////////////
void CMiniMenu::OnEvent(float x, float y, EMiniCtrlEvent event)
{
	switch (event)
	{
	case eCtrlEvent_LButtonDown:
		{
			if (m_bVisible)
				Close();
			else
				Open();
		}
		break;
	case eCtrlEvent_MouseOff:
		{
			bool closeMenu = true;
			IMiniCtrl* pCtrl = m_pGUI->GetCtrlFromPoint(x, y);

			//check if the cursor is still in one of the menu's children
			if (pCtrl)
			{
				for (int i = 0, num = GetSubCtrlCount(); i < num; i++)
				{
					if (pCtrl == GetSubCtrl(i))
					{
						closeMenu = false;
						break;
					}
				}
			}

			if (closeMenu)
			{
				Close();

				if (m_pParent)
				{
					m_pParent->OnEvent(x, y, eCtrlEvent_MouseOff);
				}
			}
		}
		break;
	}
}

CMiniMenu* CMiniMenu::UpdateSelection(EMiniCtrlEvent event)
{
	CMiniMenu* pNewMenu = this;
	IMiniCtrl* pCurrentSelection = NULL;

	if (m_selectionIndex != -1)
	{
		pCurrentSelection = m_subCtrls[m_selectionIndex];
	}

	switch (event)
	{
	case eCtrlEvent_DPadLeft: //move to parent
		if (!pCurrentSelection)
		{
			//move to previous root menu
			pNewMenu = NULL;
		}
		else if (m_bSubMenu)
		{
			Close();
			pNewMenu = (CMiniMenu*)m_pParent.get();
			//pNewMenu->SetFlag(eCtrl_Highlight);
		}
		break;

	case eCtrlEvent_DPadRight: //move to child
		if (!pCurrentSelection)
		{
			//move to next root menu
			pNewMenu = NULL;
		}
		else if (pCurrentSelection->GetType() == eCtrlType_Menu)
		{
			pNewMenu = (CMiniMenu*)pCurrentSelection;
			pNewMenu->Open();
			pNewMenu->ClearFlag(eCtrl_Highlight);
		}
		break;

	case eCtrlEvent_DPadUp: //move up list
		if (m_bSubMenu)
		{
			if (m_selectionIndex > 0)
			{
				m_selectionIndex--;
			}
		}
		else
		{
			if (m_selectionIndex >= 0)
			{
				m_selectionIndex--;

				if (m_selectionIndex == -1)
				{
					SetFlag(eCtrl_Highlight);
				}
			}
		}
		break;

	case eCtrlEvent_DPadDown: //move down the list
		if (m_selectionIndex < (int)(m_subCtrls.size() - 1))
		{
			if (m_selectionIndex == -1)
			{
				ClearFlag(eCtrl_Highlight);
			}
			m_selectionIndex++;
		}
		break;

	case eCtrlEvent_LButtonDown: //pass on button press
		{
			if (pCurrentSelection)
			{
				if (pCurrentSelection->GetType() == eCtrlType_Menu)
				{
					pNewMenu = (CMiniMenu*)pCurrentSelection;
				}
				pCurrentSelection->OnEvent(0, 0, eCtrlEvent_LButtonDown);
			}
			else
			{
				OnEvent(0, 0, eCtrlEvent_LButtonDown);
			}
		}
		break;
	}

	if (pCurrentSelection)
	{
		pCurrentSelection->ClearFlag(eCtrl_Highlight);
	}

	if (m_selectionIndex >= 0)
	{
		m_subCtrls[m_selectionIndex]->SetFlag(eCtrl_Highlight);
	}

	return pNewMenu;
}

//////////////////////////////////////////////////////////////////////////
void CMiniMenu::Open()
{
	m_bVisible = true;

	Rect rc(0, 0, m_menuWidth, 1);

	//render sub menu to the right
	if (m_bSubMenu)
	{
		CMiniMenu* pParent = static_cast<CMiniMenu*>(m_pParent.get());
		rc.left = pParent->m_menuWidth; //rcParent.right;
		rc.right = rc.left + m_menuWidth;
	}
	else
	{
		Rect rcThis = GetRect();
		rc.top = rcThis.Height();
	}

	for (int i = 0, num = GetSubCtrlCount(); i < num; i++)
	{
		IMiniCtrl* pSubCtrl = GetSubCtrl(i);
		pSubCtrl->ClearFlag(eCtrl_Hidden);
		float h = pSubCtrl->GetRect().Height();
		Rect rcCtrl = rc;
		rcCtrl.top = rc.top;
		rcCtrl.bottom = rcCtrl.top + h;
		pSubCtrl->SetRect(rcCtrl);
		rc.top += h;
	}

	//highlight first item when opened
	if (m_bSubMenu)
	{
		m_selectionIndex = 0;
		m_subCtrls[m_selectionIndex]->SetFlag(eCtrl_Highlight);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniMenu::Close()
{
	m_bVisible = false;

	for (int i = 0, num = GetSubCtrlCount(); i < num; i++)
	{
		IMiniCtrl* pSubCtrl = GetSubCtrl(i);
		pSubCtrl->SetFlag(eCtrl_Hidden);
	}

	if (m_selectionIndex != -1)
	{
		m_subCtrls[m_selectionIndex]->ClearFlag(eCtrl_Highlight);
	}
	m_selectionIndex = -1;
}

//////////////////////////////////////////////////////////////////////////
void CMiniMenu::Reset()
{
	m_bVisible = false;

	for (int i = 0, num = GetSubCtrlCount(); i < num; i++)
	{
		IMiniCtrl* pSubCtrl = GetSubCtrl(i);
		pSubCtrl->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniMenu::SaveState()
{
	for (int i = 0, num = GetSubCtrlCount(); i < num; i++)
	{
		IMiniCtrl* pSubCtrl = GetSubCtrl(i);
		pSubCtrl->SaveState();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniMenu::RestoreState()
{
	for (int i = 0, num = GetSubCtrlCount(); i < num; i++)
	{
		IMiniCtrl* pSubCtrl = GetSubCtrl(i);
		pSubCtrl->RestoreState();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniMenu::AddSubCtrl(IMiniCtrl* pCtrl)
{
	assert(pCtrl);
	pCtrl->SetFlag(eCtrl_Hidden);
	pCtrl->SetFlag(eCtrl_NoBorder);

	bool bSubMenu = false;

	if (pCtrl->GetType() == eCtrlType_Menu)
	{
		CMiniMenu* pSubMenu = static_cast<CMiniMenu*>(pCtrl);
		pSubMenu->m_bSubMenu = true;
		bSubMenu = true;
	}

	if (!m_bSubMenu)
	{
		//menu is at least the size of title bar
		m_menuWidth = max(m_menuWidth, strlen(GetTitle()) * 8.5f);
	}

	const char* title = pCtrl->GetTitle();

	if (title)
	{
		uint32 titleLen = strlen(title);

		float width = (float)titleLen * 8.5f;

		if (bSubMenu)
		{
			//increase width for submenu arrow
			width += 10.f;
		}

		m_menuWidth = max(m_menuWidth, width);
	}

	// Call parent
	CMiniButton::AddSubCtrl(pCtrl);
}
MINIGUI_END
