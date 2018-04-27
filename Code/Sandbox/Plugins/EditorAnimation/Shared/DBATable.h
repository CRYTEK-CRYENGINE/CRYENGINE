// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AnimationFilter.h"
#include <CrySerialization/Forward.h>

struct SDBAEntry
{
	SAnimationFilter filter;
	string           path;

	void             Serialize(Serialization::IArchive& ar);
};

struct SDBATable
{
	std::vector<SDBAEntry> entries;

	void                   Serialize(Serialization::IArchive& ar);

	// returns -1 when nothing is found
	int  FindDBAForAnimation(const SAnimationFilterItem& animation) const;

	bool Load(const char* dbaTablePath);
	bool Save(const char* dbaTablePath);
};

