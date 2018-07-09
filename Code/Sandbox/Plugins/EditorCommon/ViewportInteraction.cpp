// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "ViewportInteraction.h"

#include "PolledKey.h"

REGISTER_POLLED_KEY(viewport, forward, "Moves the camera forward", CKeyboardShortcut("W; Up"));
REGISTER_POLLED_KEY(viewport, backward, "Moves the camera backward", CKeyboardShortcut("S; Down"));
REGISTER_POLLED_KEY(viewport, left, "Moves the camera left", CKeyboardShortcut("A; Left"));
REGISTER_POLLED_KEY(viewport, right, "Moves the camera right", CKeyboardShortcut("D; Right"));

bool ViewportInteraction::CheckPolledKey(Key key)
{
	switch (key)
	{
	case eKey_Forward:
		return GET_POLLED_KEY(viewport, forward);
	case eKey_Backward:
		return GET_POLLED_KEY(viewport, backward);
	case eKey_Left:
		return GET_POLLED_KEY(viewport, left);
	case eKey_Right:
		return GET_POLLED_KEY(viewport, right);
	default:
		assert(0);
		return false;
	}
}

