// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScriptElement.h>

namespace Schematyc
{
// #SchematycTODO : Resolve disconnect between factory method used to construct elements during serialization and in place methods used when editing?
// #SchematycTODO : Allocate elements in pools?
class CScriptElementFactory
{
public:

	IScriptElementPtr CreateElement(EScriptElementType elementType);
};
} // Schematyc
