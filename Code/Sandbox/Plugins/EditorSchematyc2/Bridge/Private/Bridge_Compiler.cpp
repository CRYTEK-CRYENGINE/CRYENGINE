// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Bridge_Compiler.h"

#include "Bridge_ScriptFile.h"

namespace Bridge {

//////////////////////////////////////////////////////////////////////////
Schematyc2::ILibPtr CCompiler::Compile(const IScriptFile& scriptFile)
{
	return Delegate()->Compile(*scriptFile.GetDelegate());
}

//////////////////////////////////////////////////////////////////////////
void CCompiler::Compile(const Schematyc2::IScriptElement& scriptElement) 
{
	Delegate()->Compile(scriptElement);
}

//////////////////////////////////////////////////////////////////////////
void CCompiler::CompileAll()
{
	Delegate()->CompileAll();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::ICompiler* CCompiler::Delegate() const
{
	CRY_ASSERT(gEnv->pSchematyc2);
	return &gEnv->pSchematyc2->GetCompiler();
}

}
