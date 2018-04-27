// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource
	{
		//===================================================================================
		//
		// IEditorLibraryProvider
		//
		// UqsEditor uses this interface to interact with the library of queries.
		//
		//===================================================================================
		struct IEditorLibraryProvider
		{
			// TODO pavloi 2016.04.07: replace with functor? Is it OK to pass functors between DLL boundaries?
			struct IListQueriesVisitor
			{
				virtual void Visit(const char* szQueryName, const char* szQueryFilePath) = 0;
			};
			virtual void GetQueriesList(IListQueriesVisitor& visitor) = 0;

			virtual bool LoadTextualQueryBlueprint(const char* szQueryName, Core::ITextualQueryBlueprint& outBlueprint, Shared::IUqsString& error) = 0;
			virtual bool SaveTextualQueryBlueprint(const char* szQueryName, const Core::ITextualQueryBlueprint& blueprintToSave, Shared::IUqsString& error) = 0;

			virtual bool CreateNewTextualQueryBlueprint(const char* szQueryName, Core::ITextualQueryBlueprint& outBlueprint, Shared::IUqsString& error) = 0;
			virtual bool RemoveQueryBlueprint(const char* szQueryName, Shared::IUqsString& error) = 0;
			// TODO pavloi 2016.04.08: rename query
			// TODO pavloi 2016.04.08: register listener for change monitoring
		};

	} // namespace DataSource
} // namespace UQS
