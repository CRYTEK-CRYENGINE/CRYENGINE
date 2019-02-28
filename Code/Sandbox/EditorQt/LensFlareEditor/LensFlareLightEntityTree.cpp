// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LensFlareLightEntityTree.h"
#include "LensFlareUtil.h"
#include "LensFlareItem.h"
#include "Objects/EntityObject.h"
#include "LensFlareEditor.h"
#include "Viewport.h"

BEGIN_MESSAGE_MAP(CLensFlareLightEntityTree, CXTTreeCtrl)
ON_NOTIFY_REFLECT(NM_DBLCLK, OnTvnItemDoubleClicked)
END_MESSAGE_MAP()

CLensFlareLightEntityTree::CLensFlareLightEntityTree()
{
	CLensFlareEditor::GetLensFlareEditor()->RegisterLensFlareItemChangeListener(this);
	GetIEditorImpl()->GetObjectManager()->AddObjectEventListener(functor(*this, &CLensFlareLightEntityTree::OnObjectEvent));
	m_pLensFlareItem = NULL;
}

CLensFlareLightEntityTree::~CLensFlareLightEntityTree()
{
	CLensFlareEditor::GetLensFlareEditor()->UnregisterLensFlareItemChangeListener(this);
	GetIEditorImpl()->GetObjectManager()->RemoveObjectEventListener(functor(*this, &CLensFlareLightEntityTree::OnObjectEvent));
}

void CLensFlareLightEntityTree::OnLensFlareChangeItem(CLensFlareItem* pLensFlareItem)
{
	DeleteAllItems();

	if (pLensFlareItem == NULL)
		return;

	m_pLensFlareItem = pLensFlareItem;

	std::vector<CEntityObject*> lightEntities;
	LensFlareUtil::GetLightEntityObjects(lightEntities);

	for (int i = 0, iEntitySize(lightEntities.size()); i < iEntitySize; ++i)
		AddLightEntity(lightEntities[i]);
}

void CLensFlareLightEntityTree::OnLensFlareDeleteItem(CLensFlareItem* pLensFlareItem)
{
	if (m_pLensFlareItem == pLensFlareItem)
	{
		DeleteAllItems();
		m_pLensFlareItem = NULL;
	}
}

void CLensFlareLightEntityTree::OnObjectEvent(CBaseObject* pObject, int nEvent)
{
	if (!pObject || !pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		return;

	switch (nEvent)
	{
	case OBJECT_ON_RENAME:
	case OBJECT_ON_DELETE:
		{
			HTREEITEM hItem = FindItem(GetRootItem(), pObject);
			if (!hItem)
				return;
			if (nEvent == OBJECT_ON_RENAME)
				SetItemText(hItem, pObject->GetName());
			else
				DeleteItem(hItem);
		}
		break;

	case OBJECT_ON_ADD:
		AddLightEntity((CEntityObject*)pObject);
		break;
	}
}

HTREEITEM CLensFlareLightEntityTree::FindItem(HTREEITEM hStartItem, CBaseObject* pObject) const
{
	HTREEITEM hItem = hStartItem;

	while (hItem)
	{
		if ((CBaseObject*)GetItemData(hItem) == pObject)
			return hItem;
		if (ItemHasChildren(hItem))
		{
			HTREEITEM hExpectedItem = FindItem(hItem, pObject);
			if (hExpectedItem)
				return hExpectedItem;
		}
		hItem = GetNextSiblingItem(hItem);
	}

	return NULL;
}

void CLensFlareLightEntityTree::AddLightEntity(CEntityObject* pEntity)
{
	if (pEntity == NULL || m_pLensFlareItem == NULL)
		return;

	CString lensFlareName = pEntity->GetEntityPropertyString(CEntityObject::s_LensFlarePropertyName);
	if (lensFlareName.IsEmpty())
		return;

	CString fullName = m_pLensFlareItem->GetFullName();
	if (fullName.IsEmpty())
		return;

	if (!stricmp(lensFlareName.GetBuffer(), fullName.GetBuffer()))
	{
		HTREEITEM hItem = InsertItem(pEntity->GetName());
		SetItemData(hItem, (DWORD_PTR)pEntity);
	}
}

void CLensFlareLightEntityTree::OnTvnItemDoubleClicked(NMHDR* pNMHDR, LRESULT* pResult)
{
	HTREEITEM hItem = GetSelectedItem();

	if (hItem == NULL)
		return;

	CBaseObject* pObject = (CBaseObject*)GetItemData(hItem);
	if (pObject == NULL)
		return;

	if (!pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		return;

	const CSelectionGroup* pSelection = GetIEditorImpl()->GetObjectManager()->GetSelection();
	if (pSelection)
	{
		int iSelectionCount(pSelection->GetCount());
		if (iSelectionCount == 1)
		{
			if (pSelection->GetObject(0) == pObject)
			{
				CViewport* vp = GetIEditorImpl()->GetActiveView();
				if (vp)
					vp->CenterOnSelection();
				return;
			}
		}
	}

	GetIEditorImpl()->GetObjectManager()->ClearSelection();
	GetIEditorImpl()->GetObjectManager()->SelectObject(pObject);
}

