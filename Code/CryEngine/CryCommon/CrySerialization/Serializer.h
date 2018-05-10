// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/yasli/Serializer.h>
#include <CrySerialization/yasli/KeyValue.h>

#if defined(__GNUC__)
// Reinforce GCC's linker to properly export missing symbols (yasli::Serialize etc.)
// into CrySystem library:
	#include <CrySerialization/yasli/ClassFactory.h>
#endif

namespace Serialization
{
typedef yasli::Serializer         SStruct;
typedef yasli::ContainerInterface IContainer;
typedef yasli::PointerInterface   IPointer;
typedef yasli::KeyValueInterface  IKeyValue;
typedef yasli::StringInterface    IString;
typedef yasli::WStringInterface   IWString;
}
