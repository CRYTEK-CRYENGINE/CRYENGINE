// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/IFlares.h>
#include "LensFlareAtomicList.h"
#include "LensFlareUtil.h"
#include "LensFlareEditor.h"
#include "LensFlareElementTree.h"
#include "LensFlareItemTree.h"

BEGIN_MESSAGE_MAP(CLensFlareAtomicList, CImageListCtrl)
END_MESSAGE_MAP()

CLensFlareAtomicList::CLensFlareAtomicList()
{
}

CLensFlareAtomicList::~CLensFlareAtomicList()
{
}

bool CLensFlareAtomicList::InsertItem(const FlareInfo& flareInfo)
{
	CFlareImageItem* pPreviewItem = new CFlareImageItem;
	bool bUseImage(false);

	if (flareInfo.imagename)
	{
		CImageEx* pImage = new CImageEx;
		if (CImageUtil::LoadImage(flareInfo.imagename, *pImage))
		{
			pImage->SwapRedAndBlue();
			bUseImage = true;
			pPreviewItem->bBitmapValid = false;
			pPreviewItem->pImage = pImage;
			int width(pImage->GetWidth());
			int height(pImage->GetHeight());
			pPreviewItem->rect = CRect(0, 0, width, height);
			SetItemSize(CSize(width, height));
		}
		else
		{
			delete pImage;
		}
	}

	if (bUseImage == false)
	{
		pPreviewItem->bitmap.LoadBitmap(MAKEINTRESOURCE(IDB_WATER));
		pPreviewItem->bBitmapValid = true;
		pPreviewItem->pImage = NULL;
		pPreviewItem->rect = CRect(0, 0, 64, 64);
	}

	pPreviewItem->text = flareInfo.name;
	pPreviewItem->bSelected = false;

	if (!CImageListCtrl::InsertItem(pPreviewItem))
	{
		delete pPreviewItem;
		return false;
	}

	CFlareImageItem* pFlareITem = (CFlareImageItem*)(m_items[m_items.size() - 1].get());
	pFlareITem->flareType = flareInfo.type;

	return true;
}

void CLensFlareAtomicList::FillAtomicItems()
{
	const FlareInfoArray::Props array = FlareInfoArray::Get();
	for (size_t i = 0; i < array.size; ++i)
	{
		const FlareInfo& flareInfo(array.p[i]);
		if (LensFlareUtil::IsElement(flareInfo.type))
			InsertItem(flareInfo);
	}
}

bool CLensFlareAtomicList::OnBeginDrag(UINT nFlags, CPoint point)
{
	return __super::OnBeginDrag(nFlags, point);
}

void CLensFlareAtomicList::OnMouseMove(UINT nFlags, CPoint point)
{
	__super::OnMouseMove(nFlags, point);
}

void CLensFlareAtomicList::OnDropItem(CPoint point)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (!pEditor)
		return;

	CPoint screenPos(point);
	ClientToScreen(&screenPos);

	CUndo undo("Add Atomic Lens Flare Item");

	if (pEditor->IsPointInLensFlareElementTree(screenPos))
	{
		if (m_pSelectedItem)
		{
			CLensFlareElementTree* pElementTree = pEditor->GetLensFlareElementTree();
			if (pElementTree)
				pElementTree->AddElement(((CFlareImageItem*)&*m_pSelectedItem)->flareType);
		}
	}
	else if (pEditor->IsPointInLensFlareItemTree(screenPos))
	{
		if (m_pSelectedItem)
			pEditor->AddNewItemByAtomicOptics(((CFlareImageItem*)&*m_pSelectedItem)->flareType);
	}
}

void CLensFlareAtomicList::OnDraggingItem(CPoint point)
{
	CLensFlareEditor* pEditor = CLensFlareEditor::GetLensFlareEditor();
	if (!pEditor)
		return;

	CPoint screenPos(point);
	ClientToScreen(&screenPos);

	if (pEditor->IsPointInLensFlareElementTree(screenPos))
	{
		CLensFlareElementTree* pElementTree = pEditor->GetLensFlareElementTree();
		if (pElementTree)
			pElementTree->UpdateDraggingFromOtherWindow();
	}
	else if (pEditor->IsPointInLensFlareItemTree(screenPos))
	{
		pEditor->GetLensFlareItemTree()->UpdateDraggingFromOtherWindow();
	}
}

