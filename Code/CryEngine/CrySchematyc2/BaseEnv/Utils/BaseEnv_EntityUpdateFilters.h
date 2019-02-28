// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

#include <Cry3DEngine/I3DEngine.h> // Must be included before CREOcclusionQuery.h.
#include <CryRenderer/RenderElements/CREOcclusionQuery.h> // Must be included before IEntityRenderState.h.

namespace SchematycBaseEnv
{
	struct SEntityNotHidden
	{
		inline bool operator () (IEntity* pEntity) const
		{
			return pEntity && !pEntity->IsHidden();
		}
	};

	struct SEntityVisible
	{
		inline bool operator () (IEntity* pEntity) const
		{
			if(pEntity)
			{
				return pEntity->IsRendered();
			}
			return false;
		}
	};

	typedef Schematyc2::CConfigurableUpdateFilter<IEntity*, NTypelist::CConstruct<SEntityNotHidden>::TType>                 EntityNotHiddenUpdateFilter;
	typedef Schematyc2::CConfigurableUpdateFilter<IEntity*, NTypelist::CConstruct<SEntityNotHidden, SEntityVisible>::TType> EntityVisibleUpdateFilter;
}