// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// - functions that create a geometrical representation for generated items
		// - this geometrical representation has two internal purposes:
		//   -- 1. it's used to find the closest item to a debug camera (UQS::Core::SDebugCameraView) for which additional detailed debug rendering will be done (aka "which item gets the debug focus")
		//   -- 2. internally, this proxy geometry will also do some debug-rendering to visualize the item it represents
		// - they're registered as callbacks within an item factory
		// - not all types of items may have debug proxies (e. g. an 'int' or 'float' doesn't have one, but a 'Vec3' does)
		//
		//===================================================================================

		void EntityId_CreateItemDebugProxyForItem(const EntityIdWrapper& item, Core::IItemDebugProxyFactory& itemDebugProxyFactory);
		void Pos3_CreateItemDebugProxyForItem(const Pos3& item, Core::IItemDebugProxyFactory& itemDebugProxyFactory);

	}
}
