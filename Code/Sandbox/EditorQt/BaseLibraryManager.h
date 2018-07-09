// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __baselibrarymanager_h__
#define __baselibrarymanager_h__
#pragma once

#include "IDataBaseItem.h"
#include "IDataBaseLibrary.h"
#include "IDataBaseManager.h"
#include <CryCore/ToolsHelpers/GuidUtil.h>
#include "BaseLibrary.h"
#include <CryExtension/CryGUID.h>

class CBaseLibraryItem;
class CBaseLibrary;

/** Manages all Libraries and Items.
 */
class SANDBOX_API CBaseLibraryManager : public _i_reference_target_t, public IDataBaseManager, public IEditorNotifyListener
{
public:
	CBaseLibraryManager();
	~CBaseLibraryManager();

	//! Clear all libraries.
	virtual void ClearAll();

	//////////////////////////////////////////////////////////////////////////
	// IDocListener implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event);

	//////////////////////////////////////////////////////////////////////////
	// Library items.
	//////////////////////////////////////////////////////////////////////////
	//! Make a new item in specified library.
	virtual IDataBaseItem* CreateItem(IDataBaseLibrary* pLibrary);
	//! Delete item from library and manager.
	virtual void           DeleteItem(IDataBaseItem* pItem);

	//! Find Item by its GUID.
	virtual IDataBaseItem*           FindItem(CryGUID guid) const;
	virtual IDataBaseItem*           FindItemByName(const string& fullItemName);
	virtual IDataBaseItem*           LoadItemByName(const string& fullItemName);
	virtual IDataBaseItem*           FindItemByName(const char* fullItemName);
	virtual IDataBaseItem*           LoadItemByName(const char* fullItemName);

	virtual IDataBaseItemEnumerator* GetItemEnumerator();

	//////////////////////////////////////////////////////////////////////////
	// Set item currently selected.
	virtual void           SetSelectedItem(IDataBaseItem* pItem) override;
	// Get currently selected item.
	virtual IDataBaseItem* GetSelectedItem() const override;
	virtual IDataBaseItem* GetSelectedParentItem() const;

	//////////////////////////////////////////////////////////////////////////
	// Libraries.
	//////////////////////////////////////////////////////////////////////////
	//! Add Item library.
	virtual IDataBaseLibrary* AddLibrary(const string& library, bool bSetFullFilename = false);
	inline IDataBaseLibrary*  AddLibrary(const char* library, bool bSetFullFilename = false) { return AddLibrary(string(library), bSetFullFilename); } // for CString conversion
	virtual void              DeleteLibrary(const string& library);
	inline void				  DeleteLibrary(const char* library) { return DeleteLibrary(string(library)); } // for CString conversion
	//! Get number of libraries.
	virtual int               GetLibraryCount() const { return m_libs.size(); };
	//! Get number of modified libraries.
	virtual int               GetModifiedLibraryCount() const;

	//! Get Item library by index.
	virtual IDataBaseLibrary* GetLibrary(int index) const;

	//! Get Level Item library.
	virtual IDataBaseLibrary* GetLevelLibrary() const;

	//! Find Items Library by name.
	virtual IDataBaseLibrary* FindLibrary(const string& library);

	//! Find Items Library by name.
	inline IDataBaseLibrary* FindLibrary(const char* library) { return FindLibrary(string(library)); } // for CString conversion

	//! Load Items library.
	virtual IDataBaseLibrary* LoadLibrary(const string& filename, bool bReload = false);

	//! Load Items library.
	inline IDataBaseLibrary* LoadLibrary(const char* filename, bool bReload = false) { return LoadLibrary(string(filename), bReload); } // for CString conversion

	//! Save all modified libraries.
	virtual void SaveAllLibs();

	//! Serialize property manager.
	virtual void Serialize(XmlNodeRef& node, bool bLoading);

	//! Export items to game.
	virtual void Export(XmlNodeRef& node) {};

	//! Returns unique name base on input name.
	virtual string MakeUniqItemName(const string& name);
	virtual string MakeFullItemName(IDataBaseLibrary* pLibrary, const string& group, const string& itemName);

	//! Root node where this library will be saved.
	virtual string GetRootNodeName() = 0;
	//! Path to libraries in this manager.
	virtual string GetLibsPath() = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Validate library items for errors.
	void Validate();

	//////////////////////////////////////////////////////////////////////////
	void         GatherUsedResources(CUsedResources& resources);

	virtual void AddListener(IDataBaseManagerListener* pListener);
	virtual void RemoveListener(IDataBaseManagerListener* pListener);

	//////////////////////////////////////////////////////////////////////////
	virtual void RegisterItem(CBaseLibraryItem* pItem, CryGUID newGuid);
	virtual void RegisterItem(CBaseLibraryItem* pItem);
	virtual void UnregisterItem(CBaseLibraryItem* pItem);

	// Only Used internally.
	void OnRenameItem(CBaseLibraryItem* pItem, const string& oldName);

	// Called by items to indicated that they have been modified.
	// Sends item changed event to listeners.
	void    OnItemChanged(IDataBaseItem* pItem);
	void    OnUpdateProperties(IDataBaseItem* pItem);

	string MakeFilename(const string& library);

	bool HasUidMap() const { return m_bUniqGuidMap; }
	bool HasNameMap() const { return m_bUniqNameMap; }

protected:
	void SplitFullItemName(const string& fullItemName, string& libraryName, string& itemName);
	void NotifyItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event);
	void NotifyLibraryEvent(IDataBaseLibrary* pLibrary, EDataBaseLibraryEvent event);
	void NotifyDatabaseEvent(EDataBaseEvent event);
	void SetRegisteredFlag(CBaseLibraryItem* pItem, bool bFlag);

	//////////////////////////////////////////////////////////////////////////
	// Must be overriden.
	//! Makes a new Item.
	virtual CBaseLibraryItem* MakeNewItem() = 0;
	virtual CBaseLibrary*     MakeNewLibrary() = 0;
	//////////////////////////////////////////////////////////////////////////

	virtual void ReportDuplicateItem(CBaseLibraryItem* pItem, CBaseLibraryItem* pOldItem);

protected:
	bool m_bUniqGuidMap;
	bool m_bUniqNameMap;

	//! Array of all loaded entity items libraries.
	std::vector<_smart_ptr<CBaseLibrary>> m_libs;

	// There is always one current level library.
	_smart_ptr<CBaseLibrary> m_pLevelLibrary;

	// GUID to item map.
	typedef std::map<CryGUID, _smart_ptr<CBaseLibraryItem>> ItemsGUIDMap;
	ItemsGUIDMap m_itemsGuidMap;

	// Case insensitive name to items map.
	typedef std::map<string, _smart_ptr<CBaseLibraryItem>, stl::less_stricmp<string>> ItemsNameMap;
	ItemsNameMap                           m_itemsNameMap;

	std::vector<IDataBaseManagerListener*> m_listeners;

	// Currently selected item.
	_smart_ptr<CBaseLibraryItem> m_pSelectedItem;
	_smart_ptr<CBaseLibraryItem> m_pSelectedParent;
};

//////////////////////////////////////////////////////////////////////////
template<class TMap>
class CDataBaseItemEnumerator : public IDataBaseItemEnumerator
{
	TMap* m_pMap;
	typename TMap::iterator m_iterator;

public:
	CDataBaseItemEnumerator(TMap* pMap)
	{
		assert(pMap);
		m_pMap = pMap;
		m_iterator = m_pMap->begin();
	}
	virtual void           Release() { delete this; };
	virtual IDataBaseItem* GetFirst()
	{
		m_iterator = m_pMap->begin();
		if (m_iterator == m_pMap->end())
			return 0;
		return m_iterator->second;
	}
	virtual IDataBaseItem* GetNext()
	{
		if (m_iterator != m_pMap->end())
			m_iterator++;
		if (m_iterator == m_pMap->end())
			return 0;
		return m_iterator->second;
	}
};

#endif // __baselibrarymanager_h__

