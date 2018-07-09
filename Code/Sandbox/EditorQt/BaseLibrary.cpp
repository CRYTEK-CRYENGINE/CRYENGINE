// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseLibrary.h"
#include "BaseLibraryItem.h"
#include "BaseLibraryManager.h"
#include "ISourceControl.h"
#include "Dialogs/SourceControlAddDlg.h"

//////////////////////////////////////////////////////////////////////////
// Undo functionality for libraries.
//////////////////////////////////////////////////////////////////////////

class CUndoBaseLibrary : public IUndoObject
{
public:
	CUndoBaseLibrary(CBaseLibrary* pLib, const string& description)
	{
		m_pLib = pLib;
		m_description = description;
		m_redo = 0;
		m_undo = XmlHelpers::CreateXmlNode("Undo");
		m_pLib->Serialize(m_undo, false);
	}

protected:
	virtual const char* GetDescription() { return m_description; };

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
		{
			m_redo = XmlHelpers::CreateXmlNode("Redo");
			m_pLib->Serialize(m_redo, false);
		}
		m_pLib->Serialize(m_undo, true);
		m_pLib->SetModified();
		GetIEditorImpl()->Notify(eNotify_OnDataBaseUpdate);
	}

	virtual void Redo()
	{
		m_pLib->Serialize(m_redo, true);
		m_pLib->SetModified();
		GetIEditorImpl()->Notify(eNotify_OnDataBaseUpdate);
	}

private:
	string                  m_description;
	_smart_ptr<CBaseLibrary> m_pLib;
	XmlNodeRef               m_undo;
	XmlNodeRef               m_redo;
};

//////////////////////////////////////////////////////////////////////////
// CBaseLibrary implementation.
//////////////////////////////////////////////////////////////////////////
CBaseLibrary::CBaseLibrary(CBaseLibraryManager* pManager) :
	m_pManager(pManager),
	m_bModified(false),
	m_bLevelLib(false),
	m_bNewLibrary(true)
{
}

//////////////////////////////////////////////////////////////////////////
CBaseLibrary::~CBaseLibrary()
{
	m_items.clear();
}

//////////////////////////////////////////////////////////////////////////
IDataBaseManager* CBaseLibrary::GetManager()
{
	return m_pManager;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibrary::RemoveAllItems()
{
	AddRef();
	for (int i = 0; i < m_items.size(); i++)
	{
		// Clear library item.
		m_items[i]->m_library = NULL;
	}
	m_items.clear();
	Release();
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibrary::SetName(const string& name)
{
	m_name = name;

	/*
	   // Make a file name from name of library.
	   string path = Path::GetPath( m_filename );
	   m_filename = m_name;
	   m_filename.Replace( ' ','_' );
	   m_filename = path + m_filename + ".xml";
	 */
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
const string& CBaseLibrary::GetName() const
{
	return m_name;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseLibrary::Save()
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CBaseLibrary::Load(const string& filename)
{
	m_filename = filename;
	SetModified(false);
	m_bNewLibrary = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibrary::SetModified(bool bModified)
{
	m_bModified = bModified;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibrary::StoreUndo(const string& description)
{
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoBaseLibrary(this, description));
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibrary::AddItem(IDataBaseItem* item, bool bRegister)
{
	StoreUndo("Add library item");

	CBaseLibraryItem* pLibItem = (CBaseLibraryItem*)item;
	// Check if item is already assigned to this library.
	if (pLibItem->m_library != this)
	{
		pLibItem->m_library = this;
		m_items.push_back(pLibItem);
		SetModified();
		if (bRegister)
			m_pManager->RegisterItem(pLibItem);
	}
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibrary::GetItem(int index)
{
	assert(index >= 0 && index < m_items.size());
	return m_items[index];
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibrary::RemoveItem(IDataBaseItem* item)
{
	StoreUndo("Remove library item");

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
IDataBaseItem* CBaseLibrary::FindItem(const string& name)
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

bool CBaseLibrary::AddLibraryToSourceControl(const string& name) const
{
	if (!GetIEditorImpl()->IsSourceControlAvailable())
		return false;

	CSourceControlAddDlg dlg(name);
	if (dlg.DoModal() != IDCANCEL)
	{
		//bool addRes = GetIEditorImpl()->GetSourceControl()->Add(GetFilename(), dlg.m_sDesc, (dlg.GetResult() == CSourceControlAddDlg::ADD_AND_SUBMIT) ? 0 : ADD_WITHOUT_SUBMIT);
		bool addRes = GetIEditorImpl()->GetSourceControl()->Add(GetFilename(), dlg.m_sDesc);
		// The connection of the file with the source control failed
		if (!addRes)
			return false;
		// The file was correctly added
		return true;
	}
	// The source control is not connected to sandbox
	return false;
}

bool CBaseLibrary::SaveLibrary(const char* name)
{
	assert(name != NULL);
	if (name == NULL)
	{
		CryFatalError("The library you are attempting to save has no name specified.");
		return false;
	}

	string fileName(GetFilename());
	if (fileName.IsEmpty())
		return false;

	XmlNodeRef root = XmlHelpers::CreateXmlNode(name);
	Serialize(root, false);
	bool bRes = XmlHelpers::SaveXmlNode(root, fileName);
	if (m_bNewLibrary)
	{
		AddLibraryToSourceControl(GetFilename());
		m_bNewLibrary = false;
	}
	if (!bRes)
	{
		string strMessage;
		strMessage.Format("The file %s is read-only and the save of the library couldn't be performed. Try to remove the \"read-only\" flag or check-out the file and then try again.", GetFilename());
		CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_ERROR, strMessage.c_str());
	}
	return bRes;
}

