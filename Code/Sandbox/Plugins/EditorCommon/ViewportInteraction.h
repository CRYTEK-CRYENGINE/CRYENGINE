// Copyright 2001-2015 Crytek GmbH. All rights reserved.
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

