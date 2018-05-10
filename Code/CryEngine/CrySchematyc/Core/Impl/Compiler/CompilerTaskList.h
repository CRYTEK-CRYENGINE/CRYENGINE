// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Compiler/CompilerContext.h>

namespace Schematyc
{
class CCompilerTaskList : public ICompilerTaskList
{
public:

	typedef std::vector<const IScriptGraph*> PendingGraphs;

public:

	CCompilerTaskList();

	// ICompilerTaskList
	virtual void CompileGraph(const IScriptGraph& scriptGraph) override;
	// ~ICompilerTaskList

	PendingGraphs& GetPendingGraphs();

private:

	PendingGraphs m_pendingGraphs;
};
} // Schematyc
