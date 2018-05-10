// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/ICompiler.h>

namespace Bridge {

struct IScriptFile;

struct ICompiler
{
	virtual ~ICompiler() {}

	virtual Schematyc2::ILibPtr Compile(const IScriptFile& file) = 0;
	virtual void Compile(const Schematyc2::IScriptElement& scriptElement) = 0;
	virtual void CompileAll() = 0;
};

}
