// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequencerNode.h"
#include "SequencerTrack.h"
#include "MannequinNodes.h"
#include "MannequinDialog.h"
#include "MannequinUtil.h"

#include "SequencerSequence.h"
#include "SequencerDopeSheet.h"
#include "SequencerUndo.h"
#include "Dialogs/ToolbarDialog.h"

#include "Util/Clipboard.h"

#include "ISequencerSystem.h"

#include "Objects\EntityObject.h"
#include "ViewManager.h"
#include "RenderViewport.h"
#include "Util/MFCUtil.h"
#include "SequencerTrack.h"
#include "SequencerNode.h"

#include "MannequinDialog.h"
#include "MannPreferences.h"
#include "FragmentTrack.h"
#include "Controls/QuestionDialog.h"

#define EDIT_DISABLE_GRAY_COLOR RGB(180, 180, 180)

namespace
{
enum EMenuItem
{
	eMenuItem_CopySelectedKeys = 1,
	eMenuItem_CopyKeys,
	eMenuItem_PasteKeys,
	eMenuItem_CopyLayer,
	eMenuItem_PasteLayer,
	eMenuItem_RemoveLayer,
	eMenuItem_MuteNode,
	eMenuItem_MuteAll,
	eMenuItem_UnmuteAll,
	eMenuItem_SoloNode,
	eMenuItem_AddLayer_First = 100,   // Begins range Add Layer
	eMenuItem_AddLayer_Last  = 120,   // Ends range Add Layer
	eMenuItem_ShowHide_First = 200,   // Begins range Show/Hide Track
	eMenuItem_ShowHide_Last  = 220    // Ends range Show/Hide Track
};
}

const int kIconFromParamID[SEQUENCER_PARAM_TOTAL] = { 0, 0, 14, 3, 10, 13 };
const int kIconFromNodeType[SEQUENCER_NODE_TOTAL] = { 1, 1, 1, 1, 1, 2 };

class CMannNodesCtrlDropTarget : public COleDropTarget
{
public:

	CMannNodesCtrlDropTarget(CMannNodesCtrl* dlg)
		: m_pDlg(dlg)
	{}

	virtual DROPEFFECT OnDragEnter(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		// Check for dropping an animation into the Fragment editor in a context
		if (nullptr != m_pDlg->IsPointValidForAnimationInContextDrop(point, pDataObject))
		{
			return DROPEFFECT_MOVE;
		}

		return DROPEFFECT_NONE;
	}

	virtual DROPEFFECT OnDragOver(CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
	{
		// Do the same as the enter
		return OnDragEnter(pWnd, pDataObject, dwKeyState, point);
	}

	virtual BOOL OnDrop(CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
	{
		// Check for dropping an animation into the Fragment editor in an animation layer
		CMannNodesCtrl::SItemInfo* pItemInfo = m_pDlg->IsPointValidForAnimationInContextDrop(point, pDataObject);
		if (nullptr == pItemInfo)
		{
			return false;
		}
		return m_pDlg->CreatePointForAnimationInContextDrop(pItemInfo, point, pDataObject);
	}

private:

	CMannNodesCtrl* m_pDlg;
};

BEGIN_MESSAGE_MAP(CMannNodesCtrl, CTreeCtrlReport)
ON_NOTIFY_REFLECT(NM_CLICK, OnNMLclick)
ON_NOTIFY_REFLECT(NM_RCLICK, OnNMRclick)
ON_WM_SIZE()
ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CMannNodesCtrl::CMannNodesCtrl()
	: CTreeCtrlReport()
	, m_pDropTarget(nullptr)
{
	m_sequence = 0;
	m_keysCtrl = 0;

	CMFCUtils::LoadTrueColorImageList(m_imageList, IDB_SEQUENCER_NODES, 16, RGB(255, 0, 255));
	SetImageList(&m_imageList);

	CXTPReportColumn* pNodeCol
	  = AddColumn(new CXTPReportColumn(eCOLUMN_NODE, _T("Node"), 150, TRUE, XTP_REPORT_NOICON, FALSE, TRUE));
	pNodeCol->SetTreeColumn(true);
	pNodeCol->SetSortable(FALSE);

	CXTPReportColumn* pMuteCol = AddColumn(new CXTPReportColumn(eCOLUMN_MUTE, _T("Mute"), 20, TRUE, XTP_REPORT_NOICON, FALSE, TRUE));
	pMuteCol->SetTreeColumn(false);
	pMuteCol->SetSortable(FALSE);
	pMuteCol->SetHeaderAlignment(DT_RIGHT);
	pMuteCol->SetAlignment(DT_RIGHT);
	pMuteCol->SetAutoSize(FALSE);
	pMuteCol->EnableResize(FALSE);

	ShowHeader(FALSE);

	SetMultipleSelection(TRUE);

	SetGroupRowsBold(TRUE);
	//SetTreeIndent(30);

	GetReportHeader()->AllowColumnRemove(FALSE);
	GetReportHeader()->AllowColumnSort(FALSE);

	AllowEdit(FALSE);

	m_bEditLock = false;
};

//////////////////////////////////////////////////////////////////////////
CMannNodesCtrl::~CMannNodesCtrl()
{
	if (nullptr != m_pDropTarget)
	{
		delete m_pDropTarget;
	}
};

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::SetSequence(CSequencerSequence* seq)
{
	const int topRowIndex = (m_sequence == seq) ? GetTopRowIndex() : 0;

	int64 time1 = gEnv->pTimer->GetAsyncTime().GetValue();
	m_itemInfos.clear();
	int64 time2 = gEnv->pTimer->GetAsyncTime().GetValue();
	DeleteAllItems();
	int64 time3 = gEnv->pTimer->GetAsyncTime().GetValue();
	m_sequence = seq;
	int64 time4 = gEnv->pTimer->GetAsyncTime().GetValue();

	Reload();
	int64 time5 = gEnv->pTimer->GetAsyncTime().GetValue();
	m_keysCtrl->Invalidate();
	int64 time6 = gEnv->pTimer->GetAsyncTime().GetValue();

	if (nullptr == m_pDropTarget)
	{
		m_pDropTarget = new CMannNodesCtrlDropTarget(this);
		m_pDropTarget->Register(this);
	}

	//CryLog("CMannNodesCtrl::SetSequence %" PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64 , time1, time2, time3, time4, time5, time6);

	SetTopRow(topRowIndex);
	SyncKeyCtrl();
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::Reload()
{
	const float fScrollPos = SaveVerticalScrollPos();

	int64 time1 = gEnv->pTimer->GetAsyncTime().GetValue();
	__super::Reload();
	int64 time2 = gEnv->pTimer->GetAsyncTime().GetValue();

	RestoreVerticalScrollPos(fScrollPos);

	SyncKeyCtrl();
	int64 time3 = gEnv->pTimer->GetAsyncTime().GetValue();

	//CryLog("CMannNodesCtrl::Reload %" PRIi64 " %" PRIi64 " %" PRIi64 , time1, time2, time3);
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::SetKeyListCtrl(CSequencerDopeSheetBase* keysCtrl)
{
	m_keysCtrl = keysCtrl;
	//SyncKeyCtrl();
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::FillTracks(CRecord* pNodeRecord, CSequencerNode* node)
{
	CSequencerNode::SParamInfo paramInfo;
	for (int i = 0; i < node->GetTrackCount(); i++)
	{
		CSequencerTrack* track = node->GetTrackByIndex(i);
		if (track->GetFlags() & CSequencerTrack::SEQUENCER_TRACK_HIDDEN)
			continue;

		const ESequencerParamType type = track->GetParameterType();

		if (!node->GetParamInfoFromId(type, paramInfo))
			continue;

		int nImage = GetIconIndexForParam(type);
		CRecord* pTrackRecord = new CRecord(false, paramInfo.name);
		pTrackRecord->SetItemHeight(gMannequinPreferences.trackSize + 2);

		CXTPReportRecordItem* pTrackItem = new CXTPReportRecordItemText(paramInfo.name);
		pTrackRecord->AddItem(pTrackItem);
		pTrackItem->SetIconIndex(nImage);
		pTrackItem->SetTextColor(::GetSysColor(COLOR_HIGHLIGHT));

		pTrackRecord->node = node;
		pTrackRecord->track = track;
		pTrackRecord->paramId = type;

		if (track)
		{
			if ((type == SEQUENCER_PARAM_ANIMLAYER) || (type == SEQUENCER_PARAM_PROCLAYER))
			{
				pTrackItem = pTrackRecord->AddItem(new CXTPReportRecordItemIcon());
				int nMuteIcon = GetIconIndexForMute(pTrackRecord);
				pTrackItem->SetIconIndex(nMuteIcon);
			}
		}

		m_itemInfos.push_back(pTrackRecord);

		pNodeRecord->GetChilds()->Add(pTrackRecord);
	}
}

//////////////////////////////////////////////////////////////////////////
int CMannNodesCtrl::GetIconIndexForParam(ESequencerParamType type) const
{
	return kIconFromParamID[type];
}

//////////////////////////////////////////////////////////////////////////
int CMannNodesCtrl::GetIconIndexForNode(ESequencerNodeType type) const
{
	return kIconFromNodeType[type];
}

//////////////////////////////////////////////////////////////////////////
int CMannNodesCtrl::GetIconIndexForMute(SItemInfo* pItem) const
{
	int icon = MUTE_TRACK_ICON_UNMUTED;

	if (pItem)
	{
		if (pItem->track)
		{
			icon = pItem->track->IsMuted() ? MUTE_TRACK_ICON_MUTED : MUTE_TRACK_ICON_UNMUTED;
		}
		else if (pItem->node)
		{
			icon = pItem->node->IsMuted() ? MUTE_NODE_ICON_MUTED : MUTE_NODE_ICON_UNMUTED;
		}
	}

	return icon;
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::FillNodes(CRecord* pRecord)
{
	IObjectManager* pObjMgr = GetIEditorImpl()->GetObjectManager();
	CSequencerSequence* seq = m_sequence;

	const COLORREF TEXT_COLOR_FOR_MISSING_ENTITY = RGB(255, 0, 0);
	const COLORREF BACK_COLOR_FOR_ACTIVE_DIRECTOR = RGB(192, 192, 255);
	const COLORREF BACK_COLOR_FOR_INACTIVE_DIRECTOR = RGB(224, 224, 224);

	for (int i = 0; i < seq->GetNodeCount(); i++)
	{
		CSequencerNode* node = seq->GetNode(i);

		int nNodeImage = GetIconIndexForNode(node->GetType());
		CString nodeName = node->GetName();

		CRecord* pNodeRecord = new CRecord(false, nodeName);
		pNodeRecord->SetItemHeight(16 + 2);
		CXTPReportRecordItem* pNodeItem = new CXTPReportRecordItemText(nodeName);
		pNodeRecord->AddItem(pNodeItem);
		pNodeItem->SetIconIndex(nNodeImage);
		pNodeItem->SetBold(TRUE);

		pNodeRecord->node = node;
		pNodeRecord->track = 0;
		pNodeRecord->paramId = 0;

		if (node)
		{
			pNodeItem = pNodeRecord->AddItem(new CXTPReportRecordItemIcon());
			int nMuteIcon = GetIconIndexForMute(pNodeRecord);
			pNodeItem->SetIconIndex(nMuteIcon);
		}

		m_itemInfos.push_back(pNodeRecord);

		pNodeRecord->SetExpanded(node->GetStartExpanded());

		pRecord->GetChilds()->Add(pNodeRecord);

		FillTracks(pNodeRecord, node);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::OnFillItems()
{
	m_itemInfos.clear();

	CSequencerSequence* seq = m_sequence;
	if (!seq)
		return;

	string seqName = seq->GetName();
	CRecord* pRootGroupRec = new CRecord(true, seqName.c_str());
	CXTPReportRecordItem* pRootItem = new CXTPReportRecordItemText(seqName.c_str());
	pRootItem->SetBold(TRUE);
	pRootGroupRec->SetItemHeight(24);
	pRootGroupRec->AddItem(pRootItem);
	pRootGroupRec->SetExpanded(TRUE);

	AddRecord(pRootGroupRec);

	FillNodes(pRootGroupRec);

	// Additional empty record like space for scrollbar in key control
	CRecord* pGroupRec = new CRecord();
	pGroupRec->SetItemHeight(18);
	AddRecord(pGroupRec);
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::OnItemExpanded(CXTPReportRow* pRow, bool bExpanded)
{
	SyncKeyCtrl();
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::OnSelectionChanged()
{
	m_keysCtrl->ClearSelection();

	if (!m_sequence)
		return;

	// Clear track selections.
	for (int i = 0; i < m_sequence->GetNodeCount(); i++)
	{
		CSequencerNode* node = m_sequence->GetNode(i);
		for (int t = 0; t < node->GetTrackCount(); t++)
		{
			node->GetTrackByIndex(t)->SetSelected(false);
		}
	}

	int nCount = GetSelectedRows()->GetCount();
	for (int i = 0; i < nCount; i++)
	{
		CRecord* pRecord = (CRecord*)GetSelectedRows()->GetAt(i)->GetRecord();
		if (!pRecord->track)
			continue;

		for (int i = 0; i < m_keysCtrl->GetCount(); i++)
		{
			if (pRecord->track)
				pRecord->track->SetSelected(true);
			if (m_keysCtrl->GetTrack(i) == pRecord->track)
			{
				m_keysCtrl->SelectItem(i);
				break;
			}
		}
	}

	GetIEditorImpl()->Notify(eNotify_OnUpdateSequencerKeySelection);
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::SyncKeyCtrl()
{
	if (!m_keysCtrl)
		return;

	m_keysCtrl->ResetContent();

	if (!m_sequence)
		return;

	// Forces ctrl to be drawn first.
	UpdateWindow();

	int nStartRow = 0;

	CXTPReportRows* pRows = GetRows();
	int nNumRows = pRows->GetCount();
	for (int i = GetTopRowIndex(); i < nNumRows; i++)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);
		if (!pRow->IsItemsVisible())
			break;

		CRecord* pRecord = (CRecord*)pRow->GetRecord();

		const int nItemHeight = pRecord->GetItemHeight();

		CSequencerDopeSheet::Item item;

		if (pRecord != NULL && pRecord->track != NULL && pRecord->node != NULL)
		{
			item = CSequencerDopeSheet::Item(nItemHeight, pRecord->node, pRecord->paramId, pRecord->track);
		}
		else
		{
			item = CSequencerDopeSheet::Item(nItemHeight, pRecord->node);
		}
		item.nHeight = nItemHeight;
		item.bSelected = false;
		if (pRecord->track)
		{
			item.bSelected = (pRecord->track->GetFlags() & CSequencerTrack::SEQUENCER_TRACK_SELECTED) != 0;
		}
		m_keysCtrl->AddItem(item);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::ExpandNode(CSequencerNode* node)
{
	CXTPReportRows* pRows = GetRows();
	for (int i = 0, nNumRows = pRows->GetCount(); i < nNumRows; i++)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);
		CRecord* pRecord = (CRecord*)pRow->GetRecord();
		if (pRecord && pRecord->node == node)
		{
			pRow->SetExpanded(TRUE);
			break;
		}
	}
	SyncKeyCtrl();
}

//////////////////////////////////////////////////////////////////////////
bool CMannNodesCtrl::IsNodeExpanded(CSequencerNode* node)
{
	CXTPReportRows* pRows = GetRows();

	if (!pRows)
		return FALSE;

	const int nNumRows = pRows->GetCount();
	for (int i = 0; i < nNumRows; ++i)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);

		if (!pRow)
			continue;

		CRecord* pRecord = (CRecord*)pRow->GetRecord();
		if (pRecord && pRecord->node == node)
		{
			return pRow->IsExpanded();
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::SelectNode(const char* sName)
{
	CXTPReportRows* pRows = GetRows();
	for (int i = 0, nNumRows = pRows->GetCount(); i < nNumRows; i++)
	{
		CXTPReportRow* pRow = pRows->GetAt(i);
		CRecord* pRecord = (CRecord*)pRow->GetRecord();
		pRow->SetSelected(FALSE);
		if (pRecord && pRecord->node != NULL && stricmp(pRecord->node->GetName(), sName) == 0)
		{
			pRow->SetSelected(TRUE);
			break;
		}
	}
	SyncKeyCtrl();
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::OnVerticalScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SyncKeyCtrl();
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::OnNMLclick(NMHDR* pNMHDR, LRESULT* pResult)
{
	CPoint point;

	SItemInfo* pItemInfo = 0;
	uint32 column = 0;

	if (!m_sequence)
		return;

	// Find node under mouse.
	GetCursorPos(&point);
	ScreenToClient(&point);
	// Select the item that is at the point myPoint.

	CXTPReportRow* pRow = HitTest(point);
	if (pRow)
	{
		pItemInfo = (SItemInfo*)pRow->GetRecord();

		// test which column we clicked on
		CXTPReportRecordItem* pRowItem = pRow->HitTest(point);
		for (uint32 i = 0; i < pRow->GetRecord()->GetItemCount(); ++i)
		{
			if (pRowItem == pRow->GetRecord()->GetItem(i))
			{
				column = i;
				break;
			}
		}
	}

	if (column == eCOLUMN_MUTE)
	{
		if (pItemInfo)
		{
			if (pItemInfo->track || pItemInfo->node)
			{
				ToggleMute(pItemInfo);
				RefreshTracks();
			}
		}
	}

	// processed
	*pResult = 1;
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::OnNMRclick(NMHDR* pNMHDR, LRESULT* pResult)
{
	CPoint point;

	SItemInfo* pItemInfo = 0;
	uint32 column = 0;

	if (!m_sequence)
		return;

	// Find node under mouse.
	GetCursorPos(&point);
	ScreenToClient(&point);
	// Select the item that is at the point myPoint.

	CXTPReportRow* pRow = HitTest(point);
	if (pRow)
	{
		pItemInfo = (SItemInfo*)pRow->GetRecord();

		// test which column we clicked on
		CXTPReportRecordItem* pRowItem = pRow->HitTest(point);
		for (uint32 i = 0; i < pRow->GetRecord()->GetItemCount(); ++i)
		{
			if (pRowItem == pRow->GetRecord()->GetItem(i))
			{
				column = i;
				break;
			}
		}
	}

	if (!pItemInfo || !pItemInfo->node)
		return;

	int cmd = ShowPopupMenu(point, pItemInfo, column);

	float scrollPos = SaveVerticalScrollPos();

	if (cmd >= eMenuItem_AddLayer_First && cmd < eMenuItem_AddLayer_Last)
	{
		if (pItemInfo)
		{
			CSequencerNode* node = pItemInfo->node;
			if (node)
			{
				int paramIndex = cmd - eMenuItem_AddLayer_First;
				AddTrack(paramIndex, node);
			}
		}
	}
	else if (cmd == eMenuItem_CopyLayer)
	{
		m_keysCtrl->CopyTrack();
	}
	else if (cmd == eMenuItem_PasteLayer)
	{
		if (pItemInfo && pItemInfo->node)
		{
			PasteTrack(pItemInfo->node);
		}
	}
	else if (cmd == eMenuItem_RemoveLayer)
	{
		if (pItemInfo)
		{
			CSequencerNode* node = pItemInfo->node;
			if (node)
			{
				RemoveTrack(pItemInfo);
			}
		}
	}
	else if (cmd >= eMenuItem_ShowHide_First && cmd < eMenuItem_ShowHide_Last)
	{
		if (pItemInfo)
		{
			CSequencerNode* node = pItemInfo->node;
			if (node)
			{
				ShowHideTrack(node, cmd - eMenuItem_ShowHide_First);
			}
		}
	}
	else if (cmd == eMenuItem_CopySelectedKeys)
	{
		m_keysCtrl->CopyKeys();
	}
	else if (cmd == eMenuItem_CopyKeys)
	{
		m_keysCtrl->CopyKeys(true, true, true);
	}
	else if (cmd == eMenuItem_PasteKeys)
	{
		SItemInfo* pSii = GetSelectedNode();
		if (pSii)
			m_keysCtrl->PasteKeys(pSii->node, pSii->track, 0);
		else
			m_keysCtrl->PasteKeys(0, 0, 0);
	}
	else if (cmd == eMenuItem_MuteNode)
	{
		ToggleMute(pItemInfo);
		RefreshTracks();
	}
	else if (cmd == eMenuItem_SoloNode)
	{
		MuteAllBut(pItemInfo);
		RefreshTracks();
	}
	else if (cmd == eMenuItem_MuteAll)
	{
		MuteAllNodes();
		RefreshTracks();
	}
	else if (cmd == eMenuItem_UnmuteAll)
	{
		UnmuteAllNodes();
		RefreshTracks();
	}
	else
	{
		if (pItemInfo && pItemInfo->node)
			pItemInfo->node->OnMenuOption(cmd);
	}

	if (cmd)
	{
		RestoreVerticalScrollPos(scrollPos);
		SyncKeyCtrl();
	}

	// processed
	*pResult = 1;
}

//////////////////////////////////////////////////////////////////////////
CMannNodesCtrl::CRecord* CMannNodesCtrl::GetSelectedNode()
{
	if (GetSelectedCount() == 1)
	{
		Records records;
		GetSelectedRecords(records);
		return (CRecord*)records[0];
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CMannNodesCtrl::OnBeginDragAndDrop(CXTPReportSelectedRows* pRows, CPoint pt)
{
	if (pRows->GetCount() == 0)
		return false;

	for (int i = 0; i < pRows->GetCount(); ++i)
	{
		CRecord* pRec = static_cast<CRecord*>((pRows->GetAt(i))->GetRecord());
		if (!pRec || !pRec->node)
			return false;
		if (pRec->node && pRec->track && !pRec->IsGroup())
			return false;
	}
	return __super::OnBeginDragAndDrop(pRows, pt);
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::OnDragAndDrop(CXTPReportSelectedRows* pRows, CPoint absoluteCursorPos)
{
	if (pRows->GetCount() == 0)
		return;

	CPoint p = absoluteCursorPos;
	ScreenToClient(&p);

	CRect reportArea = GetReportRectangle();
	if (reportArea.PtInRect(p))
	{
		CXTPReportRow* pTargetRow = HitTest(p);
		bool bContainTarget = pRows->Contains(pTargetRow);

		if (pTargetRow)
		{
			CRecord* pFirstRecordSrc = (CRecord*)(pRows->GetAt(0)->GetRecord());
			CRecord* pRecordTrg = (CRecord*)pTargetRow->GetRecord();
			if (pFirstRecordSrc && pRecordTrg && pFirstRecordSrc->node && pRecordTrg->node && !bContainTarget)
			{
				const char* srcFirstNodeName = pFirstRecordSrc->node->GetName();

				CAnimSequenceUndo undo(m_sequence, "Reorder Node");
				CSequencerNode* pTargetNode = pRecordTrg->node;

				for (size_t i = 0; i < pRows->GetCount(); ++i)
				{
					CRecord* pRecord = static_cast<CRecord*>(pRows->GetAt(pRows->GetCount() - i - 1)->GetRecord());
					CRect targetRect = pTargetRow->GetRect();
					CRect upperTargetRect;
					CPoint br = targetRect.BottomRight();
					br.y -= targetRect.Height() / 2;
					upperTargetRect.SetRect(targetRect.TopLeft(), br);
					bool next = upperTargetRect.PtInRect(p) == FALSE;
					m_sequence->ReorderNode(pRecord->node, pTargetNode, next);
				}

				InvalidateNodes();
				SelectNode(srcFirstNodeName);
			}
			else
			{
				if (pRecordTrg && pRecordTrg == GetRows()->GetAt(0)->GetRecord())
				{
					CAnimSequenceUndo undo(m_sequence, "Detach Anim Node from Group");
					for (size_t i = 0; i < pRows->GetCount(); ++i)
					{
						CRecord* pRecord = static_cast<CRecord*>(pRows->GetAt(pRows->GetCount() - i - 1)->GetRecord());
						m_sequence->ReorderNode(pRecord->node, NULL, false);
					}
					InvalidateNodes();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::OnItemDblClick(CXTPReportRow* pRow)
{
}

//////////////////////////////////////////////////////////////////////////
void CMannNodesCtrl::InvalidateNodes()
{
	GetIEditorImpl()->Notify(eNotify_OnUpdateSequencer);
}

//////////////////////////////////////////////////////////////////////////
bool CMannNodesCtrl::GetSelectedNodes(AnimNodes& nodes)
{
	int nCount = GetSelectedRows()->GetCount();
	for (int i = 0; i < nCount; i++)
	{
		CRecord* pRecord = (CRecord*)GetSelectedRows()->GetAt(i)->GetRecord();
		if (pRecord && pRecord->node)
			stl::push_back_unique(nodes, pRecord->node);
	}

	return !nodes.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CMannNodesCtrl::HasNode(const char* name) const
{
	if (name == NULL)
		return false;

	CSequencerNode* pNode = m_sequence->FindNodeByName(name);
	if (pNode)
		return true;
	else
		return false;
}

void CMannNodesCtrl::AddTrack(int paramIndex, CSequencerNode* node)
{
	CSequencerNode::SParamInfo paramInfo;
	if (!node->GetParamInfo(paramIndex, paramInfo))
	{
		return;
	}

	CAnimSequenceUndo undo(m_sequence, "Add Layer");

	CSequencerTrack* sequenceTrack = node->CreateTrack(paramInfo.paramId);
	if (sequenceTrack)
		sequenceTrack->OnChange();
	InvalidateNodes();
	ExpandNode(node);
}

bool CMannNodesCtrl::CanPasteTrack(CSequencerNode* node)
{
	CClipboard clip;
	if (clip.IsEmpty())
		return false;

	XmlNodeRef copyNode = clip.Get();
	if (copyNode == NULL || strcmp(copyNode->getTag(), "TrackCopy"))
		return false;

	if (copyNode->getChildCount() < 1)
		return false;

	XmlNodeRef trackNode = copyNode->getChild(0);

	int intParamId = 0;
	trackNode->getAttr("paramId", intParamId);
	if (!intParamId)
		return false;

	if (!node->CanAddTrackForParameter(static_cast<ESequencerParamType>(intParamId)))
		return false;

	return true;
}

void CMannNodesCtrl::PasteTrack(CSequencerNode* node)
{
	if (!CanPasteTrack(node))
		return;

	CClipboard clip;
	XmlNodeRef copyNode = clip.Get();
	XmlNodeRef trackNode = copyNode->getChild(0);
	int intParamId = 0;
	trackNode->getAttr("paramId", intParamId);

	CAnimSequenceUndo undo(m_sequence, "Paste Layer");

	CSequencerTrack* sequenceTrack = node->CreateTrack(static_cast<ESequencerParamType>(intParamId));
	if (sequenceTrack)
	{
		sequenceTrack->SerializeSelection(trackNode, true, false, 0.0f);
		sequenceTrack->OnChange();
	}

	InvalidateNodes();
	ExpandNode(node);
}

void CMannNodesCtrl::RemoveTrack(SItemInfo* pItemInfo)
{
	if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Are you sure you want to delete this track ? Undo will not be available !")))
	{
		CAnimSequenceUndo undo(m_sequence, "Remove Track");

		pItemInfo->node->RemoveTrack(pItemInfo->track);
		InvalidateNodes();
	}
}

void CMannNodesCtrl::ShowHideTrack(CSequencerNode* node, int trackIndex)
{
	CAnimSequenceUndo undo(m_sequence, "Modify Track");

	CSequencerTrack* track = node->GetTrackByIndex(trackIndex);

	// change hidden flag for this track.
	if (track->GetFlags() & CSequencerTrack::SEQUENCER_TRACK_HIDDEN)
		track->SetFlags(track->GetFlags() & ~CSequencerTrack::SEQUENCER_TRACK_HIDDEN);
	else
		track->SetFlags(track->GetFlags() | CSequencerTrack::SEQUENCER_TRACK_HIDDEN);

	InvalidateNodes();
}

void CMannNodesCtrl::MuteAllBut(SItemInfo* pItemInfo)
{
	MuteAllNodes();
	MuteNodesRecursive(pItemInfo, false);
}

void CMannNodesCtrl::MuteNodesRecursive(SItemInfo* pItemInfo, bool bMute)
{
	if (pItemInfo)
	{
		if (pItemInfo->track)
		{
			pItemInfo->track->Mute(bMute);

			SItemInfo* pParent = (SItemInfo*)pItemInfo->GetParentRecord();
			if (pParent && pParent->node && !pParent->track && (pParent->node == pItemInfo->node))
			{
				uint32 mutedAnimLayerMask;
				uint32 mutedProcLayerMask;
				GenerateMuteMasks(pParent, mutedAnimLayerMask, mutedProcLayerMask);
				pParent->node->UpdateMutedLayerMasks(mutedAnimLayerMask, mutedProcLayerMask);
			}
		}
		else if (pItemInfo->node)
		{
			pItemInfo->node->Mute(bMute);
		}

		int numRecords = pItemInfo->GetChildCount();
		for (int i = 0; i < numRecords; i++)
		{
			SItemInfo* pChild = (SItemInfo*)pItemInfo->GetChild(i);
			MuteNodesRecursive(pChild, bMute);
		}
	}
}

void CMannNodesCtrl::ToggleMute(SItemInfo* pItemInfo)
{
	if (pItemInfo)
	{
		if (pItemInfo->track)
		{
			MuteNodesRecursive(pItemInfo, !pItemInfo->track->IsMuted());
		}
		else if (pItemInfo->node)
		{
			MuteNodesRecursive(pItemInfo, !pItemInfo->node->IsMuted());
		}
	}
}

void CMannNodesCtrl::MuteAllNodes()
{
	CXTPReportRecords* pRecords = GetRecords();
	if (pRecords)
	{
		int numRecords = pRecords->GetCount();
		for (int i = 0; i < numRecords; i++)
		{
			SItemInfo* pItem = (SItemInfo*)pRecords->GetAt(i);
			MuteNodesRecursive(pItem, true);
		}
	}
}

void CMannNodesCtrl::UnmuteAllNodes()
{
	CXTPReportRecords* pRecords = GetRecords();
	if (pRecords)
	{
		int numRecords = pRecords->GetCount();
		for (int i = 0; i < numRecords; i++)
		{
			SItemInfo* pItem = (SItemInfo*)pRecords->GetAt(i);
			MuteNodesRecursive(pItem, false);
		}
	}
}

void CMannNodesCtrl::RefreshTracks()
{
	if (m_keysCtrl)
	{
		float fTime = m_keysCtrl->GetCurrTime();
		if (CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_FRAGMENT_EDITOR_PANE))
			CMannequinDialog::GetCurrentInstance()->FragmentEditor()->SetTime(fTime);
		else if (CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_PREVIEWER_PANE))
			CMannequinDialog::GetCurrentInstance()->Previewer()->SetTime(fTime);
		else if (CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_TRANSITION_EDITOR_PANE))
			CMannequinDialog::GetCurrentInstance()->TransitionEditor()->SetTime(fTime);
	}

	InvalidateNodes();
}

void CMannNodesCtrl::GenerateMuteMasks(SItemInfo* pItem, uint32& mutedAnimLayerMask, uint32& mutedProcLayerMask)
{
	mutedAnimLayerMask = 0;
	mutedProcLayerMask = 0;

	if (pItem)
	{
		uint32 animCount = 0;
		uint32 procCount = 0;
		bool bHasFragmentId = false;

		int numRecords = pItem->GetChildCount();
		for (uint32 i = 0; i < numRecords; ++i)
		{
			SItemInfo* pChild = (SItemInfo*)pItem->GetChild(i);
			if (pChild && pChild->track)
			{
				switch (pChild->paramId)
				{
				case SEQUENCER_PARAM_FRAGMENTID:
					{
						bHasFragmentId = true;
					}
					break;
				case SEQUENCER_PARAM_ANIMLAYER:
					{
						CSequencerTrack* pTrack = pChild->track;
						if (pTrack->IsMuted())
						{
							mutedAnimLayerMask |= BIT(animCount);
						}
						++animCount;
					}
					break;
				case SEQUENCER_PARAM_PROCLAYER:
					{
						CSequencerTrack* pTrack = pChild->track;
						if (pTrack->IsMuted())
						{
							mutedProcLayerMask |= BIT(procCount);
						}
						++procCount;
					}
					break;
				default:
					break;
				}
			}
		}

		if (bHasFragmentId && !animCount && !procCount)
		{
			// special case: if there is a fragmentId, but no anim or proc layers
			// assume everything is controlled by parent state.
			if (pItem->node)
			{
				CSequencerNode* pNode = pItem->node;
				if (pNode->IsMuted())
				{
					mutedAnimLayerMask = 0xFFFFFFFF;
					mutedProcLayerMask = 0xFFFFFFFF;
				}
			}
		}
	}
}

int CMannNodesCtrl::ShowPopupMenu(CPoint point, const SItemInfo* pItemInfo, uint32 column)
{
	// Create pop up menu.

	switch (column)
	{
	default:
	case eCOLUMN_NODE:
		{
			return ShowPopupMenuNode(point, pItemInfo);
		}
		break;
	case eCOLUMN_MUTE:
		{
			return ShowPopupMenuMute(point, pItemInfo);
		}
		break;
	}

	return 0;
}

int CMannNodesCtrl::ShowPopupMenuNode(CPoint point, const SItemInfo* pItemInfo)
{
	CMenu menu;
	CMenu menuAddTrack;

	CMenu menuExpand;
	CMenu menuCollapse;

	menu.CreatePopupMenu();

	bool onNode = false;

	if (GetSelectedCount() == 1)
	{
		bool notOnValidItem = !pItemInfo || !pItemInfo->node;
		bool onValidItem = !notOnValidItem;
		onNode = onValidItem && pItemInfo->track == NULL;
		bool onTrack = onValidItem && pItemInfo->track != NULL;
		if (onValidItem)
		{
			if (onNode)
			{
				menu.AppendMenu(MF_SEPARATOR, 0, "");
				menu.AppendMenu(MF_STRING, eMenuItem_CopySelectedKeys, "Copy Selected Keys");
			}
			else  // On a track
			{
				menu.AppendMenu(MF_STRING, eMenuItem_CopyKeys, "Copy Keys");
			}

			menu.AppendMenu(MF_STRING, eMenuItem_PasteKeys, "Paste Keys");

			if (onNode)
			{
				menu.AppendMenu(MF_SEPARATOR, 0, "");

				pItemInfo->node->InsertMenuOptions(menu);
			}
		}

		// add layers submenu
		menuAddTrack.CreatePopupMenu();
		bool bTracksToAdd = false;
		if (onValidItem)
		{
			menu.AppendMenu(MF_SEPARATOR, 0, "");
			// List`s which tracks can be added to animation node.
			const int validParamCount = pItemInfo->node->GetParamCount();
			for (int i = 0; i < validParamCount; ++i)
			{
				CSequencerNode::SParamInfo paramInfo;
				if (!pItemInfo->node->GetParamInfo(i, paramInfo))
				{
					continue;
				}

				if (!pItemInfo->node->CanAddTrackForParameter(paramInfo.paramId))
				{
					continue;
				}

				menuAddTrack.AppendMenu(MF_STRING, eMenuItem_AddLayer_First + i, paramInfo.name);
				bTracksToAdd = true;
			}
		}

		if (bTracksToAdd)
			menu.AppendMenu(MF_POPUP, (UINT_PTR)menuAddTrack.m_hMenu, "Add Layer");

		if (onTrack)
			menu.AppendMenu(MF_STRING, eMenuItem_CopyLayer, "Copy Layer");

		if (bTracksToAdd)
		{
			bool canPaste = pItemInfo && pItemInfo->node && CanPasteTrack(pItemInfo->node);
			menu.AppendMenu(MF_STRING | (canPaste ? 0 : MF_GRAYED), eMenuItem_PasteLayer, "Paste Layer");
		}

		if (onTrack)
			menu.AppendMenu(MF_STRING, eMenuItem_RemoveLayer, "Remove Layer");

		if (bTracksToAdd || onTrack)
			menu.AppendMenu(MF_SEPARATOR, 0, "");

		if (onValidItem)
		{
			CString str;
			str.Format("%s Tracks", pItemInfo->node->GetName());
			menu.AppendMenu(MF_STRING | MF_DISABLED, 0, str);

			// Show tracks in anim node.
			{
				CSequencerNode::SParamInfo paramInfo;
				for (int i = 0; i < pItemInfo->node->GetTrackCount(); i++)
				{
					CSequencerTrack* track = pItemInfo->node->GetTrackByIndex(i);

					if (!pItemInfo->node->GetParamInfoFromId(track->GetParameterType(), paramInfo))
						continue;

					// change hidden flag for this track.
					int checked = MF_CHECKED;
					if (track->GetFlags() & CSequencerTrack::SEQUENCER_TRACK_HIDDEN)
					{
						checked = MF_UNCHECKED;
					}
					menu.AppendMenu(MF_STRING | checked, eMenuItem_ShowHide_First + i, CString("  ") + paramInfo.name);
				}
			}
		}
	}

	GetCursorPos(&point);
	// track menu

	if (m_bEditLock)
		SetPopupMenuLock(&menu);

	int ret = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
	                                          point.x, point.y, this, NULL);

	if (onNode)
	{
		pItemInfo->node->ClearMenuOptions(menu);
	}

	return ret;
}

int CMannNodesCtrl::ShowPopupMenuMute(CPoint point, const SItemInfo* pItemInfo)
{
	CMenu menu;

	bool onNode = false;

	if (GetSelectedCount() == 1)
	{
		bool notOnValidItem = !pItemInfo || !pItemInfo->node;
		bool onValidItem = !notOnValidItem;
		if (onValidItem)
		{
			menu.CreatePopupMenu();

			if (pItemInfo->track)
			{
				CSequencerTrack* pTrack = pItemInfo->track;
				if (pTrack)
				{
					// solo not active, so mute options are available
					menu.AppendMenu(MF_STRING, eMenuItem_SoloNode, "Mute All But This");
					menu.AppendMenu(MF_SEPARATOR, 0, "");
					menu.AppendMenu(MF_STRING, eMenuItem_MuteNode, pTrack->IsMuted() ? "Unmute" : "Mute");
					menu.AppendMenu(MF_SEPARATOR, 0, "");
					menu.AppendMenu(MF_STRING, eMenuItem_MuteAll, "Mute All");
					menu.AppendMenu(MF_STRING, eMenuItem_UnmuteAll, "Unmute All");
				}
			}
			else if (pItemInfo->node)
			{
				CSequencerNode* pNode = pItemInfo->node;
				if (pNode)
				{
					// solo not active, so mute options are available
					menu.AppendMenu(MF_STRING, eMenuItem_SoloNode, "Mute All But This");
					menu.AppendMenu(MF_SEPARATOR, 0, "");
					menu.AppendMenu(MF_STRING, eMenuItem_MuteNode, pNode->IsMuted() ? "Unmute" : "Mute");
					menu.AppendMenu(MF_SEPARATOR, 0, "");
					menu.AppendMenu(MF_STRING, eMenuItem_MuteAll, "Mute All");
					menu.AppendMenu(MF_STRING, eMenuItem_UnmuteAll, "Unmute All");
				}
			}

			GetCursorPos(&point);

			if (m_bEditLock)
				SetPopupMenuLock(&menu);

			int ret = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
			                                          point.x, point.y, this, NULL);

			return ret;
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
void CMannNodesCtrl::SetPopupMenuLock(CMenu* menu)
{
	if (!m_bEditLock || !menu)
		return;

	UINT count = menu->GetMenuItemCount();
	for (UINT i = 0; i < count; ++i)
	{
		CString menuString;
		menu->GetMenuString(i, menuString, MF_BYPOSITION);

		if (menuString != "Expand" && menuString != "Collapse")
			menu->EnableMenuItem(i, MF_DISABLED | MF_GRAYED | MF_BYPOSITION);
	}
}

void CMannNodesCtrl::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	SyncKeyCtrl();
}

//-----------------------------------------------------------------------------
BOOL CMannNodesCtrl::OnEraseBkgnd(CDC* pDC)
{
	CXTPReportPaintManager* pPaintManager = GetPaintManager();
	if (pPaintManager)
	{
		COLORREF backGround(COLORREF_NULL);
		if (m_bEditLock)
			backGround = EDIT_DISABLE_GRAY_COLOR;

		pPaintManager->m_clrControlBack.SetCustomValue(backGround);

	}
	return TRUE;
}

float CMannNodesCtrl::SaveVerticalScrollPos() const
{
	int sbMin = 0, sbMax = 0;
	GetScrollRange(SB_VERT, &sbMin, &sbMax);
	return float(GetTopRowIndex()) / std::max(float(sbMax - sbMin), 1.0f);
}

void CMannNodesCtrl::RestoreVerticalScrollPos(float fScrollPos)
{
	int sbMin = 0, sbMax = 0;
	GetScrollRange(SB_VERT, &sbMin, &sbMax);
	int newScrollPos = pos_directed_rounding(fScrollPos * (sbMax - sbMin) + sbMin);
	SetTopRow(newScrollPos);
}

//////////////////////////////////////////////////////////////////////////
CMannNodesCtrl::SItemInfo* CMannNodesCtrl::IsPointValidForAnimationInContextDrop(const CPoint& pt, COleDataObject* pDataObject) const
{
	if (!CMannequinDialog::GetCurrentInstance()->GetDockingPaneManager()->IsPaneSelected(CMannequinDialog::IDW_FRAGMENT_EDITOR_PANE))
	{
		return nullptr;
	}

	UINT clipFormat = MannequinDragDropHelpers::GetAnimationNameClipboardFormat();
	HGLOBAL hData = pDataObject->GetGlobalData(clipFormat);

	if (nullptr == hData)
	{
		return nullptr;
	}

	string sAnimName = static_cast<char*>(GlobalLock(hData));
	GlobalUnlock(hData);

	if (sAnimName.empty())
	{
		return nullptr;
	}

	if (!m_sequence)
	{
		return nullptr;
	}

	if (m_bEditLock)
	{
		return nullptr;
	}

	SItemInfo* pItemInfo = 0;

	// Find node under mouse.
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	// Select the item that is at the point myPoint.
	CXTPReportRow* pRow = HitTest(point);
	if (nullptr != pRow)
	{
		pItemInfo = (SItemInfo*)pRow->GetRecord();
	}

	if (nullptr == pItemInfo)
	{
		return nullptr;
	}

	if (!pItemInfo->node)
	{
		return nullptr;
	}

	if (!pItemInfo->node->CanAddTrackForParameter(SEQUENCER_PARAM_ANIMLAYER))
	{
		return nullptr;
	}
	return pItemInfo;

}

//////////////////////////////////////////////////////////////////////////
// Assume IsPointValidForAnimationInContextDrop returned true
bool CMannNodesCtrl::CreatePointForAnimationInContextDrop(SItemInfo* pItemInfo, const CPoint& point, COleDataObject* pDataObject)
{
	// List`s which tracks can be added to animation node.
	const unsigned int validParamCount = unsigned int (pItemInfo->node->GetParamCount());

	UINT clipFormat = MannequinDragDropHelpers::GetAnimationNameClipboardFormat();
	HGLOBAL hData = pDataObject->GetGlobalData(clipFormat);

	if (nullptr == hData)
	{
		return false;
	}

	string sAnimName = static_cast<char*>(GlobalLock(hData));
	GlobalUnlock(hData);

	unsigned int nAnimLyrIdx = 0;
	for (; nAnimLyrIdx < validParamCount; ++nAnimLyrIdx)
	{
		CSequencerNode::SParamInfo paramInfo;
		if (!pItemInfo->node->GetParamInfo(nAnimLyrIdx, paramInfo))
		{
			continue;
		}

		if (SEQUENCER_PARAM_ANIMLAYER == paramInfo.paramId)
		{
			break;
		}
	}

	if (nAnimLyrIdx == validParamCount)
	{
		return false;
	}

	_smart_ptr<CSequencerNode> pNode = pItemInfo->node;
	AddTrack(nAnimLyrIdx, pItemInfo->node);
	CSequencerTrack* pTrack = pNode->GetTrackByIndex(pNode->GetTrackCount() - 1);

	int keyID = pTrack->CreateKey(0.0f);

	CClipKey newKey;
	newKey.m_time = 0.0f;
	newKey.animRef.SetByString(sAnimName);
	pTrack->SetKey(keyID, &newKey);
	pTrack->SelectKey(keyID, true);
	return true;
}

