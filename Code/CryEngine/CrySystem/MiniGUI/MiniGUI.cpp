// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MiniGUI.cpp
//  Created:     26/08/2009 by Timur.
//  Description: Implementation of the MiniGUI class
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "MiniGUI.h"
#include "DrawContext.h"

#include "MiniButton.h"
#include "MiniMenu.h"
#include "MiniInfoBox.h"
#include "MiniTable.h"

#include <CrySystem/ISystem.h>
#include <CryRenderer/IRenderer.h>

CRYREGISTER_SINGLETON_CLASS(minigui::CMiniGUI)

MINIGUI_BEGIN

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::InitMetrics()
{
	m_metrics.clrText = ColorB(255, 255, 255, 255);
	m_metrics.clrTextSelected = ColorB(0, 255, 0, 255);
	m_metrics.fTextSize = 12.0f;

	m_metrics.clrTitle = ColorB(255, 255, 255, 255);

	m_metrics.fTitleSize = 14.0f;

	const int backgroundAlpha = 255;

	m_metrics.clrBackground = ColorB(20, 20, 20, backgroundAlpha);
	m_metrics.clrBackgroundHighlight = ColorB(10, 10, 150, backgroundAlpha);
	m_metrics.clrBackgroundSelected = ColorB(10, 120, 10, backgroundAlpha);

	m_metrics.clrFrameBorder = ColorB(255, 0, 0, 255);
	m_metrics.clrFrameBorderHighlight = ColorB(255, 255, 0, 255);
	m_metrics.clrFrameBorderOutOfFocus = ColorB(0, 0, 0, 255);

	m_metrics.clrChecked = ColorB(0, 0, 0, 255);
	m_metrics.outOfFocusAlpha = 32;
}

class CMiniCtrlRoot : public CMiniCtrl
{
public:
	CMiniCtrlRoot() {};
	virtual EMiniCtrlType GetType() const                 { return eCtrlType_Unknown; };
	virtual void          OnPaint(class CDrawContext& dc) {};
};

//////////////////////////////////////////////////////////////////////////
CMiniGUI::CMiniGUI() :
	m_enabled(false),
	m_inFocus(true),
	m_bListenersRegistered(false),
	m_pDPadMenu(NULL),
	m_pMovingCtrl(NULL)
{
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::Init()
{
	m_pEventListener = NULL;
	InitMetrics();

	if (gEnv->pInput)
	{
		gEnv->pInput->AddEventListener(this);
	}

	if (IHardwareMouse* pMouse = GetISystem()->GetIHardwareMouse())
	{
		pMouse->AddListener(this);
	}

	m_bListenersRegistered = true;

	m_pRootCtrl = new CMiniCtrlRoot;
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::Reset()
{
	m_pRootCtrl->Reset();
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::SaveState()
{
	m_pRootCtrl->SaveState();
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::RestoreState()
{
	m_pRootCtrl->RestoreState();
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::SetEnabled(bool status)
{
	m_enabled = status;
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::SetInFocus(bool status)
{
	if (status)
	{
		m_inFocus = true;
	}
	else
	{
		CloseDPadMenu();
		m_inFocus = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::Done()
{
	if (m_bListenersRegistered)
	{
		IHardwareMouse* pMouse = GetISystem()->GetIHardwareMouse();
		if (pMouse)
			pMouse->RemoveListener(this);

		if (gEnv->pInput)
		{
			gEnv->pInput->RemoveEventListener(this);
		}
	}
	m_bListenersRegistered = false;
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::Draw()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	// When console opened hide MiniGui
	bool bConsoleOpened = gEnv->pConsole->IsOpened();
	if (m_enabled && !bConsoleOpened)
	{
		ProcessInput();

		CDrawContext dc(&m_metrics);
		dc.StartDrawing();
		{
			// Draw all controls.
			m_pRootCtrl->DrawCtrl(dc);
		}
		dc.StopDrawing();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::ProcessInput()
{
	//check we are not in digital selection mode
	if (!m_pDPadMenu)
	{
		float mx(0), my(0);
		IHardwareMouse* pMouse = GetISystem()->GetIHardwareMouse();
		if (pMouse)
		{
			pMouse->GetHardwareMouseClientPosition(&mx, &my);
		}

		//update moving control
		if (m_pMovingCtrl)
			m_pMovingCtrl->Move(mx, my);

		IMiniCtrl* pCtrl = GetCtrlFromPoint(mx, my);
		if (pCtrl)
		{
			SetHighlight(pCtrl, true, mx, my);
		}
		else
		{
			SetHighlight(m_highlightedCtrl, false, mx, my);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IMiniCtrl* CMiniGUI::GetCtrlFromPoint(float x, float y) const
{
	// Draw all controls.
	return m_pRootCtrl->GetCtrlFromPoint(x, y);
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::SetHighlight(IMiniCtrl* pCtrl, bool bEnable, float x, float y)
{
	if (pCtrl)
	{
		if (m_highlightedCtrl && m_highlightedCtrl != pCtrl)
		{
			m_highlightedCtrl->OnEvent(x, y, eCtrlEvent_MouseOff);
			m_highlightedCtrl->ClearFlag(eCtrl_Highlight);
		}

		if (bEnable)
		{
			pCtrl->OnEvent(x, y, eCtrlEvent_MouseOver);
			pCtrl->SetFlag(eCtrl_Highlight);

			m_highlightedCtrl = pCtrl;
		}
		else
		{
			pCtrl->OnEvent(x, y, eCtrlEvent_MouseOff);
			pCtrl->ClearFlag(eCtrl_Highlight);
			m_highlightedCtrl = NULL;
		}
	}
	else
	{
		assert(bEnable == false);

		if (m_highlightedCtrl)
		{
			m_highlightedCtrl->OnEvent(x, y, eCtrlEvent_MouseOff);
			m_highlightedCtrl->ClearFlag(eCtrl_Highlight);
			m_highlightedCtrl = NULL;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::SetFocus(IMiniCtrl* pCtrl, bool bEnable)
{
	if (m_focusCtrl)
		m_focusCtrl->ClearFlag(eCtrl_Focus);
	m_focusCtrl = pCtrl;
	if (m_focusCtrl)
	{
		if (bEnable)
			m_focusCtrl->SetFlag(eCtrl_Focus);
		else
			m_focusCtrl->ClearFlag(eCtrl_Focus);
	}
}

//////////////////////////////////////////////////////////////////////////
SMetrics& CMiniGUI::Metrics()
{
	return m_metrics;
}

//////////////////////////////////////////////////////////////////////////
IMiniCtrl* CMiniGUI::CreateCtrl(IMiniCtrl* pParentCtrl, int nCtrlID, EMiniCtrlType type, int nCtrlFlags, const Rect& rc, const char* title)
{
	CMiniCtrl* pCtrl = 0;
	// Test code.
	switch (type)
	{
	case eCtrlType_Button:
		pCtrl = new CMiniButton;
		break;
	case eCtrlType_Menu:
		pCtrl = new CMiniMenu;
		break;
	case eCtrlType_InfoBox:
		pCtrl = new CMiniInfoBox;
		break;
	case eCtrlType_Table:
		pCtrl = new CMiniTable;
		break;
	default:
		assert(0 && "Unknown MiniGUI control type");
		break;
	}
	;
	if (pCtrl)
	{
		pCtrl->SetGUI(this);
		pCtrl->SetFlag(nCtrlFlags);
		pCtrl->SetTitle(title);
		pCtrl->SetRect(rc);
		pCtrl->SetId(nCtrlID);

		if (pCtrl->CheckFlag(eCtrl_AutoResize))
			pCtrl->AutoResize();

		if (pCtrl->CheckFlag(eCtrl_CloseButton))
			pCtrl->CreateCloseButton();

		if (pParentCtrl)
			pParentCtrl->AddSubCtrl(pCtrl);
		else
			m_pRootCtrl->AddSubCtrl(pCtrl);

		if (type == eCtrlType_Menu && pParentCtrl == NULL)
		{
			m_rootMenus.push_back(pCtrl);
		}
	}
	return pCtrl;
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::OnCommand(SCommand& cmd)
{
	if (m_pEventListener)
		m_pEventListener->OnCommand(cmd);
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::OnHardwareMouseEvent(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta /*= 0*/)
{
	if (!m_inFocus || !m_enabled)
	{
		return;
	}

	float mx = float(iX);
	float my = float(iY);
	IMiniCtrl* pCtrl = GetCtrlFromPoint(mx, my);
	if (pCtrl)
	{
		switch (eHardwareMouseEvent)
		{
		case HARDWAREMOUSEEVENT_MOVE:
			break;
		case HARDWAREMOUSEEVENT_LBUTTONDOWN:
			pCtrl->OnEvent(mx, my, eCtrlEvent_LButtonDown);
			break;
		case HARDWAREMOUSEEVENT_LBUTTONUP:
			pCtrl->OnEvent(mx, my, eCtrlEvent_LButtonUp);
			break;
		case HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK:
		case HARDWAREMOUSEEVENT_RBUTTONDOWN:
		case HARDWAREMOUSEEVENT_RBUTTONUP:
		case HARDWAREMOUSEEVENT_RBUTTONDOUBLECLICK:
		case HARDWAREMOUSEEVENT_MBUTTONDOWN:
		case HARDWAREMOUSEEVENT_MBUTTONUP:
		case HARDWAREMOUSEEVENT_MBUTTONDOUBLECLICK:
		case HARDWAREMOUSEEVENT_WHEEL:
			break;
		}
	}
}

void CMiniGUI::SetDPadMenu(IMiniCtrl* pMenu)
{
	m_pDPadMenu = (CMiniMenu*)pMenu;
	gEnv->pHardwareMouse->Hide(true);
}

void CMiniGUI::CloseDPadMenu()
{
	if (m_pDPadMenu)
	{
		CMiniMenu* closeMenu = m_pDPadMenu;

		//close menu and all parent menus
		do
		{
			closeMenu->Close();
			closeMenu = (CMiniMenu*)closeMenu->GetParent();
		}
		while (closeMenu->GetType() == eCtrlType_Menu);

		m_pDPadMenu->ClearFlag(eCtrl_Highlight);
		m_pDPadMenu = NULL;
		gEnv->pHardwareMouse->Hide(false);
	}
}

void CMiniGUI::UpdateDPadMenu(const SInputEvent& rInputEvent)
{
	EKeyId inputKey = rInputEvent.keyId;
	bool isPressed = (rInputEvent.state == eIS_Pressed);

	const EKeyId DPAD_UP = eKI_XI_DPadUp;
	const EKeyId DPAD_DOWN = eKI_XI_DPadDown;
	const EKeyId DPAD_LEFT = eKI_XI_DPadLeft;
	const EKeyId DPAD_RIGHT = eKI_XI_DPadRight;
	const EKeyId X_BUTTON = eKI_XI_A;
	const EKeyId O_BUTTON = eKI_XI_B;

	if (inputKey == eKI_XI_ThumbLUp)
	{
		inputKey = DPAD_UP;
	}
	else if (inputKey == eKI_XI_ThumbLDown)
	{
		inputKey = DPAD_DOWN;
	}
	else if (inputKey == eKI_XI_ThumbLLeft)
	{
		inputKey = DPAD_LEFT;
	}
	else if (inputKey == eKI_XI_ThumbLRight)
	{
		inputKey = DPAD_RIGHT;
	}

	if (m_pDPadMenu)
	{
		if (inputKey == O_BUTTON)
		{
			CloseDPadMenu();
			return;
		}

		if (isPressed)
		{
			switch (inputKey)
			{
			case DPAD_DOWN:
				{
					m_pDPadMenu = m_pDPadMenu->UpdateSelection(eCtrlEvent_DPadDown);
				}
				break;

			case DPAD_UP:
				{
					m_pDPadMenu = m_pDPadMenu->UpdateSelection(eCtrlEvent_DPadUp);
				}
				break;

			case DPAD_LEFT:
				{
					CMiniMenu* pNewMenu = m_pDPadMenu->UpdateSelection(eCtrlEvent_DPadLeft);

					//get previous root menu
					if (pNewMenu == NULL)
					{
						int i = 0, nRootMenus = m_rootMenus.size();

						for (i = 0; i < nRootMenus; i++)
						{
							if (m_rootMenus[i] == m_pDPadMenu)
								break;
						}

						if (i > 0)
						{
							m_pDPadMenu->Close();
							m_pDPadMenu->ClearFlag(eCtrl_Highlight);

							m_pDPadMenu = (CMiniMenu*)m_rootMenus[i - 1];
							m_pDPadMenu->Open();
							m_pDPadMenu->SetFlag(eCtrl_Highlight);
						}
						//else selected menu remains the same
					}
					else
					{
						m_pDPadMenu = pNewMenu;
					}
				}

				break;

			case DPAD_RIGHT:
				{
					CMiniMenu* pNewMenu = m_pDPadMenu->UpdateSelection(eCtrlEvent_DPadRight);

					//get next root menu
					if (pNewMenu == NULL)
					{
						int i = 0, nRootMenus = m_rootMenus.size();

						for (i = 0; i < nRootMenus; i++)
						{
							if (m_rootMenus[i] == m_pDPadMenu)
								break;
						}

						if (i < nRootMenus - 1)
						{
							m_pDPadMenu->Close();
							m_pDPadMenu->ClearFlag(eCtrl_Highlight);

							m_pDPadMenu = (CMiniMenu*)m_rootMenus[i + 1];
							m_pDPadMenu->Open();
							m_pDPadMenu->SetFlag(eCtrl_Highlight);
						}
						//else selected menu remains the same
					}
					else
					{
						m_pDPadMenu = pNewMenu;
					}
				}
				break;

			case X_BUTTON:
				{
					m_pDPadMenu = m_pDPadMenu->UpdateSelection(eCtrlEvent_LButtonDown);
				}
				break;
			}
		}
	}
}

//General input callback - on consoles use 2nd controller input
bool CMiniGUI::OnInputEvent(const SInputEvent& rInputEvent)
{
	if (!m_inFocus)
	{
		return false;
	}

	if (rInputEvent.deviceType != eIDT_Gamepad)
	{
		return false;
	}

	if (m_pDPadMenu)
	{
		UpdateDPadMenu(rInputEvent);
	}
	else
	{
		float posX, posY;
		GetISystem()->GetIHardwareMouse()->GetHardwareMouseClientPosition(&posX, &posY);

		IMiniCtrl* pCtrl = GetCtrlFromPoint(posX, posY);

		if (pCtrl)
		{
			if (rInputEvent.keyId == eKI_XI_A || rInputEvent.keyId == eKI_Orbis_Cross)
			{
				//state names a bit confusing - see HWMouse
				switch (rInputEvent.state)
				{
				case eIS_Pressed:
					pCtrl->OnEvent(posX, posY, eCtrlEvent_LButtonDown);
					break;

				case eIS_Released:
					pCtrl->OnEvent(posX, posY, eCtrlEvent_LButtonUp);
					break;

				case eIS_Down:
					pCtrl->OnEvent(posX, posY, eCtrlEvent_LButtonPressed);

					//if we've clicked on a menu, enter menu selection mode, disable mouse
					if (pCtrl->GetType() == eCtrlType_Menu)
					{
						SetDPadMenu(pCtrl);
					}

					break;
				}
			}
			//CryLogAlways("InputEvent %s %d", rInputEvent.keyName, rInputEvent.deviceId);
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::SetEventListener(IMiniGUIEventListener* pListener)
{
	m_pEventListener = pListener;
}

//////////////////////////////////////////////////////////////////////////
void CMiniGUI::RemoveAllCtrl()
{
	m_highlightedCtrl = NULL;

	//reset all console variables to default state
	Reset();

	m_pRootCtrl->RemoveAllSubCtrl();
}

//////////////////////////////////////////////////////////////////////////
//CMiniCtrl
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::Reset()
{
	for (int i = 0, num = GetSubCtrlCount(); i < num; i++)
	{
		IMiniCtrl* pSubCtrl = GetSubCtrl(i);
		pSubCtrl->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::SaveState()
{
	for (int i = 0, num = GetSubCtrlCount(); i < num; i++)
	{
		IMiniCtrl* pSubCtrl = GetSubCtrl(i);
		pSubCtrl->SaveState();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::RestoreState()
{
	for (int i = 0, num = GetSubCtrlCount(); i < num; i++)
	{
		IMiniCtrl* pSubCtrl = GetSubCtrl(i);
		pSubCtrl->RestoreState();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::AddSubCtrl(IMiniCtrl* pCtrl)
{
	assert(pCtrl);
	_smart_ptr<IMiniCtrl> pTempCtrl(pCtrl);
	IMiniCtrl* pParent = pCtrl->GetParent();
	if (pParent)
		pParent->RemoveSubCtrl(pCtrl);

	static_cast<CMiniCtrl*>(pCtrl)->m_pParent = this;
	m_subCtrls.push_back(pCtrl);
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::RemoveSubCtrl(IMiniCtrl* pCtrl)
{
	assert(pCtrl);
	_smart_ptr<IMiniCtrl> pTempCtrl(pCtrl);
	IMiniCtrl* pParent = pCtrl->GetParent();
	if (pParent == this)
	{
		static_cast<CMiniCtrl*>(pCtrl)->m_pParent = 0;
		for (int i = 0, num = (int)m_subCtrls.size(); i < num; i++)
		{
			if (m_subCtrls[i] == pCtrl)
			{
				m_subCtrls.erase(m_subCtrls.begin() + i);
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::RemoveAllSubCtrl()
{
	int nSubCtrls = m_subCtrls.size();

	if (nSubCtrls)
	{
		for (int i = 0; i < nSubCtrls; i++)
		{
			IMiniCtrl* pSubCtrl = m_subCtrls[i].get();
			pSubCtrl->RemoveAllSubCtrl();
		}

		m_subCtrls.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
IMiniCtrl* CMiniCtrl::GetCtrlFromPoint(float x, float y)
{
	if (is_flag(eCtrl_Hidden))
		return 0;

	for (int i = 0, num = (int)m_subCtrls.size(); i < num; i++)
	{
		float lx = x - m_rect.left;
		float ly = y - m_rect.top;
		IMiniCtrl* pHit = m_subCtrls[i]->GetCtrlFromPoint(lx, ly);
		if (pHit)
			return pHit;
	}
	if (m_rect.IsPointInside(x, y))
		return this;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::DrawCtrl(CDrawContext& dc)
{
	OnPaint(dc);

	dc.PushClientRect(m_rect);

	for (int i = 0, num = (int)m_subCtrls.size(); i < num; i++)
	{
		CMiniCtrl* pCtrl = static_cast<CMiniCtrl*>((IMiniCtrl*)m_subCtrls[i]);
		if (!pCtrl->is_flag(eCtrl_Hidden))
			pCtrl->DrawCtrl(dc);
	}

	dc.PopClientRect();
}

//////////////////////////////////////////////////////////////////////////
int CMiniCtrl::GetSubCtrlCount() const
{
	return (int)m_subCtrls.size();
}

//////////////////////////////////////////////////////////////////////////
IMiniCtrl* CMiniCtrl::GetSubCtrl(int nIndex) const
{
	assert(nIndex >= 0 && nIndex < (int)m_subCtrls.size());
	return m_subCtrls[nIndex];
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::SetRect(const Rect& rc)
{
	m_rect = rc;

	if (m_pCloseButton)
	{
		float width = rc.Width();

		//relative position of cross box
		Rect closeRect(width - 20.f, 0.f, width, 20.f);
		m_pCloseButton->SetRect(closeRect);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::SetVisible(bool state)
{
	if (state)
	{
		clear_flag(eCtrl_Hidden);
	}
	else
	{
		set_flag(eCtrl_Hidden);
	}

	if (m_pCloseButton)
	{
		m_pCloseButton->SetVisible(state);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::AutoResize()
{
	uint32 stringLen = m_title.length();

	if (stringLen)
	{
		//just an approximation for now - should take into account font size / kerning
		m_rect.right = m_rect.left + (8.5f * stringLen);
	}

	m_requiresResize = false;
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::StartMoving(float x, float y)
{
	if (!m_moving)
	{
		m_prevX = x;
		m_prevY = y;
		m_moving = true;

		m_pGUI->SetMovingCtrl(this);
		//gEnv->pHardwareMouse->Hide(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::StopMoving()
{
	if (m_moving)
	{
		m_moving = false;
		m_pGUI->SetMovingCtrl(NULL);
		//gEnv->pHardwareMouse->Hide(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::Move(float x, float y)
{
	if (m_moving)
	{
		float moveX = x - m_prevX;
		float moveY = y - m_prevY;

		m_rect.top += moveY;
		m_rect.bottom += moveY;

		m_rect.left += moveX;
		m_rect.right += moveX;

		m_prevX = x;
		m_prevY = y;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::OnEvent(float x, float y, EMiniCtrlEvent event)
{
	switch (event)
	{
	case eCtrlEvent_LButtonDown:
		{
			if (is_flag(eCtrl_Highlight | eCtrl_Moveable))
			{
				StartMoving(x, y);
			}
		}
		break;

	case eCtrlEvent_LButtonUp:
		if (m_moving)
		{
			StopMoving();
		}
		break;

	case eCtrlEvent_MouseOver:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiniCtrl::CreateCloseButton()
{
	if (m_pGUI)
	{
		m_pCloseButton = m_pGUI->CreateCtrl(this, 100, eCtrlType_Button, 0, Rect(0, 0, 100, 20), "X");

		if (m_pCloseButton)
		{
			m_pCloseButton->SetConnectedCtrl(this);

			float width = m_rect.Width();

			//relative position of cross box
			Rect closeRect(width - 20.f, 0.f, width, 20.f);
			m_pCloseButton->SetRect(closeRect);
		}
	}
}

MINIGUI_END
