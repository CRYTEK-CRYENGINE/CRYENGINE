// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompilerTaskList.h"

namespace Schematyc
{
CCompilerTaskList::CCompilerTaskList()
{
	m_pendingGraphs.reserve(20);
}

void CCompilerTaskList::CompileGraph(const IScriptGraph& scriptGraph)
{
	m_pendingGraphs.push_back(&scriptGraph);
}

CCompilerTaskList::PendingGraphs& CCompilerTaskList::GetPendingGraphs()
{
	return m_pendingGraphs;
}
} // Schematyc
