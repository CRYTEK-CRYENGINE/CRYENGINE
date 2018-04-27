// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LensFlareItemTree.h"
#include "LensFlareElementTree.h"
#include "LensFlareEditor.h"
#include "LensFlareItem.h"
#include "LensFlareManager.h"
#include "ViewManager.h"
#include "Objects/EntityObject.h"
#include "QtUtil.h"

BEGIN_MESSAGE_MAP(CLensFlareItemTree, CXTTreeCtrl)
ON_NOTIFY_REFLECT(TVN_BEGINDRAG, OnBeginDrag)
ON_WM_LBUTTONDOWN()
ON_WM_LBUTTONUP()
ON_WM_MOUSEMOVE()
ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CLensFlareItemTree::CLensFlareItemTree()
{
}

CLensFlareItemTree::~CLensFlareItemTree()
{
}

void CLensFlareItemTree::OnLButtonDown(UINT nFlags, CPoint point)
{
	CUndo undo("Changed lens flare item");
	__super::OnLButtonDown(nFlags, point);
}

void CLensFlareItemTree::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CUndo undo("Changed lens flare item");
	__super::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CLensFlareItemTree::OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (!pEditor)
		return;

	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	if (!pNMTreeView)
		return;

	if (!pNMTreeView->itemNew.hItem)
		return;

	SelectItem(pNMTreeView->itemNew.hItem);

	bool bControlPressed = QtUtil::IsModifierKeyDown(Qt::ControlModifier);
	XmlNodeRef XMLNode;
	XMLNode = pEditor->CreateXML(bControlPressed ? FLARECLIPBOARDTYPE_COPY : FLARECLIPBOARDTYPE_CUT);
	if (XMLNode == NULL)
		return;

	LensFlareUtil::BeginDragDrop(*this, pNMTreeView->ptDrag, XMLNode);
}

void CLensFlareItemTree::OnMouseMove(UINT nFlags, CPoint point)
{
	LensFlareUtil::MoveDragDrop(*this, point, &functor(*this, &CLensFlareItemTree::UpdateLensFlareItem));
	__super::OnMouseMove(nFlags, point);
}

void CLensFlareItemTree::OnLButtonUp(UINT nFlags, CPoint point)
{
	LensFlareUtil::EndDragDrop(*this, point, &functor(*this, &CLensFlareItemTree::Drop));
	__super::OnLButtonUp(nFlags, point);
}

void CLensFlareItemTree::UpdateLensFlareItem(HTREEITEM hSelectedItem, const CPoint& screenPos)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (!pEditor)
		return;

	if (pEditor->IsPointInLensFlareElementTree(screenPos))
	{
		CLensFlareElementTree* pElementTree = pEditor->GetLensFlareElementTree();
		if (pElementTree)
			pElementTree->UpdateDraggingFromOtherWindow();
	}
	else if (pEditor->IsPointInLensFlareItemTree(screenPos))
	{
		if (hSelectedItem)
			pEditor->UpdateLensFlareItem(pEditor->GetLensFlareItem(hSelectedItem));
	}
}

void CLensFlareItemTree::Drop(XmlNodeRef xmlNode, const CPoint& screenPos)
{
	if (xmlNode == NULL)
		return;

	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (!pEditor)
		return;

	if (pEditor->IsPointInLensFlareElementTree(screenPos))
	{
		CLensFlareElementTree* pElementTree = pEditor->GetLensFlareElementTree();
		if (pElementTree)
		{
			pElementTree->SelectTreeItemByCursorPos();
			CUndo undo("Copy/Cut & Paste library item");
			pElementTree->Paste(xmlNode);
		}
	}
	else if (pEditor->IsPointInLensFlareItemTree(screenPos))
	{
		CUndo undo("Copy/Cut & Paste library item");
		pEditor->Paste(xmlNode);
	}
	else
	{
		CViewport* pViewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(screenPos);
		if (pViewport)
			AssignLensFlareToLightEntity(xmlNode, screenPos);
	}
}

void CLensFlareItemTree::AssignLensFlareToLightEntity(XmlNodeRef xmlNode, const CPoint& screenPos) const
{
	CViewport* pViewport = GetIEditorImpl()->GetViewManager()->GetViewportAtPoint(screenPos);
	if (pViewport == NULL)
		return;

	CPoint viewportPos(screenPos);
	pViewport->ScreenToClient(&viewportPos);
	HitContext hit;
	if (!pViewport->HitTest(viewportPos, hit))
		return;

	if (hit.object && hit.object->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = (CEntityObject*)hit.object;
		if (pEntity->IsLight() && xmlNode->getChildCount() > 0)
		{
			LensFlareUtil::SClipboardData clipboardData;
			clipboardData.FillThisFromXmlNode(xmlNode->getChild(0));
			CLensFlareItem* pLensFlareItem = (CLensFlareItem*)GetIEditorImpl()->GetLensFlareManager()->FindItemByName(clipboardData.m_LensFlareFullPath);
			if (!pLensFlareItem)
				return;
			CUndo undo("Assign a lens flare item to a light entity");
			pEntity->ApplyOptics(clipboardData.m_LensFlareFullPath, pLensFlareItem->GetOptics());
		}
	}
}

bool CLensFlareItemTree::SelectItemByCursorPos(bool* pOutChanged)
{
	HTREEITEM hOldItem = GetSelectedItem();
	HTREEITEM hItem = LensFlareUtil::GetTreeItemByHitTest(*this);
	if (hItem == NULL)
		return false;
	SelectItem(hItem);
	if (pOutChanged)
		*pOutChanged = hOldItem != hItem;
	return true;
}

void CLensFlareItemTree::UpdateDraggingFromOtherWindow()
{
	bool bChanged(false);
	SelectItemByCursorPos(&bChanged);
	HTREEITEM hItem = GetSelectedItem();
	if (hItem)
	{
		if (ItemHasChildren(hItem))
		{
			if (Expand(hItem, TVE_EXPAND))
				bChanged = true;
		}
	}
	if (bChanged)
		RedrawWindow();
}

