// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "TransitionBrowser.h"
#include "PreviewerPage.h"

#include "../FragmentEditor.h"
#include "../MannequinBase.h"

#include "../MannTagEditorDialog.h"
#include "../MannequinDialog.h"
#include "../SequencerDopeSheetBase.h"
#include "../MannTransitionPicker.h"
#include "../MannTransitionSettings.h"
#include <CryString/StringUtils.h>

#include "Util/MFCUtil.h"

#include <CryGame/IGameFramework.h>
#include <ICryMannequinEditor.h>

IMPLEMENT_DYNAMIC(CTransitionBrowser, CXTResizeFormView)

BEGIN_MESSAGE_MAP(CTransitionBrowser, CXTResizeFormView)
ON_BN_CLICKED(IDC_NEW, OnNewBtn)
ON_BN_CLICKED(IDEDIT, OnEditBtn)
ON_BN_CLICKED(IDDELETE, OnDeleteBtn)
ON_BN_CLICKED(IDC_COPY, OnCopyBtn)
ON_BN_CLICKED(IDC_BLEND_OPEN, OnOpenBtn)

ON_BN_CLICKED(IDC_SWAP_FRAGMENT_FILTER, OnSwapFragmentFilters)
ON_BN_CLICKED(IDC_SWAP_TAG_FILTER, OnSwapTagFilters)
ON_BN_CLICKED(IDC_SWAP_ANIM_CLIP_FILTER, OnSwapAnimClipFilters)

ON_EN_CHANGE(IDC_FILTER_TAGS_FROM, OnChangeFilterTagsFrom)
ON_EN_CHANGE(IDC_FILTER_FRAGMENTS_FROM, OnChangeFilterFragmentsFrom)
ON_EN_CHANGE(IDC_FILTER_ANIM_CLIP_FROM, OnChangeFilterAnimClipFrom)
ON_EN_CHANGE(IDC_FILTER_TAGS_TO, OnChangeFilterTagsTo)
ON_EN_CHANGE(IDC_FILTER_FRAGMENTS_TO, OnChangeFilterFragmentsTo)
ON_EN_CHANGE(IDC_FILTER_ANIM_CLIP_TO, OnChangeFilterAnimClipTo)

ON_WM_SIZE()
ON_WM_SETFOCUS()

ON_CBN_SELCHANGE(IDC_CONTEXT, OnChangeContext)
END_MESSAGE_MAP()

const CString TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT("<FragmentID Filter>");
const CString TRANSITION_DEFAULT_TAG_FILTER_TEXT("<Tag Filter>");
const CString TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT("<Anim Filter>");

#pragma warning(push)
#pragma warning(disable:4355) // 'this': used in base member initializer list
CTransitionBrowser::CTransitionBrowser(CWnd* pParent, CTransitionEditorPage& transitionEditorPage)
	: CXTResizeFormView(CTransitionBrowser::IDD)
	, m_animDB(NULL)
	, m_transitionEditorPage(transitionEditorPage)
	, m_context(NULL)
	, m_scopeContextDataIdx(0)
	, m_treeCtrl(this)
	, m_filterAnimClipFrom(TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT)
	, m_filterAnimClipTo(TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT)
{
	LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
	Create(lpClassName, "TransitionBrowser", 0, CRect(0, 0, 100, 100), pParent, 0, nullptr);
	ShowWindow(FALSE);

	m_toolTip.Create(this);
	m_toolTip.AddTool(&m_newButton, "New Transition");
	m_toolTip.AddTool(&m_deleteButton, "Delete Transition");
	m_toolTip.AddTool(&m_editButton, "Edit Transition");
	m_toolTip.AddTool(&m_copyButton, "Copy Transition");
	m_toolTip.AddTool(&m_openButton, "Open Transition in Transition Editor");
	m_toolTip.AddTool(&m_swapFragmentButton, "Swap Fragment Filters");
	m_toolTip.AddTool(&m_swapTagButton, "Swap Tag Filters");
	m_toolTip.AddTool(&m_swapAnimClipButton, "Swap Anim Clip Filters");
	m_toolTip.Activate(true);
}
#pragma warning(pop)

CTransitionBrowser::~CTransitionBrowser()
{
}

void CTransitionBrowser::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CONTEXT, m_cbContext);

	DDX_Control(pDX, IDC_NEW, m_newButton);
	DDX_Control(pDX, IDEDIT, m_editButton);
	DDX_Control(pDX, IDC_COPY, m_copyButton);
	DDX_Control(pDX, IDDELETE, m_deleteButton);
	DDX_Control(pDX, IDC_BLEND_OPEN, m_openButton);

	DDX_Control(pDX, IDC_SWAP_FRAGMENT_FILTER, m_swapFragmentButton);
	DDX_Control(pDX, IDC_SWAP_TAG_FILTER, m_swapTagButton);
	DDX_Control(pDX, IDC_SWAP_ANIM_CLIP_FILTER, m_swapAnimClipButton);

	DDX_Control(pDX, IDC_FILTER_TAGS_FROM, m_editFilterTagsFrom);
	DDX_Control(pDX, IDC_FILTER_FRAGMENTS_FROM, m_editFilterFragmentIDsFrom);
	DDX_Control(pDX, IDC_FILTER_ANIM_CLIP_FROM, m_editFilterAnimClipFrom);
	DDX_Control(pDX, IDC_FILTER_TAGS_TO, m_editFilterTagsTo);
	DDX_Control(pDX, IDC_FILTER_FRAGMENTS_TO, m_editFilterFragmentIDsTo);
	DDX_Control(pDX, IDC_FILTER_ANIM_CLIP_TO, m_editFilterAnimClipTo);
}

BOOL CTransitionBrowser::OnInitDialog()
{
	__super::OnInitDialog();

	if (!m_treeCtrl.Create(WS_CHILD | WS_TABSTOP | WS_VISIBLE | WM_VSCROLL, CRect(0, 0, 0, 0), this, IDC_MANN_TRANSITION_TREECTRL))
	{
		TRACE(_T("Failed to create view window\n"));
		return FALSE;
	}
	m_treeCtrl.ModifyStyleEx(0, WS_EX_CLIENTEDGE);
	m_treeCtrl.SetCallback(CTreeCtrlReport::eCB_OnSelect, functor(*this, &CTransitionBrowser::OnSelectionChanged));
	m_treeCtrl.SetCallback(CTreeCtrlReport::eCB_OnDblClick, functor(*this, &CTransitionBrowser::OnItemDoubleClicked));

	// This is a bit nasty - as tree control is created at runtime we can't set the tab order correctly through CryEdit.rc
	// so instead we need to use SetWindowPos to move the tabbing order
	CWnd* treeControl = GetDlgItem(IDC_MANN_TRANSITION_TREECTRL);
	treeControl->SetWindowPos(GetDlgItem(IDC_FILTER_ANIM_CLIP_TO), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	CMFCUtils::LoadTrueColorImageList(m_buttonImages, IDB_MANN_TAGDEF_TOOLBAR, 16, RGB(255, 0, 255));
	m_newButton.SetImage(m_buttonImages, 5);
	m_editButton.SetImage(m_buttonImages, 6);
	m_deleteButton.SetImage(m_buttonImages, 7);

	CStatic* pStatic = (CStatic*)GetDlgItem(IDC_FRAGMENT_FILTER_ICON_LHS);
	if (pStatic != NULL)
	{
		CImage img;
		img.Load("Editor\\UI\\Icons\\mann_folder.png");
		pStatic->SetBitmap(img.Detach());
	}
	pStatic = (CStatic*)GetDlgItem(IDC_FRAGMENT_FILTER_ICON_RHS);
	if (pStatic != NULL)
	{
		CImage img;
		img.Load("Editor\\UI\\Icons\\mann_folder.png");
		pStatic->SetBitmap(img.Detach());
	}
	pStatic = (CStatic*)GetDlgItem(IDC_TAG_FILTER_ICON_LHS);
	if (pStatic != NULL)
	{
		CImage img;
		img.Load("Editor\\UI\\Icons\\mann_tag.png");
		pStatic->SetBitmap(img.Detach());
	}
	pStatic = (CStatic*)GetDlgItem(IDC_TAG_FILTER_ICON_RHS);
	if (pStatic != NULL)
	{
		CImage img;
		img.Load("Editor\\UI\\Icons\\mann_tag.png");
		pStatic->SetBitmap(img.Detach());
	}
	pStatic = (CStatic*)GetDlgItem(IDC_ANIM_CLIP_FILTER_ICON_LHS);
	if (pStatic != NULL)
	{
		CImage img;
		img.Load("Editor\\Icons\\animation\\animation.png");
		pStatic->SetBitmap(img.Detach());
	}
	pStatic = (CStatic*)GetDlgItem(IDC_ANIM_CLIP_FILTER_ICON_RHS);
	if (pStatic != NULL)
	{
		CImage img;
		img.Load("Editor\\Icons\\animation\\animation.png");
		pStatic->SetBitmap(img.Detach());
	}

	SetResize(IDC_FRAGMENT_FILTER_ICON_LHS, SZ_TOP_CENTER, SZ_TOP_CENTER);
	SetResize(IDC_TAG_FILTER_ICON_LHS, SZ_TOP_CENTER, SZ_TOP_CENTER);
	SetResize(IDC_ANIM_CLIP_FILTER_ICON_LHS, SZ_TOP_CENTER, SZ_TOP_CENTER);

	SetResize(IDC_FRAGMENT_FILTER_ICON_RHS, SZ_TOP_CENTER, SZ_TOP_CENTER);
	SetResize(IDC_TAG_FILTER_ICON_RHS, SZ_TOP_CENTER, SZ_TOP_CENTER);
	SetResize(IDC_ANIM_CLIP_FILTER_ICON_RHS, SZ_TOP_CENTER, SZ_TOP_CENTER);

	SetResize(IDC_FILTER_FRAGMENTS_FROM, SZ_TOP_LEFT, SZ_TOP_CENTER);
	SetResize(IDC_FILTER_TAGS_FROM, SZ_TOP_LEFT, SZ_TOP_CENTER);
	SetResize(IDC_FILTER_ANIM_CLIP_FROM, SZ_TOP_LEFT, SZ_TOP_CENTER);

	SetResize(IDC_FILTER_FRAGMENTS_TO, SZ_TOP_CENTER, SZ_TOP_RIGHT);
	SetResize(IDC_FILTER_TAGS_TO, SZ_TOP_CENTER, SZ_TOP_RIGHT);
	SetResize(IDC_FILTER_ANIM_CLIP_TO, SZ_TOP_CENTER, SZ_TOP_RIGHT);

	SetResize(IDC_CONTEXT, SZ_TOP_LEFT, SZ_TOP_RIGHT);
	SetResize(IDC_NEW, SZ_BOTTOM_LEFT, CXTResizePoint(1.f / 3, 1));
	SetResize(IDC_BLEND_OPEN, CXTResizePoint(1.f / 3, 1), CXTResizePoint(2.f / 3, 1));
	SetResize(IDEDIT, CXTResizePoint(2.f / 3, 1), SZ_BOTTOM_RIGHT);
	SetResize(IDC_COPY, SZ_BOTTOM_LEFT, CXTResizePoint(1.f / 2, 1));
	SetResize(IDDELETE, CXTResizePoint(1.f / 2, 1), SZ_BOTTOM_RIGHT);

	SetResize(IDC_SWAP_FRAGMENT_FILTER, SZ_TOP_CENTER, SZ_TOP_CENTER);
	SetResize(IDC_SWAP_TAG_FILTER, SZ_TOP_CENTER, SZ_TOP_CENTER);
	SetResize(IDC_SWAP_ANIM_CLIP_FILTER, SZ_TOP_CENTER, SZ_TOP_CENTER);

	SetResize(IDC_TRANSITION_FROM, SZ_TOP_LEFT, SZ_TOP_LEFT);
	SetResize(IDC_TRANSITION_TO, SZ_TOP_CENTER, SZ_TOP_CENTER);

	m_editFilterTagsFrom.SetWindowText(TRANSITION_DEFAULT_TAG_FILTER_TEXT);
	m_editFilterTagsFrom.SetSel(0, TRANSITION_DEFAULT_TAG_FILTER_TEXT.GetLength());
	m_editFilterFragmentIDsFrom.SetWindowText(TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT);
	m_editFilterFragmentIDsFrom.SetSel(0, TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT.GetLength());
	m_editFilterAnimClipFrom.SetWindowText(TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT);
	m_editFilterAnimClipFrom.SetSel(0, TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT.GetLength());

	m_editFilterTagsTo.SetWindowText(TRANSITION_DEFAULT_TAG_FILTER_TEXT);
	m_editFilterTagsTo.SetSel(0, TRANSITION_DEFAULT_TAG_FILTER_TEXT.GetLength());
	m_editFilterFragmentIDsTo.SetWindowText(TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT);
	m_editFilterFragmentIDsTo.SetSel(0, TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT.GetLength());
	m_editFilterAnimClipTo.SetWindowText(TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT);
	m_editFilterAnimClipTo.SetSel(0, TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT.GetLength());

	Refresh();

	return TRUE;
}

void CTransitionBrowser::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_newButton.GetSafeHwnd())
	{
		CRect statRect;
		::GetWindowRect(m_deleteButton.GetSafeHwnd(), &statRect);
		ScreenToClient(&statRect);
		if (m_treeCtrl.GetSafeHwnd())
		{
			m_treeCtrl.MoveWindow(6, 144, std::max(statRect.right, -12L), std::max(1L, statRect.top - 176L));
		}
	}
}

void CTransitionBrowser::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);

	if (auto pMannequinDialog = CMannequinDialog::GetCurrentInstance())
	{
		if (auto pDockingPaneManager = pMannequinDialog->GetDockingPaneManager())
		{
			if (pDockingPaneManager->IsPaneSelected(CMannequinDialog::IDW_PREVIEWER_PANE) == false)
			{
				pDockingPaneManager->ShowPane(CMannequinDialog::IDW_TRANSITION_EDITOR_PANE, FALSE);
			}
		}
	}
}

void CTransitionBrowser::SetContext(SMannequinContexts& context)
{
	m_context = &context;
	m_scopeContextDataIdx = -1;

	m_cbContext.ResetContent();
	const uint32 numContexts = context.m_contextData.size();
	for (uint32 i = 0; i < numContexts; i++)
	{
		m_cbContext.AddString(context.m_contextData[i].name);
	}
	m_cbContext.SetCurSel(0);

	if (context.m_controllerDef)
	{
		SetScopeContextDataIdx(0);
	}
}

BOOL CTransitionBrowser::PreTranslateMessage(MSG* pMsg)
{
	m_toolTip.RelayEvent(pMsg);

	return __super::PreTranslateMessage(pMsg);
}

void CTransitionBrowser::SetDatabase(IAnimationDatabase* animDB)
{
	if (m_animDB != animDB)
	{
		m_animDB = animDB;

		Refresh();
	}
}

void CTransitionBrowser::OnSelectionChanged(CTreeItemRecord* pRec)
{
	UpdateButtonStatus();
}

void CTransitionBrowser::OnItemDoubleClicked(CTreeItemRecord* pRec)
{
	OpenSelected();
}

void CTransitionBrowser::Refresh()
{
	CTransitionBrowserTreeCtrl::SPresentationState treePresentationState;
	m_treeCtrl.SavePresentationState(OUT treePresentationState);

	m_treeCtrl.DeleteAllItems();
	m_treeCtrl.BeginUpdate();
	typedef std::vector<std::pair<CTransitionBrowserRecord*, CTransitionBrowserRecord*>> TTreeEntries;
	TTreeEntries treeEntries;
	if (m_animDB)
	{
		SEditorFragmentBlendID::TEditorFragmentBlendIDArray blendIDs;
		MannUtils::GetMannequinEditorManager().GetFragmentBlends(m_animDB, blendIDs);

		for (SEditorFragmentBlendID::TEditorFragmentBlendIDArray::const_iterator iterBlend = blendIDs.begin(), end = blendIDs.end(); iterBlend != end; ++iterBlend)
		{
			bool bPushBranch = false;
			TTreeEntries branchEntries;
			SEditorFragmentBlendVariant::TEditorFragmentBlendVariantArray variants;
			MannUtils::GetMannequinEditorManager().GetFragmentBlendVariants(m_animDB, (*iterBlend).fragFrom, (*iterBlend).fragTo, variants);

			// get the fragment names
			const CTagDefinition& rFragDef = m_animDB->GetFragmentDefs();
			const CString cFragFrom = (TAG_ID_INVALID == (*iterBlend).fragFrom) ? "<Any Fragment>" : rFragDef.GetTagName((*iterBlend).fragFrom);
			const CString cFragTo = (TAG_ID_INVALID == (*iterBlend).fragTo) ? "<Any Fragment>" : rFragDef.GetTagName((*iterBlend).fragTo);

			// if the filter is valid and appears in the fragment name then let it in, otherwise skip it
			const CString cFragFromLower = CryStringUtils::toLower(cFragFrom.GetString());
			if (m_filterFragmentIDTextFrom[0] != 0 && cFragFromLower.Find(m_filterFragmentIDTextFrom) < 0)
				continue;

			const CString cFragToLower = CryStringUtils::toLower(cFragTo.GetString());
			if (m_filterFragmentIDTextTo[0] != 0 && cFragToLower.Find(m_filterFragmentIDTextTo) < 0)
				continue;

			// add to the tree
			CTransitionBrowserRecord* pRecord = new CTransitionBrowserRecord(cFragFrom, cFragTo);
			CTransitionBrowserRecord* pParentRecord = nullptr;
			branchEntries.push_back(std::make_pair(pRecord, pParentRecord));
			pParentRecord = pRecord;

			for (SEditorFragmentBlendVariant::TEditorFragmentBlendVariantArray::const_iterator iterVariant = variants.begin(), end = variants.end(); iterVariant != end; ++iterVariant)
			{
				const uint32 numBlends = m_animDB->GetNumBlends(
				  (*iterBlend).fragFrom,
				  (*iterBlend).fragTo,
				  (*iterVariant).tagsFrom,
				  (*iterVariant).tagsTo);

				// get the tag names
				static const uint32 MAXBUFFERSIZE = 256;
				char fromBuffer[MAXBUFFERSIZE];
				char toBuffer[MAXBUFFERSIZE];

				MannUtils::FlagsToTagList(fromBuffer, sizeof(fromBuffer), (*iterVariant).tagsFrom, (*iterBlend).fragFrom, (*m_context->m_controllerDef), "<ANY Tag>");
				MannUtils::FlagsToTagList(toBuffer, sizeof(toBuffer), (*iterVariant).tagsTo, (*iterBlend).fragTo, (*m_context->m_controllerDef), "<ANY Tag>");

				// filter out by the tags now
				bool include = true;
				const size_t numFromFilters = m_filtersFrom.size();
				if (numFromFilters > 0)
				{
					CString sTagState(fromBuffer);
					sTagState.MakeLower();
					for (uint32 f = 0; f < numFromFilters; f++)
					{
						if (sTagState.Find(m_filtersFrom[f]) < 0)
						{
							include = false;
							break;
						}
					}
				}
				const size_t numToFilters = m_filtersTo.size();
				if (numToFilters > 0 && include)
				{
					CString sTagState(toBuffer);
					sTagState.MakeLower();
					for (uint32 f = 0; f < numToFilters; f++)
					{
						if (sTagState.Find(m_filtersTo[f]) < 0)
						{
							include = false;
							break;
						}
					}
				}
				if (!include)
					continue;

				// Filter by anim clip
				bool bCheckTo = m_filterAnimClipTo.GetLength() > 0 && m_filterAnimClipTo[0] != TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT[0];
				bool bCheckFrom = m_filterAnimClipFrom.GetLength() > 0 && m_filterAnimClipFrom[0] != TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT[0];

				bool bMatchTo = bCheckTo && FilterByAnimClip((*iterBlend).fragTo, (*iterVariant).tagsTo, m_filterAnimClipTo);
				bool bMatchFrom = bCheckFrom && FilterByAnimClip((*iterBlend).fragFrom, (*iterVariant).tagsFrom, m_filterAnimClipFrom);

				bool bCheckToFailed = bCheckTo && !bMatchTo;
				bool bCheckFromFailed = bCheckFrom && !bMatchFrom;

				if (bCheckToFailed || bCheckFromFailed)
				{
					// Anim clip filter was specified, but didn't match anything in this fragment,
					// so don't add fragment to tree hierarchy.
					continue;
				}

				// add to the tree
				CTransitionBrowserRecord* pTSRecord = new CTransitionBrowserRecord(fromBuffer, toBuffer);
				CTransitionBrowserRecord* pParentTSRecord = nullptr;
				branchEntries.push_back(std::make_pair(pTSRecord, pParentRecord));
				pParentTSRecord = pTSRecord;

				for (int blendIdx = 0; blendIdx < numBlends; ++blendIdx)
				{
					const SFragmentBlend* pBlend = m_animDB->GetBlend(
					  (*iterBlend).fragFrom,
					  (*iterBlend).fragTo,
					  (*iterVariant).tagsFrom,
					  (*iterVariant).tagsTo,
					  blendIdx);

					TTransitionID transitionID(
					  (*iterBlend).fragFrom,
					  (*iterBlend).fragTo,
					  (*iterVariant).tagsFrom,
					  (*iterVariant).tagsTo,
					  pBlend->uid);

					CTransitionBrowserRecord* pBlendRecord = new CTransitionBrowserRecord(transitionID, pBlend->selectTime);

					branchEntries.push_back(std::make_pair(pBlendRecord, pParentTSRecord));
				}

				if (numBlends > 0)
				{
					bPushBranch = true;
				}
			}
			if (bPushBranch)
			{
				bPushBranch = true;
				for (TTreeEntries::iterator iterTE = branchEntries.begin(), end = branchEntries.end(); iterTE != end; ++iterTE)
				{
					treeEntries.push_back((*iterTE));
				}
			}
			branchEntries.clear();
		}
	}

	for (TTreeEntries::iterator iterTE = treeEntries.begin(), end = treeEntries.end(); iterTE != end; ++iterTE)
	{
		m_treeCtrl.AddTreeRecord((*iterTE).first, (*iterTE).second);
	}

	m_treeCtrl.EndUpdate();
	m_treeCtrl.Populate();

	m_treeCtrl.RestorePresentationState(treePresentationState);

	// Make sure opened record is visible and bold
	// do NOT unselect in transition editor
	if (HasOpenedRecord())
	{
		CTransitionBrowserRecord* pRecord = m_treeCtrl.FindRecordForTransition(m_openedRecordDataCollection[0].m_transitionID);
		if (pRecord)
			m_treeCtrl.EnsureItemVisible(pRecord);

		m_treeCtrl.SetBold(m_openedRecordDataCollection, TRUE);
	}

	UpdateButtonStatus();
}

void CTransitionBrowserTreeCtrl::SetBoldRecursive(CXTPReportRecords* pRecords, const TTransitionBrowserRecordDataCollection& recordDataCollection, const BOOL bBold)
{
	if (!pRecords)
		return;

	for (int recordIndex = 0; recordIndex < pRecords->GetCount(); recordIndex++) // GetCount changes whenever a row gets expanded!
	{
		CTransitionBrowserRecord* pRecord = (CTransitionBrowserRecord*)pRecords->GetAt(recordIndex);

		bool match = false;
		if ( // Incredibly slow! Use map for example to optimize this
		  std::find(recordDataCollection.begin(), recordDataCollection.end(), pRecord->GetRecordData()) != recordDataCollection.end())
		{
			for (int itemIndex = 0; itemIndex < pRecord->GetItemCount(); ++itemIndex)
			{
				CXTPReportRecordItem* pItem = pRecord->GetItem(itemIndex);
				if (pItem)
				{
					pItem->SetBold(bBold);
					match = true;
				}
			}
		}

		if (match)
		{
			SetBoldRecursive(pRecord->GetChilds(), recordDataCollection, bBold);
		}
	}
}

void CTransitionBrowserTreeCtrl::SetBold(const TTransitionBrowserRecordDataCollection& recordDataCollection, const BOOL bBold)
{
	SetBoldRecursive(GetRecords(), recordDataCollection, bBold);
}

void GetRecordDataCollectionRecursively(CTransitionBrowserRecord* pRecord, OUT TTransitionBrowserRecordDataCollection& recordDataCollection)
{
	recordDataCollection.clear();
	while (pRecord)
	{
		recordDataCollection.push_back(pRecord->GetRecordData());
		pRecord = (CTransitionBrowserRecord*)pRecord->GetParentRecord();
	}
}

void SetBoldIncludingParents(CXTPReportRecord* pRecord, BOOL bBold = TRUE)
{
	while (pRecord)
	{
		for (int i = 0; i < pRecord->GetItemCount(); ++i)
		{
			CXTPReportRecordItem* pItem = pRecord->GetItem(i);
			if (pItem)
			{
				pItem->SetBold(bBold);
			}
		}
		pRecord = pRecord->GetParentRecord();
	}
}

void CTransitionBrowser::SetOpenedRecord(CTransitionBrowserRecord* pRecordToOpen, const TTransitionID& pickedTransitionID)
{
	if (HasOpenedRecord())
	{
		// Unselect currently opened record
		m_treeCtrl.SetBold(m_openedRecordDataCollection, FALSE);
	}

	if (NULL != pRecordToOpen)
	{
		GetRecordDataCollectionRecursively(pRecordToOpen, m_openedRecordDataCollection);
		m_treeCtrl.SetBold(m_openedRecordDataCollection, TRUE);
		m_treeCtrl.EnsureItemVisible(pRecordToOpen);
	}
	else
	{
		m_openedRecordDataCollection.clear();
	}
	m_treeCtrl.RedrawControl();

	// Should have fetched enough data to begin displaying/building the sequence now.
	if (HasOpenedRecord())
	{
		m_openedTransitionID = pickedTransitionID;
	}
	else
	{
		m_openedTransitionID = TTransitionID();
	}

	m_transitionEditorPage.LoadSequence(m_scopeContextDataIdx, m_openedTransitionID.fromID, m_openedTransitionID.toID, m_openedTransitionID.fromFTS, m_openedTransitionID.toFTS, m_openedTransitionID.uid);
	CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->ShowPane(CMannequinDialog::IDW_TRANSITION_EDITOR_PANE, FALSE);
}

void CTransitionBrowser::OnChangeContext()
{
	SetScopeContextDataIdx(m_cbContext.GetCurSel());
}

void CTransitionBrowser::SetScopeContextDataIdx(int scopeContextDataIdx)
{
	const bool rangeValid = (scopeContextDataIdx >= 0) && (scopeContextDataIdx < m_context->m_contextData.size());
	assert(rangeValid);

	if ((scopeContextDataIdx != m_scopeContextDataIdx) && rangeValid)
	{
		m_scopeContextDataIdx = scopeContextDataIdx;

		SetDatabase(m_context->m_contextData[scopeContextDataIdx].database);

		m_cbContext.SetCurSel(scopeContextDataIdx);
	}
}

void CTransitionBrowser::OnChangeFilterTagsFrom()
{
	CString filter;
	m_editFilterTagsFrom.GetWindowText(filter);

	if (filter[0] != TRANSITION_DEFAULT_TAG_FILTER_TEXT[0] && filter != m_filterTextFrom)
	{
		m_filterTextFrom = filter;
		UpdateTags(m_filtersFrom, m_filterTextFrom);

		Refresh();

		m_editFilterTagsFrom.SetFocus();
	}

	if (filter.GetLength() == 0)
	{
		m_editFilterTagsFrom.SetWindowText(TRANSITION_DEFAULT_TAG_FILTER_TEXT);
		m_editFilterTagsFrom.SetSel(0, TRANSITION_DEFAULT_TAG_FILTER_TEXT.GetLength());
	}
}

void CTransitionBrowser::OnChangeFilterFragmentsFrom()
{
	CString filter;
	m_editFilterFragmentIDsFrom.GetWindowText(filter);

	if (filter[0] != TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT[0] && filter != m_filterFragmentIDTextFrom)
	{
		m_filterFragmentIDTextFrom = filter.MakeLower();

		Refresh();

		m_editFilterFragmentIDsFrom.SetFocus();
	}

	if (filter.GetLength() == 0)
	{
		m_editFilterFragmentIDsFrom.SetWindowText(TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT);
		m_editFilterFragmentIDsFrom.SetSel(0, TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT.GetLength());
	}
}

void CTransitionBrowser::OnChangeFilterAnimClipFrom()
{
	CString filter;
	m_editFilterAnimClipFrom.GetWindowText(filter);

	if (filter[0] != TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT[0] && filter != m_filterAnimClipFrom)
	{
		m_filterAnimClipFrom = filter.MakeLower();

		Refresh();
		m_editFilterAnimClipFrom.SetFocus();
	}

	if (filter.GetLength() == 0)
	{
		m_editFilterAnimClipFrom.SetWindowText(TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT);
		m_editFilterAnimClipFrom.SetSel(0, TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT.GetLength());
	}
}

void CTransitionBrowser::OnChangeFilterTagsTo()
{
	CString filter;
	m_editFilterTagsTo.GetWindowText(filter);

	if (filter[0] != TRANSITION_DEFAULT_TAG_FILTER_TEXT[0] && filter != m_filterTextTo)
	{
		m_filterTextTo = filter;
		UpdateTags(m_filtersTo, m_filterTextTo);

		Refresh();

		m_editFilterTagsTo.SetFocus();
	}

	if (filter.GetLength() == 0)
	{
		m_editFilterTagsTo.SetWindowText(TRANSITION_DEFAULT_TAG_FILTER_TEXT);
		m_editFilterTagsTo.SetSel(0, TRANSITION_DEFAULT_TAG_FILTER_TEXT.GetLength());
	}
}

void CTransitionBrowser::OnChangeFilterFragmentsTo()
{
	CString filter;
	m_editFilterFragmentIDsTo.GetWindowText(filter);

	if (filter[0] != TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT[0] && filter != m_filterFragmentIDTextTo)
	{
		m_filterFragmentIDTextTo = filter.MakeLower();

		Refresh();

		m_editFilterFragmentIDsTo.SetFocus();
	}

	if (filter.GetLength() == 0)
	{
		m_editFilterFragmentIDsTo.SetWindowText(TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT);
		m_editFilterFragmentIDsTo.SetSel(0, TRANSITION_DEFAULT_FRAGMENT_ID_FILTER_TEXT.GetLength());
	}
}

void CTransitionBrowser::OnChangeFilterAnimClipTo()
{
	CString filter;
	m_editFilterAnimClipTo.GetWindowText(filter);

	if (filter[0] != TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT[0] && filter != m_filterAnimClipTo)
	{
		m_filterAnimClipTo = filter.MakeLower();

		Refresh();
		m_editFilterAnimClipTo.SetFocus();
	}

	if (filter.GetLength() == 0)
	{
		m_editFilterAnimClipTo.SetWindowText(TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT);
		m_editFilterAnimClipTo.SetSel(0, TRANSITION_DEFAULT_ANIM_CLIP_FILTER_TEXT.GetLength());
	}
}

bool CTransitionBrowser::DeleteTransition(const TTransitionID& transitionToDelete)
{
	bool hadDeletedTransitionOpen = false;
	if (HasOpenedRecord())
	{
		CTransitionBrowserRecord::SRecordData& openedRecordData = m_openedRecordDataCollection[0]; // first recorddata is the opened transition (the others are the folders)
		assert(!openedRecordData.m_bFolder);

		if (transitionToDelete == openedRecordData.m_transitionID)
		{
			// the currently opened transitionID has been deleted, close it
			hadDeletedTransitionOpen = true;
			SetOpenedRecord(NULL, TTransitionID());
		}
	}

	MannUtils::GetMannequinEditorManager().DeleteBlend(m_animDB, transitionToDelete.fromID, transitionToDelete.toID, transitionToDelete.fromFTS, transitionToDelete.toFTS, transitionToDelete.uid);

	return hadDeletedTransitionOpen;
}

void CTransitionBrowser::DeleteSelected()
{
	TTransitionID transitionID;
	if (GetSelectedTransition(transitionID))
	{
		const int result = MessageBox("Are you sure you want to delete the selected transition?", "Delete Transition", MB_YESNO | MB_ICONQUESTION);
		if (IDYES != result)
			return;

		DeleteTransition(transitionID);

		Refresh();
	}
}

void CTransitionBrowser::OnEditBtn()
{
	EditSelected();
}

void CTransitionBrowser::OnCopyBtn()
{
	CopySelected();
}

void CTransitionBrowser::OnOpenBtn()
{
	OpenSelected();
}

void CTransitionBrowser::OnDeleteBtn()
{
	DeleteSelected();
}

void CTransitionBrowser::OnNewBtn()
{
	TTransitionID selectedTransitionID;
	SFragmentBlend blendToAdd;
	bool hasTransitionSelected = GetSelectedTransition(selectedTransitionID);

	TTransitionID transitionID(selectedTransitionID);
	if (ShowTransitionSettingsDialog(transitionID, "New Transition"))
	{
		if (hasTransitionSelected && transitionID.IsSameBlendVariant(selectedTransitionID))
		{
			// "New" clones an existing blend, but only if we didn't change the variant
			const SFragmentBlend* pCurBlend = m_animDB->GetBlend(transitionID.fromID, transitionID.toID, transitionID.fromFTS, transitionID.toFTS, transitionID.uid);
			const SFragmentBlendUid newUid = blendToAdd.uid;
			blendToAdd = *pCurBlend;
			blendToAdd.uid = newUid;
		}
		else
		{
			static CFragment fragmentNew;
			blendToAdd.pFragment = &fragmentNew; // we need a valid CFragment because AddBlend below copies the contents of pFragment
		}

		transitionID.uid = MannUtils::GetMannequinEditorManager().AddBlend(m_animDB, transitionID.fromID, transitionID.toID, transitionID.fromFTS, transitionID.toFTS, blendToAdd);
		Refresh();
		SelectAndOpenRecord(transitionID);
	}
}

void CTransitionBrowser::OnSwapFragmentFilters()
{
	CString fragTo = m_filterFragmentIDTextTo;
	CString fragFrom = m_filterFragmentIDTextFrom;

	m_filterFragmentIDTextFrom = fragTo;
	m_filterFragmentIDTextTo = fragFrom;

	UpdateFilters();
}

void CTransitionBrowser::OnSwapTagFilters()
{
	CString tagTo = m_filterTextTo;
	CString tagFrom = m_filterTextFrom;

	m_filterTextFrom = tagTo;
	m_filterTextTo = tagFrom;

	UpdateFilters();
}

void CTransitionBrowser::OnSwapAnimClipFilters()
{
	std::swap(m_filterAnimClipTo, m_filterAnimClipFrom);

	UpdateFilters();
}

void CTransitionBrowser::SelectAndOpenRecord(const TTransitionID& transitionID)
{
	m_treeCtrl.SelectItem(transitionID);
	OpenSelectedTransition(transitionID);
}

CTransitionBrowserRecord* CTransitionBrowser::GetSelectedRecord()
{
	CTransitionBrowserRecord* pRecord = nullptr;
	CXTPReportRow* pRow = m_treeCtrl.GetFocusedRow();
	if (pRow)
	{
		pRecord = m_treeCtrl.RowToRecord(pRow);
	}
	return pRecord;
}

bool CTransitionBrowser::GetSelectedTransition(TTransitionID& outTransitionID)
{
	const CTransitionBrowserRecord* pRecord = GetSelectedRecord();

	if (pRecord && !pRecord->IsFolder() && m_treeCtrl.GetSelectedCount() <= 1)
	{
		outTransitionID = pRecord->GetTransitionID();
		return true;
	}
	return false;
}

void CTransitionBrowser::OpenSelected()
{
	TTransitionID transitionID;
	if (GetSelectedTransition(transitionID))
	{
		OpenSelectedTransition(transitionID);
	}
}

void CTransitionBrowser::OpenSelectedTransition(const TTransitionID& transitionID)
{
	// if it has no FragmentIDs, or is missing one of them, then launch the FragmentIDPicker
	// or if it has no tags, or is missing one set of them, then it needs to launch the TagStatePicker
	FragmentID lclFromID = transitionID.fromID;
	FragmentID lclToID = transitionID.toID;
	SFragTagState fromTags = transitionID.fromFTS;
	SFragTagState toTags = transitionID.toFTS;

	// Only show a dialog when necessary to fill out incomplete information
	if (lclFromID == TAG_ID_INVALID || lclToID == TAG_ID_INVALID)
	{
		CMannTransitionPickerDlg transitionInfoDlg(lclFromID, lclToID, fromTags, toTags);
		// on return it *should* be ok to call LoadSequence with the modified parameters the user has picked.
		if (IDOK != transitionInfoDlg.DoModal())
		{
			// abort loading
			return;
		}
	}

	SetOpenedRecord(GetSelectedRecord(), TTransitionID(lclFromID, lclToID, fromTags, toTags, transitionID.uid));
}

bool CTransitionBrowser::ShowTransitionSettingsDialog(TTransitionID& inoutTransitionID, const CString& windowCaption)
{
	const SControllerDef* pControllerDef = CMannequinDialog::GetCurrentInstance()->Contexts()->m_controllerDef;
	if (!pControllerDef)
	{
		MessageBox(_T("No preview file loaded."), _T("Error"), MB_ICONERROR | MB_OK);
		return false;
	}

	CMannTransitionSettingsDlg transitionInfoDlg(windowCaption, inoutTransitionID.fromID, inoutTransitionID.toID, inoutTransitionID.fromFTS, inoutTransitionID.toFTS);
	// on return it *should* be ok to call LoadSequence with the modified parameters the user has picked.
	
	transitionInfoDlg.BringWindowToTop();
	if (IDOK != transitionInfoDlg.DoModal())
	{
		// abort loading
		return false;
	}
	return true;
}

void CTransitionBrowser::EditSelected()
{
	TTransitionID transitionID;
	if (GetSelectedTransition(transitionID))
	{
		const TTransitionID oldTransitionID(transitionID);
		if (ShowTransitionSettingsDialog(transitionID, "Edit Transition"))
		{
			// If they've selected the same variant, then drop out
			if (oldTransitionID.IsSameBlendVariant(transitionID))
			{
				return;
			}

			// Get the old blend
			SFragmentBlend blendToMove;
			MannUtils::GetMannequinEditorManager().GetFragmentBlend(m_animDB, oldTransitionID.fromID, oldTransitionID.toID, oldTransitionID.fromFTS, oldTransitionID.toFTS, oldTransitionID.uid, blendToMove);

			// And add it as a new one
			transitionID.uid = MannUtils::GetMannequinEditorManager().AddBlend(m_animDB, transitionID.fromID, transitionID.toID, transitionID.fromFTS, transitionID.toFTS, blendToMove);

			// remove the old blend
			bool editedTransitionWasOpened = DeleteTransition(oldTransitionID);

			Refresh();
			m_treeCtrl.SelectItem(transitionID);

			if (editedTransitionWasOpened)
				OpenSelectedTransition(transitionID);
		}
	}
}

void CTransitionBrowser::CopySelected()
{
	TTransitionID transitionID;
	if (GetSelectedTransition(transitionID))
	{
		const TTransitionID oldTransitionID(transitionID);
		if (ShowTransitionSettingsDialog(transitionID, "Copy Transition"))
		{
			// Get the old blend
			SFragmentBlend blendToMove;
			MannUtils::GetMannequinEditorManager().GetFragmentBlend(m_animDB, oldTransitionID.fromID, oldTransitionID.toID, oldTransitionID.fromFTS, oldTransitionID.toFTS, oldTransitionID.uid, blendToMove);

			// And add it as a new one
			blendToMove.uid = SFragmentBlendUid(SFragmentBlendUid::NEW);
			transitionID.uid = MannUtils::GetMannequinEditorManager().AddBlend(m_animDB, transitionID.fromID, transitionID.toID, transitionID.fromFTS, transitionID.toFTS, blendToMove);

			Refresh();
			OpenSelectedTransition(transitionID);
		}
	}
}

void CTransitionBrowser::FindFragmentReferences(CString fragSearchStr)
{
	fragSearchStr = CryStringUtils::toLower(fragSearchStr.GetString());

	m_filterFragmentIDTextFrom = fragSearchStr;
	m_filterFragmentIDTextTo = "";
	m_filterTextFrom = "";
	m_filterTextTo = "";
	m_filterAnimClipFrom = "";
	m_filterAnimClipTo = "";

	UpdateFilters();
}

void CTransitionBrowser::FindTagReferences(CString tagSearchStr)
{
	tagSearchStr = CryStringUtils::toLower(tagSearchStr.GetString());

	m_filterFragmentIDTextFrom = "";
	m_filterFragmentIDTextTo = "";
	m_filterTextFrom = tagSearchStr;
	m_filterTextTo = "";
	m_filterAnimClipFrom = "";
	m_filterAnimClipTo = "";

	UpdateFilters();
}

void CTransitionBrowser::FindClipReferences(CString animSearchStr)
{
	animSearchStr = CryStringUtils::toLower(animSearchStr.GetString());

	m_filterFragmentIDTextFrom = "";
	m_filterFragmentIDTextTo = "";
	m_filterTextFrom = "";
	m_filterTextTo = "";
	m_filterAnimClipFrom = animSearchStr;
	m_filterAnimClipTo = "";

	UpdateFilters();
}

void CTransitionBrowser::UpdateTags(std::vector<CString>& tags, CString tagString)
{
	int position = 0;
	CString token;

	tags.clear();
	token = tagString.Tokenize(" ", position);
	while (!token.IsEmpty())
	{
		tags.push_back(token.MakeLower());
		token = tagString.Tokenize(" ", position);
	}
}

void CTransitionBrowser::UpdateFilters()
{
	m_editFilterTagsFrom.SetWindowText(m_filterTextFrom);
	m_editFilterFragmentIDsFrom.SetWindowText(m_filterFragmentIDTextFrom);
	m_editFilterAnimClipFrom.SetWindowText(m_filterAnimClipFrom);
	m_editFilterTagsTo.SetWindowText(m_filterTextTo);
	m_editFilterFragmentIDsTo.SetWindowText(m_filterFragmentIDTextTo);
	m_editFilterAnimClipTo.SetWindowText(m_filterAnimClipTo);

	UpdateTags(m_filtersFrom, m_filterTextFrom);
	UpdateTags(m_filtersTo, m_filterTextTo);
	Refresh();
}

void CTransitionBrowser::UpdateButtonStatus()
{
	const CTransitionBrowserRecord* pRec = GetSelectedRecord();
	if (pRec && !pRec->IsFolder())
	{
		m_editButton.EnableWindow(TRUE);
		m_copyButton.EnableWindow(TRUE);
		m_deleteButton.EnableWindow(TRUE);
		m_openButton.EnableWindow(TRUE);
	}
	else
	{
		m_editButton.EnableWindow(FALSE);
		m_copyButton.EnableWindow(FALSE);
		m_deleteButton.EnableWindow(FALSE);
		m_openButton.EnableWindow(FALSE);
	}
}

bool CTransitionBrowser::FilterByAnimClip(const FragmentID fragId, const SFragTagState& tagState, const CString& filter)
{
	const uint32 numTagSets = m_animDB->GetTotalTagSets(fragId);

	for (uint32 tagId = 0; tagId < numTagSets; ++tagId)
	{
		SFragTagState tagSetState;
		uint32 numOptions = m_animDB->GetTagSetInfo(fragId, tagId, tagSetState);

		if (tagSetState != tagState)
			continue;

		for (uint32 optionId = 0; optionId < numOptions; ++optionId)
		{
			const CFragment* sourceFragment = m_animDB->GetEntry(fragId, tagId, optionId);

			if (sourceFragment)
			{
				uint32 numAnimLayers = sourceFragment->m_animLayers.size();
				for (uint32 layerId = 0; layerId < numAnimLayers; ++layerId)
				{
					const TAnimClipSequence& animLayer = sourceFragment->m_animLayers[layerId];
					const uint32 numClips = animLayer.size();
					for (uint32 clipId = 0; clipId < numClips; ++clipId)
					{
						const SAnimClip& animClip = animLayer[clipId];
						if (strstr(animClip.animation.animRef.c_str(), filter))
						{
							return TRUE;
						}
					}
				}
			}
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
// CTransitionBrowserTreeCtrl

enum ETransitionReportColumn
{
	TRANSITIONCOLUMN_FROM = 0,
	TRANSITIONCOLUMN_TO
};

//////////////////////////////////////////////////////////////////////////
CTransitionBrowserTreeCtrl::CTransitionBrowserTreeCtrl(CTransitionBrowser* pTransitionBrowser)
	: CTreeCtrlReport()
	, m_pTransitionBrowser(pTransitionBrowser)
{
	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_MANN_TAGDEF_TREE, 16, RGB(255, 0, 255));

	AllowEdit(FALSE);
	EditOnClick(FALSE);
	FocusSubItems(FALSE);
	SetMultipleSelection(FALSE);
	SetSortRecordChilds(FALSE);
	SetImageList(&m_imageList);

	CXTPReportColumn* pCol1 = AddColumn(new CXTPReportColumn(TRANSITIONCOLUMN_FROM, _T("From"), 80, false, -1, true, true));
	pCol1->SetSortable(FALSE);
	pCol1->SetTreeColumn(TRUE);
	pCol1->SetAutoSize(TRUE);
	pCol1->SetCaption(_T("From:"));

	CXTPReportColumn* pCol2 = AddColumn(new CXTPReportColumn(TRANSITIONCOLUMN_TO, _T("To"), 80));
	pCol2->SetSortable(FALSE);
	pCol2->SetTreeColumn(FALSE);
	pCol2->SetAutoSize(TRUE);
	pCol2->SetCaption(_T("To:"));

	SetExpandOnDblClick(true);
	SetHeaderVisible(true);
}

void CTransitionBrowserTreeCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_RETURN)
	{
		CXTPReportRow* pFocusedRow = GetFocusedRow();
		CRecord* pRecord = RowToRecord(pFocusedRow);
		if (pRecord && m_pTransitionBrowser)
		{
			if (pRecord->IsFolder())
			{
				pFocusedRow->SetExpanded(!pFocusedRow->IsExpanded());
			}
			else
			{
				m_pTransitionBrowser->SelectAndOpenRecord(pRecord->GetTransitionID());
			}
		}
	}
	else
	{
		__super::OnKeyDown(nChar, nRepCnt, nFlags);
	}
}

/*
   // Was useful for debugging
   //////////////////////////////////////////////////////////////////////////
   void CTransitionBrowserTreeCtrl::PrintRows(const char* szWhen)
   {
   CryWarning(
    VALIDATOR_MODULE_ANIMATION,
    VALIDATOR_WARNING,
    "\n--%s---\n", szWhen);

   CXTPReportRows *pRows = GetRows();
   for (int i = 0,nNumRows = pRows->GetCount(); i < nNumRows; i++)
   {
    CXTPReportRow *pRow = pRows->GetAt(i);
    CTransitionBrowserRecord* pCurrentRecord = RowToRecord(pRow);

    CryWarning(
      VALIDATOR_MODULE_ANIMATION,
      VALIDATOR_WARNING,
      "Row %d, from=%s  to=%s  expanded=%d\n",
      i,
      pCurrentRecord ? pCurrentRecord->GetRecordData().m_from : "<NULL>",
      pCurrentRecord ? pCurrentRecord->GetRecordData().m_to : "<NULL>",
      pRow->IsExpanded() ? 1 : 0
      );
   }

   CryWarning(
    VALIDATOR_MODULE_ANIMATION,
    VALIDATOR_WARNING,
    "-----\n");
   }*/

//////////////////////////////////////////////////////////////////////////
void CTransitionBrowserTreeCtrl::ExpandRowsForRecordDataCollection(const TTransitionBrowserRecordDataCollection& expandedRecordDataCollection)
{
	CXTPReportRows* pRows = GetRows();
	for (int i = 0; i < pRows->GetCount(); i++) // GetCount changes whenever a row gets expanded!
	{
		CXTPReportRow* pRow = pRows->GetAt(i);
		CTransitionBrowserRecord* pCurrentRecord = RowToRecord(pRow);

		if ( // Incredibly slow! Use map for example to optimize this
		  std::find(expandedRecordDataCollection.begin(), expandedRecordDataCollection.end(), pCurrentRecord->GetRecordData()) != expandedRecordDataCollection.end())
		{
			pRow->SetExpanded(TRUE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionBrowserTreeCtrl::SelectAndFocusRecordDataCollection(const CTransitionBrowserRecord::SRecordData* pFocusedRecordData, const TTransitionBrowserRecordDataCollection* pMarkedRecordDataCollection)
{
	GetSelectedRows()->Clear();

	CTransitionBrowserRecord* pFocusedRecord = NULL;
	if (pFocusedRecordData || pMarkedRecordDataCollection)
	{
		CXTPReportRows* pRows = GetRows();
		for (int i = 0, nNumRows = pRows->GetCount(); i < nNumRows; i++)
		{
			CXTPReportRow* pRow = pRows->GetAt(i);
			CTransitionBrowserRecord* pCurrentRecord = RowToRecord(pRow);
			if (pFocusedRecordData && pCurrentRecord && pCurrentRecord->IsEqualTo(*pFocusedRecordData))
			{
				pFocusedRecord = pCurrentRecord;
				pRow->SetSelected(TRUE);
				SetFocusedRow(pRow);
			}
			else if (pMarkedRecordDataCollection && // Incredibly slow! Use map for example to optimize this
			         std::find(pMarkedRecordDataCollection->begin(), pMarkedRecordDataCollection->end(), pCurrentRecord->GetRecordData()) != pMarkedRecordDataCollection->end())
			{
				pRow->SetSelected(TRUE);
			}
		}
	}

	if (pFocusedRecord)
	{
		EnsureItemVisible(pFocusedRecord);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionBrowserTreeCtrl::SelectItem(CTransitionBrowserRecord* pRecord)
{
	EnsureItemVisible(pRecord);

	GetSelectedRows()->Clear();
	if (pRecord)
	{
		CXTPReportRows* pRows = GetRows();
		for (int i = 0, nNumRows = pRows->GetCount(); i < nNumRows; i++)
		{
			CXTPReportRow* pRow = pRows->GetAt(i);
			CTransitionBrowserRecord* pCurrentRecord = RowToRecord(pRow);
			if (pCurrentRecord == pRecord)
			{
				pRow->SetSelected(TRUE);
				SetFocusedRow(pRow);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTransitionBrowserTreeCtrl::SelectItem(const TTransitionID& transitionID)
{
	CRecord* pRecord = NULL;

	GetSelectedRows()->Clear();

	CXTPReportRecords* pRecords = GetRecords();
	for (int i = 0, nNumRecords = pRecords->GetCount(); (i < nNumRecords) && (pRecord == NULL); ++i)
	{
		CXTPReportRecord* pReportRecord = pRecords->GetAt(i);

		if (pReportRecord->IsKindOf(RUNTIME_CLASS(CRecord)))
		{
			pRecord = SearchRecordForTransition((CRecord*)pReportRecord, transitionID);
		}
	}

	if (pRecord != NULL)
	{
		EnsureItemVisible(pRecord);
		SelectItem(pRecord);
	}
}

CTransitionBrowserTreeCtrl::CRecord* CTransitionBrowserTreeCtrl::FindRecordForTransition(const TTransitionID& transitionID)
{
	CRecord* pRecord = NULL;

	CXTPReportRecords* pRecords = GetRecords();
	for (int i = 0, nNumRecords = pRecords->GetCount(); (i < nNumRecords) && (pRecord == NULL); ++i)
	{
		CXTPReportRecord* pReportRecord = pRecords->GetAt(i);

		if (pReportRecord->IsKindOf(RUNTIME_CLASS(CRecord)))
		{
			pRecord = SearchRecordForTransition((CRecord*)pReportRecord, transitionID);
		}
	}

	return pRecord;
}

CTransitionBrowserTreeCtrl::CRecord* CTransitionBrowserTreeCtrl::SearchRecordForTransition(CRecord* pRecord, const TTransitionID& transitionID)
{
	CRecord* pFound = NULL;

	if (pRecord->IsEqualTo(transitionID))
	{
		pFound = pRecord;
	}
	else
	{
		if (pRecord->HasChildren())
		{
			for (int i = 0, numChildren = pRecord->GetChildCount(); (i < numChildren) && (pFound == NULL); ++i)
			{
				CXTPReportRecord* pReportRecord = pRecord->GetChild(i);
				if (pReportRecord->IsKindOf(RUNTIME_CLASS(CRecord)))
				{
					pFound = SearchRecordForTransition((CRecord*)pReportRecord, transitionID);
				}
			}
		}
	}

	return pFound;
}

void CTransitionBrowserTreeCtrl::SavePresentationState(CTransitionBrowserTreeCtrl::SPresentationState& outState) const
{
	outState = CTransitionBrowserTreeCtrl::SPresentationState();

	{
		CXTPReportRows* pRows = GetRows();
		for (int i = 0, nNumRows = pRows->GetCount(); i < nNumRows; i++)
		{
			const CXTPReportRow* pRow = pRows->GetAt(i);
			if (pRow->IsExpanded())
			{
				const CTransitionBrowserRecord* pRecord = RowToRecord(pRow);
				if (pRecord)
				{
					outState.expandedRecordDataCollection.push_back(pRecord->GetRecordData());
				}
			}
		}
	}

	{
		CXTPReportSelectedRows* pRows = GetSelectedRows();
		outState.selectedRecordDataCollection.reserve(pRows->GetCount());
		for (int i = 0, nNumRows = pRows->GetCount(); i < nNumRows; i++)
		{
			const CXTPReportRow* pRow = pRows->GetAt(i);
			const CTransitionBrowserRecord* pRecord = RowToRecord(pRow);
			if (pRecord)
			{
				outState.selectedRecordDataCollection.push_back(pRecord->GetRecordData());
			}
		}
	}

	const CTransitionBrowserRecord* pFocusedRecord = RowToRecord(GetFocusedRow());
	if (pFocusedRecord)
	{
		outState.pFocusedRecordData.reset(new CTransitionBrowserRecord::SRecordData(pFocusedRecord->GetRecordData()));
	}
}

void CTransitionBrowserTreeCtrl::RestorePresentationState(const CTransitionBrowserTreeCtrl::SPresentationState& state)
{
	ExpandRowsForRecordDataCollection(state.expandedRecordDataCollection);
	SelectAndFocusRecordDataCollection(state.pFocusedRecordData.get(), &state.selectedRecordDataCollection);
}

//////////////////////////////////////////////////////////////////////////
//
CTransitionBrowserRecord::SRecordData::SRecordData(const CString& from, const CString& to)
	: m_from(from)
	, m_to(to)
	, m_bFolder(true)
{
}

CTransitionBrowserRecord::SRecordData::SRecordData(const TTransitionID& transitionID, float selectTime)
	: m_to("")
	, m_bFolder(false)
	, m_transitionID(transitionID)
{
	m_from.Format("Select @ %5.2f", selectTime);
}

//////////////////////////////////////////////////////////////////////////
// CTransitionBrowserRecord
IMPLEMENT_DYNAMIC(CTransitionBrowserRecord, CTreeItemRecord)

CTransitionBrowserRecord::CTransitionBrowserRecord(const CString& from, const CString& to)
	: CTreeItemRecord()
	, m_data(from, to)
{
	CreateItems();
}

CTransitionBrowserRecord::CTransitionBrowserRecord(const TTransitionID& transitionID, float selectTime)
	: CTreeItemRecord()
	, m_data(transitionID, selectTime)
{
	CreateItems();
}

const TTransitionID& CTransitionBrowserRecord::GetTransitionID() const
{
	assert(!m_data.m_bFolder);
	return m_data.m_transitionID;
}

bool CTransitionBrowserRecord::IsEqualTo(const TTransitionID& transitionID) const
{
	return m_data.m_bFolder ? false : (m_data.m_transitionID == transitionID);
}

bool CTransitionBrowserRecord::IsEqualTo(const SRecordData& recordData) const
{
	return (m_data == recordData);
}

//////////////////////////////////////////////////////////////////////////
void CTransitionBrowserRecord::CreateItems()
{
	const int IMAGE_FOLDER = 0;
	const int IMAGE_ITEM = 1;

	CXTPReportRecordItem* pFrom = new CXTPReportRecordItemText(m_data.m_from);
	CXTPReportRecordItem* pTo = new CXTPReportRecordItemText(m_data.m_to);
	AddItem(pFrom);
	AddItem(pTo);

	const int nIcon = m_data.m_bFolder ? IMAGE_FOLDER : IMAGE_ITEM;
	SetIcon(nIcon);

	SetGroup(m_data.m_bFolder);
}

