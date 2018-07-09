// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "CrySchematyc2/Prerequisites.h"

namespace Schematyc2 {

namespace LibUtils {

struct SReferencesLogInfo;

void FindReferences(SGUID refGuid, SGUID refGoBack, const char* szItemName, const struct TScriptFile* pFile, SReferencesLogInfo& result);
void FindAndLogReferences(SGUID refGuid, SGUID refGoBack, const char* szItemName, const struct TScriptFile* pFile, bool searchInAllFiles = false);


} // namespace LibUtils

} // namespace Schematyc
