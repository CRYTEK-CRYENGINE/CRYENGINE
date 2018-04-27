// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MaterialLibrary.h"
#include "Material.h"
#include "BaseLibraryManager.h"

//////////////////////////////////////////////////////////////////////////
// CMaterialLibrary implementation.
//////////////////////////////////////////////////////////////////////////
bool CMaterialLibrary::Save()
{
	return SaveLibrary("MaterialLibrary");
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialLibrary::Load(const string& filename)
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
void CMaterialLibrary::Serialize(XmlNodeRef& root, bool bLoading)
{
	/*
	   if (bLoading)
	   {
	   // Loading.
	   string name = GetName();
	   root->getAttr( "Name",name );
	   SetName( name );
	   for (int i = 0; i < root->getChildCount(); i++)
	   {
	    XmlNodeRef itemNode = root->getChild(i);
	    CMaterial *material = new CMaterial(itemNode->getAttr("Name"));
	    AddItem( material );
	    CBaseLibraryItem::SerializeContext ctx( itemNode,bLoading );
	    material->Serialize( ctx );
	   }
	   SetModified(false);
	   }
	   else
	   {
	   // Saving.
	   root->setAttr( "Name",GetName() );
	   root->setAttr( "SandboxVersion",(const char*)GetIEditorImpl()->GetFileVersion().ToFullString() );
	   // Serialize prototypes.
	   for (int i = 0; i < GetItemCount(); i++)
	   {
	    CMaterial *pMtl = (CMaterial*)GetItem(i);
	    // Save materials with parents under thier parent xml node.
	    if (pMtl->GetParent())
	      continue;

	    XmlNodeRef itemNode = root->newChild( "Material" );
	    CBaseLibraryItem::SerializeContext ctx( itemNode,bLoading );
	    GetItem(i)->Serialize( ctx );
	   }
	   }
	 */
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CMaterialLibrary::AddItem(IDataBaseItem* item, bool bRegister)
{
	CBaseLibraryItem* pLibItem = (CBaseLibraryItem*)item;
	// Check if item is already assigned to this library.
	if (pLibItem->GetLibrary() != this)
	{
		pLibItem->SetLibrary(this);
		if (bRegister)
			m_pManager->RegisterItem(pLibItem);
		m_items.push_back(pLibItem);
	}
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CMaterialLibrary::GetItem(int index)
{
	assert(index >= 0 && index < m_items.size());
	return m_items[index];
}

//////////////////////////////////////////////////////////////////////////
void CMaterialLibrary::RemoveItem(IDataBaseItem* item)
{
	for (int i = 0; i < m_items.size(); i++)
	{
		if (m_items[i] == item)
		{
			m_items.erase(m_items.begin() + i);
			SetModified();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CMaterialLibrary::FindItem(const string& name)
{
	for (int i = 0; i < m_items.size(); i++)
	{
		if (stricmp(m_items[i]->GetName(), name) == 0)
		{
			return m_items[i];
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialLibrary::RemoveAllItems()
{
	AddRef();
	for (int i = 0; i < m_items.size(); i++)
	{
		// Clear library item.
		m_items[i]->SetLibrary(NULL);
	}
	m_items.clear();
	Release();
}

