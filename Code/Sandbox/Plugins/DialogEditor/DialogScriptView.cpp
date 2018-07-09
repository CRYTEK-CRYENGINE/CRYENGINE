// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DialogScriptView.h"
#include "DialogManager.h"
#include "DialogScriptRecord.h"
#include "DialogEditorDialog.h"
#include <CryString/StringUtils.h>
#include <CryAudio/IAudioSystem.h>
#include <CryMath/Cry_Camera.h>
#include "Controls/QuestionDialog.h"
#include <CryAudio/IObject.h>

#include "Resource.h"

//////////////////////////////////////////////////////////////////////////

AFX_INLINE BOOL MyCXTPReportInplaceList::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
	return CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
}
AFX_INLINE BOOL MyCXTPReportInplaceList::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	return CListBox::Create(dwStyle, rect, pParentWnd, nID);
}
//////////////////////////////////////////////////////////////////////////
// CXTPReportInplaceList

MyCXTPReportInplaceList::MyCXTPReportInplaceList()
{
}

MyCXTPReportInplaceList::~MyCXTPReportInplaceList()
{
}

void MyCXTPReportInplaceList::Create(XTP_REPORTRECORDITEM_ARGS* pItemArgs, CXTPReportRecordItemConstraints* pConstaints)
{
	SetItemArgs(pItemArgs);

	CRect rect(pItemArgs->rcItem);

	if (!m_hWnd)
	{
		CListBox::CreateEx(WS_EX_TOOLWINDOW | (pControl->GetExStyle() & WS_EX_LAYOUTRTL), _T("LISTBOX"), _T(""), LBS_NOTIFY | WS_CHILD | WS_BORDER | WS_VSCROLL, CRect(0, 0, 0, 0), pControl, 0);
		SetOwner(pControl);
	}

	SetFont(pControl->GetPaintManager()->GetTextFont());
	ResetContent();

	int dx = rect.right - rect.left + 1;

	CWindowDC dc(pControl);
	CXTPFontDC font(&dc, GetFont());
	int nThumbLength = GetSystemMetrics(SM_CXHTHUMB);

	CString strCaption = pItem->GetCaption(pColumn);
	DWORD dwData = pItem->GetSelectedConstraintData(pItemArgs);

	for (int i = 0; i < pConstaints->GetCount(); i++)
	{
		CXTPReportRecordItemConstraint* pConstaint = pConstaints->GetAt(i);
		CString str = pConstaint->m_strConstraint;
		int nIndex = AddString(str);
		SetItemDataPtr(nIndex, pConstaint);

		dx = MAX(dx, dc.GetTextExtent(str).cx + nThumbLength);

		if ((dwData == (DWORD)-1 && strCaption == str) || (dwData == pConstaint->m_dwData))
			SetCurSel(nIndex);
	}

	int nHeight = GetItemHeight(0);
	rect.top = rect.bottom;
	rect.bottom += nHeight * min(10, GetCount()) + 2;
	rect.left = rect.right - dx;

	pControl->ClientToScreen(&rect);

	CRect rcWork = XTPMultiMonitor()->GetWorkArea(&rect);
	if (rect.bottom > rcWork.bottom && rect.top > rcWork.CenterPoint().y)
	{
		rect.OffsetRect(0, -rect.Height() - pItemArgs->rcItem.Height());
	}

	if (rect.left < rcWork.left) rect.OffsetRect(rcWork.left - rect.left, 0);
	if (rect.right > rcWork.right) rect.OffsetRect(rcWork.right - rect.right, 0);

	SetFocus();

	SetWindowLongPtr(m_hWnd, GWLP_HWNDPARENT, (LONG_PTR) 0);
	ModifyStyle(WS_CHILD, WS_POPUP);
	SetWindowLongPtr(m_hWnd, GWLP_HWNDPARENT, (LONG_PTR)pControl->m_hWnd);

	SetWindowPos(&CWnd::wndTopMost, rect.left, rect.top, rect.Width(), rect.Height(), SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

	CXTPMouseMonitor::SetupHook(this);
}

BEGIN_MESSAGE_MAP(MyCXTPReportInplaceList, CListBox)
//{{AFX_MSG_MAP(CXTPReportInplaceList)
ON_WM_MOUSEACTIVATE()
ON_WM_KILLFOCUS()
ON_WM_LBUTTONUP()
ON_WM_KEYDOWN()
ON_WM_SYSKEYDOWN()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int MyCXTPReportInplaceList::OnMouseActivate(CWnd* /*pDesktopWnd*/, UINT /*nHitTest*/, UINT /*message*/)
{
	return MA_NOACTIVATE;
}

void MyCXTPReportInplaceList::PostNcDestroy()
{
	CXTPMouseMonitor::SetupHook(NULL);
	SetItemArgs(NULL);

	CListBox::PostNcDestroy();
}

void MyCXTPReportInplaceList::OnKillFocus(CWnd* pNewWnd)
{
	CListBox::OnKillFocus(pNewWnd);
	DestroyWindow();
}

void MyCXTPReportInplaceList::OnLButtonUp(UINT, CPoint point)
{
	CXTPClientRect rc(this);

	if (rc.PtInRect(point))
		Apply();
	else
		Cancel();
}

void MyCXTPReportInplaceList::Cancel(void)
{
	GetOwner()->SetFocus();
}

void MyCXTPReportInplaceList::Apply(void)
{
	if (!pControl)
		return;

	CXTPReportControl* pReportControl = pControl;

	int nIndex = GetCurSel();
	if (nIndex != LB_ERR)
	{
		CXTPReportRecordItemConstraint* pConstraint = (CXTPReportRecordItemConstraint*)GetItemDataPtr(nIndex);

		pItem->OnConstraintChanged(this, pConstraint);
		pReportControl->RedrawControl();

		pReportControl->SendMessageToParent(pRow, pItem, pColumn, XTP_NM_REPORT_VALUECHANGED, 0);

	}

	pReportControl->SetFocus();
}

void MyCXTPReportInplaceList::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_ESCAPE) Cancel();
	else if (nChar == VK_RETURN || nChar == VK_F4)
		Apply();
	else CListBox::OnKeyDown(nChar, nRepCnt, nFlags);
}

void MyCXTPReportInplaceList::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_DOWN || nChar == VK_UP)
	{
		Apply();
		return;
	}

	CListBox::OnSysKeyDown(nChar, nRepCnt, nFlags);
}

IMPLEMENT_DYNAMIC(CDialogScriptViewView, CXTPReportView)

BEGIN_MESSAGE_MAP(CDialogScriptViewView, CXTPReportView)
ON_WM_CREATE()
END_MESSAGE_MAP()

CDialogScriptViewView::CDialogScriptViewView()
{
}

CDialogScriptViewView::~CDialogScriptViewView()
{

}

//////////////////////////////////////////////////////////////////////////
BOOL CDialogScriptViewView::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	if (!m_hWnd)
	{
		//////////////////////////////////////////////////////////////////////////
		// Create window.
		//////////////////////////////////////////////////////////////////////////
		CRect rcDefault(0, 0, 100, 100);
		LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
		VERIFY(CreateEx(NULL, lpClassName, "DialogView", dwStyle, rcDefault, pParentWnd, nID));

		if (!m_hWnd)
			return FALSE;

		CDialogScriptView* pCtrl = new CDialogScriptView;
		pCtrl->Create(WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, rect, this, nID + 1);
		SetReportCtrl(pCtrl);
	}
	return TRUE;
}

int CDialogScriptViewView::OnCreate(LPCREATESTRUCT lpcs)
{
	if (CWnd::OnCreate(lpcs) == -1)
		return -1;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CDialogScriptViewView::PostNcDestroy()
{
}

void CDialogScriptViewView::OnDestroy()
{
	CWnd::OnDestroy();
}

class CDialogScriptView::CPaintManager : public CXTPReportPaintManager
{
public:
	//virtual void DrawGroupRow( CDC* pDC, CXTPReportGroupRow* pRow, CRect rcRow)
	virtual void DrawGroupRow(CDC* pDC, CXTPReportGroupRow* pRow, CRect rcRow, XTP_REPORTRECORDITEM_METRICS* pMetrics)
	{
		HGDIOBJ oldFont = m_fontText.Detach();
		m_fontText.Attach(m_fontBoldText.Detach());
		__super::DrawGroupRow(pDC, pRow, rcRow, pMetrics);
		m_fontBoldText.Attach(m_fontText.Detach());
		m_fontText.Attach(oldFont);

		if (!pRow->GetTreeDepth())
		{
			rcRow.bottom = rcRow.top + 1;
			pDC->FillSolidRect(rcRow, 0);
		}
	}
	virtual void DrawFocusedRow(CDC* pDC, CRect rcRow)
	{
		__super::DrawFocusedRow(pDC, rcRow);
	}

	virtual int GetRowHeight(CDC* pDC, CXTPReportRow* pRow)
	{
		return __super::GetRowHeight(pDC, pRow) + (pRow->IsGroupRow() ? 2 : 0);
	}

	virtual void FillRow(
	  CDC* pDC,
	  CXTPReportRow* pRow,
	  CRect rcRow
	  )
	{
		__super::FillRow(pDC, pRow, rcRow);
	}

};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CDialogScriptView, CXTPReportControl)

BEGIN_MESSAGE_MAP(CDialogScriptView, CXTPReportControl)
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_CAPTURECHANGED()
ON_WM_CONTEXTMENU()
ON_WM_DESTROY()
ON_NOTIFY_REFLECT(XTP_NM_REPORT_HEADER_RCLICK, OnReportColumnRClick)
ON_NOTIFY_REFLECT(XTP_NM_REPORT_VALUECHANGED, OnValueChanged)
ON_NOTIFY_REFLECT(XTP_NM_REPORT_CHECKED, OnValueChanged)
ON_NOTIFY_REFLECT(XTP_NM_REPORT_REQUESTEDIT, OnRequestEdit)

ON_COMMAND(ID_DE_ADDLINE, OnAddLine)
ON_COMMAND(ID_DE_DELLINE, OnDelLine)
ON_COMMAND(ID_EDIT_DELETE, OnDelLine)
ON_UPDATE_COMMAND_UI(ID_DE_ADDLINE, OnUpdateCmdUI)
ON_UPDATE_COMMAND_UI(ID_DE_DELLINE, OnUpdateCmdUI)
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

enum
{
	eColLine = 0,
	eColActor,
	eColSoundName,
	eColAnimStopWithSound,
	eColAnimUseEx,
	eColAnimType,
	eColAnimName,
	eColFacialExpression,
	eColFacialWeight,
	eColFacialFadeTime,
	eColLookAtSticky,
	eColLookAtTarget,
	eColDelay,
	eColDesc
};

class CDialogColumn : public CXTPReportColumn
{
public:
	CDialogColumn(int nItemIndex, LPCTSTR strName, int nWidth, BOOL bAutoSize = TRUE, int nIconID = XTP_REPORT_NOICON, BOOL bSortable = TRUE, BOOL bVisible = TRUE)
		: CXTPReportColumn(nItemIndex, strName, nWidth, bAutoSize, nIconID, bSortable, bVisible)
	{
	}
	void           SetHelp(const CString& help) { m_help = help; }
	const CString& GetHelp()                    { return m_help; }
protected:
	CString        m_help;
};

CryAudio::ControlId CDialogScriptView::ms_currentPlayLine = CryAudio::InvalidControlId;

CDialogScriptView::CDialogScriptView()
{
	m_pScript = 0;
	m_bDragging = false;
	m_bModified = false;
	m_ptDrag.SetPoint(0, 0);
	m_pFC = 0;
	m_hCursorNoDrop = (HCURSOR) LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_POINTER_SO_SELECT), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE);
	m_hCursorNormal = (HCURSOR) LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_POINTER), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE);

	m_pSourceRow = 0;
	m_pTargetRow = 0;

	m_pColLine = (CDialogColumn*)AddColumn(new CDialogColumn(eColLine, "Line", 30, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColActor = (CDialogColumn*)AddColumn(new CDialogColumn(eColActor, "Actor", 40, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColSoundName = (CDialogColumn*)AddColumn(new CDialogColumn(eColSoundName, "AudioID", 150, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColAnimName = (CDialogColumn*)AddColumn(new CDialogColumn(eColAnimName, "Animation", 100, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColAnimType = (CDialogColumn*)AddColumn(new CDialogColumn(eColAnimType, "Type", 40, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColAnimType->SetTooltip(_T("Animation Type: Signal or Action"));
	m_pColAnimUseEx = (CDialogColumn*)AddColumn(new CDialogColumn(eColAnimUseEx, "EP", 30, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColAnimUseEx->SetTooltip(_T("Use Exact Positioning"));
	m_pColAnimStopWithSound = (CDialogColumn*)AddColumn(new CDialogColumn(eColAnimStopWithSound, "Sync", 30, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColAnimStopWithSound->SetTooltip(_T("Stop Animation when Sound stops"));
	m_pColFacialExpression = (CDialogColumn*)AddColumn(new CDialogColumn(eColFacialExpression, "FacialExpr.", 80, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColFacialWeight = (CDialogColumn*)AddColumn(new CDialogColumn(eColFacialWeight, "Weight", 40, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColFacialFadeTime = (CDialogColumn*)AddColumn(new CDialogColumn(eColFacialFadeTime, "Fade", 40, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColLookAtTarget = (CDialogColumn*)AddColumn(new CDialogColumn(eColLookAtTarget, "LookAt", 60, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColLookAtTarget->SetTooltip(_T("LookAt Target"));
	m_pColLookAtSticky = (CDialogColumn*)AddColumn(new CDialogColumn(eColLookAtSticky, "Sticky", 40, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColLookAtSticky->SetTooltip(_T("Sticky LookAt"));
	m_pColDelay = (CDialogColumn*)AddColumn(new CDialogColumn(eColDelay, "Delay", 40, TRUE, XTP_REPORT_NOICON, FALSE));
	m_pColDesc = (CDialogColumn*)AddColumn(new CDialogColumn(eColDesc, "Description", 100, TRUE, XTP_REPORT_NOICON, FALSE));

	SetupHelp();

	HKEY hKey = AfxGetApp()->GetAppRegistryKey();
	if (hKey)
	{
		CXTPPropExchangeRegistry px(TRUE, hKey, _T("DialogEditor\\ViewLayout"));
		DoPropExchange(&px);
	}

	GetReportHeader()->AllowColumnRemove(FALSE);
	GetReportHeader()->AllowColumnResize(TRUE);
	GetReportHeader()->AllowColumnSort(FALSE);
	AllowEdit(TRUE);
	m_pColLine->SetEditable(FALSE);
	SetMultipleSelection(FALSE);

	SetupEditConstraints();

	FocusSubItems(TRUE);
	EditOnClick(TRUE);

	GetReportHeader()->SetAutoColumnSizing(FALSE);

	SetPaintManager(new CDialogScriptView::CPaintManager);

	CXTPReportPaintManager* manager = GetPaintManager();
	manager->m_clrGroupShadeBack.SetStandardValue(0x00ff00ff);
	manager->m_clrHighlight = manager->m_clrBtnFace;
	manager->m_clrHighlightText = manager->m_clrWindowText;
	manager->m_clrIndentControl = manager->m_clrGridLine;
	SetGridStyle(TRUE, xtpReportGridSolid);
	UpdateNoItemText();
	ShowFooter(FALSE);
	EnablePreviewMode(FALSE);

	m_pNewInplaceList = new MyCXTPReportInplaceList();

	CryAudio::SCreateObjectData const objectData("Dialog Lines trigger preview", CryAudio::EOcclusionType::Ignore);
	m_pIAudioObject = gEnv->pAudioSystem->CreateObject(objectData);
	gEnv->pAudioSystem->AddRequestListener(&CDialogScriptView::OnAudioTriggerFinished, m_pIAudioObject, CryAudio::ESystemEvents::TriggerFinished);
}

//////////////////////////////////////////////////////////////////////////
CDialogScriptView::~CDialogScriptView()
{
	StopSound();

	if (m_pIAudioObject != nullptr)
	{
		gEnv->pAudioSystem->RemoveRequestListener(&CDialogScriptView::OnAudioTriggerFinished, m_pIAudioObject);
		gEnv->pAudioSystem->ReleaseObject(m_pIAudioObject);
	}

	if (m_hCursorNoDrop)
		DestroyCursor(m_hCursorNoDrop);
	if (m_hCursorNormal)
		DestroyCursor(m_hCursorNormal);
	if (m_pNewInplaceList)
		delete m_pNewInplaceList;
}

//////////////////////////////////////////////////////////////////////////
void CDialogScriptView::OnDestroy()
{
	HKEY hKey = AfxGetApp()->GetAppRegistryKey();
	if (hKey)
	{
		CXTPPropExchangeRegistry px(FALSE, hKey, _T("DialogEditor\\ViewLayout"));
		DoPropExchange(&px);
	}
}

//////////////////////////////////////////////////////////////////////////
void CDialogScriptView::SetupHelp()
{
	// better use string table resource for this
	m_pColLine->SetHelp(_T("Current line. Click this column to select the whole line. You can then drag the line around.\r\nTo add a new line double click in free area or use [Add ScriptLine] from the toolbar.\r\nTo delete a line press [Delete] or use [Delete ScriptLine] from the toolbar.\r\nDoubleClick plays or stops the Sound of the Line."));
	m_pColActor->SetHelp(_T("Actor for this line. Select from the combo-box.\r\nAny Entity can be an Actor. An Entity can be assigned as Actor using the Dialog:PlayDialog flownode."));
	m_pColSoundName->SetHelp(_T("AudioControllID to be triggered. \r\nIf a sound is specified the time of 'Delay' is relative to the end of the sound."));
	m_pColAnimStopWithSound->SetHelp(_T("When checked, the animation [best an Action] will automatically stop when sound ends."));
	m_pColAnimUseEx->SetHelp(_T("When checked, Exact Positioning [EP] will be used to play the animation. The target for the EP is the LookAt target.\r\nIf you want to make sure that an animation is exactly oriented, use this option."));
	m_pColAnimType->SetHelp(_T("Signal = OneShot animation. Action = Looping animation. Animation can automatically be stopped, when [Sync] is enabled."));
	m_pColAnimName->SetHelp(_T("Name of Animation Signal or Action. You should select an entity first [in the main view] and then browse its signals/actions."));
	m_pColFacialExpression->SetHelp(_T("Facial expression. Normally, every sound is already lip-synced and may already contain facial expressions. So use with care.\r\nCan be useful when you don't play a sound, but simply want the Actor make look with a specific mood/expression.\r\nUse #RESET# to reset expression to default state."));
	m_pColFacialWeight->SetHelp(_T("Weight of the facial expression [0-1]. How strong the facial expression should be applied.\r\nWhen sound already contains a facial expression, use only small values < 0.5."));
	m_pColFacialFadeTime->SetHelp(_T("Fade-time of the facial expression in seconds. How fast the facial expression should be applied."));
	m_pColLookAtSticky->SetHelp(_T("When you tell an Actor to look at its target, it's only applied right at the beginning of the current script line.\r\nIf you want to make him constantly facing you, activate Sticky lookat.\r\nYou can reset Sticky lookat by #RESET# as LookAt target or a new lookat target with Sticky enabled."));
	m_pColLookAtTarget->SetHelp(_T("Lookat target of the Actor. Actor will try to look at his target before he starts to talk/animate.\r\nSometimes this cannot be guaranteed. Also, while doing his animation or talking he may no longer face his target. For these cases use Sticky lookat. To disable sticky look-at use #RESET#."));
	m_pColDelay->SetHelp(_T("Delay in seconds before advancing to the next line. When a sound is played, Delay is relative to the end of the sound.\r\nSo, to slightly overlap lines and to make dialog more natural, you can use negative delays.\r\nWhen no sound is specified, the delay is relative to the start of the line."));
	m_pColDesc->SetHelp(_T("Description of the line."));
}

//////////////////////////////////////////////////////////////////////////
void CDialogScriptView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_bDragging)
	{
		__super::OnLButtonDown(nFlags, point);

		if (IsAllowEdit())
		{
			CRect reportArea = GetReportRectangle();
			if (reportArea.PtInRect(point))
			{
				m_pTargetRow = 0;
				m_pSourceRow = HitTest(point);
				if (m_pSourceRow)
				{
					m_ptDrag = point;
				}
			}
		}
	}
}

BOOL CDialogScriptView::PreTranslateMessage(MSG* msg)
{
	if (msg->message == WM_LBUTTONDBLCLK)
	{
		CPoint point(msg->lParam);
		CRect reportArea = GetReportRectangle();
		if (reportArea.PtInRect(point))
		{
			CXTPReportRow* pRow = HitTest(point);
			if (pRow == 0)
			{
				AddNewLine(true);
				return TRUE;
			}
		}
	}
	return __super::PreTranslateMessage(msg);
}

//////////////////////////////////////////////////////////////////////////
#define ID_REMOVE_ITEM            1
#define ID_SORT_ASC               2
#define ID_SORT_DESC              3
#define ID_GROUP_BYTHIS           4
#define ID_SHOW_GROUPBOX          5
#define ID_SHOW_FIELDCHOOSER      6
#define ID_COLUMN_BESTFIT         7
#define ID_COLUMN_ARRANGEBY       100
#define ID_COLUMN_ALIGMENT        200
#define ID_COLUMN_ALIGMENT_LEFT   ID_COLUMN_ALIGMENT + 1
#define ID_COLUMN_ALIGMENT_RIGHT  ID_COLUMN_ALIGMENT + 2
#define ID_COLUMN_ALIGMENT_CENTER ID_COLUMN_ALIGMENT + 3
#define ID_COLUMN_SHOW            500

void CDialogScriptView::OnReportColumnRClick(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify->pColumn);
	CPoint ptClick = pItemNotify->pt;

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	menu.AppendMenu(MF_SEPARATOR);

	// create columns items
	CXTPReportColumns* pColumns = GetColumns();
	int nColumnCount = pColumns->GetCount();
	for (int i = 0; i < nColumnCount; ++i)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(i);
		CString sCaption = pCol->GetCaption();
		if (!sCaption.IsEmpty())
		{
			menu.AppendMenu(MF_STRING, ID_COLUMN_SHOW + i, sCaption);
			menu.CheckMenuItem(ID_COLUMN_SHOW + i, MF_BYCOMMAND | (pCol->IsVisible() ? MF_CHECKED : MF_UNCHECKED));
		}
	}

	// track menu
	int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptClick.x, ptClick.y, this, NULL);

	// process column selection item
	if (nMenuResult >= ID_COLUMN_SHOW)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nMenuResult - ID_COLUMN_SHOW);
		if (pCol)
			pCol->SetVisible(!pCol->IsVisible());
	}
}

void CDialogScriptView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (!m_bDragging)
		__super::OnLButtonUp(nFlags, point);
	else
	{
		m_bDragging = false;
		// CDialogEditorDialog* pDlg = (CDialogEditorDialog*) GetParent();
		if (m_pTargetRow && m_pSourceRow != m_pTargetRow)
		{
			// CDialogObjectEntry* pSource = (CDialogObjectEntry*) m_pSourceRow->GetRecord();
			// CDialogObjectEntry* pTarget = (CDialogObjectEntry*) m_pTargetRow->GetRecord();
			// pDlg->ModifyRuleOrder( pSource->m_pCondition->iOrder, pTarget->m_pCondition->iOrder );
			// pDlg->ReloadEntries( false );
			CDialogScriptRecord* pSource = static_cast<CDialogScriptRecord*>(m_pSourceRow->GetRecord());
			CDialogScriptRecord* pTarget = static_cast<CDialogScriptRecord*>(m_pTargetRow->GetRecord());
			CXTPReportRecords* pRecords = GetRecords();
			const int from = pSource->GetIndex();
			const int to = pTarget->GetIndex();
			CDialogScriptRecord* pNewRec = new CDialogScriptRecord(m_pScript, pSource->GetLine());

			if (from < to) // move from down
			{
				// insert after to
				pRecords->RemoveAt(from);
				pRecords->InsertAt(to, pNewRec);
				SetModified(true);
			}
			else // move from up
			{
				// insert before to
				pRecords->RemoveAt(from);
				pRecords->InsertAt(to, pNewRec);
				SetModified(true);
			}

			Populate();
			CXTPReportRow* pRow = GetRows()->Find(pNewRec);
			if (pRow)
			{
				SetFocusedRow(pRow);
			}

			m_pTargetRow = 0;
			m_pSourceRow = 0;
		}
		ReleaseCapture();
	}
}

void CDialogScriptView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!m_bDragging)
	{
		__super::OnMouseMove(nFlags, point);
		const int dragX = ::GetSystemMetrics(SM_CXDRAG);
		const int dragY = ::GetSystemMetrics(SM_CYDRAG);
		if ((m_ptDrag.x || m_ptDrag.y) && (abs(point.x - m_ptDrag.x) > dragX || abs(point.y - m_ptDrag.y) > dragY))
		{
			CXTPReportRow* pRow = HitTest(m_ptDrag);
			if (pRow)
			{
				GetSelectedRows()->Add(pRow);
				SetCapture();
				SetCursor(m_hCursorNoDrop);
				m_bDragging = true;
			}
		}
	}
	else
	{
		CPoint pt = point;
		if (m_pSourceRow)
		{
			CXTPReportRow* pOverRow = HitTest(pt);

			// erase previous drop marker...
			DrawDropTarget();
			m_pTargetRow = pOverRow;

			CRect rc = GetReportRectangle();
			if (rc.left <= pt.x && pt.x < rc.right)
			{
				if (pt.y < rc.top)
				{
					OnVScroll(SB_LINEUP, 0, NULL);
					UpdateWindow();
				}
				else if (pt.y > rc.bottom)
				{
					OnVScroll(SB_LINEDOWN, 0, NULL);
					UpdateWindow();
				}
			}

			// ...and draw current drop marker
			DrawDropTarget();
		}
	}
}

void CDialogScriptView::DrawDropTarget()
{
	if (m_pSourceRow && m_pTargetRow && m_pSourceRow != m_pTargetRow)
	{
		int sourceIdx = m_pSourceRow->GetIndex();
		int targetIdx = m_pTargetRow->GetIndex();
		CClientDC dc(this);
		CRect rc = m_pTargetRow->GetRect();
		if (sourceIdx < targetIdx)
			rc.top = rc.bottom - 2;
		else
			rc.top = rc.top - 2;
		rc.bottom = rc.top + 4;

		dc.InvertRect(rc);
	}
}

void CDialogScriptView::OnCaptureChanged(CWnd* pWnd)
{
	// erase marker
	DrawDropTarget();
	SetCursor(m_hCursorNormal);
	m_bDragging = false;
	m_ptDrag.SetPoint(0, 0);
	__super::OnCaptureChanged(pWnd);
}

void CDialogScriptView::OnContextMenu(CWnd* pWnd, CPoint pos)
{
	int selCount = GetSelectedRows()->GetCount();

	if (pos.x == -1 && pos.y == -1)
	{
		pos.x = 7;
		pos.y = GetReportRectangle().top + 1;
		ClientToScreen(&pos);
	}

	CPoint ptClient(pos);
	ScreenToClient(&ptClient);
	CXTPReportRow* pRow = HitTest(ptClient);
	if (pRow || ptClient.y > GetReportRectangle().top)
	{
		CMenu menu;
		VERIFY(menu.CreatePopupMenu());

		menu.AppendMenu(MF_STRING, ID_SOED_NEW_CONDITION, _T("New..."));
		menu.AppendMenu(MF_STRING | (selCount != 1 ? MF_DISABLED : 0), ID_SOED_CLONE_CONDITION, _T("Duplicate..."));
		menu.AppendMenu(MF_STRING | (!selCount ? MF_DISABLED : 0), ID_SOED_DELETE_CONDITION, _T("Delete"));

#if 0
		CDialogEditorDialog* pDlg = (CDialogEditorDialog*) GetParent();

		// track menu
		int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pos.x, pos.y, this, NULL);

		switch (nMenuResult)
		{
		case ID_SOED_NEW_CONDITION:
			pDlg->OnAddEntry();
			break;
		case ID_SOED_CLONE_CONDITION:
			pDlg->OnDuplicateEntry();
			break;
		case ID_SOED_DELETE_CONDITION:
			pDlg->OnRemoveEntry();
			break;
		}
#endif

	}
	else
		__super::OnContextMenu(pWnd, pos);
}

void CDialogScriptView::SetScript(CEditorDialogScript* pScript)
{
	StopSound();
	m_pScript = pScript;
	UpdateNoItemText();
	Reload();
	SetModified(false);
	OnSelectionChanged();
}

void CDialogScriptView::UpdateNoItemText()
{
	if (m_pScript)
		m_pPaintManager->m_strNoItems = _T("No Dialog Lines yet.\nUse double click or toolbar to add a new line.");
	else
		m_pPaintManager->m_strNoItems = _T("No Dialog Script selected.\nCreate a new one or use browser on the left to select one.");
}

void CDialogScriptView::Reload()
{
	BeginUpdate();
	GetRecords()->RemoveAll();

	if (m_pScript)
	{
		for (int i = 0; i < m_pScript->GetNumLines(); ++i)
		{
			AddRecord(new CDialogScriptRecord(m_pScript, m_pScript->GetLine(i)));
		}
	}

	EndUpdate();
	Populate();

}

void CDialogScriptView::AddNewLine(bool bForceEnd)
{
	if (!IsAllowEdit())
		return;

	if (m_pScript == 0)
		return;

	static const CEditorDialogScript::SScriptLine newLine;
	CDialogScriptRecord* pRecord = new CDialogScriptRecord(m_pScript, &newLine);
	CXTPReportRow* pFocusRow = GetFocusedRow();
	if (pFocusRow && !bForceEnd)
	{
		int index = pFocusRow->GetIndex();
		GetRecords()->InsertAt(index + 1, pRecord);
	}
	else
	{
		AddRecord(pRecord);
	}

	Populate();
	CXTPReportRow* pRow = GetRows()->Find(pRecord);
	if (pRow)
	{
		SetFocusedRow(pRow);
		// XTP_REPORTRECORDITEM_ARGS itemArgs(this, pRow, GetColumns()->Find(eColActor));
		// EditItem(&itemArgs);
	}
	SetModified(true);
}

void CDialogScriptView::SetupEditConstraints()
{
	CXTPReportRecordItemEditOptions* pEditOpts;
	pEditOpts = m_pColActor->GetEditOptions();
	pEditOpts->AddConstraint("Actor1", 0);
	pEditOpts->AddConstraint("Actor2", 1);
	pEditOpts->AddConstraint("Actor3", 2);
	pEditOpts->AddConstraint("Actor4", 3);
	pEditOpts->AddConstraint("Actor5", 4);
	pEditOpts->AddConstraint("Actor6", 5);
	pEditOpts->AddConstraint("Actor7", 6);
	pEditOpts->AddConstraint("Actor8", 7);
	pEditOpts->m_bAllowEdit = FALSE;
	pEditOpts->m_bConstraintEdit = TRUE;
	pEditOpts->AddComboButton();

	pEditOpts = m_pColLookAtTarget->GetEditOptions();
	pEditOpts->AddConstraint("---", CEditorDialogScript::NO_ACTOR_ID);
	pEditOpts->AddConstraint("#RESET#", CEditorDialogScript::STICKY_LOOKAT_RESET_ID);
	pEditOpts->AddConstraint("Actor1", 0);
	pEditOpts->AddConstraint("Actor2", 1);
	pEditOpts->AddConstraint("Actor3", 2);
	pEditOpts->AddConstraint("Actor4", 3);
	pEditOpts->AddConstraint("Actor5", 4);
	pEditOpts->AddConstraint("Actor6", 5);
	pEditOpts->AddConstraint("Actor7", 6);
	pEditOpts->AddConstraint("Actor8", 7);
	pEditOpts->m_bAllowEdit = FALSE;
	pEditOpts->m_bConstraintEdit = TRUE;
	pEditOpts->AddComboButton();

	pEditOpts = m_pColAnimType->GetEditOptions();
	pEditOpts->AddConstraint("Action", 0);
	pEditOpts->AddConstraint("Signal", 1);
	pEditOpts->m_bAllowEdit = FALSE;
	pEditOpts->m_bConstraintEdit = TRUE;
	pEditOpts->AddComboButton();

	pEditOpts = m_pColSoundName->GetEditOptions();
	pEditOpts->AddExpandButton();

	pEditOpts = m_pColAnimUseEx->GetEditOptions();
	pEditOpts->m_bAllowEdit = FALSE;

	pEditOpts = m_pColAnimName->GetEditOptions();
	pEditOpts->AddExpandButton();

	pEditOpts = m_pColLookAtSticky->GetEditOptions();
	pEditOpts->m_bAllowEdit = FALSE;

	pEditOpts = m_pColAnimStopWithSound->GetEditOptions();
	pEditOpts->m_bAllowEdit = FALSE;

	pEditOpts = m_pColFacialExpression->GetEditOptions();
	pEditOpts->AddConstraint("#RESET#");
	pEditOpts->m_bConstraintEdit = FALSE;
	pEditOpts->m_bAllowEdit = TRUE;
	pEditOpts->AddComboButton();
	pEditOpts->AddExpandButton();

	pEditOpts = m_pColDelay->GetEditOptions();
	pEditOpts->m_bSelectTextOnEdit = TRUE;

	pEditOpts = m_pColFacialWeight->GetEditOptions();
	pEditOpts->m_bSelectTextOnEdit = TRUE;

	pEditOpts = m_pColFacialFadeTime->GetEditOptions();
	pEditOpts->m_bSelectTextOnEdit = TRUE;

}

void CDialogScriptView::GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
{
	/*
	   if (pDrawArgs->pRow->IsFocused())
	   {
	   COLORREF color = pItemMetrics->clrBackground;
	   const float fac = 0.8f;
	   color = RGB(GetRValue(color)*fac, GetGValue(color)*fac, GetBValue(color)*fac);
	   pItemMetrics->clrBackground = color;
	   color = pItemMetrics->clrForeground;
	   color = RGB(GetRValue(color)*fac, GetGValue(color)*fac, GetBValue(color)*fac);
	   pItemMetrics->clrForeground = color;
	   }
	 */
}

void CDialogScriptView::OnValueChanged(NMHDR* pNotifyStruct, LRESULT* /*result*/)
{
	SetModified(true);
}

void CDialogScriptView::OnRequestEdit(NMHDR* pNotifyStruct, LRESULT* result)
{
	XTP_NM_REPORTREQUESTEDIT* pItemNotify = (XTP_NM_REPORTREQUESTEDIT*) pNotifyStruct;
	pItemNotify->bCancel = IsAllowEdit() == false;
}

void CDialogScriptView::SaveToScript()
{
	if (m_pScript == 0)
		return;
	m_pScript->RemoveAll();
	CXTPReportRecords* pRecords = GetRecords();
	for (int i = 0; i < pRecords->GetCount(); ++i)
	{
		CDialogScriptRecord* pRecord = static_cast<CDialogScriptRecord*>(pRecords->GetAt(i));
		m_pScript->AddLine(*pRecord->GetLine());
	}
}

//////////////////////////////////////////////////////////////////////////
void CDialogScriptView::OnUpdateCmdUI(CCmdUI* target)
{
	switch (target->m_nID)
	{
	case ID_DE_DELLINE:
		{
			CXTPReportSelectedRows* pRows = GetSelectedRows();
			target->Enable(IsAllowEdit() && m_pScript != 0 && (pRows->GetCount() > 0) ? TRUE : FALSE);
		}
		break;
	case ID_DE_ADDLINE:
		target->Enable((IsAllowEdit() && m_pScript != 0) ? TRUE : FALSE);
		break;
	}
}

void CDialogScriptView::OnAddLine()
{
	AddNewLine(false);
}

void CDialogScriptView::OnDelLine()
{
	if (!IsAllowEdit())
		return;
	CXTPReportSelectedRows* pSelRows = GetSelectedRows();
	if (pSelRows->GetCount() == 0)
		return;

	if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Delete current line?")))
		return;

	int highIndex = 0;

	CXTPReportRecords* pRecords = GetRecords();
	for (int i = 0; i < pSelRows->GetCount(); ++i)
	{
		CXTPReportRow* pRow = pSelRows->GetAt(i);
		CXTPReportRecord* pRecord = pRow->GetRecord();
		highIndex = std::max(highIndex, pRow->GetIndex());
		pRecords->RemoveAt(pRecord->GetIndex());
	}
	CXTPReportRecord* pSelRec = 0;
	CXTPReportRow* pSelRow = 0;
	int rowCount = GetRows()->GetCount();
	if (rowCount > 0)
	{
		++highIndex;
		if (highIndex < rowCount)
		{
			CXTPReportRow* pSelRow = GetRows()->GetAt(highIndex);
			if (pSelRow)
			{
				pSelRec = pSelRow->GetRecord();
			}
		}
	}
	Populate();

	if (pSelRec)
		pSelRow = GetRows()->Find(pSelRec);
	else
	{
		rowCount = GetRows()->GetCount();
		if (rowCount > 0)
			pSelRow = GetRows()->GetAt(rowCount - 1);
	}
	if (pSelRow)
		SetFocusedRow(pSelRow);

	SetModified(true);
}

void CDialogScriptView::SetDialogEditor(CDialogEditorDialog* pDialogEditor)
{
	m_pDialogEditor = pDialogEditor;
}

void CDialogScriptView::SetModified(bool bModified)
{
	m_bModified = bModified;
	if (m_pDialogEditor)
		m_pDialogEditor->NotifyChange(m_pScript, m_bModified);
}

void CDialogScriptView::OnSelectionChanged()
{
	if (GetFocusedColumn() != m_pFC)
	{
		m_pFC = (CDialogColumn*) GetFocusedColumn();
		if (m_pFC)
		{
			CString help = "[";
			help += m_pFC->GetCaption();
			help += "]\r\n";
			help += m_pFC->GetHelp();
			m_pDialogEditor->UpdateHelp(help);
		}
		else
		{
			m_pDialogEditor->UpdateHelp("");
		}
	}
}

void CDialogScriptView::StopSound()
{
	if (ms_currentPlayLine != CryAudio::InvalidControlId)
	{
		if (m_pIAudioObject != nullptr)
		{
			m_pIAudioObject->StopTrigger(ms_currentPlayLine);
		}
		ms_currentPlayLine = CryAudio::InvalidControlId;
	}
}

void CDialogScriptView::PlayLine(int index)
{
	StopSound();

	if (m_pScript != nullptr && m_pIAudioObject != nullptr)
	{
		CXTPReportRecords* pRecords = GetRecords();
		CDialogScriptRecord* pRecord = static_cast<CDialogScriptRecord*>(pRecords->GetAt(index));
		const CEditorDialogScript::SScriptLine* pLine = pRecord->GetLine();
		const CCamera& camera = GetIEditor()->GetSystem()->GetViewCamera();
		m_pIAudioObject->SetTransformation(camera.GetMatrix());
		CryAudio::ControlId const triggerId = CryAudio::StringToId(pLine->m_audioTriggerName.c_str());
		m_pIAudioObject->ExecuteTrigger(triggerId);
		ms_currentPlayLine = triggerId;
	}
}

void CDialogScriptView::OnAudioTriggerFinished(CryAudio::SRequestInfo const* const pAudioRequestInfo)
{
	//Remark: called from audio thread
	if (ms_currentPlayLine == pAudioRequestInfo->audioControlId)  //the currently playing line has finished
	{
		ms_currentPlayLine = CryAudio::InvalidControlId;
	}
}

