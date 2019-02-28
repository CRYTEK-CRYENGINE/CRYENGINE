// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySerialization/yasli/decorators/FileOpen.h>
#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/STLImpl.h>

namespace yasli {

YASLI_INLINE void FileOpen::YASLI_SERIALIZE_METHOD(Archive& ar)
{
	ar(path, "path");
	ar(filter, "filter");
	ar(relativeToFolder, "folder");
	ar(flags, "flags");
}

YASLI_INLINE bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, FileOpen& value, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serializer(value), name, label);
	else
		return ar(value.path, name, label);
}

}

