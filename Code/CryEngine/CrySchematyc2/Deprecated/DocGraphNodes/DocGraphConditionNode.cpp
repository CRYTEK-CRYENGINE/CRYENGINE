// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphConditionNode.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "DomainContext.h"
#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"
#include "Script/ScriptVariableDeclaration.h"

SERIALIZATION_ENUM_BEGIN_NESTED2(Schematyc2, CDocGraphConditionNode, EConditionType, "Schematyc Condition Type")
	SERIALIZATION_ENUM(Schematyc2::CDocGraphConditionNode::EConditionType::EnvGlobal, "EnvGlobal", "Environment Global")
	SERIALIZATION_ENUM(Schematyc2::CDocGraphConditionNode::EConditionType::EnvComponent, "EnvComponent", "Environment Component")
	SERIALIZATION_ENUM(Schematyc2::CDocGraphConditionNode::EConditionType::EnvAction, "EnvAction", "Environment Action")
	SERIALIZATION_ENUM(Schematyc2::CDocGraphConditionNode::EConditionType::Script, "Script", "Script")
SERIALIZATION_ENUM_END()

namespace Schematyc2
{
	namespace DocGraphConditionNodeUtils
	{
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
	CDocGraphConditionNode::CDocGraphConditionNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "", EScriptGraphNodeType::Condition, contextGUID, refGUID, pos)
	{
		m_conditionType = ResolveConditionType();
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other)); // #SchematycTODO : Remove once we've moved over to unified load/save?
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphConditionNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphConditionNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphConditionNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("True", EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("False", EScriptGraphPortFlags::Execute);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		CDomainContext     domainContext(SDomainContextScope(&file, graph.GetScopeGUID()));
		switch(m_conditionType)
		{
		case EConditionType::EnvGlobal:
			{
				IGlobalFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(refGUID);
				if(pFunction)
				{
					if(DocGraphConditionNodeUtils::FunctionIsCondition(*pFunction))
					{
						stack_string name;
						domainContext.QualifyName(*pFunction, name);
						CDocGraphNodeBase::SetName(name.c_str());
						const size_t functionInputCount = pFunction->GetInputCount();
						m_inputValues.resize(EInput::FirstFunctionInput + functionInputCount);
						for(size_t functionInputIdx = 0; functionInputIdx < functionInputCount; ++ functionInputIdx)
						{
							IAnyConstPtr pFunctionInputValue = pFunction->GetInputValue(functionInputIdx);
							SCHEMATYC2_SYSTEM_ASSERT(pFunctionInputValue);
							if(pFunctionInputValue)
							{
								CDocGraphNodeBase::AddInput(pFunction->GetInputName(functionInputIdx), EScriptGraphPortFlags::None, pFunctionInputValue->GetTypeInfo().GetTypeId());
								IAnyPtr& pInputValue = m_inputValues[EInput::FirstFunctionInput + functionInputIdx];
								if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pFunctionInputValue->GetTypeInfo().GetTypeId()))
								{
									pInputValue = pFunctionInputValue->Clone();
								}
							}
						}
					}
				}
				break;
			}
		case EConditionType::EnvComponent:
			{
				IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID);
				if(pFunction)
				{
					if(DocGraphConditionNodeUtils::FunctionIsCondition(*pFunction))
					{
						stack_string                    name;
						const IScriptComponentInstance* pComponentInstance = domainContext.GetScriptComponentInstance(contextGUID);
						if(pComponentInstance)
						{
							domainContext.QualifyName(*pComponentInstance, *pFunction, EDomainQualifier::Local, name);
						}
						CDocGraphNodeBase::SetName(name.c_str());
						const size_t functionInputCount = pFunction->GetInputCount();
						m_inputValues.resize(EInput::FirstFunctionInput + functionInputCount);
						for(size_t functionInputIdx = 0; functionInputIdx < functionInputCount; ++ functionInputIdx)
						{
							IAnyConstPtr pFunctionInputValue = pFunction->GetInputValue(functionInputIdx);
							SCHEMATYC2_SYSTEM_ASSERT(pFunctionInputValue);
							if(pFunctionInputValue)
							{
								CDocGraphNodeBase::AddInput(pFunction->GetInputName(functionInputIdx), EScriptGraphPortFlags::None, pFunctionInputValue->GetTypeInfo().GetTypeId());
								IAnyPtr& pInputValue = m_inputValues[EInput::FirstFunctionInput + functionInputIdx];
								if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pFunctionInputValue->GetTypeInfo().GetTypeId()))
								{
									pInputValue = pFunctionInputValue->Clone();
								}
							}
						}
					}
				}
				break;
			}
		case EConditionType::EnvAction:
			{
				IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID);
				if(pFunction)
				{
					if(DocGraphConditionNodeUtils::FunctionIsCondition(*pFunction))
					{
						stack_string                 name;
						const IScriptActionInstance* pActionInstance = domainContext.GetScriptActionInstance(contextGUID);
						if(pActionInstance)
						{
							domainContext.QualifyName(*pActionInstance, *pFunction, EDomainQualifier::Local, name);
						}
						CDocGraphNodeBase::SetName(name.c_str());
						const size_t functionInputCount = pFunction->GetInputCount();
						m_inputValues.resize(EInput::FirstFunctionInput + functionInputCount);
						for(size_t functionInputIdx = 0; functionInputIdx < functionInputCount; ++ functionInputIdx)
						{
							IAnyConstPtr pFunctionInputValue = pFunction->GetInputValue(functionInputIdx);
							SCHEMATYC2_SYSTEM_ASSERT(pFunctionInputValue);
							if(pFunctionInputValue)
							{
								CDocGraphNodeBase::AddInput(pFunction->GetInputName(functionInputIdx), EScriptGraphPortFlags::None, pFunctionInputValue->GetTypeInfo().GetTypeId());
								IAnyPtr& pInputValue = m_inputValues[EInput::FirstFunctionInput + functionInputIdx];
								if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pFunctionInputValue->GetTypeInfo().GetTypeId()))
								{
									pInputValue = pFunctionInputValue->Clone();
								}
							}
						}
					}
				}
				break;
			}
		case EConditionType::Script:
			{
				const IDocGraph* pGraph = domainContext.GetDocGraph(refGUID);
				if(pGraph)
				{
					stack_string name;
					domainContext.QualifyName(*pGraph, EDomainQualifier::Local, name);
					CDocGraphNodeBase::SetName(name.c_str());
					const size_t graphInputCount = pGraph->GetInputCount();
					m_inputValues.resize(EInput::FirstFunctionInput + graphInputCount);
					for(size_t graphInputIdx = 0; graphInputIdx < graphInputCount; ++ graphInputIdx)
					{
						IAnyConstPtr pGraphInputValue = pGraph->GetInputValue(graphInputIdx);
						SCHEMATYC2_SYSTEM_ASSERT(pGraphInputValue);
						if(pGraphInputValue)
						{
							CDocGraphNodeBase::AddInput(pGraph->GetInputName(graphInputIdx), EScriptGraphPortFlags::None, pGraphInputValue->GetTypeInfo().GetTypeId());
							IAnyPtr& pInputValue = m_inputValues[EInput::FirstFunctionInput + graphInputIdx];
							if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pGraphInputValue->GetTypeInfo().GetTypeId()))
							{
								pInputValue = pGraphInputValue->Clone();
							}
						}
					}
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);

		if(archive.isEdit() && ((m_conditionType == EConditionType::EnvComponent) || (m_conditionType == EConditionType::EnvAction)))
		{
			EditContext(archive);
		}

		archive(m_conditionType, "conditionType");

		if(SerializationContext::GetPass(archive) == ESerializationPass::PostLoad)
		{
			Refresh(SScriptRefreshParams(EScriptRefreshReason::Other)); // Ensure inputs exist before reading values. There should be a cleaner way to do this!
		}

		for(size_t inputIdx = EInput::FirstFunctionInput, inputCount = CDocGraphNodeBase::GetInputCount(); inputIdx < inputCount; ++ inputIdx)
		{
			const char* szInputName = CDocGraphNodeBase::GetInputName(inputIdx);
			archive(*m_inputValues[inputIdx], szInputName, szInputName);
		}

		// Patch-up old documents.
		if(m_conditionType == EConditionType::Unknown)
		{
			m_conditionType = ResolveConditionType();
		}
		if(!CDocGraphNodeBase::GetContextGUID() && ((m_conditionType == EConditionType::EnvComponent) || (m_conditionType == EConditionType::EnvAction)))
		{
			CDocGraphNodeBase::SetContextGUID(ResolveContextGUID());
		}

		if(archive.isValidation())
		{
			Validate(archive);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				CompileInputs(compiler);
				break;
			}
		case EDocGraphSequenceStep::BeginOutput:
			{
				switch(portIdx)
				{
				case EOutput::True:
					{
						CompileTrue(compiler);
						break;
					}
				case EOutput::False:
					{
						CompileFalse(compiler);
						break;
					}
				}
				break;
			}
		case EDocGraphSequenceStep::EndInput:
			{
				CompileEnd(compiler);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphConditionNode::ResolveContextGUID() const
	{
		const SGUID refGUID = CDocGraphNodeBase::GetRefGUID();
		if(refGUID)
		{
			const IScriptFile& file = CDocGraphNodeBase::GetFile();
			const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
			CDomainContext     domainContext(SDomainContextScope(&file, graph.GetScopeGUID()));
			if(IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID))
			{
				const SGUID componentGUID = pFunction->GetComponentGUID();
				SGUID       componentInstanceGUID;
				auto visitScriptComponentInstance = [&componentGUID, &componentInstanceGUID] (const IScriptComponentInstance& componentInstance) -> EVisitStatus
				{
					if(componentInstance.GetComponentGUID() == componentGUID)
					{
						componentInstanceGUID = componentInstance.GetGUID();
						return EVisitStatus::Stop;
					}
					return EVisitStatus::Continue;
				};
				domainContext.VisitScriptComponentInstances(ScriptComponentInstanceConstVisitor::FromLambdaFunction(visitScriptComponentInstance), EDomainScope::Local);
				SCHEMATYC2_SYSTEM_ASSERT(componentInstanceGUID);
				return componentInstanceGUID;
			}
			else if(IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID))
			{
				const SGUID         actionGUID = pFunction->GetActionGUID();
				SGUID               actionInstanceGUID;
				const IScriptState* pState = DocUtils::FindOwnerState(file, graph.GetScopeGUID());
				while(pState)
				{
					TScriptActionInstanceConstVector actionInstances;
					DocUtils::CollectActionInstances(file, pState->GetGUID(), true, actionInstances);
					for(const IScriptActionInstance* pActionInstance : actionInstances)
					{
						if(pActionInstance->GetActionGUID() == actionGUID)
						{
							actionInstanceGUID = pActionInstance->GetGUID();
							break;
						}
					}
					if(!actionInstanceGUID)
					{
						pState = DocUtils::FindOwnerState(file, pState->GetScopeGUID());
					}
					else
					{
						break;
					}
				}
				if(!actionInstanceGUID)
				{
					const IScriptClass* pClass = DocUtils::FindOwnerClass(file, graph.GetScopeGUID());
					CRY_ASSERT(pClass);
					if(pClass)
					{
						TScriptActionInstanceConstVector actionInstances;
						DocUtils::CollectActionInstances(file, pClass->GetGUID(), true, actionInstances);
						for(const IScriptActionInstance* pActionInstance : actionInstances)
						{
							if(pActionInstance->GetActionGUID() == actionGUID)
							{
								actionInstanceGUID = pActionInstance->GetGUID();
								break;
							}
						}
					}
				}
				SCHEMATYC2_SYSTEM_ASSERT(actionInstanceGUID);
				return actionInstanceGUID;
			}
		}
		return SGUID();
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphConditionNode::EConditionType CDocGraphConditionNode::ResolveConditionType() const
	{
		const SGUID refGUID = CDocGraphNodeBase::GetRefGUID();
		if(refGUID)
		{
			const IScriptFile& file = CDocGraphNodeBase::GetFile();
			const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
			CDomainContext     domainContext(SDomainContextScope(&file, graph.GetScopeGUID()));
			if(IGlobalFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(refGUID))
			{
				return EConditionType::EnvGlobal;
			}
			else if(IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID))
			{
				return EConditionType::EnvComponent;
			}
			else if(IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID))
			{
				return EConditionType::EnvAction;
			}
			else if(const IDocGraph* pGraph = domainContext.GetDocGraph(refGUID))
			{
				return EConditionType::Script;
			}
		}
		return EConditionType::Unknown;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::EditContext(Serialization::IArchive& archive)
	{
		typedef std::vector<SGUID> ContextGUIDs;

		Serialization::StringList contextNames;
		ContextGUIDs              contextGUIDs;

		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		CDomainContext     domainContext(SDomainContextScope(&file, graph.GetScopeGUID()));
		switch(m_conditionType)
		{
		case EConditionType::EnvComponent:
			{
				IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID);
				if(pFunction)
				{
					const SGUID componentGUID = pFunction->GetComponentGUID();
					auto visitScriptComponentInstance = [&contextNames, &contextGUIDs, &domainContext, &componentGUID] (const IScriptComponentInstance& componentInstance) -> EVisitStatus
					{
						if(componentInstance.GetComponentGUID() == componentGUID)
						{
							stack_string qualifiedName;
							domainContext.QualifyName(componentInstance, EDomainQualifier::Local, qualifiedName);
							contextNames.push_back(qualifiedName.c_str());
							contextGUIDs.push_back(componentInstance.GetGUID());
						}
						return EVisitStatus::Continue;
					};
					domainContext.VisitScriptComponentInstances(ScriptComponentInstanceConstVisitor::FromLambdaFunction(visitScriptComponentInstance), EDomainScope::Local);
				}
				break;
			}
		case EConditionType::EnvAction:
			{
				IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID);
				if(pFunction)
				{
					const SGUID actionGUID = pFunction->GetActionGUID();
					auto visitScriptActionInstance = [&contextNames, &contextGUIDs, &domainContext, &actionGUID] (const IScriptActionInstance& actionInstance) -> EVisitStatus
					{
						if(actionInstance.GetActionGUID() == actionGUID)
						{
							stack_string qualifiedName;
							domainContext.QualifyName(actionInstance, EDomainQualifier::Local, qualifiedName);
							contextNames.push_back(qualifiedName.c_str());
							contextGUIDs.push_back(actionInstance.GetGUID());
						}
						return EVisitStatus::Continue;
					};
					domainContext.VisitScriptActionInstances(ScriptActionInstanceConstVisitor::FromLambdaFunction(visitScriptActionInstance), EDomainScope::Local);
				}
				break;
			}
		}

		int                          index = 0;
		ContextGUIDs::const_iterator itContextGUID = std::find(contextGUIDs.begin(), contextGUIDs.end(), contextGUID);
		if(itContextGUID == contextGUIDs.end())
		{
			contextNames.push_back("None");
			contextGUIDs.push_back(contextGUID);
		}
		else
		{
			index = static_cast<int>(itContextGUID - contextGUIDs.begin());
		}

		Serialization::StringListValue contextName(contextNames, index);
		archive(contextName, "context", "Context");

		if(archive.isInput())
		{
			SetContextGUID(contextGUIDs[contextName.index()]);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::Validate(Serialization::IArchive& archive)
	{
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		CDomainContext     domainContext(SDomainContextScope(&file, graph.GetScopeGUID()));
		switch(m_conditionType)
		{
		case EConditionType::EnvGlobal:
			{
				IGlobalFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(refGUID);
				if(pFunction)
				{
					if(!DocGraphConditionNodeUtils::FunctionIsCondition(*pFunction))
					{
						archive.error(*this, "Global function is not valid condition!");
					}
				}
				else
				{
					archive.error(*this, "Unable to retrieve global function!");
				}
				break;
			}
		case EConditionType::EnvComponent:
			{
				const IScriptComponentInstance* pComponentInstance = domainContext.GetScriptComponentInstance(contextGUID);
				if(!pComponentInstance)
				{
					archive.error(*this, "Unable to retrieve component instance!");
				}
				IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID);
				if(pFunction)
				{
					if(!DocGraphConditionNodeUtils::FunctionIsCondition(*pFunction))
					{
						archive.error(*this, "Component function is not valid condition!");
					}
				}
				else
				{
					archive.error(*this, "Unable to retrieve component function!");
				}
				break;
			}
		case EConditionType::EnvAction:
			{
				const IScriptActionInstance* pActionInstance = domainContext.GetScriptActionInstance(contextGUID);
				if(!pActionInstance)
				{
					archive.error(*this, "Unable to retrieve action instance!");
				}
				IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID);
				if(pFunction)
				{
					if(!DocGraphConditionNodeUtils::FunctionIsCondition(*pFunction))
					{
						archive.error(*this, "Action function is not valid condition!");
					}
				}
				else
				{
					archive.error(*this, "Unable to retrieve action function!");
				}
				break;
			}
		case EConditionType::Script:
			{
				const IDocGraph* pGraph = domainContext.GetDocGraph(refGUID);
				if(!pGraph || (pGraph->GetType() != EScriptGraphType::Condition))
				{
					archive.error(*this, "Unable to retrieve script condition!");
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		compiler.CreateStackFrame(*this, EStackFrame::FirstFunctionInput);
		switch(m_conditionType)
		{
		case EConditionType::EnvGlobal:
			{
				IGlobalFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(refGUID);
				if(pFunction)
				{
					if(DocGraphConditionNodeUtils::FunctionIsCondition(*pFunction))
					{
						DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstFunctionInput, m_inputValues, compiler);
						compiler.CallGlobalFunction(refGUID);
					}
				}
				break;
			}
		case EConditionType::EnvComponent:
			{
				IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID);
				if(pFunction)
				{
					if(DocGraphConditionNodeUtils::FunctionIsCondition(*pFunction))
					{
						const size_t componentInstanceIdx = LibUtils::FindComponentInstanceByGUID(compiler.GetLibClass(), contextGUID);
						SCHEMATYC2_SYSTEM_ASSERT(componentInstanceIdx != INVALID_INDEX);
						if(componentInstanceIdx != INVALID_INDEX)
						{
							const CAny<uint32> componentIdx = MakeAny(static_cast<uint32>(componentInstanceIdx));
							compiler.Push(componentIdx);
							DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstFunctionInput, m_inputValues, compiler);
							compiler.CallComponentMemberFunction(refGUID);
						}
					}
				}
				break;
			}
		case EConditionType::EnvAction:
			{
				IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID);
				if(pFunction)
				{
					if(DocGraphConditionNodeUtils::FunctionIsCondition(*pFunction))
					{
						const size_t actionInstanceIdx = LibUtils::FindActionInstanceByGUID(compiler.GetLibClass(), contextGUID);
						SCHEMATYC2_SYSTEM_ASSERT(actionInstanceIdx != INVALID_INDEX);
						if(actionInstanceIdx != INVALID_INDEX)
						{
							const CAny<uint32> actionIdx = MakeAny(static_cast<uint32>(actionInstanceIdx));
							compiler.Push(actionIdx);
							DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstFunctionInput, m_inputValues, compiler);
							compiler.CallActionMemberFunction(refGUID);
						}
					}
				}
				break;
			}
		case EConditionType::Script:
			{
				const LibFunctionId libFunctionId = compiler.GetLibClass().GetFunctionId(refGUID);
				if(libFunctionId != LibFunctionId::s_invalid)
				{
					DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstFunctionInput, m_inputValues, compiler);
					compiler.CallLibFunction(libFunctionId);
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::CompileTrue(IDocGraphNodeCompiler& compiler) const
	{
		compiler.BranchIfZero(*this, EMarker::False);
		compiler.CollapseStackFrame(*this, EStackFrame::FirstFunctionInput);
		compiler.CreateStackFrame(*this, EStackFrame::FirstFunctionInput);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::CompileFalse(IDocGraphNodeCompiler& compiler) const
	{
		compiler.Branch(*this, EMarker::End);
		compiler.CreateMarker(*this, EMarker::False);
		compiler.CollapseStackFrame(*this, EStackFrame::FirstFunctionInput);
		compiler.CreateStackFrame(*this, EStackFrame::FirstFunctionInput);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphConditionNode::CompileEnd(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CreateMarker(*this, EMarker::End);
		compiler.CollapseStackFrame(*this, EStackFrame::FirstFunctionInput);
	}
}
