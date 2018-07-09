// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "Serialization.h"
#include "AnimSettings.h"

bool Serialize(Serialization::IArchive& ar, SkeletonAlias& value, const char* name, const char* label)
{
	return ar(value.alias, name, label);
}

