// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptUserDocumentation.h"

#include <CrySerialization/STL.h>

namespace Schematyc
{
void SScriptUserDocumentation::SetCurrentUserAsAuthor()
{
	author = gEnv->pSystem->GetUserName();
}

void SScriptUserDocumentation::Serialize(Serialization::IArchive& archive)
{
	archive(author, "author", "Author(s)");
	archive(description, "description", "Description");
}
}
