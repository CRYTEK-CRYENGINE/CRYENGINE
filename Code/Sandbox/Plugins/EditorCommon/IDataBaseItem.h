// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IDataBaseItem_h__
#define __IDataBaseItem_h__
#pragma once

#include "IEditor.h"
#include <CryExtension/CryGUID.h>

struct IDataBaseLibrary;
class CUsedResources;

//////////////////////////////////////////////////////////////////////////
/** Base class for all items contained in BaseLibraray.
 */
struct IDataBaseItem
{
	struct SerializeContext
	{
		XmlNodeRef node;
		bool       bUndo;
		bool       bLoading;
		bool       bCopyPaste;
		bool       bIgnoreChilds;
		bool       bUniqName;
		SerializeContext() : node(0), bLoading(false), bCopyPaste(false), bIgnoreChilds(false), bUniqName(false), bUndo(false) {};
		SerializeContext(XmlNodeRef _node, bool bLoad) : node(_node), bLoading(bLoad), bCopyPaste(false), bIgnoreChilds(false), bUniqName(false), bUndo(false) {};
		SerializeContext(const SerializeContext& ctx) : node(ctx.node), bLoading(ctx.bLoading),
			bCopyPaste(ctx.bCopyPaste), bIgnoreChilds(ctx.bIgnoreChilds), bUniqName(ctx.bUniqName), bUndo(ctx.bUndo) {};
	};

	virtual EDataBaseItemType GetType() const = 0;

	//! Return Library this item are contained in.
	//! Item can only be at one library.
	virtual IDataBaseLibrary* GetLibrary() const = 0;

	//! Change item name.
	virtual void           SetName(const string& name) = 0;
	//! Get item name.
	virtual const string& GetName() const = 0;

	//! Get full item name, including name of library.
	//! Name formed by adding dot after name of library
	//! eg. library Pickup and item PickupRL form full item name: "Pickups.PickupRL".
	virtual string GetFullName() const = 0;

	//! Get only nameof group from prototype.
	virtual string GetGroupName() = 0;
	//! Get short name of prototype without group.
	virtual string GetShortName() = 0;

	//! Serialize library item to archive.
	virtual void Serialize(SerializeContext& ctx) = 0;

	//! Generate new unique id for this item.
	virtual void    GenerateId() = 0;
	//! Returns GUID of this material.
	virtual CryGUID GetGUID() const = 0;

	//! Validate item for errors.
	virtual void Validate() {};

	//! Gathers resources by this item.
	virtual void GatherUsedResources(CUsedResources& resources) {};
};

#endif // __IDataBaseItem_h__

