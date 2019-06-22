// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySystem/ConsoleRegistration.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace
{
	const char* szDebugDrawZTestOn         = "udr_debugDrawZTestOn";
	const char* szDebugDrawLineThickness   = "udr_debugDrawLineThickness";
	const char* szDebugDrawUpdate          = "udr_debugDrawUpdate";
	const char* szDebugDrawDuration        = "udr_debugDrawDuration";
	const char* szDebugDrawMinimumDuration = "udr_debugDrawMinimumDuration";
	const char* szDebugDrawMaximumDuration = "udr_debugDrawMaximumDuration";
}

namespace Cry
{
	namespace UDR
	{
		int   SCvars::debugDrawZTestOn;
		float SCvars::debugDrawLineThickness;
		int   SCvars::debugDrawUpdate;
		float SCvars::debugDrawDuration;
		float SCvars::debugDrawMinimumDuration;
		float SCvars::debugDrawMaximumDuration;

		void SCvars::Register()
		{
			REGISTER_CVAR2(szDebugDrawZTestOn, &debugDrawZTestOn, 1, 0,
				"0/1: Disable/enable z-tests for 3D debug geometry. Default value is 1.");

			REGISTER_CVAR2(szDebugDrawLineThickness, &debugDrawLineThickness, 1.0f, 0,
				"Thickness of all 3d lines that are used for debug drawing. Default value is 1.0f.");
			
			REGISTER_CVAR2(szDebugDrawUpdate, &debugDrawUpdate, 1, 0,
				"0/1: Disable/enable debug renderer system update loop. Enabled by default.");
			
			REGISTER_CVAR2(szDebugDrawDuration, &debugDrawDuration, 0.0f, 0,
				"Overwrites the duration of all primitives drawn by the Debug Renderer with the provided value. Default value is 0.0f which does nothing. If set, the value of this Cvar overwrites 'udr_debugDrawMinimumDuration' and 'udr_debugDrawMaximumDuration'.");
			
			REGISTER_CVAR2(szDebugDrawMinimumDuration, &debugDrawMinimumDuration, 0.0f, 0,
				"Overwrites the duration of all primitives drawn by the Debug Renderer which do not exceed the provided value. Default value is 0.0f which does nothing.");
			
			REGISTER_CVAR2(szDebugDrawMaximumDuration, &debugDrawMaximumDuration, 0.0f, 0,
				"Overwrites the duration of all primitives drawn by the Debug Renderer which exceed the provided value. Default value is 0.0f which does nothing.");
		}

		void SCvars::Unregister()
		{
			gEnv->pConsole->UnregisterVariable(szDebugDrawZTestOn);
			gEnv->pConsole->UnregisterVariable(szDebugDrawLineThickness);
			gEnv->pConsole->UnregisterVariable(szDebugDrawUpdate);
			gEnv->pConsole->UnregisterVariable(szDebugDrawDuration);
			gEnv->pConsole->UnregisterVariable(szDebugDrawMinimumDuration);
			gEnv->pConsole->UnregisterVariable(szDebugDrawMaximumDuration);
		}

		void SCvars::Validate()
		{
			CRY_ASSERT_MESSAGE(debugDrawZTestOn == 0 || debugDrawZTestOn == 1, "CVar '%s' value must be 0/1.", szDebugDrawZTestOn);

			CRY_ASSERT_MESSAGE(debugDrawLineThickness > 0.0f, "CVar '%s' value must be > 0.0f.", szDebugDrawLineThickness);
			
			CRY_ASSERT_MESSAGE(debugDrawUpdate == 0 || debugDrawUpdate == 1, "CVar '%s' value must be 0/1.", szDebugDrawUpdate);
			
			CRY_ASSERT_MESSAGE(debugDrawDuration >= 0.0f, "CVar '%s' value must be >= 0.0f.", szDebugDrawDuration);
			
			CRY_ASSERT_MESSAGE(debugDrawMinimumDuration >= 0.0f, "CVar '%s' value must be >= 0.0f.", szDebugDrawMinimumDuration);
			
			CRY_ASSERT_MESSAGE(debugDrawMaximumDuration >= 0.0f, "CVar '%s' value must be >= 0.0f.", szDebugDrawMaximumDuration);
		}

	}
}