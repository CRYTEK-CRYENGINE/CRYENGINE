// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IENTITYATTRIBUTESPROXY_H__
#define __IENTITYATTRIBUTESPROXY_H__

#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryEntitySystem/IEntity.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/Serializer.h>

#include <CrySchematyc2/ILib.h>

struct IEntityAttribute;
DECLARE_SHARED_POINTERS(IEntityAttribute)

//! Derive from this interface to expose custom entity properties in the editor using the serialization framework.
struct IEntityAttribute
{
	virtual ~IEntityAttribute() {}

	virtual const char*         GetName() const = 0;
	virtual const char*         GetLabel() const = 0;
	virtual void                Serialize(Serialization::IArchive& archive) = 0;
	virtual IEntityAttributePtr Clone() const = 0;
	virtual void                OnAttributeChange(IEntity* pEntity) const {}
	virtual Schematyc2::ILibClassPropertiesConstPtr GetProperties() const { return nullptr; }
};

typedef DynArray<IEntityAttributePtr> TEntityAttributeArray;

//////////////////////////////////////////////////////////////////////////
struct SEntityAttributesSerializer
{
	SEntityAttributesSerializer(TEntityAttributeArray& _attributes)
		: attributes(_attributes)
		, entityId(INVALID_ENTITYID)
	{}

	SEntityAttributesSerializer(TEntityAttributeArray& _attributes, const EntityId _entityId)
		: attributes(_attributes)
		, entityId(_entityId)
	{}

	void Serialize(Serialization::IArchive& archive)
	{
		Serialization::SContext context(archive, this);

		for (size_t iAttribute = 0, attributeCount = attributes.size(); iAttribute < attributeCount; ++iAttribute)
		{
			IEntityAttribute* pAttribute = attributes[iAttribute].get();
			if (pAttribute != NULL)
			{
				archive(*pAttribute, pAttribute->GetName(), pAttribute->GetLabel());
			}
		}
	}

	TEntityAttributeArray& attributes;
	EntityId               entityId;
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    Proxy for storage of entity attributes.
//////////////////////////////////////////////////////////////////////////
struct IEntityAttributesComponent : public IEntityComponent
{
	CRY_ENTITY_COMPONENT_INTERFACE_GUID(IEntityAttributesComponent, "807b8678-0dbf-405e-892f-24737d11cb20"_cry_guid)

	virtual void                         SetAttributes(const TEntityAttributeArray& attributes) = 0;
	virtual TEntityAttributeArray&       GetAttributes() = 0;
	virtual const TEntityAttributeArray& GetAttributes() const = 0;
};

namespace EntityAttributeUtils
{
//////////////////////////////////////////////////////////////////////////
// Description:
//    Clone array of entity attributes.
//////////////////////////////////////////////////////////////////////////
inline void CloneAttributes(const TEntityAttributeArray& src, TEntityAttributeArray& dst)
{
	dst.clear();
	dst.reserve(src.size());
	for (TEntityAttributeArray::const_iterator iAttribute = src.begin(), iEndAttribute = src.end(); iAttribute != iEndAttribute; ++iAttribute)
	{
		dst.push_back((*iAttribute)->Clone());
	}
}

//////////////////////////////////////////////////////////////////////////
// Description:
//    Find entity attribute by name.
//////////////////////////////////////////////////////////////////////////
inline IEntityAttributePtr FindAttribute(TEntityAttributeArray& attributes, const char* name)
{
	CRY_ASSERT(name != NULL);
	if (name != NULL)
	{
		for (TEntityAttributeArray::iterator iAttribute = attributes.begin(), iEndAttribute = attributes.end(); iAttribute != iEndAttribute; ++iAttribute)
		{
			if (strcmp((*iAttribute)->GetName(), name) == 0)
			{
				return *iAttribute;
			}
		}
	}
	return IEntityAttributePtr();
}

//////////////////////////////////////////////////////////////////////////
// Description:
//    Find entity attribute by name.
//////////////////////////////////////////////////////////////////////////
inline IEntityAttributeConstPtr FindAttribute(const TEntityAttributeArray& attributes, const char* name)
{
	CRY_ASSERT(name != NULL);
	if (name != NULL)
	{
		for (TEntityAttributeArray::const_iterator iAttribute = attributes.begin(), iEndAttribute = attributes.end(); iAttribute != iEndAttribute; ++iAttribute)
		{
			if (strcmp((*iAttribute)->GetName(), name) == 0)
			{
				return *iAttribute;
			}
		}
	}
	return IEntityAttributePtr();
}
}

#endif //__IENTITYATTRIBUTESPROXY_H__
