// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphAbstractInterfaceFunctionNode.h"

#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/IAbstractInterface.h>
#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

namespace Schematyc2
{
	CDocGraphAbstractInterfaceFunctionNode::CDocGraphAbstractInterfaceFunctionNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "", EScriptGraphNodeType::AbstractInterfaceFunction, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	IAnyConstPtr CDocGraphAbstractInterfaceFunctionNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	size_t CDocGraphAbstractInterfaceFunctionNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	void CDocGraphAbstractInterfaceFunctionNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	size_t CDocGraphAbstractInterfaceFunctionNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	void CDocGraphAbstractInterfaceFunctionNode::RemoveOutput(size_t outputIdx) {}

	void CDocGraphAbstractInterfaceFunctionNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddInput("Object", EScriptGraphPortFlags::None, GetAggregateTypeId<ObjectId>());
		CDocGraphNodeBase::AddOutput("True", EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("False", EScriptGraphPortFlags::Execute);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		if(IAbstractInterfaceFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetAbstractInterfaceFunction(refGUID))
		{
			stack_string fullName;
			EnvRegistryUtils::GetFullName(pFunction->GetName(), nullptr, contextGUID, fullName);
			CDocGraphNodeBase::SetName(fullName.c_str());
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
					IAnyPtr& pOutputValue = m_outputValues[EOutput::FirstParam + functionOutputIdx];
					if(!pOutputValue || (pOutputValue->GetTypeInfo().GetTypeId() != pFunctionOutputValue->GetTypeInfo().GetTypeId()))
					{
						pOutputValue = pFunctionOutputValue->Clone();
					}
				}
			}
		}
		else
		{
			ScriptIncludeRecursionUtils::TGetAbstractInterfaceFunctionResult function = ScriptIncludeRecursionUtils::GetAbstractInterfaceFunction(file, refGUID);
			if(function.second)
			{
				stack_string fullName;
				DocUtils::GetFullElementName(*function.first, *function.second, fullName);
				CDocGraphNodeBase::SetName(fullName.c_str());
				const size_t functionInputCount = function.second->GetInputCount();
				m_inputValues.resize(EInput::FirstParam + functionInputCount);
				for(size_t functionInputIdx = 0; functionInputIdx < functionInputCount; ++ functionInputIdx)
				{
					IAnyConstPtr pFunctionInputValue = function.second->GetInputValue(functionInputIdx);
					CRY_ASSERT(pFunctionInputValue);
					if(pFunctionInputValue)
					{
						CDocGraphNodeBase::AddInput(function.second->GetInputName(functionInputIdx), EScriptGraphPortFlags::None, pFunctionInputValue->GetTypeInfo().GetTypeId());
						IAnyPtr& pInputValue = m_inputValues[EInput::FirstParam + functionInputIdx];
						if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pFunctionInputValue->GetTypeInfo().GetTypeId()))
						{
							pInputValue = pFunctionInputValue->Clone();
						}
					}
				}
				const size_t functionOutputCount = function.second->GetOutputCount();
				m_outputValues.resize(EOutput::FirstParam + functionOutputCount);
				for(size_t functionOutputIdx = 0; functionOutputIdx < functionOutputCount; ++ functionOutputIdx)
				{
					IAnyConstPtr pFunctionOutputValue = function.second->GetOutputValue(functionOutputIdx);
					CRY_ASSERT(pFunctionOutputValue);
					if(pFunctionOutputValue)
					{
						CDocGraphNodeBase::AddOutput(function.second->GetOutputName(functionOutputIdx), EScriptGraphPortFlags::MultiLink, pFunctionOutputValue->GetTypeInfo().GetTypeId());
						IAnyPtr& pOutputValue = m_outputValues[EOutput::FirstParam + functionOutputIdx];
						if(!pOutputValue || (pOutputValue->GetTypeInfo().GetTypeId() != pFunctionOutputValue->GetTypeInfo().GetTypeId()))
						{
							pOutputValue = pFunctionOutputValue->Clone();
						}
					}
				}
			}
		}
	}

	void CDocGraphAbstractInterfaceFunctionNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);
		
		if(SerializationContext::GetPass(archive) == ESerializationPass::PostLoad)
		{
			Refresh(SScriptRefreshParams(EScriptRefreshReason::Other)); // Ensure inputs exist before reading values. There should be a cleaner way to do this!
		}

		for(size_t inputIdx = EInput::FirstParam, inputCount = CDocGraphNodeBase::GetInputCount(); inputIdx < inputCount; ++ inputIdx)
		{
			const char* szInputName = CDocGraphNodeBase::GetInputName(inputIdx);
			archive(*m_inputValues[inputIdx], szInputName, szInputName);
		}
	}

	void CDocGraphAbstractInterfaceFunctionNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	void CDocGraphAbstractInterfaceFunctionNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	void CDocGraphAbstractInterfaceFunctionNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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

	void CDocGraphAbstractInterfaceFunctionNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CreateStackFrame(*this, EStackFrame::Body);

		const size_t stackPos = compiler.FindInputOnStack(*this, EInput::Object);
		if(stackPos != INVALID_INDEX)
		{
			compiler.Copy(stackPos, INVALID_INDEX, MakeAny(ObjectId()));
		}
		else
		{
			compiler.Push(MakeAny(ObjectId()));	// #SchematycTODO : Can we perhaps grey out the node if this is not connected?
		}
		DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstParam, m_inputValues, compiler);

		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const SGUID        contextGUID = CDocGraphNodeBase::GetContextGUID();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		if(IAbstractInterfaceFunctionConstPtr pFunction = gEnv->pSchematyc2->GetEnvRegistry().GetAbstractInterfaceFunction(refGUID))
		{
			compiler.CallEnvAbstractInterfaceFunction(contextGUID, refGUID);
		}
		else
		{
			ScriptIncludeRecursionUtils::TGetAbstractInterfaceFunctionResult function = ScriptIncludeRecursionUtils::GetAbstractInterfaceFunction(file, refGUID);
			if(function.second)
			{
				compiler.CallLibAbstractInterfaceFunction(contextGUID, refGUID);
			}
		}

		for(size_t outputIdx = EOutput::FirstParam, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
		{
			compiler.AddOutputToStack(*this, outputIdx, *m_outputValues[outputIdx]);
		}
		compiler.AddOutputToStack(*this, INVALID_INDEX, MakeAny(bool())); // Ideally this would be done by the compiler but it needs to happen after function outputs are bound!
	}

	void CDocGraphAbstractInterfaceFunctionNode::CompileTrue(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CreateStackFrame(*this, EStackFrame::True);
		compiler.BranchIfZero(*this, EMarker::False);
	}

	void CDocGraphAbstractInterfaceFunctionNode::CompileFalse(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CollapseStackFrame(*this, EStackFrame::True);
		compiler.Branch(*this, EMarker::End);
		compiler.CreateMarker(*this, EMarker::False);
		compiler.CreateStackFrame(*this, EStackFrame::False);
	}

	void CDocGraphAbstractInterfaceFunctionNode::CompileEnd(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CollapseStackFrame(*this, EStackFrame::False);
		compiler.CreateMarker(*this, EMarker::End);
		compiler.CollapseStackFrame(*this, EStackFrame::Body);
	}
}
