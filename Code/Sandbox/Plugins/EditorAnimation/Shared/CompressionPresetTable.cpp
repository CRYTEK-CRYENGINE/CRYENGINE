// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryCore/Platform/platform.h>
#include "CompressionPresetTable.h"
#include "AnimationFilter.h"
#include "DBATable.h"
#include "Serialization.h"
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

void SCompressionPresetEntry::Serialize(Serialization::IArchive& ar)
{
	if (ar.isEdit() && ar.isOutput())
		ar(name, "digest", "!^");
	ar(name, "name", "Name");
	filter.Serialize(ar);
	if (!filter.condition)
		ar.warning(filter.condition, "Specify filter to apply preset to animations.");
	ar(settings, "settings", "+Settings");
}

void SCompressionPresetTable::Serialize(Serialization::IArchive& ar)
{
	ar(entries, "entries", "Entries");
}

bool SCompressionPresetTable::Load(const char* fullPath)
{
	yasli::JSONIArchive ia;
	if (!ia.load(fullPath))
		return false;

	ia(*this);
	return true;
}

bool SCompressionPresetTable::Save(const char* fullPath)
{
	yasli::JSONOArchive oa(120);
	oa(*this);

	return oa.save(fullPath);
}

const SCompressionPresetEntry* SCompressionPresetTable::FindPresetForAnimation(const char* animationPath, const vector<string>& tags, const char* skeletonAlias) const
{
	SAnimationFilterItem item;
	item.path = animationPath;
	item.tags = tags;
	item.skeletonAlias = skeletonAlias;

	for (size_t i = 0; i < entries.size(); ++i)
	{
		const SCompressionPresetEntry& entry = entries[i];
		if (entry.filter.MatchesFilter(item))
			return &entry;
	}

	return 0;
}

