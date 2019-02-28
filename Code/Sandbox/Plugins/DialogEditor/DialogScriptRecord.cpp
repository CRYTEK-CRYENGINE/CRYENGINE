// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DialogScriptRecord.h"
#include "Objects/BaseObject.h"
#include "Dialogs/GenericSelectItemDialog.h"
#include "DialogScriptView.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IFacialAnimation.h>
#include <CryString/StringUtils.h>

#include <CryGame/IGameFramework.h>
#include "IResourceSelectorHost.h"
#include "Controls/QuestionDialog.h"

#include "ICryMannequin.h"

// whether we place the in-place controls within the item area (true) or outside (false)
static const bool g_bAlignInplaceControls = false;
static const float HFAC = 1.2f;
static const float HLOWFAC = 0.9f;
static const COLORREF HCOL = RGB(0xFF, 0xFF, 0x60);

#define DSR_ENABLE_CUSTOM_DRAW
// #undef DSR_ENABLE_CUSTOM_DRAW

/*
   eColLine      = 0,
   eColActor,
   eColSoundName,
   eColAnimStopWithSound,
   eColAnimUseEx,
   eColAnimType,
   eColAnimName,
   eColFacialExpression,
   eColLookAtSticky,
   eColLookAtTarget,
   eColDelay,
   eColDesc
 */

namespace KeyWords
{
enum { eKW_BOLD = 0x0001, eKW_COL = 0x0002 };

struct KeyWord
{
	const char* name;
	COLORREF    col;
	int         flag;
};

KeyWord keyWords[] =
{
	{ "#RESET#", RGB(0xA0, 0x20, 0x20), eKW_COL | eKW_BOLD },
};

static const int numKeyWords = sizeof(keyWords) / sizeof(*keyWords);

bool AdjustKeyWords(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
{
	// hard-code some keywords' special layout
	for (int i = 0; i < numKeyWords; ++i)
	{
		const KeyWord& kw = keyWords[i];
		if (pDrawArgs->pItem->GetCaption(pDrawArgs->pColumn).Compare(kw.name) == 0)
		{
			if (kw.flag & eKW_COL)
				pItemMetrics->clrForeground = kw.col;
			if (kw.flag & eKW_BOLD)
				pItemMetrics->pFont = &pDrawArgs->pControl->GetPaintManager()->m_fontBoldText;
			return true;
		}
	}
	return false;
}
}

class CLineNo : public CXTPReportRecordItemText
{
	virtual CString GetCaption(CXTPReportColumn* pColumn)
	{
		CString tmp;
		tmp.AppendFormat("%d", GetRecord()->GetIndex() + 1);
		return tmp;
	}

#ifdef DSR_ENABLE_CUSTOM_DRAW
	int Draw(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs)
	{
		if (pDrawArgs->pControl->HasFocus() && pDrawArgs->pRow->IsFocused())
		{
			XTP_REPORTRECORDITEM_METRICS* pMetrics = new XTP_REPORTRECORDITEM_METRICS();
			pDrawArgs->pItem->GetItemMetrics(pDrawArgs, pMetrics);
			pDrawArgs->pRow->GetRecord()->GetItemMetrics(pDrawArgs, pMetrics);
			CXTPReportPaintManager* pPaintManager = pDrawArgs->pControl->GetPaintManager();
			CXTPPaintManagerColor backHT = pPaintManager->m_clrHighlightText;
			CXTPPaintManagerColor backH = pPaintManager->m_clrHighlight;
			pPaintManager->m_clrHighlightText = pMetrics->clrForeground;
			pPaintManager->m_clrHighlight = pMetrics->clrBackground;
			int w = __super::Draw(pDrawArgs);
			pPaintManager->m_clrHighlightText = backHT;
			pPaintManager->m_clrHighlight = backH;
			pMetrics->InternalRelease();
			return w;
		}
		else return __super::Draw(pDrawArgs);
	}
#endif
	void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	{
		__super::GetItemMetrics(pDrawArgs, pItemMetrics);
		if (pDrawArgs->pControl->HasFocus() && pDrawArgs->pRow->IsFocused() && pDrawArgs->pColumn == pDrawArgs->pControl->GetFocusedColumn())
		{
			COLORREF color = pItemMetrics->clrBackground;
			const float fac = HFAC;
			color = HCOL;
			pItemMetrics->clrBackground = color;
		}
	}
	virtual void OnDblClick(
	  XTP_REPORTRECORDITEM_CLICKARGS* pClickArgs
	  )
	{
		CDialogScriptView* pView = static_cast<CDialogScriptView*>(pClickArgs->pControl);
		pView->PlayLine(GetRecord()->GetIndex());
	}

};

void WorkaroundComboButton(CXTPReportRecordItem* pItem, CXTPReportInplaceButton* pButton)
{
	CXTPReportControl* pControl = pButton->pControl;

	XTP_NM_REPORTINPLACEBUTTON nm;
	::ZeroMemory(&nm, sizeof(nm));

	nm.pButton = pButton;
	nm.pItem = pItem;

	if (pControl->SendNotifyMessage(XTP_NM_REPORT_INPLACEBUTTONDOWN, (NMHDR*)&nm) == TRUE)
		return;

	if (pButton->GetID() == XTP_ID_REPORT_COMBOBUTTON)
	{
		MyCXTPReportInplaceList* pList = ((CDialogScriptView*)pControl)->GetNewInplaceList();
		//CXTPReportInplaceList* pList = pControl->GetInplaceList();

		XTP_REPORTRECORDITEM_ARGS itemArgs = *pButton;
		if (!itemArgs.pColumn && !itemArgs.pControl && !itemArgs.pItem && !itemArgs.pRow)
		{
			return;
		}
		ASSERT(itemArgs.pItem == pItem);

		CXTPWindowRect rcButton(pButton);
		pControl->ScreenToClient(&rcButton);
		itemArgs.rcItem.right = rcButton.right;

		CXTPReportRecordItemEditOptions* pEditOptions = pItem->GetEditOptions(itemArgs.pColumn);

		if (pEditOptions->GetConstraints()->GetCount() > 0)
		{
			pList->Create(&itemArgs, pEditOptions->GetConstraints());
		}
	}
}

template<typename T, class C>
class CHelperValue : public C
{
public:
	typedef Functor2<T&, CDialogScriptRecord*> SelectCallback;

	CHelperValue(T& ref, SelectCallback cb = 0, bool bIsPreview = false) : m_valueRef(ref), m_cb(cb), m_bIsPreview(bIsPreview), C(ref) {}

	virtual void OnEditChanged(XTP_REPORTRECORDITEM_ARGS* pItemArgs, LPCTSTR szText)
	{
		// CryLogAlways("NewText: %s", szText);
		__super::OnEditChanged(pItemArgs, szText);
		m_valueRef = GetValue();
		if (m_bIsPreview)
		{
			CXTPReportRecordItemPreview* pPrev = pItemArgs->pRow->GetRecord()->GetItemPreview();
			if (pPrev)
			{
				pPrev->SetPreviewText(szText);
			}
		}
	}

	virtual void OnInplaceButtonDown(CXTPReportInplaceButton* pButton)
	{
		if (m_cb != 0 && pButton->GetID() == XTP_ID_REPORT_EXPANDBUTTON)
		{
			T tmp = m_valueRef;
			m_cb(tmp, static_cast<CDialogScriptRecord*>(GetRecord()));
			if (tmp != m_valueRef)
			{
				pButton->pControl->SendMessageToParent(pButton->pRow, pButton->pItem, pButton->pColumn, XTP_NM_REPORT_VALUECHANGED, 0);
				SetValue(tmp);
				m_valueRef = GetValue();
				pButton->pControl->RedrawControl();
			}
		}
		else if (pButton->GetID() == XTP_ID_REPORT_COMBOBUTTON)
		{
			WorkaroundComboButton(this, pButton);
		}
		else
			__super::OnInplaceButtonDown(pButton);
	}

	virtual void OnBeginEdit(XTP_REPORTRECORDITEM_ARGS* pItemArgs)
	{
		__super::OnBeginEdit(pItemArgs);
		if (!g_bAlignInplaceControls)
			return;

		CXTPReportInplaceButtons* pInpaceButtons = pItemArgs->pControl->GetInplaceButtons();
		if (pInpaceButtons && pInpaceButtons->GetSize() > 0)
		{
			CRect rcButtons(pItemArgs->rcItem);
			for (int i = pInpaceButtons->GetSize() - 1; i >= 0; --i)
			{
				CXTPReportInplaceButton* pButton = pInpaceButtons->GetAt(i);
				// get rect of button
				CRect buttonRect;
				pButton->GetClientRect(&buttonRect);
				buttonRect.MoveToXY(rcButtons.right - buttonRect.Width(), rcButtons.top);
				pButton->SetWindowPos(0, buttonRect.left, buttonRect.top + 1, buttonRect.Width(), buttonRect.Height() - 2, SWP_NOZORDER | SWP_SHOWWINDOW);
				rcButtons.right -= buttonRect.Width();
			}
			CXTPReportInplaceEdit* pEdit = pItemArgs->pControl->GetInplaceEdit();
			if (pEdit && pEdit->GetSafeHwnd() && pEdit->GetItem() == this)
			{
				CRect editRect;
				pEdit->GetWindowRect(&editRect);
				pItemArgs->pControl->ScreenToClient(&editRect);
				editRect.right = rcButtons.right;
				pEdit->SetWindowPos(0, editRect.left, editRect.top, editRect.Width(), editRect.Height(), SWP_NOZORDER | SWP_SHOWWINDOW);
			}
		}
	}

	void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	{
		KeyWords::AdjustKeyWords(pDrawArgs, pItemMetrics);
		__super::GetItemMetrics(pDrawArgs, pItemMetrics);
		if (pDrawArgs->pControl->HasFocus() && pDrawArgs->pRow->IsFocused() && pDrawArgs->pColumn == pDrawArgs->pControl->GetFocusedColumn())
		{
			COLORREF color = pItemMetrics->clrBackground;
			const float fac = HFAC;
			color = RGB(GetRValue(color) * fac, GetGValue(color) * fac, GetBValue(color) * fac);
			color = HCOL;
			pItemMetrics->clrBackground = color;
		}
	}

#ifdef DSR_ENABLE_CUSTOM_DRAW
	int Draw(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs)
	{
		if (pDrawArgs->pRow->IsFocused() && pDrawArgs->pControl->HasFocus())
		{
			XTP_REPORTRECORDITEM_METRICS* pMetrics = new XTP_REPORTRECORDITEM_METRICS();
			pDrawArgs->pItem->GetItemMetrics(pDrawArgs, pMetrics);
			pDrawArgs->pRow->GetRecord()->GetItemMetrics(pDrawArgs, pMetrics);
			CXTPReportPaintManager* pPaintManager = pDrawArgs->pControl->GetPaintManager();
			CXTPPaintManagerColor backHT = pPaintManager->m_clrHighlightText;
			CXTPPaintManagerColor backH = pPaintManager->m_clrHighlight;
			pPaintManager->m_clrHighlightText = pMetrics->clrForeground;
			pPaintManager->m_clrHighlight = pMetrics->clrBackground;
			int w = __super::Draw(pDrawArgs);
			pPaintManager->m_clrHighlightText = backHT;
			pPaintManager->m_clrHighlight = backH;
			pMetrics->InternalRelease();
			return w;
		}
		else return __super::Draw(pDrawArgs);
	}
#endif

	SelectCallback m_cb;
	T&             m_valueRef;
	bool           m_bIsPreview;
};

class CHelperCheckedItem : public CXTPReportRecordItemText
{
public:
	CHelperCheckedItem(bool& ref) : m_valueRef(ref) {}

	virtual void OnEditChanged(XTP_REPORTRECORDITEM_ARGS* pItemArgs, LPCTSTR szText)
	{
		// CryLogAlways("NewText: %s", szText);
		__super::OnEditChanged(pItemArgs, szText);
		m_valueRef = GetValue();
	}

	virtual void SetChecked(BOOL bChecked)
	{
		m_valueRef = bChecked;
		__super::SetChecked(bChecked);
	}

#ifdef DSR_ENABLE_CUSTOM_DRAW
	int Draw(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs)
	{
		if (pDrawArgs->pRow->IsFocused() && pDrawArgs->pControl->HasFocus())
		{
			XTP_REPORTRECORDITEM_METRICS* pMetrics = new XTP_REPORTRECORDITEM_METRICS();
			pDrawArgs->pItem->GetItemMetrics(pDrawArgs, pMetrics);
			pDrawArgs->pRow->GetRecord()->GetItemMetrics(pDrawArgs, pMetrics);
			CXTPReportPaintManager* pPaintManager = pDrawArgs->pControl->GetPaintManager();
			CXTPPaintManagerColor backHT = pPaintManager->m_clrHighlightText;
			CXTPPaintManagerColor backH = pPaintManager->m_clrHighlight;
			pPaintManager->m_clrHighlightText = pMetrics->clrForeground;
			pPaintManager->m_clrHighlight = pMetrics->clrBackground;
			int w = __super::Draw(pDrawArgs);
			pPaintManager->m_clrHighlightText = backHT;
			pPaintManager->m_clrHighlight = backH;
			pMetrics->InternalRelease();
			return w;
		}
		else return __super::Draw(pDrawArgs);
	}
#endif

	void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	{
		__super::GetItemMetrics(pDrawArgs, pItemMetrics);
		if (pDrawArgs->pControl->HasFocus() && pDrawArgs->pRow->IsFocused() && pDrawArgs->pColumn == pDrawArgs->pControl->GetFocusedColumn())
		{
			COLORREF color = pItemMetrics->clrBackground;
			const float fac = HFAC;
			color = RGB(GetRValue(color) * fac, GetGValue(color) * fac, GetBValue(color) * fac);
			color = HCOL;
			pItemMetrics->clrBackground = color;
		}
	}

	bool& m_valueRef;
};

template<typename T>
class CHelperConstraint : public CXTPReportRecordItem
{
public:
	CHelperConstraint(T& ref) : m_valueRef(ref)
	{}

	CString GetCaption(CXTPReportColumn* pColumn)
	{
		CXTPReportRecordItemConstraint* pConstraint = pColumn->GetEditOptions()->FindConstraint(static_cast<DWORD_PTR>(m_valueRef));
		ASSERT(pConstraint);
		if (pConstraint)
			return pConstraint->m_strConstraint;
		return "";
	}

	void OnConstraintChanged(XTP_REPORTRECORDITEM_ARGS* pItemArgs, CXTPReportRecordItemConstraint* pConstraint)
	{
		m_valueRef = static_cast<T>(pConstraint->m_dwData);
		__super::OnConstraintChanged(pItemArgs, pConstraint);
	}

	void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	{
		KeyWords::AdjustKeyWords(pDrawArgs, pItemMetrics);
		__super::GetItemMetrics(pDrawArgs, pItemMetrics);
		if (pDrawArgs->pControl->HasFocus() && pDrawArgs->pRow->IsFocused() && pDrawArgs->pColumn == pDrawArgs->pControl->GetFocusedColumn())
		{
			COLORREF color = pItemMetrics->clrBackground;
			const float fac = HFAC;
			color = RGB(GetRValue(color) * fac, GetGValue(color) * fac, GetBValue(color) * fac);
			color = HCOL;
			pItemMetrics->clrBackground = color;
		}
	}

	virtual void OnBeginEdit(XTP_REPORTRECORDITEM_ARGS* pItemArgs)
	{
		__super::OnBeginEdit(pItemArgs);
		if (!g_bAlignInplaceControls)
			return;

		CXTPReportInplaceButtons* pInpaceButtons = pItemArgs->pControl->GetInplaceButtons();
		if (pInpaceButtons && pInpaceButtons->GetSize() > 0)
		{
			CRect rcButtons(pItemArgs->rcItem);
			for (int i = pInpaceButtons->GetSize() - 1; i >= 0; --i)
			{
				CXTPReportInplaceButton* pButton = pInpaceButtons->GetAt(i);
				// get rect of button
				CRect buttonRect;
				pButton->GetClientRect(&buttonRect);
				buttonRect.MoveToXY(rcButtons.right - buttonRect.Width(), rcButtons.top);
				pButton->SetWindowPos(0, buttonRect.left, buttonRect.top + 1, buttonRect.Width(), buttonRect.Height() - 2, SWP_SHOWWINDOW | SWP_NOZORDER);
				rcButtons.right -= buttonRect.Width();
				/*
				   pButton->SetActiveWindow();
				   pButton->SetFocus();
				   pButton->ShowWindow(SW_HIDE);
				   pButton->ShowWindow(SW_SHOW);
				 */
			}
			CXTPReportInplaceEdit* pEdit = pItemArgs->pControl->GetInplaceEdit();
			if (pEdit && pEdit->GetSafeHwnd() && pEdit->GetItem() == this)
			{
				CRect editRect;
				pEdit->GetWindowRect(&editRect);
				pItemArgs->pControl->ScreenToClient(&editRect);
				editRect.right = rcButtons.right;
				pEdit->SetWindowPos(0, editRect.left, editRect.top, editRect.Width(), editRect.Height(), SWP_SHOWWINDOW | SWP_NOZORDER);
			}
		}
	}

#ifdef DSR_ENABLE_CUSTOM_DRAW
	int Draw(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs)
	{
		if (pDrawArgs->pRow->IsFocused() && pDrawArgs->pControl->HasFocus())
		{
			XTP_REPORTRECORDITEM_METRICS* pMetrics = new XTP_REPORTRECORDITEM_METRICS();
			pDrawArgs->pItem->GetItemMetrics(pDrawArgs, pMetrics);
			pDrawArgs->pRow->GetRecord()->GetItemMetrics(pDrawArgs, pMetrics);
			CXTPReportPaintManager* pPaintManager = pDrawArgs->pControl->GetPaintManager();
			CXTPPaintManagerColor backHT = pPaintManager->m_clrHighlightText;
			CXTPPaintManagerColor backH = pPaintManager->m_clrHighlight;
			pPaintManager->m_clrHighlightText = pMetrics->clrForeground;
			pPaintManager->m_clrHighlight = pMetrics->clrBackground;
			int w = __super::Draw(pDrawArgs);
			pPaintManager->m_clrHighlightText = backHT;
			pPaintManager->m_clrHighlight = backH;
			pMetrics->InternalRelease();
			return w;
		}
		else return __super::Draw(pDrawArgs);
	}
#endif

	virtual void OnInplaceButtonDown(CXTPReportInplaceButton* pButton)
	{
		if (pButton->GetID() == XTP_ID_REPORT_COMBOBUTTON)
		{
			WorkaroundComboButton(this, pButton);
		}
		else
			__super::OnInplaceButtonDown(pButton);
	}

	T& m_valueRef;
};

static const COLORREF COL_ACTOR = RGB(100, 100, 200);
static const COLORREF COL_SOUND = RGB(120, 100, 220);
static const COLORREF COL_ANIM = RGB(100, 100, 220);
static const COLORREF COL_FACIAL = RGB(100, 120, 200);
static const COLORREF COL_LOOKAT = RGB(120, 100, 200);
static const COLORREF COL_REST = RGB(120, 120, 200);

CDialogScriptRecord::CDialogScriptRecord()
{
	m_pScript = 0;
}

CDialogScriptRecord::CDialogScriptRecord(CEditorDialogScript* pScript, const CEditorDialogScript::SScriptLine* pLine)
{
	m_line = *pLine;
	m_pScript = pScript;

	FillItems();
}

void
CDialogScriptRecord::FillItems()
{
	CXTPReportRecordItem* pItem = 0;
	pItem = AddItem(new CLineNo());
	// Actor
	pItem = AddItem(new CHelperConstraint<CEditorDialogScript::TActorID>(m_line.m_actor));
	pItem->SetBackgroundColor(COL_ACTOR);

	// Sound
	pItem = AddItem(new CHelperValue<string, CXTPReportRecordItemText>
	                  (m_line.m_audioTriggerName, functor(*this, &CDialogScriptRecord::OnBrowseAudioTrigger)));
	pItem->SetBackgroundColor(COL_SOUND);

	// AnimStopWithSound
	pItem = AddItem(new CHelperCheckedItem(m_line.m_flagSoundStopsAnim));
	pItem->HasCheckbox(TRUE);
	pItem->SetChecked(m_line.m_flagSoundStopsAnim);
	pItem->SetBackgroundColor(COL_ANIM);

	// AnimUseEx
	pItem = AddItem(new CHelperCheckedItem(m_line.m_flagAGEP));
	pItem->HasCheckbox(TRUE);
	pItem->SetChecked(m_line.m_flagAGEP);
	pItem->SetBackgroundColor(COL_ANIM);

	// AnimType [Action/Signal]
	pItem = AddItem(new CHelperConstraint<bool>(m_line.m_flagAGSignal));
	pItem->SetBackgroundColor(COL_ANIM);

	// Anim
	pItem = AddItem(new CHelperValue<string, CXTPReportRecordItemText>
	                  (m_line.m_anim, functor(*this, &CDialogScriptRecord::OnBrowseAG)));
	pItem->SetBackgroundColor(COL_ANIM);

	// Facial Expr
	pItem = AddItem(new CHelperValue<string, CXTPReportRecordItemText>
	                  (m_line.m_facial, functor(*this, &CDialogScriptRecord::OnBrowseFacial)));
	pItem->SetBackgroundColor(COL_FACIAL);

	// Facial Weight
	pItem = AddItem(new CHelperValue<float, CXTPReportRecordItemNumber>(m_line.m_facialWeight));
	pItem->SetFormatString("%.2f");
	pItem->SetBackgroundColor(COL_FACIAL);

	// Facial FadeTime
	pItem = AddItem(new CHelperValue<float, CXTPReportRecordItemNumber>(m_line.m_facialFadeTime));
	pItem->SetFormatString("%.2f");
	pItem->SetBackgroundColor(COL_FACIAL);

	// LookAtSticky
	pItem = AddItem(new CHelperCheckedItem(m_line.m_flagLookAtSticky));
	pItem->HasCheckbox(TRUE);
	pItem->SetChecked(m_line.m_flagLookAtSticky);
	pItem->SetBackgroundColor(COL_LOOKAT);

	// LookAtTarget
	pItem = AddItem(new CHelperConstraint<CEditorDialogScript::TActorID>(m_line.m_lookatActor));
	pItem->SetBackgroundColor(COL_LOOKAT);

	// Delay
	pItem = AddItem(new CHelperValue<float, CXTPReportRecordItemNumber>(m_line.m_delay));
	pItem->SetFormatString("%.2f");
	pItem->SetBackgroundColor(COL_REST);

	// no preview at the moment. looks bad
	// SetPreviewItem(new CXTPReportRecordItemPreview(m_line.m_desc.c_str()));

	// Description [also preview]
	pItem = AddItem(new CHelperValue<string, CXTPReportRecordItemText>(m_line.m_desc, 0));
	pItem->SetBackgroundColor(COL_REST);
}

struct MsgHelper
{
	MsgHelper(const CString& msg) : msg(msg), bShow(true) {}
	~MsgHelper()
	{
		if (bShow)
		{
			CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr(msg));
		}
	}
	CString msg;
	bool    bShow;
};

void CDialogScriptRecord::OnBrowseAudioTrigger(string& value, CDialogScriptRecord* pRecord)
{
	dll_string newValue = GetIEditor()->GetResourceSelectorHost()->GetSelector("AudioTrigger")->SelectResource(SResourceSelectorContext(), value);
	value = newValue.c_str();
}

void CDialogScriptRecord::OnBrowseFacial(string& value, CDialogScriptRecord* pRecord)
{
	MsgHelper msg(_T("Please select an Entity to be used\nas reference for facial expression."));

	CBaseObject* pObject = GetIEditor()->GetSelectedObject();
	if (pObject == 0)
		return;

	IEntity* pEntity = pObject->GetIEntity();
	if (pEntity == 0)
		return;

	ICharacterInstance* pChar = pEntity->GetCharacter(0);
	if (pChar == 0)
		return;

	IFacialInstance* pFacialInstance = pChar->GetFacialInstance();
	if (pFacialInstance == 0)
		return;

	IFacialModel* pFacialModel = pFacialInstance->GetFacialModel();
	if (pFacialModel == 0)
		return;

	IFacialEffectorsLibrary* pLibrary = pFacialModel->GetLibrary();
	if (pLibrary == 0)
		return;

	// if (efCount == 0)
	//	return;

	struct Recurser
	{
		void Recurse(IFacialEffector* pEff, CString path, std::set<CString>& itemSet)
		{
			const EFacialEffectorType efType = pEff->GetType();
			if (efType != EFE_TYPE_GROUP)
			{
				CString val = path;
				if (val.IsEmpty() == false)
					val += ".";
				val += pEff->GetName();
				itemSet.insert(val);
			}
			else
			{
				if (path.IsEmpty() == false)
					path += ".";
				if (strcmp(pEff->GetName(), "Root") != 0)
					path += pEff->GetName();
				for (int i = 0; i < pEff->GetSubEffectorCount(); ++i)
				{
					Recurse(pEff->GetSubEffector(i), path, itemSet);
				}
			}
		}
	};

	std::set<CString> itemSet;
	Recurser recurser;
	recurser.Recurse(pLibrary->GetRoot(), "", itemSet);
	if (itemSet.empty())
	{
		msg.msg = _T("Entity has no facial expressions.");
		return;
	}

	msg.bShow = false;

	CGenericSelectItemDialog dlg;
	std::vector<CString> items(itemSet.begin(), itemSet.end());

	// too bad. in dialog, items are with path, but value is without
	// as facial expressions have a unique name
	CString valString = value.c_str();
	if (valString.IsEmpty() == false)
	{
		std::vector<CString>::iterator iter = items.begin();
		while (iter != items.end())
		{
			const CString& s = *iter;
			if (s.Find(valString) >= 0)
			{
				valString = s;
				break;
			}
			++iter;
		}
	}

	dlg.SetMode(CGenericSelectItemDialog::eMODE_TREE);
	dlg.SetTreeSeparator(".");
	dlg.SetItems(items);
	dlg.SetTitle(_T("Choose Facial Expression"));
	dlg.PreSelectItem(valString);
	if (dlg.DoModal() == IDOK)
	{
		valString = dlg.GetSelectedItem();
		int delim = valString.ReverseFind('.');
		if (delim > 0)
			valString = valString.Mid(delim + 1);
		value = valString.GetString();
	}
}

void CDialogScriptRecord::OnBrowseAG(string& value, CDialogScriptRecord* pRecord)
{
	MsgHelper msg(_T("Please select an Entity to be used\nas reference for AnimationGraph actions/signals."));

	CBaseObject* pObject = GetIEditor()->GetSelectedObject();
	if (pObject == 0)
		return;

	IEntity* pEntity = pObject->GetIEntity();
	if (pEntity == 0)
		return;

	const CEditorDialogScript::SScriptLine* pLine = pRecord->GetLine();
	int method = pLine->m_flagAGSignal ? 1 : 0;

	if (gEnv->pGameFramework)
	{
		IMannequin& mannequin = gEnv->pGameFramework->GetMannequinInterface();

		IActionController* pActionController = mannequin.FindActionController(*pEntity);
		if (pActionController)
		{
			msg.msg = _T("Dialogs do not support starting Mannequin fragments");
			return;
		}
	}
}

void CDialogScriptRecord::GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
{
	if (pDrawArgs->pRow->IsFocused() && pDrawArgs->pControl->HasFocus())
	{
		COLORREF color = pItemMetrics->clrBackground;
		const float fac = HLOWFAC;
		color = RGB(GetRValue(color) * fac, GetGValue(color) * fac, GetBValue(color) * fac);
		pItemMetrics->clrBackground = color;
		/*
		   color = pItemMetrics->clrForeground;
		   color = RGB(GetRValue(color)*fac, GetGValue(color)*fac, GetBValue(color)*fac);
		   pItemMetrics->clrForeground = color;
		 */
	}
}

void CDialogScriptRecord::Swap(CDialogScriptRecord* pOther)
{
	CEditorDialogScript* pTmpScript = m_pScript;
	CEditorDialogScript::SScriptLine tmpLine = m_line;

	RemoveAll();
	m_line = pOther->m_line;
	m_pScript = pOther->m_pScript;
	FillItems();

	pOther->RemoveAll();
	pOther->m_line = tmpLine;
	pOther->m_pScript = pTmpScript;
	pOther->FillItems();
}

