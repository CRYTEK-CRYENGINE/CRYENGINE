// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GenericWidgetModelImpl.h"

#include <CrySchematyc2/Script/IScriptGraph.h>

#include "Util.h"

namespace Cry {
namespace SchematycEd {

////////////////////////////////////////////////////////////////////////
template <> inline EVisitStatus CGenericWidgetDictionaryModel::BuildScriptElementModel<IScriptSignal>(const IScriptSignal& signal)
{
	m_entryGuid.push_back(BuildScriptElementEntry(signal, "Signal")->m_guid);
	return EVisitStatus::Continue;
}

//////////////////////////////////////////////////////////////////////////
template <> EVisitStatus CGenericWidgetDictionaryModel::BuildScriptElementModel<IScriptStructure>(const IScriptStructure& structure)
{
	m_entryGuid.push_back(BuildScriptElementEntry(structure, "Structure")->m_guid);
	return EVisitStatus::Continue;
}

//////////////////////////////////////////////////////////////////////////
template <> EVisitStatus CGenericWidgetDictionaryModel::BuildScriptElementModel<IScriptEnumeration>(const IScriptEnumeration& enumeration)
{
	m_entryGuid.push_back(BuildScriptElementEntry(enumeration, "Enumeration")->m_guid);
	return EVisitStatus::Continue;
}

//////////////////////////////////////////////////////////////////////////
template <> EVisitStatus CGenericWidgetDictionaryModel::BuildScriptElementModel<IDocGraph>(const IDocGraph& graph)
{
	m_entryGuid.push_back(BuildScriptElementEntry(graph, "Graph")->m_guid);
	return EVisitStatus::Continue;
}

//////////////////////////////////////////////////////////////////////////
template <> EVisitStatus CGenericWidgetDictionaryModel::BuildScriptElementModel<IScriptStateMachine>(const IScriptStateMachine& stateMachine)
{
	m_entryGuid.push_back(BuildScriptElementEntry(stateMachine, "StateMachine")->m_guid);
	return EVisitStatus::Continue;
}

//////////////////////////////////////////////////////////////////////////
template <> EVisitStatus CGenericWidgetDictionaryModel::BuildScriptElementModel<IScriptState>(const IScriptState& state)
{
	BuildScriptElementParent(state, BuildScriptElementEntry(state, "State"), { EScriptElementType::StateMachine, EScriptElementType::State });
	return EVisitStatus::Continue;
}

//////////////////////////////////////////////////////////////////////////
template <> EVisitStatus CGenericWidgetDictionaryModel::BuildScriptElementModel<IScriptTimer>(const IScriptTimer& timer)
{
	m_entryGuid.push_back(BuildScriptElementEntry(timer, "Timer")->m_guid);
	return EVisitStatus::Continue;
}

//////////////////////////////////////////////////////////////////////////
template <> EVisitStatus CGenericWidgetDictionaryModel::BuildScriptElementModel<IScriptVariable>(const IScriptVariable& variable)
{
	m_entryGuid.push_back(BuildScriptElementEntry(variable, "Variable")->m_guid);
	return EVisitStatus::Continue;
}

//////////////////////////////////////////////////////////////////////////
template <> EVisitStatus CGenericWidgetDictionaryModel::BuildScriptElementModel<IScriptComponentInstance>(const IScriptComponentInstance& component)
{
	BuildScriptElementParent(component, BuildScriptElementEntry(component, "Component"), { EScriptElementType::ComponentInstance });
	return EVisitStatus::Continue;
}

//////////////////////////////////////////////////////////////////////////
template <> void CGenericWidgetDictionaryModel::LoadScriptElementType<IScriptSignal>(IScriptFile* pFile, const SGUID& scopeGUID)
{	
	pFile->VisitSignals(ScriptSignalConstVisitor::FromLambdaFunction([this](const IScriptSignal& signal)
	{
		return BuildScriptElementModel(signal);
	}
	), scopeGUID, true);	
}

//////////////////////////////////////////////////////////////////////////
template <> void CGenericWidgetDictionaryModel::LoadScriptElementType<IScriptStructure>(IScriptFile* pFile, const SGUID& scopeGUID)
{
	pFile->VisitStructures(ScriptStructureConstVisitor::FromLambdaFunction([this](const IScriptStructure& structure)
	{
		return BuildScriptElementModel(structure);
	}
	), scopeGUID, true);
}

//////////////////////////////////////////////////////////////////////////
template <> void CGenericWidgetDictionaryModel::LoadScriptElementType<IScriptEnumeration>(IScriptFile* pFile, const SGUID& scopeGUID)
{
	pFile->VisitEnumerations(ScriptEnumerationConstVisitor::FromLambdaFunction([this](const IScriptEnumeration& enumeration)
	{
		return BuildScriptElementModel(enumeration);
	}
	), scopeGUID, true);
}

//////////////////////////////////////////////////////////////////////////
template <> void CGenericWidgetDictionaryModel::LoadScriptElementType<IDocGraph>(IScriptFile* pFile, const SGUID& scopeGUID)
{
	pFile->VisitGraphs(DocGraphConstVisitor::FromLambdaFunction([this](const IDocGraph& graph)
	{
		return BuildScriptElementModel(graph);
	}
	), scopeGUID, true);
}

//////////////////////////////////////////////////////////////////////////
template <> void CGenericWidgetDictionaryModel::LoadScriptElementType<IScriptState>(IScriptFile* pFile, const SGUID& scopeGUID)
{
	pFile->VisitStates(ScriptStateConstVisitor::FromLambdaFunction([this](const IScriptState& state)
	{
		return BuildScriptElementModel(state);
	}
	), scopeGUID, true);
}

//////////////////////////////////////////////////////////////////////////
template <> void CGenericWidgetDictionaryModel::LoadScriptElementType<IScriptStateMachine>(IScriptFile* pFile, const SGUID& scopeGUID)
{
	pFile->VisitStateMachines(ScriptStateMachineConstVisitor::FromLambdaFunction([this](const IScriptStateMachine& stateMachine)
	{
		return BuildScriptElementModel(stateMachine);
	}
	), scopeGUID, true);	
}

//////////////////////////////////////////////////////////////////////////
template <> void CGenericWidgetDictionaryModel::LoadScriptElementType<IScriptComponentInstance>(IScriptFile* pFile, const SGUID& scopeGUID)
{
	pFile->VisitComponentInstances(ScriptComponentInstanceConstVisitor::FromLambdaFunction([this](const IScriptComponentInstance& component)
	{
		return BuildScriptElementModel(component);
	}
	), scopeGUID, true);
}

//////////////////////////////////////////////////////////////////////////
template <> void CGenericWidgetDictionaryModel::LoadScriptElementType<IScriptTimer>(IScriptFile* pFile, const SGUID& scopeGUID)
{
	pFile->VisitTimers(ScriptTimerConstVisitor::FromLambdaFunction([this](const IScriptTimer& timer)
	{
		return BuildScriptElementModel(timer);
	}
	), scopeGUID, true);
}

//////////////////////////////////////////////////////////////////////////
template <> void CGenericWidgetDictionaryModel::LoadScriptElementType<IScriptVariable>(IScriptFile* pFile, const SGUID& scopeGUID)
{
	pFile->VisitVariables(ScriptVariableConstVisitor::FromLambdaFunction([this](const IScriptVariable& variable)
	{
		return BuildScriptElementModel(variable);
	}
	), scopeGUID, true);
}

CGenericWidgetDictionaryModel* CGenericWidgetDictionaryModel::CreateByCategory(const string& category)
{
	CGenericWidgetDictionaryModel* pModel = nullptr;

	if (category == "Types")
	{
		pModel = new CTypesCategoryModel();
	}
	else if (category == "Signals")
	{
		pModel = new CSignalsCategoryModel();
	}
	else if (category == "Graphs")
	{
		pModel = new CGraphsCategoryModel();
	}
	else if (category == "Components")
	{
		pModel = new CComponentsCategoryModel();
	}
	else if (category == "Variables")
	{
		pModel = new CVariablesCategoryModel();
	}	

	return pModel;
}

} //namespace SchematycEd
} //namespace Cry
