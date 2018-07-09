// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

struct EDITOR_COMMON_API ViewportInteraction
{
	enum Key
	{
		eKey_Forward,
		eKey_Backward,
		eKey_Left,
		eKey_Right
	};

	static bool CheckPolledKey(Key key);
};

