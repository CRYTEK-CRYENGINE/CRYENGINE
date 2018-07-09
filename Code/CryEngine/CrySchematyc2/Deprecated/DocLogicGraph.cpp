// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DocLogicGraph.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/AbstractInterfaceUtils.h>
#include <CrySchematyc2/IAbstractInterface.h>
#include <CrySchematyc2/IComponentFactory.h>
#include <CrySchematyc2/IDomainContext.h>
#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "DomainContext.h"
#include "Deprecated/DocGraphNodes/DocGraphAbstractInterfaceFunctionNode.h"
#include "Deprecated/DocGraphNodes/DocGraphBranchNode.h"
#include "Deprecated/DocGraphNodes/DocGraphContainerAddNode.h"
#include "Deprecated/DocGraphNodes/DocGraphContainerFindByValueNode.h"
#include "Deprecated/DocGraphNodes/DocGraphContainerRemoveByIndexNode.h"
#include "Deprecated/DocGraphNodes/DocGraphContainerRemoveByValueNode.h"
#include "Deprecated/DocGraphNodes/DocGraphContainerSetNode.h"
#include "Deprecated/DocGraphNodes/DocGraphFunctionNode.h"
#include "Deprecated/DocGraphNodes/DocGraphConditionNode.h"
#include "Deprecated/DocGraphNodes/DocGraphGetNode.h"
#include "Deprecated/DocGraphNodes/DocGraphNodes.h"
#include "Deprecated/DocGraphNodes/DocGraphReturnNode.h"
#include "Deprecated/DocGraphNodes/DocGraphStateNode.h"
#include "Deprecated/DocGraphNodes/DocGraphSendSignalNode.h"
#include "Deprecated/DocGraphNodes/DocGraphSequenceNode.h"
#include "Deprecated/DocGraphNodes/DocGraphSetNode.h"
#include "Deprecated/DocGraphNodes/DocGraphSwitchNode.h"

namespace Schematyc2
{
	namespace DocLogicGraphUtils
	{
		struct SDocVisitor
		{
			inline SDocVisitor(const IScriptFile& _file, CDocGraphBase& _docGraph)
				: file(_file)
				, docGraph(_docGraph)
			{}

			inline void VisitSignal(const IScriptFile& signalFile, const IScriptSignal& signal) const
			{
				stack_string nodeName;
				DocUtils::GetFullElementName(file, signal, nodeName);
				nodeName.insert(0, "Send Signal::");
				docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::SendSignal, SGUID(), signal.GetGUID());
			}

			inline void VisitAbstractInterfaceFunction(const IScriptFile& abstractInterfaceFunctionFile, const IScriptAbstractInterfaceFunction& abstractInterfaceFunction) const
			{
				stack_string nodeName;
				DocUtils::GetFullElementName(abstractInterfaceFunctionFile, abstractInterfaceFunction.GetGUID(), nodeName);
				nodeName.insert(0, "Abstract Interface Function::");
				docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::AbstractInterfaceFunction, abstractInterfaceFunction.GetScopeGUID(), abstractInterfaceFunction.GetGUID());
			}

			const IScriptFile& file;
			CDocGraphBase&     docGraph;
		};

		struct SClassElementVisitor
		{
			inline SClassElementVisitor(const IScriptFile& _file, CDocGraphBase& _docGraph)
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
						file.VisitElements(ScriptElementConstVisitor::FromConstMemberFunction<SClassElementVisitor, &SClassElementVisitor::VisitElement>(*this), element.GetGUID(), false);
						break;
					}
				case EScriptElementType::Container: // #SchematycTODO : Use domain context to visit containers!!!
					{
						{
							stack_string nodeName = "Container Add::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::ContainerAdd, SGUID(), element.GetGUID());
						}
						{
							stack_string nodeName = "Container Remove By Index::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::ContainerRemoveByIndex, SGUID(), element.GetGUID());
						}
						{
							stack_string nodeName = "Container Remove By Value::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::ContainerRemoveByValue, SGUID(), element.GetGUID());
						}
						{
							stack_string nodeName = "Container Clear::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::ContainerClear, SGUID(), element.GetGUID());
						}
						{
							stack_string nodeName = "Container Size::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::ContainerSize, SGUID(), element.GetGUID());
						}
						{
							stack_string nodeName = "Container Get::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::ContainerGet, SGUID(), element.GetGUID());
						}
						{
							stack_string nodeName = "Container Set::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::ContainerSet, SGUID(), element.GetGUID());
						}
						{
							stack_string nodeName = "Container Find By Value::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::ContainerFindByValue, SGUID(), element.GetGUID());
						}
						break;
					}
				case EScriptElementType::Timer: // #SchematycTODO : Use domain context to visit timers!!!
					{
						{
							stack_string nodeName = "Start Timer::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::StartTimer, SGUID(), element.GetGUID());
						}
						{
							stack_string nodeName = "Stop Timer::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::StopTimer, SGUID(), element.GetGUID());
						}
						{
							stack_string nodeName = "Reset Timer::";
							nodeName.append(szElementName);
							docGraph.AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::ResetTimer, SGUID(), element.GetGUID());
						}
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
	CDocLogicGraph::CDocLogicGraph(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName, EScriptGraphType type, const SGUID& contextGUID)
		: CDocGraphBase(file, guid, scopeGUID, szName, type, contextGUID)
		, m_accessor(EAccessor::Private)
	{}

	//////////////////////////////////////////////////////////////////////////
	EAccessor CDocLogicGraph::GetAccessor() const
	{
		return m_accessor;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(enumerator);
		if (enumerator)
		{
			CDocGraphBase::EnumerateDependencies(enumerator);

			if (CDocGraphBase::GetType() == EScriptGraphType::AbstractInterfaceFunction)
			{
				if (const SGUID scopeGUID = CDocGraphBase::GetScopeGUID())
				{
					// scopeGUID is AbstractInterfaceImplementation
					enumerator(scopeGUID);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		RefreshInputsAndOutputs();
		CDocGraphBase::Refresh(params);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
			{
				CDocGraphBase::Serialize(archive);
				const EScriptGraphType type = CDocGraphBase::GetType();
				if((type == EScriptGraphType::Function) || (type == EScriptGraphType::Condition))
				{
					archive(m_accessor, "accessor", "Accessor");
				}
				break;
			}
		case ESerializationPass::Load:
			{
				RefreshInputsAndOutputs();
				CDocGraphBase::Serialize(archive);
				break;
			}
		case ESerializationPass::PostLoad:
			{
				CDocGraphBase::Serialize(archive);
				break;
			}
		case ESerializationPass::Save:
			{
				CDocGraphBase::Serialize(archive);
				SInfoSerializer(*this).Serialize(archive);
				break;
			}
		case ESerializationPass::Edit:
			{
				CDocGraphBase::Serialize(archive);
				SInfoSerializer(*this).Serialize(archive);

				Serialization::SContext fileContext(archive, static_cast<IScriptFile*>(&CDocGraphBase::GetFile())); // #SchematycTODO : Do we still need this?

				switch(CDocGraphBase::GetType())
				{
				case EScriptGraphType::Function:
					{
						archive(CDocGraphBase::GetInputs(), "inputs", "Inputs");
						archive(CDocGraphBase::GetOutputs(), "outputs", "Outputs");
						break;
					}
				case EScriptGraphType::Condition:
					{
						archive(CDocGraphBase::GetInputs(), "inputs", "Inputs");
						for(CScriptVariableDeclaration& output : GetOutputs())
						{
							const IAnyConstPtr& pOutputValue = output.GetValue();
							if(pOutputValue)
							{
								const char* szOutputName = output.GetName();
								archive(*pOutputValue, szOutputName, szOutputName);
							}
						}
						break;
					}
				case EScriptGraphType::AbstractInterfaceFunction:
					{
						// #SchematycTODO : Can we display inputs and outputs in editor?
						break;
					}
				}
				break;
			}
		case ESerializationPass::Validate:
			{
				Validate(archive);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::RefreshAvailableNodes(const CAggregateTypeId& inputTypeId)
	{
		CDocGraphBase::ClearAvailablNodes();
		CDocGraphBase::AddAvailableNode("Sequence", EScriptGraphNodeType::Sequence, SGUID(), SGUID());
		CDocGraphBase::AddAvailableNode("Branch", EScriptGraphNodeType::Branch, SGUID(), SGUID());
		CDocGraphBase::AddAvailableNode("Switch", EScriptGraphNodeType::Switch, SGUID(), SGUID());
		CDocGraphBase::AddAvailableNode("Make Enumeration", EScriptGraphNodeType::MakeEnumeration, SGUID(), SGUID());
		CDocGraphBase::AddAvailableNode("For Loop", EScriptGraphNodeType::ForLoop, SGUID(), SGUID());
		CDocGraphBase::AddAvailableNode("Return", EScriptGraphNodeType::Return, SGUID(), SGUID());
		CDocGraphBase::AddAvailableNode("Comment", EScriptGraphNodeType::Comment, SGUID(), SGUID());

		const IScriptFile&  file = CDocGraphBase::GetFile();
		const SGUID         scopeGUID = CDocGraphBase::GetScopeGUID();
		const IScriptClass* pOwnerClass = DocUtils::FindOwnerClass(file, scopeGUID);
		if(pOwnerClass)
		{
			CDocGraphBase::AddAvailableNode("Get Object", EScriptGraphNodeType::GetObject, SGUID(), pOwnerClass->GetGUID());
		}

		CDomainContext domainContext(SDomainContextScope(&file, scopeGUID));
		VisitEnvGlobalFunctions(domainContext);
		VisitEnvAbstractInterfaces(domainContext);
		VisitEnvComponentMemberFunctions(domainContext);
		VisitEnvActionMemberFunctions(domainContext);
		VisitVariables(domainContext);
		VisitProperties(domainContext);
		VisitGraphs(domainContext);

		// #SchematycTODO : Use domain context to visit everything!

		DocLogicGraphUtils::SDocVisitor docVisitor(file, *this);
		ScriptIncludeRecursionUtils::VisitSignals(file, ScriptIncludeRecursionUtils::SignalVisitor::FromConstMemberFunction<DocLogicGraphUtils::SDocVisitor, &DocLogicGraphUtils::SDocVisitor::VisitSignal>(docVisitor), SGUID(), true);
		ScriptIncludeRecursionUtils::VisitAbstractInterfaceFunctions(file, ScriptIncludeRecursionUtils::AbstractInterfaceFunctionVisitor::FromConstMemberFunction<DocLogicGraphUtils::SDocVisitor, &DocLogicGraphUtils::SDocVisitor::VisitAbstractInterfaceFunction>(docVisitor), SGUID(), true);
		
		DocLogicGraphUtils::SClassElementVisitor classElementVisitor(file, *this);
		const IScriptElement*                    pScopeElement = file.GetElement(scopeGUID);
		CRY_ASSERT(pScopeElement);
		while(pScopeElement)
		{
			file.VisitElements(ScriptElementConstVisitor::FromConstMemberFunction<DocLogicGraphUtils::SClassElementVisitor, &DocLogicGraphUtils::SClassElementVisitor::VisitElement>(classElementVisitor), pScopeElement->GetGUID(), false);
			if(pScopeElement->GetElementType() != EScriptElementType::Class)
			{
				pScopeElement = file.GetElement(pScopeElement->GetScopeGUID());
			}
			else
			{
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IScriptGraphNodePtr CDocLogicGraph::CreateNode(const SGUID& guid, EScriptGraphNodeType type, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
	{
		switch(type)
		{
		case EScriptGraphNodeType::Begin:
			{
				return IScriptGraphNodePtr(new CDocGraphBeginNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::BeginConstructor:
			{
				return IScriptGraphNodePtr(new CDocGraphBeginConstructorNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::BeginDestructor:
			{
				return IScriptGraphNodePtr(new CDocGraphBeginDestructorNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::BeginSignalReceiver:
			{
				return IScriptGraphNodePtr(new CDocGraphBeginSignalReceiverNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Return:
			{
				return IScriptGraphNodePtr(new CDocGraphReturnNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Sequence:
			{
				return IScriptGraphNodePtr(new CDocGraphSequenceNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
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
		case EScriptGraphNodeType::ForLoop:
			{
				return IScriptGraphNodePtr(new CDocGraphForLoopNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ProcessSignal:
			{
				return IScriptGraphNodePtr(new CDocGraphProcessSignalNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::SendSignal:
			{
				return IScriptGraphNodePtr(new CDocGraphSendSignalNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::BroadcastSignal:
			{
				return IScriptGraphNodePtr(new CDocGraphBroadcastSignalNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Function:
			{
				return IScriptGraphNodePtr(new CDocGraphFunctionNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Condition:
			{
				return IScriptGraphNodePtr(new CDocGraphConditionNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::AbstractInterfaceFunction:
			{
				return IScriptGraphNodePtr(new CDocGraphAbstractInterfaceFunctionNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::GetObject:
			{
				return IScriptGraphNodePtr(new CDocGraphGetObjectNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Set:
			{
				return IScriptGraphNodePtr(new CDocGraphSetNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::Get:
			{
				return IScriptGraphNodePtr(new CDocGraphGetNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ContainerAdd:
			{
				return IScriptGraphNodePtr(new CDocGraphContainerAddNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ContainerRemoveByIndex:
			{
				return IScriptGraphNodePtr(new CDocGraphContainerRemoveByIndexNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ContainerRemoveByValue:
			{
				return IScriptGraphNodePtr(new CDocGraphContainerRemoveByValueNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ContainerClear:
			{
				return IScriptGraphNodePtr(new CDocGraphContainerClearNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ContainerSize:
			{
				return IScriptGraphNodePtr(new CDocGraphContainerSizeNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ContainerGet:
			{
				return IScriptGraphNodePtr(new CDocGraphContainerGetNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ContainerSet:
			{
				return IScriptGraphNodePtr(new CDocGraphContainerSetNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ContainerFindByValue:
			{
				return IScriptGraphNodePtr(new CDocGraphContainerFindByValueNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::StartTimer:
			{
				return IScriptGraphNodePtr(new CDocGraphStartTimerNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::StopTimer:
			{
				return IScriptGraphNodePtr(new CDocGraphStopTimerNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
			}
		case EScriptGraphNodeType::ResetTimer:
			{
				return IScriptGraphNodePtr(new CDocGraphResetTimerNode(CDocGraphBase::GetFile(), *this, guid, contextGUID, refGUID, pos));
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
	CDocLogicGraph::SInfoSerializer::SInfoSerializer(CDocLogicGraph& _graph)
		: graph(_graph)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::SInfoSerializer::Serialize(Serialization::IArchive& archive)
	{
		const EScriptGraphType type = graph.GetType();
		if((type == EScriptGraphType::Function) || (type == EScriptGraphType::Condition))
		{
			archive(graph.m_accessor, "accessor", "Accessor");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CDocLogicGraph::RefreshInputsAndOutputs()
	{
		LOADING_TIME_PROFILE_SECTION;
		// #SchematycTODO : Clean up this function, there's a lot of duplicated code here.
		switch(CDocGraphBase::GetType())
		{
		case EScriptGraphType::Condition:
			{
				ScriptVariableDeclarationVector& outputs = GetOutputs();
				if(outputs.empty())
				{
					outputs.push_back(CScriptVariableDeclaration("Result", GetAggregateTypeId<bool>(), MakeAnyShared(bool(false))));
				}
				return true;
			}
		case EScriptGraphType::AbstractInterfaceFunction:
			{
				// Retrieve interface function.
				// #SchematycTODO : Check that inputs and output actually need to be refreshed?
				const IScriptFile&                            file = CDocGraphBase::GetFile();
				const IScriptAbstractInterfaceImplementation* pAbstractInterfaceImplementation = file.GetAbstractInterfaceImplementation(CDocGraphBase::GetScopeGUID());
				if(pAbstractInterfaceImplementation)
				{
					const SGUID abstractInterfaceGUID = pAbstractInterfaceImplementation->GetRefGUID();
					const SGUID abstractInterfaceFunctionGUID = CDocGraphBase::GetContextGUID();
					switch(pAbstractInterfaceImplementation->GetDomain())
					{
					case EDomain::Env:
						{
							IAbstractInterfaceConstPtr pAbstractInterface = gEnv->pSchematyc2->GetEnvRegistry().GetAbstractInterface(abstractInterfaceGUID);
							if(pAbstractInterface)
							{
								IAbstractInterfaceFunctionConstPtr	pAbstractInterfaceFunction = pAbstractInterface->GetFunction(AbstractInterfaceUtils::FindFunction(*pAbstractInterface, abstractInterfaceFunctionGUID));
								CRY_ASSERT(pAbstractInterfaceFunction);
								if(pAbstractInterfaceFunction)
								{
									CDocGraphBase::SetName(pAbstractInterfaceFunction->GetName());
									// Refresh inputs.
									const size_t                     inputCount = pAbstractInterfaceFunction->GetInputCount();
									ScriptVariableDeclarationVector& inputs = GetInputs();
									inputs.resize(inputCount);
									for(size_t inputIdx = 0; inputIdx < inputCount; ++ inputIdx)
									{
										const IAnyConstPtr pNewInputValue = pAbstractInterfaceFunction->GetInputValue(inputIdx);
										inputs[inputIdx] = CScriptVariableDeclaration(pAbstractInterfaceFunction->GetInputName(inputIdx), pNewInputValue ? pNewInputValue->Clone() : IAnyPtr());
									}
									// Refresh outputs.
									const size_t                     outputCount = pAbstractInterfaceFunction->GetOutputCount();
									ScriptVariableDeclarationVector& outputs = GetOutputs();
									outputs.resize(outputCount);
									for(size_t outputIdx = 0; outputIdx < outputCount; ++ outputIdx)
									{
										const IAnyConstPtr pNewOutputValue = pAbstractInterfaceFunction->GetOutputValue(outputIdx);
										outputs[outputIdx] = CScriptVariableDeclaration(pAbstractInterfaceFunction->GetOutputName(outputIdx), pNewOutputValue ? pNewOutputValue->Clone() : IAnyPtr());
									}
									return true;
								}
							}
							break;
						}
					case EDomain::Script:
						{
							ScriptIncludeRecursionUtils::TGetAbstractInterfaceResult abstractInterface = ScriptIncludeRecursionUtils::GetAbstractInterface(file, abstractInterfaceGUID);
							if(abstractInterface.second)
							{
								const IScriptAbstractInterfaceFunction* pAbstractInterfaceFunction = abstractInterface.first->GetAbstractInterfaceFunction(abstractInterfaceFunctionGUID);
								if(pAbstractInterfaceFunction)
								{
									CDocGraphBase::SetName(pAbstractInterfaceFunction->GetName());
									// Refresh inputs.
									const size_t                     inputCount = pAbstractInterfaceFunction->GetInputCount();
									ScriptVariableDeclarationVector& inputs = GetInputs();
									inputs.resize(inputCount);
									for(size_t inputIdx = 0; inputIdx < inputCount; ++ inputIdx)
									{
										const IAnyConstPtr pNewInputValue = pAbstractInterfaceFunction->GetInputValue(inputIdx);
										inputs[inputIdx] = CScriptVariableDeclaration(pAbstractInterfaceFunction->GetInputName(inputIdx), pNewInputValue ? pNewInputValue->Clone() : IAnyPtr());
									}
									// Refresh outputs.
									const size_t                     outputCount = pAbstractInterfaceFunction->GetOutputCount();
									ScriptVariableDeclarationVector& outputs = GetOutputs();
									outputs.resize(outputCount);
									for(size_t outputIdx = 0; outputIdx < outputCount; ++ outputIdx)
									{
										const IAnyConstPtr pNewOutputValue = pAbstractInterfaceFunction->GetOutputValue(outputIdx);
										outputs[outputIdx] = CScriptVariableDeclaration(pAbstractInterfaceFunction->GetOutputName(outputIdx), pNewOutputValue ? pNewOutputValue->Clone() : IAnyPtr());
									}
									return true;
								}
							}
							break;
						}
					}
				}
				break;
			}
		default:
			{
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::Validate(Serialization::IArchive& archive)
	{
		for(CScriptVariableDeclaration& input : GetInputs())
		{
			if(!input.GetValue())
			{
				archive.error(*this, "Failed to instantiate input value: input = %s", input.GetName());
			}
		}

		for(CScriptVariableDeclaration& output : GetOutputs())
		{
			if(!output.GetValue())
			{
				archive.error(*this, "Failed to instantiate output value: output = %s", output.GetName());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::VisitEnvGlobalFunctions(const IDomainContext& domainContext)
	{
		auto visitEnvGlobalFunction = [this, &domainContext] (const IGlobalFunctionConstPtr& pFunction) -> EVisitStatus
		{
			stack_string qualifiedFunctionName;
			domainContext.QualifyName(*pFunction, qualifiedFunctionName);
			const SGUID contextGUID = SGUID();
			const SGUID refGUID = pFunction->GetGUID();
			if(DocLogicGraphUtils::FunctionIsCondition(*pFunction))
			{
				stack_string nodeName = "Condition::";
				nodeName.append(qualifiedFunctionName);
				AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Condition, contextGUID, refGUID);
			}
			{
				stack_string nodeName = "Function::";
				nodeName.append(qualifiedFunctionName);
				AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Function, contextGUID, refGUID);
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitEnvGlobalFunctions(EnvGlobalFunctionVisitor::FromLambdaFunction(visitEnvGlobalFunction));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::VisitEnvAbstractInterfaces(const IDomainContext& domainContext)
	{
		auto visitEnvAbstractInterface = [this, &domainContext] (const IAbstractInterfaceConstPtr& pAbstractInterface) -> EVisitStatus
		{
			for(size_t functionIdx = 0, functionCount = pAbstractInterface->GetFunctionCount(); functionIdx < functionCount; ++ functionIdx)
			{
				IAbstractInterfaceFunctionConstPtr pFunction = pAbstractInterface->GetFunction(functionIdx);
				if(pFunction)
				{
					stack_string nodeName;
					domainContext.QualifyName(*pFunction, nodeName);
					nodeName.insert(0, "Abstract Interface Function::");
					AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::AbstractInterfaceFunction, pAbstractInterface->GetGUID(), pFunction->GetGUID());
				}
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitEnvAbstractInterfaces(EnvAbstractInterfaceVisitor::FromLambdaFunction(visitEnvAbstractInterface));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::VisitEnvComponentMemberFunctions(const IDomainContext& domainContext)
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
			const SGUID componentGUID = pFunction->GetComponentGUID();
			const bool  bFunctionIsCondition = DocLogicGraphUtils::FunctionIsCondition(*pFunction);
			for(const IScriptComponentInstance* pComponentInstance : componentInstances)
			{
				if(pComponentInstance->GetComponentGUID() == componentGUID)
				{
					stack_string qualifiedFunctionName;
					domainContext.QualifyName(*pComponentInstance, *pFunction, EDomainQualifier::Global, qualifiedFunctionName);
					const SGUID contextGUID = pComponentInstance->GetGUID();
					const SGUID refGUID = pFunction->GetGUID();
					if(bFunctionIsCondition)
					{
						stack_string nodeName = "Condition::";
						nodeName.append(qualifiedFunctionName);
						AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Condition, contextGUID, refGUID);
					}
					{
						stack_string nodeName = "Function::";
						nodeName.append(qualifiedFunctionName);
						AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Function, contextGUID, refGUID);
					}
				}
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitEnvComponentMemberFunctions(EnvComponentMemberFunctionVisitor::FromLambdaFunction(visitEnvComponentMemberFunction));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::VisitEnvActionMemberFunctions(const IDomainContext& domainContext)
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
			const SGUID actionGUID = pFunction->GetActionGUID();
			const bool  bFunctionIsCondition = DocLogicGraphUtils::FunctionIsCondition(*pFunction);
			for(const IScriptActionInstance* pActionInstance : actionInstances)
			{
				if(pActionInstance->GetActionGUID() == actionGUID)
				{
					stack_string qualifiedFunctionName;
					domainContext.QualifyName(*pActionInstance, *pFunction, EDomainQualifier::Global, qualifiedFunctionName);
					const SGUID contextGUID = pActionInstance->GetGUID();
					const SGUID refGUID = pFunction->GetGUID();
					if(bFunctionIsCondition)
					{
						stack_string nodeName = "Condition::";
						nodeName.append(qualifiedFunctionName);
						AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Condition, contextGUID, refGUID);
					}
					{
						stack_string nodeName = "Function::";
						nodeName.append(qualifiedFunctionName);
						AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Function, contextGUID, refGUID);
					}
				}
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitEnvActionMemberFunctions(EnvActionMemberFunctionVisitor::FromLambdaFunction(visitEnvActionMemberFunction));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::VisitVariables(const IDomainContext& domainContext)
	{
		auto visitVariable = [this, &domainContext] (const IScriptVariable& variable) -> EVisitStatus
		{
			stack_string qualifiedVariableName;
			domainContext.QualifyName(variable, EDomainQualifier::Global, qualifiedVariableName);
			const SGUID contextGUID = SGUID();
			const SGUID refGUID = variable.GetGUID();
			{
				stack_string nodeName = "Set::";
				nodeName.append(qualifiedVariableName);
				AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Set, contextGUID, refGUID);
			}
			{
				stack_string nodeName = "Get::";
				nodeName.append(qualifiedVariableName);
				AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Get, contextGUID, refGUID);
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitScriptVariables(ScriptVariableConstVisitor::FromLambdaFunction(visitVariable), EDomainScope::Derived);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocLogicGraph::VisitProperties(const IDomainContext& domainContext)
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
	void CDocLogicGraph::VisitGraphs(const IDomainContext& domainContext)
	{
		auto visitGraph = [this, &domainContext] (const IDocGraph& graph) -> EVisitStatus
		{
			switch(graph.GetType())
			{
			case EScriptGraphType::Function:
				{
					stack_string nodeName;
					domainContext.QualifyName(graph, EDomainQualifier::Global, nodeName);
					nodeName.insert(0, "Function::");
					AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Function, SGUID(), graph.GetGUID());
					break;
				}
			case EScriptGraphType::Condition:
				{
					stack_string nodeName;
					domainContext.QualifyName(graph, EDomainQualifier::Global, nodeName);
					nodeName.insert(0, "Condition::");
					AddAvailableNode(nodeName.c_str(), EScriptGraphNodeType::Condition, SGUID(), graph.GetGUID());
				}
			}
			return EVisitStatus::Continue;
		};
		domainContext.VisitDocGraphs(DocGraphConstVisitor::FromLambdaFunction(visitGraph), EDomainScope::Derived);
	}
}
