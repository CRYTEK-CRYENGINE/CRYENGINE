// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrefabLibrary.h"
#include "PrefabItem.h"

//////////////////////////////////////////////////////////////////////////
// CPrefabLibrary implementation.
//////////////////////////////////////////////////////////////////////////
bool CPrefabLibrary::Save()
{
	return SaveLibrary("PrefabsLibrary");
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabLibrary::Load(const string& filename)
{
	if (filename.IsEmpty())
		return false;

	SetFilename(filename);
	XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filename);
	if (!root)
		return false;

	Serialize(root, true);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabLibrary::Serialize(XmlNodeRef& root, bool bLoading)
{
	if (bLoading)
	{
		// Loading.
		RemoveAllItems();
		string name = GetName();
		root->getAttr("Name", name);
		SetName(name);
		for (int i = 0; i < root->getChildCount(); i++)
		{
			XmlNodeRef itemNode = root->getChild(i);
			// Only accept nodes with correct name.
			if (stricmp(itemNode->getTag(), "Prefab") != 0)
				continue;
			CBaseLibraryItem* pItem = new CPrefabItem;
			AddItem(pItem);

			CBaseLibraryItem::SerializeContext ctx(itemNode, bLoading);
			pItem->Serialize(ctx);
		}
		SetModified(false);
	}
	else
	{
		// Saving.
		root->setAttr("Name", GetName());
		// Serialize prototypes.
		for (int i = 0; i < GetItemCount(); i++)
		{
			XmlNodeRef itemNode = root->newChild("Prefab");
			CBaseLibraryItem::SerializeContext ctx(itemNode, bLoading);
			GetItem(i)->Serialize(ctx);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabLibrary::UpdatePrefabObjects()
{
	for (int i = 0, iItemSize(m_items.size()); i < iItemSize; ++i)
	{
		CPrefabItem* pPrefabItem = (CPrefabItem*)&*m_items[i];
		pPrefabItem->UpdateObjects();
	}
}

