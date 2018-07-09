// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
//
// Serialization::EntityLink provides a decorator to expose entity reference
// GUID as a UI element that allows you to pick entities.
//
// Example of usage:
//
//    GUID m_entityGuid;
//
//    void MyType::Serialize(IArchive& ar)
//    {
//      ar(Serialization::EntityLink(m_referenceGuid), "reference", "Reference");
//    }
//

#include <CryExtension/CryGUID.h>

namespace Serialization
{

// Helper to serialize an entity GUID with name
// Previously used for entity links, but now only applies to any Entity GUID with a name (e.g. area target)
struct EntityTarget
{
	CryGUID guid;
	string  name;

	// Default constructor for yasli
	EntityTarget()
		: guid(CryGUID::Null())
	{
	}

	EntityTarget(const CryGUID& guid, string name)
		: guid(guid)
		, name(name)
	{

	}

	bool Serialize(Serialization::IArchive& ar)
	{
		if (ar.isEdit())
			return ar(name, "targetName", "!Target");
		else
			return ar(guid, "targetName", "!Target");
	}
};

// same as EntityTarget really, we just want different decorator
struct PrefabLink : public EntityTarget
{
	CryGUID owner_guid_;

	PrefabLink()
		: EntityTarget()
		, owner_guid_(CryGUID::Null())
	{
	}

	PrefabLink(const CryGUID& guid, string name, const CryGUID& ownerid)
		: EntityTarget(guid, name)
		, owner_guid_(ownerid)
	{
	}
};

inline bool Serialize(Serialization::IArchive& ar, PrefabLink& link, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serialization::SStruct::forEdit(link), name, label);
	else
		return ar(link.guid, name, label);
}

}

