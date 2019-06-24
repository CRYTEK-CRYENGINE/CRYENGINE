// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>
namespace Cry
{
	namespace UDR
	{

		struct SCvars
		{
			static void          Register();
			static void          Unregister();
			static void          Validate();

			static int           debugDrawZTestOn;          // z-buffer on/off of 3d in-world rendering
			static float         debugDrawLineThickness;    // thickness of all 3d lines (affects every primitive that uses lines for its basic debug drawing)
			static int           debugDrawUpdate;
			static float         debugDrawDuration;
			static float         debugDrawMinimumDuration;
			static float         debugDrawMaximumDuration;
		};

	}
}