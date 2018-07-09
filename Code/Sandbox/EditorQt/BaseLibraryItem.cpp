// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseLibraryItem.h"
#include "BaseLibrary.h"
#include "BaseLibraryManager.h"

//////////////////////////////////////////////////////////////////////////
// CBaseLibraryItem implementation.
//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem::CBaseLibraryItem()
{
	m_library = 0;
	GenerateId();
	m_bModified = false;
}

CBaseLibraryItem::~CBaseLibraryItem()
{
}

//////////////////////////////////////////////////////////////////////////
string CBaseLibraryItem::GetFullName() const
{
	string name;
	if (m_library)
		name = m_library->GetName() + ".";
	name += m_name;
	return name;
}

//////////////////////////////////////////////////////////////////////////
string CBaseLibraryItem::GetGroupName()
{
	string str = GetName();
	int p = str.ReverseFind('.');
	if (p >= 0)
	{
		return str.Mid(0, p);
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
string CBaseLibraryItem::GetShortName()
{
	string str = GetName();
	int p = str.ReverseFind('.');
	if (p >= 0)
	{
		return str.Mid(p + 1);
	}
	p = str.ReverseFind('/');
	if (p >= 0)
	{
		return str.Mid(p + 1);
	}
	return str;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryItem::SetName(const string& name, bool bSkipUndo)
{
	assert(m_library);
	if (name == m_name)
		return;
	if (!bSkipUndo)
		m_library->StoreUndo("Rename item library");
	string oldName = GetFullName();
	m_name = name;
	((CBaseLibraryManager*)m_library->GetManager())->OnRenameItem(this, oldName);
}

//////////////////////////////////////////////////////////////////////////
const string& CBaseLibraryItem::GetName() const
{
	return m_name;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryItem::GenerateId()
{
	SetGUID(CryGUID::Create());
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryItem::SetGUID(CryGUID guid)
{
	if (m_library)
	{
		((CBaseLibraryManager*)m_library->GetManager())->RegisterItem(this, guid);
	}
	m_guid = guid;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryItem::Serialize(SerializeContext& ctx)
{
	assert(m_library);

	XmlNodeRef node = ctx.node;
	if (ctx.bLoading)
	{
		string name = m_name;
		// Loading
		node->getAttr("Name", name);

		if (!ctx.bUniqName)
		{
			SetName(name);
		}
		else
		{
			SetName(GetLibrary()->GetManager()->MakeUniqItemName(name));
		}

		if (!ctx.bCopyPaste)
		{
			CryGUID guid;
			if (node->getAttr("Id", guid))
				SetGUID(guid);
		}
	}
	else
	{
		// Saving.
		node->setAttr("Name", m_name);
		node->setAttr("Id", m_guid);
		node->setAttr("Library", GetLibrary()->GetName());
	}
	m_bModified = false;
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryItem::GetLibrary() const
{
	return m_library;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryItem::SetLibrary(CBaseLibrary* pLibrary)
{
	m_library = pLibrary;
}

//! Mark library as modified.
void CBaseLibraryItem::SetModified(bool bModified)
{
	m_bModified = bModified;
	if (m_bModified && m_library != NULL)
		m_library->SetModified(bModified);
}

