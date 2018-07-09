// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource
	{

		//===================================================================================
		//
		// IQueryBlueprintLoader
		//
		//===================================================================================

		struct IQueryBlueprintLoader
		{
			virtual                        ~IQueryBlueprintLoader() {}
			virtual bool                   LoadTextualQueryBlueprint(Core::ITextualQueryBlueprint& out, Shared::IUqsString& error) = 0;
		};

	}
}
