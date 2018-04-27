// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphFunctionNode.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "DomainContext.h"
#include "Script/ScriptVariableDeclaration.h"

SERIALIZATION_ENUM_BEGIN_NESTED2(Schematyc2, CDocGraphFunctionNode, EFunctionType, "Schematyc Function Type")
	SERIALIZATION_ENUM(Schematyc2::CDocGraphFunctionNode::EFunctionType::EnvGlobal, "EnvGlobal", "Environment Global")
	SERIALIZATION_ENUM(Schematyc2::CDocGraphFunctionNode::EFunctionType::EnvComponent, "EnvComponent", "Environment Component")
	SERIALIZATION_ENUM(Schematyc2::CDocGraphFunctionNode::EFunctionType::EnvAction, "EnvAction", "Environment Action")
	SERIALIZATION_ENUM(Schematyc2::CDocGraphFunctionNode::EFunctionType::Script, "Script", "Script")
SERIALIZATION_ENUM_END()

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphFunctionNode::CDocGraphFunctionNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "", EScriptGraphNodeType::Function, contextGUID, refGUID, pos)
	{
		m_functionType = ResolveFunctionType();
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other)); // #SchematycTODO : Remove once we've moved over to unified load/save?
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphFunctionNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphFunctionNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphFunctionNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphFunctionNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphFunctionNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphFunctionNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::Execute);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		CDomainContext     domainContext(SDomainContextScope(&file, graph.GetScopeGUID()));
		switch(m_functionType)
		{
		case EFunctionType::EnvGlobal:
			{
				IGlobalFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(refGUID);
				if(pFunction)
				{
					stack_string name;
					domainContext.QualifyName(*pFunction, name);
					CDocGraphNodeBase::SetName(name.c_str());
					const size_t functionInputCount = pFunction->GetInputCount();
					m_inputValues.resize(EInput::FirstParam + functionInputCount);
					for(size_t functionInputIdx = 0; functionInputIdx < functionInputCount; ++ functionInputIdx)
					{
						IAnyConstPtr pFunctionInputValue = pFunction->GetInputValue(functionInputIdx);
						CRY_ASSERT(pFunctionInputValue);
						if(pFunctionInputValue)
						{
							CDocGraphNodeBase::AddInput(pFunction->GetInputName(functionInputIdx), EScriptGraphPortFlags::None, pFunctionInputValue->GetTypeInfo().GetTypeId());
							IAnyPtr& pInputValue = m_inputValues[EInput::FirstParam + functionInputIdx];
							if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pFunctionInputValue->GetTypeInfo().GetTypeId()))
							{
								pInputValue = pFunctionInputValue->Clone();
							}
						}
					}
					const size_t functionOutputCount = pFunction->GetOutputCount();
					m_outputValues.resize(EOutput::FirstParam + functionOutputCount);
					for(size_t functionOutputIdx = 0; functionOutputIdx < functionOutputCount; ++ functionOutputIdx)
					{
						IAnyConstPtr	pFunctionOutputValue = pFunction->GetOutputValue(functionOutputIdx);
						CRY_ASSERT(pFunctionOutputValue);
						if(pFunctionOutputValue)
						{
							CDocGraphNodeBase::AddOutput(pFunction->GetOutputName(functionOutputIdx), EScriptGraphPortFlags::MultiLink, pFunctionOutputValue->GetTypeInfo().GetTypeId());
							m_outputValues[EOutput::FirstParam + functionOutputIdx] = pFunctionOutputValue->Clone();
						}
					}
				}
				break;
			}
		case EFunctionType::EnvComponent:
			{
				IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID);
				if(pFunction)
				{
					stack_string                    name;
					const IScriptComponentInstance* pComponentInstance = domainContext.GetScriptComponentInstance(contextGUID);
					if(pComponentInstance)
					{
						domainContext.QualifyName(*pComponentInstance, *pFunction, EDomainQualifier::Local, name);
					}
					CDocGraphNodeBase::SetName(name.c_str());
					const size_t functionInputCount = pFunction->GetInputCount();
					m_inputValues.resize(EInput::FirstParam + functionInputCount);
					for(size_t functionInputIdx = 0; functionInputIdx < functionInputCount; ++ functionInputIdx)
					{
						IAnyConstPtr pFunctionInputValue = pFunction->GetInputValue(functionInputIdx);
						CRY_ASSERT(pFunctionInputValue);
						if(pFunctionInputValue)
						{
							CDocGraphNodeBase::AddInput(pFunction->GetInputName(functionInputIdx), EScriptGraphPortFlags::None, pFunctionInputValue->GetTypeInfo().GetTypeId());
							IAnyPtr& pInputValue = m_inputValues[EInput::FirstParam + functionInputIdx];
							if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pFunctionInputValue->GetTypeInfo().GetTypeId()))
							{
								pInputValue = pFunctionInputValue->Clone();
							}
						}
					}
					const size_t functionOutputCount = pFunction->GetOutputCount();
					m_outputValues.resize(EOutput::FirstParam + functionOutputCount);
					for(size_t functionOutputIdx = 0; functionOutputIdx < functionOutputCount; ++ functionOutputIdx)
					{
						IAnyConstPtr pFunctionOutputValue = pFunction->GetOutputValue(functionOutputIdx);
						CRY_ASSERT(pFunctionOutputValue);
						if(pFunctionOutputValue)
						{
							CDocGraphNodeBase::AddOutput(pFunction->GetOutputName(functionOutputIdx), EScriptGraphPortFlags::MultiLink, pFunctionOutputValue->GetTypeInfo().GetTypeId());
							m_outputValues[EOutput::FirstParam + functionOutputIdx] = pFunctionOutputValue->Clone();
						}
					}
				}
				break;
			}
		case EFunctionType::EnvAction:
			{
				IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID);
				if(pFunction)
				{
					stack_string                 name;
					const IScriptActionInstance* pActionInstance = domainContext.GetScriptActionInstance(contextGUID);
					if(pActionInstance)
					{
						domainContext.QualifyName(*pActionInstance, *pFunction, EDomainQualifier::Local, name);
					}
					CDocGraphNodeBase::SetName(name.c_str());
					const size_t functionInputCount = pFunction->GetInputCount();
					m_inputValues.resize(EInput::FirstParam + functionInputCount);
					for(size_t functionInputIdx = 0; functionInputIdx < functionInputCount; ++ functionInputIdx)
					{
						IAnyConstPtr pFunctionInputValue = pFunction->GetInputValue(functionInputIdx);
						CRY_ASSERT(pFunctionInputValue);
						if(pFunctionInputValue)
						{
							CDocGraphNodeBase::AddInput(pFunction->GetInputName(functionInputIdx), EScriptGraphPortFlags::None, pFunctionInputValue->GetTypeInfo().GetTypeId());
							IAnyPtr& pInputValue = m_inputValues[EInput::FirstParam + functionInputIdx];
							if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pFunctionInputValue->GetTypeInfo().GetTypeId()))
							{
								pInputValue = pFunctionInputValue->Clone();
							}
						}
					}
					const size_t functionOutputCount = pFunction->GetOutputCount();
					m_outputValues.resize(EOutput::FirstParam + functionOutputCount);
					for(size_t functionOutputIdx = 0; functionOutputIdx < functionOutputCount; ++ functionOutputIdx)
					{
						IAnyConstPtr	pFunctionOutputValue = pFunction->GetOutputValue(functionOutputIdx);
						CRY_ASSERT(pFunctionOutputValue);
						if(pFunctionOutputValue)
						{
							CDocGraphNodeBase::AddOutput(pFunction->GetOutputName(functionOutputIdx), EScriptGraphPortFlags::MultiLink, pFunctionOutputValue->GetTypeInfo().GetTypeId());
							m_outputValues[EOutput::FirstParam + functionOutputIdx] = pFunctionOutputValue->Clone();
						}
					}
				}
				break;
			}
		case EFunctionType::Script:
			{
				const IDocGraph* pGraph = domainContext.GetDocGraph(refGUID);
				if(pGraph)
				{
					stack_string name;
					domainContext.QualifyName(*pGraph, EDomainQualifier::Local, name);
					CDocGraphNodeBase::SetName(name.c_str());
					const size_t graphInputCount = pGraph->GetInputCount();
					m_inputValues.resize(EInput::FirstParam + graphInputCount);
					for(size_t graphInputIdx = 0; graphInputIdx < graphInputCount; ++ graphInputIdx)
					{
						IAnyConstPtr pGraphInputValue = pGraph->GetInputValue(graphInputIdx);
						CRY_ASSERT(pGraphInputValue);
						if(pGraphInputValue)
						{
							CDocGraphNodeBase::AddInput(pGraph->GetInputName(graphInputIdx), EScriptGraphPortFlags::None, pGraphInputValue->GetTypeInfo().GetTypeId());
							IAnyPtr& pInputValue = m_inputValues[EInput::FirstParam + graphInputIdx];
							if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pGraphInputValue->GetTypeInfo().GetTypeId()))
							{
								pInputValue = pGraphInputValue->Clone();
							}
						}
					}
					const size_t graphOutputCount = pGraph->GetOutputCount();
					m_outputValues.resize(EOutput::FirstParam + graphOutputCount);
					for(size_t graphOutputIdx = 0; graphOutputIdx < graphOutputCount; ++ graphOutputIdx)
					{
						IAnyConstPtr pGraphOutputValue = pGraph->GetOutputValue(graphOutputIdx);
						CRY_ASSERT(pGraphOutputValue);
						if(pGraphOutputValue)
						{
							CDocGraphNodeBase::AddOutput(pGraph->GetOutputName(graphOutputIdx), EScriptGraphPortFlags::MultiLink, pGraphOutputValue->GetTypeInfo().GetTypeId());
							m_outputValues[EOutput::FirstParam + graphOutputIdx] = pGraphOutputValue->Clone();
						}
					}
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphFunctionNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);

		if(archive.isEdit() && ((m_functionType == EFunctionType::EnvComponent) || (m_functionType == EFunctionType::EnvAction)))
		{
			EditContext(archive);
		}

		archive(m_functionType, "functionType");

		if(SerializationContext::GetPass(archive) == ESerializationPass::PostLoad)
		{
			Refresh(SScriptRefreshParams(EScriptRefreshReason::Other)); // Ensure inputs exist before reading values. There should be a cleaner way to do this!
		}

		for(size_t inputIdx = EInput::FirstParam, inputCount = CDocGraphNodeBase::GetInputCount(); inputIdx < inputCount; ++ inputIdx)
		{
			const char* szInputName = CDocGraphNodeBase::GetInputName(inputIdx);
			archive(*m_inputValues[inputIdx], szInputName, szInputName);
		}

		// Patch-up old documents.
		if(m_functionType == EFunctionType::Unknown)
		{
			m_functionType = ResolveFunctionType();
		}
		if(!CDocGraphNodeBase::GetContextGUID() && ((m_functionType == EFunctionType::EnvComponent) || (m_functionType == EFunctionType::EnvAction)))
		{
			CDocGraphNodeBase::SetContextGUID(ResolveContextGUID());
		}

		if(archive.isValidation())
		{
			Validate(archive);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphFunctionNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphFunctionNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphFunctionNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				CompileInputs(compiler);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CDocGraphFunctionNode::ResolveContextGUID() const
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
	CDocGraphFunctionNode::EFunctionType CDocGraphFunctionNode::ResolveFunctionType() const
	{
		const SGUID refGUID = CDocGraphNodeBase::GetRefGUID();
		if(refGUID)
		{
			const IScriptFile& file = CDocGraphNodeBase::GetFile();
			const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
			CDomainContext     domainContext(SDomainContextScope(&file, graph.GetScopeGUID()));
			if(IGlobalFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(refGUID))
			{
				return EFunctionType::EnvGlobal;
			}
			else if(IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID))
			{
				return EFunctionType::EnvComponent;
			}
			else if(IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID))
			{
				return EFunctionType::EnvAction;
			}
			else if(const IDocGraph* pGraph = domainContext.GetDocGraph(refGUID))
			{
				return EFunctionType::Script;
			}
		}
		return EFunctionType::Unknown;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphFunctionNode::EditContext(Serialization::IArchive& archive)
	{
		typedef std::vector<SGUID> ContextGUIDs;

		Serialization::StringList contextNames;
		ContextGUIDs              contextGUIDs;

		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		CDomainContext     domainContext(SDomainContextScope(&file, graph.GetScopeGUID()));
		switch(m_functionType)
		{
		case EFunctionType::EnvComponent:
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
		case EFunctionType::EnvAction:
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
	void CDocGraphFunctionNode::Validate(Serialization::IArchive& archive)
	{
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		CDomainContext     domainContext(SDomainContextScope(&file, graph.GetScopeGUID()));
		switch(m_functionType)
		{
		case EFunctionType::EnvGlobal:
			{
				IGlobalFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(refGUID);
				if(!pFunction)
				{
					archive.error(*this, "Unable to retrieve global function!");
				}
				break;
			}
		case EFunctionType::EnvComponent:
			{
				const IScriptComponentInstance* pComponentInstance = domainContext.GetScriptComponentInstance(contextGUID);
				if(!pComponentInstance)
				{
					archive.error(*this, "Unable to retrieve component instance!");
				}
				IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID);
				if(!pFunction)
				{
					archive.error(*this, "Unable to retrieve component function!");
				}
				break;
			}
		case EFunctionType::EnvAction:
			{
				const IScriptActionInstance* pActionInstance = domainContext.GetScriptActionInstance(contextGUID);
				if(!pActionInstance)
				{
					archive.error(*this, "Unable to retrieve action instance!");
				}
				IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID);
				if(!pFunction)
				{
					archive.error(*this, "Unable to retrieve action function!");
				}
				break;
			}
		case EFunctionType::Script:
			{
				const IDocGraph* pGraph = domainContext.GetDocGraph(refGUID);
				if(!pGraph)
				{
					archive.error(*this, "Unable to retrieve script function!");
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphFunctionNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		switch(m_functionType)
		{
		case EFunctionType::EnvGlobal:
			{
				IGlobalFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetGlobalFunction(refGUID);
				if(pFunction)
				{
					if (pFunction->AreInputParamsResources())
					{
						for (IAnyConstPtr input : m_inputValues)
						{
							if (input)
							{
								compiler.GetLibClass().AddPrecacheResource(input);
							}
						}
					}
					DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstParam, m_inputValues, compiler);
					compiler.CallGlobalFunction(refGUID);
				}
				break;
			}
		case EFunctionType::EnvComponent:
			{
				IComponentMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetComponentMemberFunction(refGUID);
				if(pFunction)
				{
					const size_t componentInstanceIdx = LibUtils::FindComponentInstanceByGUID(compiler.GetLibClass(), contextGUID);
					SCHEMATYC2_SYSTEM_ASSERT(componentInstanceIdx != INVALID_INDEX);
					if(componentInstanceIdx != INVALID_INDEX)
					{
						const CAny<uint32> componentIdx = MakeAny(static_cast<uint32>(componentInstanceIdx));
						compiler.Push(componentIdx);
						DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstParam, m_inputValues, compiler);
						compiler.CallComponentMemberFunction(refGUID);
					}
				}
				break;
			}
		case EFunctionType::EnvAction:
			{
				IActionMemberFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetActionMemberFunction(refGUID);
				if(pFunction)
				{
					const size_t actionInstanceIdx = LibUtils::FindActionInstanceByGUID(compiler.GetLibClass(), contextGUID);
					SCHEMATYC2_SYSTEM_ASSERT(actionInstanceIdx != INVALID_INDEX);
					if(actionInstanceIdx != INVALID_INDEX)
					{
						const CAny<uint32> actionIdx = MakeAny(static_cast<uint32>(actionInstanceIdx));
						compiler.Push(actionIdx);
						DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstParam, m_inputValues, compiler);
						compiler.CallActionMemberFunction(refGUID);
					}
				}
				break;
			}
		case EFunctionType::Script:
			{
				const LibFunctionId libFunctionId = compiler.GetLibClass().GetFunctionId(refGUID);
				if(libFunctionId != LibFunctionId::s_invalid)
				{
					DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstParam, m_inputValues, compiler);
					compiler.CallLibFunction(libFunctionId);
				}
				break;
			}
		}
		for(size_t outputIdx = EOutput::FirstParam, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
		{
			compiler.AddOutputToStack(*this, outputIdx, *m_outputValues[outputIdx]);
		}
	}
}
