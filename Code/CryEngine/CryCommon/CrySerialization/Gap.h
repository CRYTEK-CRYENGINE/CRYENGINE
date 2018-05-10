// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>

namespace Serialization
{
// used to fill the property rows by empty space
struct SGap
{
	SGap() : width(-1) {}
	explicit SGap(int w) : width(w) {}

	void Serialize(Serialization::IArchive& ar) {}

	int  width;
};
}
