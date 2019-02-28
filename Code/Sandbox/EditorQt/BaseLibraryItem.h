// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __baselibraryitem_h__
#define __baselibraryitem_h__
#pragma once

#include "SandboxAPI.h"
#include "IDataBaseItem.h"
#include "BaseLibrary.h"

class CBaseLibraryItem;
class CBaseLibrary;

//////////////////////////////////////////////////////////////////////////
/** Base class for all items contained in BaseLibraray.
 */
class SANDBOX_API CBaseLibraryItem : public _i_reference_target_t, public IDataBaseItem
{
public:
	CBaseLibraryItem();
	~CBaseLibraryItem();

	//! Set item name.
	//! Its virtual, in case you want to override it in derrived item.
	virtual void SetName(const string& name)
	{
		SetName(name, false);
	}
	//! Set item name.
	inline void SetName(const char* name) // for CString conversion
	{
		SetName(name, false);
	}
	//! Get item name.
	const string& GetName() const;

	//! Get full item name, including name of library.
	//! Name formed by adding dot after name of library
	//! eg. library Pickup and item PickupRL form full item name: "Pickups.PickupRL".
	string GetFullName() const;

	//! Get only nameof group from prototype.
	string GetGroupName();
	//! Get short name of prototype without group.
	string GetShortName();

	//! Return Library this item are contained in.
	//! Item can only be at one library.
	IDataBaseLibrary* GetLibrary() const;
	void              SetLibrary(CBaseLibrary* pLibrary);

	//////////////////////////////////////////////////////////////////////////
	//! Serialize library item to archive.
	virtual void Serialize(SerializeContext& ctx);

	//////////////////////////////////////////////////////////////////////////
	//! Generate new unique id for this item.
	void    GenerateId();
	//! Returns GUID of this material.
	CryGUID GetGUID() const { return m_guid; }

	//! Mark library as modified.
	void SetModified(bool bModified = true);
	//! Check if library was modified.
	bool IsModified() const { return m_bModified; };

	//! Validate item for errors.
	virtual void Validate() {};

	//////////////////////////////////////////////////////////////////////////
	//! Gathers resources by this item.
	virtual void GatherUsedResources(CUsedResources& resources) {};

protected:

	void SetName(const string& name, bool bSkipUndo);
	void SetGUID(CryGUID guid);
	friend class CBaseLibrary;
	friend class CBaseLibraryManager;
	// Name of this prototype.
	string                  m_name;
	//! Reference to prototype library who contains this prototype.
	_smart_ptr<CBaseLibrary> m_library;

	//! Every base library item have unique id.
	CryGUID m_guid;
	// True when item modified by editor.
	bool m_bModified;
	// True when item registered in manager.
	bool m_bRegistered;
};

TYPEDEF_AUTOPTR(CBaseLibraryItem);

#endif // __baselibraryitem_h__

