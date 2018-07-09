// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

// EXPECTED macro is used in the situations where you want to have an assertion
// combined with a runtime check/action, that is done in all configurations.
//
// Examples of use:
//
//   EXPECTED(connect(button, SIGNAL(triggered()), this, SLOT(OnButtonTriggered())));
//
//   if (!EXPECTED(argument != nullptr))
//     return;
//
// This will break under the debugger, but will still perform the check and call in the production build.

#define EXPECTED(x) ((x) || (ExpectedIsDebuggerPresent() && (__debugbreak(), true), false))

bool EDITOR_COMMON_API ExpectedIsDebuggerPresent();

