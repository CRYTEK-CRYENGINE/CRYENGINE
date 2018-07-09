// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>

struct TagListContainer : Serialization::ContainerSTL<std::vector<string>, string>
{
	TagListContainer(TagList& tagList)
		: ContainerSTL(tagList.tags)
	{
	}

	Serialization::TypeID containerType() const override { return Serialization::TypeID::get<TagList>(); };
};

inline bool Serialize(Serialization::IArchive& ar, TagList& tagList, const char* name, const char* label)
{
	TagListContainer container(tagList);
	return ar(static_cast<Serialization::IContainer&>(container), name, label);
}
