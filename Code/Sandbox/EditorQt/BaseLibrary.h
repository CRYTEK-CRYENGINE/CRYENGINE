// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __baselibrary_h__
#define __baselibrary_h__
#pragma once

#include "SandboxAPI.h"
#include "IDataBaseLibrary.h"
#include "BaseLibraryItem.h"

class CBaseLibraryManager;
class CBaseLibraryItem;

/** This a base class for all Libraries used by Editor.
 */
class SANDBOX_API CBaseLibrary : public _i_reference_target_t, public IDataBaseLibrary
{
public:
	CBaseLibrary(CBaseLibraryManager* pManager);
	~CBaseLibrary();

	//! Set library name.
	virtual void   SetName(const string& name);
	//! Get library name.
	const string& GetName() const;

	//! Set new filename for this library.
	void           SetFilename(const string& filename) { m_filename = filename; m_filename.MakeLower(); };
	const string& GetFilename() const                  { return m_filename; };

	virtual bool   Save() = 0;
	virtual bool   Load(const string& filename) = 0;
	virtual void   Serialize(XmlNodeRef& node, bool bLoading) = 0;

	//! Mark library as modified.
	void SetModified(bool bModified = true);
	//! Check if library was modified.
	bool IsModified() const { return m_bModified; };

	//////////////////////////////////////////////////////////////////////////
	// Working with items.
	//////////////////////////////////////////////////////////////////////////
	//! Add a new prototype to library.
	void           AddItem(IDataBaseItem* item, bool bRegister = true);
	//! Get number of known prototypes.
	int            GetItemCount() const { return m_items.size(); }
	//! Get prototype by index.
	IDataBaseItem* GetItem(int index);

	//! Delete item by pointer of item.
	void RemoveItem(IDataBaseItem* item);

	//! Delete all items from library.
	void RemoveAllItems();

	//! Find library item by name.
	//! Using linear search.
	IDataBaseItem* FindItem(const string& name);

	//! Check if this library is local level library.
	bool IsLevelLibrary() const { return m_bLevelLib; };

	//! Set library to be level library.
	void SetLevelLibrary(bool bEnable) { m_bLevelLib = bEnable; };

	//////////////////////////////////////////////////////////////////////////
	//! Return manager for this library.
	IDataBaseManager* GetManager();

	// Saves the library with the main tag defined by the parameter name
	bool SaveLibrary(const char* name);

	// Store Undo for some operation
	void StoreUndo(const string& description);

private:
	// Add the library to the source control
	bool AddLibraryToSourceControl(const string& name) const;

protected:

	//! Name of the library.
	string m_name;
	//! Filename of the library.
	string m_filename;

	//! Flag set when library was modified.
	bool m_bModified;

	// Flag set when the library is just created and it's not yet saved for the first time.
	bool m_bNewLibrary;

	//! Level library is saved within the level .cry file and is local for this level.
	bool m_bLevelLib;

	//////////////////////////////////////////////////////////////////////////
	// Manager.
	CBaseLibraryManager* m_pManager;

	// Array of all our library items.
	std::vector<_smart_ptr<CBaseLibraryItem>> m_items;
};

#endif // __baselibrary_h__

