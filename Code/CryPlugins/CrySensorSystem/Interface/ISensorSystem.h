// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Cry
{
	namespace SensorSystem
	{
		struct ISensorMap;
		struct ISensorTagLibrary;

		struct ISensorSystem
		{
			virtual ~ISensorSystem() {}

			virtual ISensorTagLibrary& GetTagLibrary() = 0;
			virtual ISensorMap&        GetMap() = 0;
		};
	}
}