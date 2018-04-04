// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

#include "ISensorSystem.h"

enum class ESensorTags : uint32 {};

typedef CEnumFlags<ESensorTags> SensorTags;

struct ISensorTagLibrary
{
	virtual ~ISensorTagLibrary() {}

	virtual SensorTags CreateTag(const char* szTagName) = 0;
	virtual SensorTags GetTag(const char* szTagName) const = 0;
	virtual void GetTagNames(DynArray<const char*>& tagNames, SensorTags tags) const = 0;
};
