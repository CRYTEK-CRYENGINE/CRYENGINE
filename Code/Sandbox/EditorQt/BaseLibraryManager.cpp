// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseLibraryManager.h"

#include "BaseLibrary.h"
#include "BaseLibraryItem.h"

//////////////////////////////////////////////////////////////////////////
// CBaseLibraryManager implementation.
//////////////////////////////////////////////////////////////////////////
CBaseLibraryManager::CBaseLibraryManager()
{
	m_bUniqNameMap = false;
	m_bUniqGuidMap = true;
	GetIEditorImpl()->RegisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryManager::~CBaseLibraryManager()
{
	ClearAll();
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::ClearAll()
{
	// Delete all items from all libraries.
	for (int i = 0; i < m_libs.size(); i++)
	{
		m_libs[i]->RemoveAllItems();
	}
	NotifyDatabaseEvent(EDB_EVENT_CLEAR);

	// if we will not copy maps locally then destructors of the elements of
	// the map will operate on the already invalid map object
	// see:
	//  CBaseLibraryManager::UnregisterItem()
	//  CBaseLibraryManager::DeleteItem()
	//  CMaterial::~CMaterial()
	ItemsGUIDMap itemsGuidMap = m_itemsGuidMap;
	ItemsNameMap itemsNameMap = m_itemsNameMap;

	m_itemsGuidMap.clear();
	m_itemsNameMap.clear();
	m_libs.clear();
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryManager::FindLibrary(const string& library)
{
	for (int i = 0; i < m_libs.size(); i++)
	{
		if (stricmp(library, m_libs[i]->GetName()) == 0 || stricmp(library, m_libs[i]->GetFilename()) == 0)
		{
			return m_libs[i];
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::FindItem(CryGUID guid) const
{
	if (!m_bUniqGuidMap)
		return nullptr;

	CBaseLibraryItem* pMtl = stl::find_in_map(m_itemsGuidMap, guid, (CBaseLibraryItem*)0);
	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::SplitFullItemName(const string& fullItemName, string& libraryName, string& itemName)
{
	int p;
	p = fullItemName.Find('.');
	if (p < 0 || !stricmp(fullItemName.Mid(p + 1), "mtl"))
	{
		libraryName = "";
		itemName = fullItemName;
		return;
	}
	libraryName = fullItemName.Mid(0, p);
	itemName = fullItemName.Mid(p + 1);
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::FindItemByName(const string& fullItemName)
{
	if (!m_bUniqNameMap)
		return nullptr;

	return stl::find_in_map(m_itemsNameMap, fullItemName, 0);
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::LoadItemByName(const string& fullItemName)
{
	string libraryName, itemName;
	SplitFullItemName(fullItemName, libraryName, itemName);

	if (!FindLibrary(libraryName))
	{
		LoadLibrary(MakeFilename(libraryName));
	}
	return stl::find_in_map(m_itemsNameMap, fullItemName, 0);
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::FindItemByName(const char* fullItemName)
{
	return FindItemByName(string(fullItemName));
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::LoadItemByName(const char* fullItemName)
{
	return LoadItemByName(string(fullItemName));
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::CreateItem(IDataBaseLibrary* pLibrary)
{
	assert(pLibrary);

	// Add item to this library.
	TSmartPtr<CBaseLibraryItem> pItem = MakeNewItem();
	pLibrary->AddItem(pItem);
	return pItem;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::DeleteItem(IDataBaseItem* pItem)
{
	assert(pItem);

	UnregisterItem((CBaseLibraryItem*)pItem);
	if (pItem->GetLibrary())
	{
		pItem->GetLibrary()->RemoveItem(pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryManager::LoadLibrary(const string& inFilename, bool bReload)
{
	string filename = inFilename;
	// If library is already loaded ignore it.
	for (int i = 0; i < m_libs.size(); i++)
	{
		if (stricmp(filename, m_libs[i]->GetFilename()) == 0 || stricmp(inFilename, m_libs[i]->GetFilename()) == 0)
		{
			Error(_T("Loading Duplicate Library: %s"), (const char*)filename);
			return 0;
		}
	}

	TSmartPtr<CBaseLibrary> pLib = MakeNewLibrary();
	if (!pLib->Load(filename))
	{
		Error(_T("Failed to Load Item Library: %s"), (const char*)filename);
		return 0;
	}
	if (FindLibrary(pLib->GetName()) != 0)
	{
		Error(_T("Loading Duplicate Library: %s"), (const char*)pLib->GetName());
		return 0;
	}
	m_libs.push_back(pLib);
	return pLib;
}

//////////////////////////////////////////////////////////////////////////
int CBaseLibraryManager::GetModifiedLibraryCount() const
{
	int count = 0;
	for (int i = 0; i < m_libs.size(); i++)
	{
		if (m_libs[i]->IsModified())
			count++;
	}
	return count;
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryManager::AddLibrary(const string& library, bool bSetFullFilename)
{
	// Check if library with same name already exist.
	IDataBaseLibrary* pBaseLib = FindLibrary(library);
	if (pBaseLib)
		return pBaseLib;

	CBaseLibrary* lib = MakeNewLibrary();
	lib->SetName(library);

	// Set filename of this library.
	// Make a filename from name of library.
	string filename = library;
	filename.Replace(' ', '_');
	// according to timur doesnt matter if by this time the libs path is set or not.
	// therefore it will be delayed later in order to get the proper game path when overriding.
	if (bSetFullFilename)
	{
		filename = GetLibsPath() + filename + ".xml";
	}
	else
	{
		filename = filename + ".xml";
	}
	lib->SetFilename(filename);
	lib->SetModified(false);

	m_libs.push_back(lib);
	return lib;
}

//////////////////////////////////////////////////////////////////////////
string CBaseLibraryManager::MakeFilename(const string& library)
{
	string filename = library;
	filename.Replace(' ', '_');
	return GetLibsPath() + filename + ".xml";
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::DeleteLibrary(const string& library)
{
	for (int i = 0; i < m_libs.size(); i++)
	{
		if (stricmp(library, m_libs[i]->GetName()) == 0)
		{
			CBaseLibrary* pLibrary = m_libs[i];
			// Check if not level library, they cannot be deleted.
			if (!pLibrary->IsLevelLibrary())
			{
				NotifyLibraryEvent(pLibrary, EDB_LIBRARY_EVENT_DELETE);
				for (int j = 0; j < pLibrary->GetItemCount(); j++)
				{
					UnregisterItem((CBaseLibraryItem*)pLibrary->GetItem(j));
				}
				pLibrary->RemoveAllItems();
				m_libs.erase(m_libs.begin() + i);
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryManager::GetLibrary(int index) const
{
	assert(index >= 0 && index < m_libs.size());
	return m_libs[index];
};

//////////////////////////////////////////////////////////////////////////
IDataBaseLibrary* CBaseLibraryManager::GetLevelLibrary() const
{
	IDataBaseLibrary* pLevelLib = NULL;

	for (int i = 0; i < GetLibraryCount(); i++)
	{
		if (GetLibrary(i)->IsLevelLibrary())
		{
			pLevelLib = GetLibrary(i);
			break;
		}
	}

	return pLevelLib;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::SaveAllLibs()
{
	for (int i = 0; i < GetLibraryCount(); i++)
	{
		// Check if library is modified.
		IDataBaseLibrary* pLibrary = GetLibrary(i);
		if (pLibrary->IsLevelLibrary())
			continue;
		if (pLibrary->IsModified())
		{
			if (pLibrary->Save())
			{
				pLibrary->SetModified(false);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::Serialize(XmlNodeRef& node, bool bLoading)
{
	LOADING_TIME_PROFILE_SECTION;
	string rootNodeName = GetRootNodeName();
	if (bLoading)
	{
		XmlNodeRef libs = node->findChild(rootNodeName);
		if (libs)
		{
			for (int i = 0; i < libs->getChildCount(); i++)
			{
				// Load only library name.
				XmlNodeRef libNode = libs->getChild(i);
				if (strcmp(libNode->getTag(), "LevelLibrary") == 0)
				{
					m_pLevelLibrary->Serialize(libNode, bLoading);
				}
				else
				{
					string libName;
					if (libNode->getAttr("Name", libName))
					{
						// Load this library.
						if (!FindLibrary(libName))
						{
							LoadLibrary(MakeFilename(libName));
						}
					}
				}
			}
		}
	}
	else
	{
		// Save all libraries.
		XmlNodeRef libs = node->newChild(rootNodeName);
		for (int i = 0; i < GetLibraryCount(); i++)
		{
			IDataBaseLibrary* pLib = GetLibrary(i);
			if (pLib->IsLevelLibrary())
			{
				// Level libraries are saved in in level.
				XmlNodeRef libNode = libs->newChild("LevelLibrary");
				pLib->Serialize(libNode, bLoading);
			}
			else
			{
				// Save only library name.
				XmlNodeRef libNode = libs->newChild("Library");
				libNode->setAttr("Name", pLib->GetName());
			}
		}
		SaveAllLibs();
	}
}

//////////////////////////////////////////////////////////////////////////
string CBaseLibraryManager::MakeUniqItemName(const string& srcName)
{
	// Remove all numbers from the end of name.
	string typeName = srcName;
	int len = typeName.GetLength();
	while (len > 0 && isdigit(typeName[len - 1]))
		len--;

	typeName = typeName.Left(len);

	string tpName = typeName;
	int num = 0;

	IDataBaseItemEnumerator* pEnum = GetItemEnumerator();
	for (IDataBaseItem* pItem = pEnum->GetFirst(); pItem != NULL; pItem = pEnum->GetNext())
	{
		const char* name = pItem->GetName();
		if (strncmp(name, tpName, len) == 0)
		{
			int n = atoi(name + len) + 1;
			num = std::max(num, n);
		}
	}
	pEnum->Release();
	if (num)
	{
		string str;
		str.Format("%s%d", (const char*)typeName, num);
		return str;
	}
	else
		return typeName;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::Validate()
{
	IDataBaseItemEnumerator* pEnum = GetItemEnumerator();
	for (IDataBaseItem* pItem = pEnum->GetFirst(); pItem != NULL; pItem = pEnum->GetNext())
	{
		pItem->Validate();
	}
	pEnum->Release();
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::RegisterItem(CBaseLibraryItem* pItem, CryGUID newGuid)
{
	assert(pItem);

	bool bNotify = false;

	if (m_bUniqGuidMap)
	{
		bool bNewItem = true;
		CryGUID oldGuid = pItem->GetGUID();
		if (oldGuid != CryGUID::Null())
		{
			bNewItem = false;
			m_itemsGuidMap.erase(oldGuid);
			NotifyItemEvent(pItem, EDB_ITEM_EVENT_DELETE);
		}
		if (newGuid == CryGUID::Null())
			return;
		CBaseLibraryItem* pOldItem = stl::find_in_map(m_itemsGuidMap, newGuid, (CBaseLibraryItem*)0);
		if (!pOldItem)
		{
			pItem->m_guid = newGuid;
			m_itemsGuidMap[newGuid] = pItem;
			pItem->m_bRegistered = true;
			bNotify = true;
		}
		else
		{
			if (pOldItem != pItem)
			{
				ReportDuplicateItem(pItem, pOldItem);
			}
		}
	}

	if (m_bUniqNameMap)
	{
		string fullName = pItem->GetFullName();
		if (!pItem->GetName().IsEmpty())
		{
			CBaseLibraryItem* pOldItem = stl::find_in_map(m_itemsNameMap, fullName, (CBaseLibraryItem*)0);
			if (!pOldItem)
			{
				m_itemsNameMap[fullName] = pItem;
				pItem->m_bRegistered = true;
				bNotify = true;
			}
			else
			{
				if (pOldItem != pItem)
				{
					ReportDuplicateItem(pItem, pOldItem);
				}
			}
		}
	}

	// Notify listeners.
	if (bNotify)
		NotifyItemEvent(pItem, EDB_ITEM_EVENT_ADD);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::RegisterItem(CBaseLibraryItem* pItem)
{
	assert(pItem);

	bool bNotify = false;

	if (m_bUniqGuidMap)
	{
		if (pItem->GetGUID() == CryGUID::Null())
			return;
		CBaseLibraryItem* pOldItem = stl::find_in_map(m_itemsGuidMap, pItem->GetGUID(), (CBaseLibraryItem*)0);
		if (!pOldItem)
		{
			m_itemsGuidMap[pItem->GetGUID()] = pItem;
			pItem->m_bRegistered = true;
			bNotify = true;
		}
		else
		{
			if (pOldItem != pItem)
			{
				ReportDuplicateItem(pItem, pOldItem);
			}
		}
	}

	if (m_bUniqNameMap)
	{
		string fullName = pItem->GetFullName();
		if (!fullName.IsEmpty())
		{
			CBaseLibraryItem* pOldItem = stl::find_in_map(m_itemsNameMap, fullName, (CBaseLibraryItem*)0);
			if (!pOldItem)
			{
				m_itemsNameMap[fullName] = pItem;
				pItem->m_bRegistered = true;
				bNotify = true;
			}
			else
			{
				if (pOldItem != pItem)
				{
					ReportDuplicateItem(pItem, pOldItem);
				}
			}
		}
	}

	// Notify listeners.
	if (bNotify)
		NotifyItemEvent(pItem, EDB_ITEM_EVENT_ADD);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::SetRegisteredFlag(CBaseLibraryItem* pItem, bool bFlag)
{
	pItem->m_bRegistered = bFlag;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::ReportDuplicateItem(CBaseLibraryItem* pItem, CBaseLibraryItem* pOldItem)
{
	string sLibName;
	if (pOldItem->GetLibrary())
		sLibName = pOldItem->GetLibrary()->GetName();

	CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Item %s with duplicate GUID to loaded item %s ignored", (const char*)pItem->GetFullName(), (const char*)pOldItem->GetFullName());
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::UnregisterItem(CBaseLibraryItem* pItem)
{
	// Notify listeners.
	NotifyItemEvent(pItem, EDB_ITEM_EVENT_DELETE);

	if (!pItem)
		return;

	if (m_bUniqGuidMap)
		m_itemsGuidMap.erase(pItem->GetGUID());
	if (m_bUniqNameMap && !pItem->GetFullName().IsEmpty())
	{
		auto findIter = m_itemsNameMap.find(pItem->GetFullName());
		if (findIter != m_itemsNameMap.end())
		{
			_smart_ptr<CBaseLibraryItem> item = findIter->second;
			m_itemsNameMap.erase(findIter);
		}
	}

	pItem->m_bRegistered = false;
}

//////////////////////////////////////////////////////////////////////////
string CBaseLibraryManager::MakeFullItemName(IDataBaseLibrary* pLibrary, const string& group, const string& itemName)
{
	assert(pLibrary);
	string name = pLibrary->GetName() + ".";
	if (!group.IsEmpty())
		name += group + ".";
	name += itemName;
	return name;
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::GatherUsedResources(CUsedResources& resources)
{
	IDataBaseItemEnumerator* pEnum = GetItemEnumerator();
	for (IDataBaseItem* pItem = pEnum->GetFirst(); pItem != NULL; pItem = pEnum->GetNext())
	{
		pItem->GatherUsedResources(resources);
	}
	pEnum->Release();
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItemEnumerator* CBaseLibraryManager::GetItemEnumerator()
{
	if (m_bUniqNameMap)
		return new CDataBaseItemEnumerator<ItemsNameMap>(&m_itemsNameMap);
	else
		return new CDataBaseItemEnumerator<ItemsGUIDMap>(&m_itemsGuidMap);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginNewScene:
		SetSelectedItem(0);
		ClearAll();
		break;
	case eNotify_OnBeginSceneOpen:
		SetSelectedItem(0);
		ClearAll();
		break;
	case eNotify_OnMissionChange:
		SetSelectedItem(0);
		break;
	case eNotify_OnClearLevelContents:
		SetSelectedItem(0);
		ClearAll();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::OnRenameItem(CBaseLibraryItem* pItem, const string& oldName)
{
	if (m_bUniqNameMap)
	{
		if (!oldName.IsEmpty())
		{
			m_itemsNameMap.erase(oldName);
		}
		if (!pItem->GetFullName().IsEmpty())
		{
			m_itemsNameMap[pItem->GetFullName()] = pItem;
		}
	}
	OnItemChanged(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::AddListener(IDataBaseManagerListener* pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::RemoveListener(IDataBaseManagerListener* pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::NotifyItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	// Notify listeners.
	if (!m_listeners.empty())
	{
		for (int i = 0; i < m_listeners.size(); i++)
			m_listeners[i]->OnDataBaseItemEvent(pItem, event);
	}
}

void CBaseLibraryManager::NotifyLibraryEvent(IDataBaseLibrary* pLibrary, EDataBaseLibraryEvent event)
{
	for (int i = 0; i < m_listeners.size(); ++i)
	{
		m_listeners[i]->OnDataBaseLibraryEvent(pLibrary, event);
	}
}

void CBaseLibraryManager::NotifyDatabaseEvent(EDataBaseEvent event)
{
	// Notify listeners.
	if (!m_listeners.empty())
	{
		for (int i = 0; i < m_listeners.size(); i++)
			m_listeners[i]->OnDataBaseEvent(event);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::OnItemChanged(IDataBaseItem* pItem)
{
	NotifyItemEvent(pItem, EDB_ITEM_EVENT_CHANGED);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::OnUpdateProperties(IDataBaseItem* pItem)
{
	NotifyItemEvent(pItem, EDB_ITEM_EVENT_UPDATE_PROPERTIES);
}

//////////////////////////////////////////////////////////////////////////
void CBaseLibraryManager::SetSelectedItem(IDataBaseItem* pItem)
{
	if (m_pSelectedItem == pItem)
		return;
	m_pSelectedItem = (CBaseLibraryItem*)pItem;
	NotifyItemEvent(m_pSelectedItem, EDB_ITEM_EVENT_SELECTED);
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::GetSelectedItem() const
{
	return m_pSelectedItem;
}

//////////////////////////////////////////////////////////////////////////
IDataBaseItem* CBaseLibraryManager::GetSelectedParentItem() const
{
	return m_pSelectedParent;
}

