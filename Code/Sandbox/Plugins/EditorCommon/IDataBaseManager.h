// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IDataBaseManager_h__
#define __IDataBaseManager_h__
#pragma once

#include "IObjectEnumerator.h"
#include "IDataBaseLibrary.h"
#include "IDataBaseItem.h"

enum EDataBaseItemEvent
{
	EDB_ITEM_EVENT_ADD,
	EDB_ITEM_EVENT_DELETE,
	EDB_ITEM_EVENT_CHANGED, //According to usage, this is for name changes or structure changes (ex: matetrial changed into submaterial)
	EDB_ITEM_EVENT_SELECTED, //selected in database view or equivalent
	EDB_ITEM_EVENT_UPDATE_PROPERTIES, //Internal properties were updated
};

enum EDataBaseLibraryEvent
{
	EDB_LIBRARY_EVENT_DELETE,
};

enum EDataBaseEvent
{
	EDB_EVENT_CLEAR
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    Callback class to intercept item creation and deletion events.
//////////////////////////////////////////////////////////////////////////
struct IDataBaseManagerListener
{
	virtual void OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event) = 0;
	virtual void OnDataBaseLibraryEvent(IDataBaseLibrary* pLibrary, EDataBaseLibraryEvent event) {}
	virtual void OnDataBaseEvent(EDataBaseEvent event)                                           {}
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    his interface is used to enumerate al items registered to the database manager.
//////////////////////////////////////////////////////////////////////////
struct IDataBaseItemEnumerator
{
	virtual void           Release() = 0;
	virtual IDataBaseItem* GetFirst() = 0;
	virtual IDataBaseItem* GetNext() = 0;
};

//////////////////////////////////////////////////////////////////////////
//
// Interface to the collection of all items or specific type
// in data base libraries.
//
//////////////////////////////////////////////////////////////////////////
struct IDataBaseManager
{
	//! Clear all libraries.
	virtual void ClearAll() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Library items.
	//////////////////////////////////////////////////////////////////////////
	//! Make a new item in specified library.
	virtual IDataBaseItem* CreateItem(IDataBaseLibrary* pLibrary) = 0;
	//! Delete item from library and manager.
	virtual void           DeleteItem(IDataBaseItem* pItem) = 0;

	//! Find Item by its GUID.
	virtual IDataBaseItem*           FindItem(CryGUID guid) const = 0;
	virtual IDataBaseItem*           FindItemByName(const string& fullItemName) = 0;

	virtual IDataBaseItemEnumerator* GetItemEnumerator() = 0;

	// Select one item in DB.
	virtual void SetSelectedItem(IDataBaseItem* pItem) = 0;
	virtual IDataBaseItem* GetSelectedItem() const = 0;

	//////////////////////////////////////////////////////////////////////////
	// Libraries.
	//////////////////////////////////////////////////////////////////////////
	//! Add Item library. bSetFullFilename should be true for new libraries (using LibsPath)
	virtual IDataBaseLibrary* AddLibrary(const string& library, bool bSetFullFilename = false) = 0;
	virtual void              DeleteLibrary(const string& library) = 0;
	//! Get number of libraries.
	virtual int               GetLibraryCount() const = 0;
	//! Get Item library by index.
	virtual IDataBaseLibrary* GetLibrary(int index) const = 0;

	//! Find Items Library by name.
	virtual IDataBaseLibrary* FindLibrary(const string& library) = 0;

	//! Load Items library.
	virtual IDataBaseLibrary* LoadLibrary(const string& filename, bool bReload = false) = 0;

	//! Save all modified libraries.
	virtual void SaveAllLibs() = 0;

	//! Serialize property manager.
	virtual void Serialize(XmlNodeRef& node, bool bLoading) = 0;

	//! Export items to game.
	virtual void Export(XmlNodeRef& node) {};

	//! Enumerate objects in database
	virtual void EnumerateObjects(IObjectEnumerator* pEnumerator)
	{
		for (int j = 0; j < GetLibraryCount(); j++)
		{
			IDataBaseLibrary* lib = GetLibrary(j);

			for (int i = 0; i < lib->GetItemCount(); i++)
			{
				IDataBaseItem* pItem = lib->GetItem(i);
				pEnumerator->AddEntry(pItem->GetFullName(), pItem->GetGUID().ToString());
			}
		}
	};

	//! Returns unique name base on input name.
	virtual string MakeUniqItemName(const string& name) = 0;
	virtual string MakeFullItemName(IDataBaseLibrary* pLibrary, const string& group, const string& itemName) = 0;

	//! Root node where this library will be saved.
	virtual string GetRootNodeName() = 0;
	//! Path to libraries in this manager.
	virtual string GetLibsPath() = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Validate library items for errors.
	virtual void Validate() = 0;

	// Description:
	//		Collects names of all resource files used by managed items.
	// Arguments:
	//		resources - Structure where all filenames are collected.
	virtual void GatherUsedResources(CUsedResources& resources) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Register listeners.
	virtual void AddListener(IDataBaseManagerListener* pListener) = 0;
	virtual void RemoveListener(IDataBaseManagerListener* pListener) = 0;
};

#endif // __IDataBaseManager_h__

