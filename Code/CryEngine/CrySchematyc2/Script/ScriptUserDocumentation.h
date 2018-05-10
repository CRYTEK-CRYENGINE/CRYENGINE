// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

namespace Schematyc2
{
	struct SScriptUserDocumentation
	{
		void SetCurrentUserAsAuthor();
		void Serialize(Serialization::IArchive& archive);

		string	author;
		string	description;
	};
}
