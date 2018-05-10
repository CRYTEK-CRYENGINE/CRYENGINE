// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

namespace SchematycBaseEnv
{
	struct SEntityUserData
	{
		inline SEntityUserData(bool _bIsPreview, Schematyc2::EObjectFlags _objectFlags)
			: bIsPreview(_bIsPreview)
			, objectFlags(_objectFlags)
		{}

		bool                    bIsPreview;
		Schematyc2::EObjectFlags objectFlags;
	};
}