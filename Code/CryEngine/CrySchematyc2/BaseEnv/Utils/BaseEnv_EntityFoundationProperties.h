// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

namespace SchematycBaseEnv
{
	struct SEntityFoundationProperties
	{
		SEntityFoundationProperties();

		void Serialize(Serialization::IArchive& archive);

		string icon;
		bool   bHideInEditor;
		bool   bTriggerAreas;
	};
}