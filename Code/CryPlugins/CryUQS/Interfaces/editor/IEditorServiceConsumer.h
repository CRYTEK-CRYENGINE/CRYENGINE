// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Editor
	{

		//===================================================================================
		//
		// IEditorServiceConsumer
		//
		// - used by the IEditorService in the core to call back into the actual editor implementation
		// - this is used as a double dispatch pattern: someone calls methods on IEditorService, passes in an IEditorServiceConsumer, and the IEditorService in turn calls methods on that IEditorServiceConsumer in the same call stack
		//
		//===================================================================================

		// TODO: rename to IEditorServiceCallbacks
		struct IEditorServiceConsumer
		{
			virtual            ~IEditorServiceConsumer() {}

			// - called by IEditorService::LoadTextualQueryBlueprint()
			// - given ITextualQueryBlueprint exists only on the stack for the duration of this function call (!)
			virtual void       OnTextualQueryBlueprintLoaded(const Core::ITextualQueryBlueprint& loadedTextualQueryBlueprint) = 0;

			// - called by IEditorService::SaveTextualQueryBlueprint()
			virtual void       OnTextualQueryBlueprintGettingSaved(Core::ITextualQueryBlueprint& textualQueryBlueprintToFillBeforeSaving) = 0;

			virtual void       OnTextualQueryBlueprintGettingValidated(Core::ITextualQueryBlueprint& textualQueryBlueprintToFillBeforeValidating) = 0;
		};

	}
}
