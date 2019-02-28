// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IEditorService
		//
		// - some functionality, like query-blueprint validation, to support the implementation of IEditorServiceConsumer
		// - this is basically a stand-alone interface that isn't used at game runtime
		//
		//===================================================================================

		struct IEditorService
		{
			virtual               ~IEditorService() {}

			// - attempts to load the textual query-blueprint with given name (but does *not* do any validation, of course)
			// - the return value indicates whether the blueprint could be loaded (e. g. xml file existed and contains all necessary xml data everything to build a textual query-blueprint)
			//   or whether something on the datasource level went wrong (e. g. file not found, xml not well-formed, xml misses some required attributes, contains unknown elements, ...)
			// - if successfully loaded, will call given IEditorServiceConsumer on it
			virtual bool          LoadTextualQueryBlueprint(const char* szQueryBlueprintName, Editor::IEditorServiceConsumer& callback, Shared::IUqsString& error) const = 0;

			// - works in the same way as LoadTextualQueryBlueprint() does
			// - except it calls IEditorServiceConsumer::OnTextualQueryBlueprintGettingSaved()
			virtual bool          SaveTextualQueryBlueprint(const char* szQueryBlueprintName, Editor::IEditorServiceConsumer& callback, Shared::IUqsString& error) const = 0;

			// - creates and registers a new query-blueprint record with the given name
			// - if successfully created, will call given IEditorServiceConsumer on it to allow to load query-blueprint
			// - implementation is allowed to sanitize and change requested name - actual name is set to blueprint
			virtual bool          CreateNewTextualQueryBlueprint(const char* szQueryBlueprintName, Editor::IEditorServiceConsumer& callback, Shared::IUqsString& error) const = 0;

			// - removes query blueprint by name
			virtual bool          RemoveQueryBlueprint(const char* szQueryBlueprintName, Shared::IUqsString& error) const = 0;

			// validating given ITextualQueryBlueprint will use its attached ISyntaxErrorCollectors to communicate syntax to the outside
			virtual bool          ValidateTextualQueryBlueprint(Editor::IEditorServiceConsumer& callback) const = 0;
		};

	}
}
