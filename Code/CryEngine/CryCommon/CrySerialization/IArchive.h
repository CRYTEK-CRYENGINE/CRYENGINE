// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/yasli/Archive.h>
#include "Serializer.h"

namespace Serialization{

typedef yasli::Archive       IArchive;
typedef yasli::Serializer    SStruct;
typedef yasli::Context       SContext;
typedef yasli::Context       SContextLink;
typedef yasli::TypeID        TypeID;
typedef std::vector<SStruct> SStructs;

template<class T>
constexpr bool IsDefaultSerializeable()
{
	return yasli::Helpers::IsDefaultSerializaeble<T>::value;
}

template<class T>
constexpr bool HasSerializeOverride()
{
	return yasli::Helpers::HasSerializeOverride<T>(0);
}

template<class T>
constexpr bool IsSerializeable()
{
	return IsDefaultSerializeable<T>() || HasSerializeOverride<T>();
}
}
