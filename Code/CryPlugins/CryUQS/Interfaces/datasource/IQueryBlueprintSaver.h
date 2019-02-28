// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace DataSource
	{

		//===================================================================================
		//
		// IQueryBlueprintSaver
		//
		//===================================================================================

		struct IQueryBlueprintSaver
		{
			virtual                        ~IQueryBlueprintSaver() {}
			virtual bool                   SaveTextualQueryBlueprint(const Core::ITextualQueryBlueprint& queryBlueprintToSave, Shared::IUqsString& error) = 0;
		};

	}
}
