// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Expected.h"
#include <CryCore/Platform/CryWindows.h>

bool ExpectedIsDebuggerPresent()
{
	return IsDebuggerPresent() ? true : false;
}

