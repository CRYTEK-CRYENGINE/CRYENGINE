// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Bridge_ICompiler.h"

namespace Schematyc2 {

struct ICompiler;

}

namespace Bridge {

class CCompiler : public ICompiler
{
public:
	// ICompiler
	virtual Schematyc2::ILibPtr Compile(const IScriptFile& scriptFile) override;
	virtual void Compile(const Schematyc2::IScriptElement& scriptElement) override;
	virtual void CompileAll() override;
	// ~ICompiler

private:
	Schematyc2::ICompiler* Delegate() const;
};

}