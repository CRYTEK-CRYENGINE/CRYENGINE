// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/Enum.h>

namespace Serialization {

struct BitFlagsWrapper
{
	int*                                  variable;
	unsigned int                          visibleMask;
	const Serialization::EnumDescription* description;

	void                                  Serialize(IArchive& ar);
};

template<class Enum>
BitFlagsWrapper BitFlags(Enum& value)
{
	BitFlagsWrapper wrapper;
	wrapper.variable = (int*)&value;
	wrapper.visibleMask = ~0U;
	wrapper.description = &getEnumDescription<Enum>();
	return wrapper;
}

template<class Enum>
BitFlagsWrapper BitFlags(int& value, int visibleMask = ~0)
{
	BitFlagsWrapper wrapper;
	wrapper.variable = &value;
	wrapper.visibleMask = visibleMask;
	wrapper.description = &getEnumDescription<Enum>();
	return wrapper;
}

template<class Enum>
BitFlagsWrapper BitFlags(unsigned int& value, unsigned int visibleMask = ~0)
{
	BitFlagsWrapper wrapper;
	wrapper.variable = (int*)&value;
	wrapper.visibleMask = visibleMask;
	wrapper.description = &getEnumDescription<Enum>();
	return wrapper;
}

}

#include "BitFlagsImpl.h"
