// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/Schematyc_IScriptFile.h>

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