// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryCore/Platform/platform.h>
#include <CrySystem/File/ICryPak.h>
#include <CryString/CryPath.h>
#include "DBATable.h"
#include "Serialization.h"
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>
#include <CrySystem/XML/IXml.h>

void SDBAEntry::Serialize(IArchive& ar)
{
	if (ar.isEdit() && ar.isOutput())
		ar(path, "digest", "!^");
	ar(OutputFilePath(path, "Animation Databases (.dba)|*.dba", "Animations"), "path", "<Path");
	filter.Serialize(ar);
}

void SDBATable::Serialize(IArchive& ar)
{
	ar(entries, "entries", "Entries");
}

bool SDBATable::Load(const char* fullPath)
{
	yasli::JSONIArchive ia;
	if (!ia.load(fullPath))
		return false;

	ia(*this);

	return true;
}

bool SDBATable::Save(const char* fullPath)
{
	yasli::JSONOArchive oa(120);
	oa(*this);

	oa.save(fullPath);
	return true;
}

int SDBATable::FindDBAForAnimation(const SAnimationFilterItem& animation) const
{
	for (size_t i = 0; i < entries.size(); ++i)
		if (entries[i].filter.MatchesFilter(animation))
			return int(i);

	return -1;
}

