// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DocTransitionGraph.h"

#include <CrySchematyc2/IDomainContext.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>

#include "DomainContext.h"
#include "Deprecated/DocGraphNodes/DocGraphBranchNode.h"
#include "Deprecated/DocGraphNodes/DocGraphFunctionNode.h"
#include "Deprecated/DocGraphNodes/DocGraphConditionNode.h"
#include "Deprecated/DocGraphNodes/DocGraphGetNode.h"
#include "Deprecated/DocGraphNodes/DocGraphNodes.h"
#include "Deprecated/DocGraphNodes/DocGraphSequenceNode.h"
#include "Deprecated/DocGraphNodes/DocGraphStateNode.h"
#include "Deprecated/DocGraphNodes/DocGraphSwitchNode.h"

namespace Schematyc2
{
	namespace DocTransitionGraphUtils
	{
		struct SSchemaElementVisitor
		{
			inline SSchemaElementVisitor(const IScriptFile& _file, CDocGraphBase& _docGraph)
				: file(_file)
				, docGraph(_docGraph)
			{}

			inline EVisitStatus VisitElement(const IScriptElement& element) const
			{
				const char* szElementName = element.GetName();
				switch(element.GetElementType())
				{
				case EScriptElementType::Group:
				case EScriptElementType::Class:
					{
						file.VisitElements(ScriptElementConstVisitor::FromConstMemberFunction<SSchemaElementVisitor, &SSchemaElementVisitor::VisitElement>(*this), element.GetGUID(), false);
						break;
					}
				case EScriptElementType::State:
					{
						{
							stack_string name = "State::";
							name.append(szElementName);
							docGraph.AddAvailableNode(name.c_str(), EScriptGraphNodeType::State, SGUID(), element.GetGUID());
						}
						file.VisitElements(ScriptElementConstVisitor::FromConstMemberFunction<SSchemaElementVisitor, &SSchemaElementVisitor::VisitElement>(*this), element.GetGUID(), false);
						break;
					}
				}
				return EVisitStatus::Continue;
			}

			const IScriptFile& file;
			CDocGraphBase&     docGraph;
		};

		inline bool FunctionIsCondition(const IGlobalFunction& function)
		{
			return (function.GetOutputCount() == 1) && (function.GetOutputValue(0)->GetTypeInfo().GetTypeId() == GetAggregateTypeId<bool>());
		}

		inline bool FunctionIsCondition(const IComponentMemberFunction& function)
		{
			return (function.GetOutputCount() == 1) && (function.GetOutputValue(0)->GetTypeInfo().GetTypeId() == GetAggregateTypeId<bool>());
		}

		inline bool FunctionIsCondition(const IActionMemberFunction& function)
		{
			return (function.GetOutputCount() == 1) && (function.GetOutputValue(0)->GetTypeInfo().GetTypeId() == GetAggregateTypeId<bool>());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocTransitionGraph::CDocTransitionGraph(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName, EScriptGraphType type, const SGUID& contextGUID)
		: CDocGraphBase(file, guid, scopeGUID, szName, type, contextGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	EAccessor CDocTransitionGraph::GetAccessor() const
	{
		return EAccessor::Private;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocTransitionGraph::RefreshAvailableNodes(const CAggregateTypeId& inputTypeId)
	{
		CDocGraphBase::ClearAvailablNodes();
		CDocGraphBase::AddAvailableNode("Branch", EScriptGraphNodeType::Branch, SGUID(), SGUID());
		CDocGraphBase::AddAvailableNode("Switch", EScriptGraphNodeType::Switch, SGUID(), SGUID());
		CDocGraphBase::AddAvailableNode("Make Enumeration", EScriptGraphNodeType::MakeEnumeration, SGUID(), SGUID());
		CDocGraphBase::AddAvailableNode("Comment", EScriptGraphNodeType::Comment, SGUID(), SGUID());

		const IScriptFile& file = CDocGraphBase::GetFile();
		const SGUID        scopeGUID = CDocGraphBase::GetScopeGUID();
		CDomainContext     domainContext(SDomainContextScope(&file, scopeGUID));
		VisitEnvGlobalFunctions(domainContext);
		VisitEnvComponentMemberFunctions(domainContext);
		VisitEnvActionMemberFunctions(domainContext);
		VisitVariables(domainContext);
		VisitProperties(domainContext);
		VisitGraphs(domainContext);

		// #SchematycTODO : Use domain context to visit everything!

		const IScriptElement* pOwnerElement = file.GetElement(scopeGUID);
		CRY_ASSERT(pOwnerElement);
		if(pOwnerElement)
		{
			DocTransitionGraphUtils::SSchemaElementVisitor schemaElementVisitor(file, *this);
			file.VisitElements(ScriptElementConstVisitor::FromConstMemberFunction<DocTransitionGraphUtils::SSchemaElementVisitor, &DocTransitionGraphUtils::SSchemaElementVisitor::VisitElement>(schemaElementVisitor), scopeGUID, false);
			file.VisitElements(ScriptElementConstVisitor::FromConstMemberFunction<DocTransitionGraphUtils::SSchemaElementVisitor, &DocTransitionGraphUtils::SSchemaElementVisitor::VisitElement>(schemaElementVisitor), pOwnerElement->GetScopeGUID(), false);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptGraphNodePtr CDocTransitionGraph::CreateNode(const SGUID& guid, EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
	{
		switch(type)
		{
		case EScriptGraphNodeType::Branch:
			{
				return IScriptGraphNodePtr(new CDocGraphBranchNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Switch:
			{
				return IScriptGraphNodePtr(new CDocGraphSwitchNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::MakeEnumeration:
			{
				return IScriptGraphNodePtr(new CDocGraphMakeEnumerationNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Condition:
			{
				return IScriptGraphNodePtr(new CDocGraphConditionNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Get:
			{
				return IScriptGraphNodePtr(new CDocGraphGetNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::State:
			{
				return IScriptGraphNodePtr(new CDocGraphStateNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::BeginState:
			{
				return IScriptGraphNodePtr(new CDocGraphBeginStateNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::EndState:
			{
				return IScriptGraphNodePtr(new CDocGraphEndStateNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Comment:
			{
				return IScriptGraphNodePtr(new CDocGraphCommentNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		default:
			{
				return IScriptGraphNodePtr();
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocTransitionGraph::VisitEnvGlobalFunctions(const IDomainContext& domainContext)
	{
		auto visitEnvGlobalFunction = [this, &domainContext] (const IGlobalFunctionConstPtr& pFunction) -> EVisitStatus
		{
			if(DocTransitionGraphUtils::FunctionIsCondition(*pFunction))
			{
				stack_string nodeName;
				domainContext.QualifyName(*pFunction, nodeName);
				nodeName.insert(0, "Condition::");
				AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Condition, SGUID(), pFunction->GetGUID());
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitEnvGlobalFunctions(EnvGlobalFunctionVisitor::FromLambdaFunction(visitEnvGlobalFunction));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocTransitionGraph::VisitEnvComponentMemberFunctions(const IDomainContext& domainContext)
	{
		typedef std::vector<const IScriptComponentInstance*> ComponentInstances;

		ComponentInstances componentInstances;
		componentInstances.reserve(32);
		auto visitComponentInstance = [&componentInstances] (const IScriptComponentInstance& componentInstance) -> EVisitStatus
		{
			componentInstances.push_back(&componentInstance);
			return EVisitStatus::Continue;
		};
		domainContext.VisitScriptComponentInstances(ScriptComponentInstanceConstVisitor::FromLambdaFunction(visitComponentInstance), EDomainScope::Local);

		auto visitEnvComponentMemberFunction = [this, &domainContext, &componentInstances] (const IComponentMemberFunctionConstPtr& pFunction) -> EVisitStatus
		{
			if(DocTransitionGraphUtils::FunctionIsCondition(*pFunction))
			{
				const SGUID componentGUID = pFunction->GetComponentGUID();
				for(const IScriptComponentInstance* pComponentInstance : componentInstances)
				{
					if(pComponentInstance->GetComponentGUID() == componentGUID)
					{
						stack_string nodeName;
						domainContext.QualifyName(*pComponentInstance, *pFunction, EDomainQualifier::Global, nodeName);
						nodeName.insert(0, "Condition::");
						AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Condition, pComponentInstance->GetGUID(), pFunction->GetGUID());
					}
				}
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitEnvComponentMemberFunctions(EnvComponentMemberFunctionVisitor::FromLambdaFunction(visitEnvComponentMemberFunction));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocTransitionGraph::VisitEnvActionMemberFunctions(const IDomainContext& domainContext)
	{
		typedef std::vector<const IScriptActionInstance*> ActionInstances;

		ActionInstances actionInstances;
		actionInstances.reserve(32);
		auto visitActionInstance = [&actionInstances] (const IScriptActionInstance& actionInstance) -> EVisitStatus
		{
			actionInstances.push_back(&actionInstance);
			return EVisitStatus::Continue;
		};
		domainContext.VisitScriptActionInstances(ScriptActionInstanceConstVisitor::FromLambdaFunction(visitActionInstance), EDomainScope::Local);

		auto visitEnvActionMemberFunction = [this, &domainContext, &actionInstances] (const IActionMemberFunctionConstPtr& pFunction) -> EVisitStatus
		{
			if(DocTransitionGraphUtils::FunctionIsCondition(*pFunction))
			{
				const SGUID actionGUID = pFunction->GetActionGUID();
				for(const IScriptActionInstance* pActionInstance : actionInstances)
				{
					if(pActionInstance->GetActionGUID() == actionGUID)
					{
						stack_string nodeName;
						domainContext.QualifyName(*pActionInstance, *pFunction, EDomainQualifier::Global, nodeName);
						nodeName.insert(0, "Condition::");
						AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Condition, pActionInstance->GetGUID(), pFunction->GetGUID());
					}
				}
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitEnvActionMemberFunctions(EnvActionMemberFunctionVisitor::FromLambdaFunction(visitEnvActionMemberFunction));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocTransitionGraph::VisitVariables(const IDomainContext& domainContext)
	{
		auto visitVariable = [this, &domainContext] (const IScriptVariable& variable) -> EVisitStatus
		{
			stack_string nodeName;
			domainContext.QualifyName(variable, EDomainQualifier::Global, nodeName);
			nodeName.insert(0, "Get::");
			AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Get, SGUID(), variable.GetGUID());
			return EVisitStatus::Continue;
		};
		domainContext.VisitScriptVariables(ScriptVariableConstVisitor::FromLambdaFunction(visitVariable), EDomainScope::Derived);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocTransitionGraph::VisitProperties(const IDomainContext& domainContext)
	{
		auto visitProperty = [this, &domainContext] (const IScriptProperty& property) -> EVisitStatus
		{
			stack_string nodeName;
			domainContext.QualifyName(property, EDomainQualifier::Global, nodeName);
			nodeName.insert(0, "Get::");
			AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Get, SGUID(), property.GetGUID());
			return EVisitStatus::Continue;
		};
		domainContext.VisitScriptProperties(ScriptPropertyConstVisitor::FromLambdaFunction(visitProperty), EDomainScope::Derived);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocTransitionGraph::VisitGraphs(const IDomainContext& domainContext)
	{
		auto visitGraph = [this, &domainContext] (const IDocGraph& graph) -> EVisitStatus
		{
			if(graph.GetType() == EScriptGraphType::Condition)
			{
				stack_string nodeName;
				domainContext.QualifyName(graph, EDomainQualifier::Global, nodeName);
				nodeName.insert(0, "Condition::");
				AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Condition, SGUID(), graph.GetGUID());
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitDocGraphs(DocGraphConstVisitor::FromLambdaFunction(visitGraph), EDomainScope::Derived);
	}
}
