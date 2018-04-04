// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "Slider.h"
#include <CrySerialization/IArchive.h>

namespace Serialization
{

inline bool Serialize(IArchive& ar, SSliderF& slider, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(SStruct::forEdit(slider), name, label);
	else
		return ar(*slider.valuePointer, name, label);
}

inline bool Serialize(IArchive& ar, SSliderI& slider, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(SStruct::forEdit(slider), name, label);
	else
		return ar(*slider.valuePointer, name, label);
}

}
