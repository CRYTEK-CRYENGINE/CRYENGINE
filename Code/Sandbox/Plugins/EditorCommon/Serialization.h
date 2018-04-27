// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Color.h>

#include <CrySerialization/Forward.h>

#include <CrySerialization/STL.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/Decorators/BitFlags.h>
#include <CrySerialization/Decorators/Range.h>
#include <Serialization/Decorators/ToggleButton.h>
#include <Serialization/Qt.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/SmartPtr.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/IArchiveHost.h>

using Serialization::IArchive;
using Serialization::BitFlags;

#if 0 // useful for debugging
	#include <CrySerialization/yasli/JSONArchive.h>
typedef yasli::JSONOArchive MemoryOArchive;
typedef yasli::JSONIArchive MemoryIArchive;
#else
	#include <CrySerialization/yasli/BinArchive.h>
typedef yasli::BinOArchive MemoryOArchive;
typedef yasli::BinIArchive MemoryIArchive;
#endif

namespace Serialization {

void EDITOR_COMMON_API SerializeToMemory(std::vector<char>* buffer, const SStruct& obj);
void EDITOR_COMMON_API SerializeToMemory(DynArray<char>* buffer, const SStruct& obj);
void EDITOR_COMMON_API SerializeFromMemory(const SStruct& outObj, const std::vector<char>& buffer);
void EDITOR_COMMON_API SerializeFromMemory(const SStruct& outObj, const DynArray<char>& buffer);

}

