// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/Decorators/BitFlags.h>

namespace Serialization {

inline void BitFlagsWrapper::Serialize(IArchive& ar)
{
	const yasli::EnumDescription& desc = *description;
	int count = desc.count();
	if (ar.isInput())
	{
		int previousValue = *variable;
		for (int i = 0; i < count; ++i)
		{
			int flagValue = desc.valueByIndex(i);
			if (!(flagValue & visibleMask))
				continue;
			bool flag = (previousValue & flagValue) == flagValue;
			bool previousFlag = flag;
			ar(flag, desc.nameByIndex(i), desc.labelByIndex(i));
			if (flag != previousFlag)
			{
				if (flag)
					*variable |= flagValue;
				else
					*variable &= ~flagValue;
			}
		}
	}
	else
	{
		for (int i = 0; i < count; ++i)
		{
			int flagValue = desc.valueByIndex(i);
			if (!(flagValue & visibleMask))
				continue;
			bool flag = (*variable & flagValue) == flagValue;
			ar(flag, desc.nameByIndex(i), desc.labelByIndex(i));
		}
	}
}

}
