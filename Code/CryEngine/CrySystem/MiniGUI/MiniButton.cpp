// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MiniButton.cpp
//  Created:     26/08/2009 by Timur.
//  Description: Button implementation in the MiniGUI
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "MiniButton.h"
#include "DrawContext.h"
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>

MINIGUI_BEGIN

CMiniButton::CMiniButton()
{
	m_pCVar = NULL;
	m_fCVarValue[0] = 0;
	m_fCVarValue[1] = 1;
	m_saveStateOn = false;
	m_clickCallback = NULL;
	m_pCallbackData = NULL;
	m_pConnectedCtrl = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CMiniButton::Reset()
{
	/*if (m_pCVar)
	   {
	   pCVar->Set( m_fCVarValue[0] );
	   }*/

	/*if(m_pConnectedCtrl)
	   {
	   m_pConnectedCtrl->SetVisible(false);
	   }*/

	clear_flag(eCtrl_Checked);
}

//////////////////////////////////////////////////////////////////////////
void CMiniButton::SaveState()
{
	if (is_flag(eCtrl_Checked))
	{
		if (m_pConnectedCtrl && m_pConnectedCtrl->CheckFlag(eCtrl_Hidden))
		{
			m_saveStateOn = false;
		}
		else
		{
			m_saveStateOn = true;
		}
	}
	else
	{
		m_saveStateOn = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniButton::RestoreState()
{
	if (m_pCVar)
	{

		if (m_pCVar->GetFVal() == m_fCVarValue[1])
		{
			set_flag(eCtrl_Checked);

			//Restoring CVars has caused a few issues, especially when the user is changing
			//CVars through the console while perfHud is active, removing for now
			//pCVar->Set( m_saveStateOn ? m_fCVarValue[1] : m_fCVarValue[0] );
		}
		else
		{
			clear_flag(eCtrl_Checked);
		}
	}
	else
	{
		//connected controls (tables / info boxes etc) now look after themselves
		/*if(m_pConnectedCtrl)
		   {
		   m_pConnectedCtrl->SetVisible(m_saveStateOn);
		   }*/

		if (CheckFlag(eCtrl_CheckButton))
		{
			if (m_saveStateOn)
			{
				set_flag(eCtrl_Checked);
			}
			else
			{
				clear_flag(eCtrl_Checked);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniButton::OnPaint(CDrawContext& dc)
{
	ColorB bkgColor = dc.Metrics().clrBackground;

	if (is_flag(eCtrl_Highlight))
	{
		bkgColor = dc.Metrics().clrBackgroundHighlight;
	}
	else if (is_flag(eCtrl_Checked))
	{
		//if connected control has been hidden, this button should not be checked
		if (m_pConnectedCtrl && m_pConnectedCtrl->CheckFlag(eCtrl_Hidden))
		{
			clear_flag(eCtrl_Checked);
		}
		else
		{
			bkgColor = dc.Metrics().clrBackgroundSelected;
		}
	}

	float borderThickness = 1.0f;

	if (is_flag(eCtrl_Focus))
		borderThickness = 3.0f;

	ColorB borderCol = dc.Metrics().clrFrameBorder;

	if (!m_pGUI->InFocus())
	{
		borderCol = dc.Metrics().clrFrameBorderOutOfFocus;
		bkgColor.a = dc.Metrics().outOfFocusAlpha;
	}

	dc.DrawFrame(m_rect, borderCol, bkgColor, borderThickness);

	ColorB textColor = dc.Metrics().clrText;

	if (is_flag(eCtrl_Checked | eCtrl_Highlight))
	{
		textColor = dc.Metrics().clrTextSelected;
	}

	dc.SetColor(textColor);

	ETextAlign align;
	float startX;

	if (is_flag(eCtrl_TextAlignCentre))
	{
		startX = (m_rect.left + m_rect.right) / 2.f;
		align = eTextAlign_Center;
	}
	else
	{
		startX = m_rect.left + 5.f;
		align = eTextAlign_Left;
	}

	dc.DrawString(startX, m_rect.top, dc.Metrics().fTitleSize, align, GetTitle());

	//Check not very obvious
#if 0
	if (is_flag(eCtrl_Checked))
	{
		// Draw checked mark.
		float checkX = m_rect.left + 4;
		float checkY = (m_rect.bottom + m_rect.top) * 0.5f;
		dc.SetColor(dc.Metrics().clrChecked);
		dc.DrawLine(checkX, checkY, checkX + 3, checkY + 3);
		dc.DrawLine(checkX + 3, checkY + 3, checkX + 7, checkY - 6);
	}
#endif

}

//////////////////////////////////////////////////////////////////////////
void CMiniButton::SetRect(const Rect& rc)
{
	Rect newrc = rc;
	newrc.bottom = newrc.top + m_pGUI->Metrics().fTitleSize + 2;
	CMiniCtrl::SetRect(newrc);
}

//////////////////////////////////////////////////////////////////////////
void CMiniButton::OnEvent(float x, float y, EMiniCtrlEvent event)
{
	switch (event)
	{
	case eCtrlEvent_LButtonDown:
		{
			SCommand cmd;

			if (CheckFlag(eCtrl_CheckButton))
			{
				if (!CheckFlag(eCtrl_Checked))
					cmd.command = eCommand_ButtonChecked;
				else
					cmd.command = eCommand_ButtonUnchecked;
			}
			else
			{
				cmd.command = eCommand_ButtonPress;
			}

			cmd.nCtrlID = GetId();
			cmd.pCtrl = this;
			GetGUI()->OnCommand(cmd);

			if (CheckFlag(eCtrl_CheckButton))
			{
				bool bOn = false;

				if (CheckFlag(eCtrl_Checked))
				{
					bOn = false;
					ClearFlag(eCtrl_Checked);
				}
				else
				{
					bOn = true;
					SetFlag(eCtrl_Checked);
				}

				if (m_pCVar)
				{
					m_pCVar->Set(m_fCVarValue[bOn ? 1 : 0]);
				}

				if (m_pConnectedCtrl)
				{
					m_pConnectedCtrl->SetVisible(bOn);
				}
			}
			else
			{
				//cross button behavior
				if (m_pConnectedCtrl)
				{
					m_pConnectedCtrl->SetVisible(false);
				}
			}

			if (m_clickCallback)
			{
				m_clickCallback(m_pCallbackData, true);
			}

		}
		break;
	case eCtrlEvent_MouseOff:
		{
			if (m_pParent)
			{
				m_pParent->OnEvent(x, y, eCtrlEvent_MouseOff);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMiniButton::SetControlCVar(const char* sCVarName, float fOffValue, float fOnValue)
{
	m_pCVar = GetISystem()->GetIConsole()->GetCVar(sCVarName);

	if (!m_pCVar)
	{
		CryLogAlways("failed to find CVar: %s\n", sCVarName);
	}

	m_fCVarValue[0] = fOffValue;
	m_fCVarValue[1] = fOnValue;

	if (m_pCVar && m_pCVar->GetFVal() == fOnValue)
	{
		set_flag(eCtrl_Checked);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMiniButton::SetClickCallback(ClickCallback callback, void* pCallbackData)
{
	m_clickCallback = callback;
	m_pCallbackData = pCallbackData;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMiniButton::SetConnectedCtrl(IMiniCtrl* pConnectedCtrl)
{
	m_pConnectedCtrl = pConnectedCtrl;
	return true;
}

MINIGUI_END
