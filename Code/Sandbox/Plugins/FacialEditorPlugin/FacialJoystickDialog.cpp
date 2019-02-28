// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryAnimation/ICryAnimation.h>
#include "FacialJoystickDialog.h"
#include <CrySandbox/ScopedVariableSetter.h>
#include "Resource.h"

class CJoystickDialogDropTarget : public COleDropTarget
{
public:
	CJoystickDialogDropTarget(CFacialJoystickDialog* dlg) : m_dlg(dlg) {}

	virtual DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		point = ConvertToJoystickClientCoords(pWnd, point);
		if (m_dlg->m_pContext->GetChannelDescriptorFromDataSource(pDataObject).size() != 0)
			return DROPEFFECT_LINK;
		return DROPEFFECT_NONE;
	}

	virtual void OnDragLeave(CWnd* pWnd)
	{
	}

	virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		point = ConvertToJoystickClientCoords(pWnd, point);
		if (m_dlg->m_pContext->GetChannelDescriptorFromDataSource(pDataObject).size() != 0)
			return DROPEFFECT_LINK;
		return DROPEFFECT_NONE;
	}

	virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		point = ConvertToJoystickClientCoords(pWnd, point);
		IJoystick* pJoystick = 0;
		IJoystick::ChannelType axis = IJoystick::ChannelTypeHorizontal;
		if (m_dlg->m_pContext && m_dlg->m_ctrl.HitTestPoint(pJoystick, axis, point))
		{
			IFacialAnimChannel* pChannel = m_dlg->m_pContext->GetChannelFromDataSource(pDataObject);
			if (pChannel && !pChannel->IsGroup())
			{
				ISystem* pSystem = GetISystem();
				ICharacterManager* pCharacterManager = pSystem ? pSystem->GetIAnimationSystem() : 0;
				IFacialAnimation* pFacialAnimationSystem = pCharacterManager ? pCharacterManager->GetIFacialAnimation() : 0;

				IJoystickChannel* pJoystickChannel = (pFacialAnimationSystem && pChannel ? pFacialAnimationSystem->CreateJoystickChannel(pChannel) : 0);

				m_dlg->m_ctrl.OnSetJoystickChannel(pJoystick, axis, pJoystickChannel);
			}
			return TRUE;
		}
		return FALSE;
	}

private:
	CPoint ConvertToJoystickClientCoords(CWnd* pWnd, const CPoint& point)
	{
		CPoint point2 = point;
		pWnd->ClientToScreen(&point2);
		m_dlg->m_ctrl.ScreenToClient(&point2);
		return point2;
	}

	CFacialJoystickDialog* m_dlg;
};

IMPLEMENT_DYNAMIC(CFacialJoystickDialog, CToolbarDialog)

BEGIN_MESSAGE_MAP(CFacialJoystickDialog, CToolbarDialog)
ON_WM_SIZE()
ON_COMMAND(ID_FREEZE_LAYOUT, OnFreezeLayout)
ON_XTP_EXECUTE(ID_SNAP_MARGIN, OnSnapMarginChanged)
ON_UPDATE_COMMAND_UI(ID_SNAP_MARGIN, OnUpdateSnapMarginSizeUI)
ON_COMMAND(ID_AUTO_CREATE_KEY, OnAutoCreateKeyChanged)
ON_COMMAND(ID_KEY_ALL, OnKeyAll)
ON_COMMAND(ID_ZERO_ALL, OnZeroAll)
ON_WM_MOUSEWHEEL()
ON_WM_DESTROY()
END_MESSAGE_MAP()

CFacialJoystickDialog::CFacialJoystickDialog()
	: m_pContext(0),
	m_pFreezeLayoutButton(0),
	m_pAutoCreateKeyButton(0),
	m_bIgnoreSplineChangeEvents(false),
	m_hAccelerators(0),
	m_pDropTarget(0)
{
	GetIEditor()->RegisterNotifyListener(this);
}

CFacialJoystickDialog::~CFacialJoystickDialog()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

BOOL CFacialJoystickDialog::OnInitDialog()
{
	BOOL bValue = __super::OnInitDialog();

	m_pDropTarget = new CJoystickDialogDropTarget(this);
	m_pDropTarget->Register(this);

	m_hAccelerators = LoadAccelerators(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDR_FACED_MENU));

	m_snapMargins.push_back(5);
	m_snapMargins.push_back(10);
	m_snapMargins.push_back(20);
	m_snapMargins.push_back(40);

	m_displayedSnapMargin = 0;

	m_ctrl.SetContainer(this);

	CRect rect;
	GetClientRect(&rect);
	m_ctrl.Create(WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, rect, this, 0);
	m_ctrl.ShowWindow(SW_SHOWDEFAULT);

	VERIFY(m_toolbar.CreateToolBar(WS_VISIBLE | WS_CHILD | CBRS_TOOLTIPS | CBRS_ALIGN_TOP, this, AFX_IDW_TOOLBAR));
	VERIFY(m_toolbar.LoadToolBar(IDR_FACE_JOYSTICK_TOOLBAR));
	m_toolbar.SetFlags(xtpFlagAlignTop | xtpFlagStretched);
	m_toolbar.EnableCustomization(FALSE);

	//------------------------------------------------------------------------------------
	// Initialize the toolbar controls.
	//------------------------------------------------------------------------------------
	enum ButtonStyle
	{
		BUTTON_STYLE_TOGGLE,
		BUTTON_STYLE_DROPDOWN,
		BUTTON_STYLE_PUSH,

		BUTTON_STYLE_COUNT
	};

	class ButtonDescription
	{
	public:
		ButtonDescription(ButtonStyle style, int id, const char* caption, CXTPControl** controlPointer)
			: style(style), id(id), caption(caption), controlPointer(controlPointer) {}

		ButtonStyle   style;
		int           id;
		const char*   caption;
		CXTPControl** controlPointer;
	};

	CXTPControl* pSnapMarginControl = 0;
	CXTPControl* pFreezeLayoutButton = 0;
	CXTPControl* pAutoCreateKeyButton = 0;
	ButtonDescription buttonDescriptions[] = {
		ButtonDescription(BUTTON_STYLE_TOGGLE,   ID_FREEZE_LAYOUT,   "Edit Layout",     &pFreezeLayoutButton),
		ButtonDescription(BUTTON_STYLE_TOGGLE,   ID_AUTO_CREATE_KEY, "Auto-Create Key", &pAutoCreateKeyButton),
		ButtonDescription(BUTTON_STYLE_DROPDOWN, ID_SNAP_MARGIN,     "Snap Margin",     &pSnapMarginControl),
		ButtonDescription(BUTTON_STYLE_PUSH,     ID_ZERO_ALL,        "Zero All",        0),
		ButtonDescription(BUTTON_STYLE_PUSH,     ID_KEY_ALL,         "Key All",         0)
	};
	enum {NUM_BUTTONS = sizeof(buttonDescriptions) / sizeof(buttonDescriptions[0])};

	for (int buttonIndex = 0; buttonIndex < NUM_BUTTONS; ++buttonIndex)
	{
		static const XTPControlType controlTypeMap[BUTTON_STYLE_COUNT] = { xtpControlButton, xtpControlComboBox, xtpControlButton };
		XTPControlType controlType = controlTypeMap[buttonDescriptions[buttonIndex].style];
		if (controlType == 0)
			continue;

		CXTPControl* pCtrl = m_toolbar.GetControls()->FindControl(xtpControlButton, buttonDescriptions[buttonIndex].id, TRUE, FALSE);
		if (pCtrl)
		{
			int nIndex = pCtrl->GetIndex();
			CXTPControl* pDerivedControl = (CXTPControl*)m_toolbar.GetControls()->SetControlType(nIndex, controlType);
			pDerivedControl->SetCaption(buttonDescriptions[buttonIndex].caption);
			pDerivedControl->SetStyle(xtpButtonCaption);
			pDerivedControl->SetEnabled(TRUE);
			if (buttonDescriptions[buttonIndex].controlPointer)
				*buttonDescriptions[buttonIndex].controlPointer = pDerivedControl;
		}
	}

	m_pAutoCreateKeyButton = static_cast<CXTPControlButton*>(pAutoCreateKeyButton);
	m_pFreezeLayoutButton = static_cast<CXTPControlButton*>(pFreezeLayoutButton);

	UpdateFreezeLayoutStatus();
	UpdateAutoCreateKeyStatus();

	{
		static_cast<CXTPControlComboBox*>(pSnapMarginControl)->SetStyle(xtpComboLabel);
		int index = 0;
		for (SnapMarginList::iterator itSize = m_snapMargins.begin(); itSize != m_snapMargins.end(); ++itSize)
		{
			int snapMargin = (*itSize);
			string text;
			text.Format("%d", snapMargin);
			static_cast<CXTPControlComboBox*>(pSnapMarginControl)->InsertString(index, text.c_str());
			++index;
		}
	}

	return bValue;
}

void CFacialJoystickDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

void CFacialJoystickDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	RecalcLayout();
	Invalidate();
}

void CFacialJoystickDialog::OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels)
{
	switch (event)
	{
	case EFD_EVENT_SEQUENCE_TIME:
		m_ctrl.UpdatePreview();
		break;

	case EFD_EVENT_CHANGE_RELOAD:
	case EFD_EVENT_JOYSTICK_SET_CHANGED:
		{
			if (m_pContext)
				m_pContext->BindJoysticks();
			RefreshControlJoystickSet();
		}
		break;

	case EFD_EVENT_SEQUENCE_UNDO:
	case EFD_EVENT_SEQUENCE_CHANGE:
		if (m_pContext)
		{
			m_pContext->BindJoysticks();
			m_ctrl.UpdatePreview();
		}
		break;

	case EFD_EVENT_SPLINE_CHANGE:
	case EFD_EVENT_SPLINE_CHANGE_CURRENT:
		if (!m_bIgnoreSplineChangeEvents)
			m_ctrl.Redraw();
		break;
	}
}

void CFacialJoystickDialog::SetContext(CFacialEdContext* pContext)
{
	assert(pContext);
	m_pContext = pContext;
	if (!m_pContext)
		return;

	m_pContext->RegisterListener(this);

	ISystem* pSystem = GetISystem();
	ICharacterManager* pCharacterManager = pSystem ? pSystem->GetIAnimationSystem() : 0;
	IFacialAnimation* pFacialAnimationSystem = pCharacterManager ? pCharacterManager->GetIFacialAnimation() : 0;
	IJoystickContext* pJoystickContext = pFacialAnimationSystem ? pFacialAnimationSystem->GetJoystickContext() : 0;

	if (pJoystickContext)
		m_ctrl.SetJoystickContext(pJoystickContext);

	RefreshControlJoystickSet();
}

void CFacialJoystickDialog::RecalcLayout()
{
	if (!m_ctrl.m_hWnd)
		return;

	CRect rc;
	GetClientRect(rc);

	CSize sz(0, 0);
	sz = m_toolbar.CalcDockingLayout(32000, LM_HORZ | LM_HORZDOCK | LM_STRETCH | LM_COMMIT);
	m_toolbar.MoveWindow(CRect(0, 0, sz.cx, sz.cy), FALSE);

	rc.top += sz.cy;

	m_ctrl.MoveWindow(rc, TRUE);
}

void CFacialJoystickDialog::OnFreezeLayout()
{
	m_ctrl.SetFreezeLayout(!m_ctrl.GetFreezeLayout());
	UpdateFreezeLayoutStatus();
}

void CFacialJoystickDialog::OnSnapMarginChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	ReadDisplayedSnapMargin();
	*pResult = 1;
}

void CFacialJoystickDialog::OnUpdateSnapMarginSizeUI(CCmdUI* pCmdUI)
{
	pCmdUI->Enable();

	int snapMargin = m_ctrl.GetSnapMargin();
	if (m_snapMargins[m_displayedSnapMargin] != snapMargin)
	{
		CXTPControlComboBox* pControl = (CXTPControlComboBox*)m_toolbar.GetControls()->FindControl(xtpControlComboBox, ID_SNAP_MARGIN, TRUE, FALSE);
		if (!pControl || pControl->GetType() != xtpControlComboBox)
			return;

		int sel = CB_ERR;
		for (int index = 0; index < int(m_snapMargins.size()); ++index)
		{
			if (snapMargin == m_snapMargins[index])
				sel = index;
		}

		if (sel != CB_ERR && sel != pControl->GetCurSel())
			pControl->SetCurSel(sel);

		ReadDisplayedSnapMargin();
	}
}

void CFacialJoystickDialog::OnAutoCreateKeyChanged()
{
	m_ctrl.SetAutoCreateKey(!m_ctrl.GetAutoCreateKey());
	UpdateAutoCreateKeyStatus();
}

void CFacialJoystickDialog::OnKeyAll()
{
	m_ctrl.KeyAll();
}

void CFacialJoystickDialog::OnZeroAll()
{
	m_ctrl.ZeroAll();
	if (m_pContext)
		m_pContext->StopPreviewingEffector();
}

void CFacialJoystickDialog::UpdateFreezeLayoutStatus()
{
	if (m_pFreezeLayoutButton)
		m_pFreezeLayoutButton->SetChecked(!m_ctrl.GetFreezeLayout());
}

void CFacialJoystickDialog::UpdateAutoCreateKeyStatus()
{
	if (m_pAutoCreateKeyButton)
		m_pAutoCreateKeyButton->SetChecked(m_ctrl.GetAutoCreateKey());
}

void CFacialJoystickDialog::OnFreezeLayoutChanged()
{
	UpdateFreezeLayoutStatus();
}

IJoystickChannel* CFacialJoystickDialog::GetPotentialJoystickChannel()
{
	ISystem* pSystem = GetISystem();
	ICharacterManager* pCharacterManager = pSystem ? pSystem->GetIAnimationSystem() : 0;
	IFacialAnimation* pFacialAnimationSystem = pCharacterManager ? pCharacterManager->GetIFacialAnimation() : 0;

	IFacialAnimChannel* pChannel = m_pContext ? m_pContext->pSelectedChannel : 0;

	IJoystickChannel* pJoystickChannel = (pFacialAnimationSystem && pChannel ? pFacialAnimationSystem->CreateJoystickChannel(pChannel) : 0);

	return pJoystickChannel;
}

float CFacialJoystickDialog::GetCurrentEvaluationTime()
{
	return m_pContext ? m_pContext->GetSequenceTime() : 0;
}

float CFacialJoystickDialog::GetMaxEvaluationTime()
{
	IFacialAnimSequence* pSequence = (m_pContext ? m_pContext->GetSequence() : 0);
	float maxTime = (pSequence ? pSequence->GetTimeRange().end : 0.0f);
	return maxTime;
}

void CFacialJoystickDialog::OnSplineChanged()
{
	m_pContext->bSequenceModfied = true;
	m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE_CURRENT);
}

void CFacialJoystickDialog::OnJoysticksChanged()
{
	if (m_pContext)
	{
		m_pContext->BindJoysticks();
		m_pContext->bJoysticksModfied = true;
	}
}

void CFacialJoystickDialog::OnAction(JoystickAction action)
{
	EFacialEdEvent event = EFD_EVENT_STOP_EDITTING_JOYSTICKS;
	switch (action)
	{
	case JOYSTICK_ACTION_START:
		event = EFD_EVENT_START_EDITTING_JOYSTICKS;
		break;
	case JOYSTICK_ACTION_END:
		event = EFD_EVENT_STOP_EDITTING_JOYSTICKS;
		break;
	}
	if (m_pContext)
		m_pContext->SendEvent(event);
}

void CFacialJoystickDialog::ReadDisplayedSnapMargin()
{
	CXTPControlComboBox* pControl = (CXTPControlComboBox*)m_toolbar.GetControls()->FindControl(xtpControlComboBox, ID_SNAP_MARGIN, TRUE, FALSE);
	if (!pControl || pControl->GetType() != xtpControlComboBox)
		return;

	if (pControl->GetCurSel() != CB_ERR)
	{
		int selection = pControl->GetCurSel();
		if (m_displayedSnapMargin != selection)
		{
			m_displayedSnapMargin = selection;
			m_ctrl.SetSnapMargin(m_snapMargins[m_displayedSnapMargin]);
		}
	}
}

void CFacialJoystickDialog::Update()
{
	if (m_ctrl)
		m_ctrl.Update();
}

void CFacialJoystickDialog::OnBeginDraggingJoystickKnob(IJoystick* pJoystick)
{
	IFacialAnimChannel* channels[2];
	int numChannels = 0;
	for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
	{
		IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
		IFacialAnimChannel* pFacialChannel = (pChannel ? (IFacialAnimChannel*)pChannel->GetTarget() : 0);
		if (pFacialChannel)
			channels[numChannels++] = pFacialChannel;
	}

	if (m_pContext)
	{
		m_pContext->StopPreviewingEffector();
		m_pContext->SendEvent(EFD_EVENT_START_CHANGING_SPLINES, 0, numChannels, channels);
	}
}

void CFacialJoystickDialog::OnJoystickSelected(IJoystick* pJoystick, bool exclusive)
{
	IFacialAnimChannel* channels[2];
	int numChannels = 0;
	for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
	{
		IJoystickChannel* pChannel = (pJoystick ? pJoystick->GetChannel(axis) : 0);
		IFacialAnimChannel* pFacialChannel = (pChannel ? (IFacialAnimChannel*)pChannel->GetTarget() : 0);
		if (pFacialChannel)
			channels[numChannels++] = pFacialChannel;
	}

	if (m_pContext)
	{
		m_pContext->StopPreviewingEffector();
		m_pContext->SendEvent(EFD_SPLINES_NEED_ACTIVATING, 0, numChannels, channels);
	}
}

bool CFacialJoystickDialog::GetPlaying() const
{
	return (m_pContext ? m_pContext->IsPlayingSequence() : 0);
}

void CFacialJoystickDialog::SetPlaying(bool playing)
{
	if (m_pContext && m_pContext->IsPlayingSequence() != playing)
		m_pContext->PlaySequence(playing);
}

void CFacialJoystickDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		Update();
		break;
	}
}

void CFacialJoystickDialog::RefreshControlJoystickSet()
{
	IJoystickSet* pJoystickSet = (m_pContext ? m_pContext->GetJoystickSet() : 0);
	m_ctrl.SetJoystickSet(pJoystickSet);
}

BOOL CFacialJoystickDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST && m_hAccelerators)
		return ::TranslateAccelerator(m_hWnd, m_hAccelerators, pMsg);
	return CDialog::PreTranslateMessage(pMsg);
}

BOOL CFacialJoystickDialog::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CFacialEdContext::MoveToKeyDirection direction = (zDelta < 0 ? CFacialEdContext::MoveToKeyDirectionBackward : CFacialEdContext::MoveToKeyDirectionForward);
	if (m_pContext)
		m_pContext->MoveToFrame(direction);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFacialJoystickDialog::OnDestroy()
{
	if (m_pDropTarget)
		delete m_pDropTarget;
}

