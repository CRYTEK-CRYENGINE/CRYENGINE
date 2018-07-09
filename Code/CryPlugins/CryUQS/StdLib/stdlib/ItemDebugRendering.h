// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// - functions that add visual representations of items to an IDebugRenderWorldPersistent
		// - they're registered as callbacks within an item factory and are used to add persistent debug geometry for all items of an ongoing query
		// - not all types of items may have a visual representation (e. g. an 'int' or 'float' doesn't have one, but a 'CAIActor' does)
		//
		//===================================================================================

		void EntityId_AddToDebugRenderWorld(const EntityIdWrapper& item, Core::IDebugRenderWorldPersistent& debugRW);
		void Pos3_AddToDebugRenderWorld(const Pos3& item, Core::IDebugRenderWorldPersistent& debugRW);

	}
}
