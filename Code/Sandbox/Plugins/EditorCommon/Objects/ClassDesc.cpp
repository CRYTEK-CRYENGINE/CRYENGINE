// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ClassDesc.h"
#include "IIconManager.h"

int CObjectClassDesc::GetTextureIconId()
{
	if (!m_nTextureIcon)
	{
		const char* pTexName = GetTextureIcon();

		if (pTexName && *pTexName)
		{
			m_nTextureIcon = GetIEditor()->GetIconManager()->GetIconTexture(pTexName);
		}
	}

	return m_nTextureIcon;
}

void CObjectClassDesc::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	switch (event)
	{
	case EDB_ITEM_EVENT_ADD:
		m_itemAdded(pItem->GetFullName(), pItem->GetGUID().ToString(), "");
		break;
	case EDB_ITEM_EVENT_CHANGED:
		m_itemChanged(pItem->GetFullName(), pItem->GetGUID().ToString(), "");
		break;
	case EDB_ITEM_EVENT_DELETE:
		m_itemRemoved(pItem->GetFullName(), pItem->GetGUID().ToString());
		break;
	default:
		break;
	}
}

void CObjectClassDesc::OnDataBaseLibraryEvent(IDataBaseLibrary* pLibrary, EDataBaseLibraryEvent event)
{
	switch (event)
	{
	case EDB_LIBRARY_EVENT_DELETE:
		m_libraryRemoved(pLibrary->GetName());
		break;
	default:
		break;
	}
}

void CObjectClassDesc::OnDataBaseEvent(EDataBaseEvent event)
{
	switch (event)
	{
	case EDB_EVENT_CLEAR:
		m_databaseCleared();
		break;
	default:
		break;
	}
}

