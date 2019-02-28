// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAttachment.h>
#include "EffectorInfoWnd.h"
#include "Controls/SplineCtrl.h"
#include "Controls/SharedFonts.h"
#include <CrySystem/ITimer.h>
#include "Util/MFCUtil.h"
#include "Resource.h"
#include "Dialogs/QNumericBoxDialog.h"

#define IDC_TASKPANEL                 1
#define IDC_EXPRESSION_WEIGHT_SLIDER  2
#define IDC_EXPRESSION_BALANCE_SLIDER 3
#define IDC_SPLINES                   4

#define SLIDER_SCALE                  100

#define FIRST_SLIDER_ID               1000
#define PROPERTIES_WINDOW_TOP         80

//////////////////////////////////////////////////////////////////////////
class CSplineCtrlContainer : public CSplineCtrl
{
public:
	virtual void PostNcDestroy() { delete this; };
};

//////////////////////////////////////////////////////////////////////////
class CFacialControllerContainerDialog : public CToolbarDialog
{
public:
	enum { IDD = IDD_DATABASE };
	CFacialControllerContainerDialog() : CToolbarDialog(IDD, 0) {};

	DECLARE_MESSAGE_MAP()

	BOOL OnInitDialog()
	{
		BOOL res = __super::OnInitDialog();

		//////////////////////////////////////////////////////////////////////////
		VERIFY(m_wndToolBar.CreateToolBar(WS_VISIBLE | WS_CHILD | CBRS_TOOLTIPS | CBRS_LEFT | CBRS_ORIENT_VERT, this, AFX_IDW_TOOLBAR));
		VERIFY(m_wndToolBar.LoadToolBar(IDR_FACEIT_SPLINE_BAR));
		//m_wndToolBar.SetParent( this );
		//m_wndToolBar.SetOwner( GetParent()->GetParent() );
		//m_wndToolBar.SetFlags(xtpFlagAlignLeft);
		//m_wndToolBar.EnableCustomization(FALSE);
		return res;
	}
	virtual void PostNcDestroy() { delete this; };
	afx_msg void OnSize(UINT nType, int cx, int cy)
	{
		__super::OnSize(nType, cx, cy);
		if (m_wndToolBar.m_hWnd)
		{
			// get maximum requested size
			DWORD dwMode = LM_HORZ | LM_HORZDOCK | LM_STRETCH | LM_COMMIT;
			CSize sz = m_wndToolBar.CalcDockingLayout(32000, dwMode);
			//CSize sz = m_wndToolBar.CalcFixedLayout( TRUE,FALSE );
			m_wndToolBar.MoveWindow(CRect(0, 0, sz.cx, sz.cy), FALSE);

			CPoint ptOffset(sz.cx, 0);

			/*
			   CRect clientRect;
			   GetClientRect(clientRect);
			   clientRect.left = sz.cx;

			   CWnd *pwndChild = GetWindow(GW_CHILD);
			   while (pwndChild)
			   {
			   UINT nID = pwndChild->GetDlgCtrlID();
			   if (nID >= AFX_IDW_CONTROLBAR_FIRST && nID < AFX_IDW_CONTROLBAR_LAST)
			   {
			    pwndChild = pwndChild->GetNextWindow();
			    continue;
			   }
			   pwndChild->MoveWindow(clientRect,FALSE);
			   pwndChild = pwndChild->GetNextWindow();
			   }
			 */
		}
	}
	afx_msg void OnSplineEnable()
	{
	}
public:
	CXTPToolBar m_wndToolBar;
};

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CFacialControllerContainerDialog, CToolbarDialog)
ON_WM_SIZE()
ON_COMMAND(ID_FACEIT_SPLNIE_ENABLE, OnSplineEnable)
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CFacialAttachmentEffectorUI
{
public:
	_smart_ptr<CVarBlock>       m_pVarBlock;
	IFacialEffector*            m_pEffector;
	CFacialEdContext*           m_pContext;

	CSmartVariableEnum<CString> mv_attachment;
	CSmartVariable<Vec3>        mv_posOffset;
	CSmartVariable<Vec3>        mv_rotOffset;

	CFacialAttachmentEffectorUI()
	{
	}
	void Attach(CFacialEdContext* pContext, CPropertyCtrl* pPropsCtrl, IFacialEffector* pEffector)
	{
		m_pContext = pContext;
		m_pEffector = pEffector;

		m_pVarBlock = new CVarBlock;

		m_pVarBlock->AddVariable(mv_attachment, "Attachment/Bone");
		m_pVarBlock->AddVariable(mv_posOffset, "Position Offset");
		m_pVarBlock->AddVariable(mv_rotOffset, "Rotation Angle");
		mv_rotOffset->SetLimits(-360, 360);

		mv_attachment = (CString)m_pEffector->GetParamString(EFE_PARAM_BONE_NAME);
		mv_posOffset = m_pEffector->GetParamVec3(EFE_PARAM_BONE_POS_AXIS);
		mv_rotOffset = RAD2DEG(m_pEffector->GetParamVec3(EFE_PARAM_BONE_ROT_AXIS));

		UpdateVars();

		pPropsCtrl->RemoveAllItems();
		pPropsCtrl->SetUpdateCallback(functor(*this, &CFacialAttachmentEffectorUI::OnVarChange));
		pPropsCtrl->AddVarBlock(m_pVarBlock, pEffector->GetName());
		pPropsCtrl->ExpandAll();
	}
	void OnVarChange(IVariable* pVar)
	{
		if (m_pEffector && m_pContext->pSelectedEffector == m_pEffector)
		{
			CString sParamName = mv_attachment;
			Vec3 vRot = mv_rotOffset;
			vRot = DEG2RAD(vRot);
			m_pEffector->SetParamString(EFE_PARAM_BONE_NAME, sParamName);
			m_pEffector->SetParamVec3(EFE_PARAM_BONE_POS_AXIS, mv_posOffset);
			m_pEffector->SetParamVec3(EFE_PARAM_BONE_ROT_AXIS, vRot);
			m_pContext->SetModified(m_pEffector);
		}
	}
	virtual void UpdateVars()
	{
		// Fill attachments combo with character attachments.
		if (m_pContext && m_pContext->pCharacter)
		{
			int nAttachments = m_pContext->pCharacter->GetIAttachmentManager()->GetAttachmentCount();
			for (int i = 0; i < nAttachments; i++)
			{
				IAttachment* pAttachment = m_pContext->pCharacter->GetIAttachmentManager()->GetInterfaceByIndex(i);
				if (pAttachment)
				{
					mv_attachment.AddEnumItem(pAttachment->GetName(), pAttachment->GetName());
				}
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
class CFacialBoneEffectorUI : public CFacialAttachmentEffectorUI
{
public:
	virtual void UpdateVars()
	{
		// Fill attachments combo with character attachments.

		if (m_pContext && m_pContext->pCharacter)
		{
			IDefaultSkeleton& rIDefaultSkeleton = m_pContext->pCharacter->GetIDefaultSkeleton();
			int nBones = rIDefaultSkeleton.GetJointCount();
			for (int i = 0; i < nBones; i++)
			{
				const char* sBoneName = rIDefaultSkeleton.GetJointNameByID(i);
				mv_attachment.AddEnumItem(sBoneName, sBoneName);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CEffectorInfoWnd, CToolbarDialog)
ON_WM_SIZE()
ON_WM_HSCROLL()
ON_NOTIFY_RANGE(NM_RCLICK, FIRST_SLIDER_ID, FIRST_SLIDER_ID + 1000, OnSplineRClick)
ON_NOTIFY_RANGE(SPLN_CHANGE, FIRST_SLIDER_ID, FIRST_SLIDER_ID + 1000, OnSplineChange)
ON_NOTIFY_RANGE(SPLN_BEFORE_CHANGE, FIRST_SLIDER_ID, FIRST_SLIDER_ID + 1000, OnBeforeSplineChange)
ON_COMMAND(ID_FACEIT_PLAY_EXPRESSION, OnPlayAnim)
ON_UPDATE_COMMAND_UI(ID_FACEIT_PLAY_EXPRESSION, OnUpdatePlayAnim)
ON_COMMAND(ID_FACEIT_PLAY_FROM_0, OnPlayAnimFrom0)
ON_UPDATE_COMMAND_UI(ID_FACEIT_PLAY_FROM_0, OnUpdatePlayAnimFrom0)
ON_COMMAND(ID_FACEIT_GOTO_MINUS1, OnGotoMinus1)
ON_COMMAND(ID_FACEIT_GOTO_0, OnGoto0)
ON_COMMAND(ID_FACEIT_GOTO_1, OnGoto1)
ON_WM_MEASUREITEM()
ON_WM_DRAWITEM()
//ON_EN_CHANGE(ID_FACEIT_CURRENT_POS,OnChangeCurrentTime)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
/// CXTPTaskPanelOffice2003ThemePlain

class CEffectorControllersTheme : public CXTPTaskPanelPaintManagerPlain
{
public:
	CEffectorControllersTheme()
	{
		m_bRoundedFrame = FALSE;
		m_bOfficeCaption = TRUE;
		m_bOfficeHighlight = TRUE;
		RefreshMetrics();
	}
	void RefreshMetrics()
	{
		CXTPTaskPanelPaintManager::RefreshMetrics();

		m_eGripper = xtpTaskPanelGripperNone;

		COLORREF clrBackground = GetXtremeColor(XPCOLOR_MENUBAR_FACE);
		COLORREF clr3DShadow = GetXtremeColor(COLOR_3DSHADOW);

		m_clrBackground = CXTPPaintManagerColorGradient(clrBackground, clrBackground);

		if (!XTPColorManager()->IsLunaColorsDisabled())
		{
			XTPCurrentSystemTheme systemTheme = XTPColorManager()->GetCurrentSystemTheme();

			switch (systemTheme)
			{
			case xtpSystemThemeBlue:
				m_clrBackground = m_groupNormal.clrClient = (RGB(216, 231, 252));
				break;
			case xtpSystemThemeOlive:
				m_clrBackground = m_groupNormal.clrClient = (RGB(226, 231, 191));
				break;
			case xtpSystemThemeSilver:
				m_clrBackground = m_groupNormal.clrClient = (RGB(223, 223, 234));
				break;
			}
		}

		m_groupNormal.clrClient = clrBackground;
		m_groupNormal.clrHead = CXTPPaintManagerColorGradient(clr3DShadow, clr3DShadow);
		m_groupNormal.clrClientLink = m_groupNormal.clrClientLinkHot = RGB(0, 0, 0xFF);

		m_groupSpecial = m_groupNormal;
	}
};

//////////////////////////////////////////////////////////////////////////
CEffectorInfoWnd::CEffectorInfoWnd()
	: CToolbarDialog(IDD_DATABASE, NULL)
{
	m_pContext = NULL;
	m_bAnimation = false;
	m_bPlayFrom0 = false;
	m_bIgnoreScroll = false;
	m_pCurrPosCtrl = 0;
	GetIEditor()->RegisterNotifyListener(this);

	m_nSplineHeight = 80;
	m_nSplineHeight = AfxGetApp()->GetProfileInt("Dialogs\\FaceEd", "SplineHeight", m_nSplineHeight);

	m_pAttachEffectorUI = 0;
}

//////////////////////////////////////////////////////////////////////////
CEffectorInfoWnd::~CEffectorInfoWnd()
{
	AfxGetApp()->WriteProfileInt("Dialogs\\FaceEd", "SplineHeight", m_nSplineHeight);

	delete m_pAttachEffectorUI;
	GetIEditor()->UnregisterNotifyListener(this);
	ClearCtrls();
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::Create(CWnd* pParent)
{
	__super::Create(IDD_DATABASE, pParent);
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::RecalcLayout()
{
	if (m_wndTaskPanel.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);

		CSize sz = m_wndToolBar.CalcDockingLayout(32000, LM_HORZ | LM_HORZDOCK | LM_STRETCH | LM_COMMIT);
		m_wndToolBar.MoveWindow(CRect(0, 0, sz.cx, sz.cy), FALSE);

		rc.top += sz.cy;

		int sh = 20;
		m_weightSlider.MoveWindow(CRect(rc.left, rc.top + 10, rc.right, rc.top + 10 + sh), FALSE);
		m_textMinus100.MoveWindow(CRect(rc.left + 10, rc.top + 32, rc.left + 100, rc.top + 45), FALSE);
		m_textPlus100.MoveWindow(CRect(rc.right - 100, rc.top + 32, rc.right - 10, rc.top + 45), FALSE);
		m_text0.MoveWindow(CRect((rc.right + rc.left) / 2 - 4, rc.top + 32, (rc.right + rc.left) / 2 + 20, rc.top + 45), FALSE);

		//m_balanceSlider.ShowWindow(FALSE);
		//m_balanceSlider.MoveWindow( CRect(rc.left,rc.top+10,rc.right,rc.top+10+sh),FALSE );

		GetClientRect(rc);
		rc.top += PROPERTIES_WINDOW_TOP;
		m_wndTaskPanel.MoveWindow(rc, FALSE);
		//		m_wndSplines.MoveWindow(rc,FALSE);
		m_wndProperties.MoveWindow(rc, FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::SetContext(CFacialEdContext* pContext)
{
	m_pContext = pContext;
	if (pContext)
		pContext->RegisterListener(this);

	ReloadCtrls();
}

//////////////////////////////////////////////////////////////////////////
BOOL CEffectorInfoWnd::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();

	CRect rc;
	GetClientRect(rc);

	//////////////////////////////////////////////////////////////////////////
	VERIFY(m_wndToolBar.CreateToolBar(WS_VISIBLE | WS_CHILD | CBRS_TOOLTIPS | CBRS_ALIGN_TOP, this, AFX_IDW_TOOLBAR));
	VERIFY(m_wndToolBar.LoadToolBar(IDR_FACEIT_PLAY_BAR));
	m_wndToolBar.SetFlags(xtpFlagAlignTop | xtpFlagStretched);
	m_wndToolBar.EnableCustomization(FALSE);

	{
		CXTPControl* pCtrl = m_wndToolBar.GetControls()->FindControl(xtpControlButton, ID_FACEIT_CURRENT_POS, TRUE, FALSE);
		if (pCtrl)
		{
			int nIndex = pCtrl->GetIndex();
			m_pCurrPosCtrl = (CXTPControlLabel*)m_wndToolBar.GetControls()->SetControlType(nIndex, xtpControlLabel);
			m_pCurrPosCtrl->SetCaption("00.00");
			m_pCurrPosCtrl->SetFlags(xtpFlagManualUpdate);
			//m_pCurrPosCtrl->SetWidth(40);
		}
	}

	m_weightSlider.Create(WS_CHILD | WS_VISIBLE | TBS_BOTH | TBS_AUTOTICKS | TBS_NOTICKS, CRect(0, 0, 1, 1), this, IDC_EXPRESSION_WEIGHT_SLIDER);
	m_weightSlider.SetRange(-SLIDER_SCALE, SLIDER_SCALE);
	m_weightSlider.SetTicFreq(10);

	m_balanceSlider.Create(WS_CHILD | WS_VISIBLE | TBS_BOTH | TBS_AUTOTICKS | TBS_NOTICKS, CRect(0, 0, 1, 1), this, IDC_EXPRESSION_BALANCE_SLIDER);
	m_balanceSlider.SetRange(-SLIDER_SCALE, SLIDER_SCALE);
	m_balanceSlider.SetTicFreq(10);
	m_balanceSlider.ShowWindow(FALSE);

	m_textMinus100.Create("-1", WS_CHILD | WS_VISIBLE, rc, this, IDC_STATIC);
	m_textMinus100.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_textPlus100.Create("+1", WS_CHILD | WS_VISIBLE | SS_RIGHT, rc, this, IDC_STATIC);
	m_textPlus100.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));
	m_text0.Create("0", WS_CHILD | WS_VISIBLE, rc, this, IDC_STATIC);
	m_text0.SetFont(CFont::FromHandle((HFONT)SMFCFonts::GetInstance().hSystemFont));

	//////////////////////////////////////////////////////////////////////////
	m_wndTaskPanel.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, rc, this, IDC_TASKPANEL);
	m_wndTaskPanel.ModifyStyleEx(0, WS_EX_CLIENTEDGE);

	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	//m_wndTaskPanel.SetTheme(xtpTaskPanelThemeListViewOffice2003);
	//m_wndTaskPanel.SetHotTrackStyle(xtpTaskPanelHighlightItem);
	//m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(FALSE);

	m_wndTaskPanel.SetAnimation(xtpTaskPanelAnimationNo);
	m_wndTaskPanel.SetCustomTheme(new CEffectorControllersTheme);

	m_wndTaskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(0, 0, 0, 0);
	m_wndTaskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(0, 0, 0, 0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(0, 0, 0, 0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(0, 0, 0, 0);
	m_wndTaskPanel.GetPaintManager()->m_rcControlMargins.SetRect(0, 0, 0, 0);
	m_wndTaskPanel.GetPaintManager()->m_nGroupSpacing = 0;

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_FACED_LIBTREE, 16, RGB(255, 0, 255));
	m_wndTaskPanel.SetGroupImageList(&m_imageList, CSize(16, 16));
	//////////////////////////////////////////////////////////////////////////

	m_wndProperties.Create(WS_CHILD, CRect(0, 0, 0, 0), this, IDC_PROPERTIES);

	//	m_wndSplines.Create( LBS_OWNERDRAWFIXED|WS_VISIBLE|WS_CHILD|WS_TABSTOP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rc,this,IDC_SPLINES );
	//	m_wndSplines.SetItemHeight(0,80);

	ReloadCtrls();
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::ClearCtrls()
{
	if (m_wndTaskPanel.m_hWnd)
		m_wndTaskPanel.GetGroups()->Clear();
	m_controllers.clear();
}

//////////////////////////////////////////////////////////////////////////
inline bool SortSubEfectorsByName(IFacialEffCtrl* p1, IFacialEffCtrl* p2)
{
	return stricmp(p1->GetEffector()->GetName(), p2->GetEffector()->GetName()) < 0;
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::ReloadCtrls()
{
	ClearCtrls();

	if (!m_pContext || !m_pContext->pSelectedEffector)
		return;

	SetWeight(1.0f);

	IFacialEffector* pEffector = m_pContext->pSelectedEffector;

	std::vector<IFacialEffCtrl*> sortedSubEffectors;
	int nNum = pEffector->GetSubEffectorCount();
	sortedSubEffectors.resize(nNum);
	for (int i = 0; i < nNum; i++)
	{
		sortedSubEffectors[i] = pEffector->GetSubEffCtrl(i);
	}
	std::sort(sortedSubEffectors.begin(), sortedSubEffectors.end(), SortSubEfectorsByName);

	int nCommand = 0;
	for (int i = 0; i < (int)sortedSubEffectors.size(); i++)
	{
		IFacialEffCtrl* pCtrl = sortedSubEffectors[i];

		int nImage = 0;
		EFacialEffectorType eftype = pCtrl->GetEffector()->GetType();
		switch (eftype)
		{
		case EFE_TYPE_GROUP:
			nImage = 0;
		case EFE_TYPE_EXPRESSION:
			nImage = 1;
		case EFE_TYPE_MORPH_TARGET:
		default:
			nImage = 2;
		}

		nCommand++;
		CXTPTaskPanelGroup* pFolder = m_wndTaskPanel.AddGroup(nCommand, nImage);
		pFolder->SetCaption(pCtrl->GetEffector()->GetName());
		pFolder->SetItemLayout(xtpTaskItemLayoutDefault);
		pFolder->SetExpanded(FALSE);

		CRect sliderRc(0, 0, 1, m_nSplineHeight);
		CSplineCtrlContainer* pSplineCtrl = new CSplineCtrlContainer;
		pSplineCtrl->Create(WS_CHILD | WS_BORDER, sliderRc, this, FIRST_SLIDER_ID + nCommand);
		pSplineCtrl->ModifyStyleEx(0, WS_EX_CLIENTEDGE);
		pSplineCtrl->SetParent(&m_wndTaskPanel);
		pSplineCtrl->SetOwner(this);

		pSplineCtrl->SetSpline(pCtrl->GetSpline());

		ControllerInfo ci;
		ci.pSplineCtrl = pSplineCtrl;
		ci.pCtrl = pCtrl;
		m_controllers.push_back(ci);

		CXTPTaskPanelGroupItem* pItem = pFolder->AddControlItem(*pSplineCtrl);
		//pItem->GetMargins().SetRect(0,0,0,0);

		pSplineCtrl->MoveWindow(sliderRc);
		//m_wndTaskPanel.ExpandGroup( pFolder,TRUE );

		//////////////////////////////////////////////////////////////////////////
		//int nItemId = m_wndSplines.AddString( "test" );
		//m_wndSplines.SetItemControl( nItemId,pSplineCtrl );

		//////////////////////////////////////////////////////////////////////////
		pSplineCtrl->ShowWindow(SW_SHOW);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (m_bIgnoreScroll)
		return;
	if (pScrollBar == (CScrollBar*)&m_weightSlider)
	{
		float fWeight = (float)m_weightSlider.GetPos() / SLIDER_SCALE;
		SetWeight(fWeight, m_fCurrBalance);

		// Send a special facial event to redraw preview window to make preview smooth.
		if (m_pContext)
			m_pContext->SendEvent(EFD_EVENT_REDRAW_PREVIEW);
	}
	else if (pScrollBar == (CScrollBar*)&m_balanceSlider)
	{
		float fBalance = (float)m_balanceSlider.GetPos() / SLIDER_SCALE;
		SetWeight(m_fCurrWeight, fBalance);

		// Send a special facial event to redraw preview window to make preview smooth.
		if (m_pContext)
			m_pContext->SendEvent(EFD_EVENT_REDRAW_PREVIEW);
	}
}

//////////////////////////////////////////////////////////////////////////
#define MENU_LINEAR        1
#define MENU_SPLINE_1      2
#define MENU_WEIGHT        3
#define MENU_SPLINE_HEIGHT 4

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnSplineRClick(UINT nID, NMHDR* pNMHDR, LRESULT* lpResult)
{
	if (nID >= FIRST_SLIDER_ID && nID <= FIRST_SLIDER_ID + 1000)
	{
		CWnd* pWnd = CWnd::FromHandle(pNMHDR->hwndFrom);
		if (pWnd->IsKindOf(RUNTIME_CLASS(CSplineCtrl)))
		{
			CSplineCtrl* pSlineCtrl = (CSplineCtrl*)pWnd;

			for (int i = 0; i < (int)m_controllers.size(); i++)
			{
				if (m_controllers[i].pSplineCtrl == pSlineCtrl)
				{
					IFacialEffCtrl* pCtrl = m_controllers[i].pCtrl;

					CMenu menu;
					VERIFY(menu.CreatePopupMenu());

					// create main menu items
					menu.AppendMenu(MF_STRING | ((pCtrl->GetType() == IFacialEffCtrl::CTRL_LINEAR) ? MF_CHECKED : 0), MENU_LINEAR, _T("Linear"));
					menu.AppendMenu(MF_STRING | ((pCtrl->GetType() == IFacialEffCtrl::CTRL_SPLINE) ? MF_CHECKED : 0), MENU_SPLINE_1, _T("Spline"));
					menu.AppendMenu(MF_SEPARATOR);
					menu.AppendMenu(MF_STRING | ((pCtrl->GetType() == IFacialEffCtrl::CTRL_LINEAR) ? MF_ENABLED : MF_DISABLED), MENU_WEIGHT, _T("Change Weight"));
					menu.AppendMenu(MF_SEPARATOR);
					menu.AppendMenu(MF_STRING, MENU_SPLINE_HEIGHT, _T("Adjust Control Height..."));

					CPoint pos;
					GetCursorPos(&pos);
					int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY, pos.x, pos.y, this);
					switch (cmd)
					{
					case MENU_LINEAR:
						pCtrl->SetType(IFacialEffCtrl::CTRL_LINEAR);
						pSlineCtrl->Invalidate();
						break;
					case MENU_SPLINE_1:
						pCtrl->SetType(IFacialEffCtrl::CTRL_SPLINE);
						pSlineCtrl->Invalidate();
						break;
					case MENU_WEIGHT:
						{
							QNumericBoxDialog dlg(QObject::tr("Change Weight"), pCtrl->GetConstantWeight());
							if (dlg.exec() == QDialog::Accepted)
							{
								float val = dlg.GetValue();
								val = min(max(val, -1.0f), 1.0f);
								pCtrl->SetConstantWeight(val);
							}
							pSlineCtrl->Invalidate();
						}
						break;
					case MENU_SPLINE_HEIGHT:
						{
							QNumericBoxDialog dlg(QObject::tr("Change Control Height"), m_nSplineHeight);
							dlg.RestrictToInt();
							if (dlg.exec() == QDialog::Accepted)
							{
								m_nSplineHeight = dlg.GetValue();
								if (m_nSplineHeight < 10)
									m_nSplineHeight = 10;
								if (m_nSplineHeight > 1000)
									m_nSplineHeight = 1000;
								AfxGetApp()->WriteProfileInt("Dialogs\\FaceEd", "SplineHeight", m_nSplineHeight);
								ReloadCtrls();
								return;
							}
						}
						break;
					}
					break;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnBeforeSplineChange(UINT nID, NMHDR* pNMHDR, LRESULT* lpResult)
{
	if (m_pContext)
		m_pContext->StoreLibraryUndo();
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnSplineChange(UINT nID, NMHDR* pNMHDR, LRESULT* lpResult)
{
	if (!m_pContext || !m_pContext->pSelectedEffector)
		return;

	if (nID >= FIRST_SLIDER_ID && nID <= FIRST_SLIDER_ID + 1000)
	{
		CWnd* pWnd = CWnd::FromHandle(pNMHDR->hwndFrom);
		if (pWnd->IsKindOf(RUNTIME_CLASS(CSplineCtrl)))
		{
			CSplineCtrl* pSlineCtrl = (CSplineCtrl*)pWnd;

			for (int i = 0; i < (int)m_controllers.size(); i++)
			{
				if (m_controllers[i].pSplineCtrl == pSlineCtrl)
				{
					IFacialEffCtrl* pCtrl = m_controllers[i].pCtrl;
					m_pContext->SetModified(m_pContext->pSelectedEffector);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::SetWeight(float fWeight, float fBalance)
{
	m_fCurrWeight = fWeight;
	m_fCurrBalance = fBalance;

	m_bIgnoreScroll = true;

	{
		int p = m_fCurrWeight * SLIDER_SCALE;
		m_weightSlider.SetPos(p);
		if (p < 0)
			m_weightSlider.SetSelection(p, 0);
		else if (p > 0)
			m_weightSlider.SetSelection(0, p);
	}
	{
		int p = m_fCurrBalance * SLIDER_SCALE;
		m_balanceSlider.SetPos(p);
		if (p < 0)
			m_balanceSlider.SetSelection(p, 0);
		else if (p > 0)
			m_balanceSlider.SetSelection(0, p);
	}

	m_bIgnoreScroll = false;

	if (m_pContext && m_pContext->pInstance && m_pContext->pSelectedEffector)
		m_pContext->pInstance->PreviewEffector(m_pContext->pSelectedEffector, fWeight, fBalance);

	for (int i = 0; i < (int)m_controllers.size(); i++)
	{
		m_controllers[i].pSplineCtrl->SetTimeMarker(fWeight);
	}

	if (m_pCurrPosCtrl)
	{
		CString str;
		str.Format("%3.2f", m_fCurrWeight);
		m_pCurrPosCtrl->SetCaption(str);
		m_wndToolBar.InvalidateRect(m_pCurrPosCtrl->GetRect());
	}

	if (m_pContext)
		m_pContext->SetPreviewWeight(fWeight);
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnSelectEffector(IFacialEffector* pEffector)
{
	//RecalcLayout();
	if (pEffector)
	{
		switch (pEffector->GetType())
		{
		case EFE_TYPE_EXPRESSION:
			ReloadCtrls();
			m_wndProperties.ShowWindow(SW_HIDE);
			m_wndTaskPanel.ShowWindow(SW_SHOW);
			m_wndProperties.DeleteAllItems();
			break;
		case EFE_TYPE_ATTACHMENT:
			ClearCtrls();
			m_wndTaskPanel.ShowWindow(SW_HIDE);
			m_wndProperties.ShowWindow(SW_SHOW);
			if (m_pAttachEffectorUI)
				delete m_pAttachEffectorUI;
			m_pAttachEffectorUI = new CFacialAttachmentEffectorUI;
			m_pAttachEffectorUI->Attach(m_pContext, &m_wndProperties, pEffector);
			break;
		case EFE_TYPE_BONE:
			ReloadCtrls();
			m_wndTaskPanel.ShowWindow(SW_HIDE);
			m_wndProperties.ShowWindow(SW_SHOW);
			if (m_pAttachEffectorUI)
				delete m_pAttachEffectorUI;
			m_pAttachEffectorUI = new CFacialBoneEffectorUI;
			m_pAttachEffectorUI->Attach(m_pContext, &m_wndProperties, pEffector);
			break;
		default:
			ClearCtrls();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnFacialEdEvent(EFacialEdEvent event, IFacialEffector* pEffector, int nChannelCount, IFacialAnimChannel** ppChannels)
{
	switch (event)
	{
	case EFD_EVENT_LIBRARY_CHANGE:
	case EFD_EVENT_LIBRARY_UNDO:
		{
			ClearCtrls();
		}
		break;
	case EFD_EVENT_SELECT_EFFECTOR:
		{
			OnSelectEffector(pEffector);
		}
		break;
	case EFD_EVENT_CHANGE:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnPlayAnim()
{
	m_bAnimation = !m_bAnimation;
	m_fCurrWeight = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnUpdatePlayAnim(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bAnimation);
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnPlayAnimFrom0()
{
	m_bPlayFrom0 = !m_bPlayFrom0;
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnUpdatePlayAnimFrom0(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bPlayFrom0);
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnGotoMinus1()
{
	SetWeight(-1, m_fCurrBalance);
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnGoto0()
{
	SetWeight(0, m_fCurrBalance);
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnGoto1()
{
	SetWeight(1, m_fCurrBalance);
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (m_bAnimation)
	{
		if (event == eNotify_OnIdleUpdate)
		{
			float dt = gEnv->pTimer->GetFrameTime();
			float fWeight = m_fCurrWeight + dt;
			if (fWeight > 1)
				fWeight = -1;
			if (fWeight < -1)
				fWeight = 1;
			if (m_bPlayFrom0 && fWeight < 0)
				fWeight = 0;
			SetWeight(fWeight, m_fCurrBalance);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnChangeCurrentTime()
{
	//	SetWeight( atof(m_pCurrPosCtrl->GetEditText()) );
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnMeasureItemSplines(LPMEASUREITEMSTRUCT pMeasureItem)
{
	if (pMeasureItem->CtlID == IDC_SPLINES)
	{
		pMeasureItem->itemWidth = 400;
		pMeasureItem->itemHeight = 100;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEffectorInfoWnd::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl == IDC_SPLINES)
	{

	}
}

