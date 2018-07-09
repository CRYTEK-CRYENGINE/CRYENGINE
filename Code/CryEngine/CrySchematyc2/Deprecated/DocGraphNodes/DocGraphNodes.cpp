// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DocGraphNodes.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>

#include "AggregateTypeIdSerialize.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphBeginNode::CDocGraphBeginNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Begin", EScriptGraphNodeType::Begin, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphBeginNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}
	
	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const size_t       graphInputCount = graph.GetInputCount();
		m_outputValues.resize(EOutput::FirstParam + graphInputCount);
		for(size_t graphInputIdx = 0; graphInputIdx < graphInputCount; ++ graphInputIdx)
		{
			IAnyConstPtr pGraphInputValue = graph.GetInputValue(graphInputIdx);
			if(pGraphInputValue)
			{
				CDocGraphNodeBase::AddOutput(graph.GetInputName(graphInputIdx), EScriptGraphPortFlags::MultiLink, pGraphInputValue->GetTypeInfo().GetTypeId());
				m_outputValues[EOutput::FirstParam + graphInputIdx] = pGraphInputValue->Clone();
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const
	{
		switch(outputIdx)
		{
		case EOutput::Out:
			{
				const IScriptFile& file = CDocGraphNodeBase::GetFile();
				const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
				preCompiler.BindFunctionToGUID(graph.GetGUID());
				for(size_t graphInputIdx = 0, graphInputCount = graph.GetInputCount(); graphInputIdx < graphInputCount; ++ graphInputIdx)
				{
					IAnyConstPtr pGraphInputValue = graph.GetInputValue(graphInputIdx);
					CRY_ASSERT(pGraphInputValue);
					if(pGraphInputValue)
					{
						preCompiler.AddFunctionInput(graph.GetInputName(graphInputIdx), nullptr, *pGraphInputValue);
					}
				}
				if(graph.GetType() == EScriptGraphType::Condition)
				{
					preCompiler.AddFunctionOutput("Result", nullptr, MakeAny(bool(false)));
				}
				else
				{
					for(size_t graphOutputIdx = 0, graphOutputCount = graph.GetOutputCount(); graphOutputIdx < graphOutputCount; ++ graphOutputIdx)
					{
						IAnyConstPtr pGraphOutputValue = graph.GetOutputValue(graphOutputIdx);
						CRY_ASSERT(pGraphOutputValue);
						if(pGraphOutputValue)
						{
							preCompiler.AddFunctionOutput(graph.GetOutputName(graphOutputIdx), nullptr, *pGraphOutputValue);
						}
					}
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				BindOutputs(compiler);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginNode::BindOutputs(IDocGraphNodeCompiler& compiler) const
	{
		size_t stackPos = compiler.GetStackInputPos();
		for(size_t outputIdx = EOutput::FirstParam, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
		{
			stackPos += compiler.BindOutputToStack(*this, stackPos, outputIdx, *m_outputValues[outputIdx]);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBeginConstructorNode::CDocGraphBeginConstructorNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Begin", EScriptGraphNodeType::BeginConstructor, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphBeginConstructorNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginConstructorNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginConstructorNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginConstructorNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginConstructorNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginConstructorNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const SGUID        contextGUID = CDocGraphNodeBase::GetGraph().GetContextGUID();
		if(contextGUID)
		{
			if(ISignalConstPtr pSignal = gEnv->pSchematyc2->GetEnvRegistry().GetSignal(contextGUID))
			{
				const size_t signalInputCount = pSignal->GetInputCount();
				m_outputValues.resize(EOutput::FirstParam + signalInputCount);
				for(size_t signalInputIdx = 0; signalInputIdx < signalInputCount; ++ signalInputIdx)
				{
					IAnyConstPtr pSignalInputValue = pSignal->GetInputValue(signalInputIdx);
					CRY_ASSERT(pSignalInputValue);
					if(pSignalInputValue)
					{
						CDocGraphNodeBase::AddOutput(pSignal->GetInputName(signalInputIdx), EScriptGraphPortFlags::MultiLink, pSignalInputValue->GetTypeInfo().GetTypeId());
						m_outputValues[EOutput::FirstParam + signalInputIdx] = pSignalInputValue->Clone();
					}
				}
			}
			else if(const IScriptSignal* pSignal = ScriptIncludeRecursionUtils::GetSignal(file, contextGUID).second)
			{
				const size_t signalInputCount = pSignal->GetInputCount();
				m_outputValues.resize(EOutput::FirstParam + signalInputCount);
				for(size_t signalInputIdx = 0; signalInputIdx < signalInputCount; ++ signalInputIdx)
				{
					IAnyConstPtr pSignalInputValue = pSignal->GetInputValue(signalInputIdx);
					CRY_ASSERT(pSignalInputValue);
					if(pSignalInputValue)
					{
						CDocGraphNodeBase::AddOutput(pSignal->GetInputName(signalInputIdx), EScriptGraphPortFlags::MultiLink, pSignalInputValue->GetTypeInfo().GetTypeId());
						m_outputValues[EOutput::FirstParam + signalInputIdx] = pSignalInputValue->Clone();
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginConstructorNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t iActiveOutput) const
	{
		switch(iActiveOutput)
		{
		case EOutput::Out:
			{
				for(size_t outputIdx = EOutput::FirstParam, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
				{
					preCompiler.AddFunctionInput(CDocGraphNodeBase::GetOutputName(outputIdx), nullptr, *m_outputValues[outputIdx]);
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginConstructorNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const
	{
		switch(outputIdx)
		{
		case EOutput::Out:
			{
				const IDocGraph& graph = CDocGraphNodeBase::GetGraph();
				linker.CreateConstructor(graph.GetGUID(), functionId);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginConstructorNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				BindOutputs(compiler);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginConstructorNode::BindOutputs(IDocGraphNodeCompiler& compiler) const
	{
		size_t stackPos = 0;
		for(size_t outputIdx = EOutput::FirstParam, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
		{
			stackPos += compiler.BindOutputToStack(*this, stackPos, outputIdx, *m_outputValues[outputIdx]);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBeginDestructorNode::CDocGraphBeginDestructorNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Begin", EScriptGraphNodeType::BeginDestructor, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphBeginDestructorNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginDestructorNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginDestructorNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginDestructorNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginDestructorNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginDestructorNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const SGUID        contextGUID = CDocGraphNodeBase::GetGraph().GetContextGUID();
		if(contextGUID)
		{
			if(ISignalConstPtr pSignal = gEnv->pSchematyc2->GetEnvRegistry().GetSignal(contextGUID))
			{
				const size_t signalInputCount = pSignal->GetInputCount();
				m_outputValues.resize(EOutput::FirstParam + signalInputCount);
				for(size_t signalInputIdx = 0; signalInputIdx < signalInputCount; ++ signalInputIdx)
				{
					IAnyConstPtr pSignalInputValue = pSignal->GetInputValue(signalInputIdx);
					CRY_ASSERT(pSignalInputValue);
					if(pSignalInputValue)
					{
						CDocGraphNodeBase::AddOutput(pSignal->GetInputName(signalInputIdx), EScriptGraphPortFlags::MultiLink, pSignalInputValue->GetTypeInfo().GetTypeId());
						m_outputValues[EOutput::FirstParam + signalInputIdx] = pSignalInputValue->Clone();
					}
				}
			}
			else if(const IScriptSignal* pSignal = ScriptIncludeRecursionUtils::GetSignal(file, contextGUID).second)
			{
				const size_t signalInputCount = pSignal->GetInputCount();
				m_outputValues.resize(EOutput::FirstParam + signalInputCount);
				for(size_t signalInputIdx = 0; signalInputIdx < signalInputCount; ++ signalInputIdx)
				{
					IAnyConstPtr pSignalInputValue = pSignal->GetInputValue(signalInputIdx);
					CRY_ASSERT(pSignalInputValue);
					if(pSignalInputValue)
					{
						CDocGraphNodeBase::AddOutput(pSignal->GetInputName(signalInputIdx), EScriptGraphPortFlags::MultiLink, pSignalInputValue->GetTypeInfo().GetTypeId());
						m_outputValues[EOutput::FirstParam + signalInputIdx] = pSignalInputValue->Clone();
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginDestructorNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const
	{
		switch(outputIdx)
		{
		case EOutput::Out:
			{
				for(size_t outputIdx = EOutput::FirstParam, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
				{
					preCompiler.AddFunctionInput(CDocGraphNodeBase::GetOutputName(outputIdx), nullptr, *m_outputValues[outputIdx]);
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginDestructorNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const
	{
		switch(outputIdx)
		{
		case EOutput::Out:
			{
				const IDocGraph& graph = CDocGraphNodeBase::GetGraph();
				linker.CreateDestructor(graph.GetGUID(), functionId);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginDestructorNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				BindOutputs(compiler);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginDestructorNode::BindOutputs(IDocGraphNodeCompiler& compiler) const
	{
		size_t stackPos = 0;
		for(size_t outputIdx = EOutput::FirstParam, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
		{
			stackPos += compiler.BindOutputToStack(*this, stackPos, outputIdx, *m_outputValues[outputIdx]);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBeginSignalReceiverNode::CDocGraphBeginSignalReceiverNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Begin", EScriptGraphNodeType::BeginSignalReceiver, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphBeginSignalReceiverNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginSignalReceiverNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginSignalReceiverNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginSignalReceiverNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginSignalReceiverNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginSignalReceiverNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const SGUID        contextGUID = CDocGraphNodeBase::GetGraph().GetContextGUID();
		if(ISignalConstPtr pSignal = gEnv->pSchematyc2->GetEnvRegistry().GetSignal(contextGUID))
		{
			const size_t signalInputCount = pSignal->GetInputCount();
			m_outputValues.resize(EOutput::FirstParam + signalInputCount);
			for(size_t signalInputIdx = 0; signalInputIdx < signalInputCount; ++ signalInputIdx)
			{
				IAnyConstPtr pSignalInputValue = pSignal->GetInputValue(signalInputIdx);
				CRY_ASSERT(pSignalInputValue);
				if(pSignalInputValue)
				{
					CDocGraphNodeBase::AddOutput(pSignal->GetInputName(signalInputIdx), EScriptGraphPortFlags::MultiLink, pSignalInputValue->GetTypeInfo().GetTypeId());
					m_outputValues[EOutput::FirstParam + signalInputIdx] = pSignalInputValue->Clone();
				}
			}
		}
		else if(const IScriptSignal* pSignal = ScriptIncludeRecursionUtils::GetSignal(file, contextGUID).second)
		{
			const size_t signalInputCount = pSignal->GetInputCount();
			m_outputValues.resize(EOutput::FirstParam + signalInputCount);
			for(size_t signalInputIdx = 0; signalInputIdx < signalInputCount; ++ signalInputIdx)
			{
				IAnyConstPtr pSignalInputValue = pSignal->GetInputValue(signalInputIdx);
				CRY_ASSERT(pSignalInputValue);
				if(pSignalInputValue)
				{
					CDocGraphNodeBase::AddOutput(pSignal->GetInputName(signalInputIdx), EScriptGraphPortFlags::MultiLink, pSignalInputValue->GetTypeInfo().GetTypeId());
					m_outputValues[EOutput::FirstParam + signalInputIdx] = pSignalInputValue->Clone();
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginSignalReceiverNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const
	{
		switch(outputIdx)
		{
		case EOutput::Out:
			{
				for(size_t outputIdx = EOutput::FirstParam, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
				{
					preCompiler.AddFunctionInput(CDocGraphNodeBase::GetOutputName(outputIdx), nullptr, *m_outputValues[outputIdx]);
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginSignalReceiverNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const
	{
		switch(outputIdx)
		{
		case EOutput::Out:
			{
				const IDocGraph& graph = CDocGraphNodeBase::GetGraph();
				linker.CreateSignalReceiver(graph.GetGUID(), graph.GetContextGUID(), functionId);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginSignalReceiverNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				BindOutputs(compiler);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginSignalReceiverNode::BindOutputs(IDocGraphNodeCompiler& compiler) const
	{
		size_t stackPos = 0;
		for(size_t outputIdx = EOutput::FirstParam, outputCount = CDocGraphNodeBase::GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
		{
			stackPos += compiler.BindOutputToStack(*this, stackPos, outputIdx, *m_outputValues[outputIdx]);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphMakeEnumerationNode::CDocGraphMakeEnumerationNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Make Enumeration", EScriptGraphNodeType::MakeEnumeration, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphMakeEnumerationNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphMakeEnumerationNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphMakeEnumerationNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		if(m_pValue)
		{
			CDocGraphNodeBase::AddOutput("Value", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Pull, m_pValue->GetTypeInfo().GetTypeId());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);

		if(archive.isEdit())
		{
			TypeIds                   typeIds;
			Serialization::StringList typeNames;
			typeIds.reserve(64);
			typeNames.reserve(64);
			if(!m_typeId)
			{
				typeIds.push_back(CAggregateTypeId());
				typeNames.push_back("");
			}

			STypeVisitor typeVisitor(typeIds, typeNames);
			gEnv->pSchematyc2->GetEnvRegistry().VisitTypeDescs(EnvTypeDescVisitor::FromMemberFunction<STypeVisitor, &STypeVisitor::VisitEnvTypeDesc>(typeVisitor));
			ScriptIncludeRecursionUtils::VisitEnumerations(CDocGraphNodeBase::GetFile(), ScriptIncludeRecursionUtils::EnumerationVisitor::FromMemberFunction<STypeVisitor, &STypeVisitor::VisitScriptEnumeration>(typeVisitor), SGUID(), true);

			if(archive.isInput())
			{
				Serialization::StringListValue typeName(typeNames,  0);
				archive(typeName, "typeName", "Type");
				SetTypeId(typeIds[typeName.index()]);
			}
			else if(archive.isOutput())
			{
				TypeIds::iterator              itTypeId = std::find(typeIds.begin(), typeIds.end(), m_typeId);
				Serialization::StringListValue typeName(typeNames, itTypeId != typeIds.end() ? static_cast<int>(itTypeId - typeIds.begin()) : 0);
				archive(typeName, "typeName", "Type");
			}

			if(m_pValue)
			{
				archive(*m_pValue, "value", "Value");
			}
		}
		else
		{
			if(archive.isInput())
			{
				CAggregateTypeId typeId;
				archive(typeId, "typeId");
				SetTypeId(typeId);
			}
			else if(archive.isOutput())
			{
				archive(m_typeId, "typeId");
			}

			if(m_pValue)
			{
				archive(*m_pValue, "value");
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::PullOutput:
			{
				switch(portIdx)
				{
				case EOutput::Value:
					{
						CompileOutput(compiler);
						break;
					}
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphMakeEnumerationNode::STypeVisitor::STypeVisitor(TypeIds& _typeIds, Serialization::StringList& _typeNames)
		: typeIds(_typeIds)
		, typeNames(_typeNames)
	{}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CDocGraphMakeEnumerationNode::STypeVisitor::VisitEnvTypeDesc(const IEnvTypeDesc& typeDesc)
	{
		if(typeDesc.GetCategory() == EEnvTypeCategory::Enumeration)
		{
			typeIds.push_back(typeDesc.GetTypeInfo().GetTypeId());
			typeNames.push_back(typeDesc.GetName());
		}
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::STypeVisitor::VisitScriptEnumeration(const IScriptFile& enumerationFile, const IScriptEnumeration& enumeration)
	{
		typeIds.push_back(enumeration.GetTypeId());
		typeNames.push_back(enumeration.GetName());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::SetTypeId(const CAggregateTypeId& typeId)
	{
		if(typeId != m_typeId)
		{
			m_typeId = typeId;
			m_pValue = MakeScriptVariableValueShared(CDocGraphNodeBase::GetFile(), typeId);

			CDocGraphNodeBase::GetGraph().RemoveLinks(CDocGraphNodeBase::GetGUID());
			Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphMakeEnumerationNode::CompileOutput(IDocGraphNodeCompiler& compiler) const
	{
		if(m_pValue)
		{
			const size_t stackPos = compiler.GetStackSize();
			compiler.Push(*m_pValue);
			compiler.BindOutputToStack(*this, stackPos, EOutput::Value, *m_pValue);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphForLoopNode::CDocGraphForLoopNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "For Loop", EScriptGraphNodeType::ForLoop, contextGUID, refGUID, pos)
		, m_begin(0)
		, m_end(0)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphForLoopNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphForLoopNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphForLoopNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddInput("Begin", EScriptGraphPortFlags::None, GetAggregateTypeId<int32>());
		CDocGraphNodeBase::AddInput("End", EScriptGraphPortFlags::None, GetAggregateTypeId<int32>());
		CDocGraphNodeBase::AddInput("Break", EScriptGraphPortFlags::EndSequence | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Loop", EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("End", EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Index", EScriptGraphPortFlags::MultiLink, GetAggregateTypeId<int32>());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Serialize(archive);
		archive(m_begin, "begin", "Begin");
		archive(m_end, "end", "End");
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				switch(portIdx)
				{
				case EInput::In:
					{
						CompileInputs(compiler);
						break;
					}
				case EInput::Break:
					{
						CompileBreak(compiler);
						break;
					}
				}
				break;
			}
		case EDocGraphSequenceStep::BeginOutput:
			{
				switch(portIdx)
				{
				case EOutput::Loop:
					{
						CompileLoop(compiler);
						break;
					}
				case EOutput::End:
					{
						CompileEnd(compiler);
						break;
					}
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		const size_t endStackPos = compiler.FindInputOnStack(*this, EInput::End);
		if(endStackPos != INVALID_INDEX)
		{
			if(endStackPos != (compiler.GetStackSize() - 1))
			{
				compiler.Copy(endStackPos, INVALID_INDEX, MakeAny(m_end));
			}
		}
		else
		{
			compiler.Push(MakeAny(m_end));
		}
		compiler.BindVariableToStack(*this, compiler.GetStackSize() - 1, EVariable::End);
		compiler.CreateStackFrame(*this, EStackFrame::Body);
		const size_t beginStackPos = compiler.FindInputOnStack(*this, EInput::Begin);
		if(beginStackPos != INVALID_INDEX)
		{
			compiler.Copy(beginStackPos, INVALID_INDEX, MakeAny(m_begin));
		}
		else
		{
			compiler.Push(MakeAny(m_begin));
		}
		compiler.BindOutputToStack(*this, compiler.GetStackSize() - 1, EOutput::Index, MakeAny(int32()));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::CompileBreak(IDocGraphNodeCompiler& compiler) const
	{
		compiler.Branch(*this, EMarker::End);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::CompileLoop(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CreateMarker(*this, EMarker::Begin);
		compiler.CreateStackFrame(*this, EStackFrame::Loop);
		compiler.LessThanInt32(compiler.FindOutputOnStack(*this, EOutput::Index), compiler.FindVariableOnStack(*this, EVariable::End));
		compiler.BranchIfZero(*this, EMarker::End);
		compiler.CollapseStackFrame(*this, EStackFrame::Loop);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphForLoopNode::CompileEnd(IDocGraphNodeCompiler& compiler) const
	{
		compiler.IncrementInt32(compiler.FindOutputOnStack(*this, EOutput::Index));
		compiler.Branch(*this, EMarker::Begin);
		compiler.CreateMarker(*this, EMarker::End);
		compiler.CollapseStackFrame(*this, EStackFrame::Body);
		compiler.BindOutputToStack(*this, compiler.GetStackSize() - 1, EOutput::Index, MakeAny(int32()));
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphProcessSignalNode::CDocGraphProcessSignalNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Process Signal", EScriptGraphNodeType::ProcessSignal, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphProcessSignalNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphProcessSignalNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphProcessSignalNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphProcessSignalNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphProcessSignalNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphProcessSignalNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::Execute);
		const IScriptFile&   file = CDocGraphNodeBase::GetFile();
		const SGUID          refGUID = CDocGraphNodeBase::GetRefGUID();
		stack_string         name = "Process Signal: ";
		const IScriptSignal* pSignal = ScriptIncludeRecursionUtils::GetSignal(file, refGUID).second;
		if(pSignal)
		{
			name.append(pSignal->GetName());
			const size_t signalInputCount = pSignal->GetInputCount();
			m_inputValues.resize(EInput::FirstParam + signalInputCount);
			for(size_t signalInputIdx = 0; signalInputIdx < signalInputCount; ++ signalInputIdx)
			{
				IAnyConstPtr pSignalInputValue = pSignal->GetInputValue(signalInputIdx);
				CRY_ASSERT(pSignalInputValue);
				if(pSignalInputValue)
				{
					CDocGraphNodeBase::AddInput(pSignal->GetInputName(signalInputIdx), EScriptGraphPortFlags::None, pSignalInputValue->GetTypeInfo().GetTypeId());
					IAnyPtr& pInputValue = m_inputValues[EInput::FirstParam + signalInputIdx];
					if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pSignalInputValue->GetTypeInfo().GetTypeId()))
					{
						pInputValue = pSignalInputValue->Clone();
					}
				}
			}
		}
		CDocGraphNodeBase::SetName(name.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphProcessSignalNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Serialize(archive);
		for(size_t inputIdx = EInput::FirstParam, inputCount = CDocGraphNodeBase::GetInputCount(); inputIdx < inputCount; ++ inputIdx)
		{
			const char* szName = CDocGraphNodeBase::GetInputName(inputIdx);
			archive(*m_inputValues[inputIdx], szName, szName);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphProcessSignalNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphProcessSignalNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphProcessSignalNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphProcessSignalNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		compiler.GetObject();
		DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstParam, m_inputValues, compiler);
		compiler.SendSignal(CDocGraphNodeBase::GetRefGUID());
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBroadcastSignalNode::CDocGraphBroadcastSignalNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Broadcast Signal", EScriptGraphNodeType::BroadcastSignal, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphBroadcastSignalNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBroadcastSignalNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBroadcastSignalNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBroadcastSignalNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBroadcastSignalNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBroadcastSignalNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::Execute);
		const IScriptFile&   file = CDocGraphNodeBase::GetFile();
		const SGUID          refGUID = CDocGraphNodeBase::GetRefGUID();
		stack_string         name = "Broadcast Signal: ";
		const IScriptSignal* pSignal = ScriptIncludeRecursionUtils::GetSignal(file, refGUID).second;
		if(pSignal)
		{
			name.append(pSignal->GetName());
			const size_t signalInputCount = pSignal->GetInputCount();
			m_inputValues.resize(EInput::FirstParam + signalInputCount);
			for(size_t signalInputIdx = 0; signalInputIdx < signalInputCount; ++ signalInputIdx)
			{
				IAnyConstPtr pSignalInputValue = pSignal->GetInputValue(signalInputIdx);
				CRY_ASSERT(pSignalInputValue);
				if(pSignalInputValue)
				{
					CDocGraphNodeBase::AddInput(pSignal->GetInputName(signalInputIdx), EScriptGraphPortFlags::None, pSignalInputValue->GetTypeInfo().GetTypeId());
					IAnyPtr& pInputValue = m_inputValues[EInput::FirstParam + signalInputIdx];
					if(!pInputValue || (pInputValue->GetTypeInfo().GetTypeId() != pSignalInputValue->GetTypeInfo().GetTypeId()))
					{
						pInputValue = pSignalInputValue->Clone();
					}
				}
			}
		}
		CDocGraphNodeBase::SetName(name.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBroadcastSignalNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Serialize(archive);
		for(size_t inputIdx = EInput::FirstParam, inputCount = CDocGraphNodeBase::GetInputCount(); inputIdx < inputCount; ++ inputIdx)
		{
			const char* szName = CDocGraphNodeBase::GetInputName(inputIdx);
			archive(*m_inputValues[inputIdx], szName, szName);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBroadcastSignalNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBroadcastSignalNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBroadcastSignalNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphBroadcastSignalNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		DocGraphNodeUtils::CopyInputsToStack(*this, EInput::FirstParam, m_inputValues, compiler);
		compiler.BroadcastSignal(CDocGraphNodeBase::GetRefGUID());
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphGetObjectNode::CDocGraphGetObjectNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Get Object", EScriptGraphNodeType::GetObject, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphGetObjectNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphGetObjectNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetObjectNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphGetObjectNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetObjectNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetObjectNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddOutput("Object", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Pull, GetAggregateTypeId<ObjectId>());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetObjectNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetObjectNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetObjectNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::PullOutput:
			{
				switch(portIdx)
				{
				case EOutput::Object:
					{
						CompileOutput(compiler);
						break;
					}
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetObjectNode::CompileOutput(IDocGraphNodeCompiler& compiler) const
	{
		compiler.GetObject();
		compiler.BindOutputToStack(*this, compiler.GetStackSize() - 1, EOutput::Object, MakeAny(ObjectId()));
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphContainerClearNode::CDocGraphContainerClearNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Container Clear", EScriptGraphNodeType::ContainerClear, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphContainerClearNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerClearNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerClearNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerClearNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerClearNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerClearNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::Execute);

		stack_string            name = "Container Clear: ";
		const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
		if(pContainer)
		{
			name.append(pContainer->GetName());
		}

		CDocGraphNodeBase::SetName(name.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerClearNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerClearNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerClearNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphContainerClearNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		compiler.ContainerClear(CDocGraphNodeBase::GetRefGUID());
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphContainerSizeNode::CDocGraphContainerSizeNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Container Size", EScriptGraphNodeType::ContainerSize, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphContainerSizeNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerSizeNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerSizeNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerSizeNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerSizeNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerSizeNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddOutput("Size", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Pull, GetAggregateTypeId<int32>());

		stack_string            name = "Container Size: ";
		const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
		if(pContainer)
		{
			name.append(pContainer->GetName());
		}

		CDocGraphNodeBase::SetName(name.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerSizeNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerSizeNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerSizeNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::PullOutput:
			{
				switch(portIdx)
				{
				case EOutput::Size:
					{
						CompileOutput(compiler);
						break;
					}
				}
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerSizeNode::CompileOutput(IDocGraphNodeCompiler& compiler) const
	{
		const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
		if(pContainer)
		{
			const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(pContainer->GetTypeGUID());
			if(pTypeDesc)
			{
				IAnyPtr pValue = pTypeDesc->Create();
				if(pValue)
				{
					compiler.ContainerSize(CDocGraphNodeBase::GetRefGUID(), *pValue);
					compiler.BindOutputToStack(*this, compiler.GetStackSize() - 1, EOutput::Size, MakeAny(int32()));
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphContainerGetNode::CDocGraphContainerGetNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Container Get", EScriptGraphNodeType::ContainerGet, contextGUID, refGUID, pos)
		, m_index(0)
	{}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphContainerGetNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerGetNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerGetNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddInput("Index", EScriptGraphPortFlags::None, GetAggregateTypeId<int32>());
		CDocGraphNodeBase::AddOutput("True", EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("False", EScriptGraphPortFlags::Execute);

		stack_string            name = "Container Get: ";
		const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
		if(pContainer)
		{
			const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(pContainer->GetTypeGUID());
			if(pTypeDesc)
			{
				const CAggregateTypeId typeId = pTypeDesc->GetTypeInfo().GetTypeId();
				CDocGraphNodeBase::AddOutput("Value", EScriptGraphPortFlags::MultiLink, typeId);
			}
			name.append(pContainer->GetName());
		}

		CDocGraphNodeBase::SetName(name.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Serialize(archive);
		archive(m_index, "index", "Index");
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t output) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t output, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphContainerGetNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
		if(pContainer)
		{
			const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(pContainer->GetTypeGUID());
			if(pTypeDesc)
			{
				IAnyPtr pValue = pTypeDesc->Create();
				if(pValue)
				{
					const size_t stackPos = compiler.FindInputOnStack(*this, EInput::Index);
					if(stackPos != INVALID_INDEX)
					{
						compiler.Copy(stackPos, INVALID_INDEX, MakeAny(m_index));
					}
					else
					{
						compiler.Push(MakeAny(m_index));
					}
					compiler.AddOutputToStack(*this, EOutput::Value, *pValue);
					compiler.ContainerGet(CDocGraphNodeBase::GetRefGUID(), *pValue);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::CompileTrue(IDocGraphNodeCompiler& compiler) const
	{
		compiler.BranchIfZero(*this, EMarker::False);
		compiler.Pop(1);
		compiler.CreateStackFrame(*this, EStackFrame::True);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::CompileFalse(IDocGraphNodeCompiler& compiler) const
	{
		const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
		if(pContainer)
		{
			const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(pContainer->GetTypeGUID());
			if(pTypeDesc)
			{
				IAnyPtr pValue = pTypeDesc->Create();
				if(pValue)
				{
					compiler.CollapseStackFrame(*this, EStackFrame::True);
					compiler.Branch(*this, EMarker::End);
					compiler.CreateMarker(*this, EMarker::False);
					compiler.Pop(1);
					compiler.Push(*pValue);
					compiler.CreateStackFrame(*this, EStackFrame::False);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerGetNode::CompileEnd(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CollapseStackFrame(*this, EStackFrame::False);
		compiler.CreateMarker(*this, EMarker::End);
	}
	
	//////////////////////////////////////////////////////////////////////////
	CDocGraphStartTimerNode::CDocGraphStartTimerNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Start Timer", EScriptGraphNodeType::StartTimer, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphStartTimerNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphStartTimerNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStartTimerNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphStartTimerNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStartTimerNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStartTimerNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::Execute);
		stack_string        name = "Start Timer: ";
		const IScriptTimer* pTimer = file.GetTimer(refGUID);
		if(pTimer)
		{
			name.append(pTimer->GetName());
		}
		CDocGraphNodeBase::SetName(name.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStartTimerNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStartTimerNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStartTimerNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphStartTimerNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		const IScriptTimer* pTimer = CDocGraphNodeBase::GetFile().GetTimer(CDocGraphNodeBase::GetRefGUID());
		CRY_ASSERT(pTimer);
		if(pTimer)
		{
			compiler.StartTimer(pTimer->GetGUID());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphStopTimerNode::CDocGraphStopTimerNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Stop Timer", EScriptGraphNodeType::StopTimer, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphStopTimerNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphStopTimerNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStopTimerNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphStopTimerNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStopTimerNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStopTimerNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::Execute);
		stack_string        name = "Stop Timer: ";
		const IScriptTimer* pTimer = file.GetTimer(refGUID);
		if(pTimer)
		{
			name.append(pTimer->GetName());
		}
		CDocGraphNodeBase::SetName(name.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStopTimerNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStopTimerNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphStopTimerNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphStopTimerNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		const IScriptTimer* pTimer = CDocGraphNodeBase::GetFile().GetTimer(CDocGraphNodeBase::GetRefGUID());
		CRY_ASSERT(pTimer);
		if(pTimer)
		{
			compiler.StopTimer(pTimer->GetGUID());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphResetTimerNode::CDocGraphResetTimerNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Reset Timer", EScriptGraphNodeType::ResetTimer, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphResetTimerNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphResetTimerNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphResetTimerNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphResetTimerNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphResetTimerNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphResetTimerNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const SGUID        refGUID = CDocGraphNodeBase::GetRefGUID();
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::Execute);
		stack_string        name = "Reset Timer: ";
		const IScriptTimer* pTimer = file.GetTimer(refGUID);
		if(pTimer)
		{
			name.append(pTimer->GetName());
		}
		CDocGraphNodeBase::SetName(name.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphResetTimerNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphResetTimerNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphResetTimerNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphResetTimerNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		const IScriptTimer* pTimer = CDocGraphNodeBase::GetFile().GetTimer(CDocGraphNodeBase::GetRefGUID());
		CRY_ASSERT(pTimer);
		if(pTimer)
		{
			compiler.ResetTimer(pTimer->GetGUID());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphBeginStateNode::CDocGraphBeginStateNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Begin", EScriptGraphNodeType::BeginState, contextGUID, refGUID, pos)
	{
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::BeginSequence | EScriptGraphPortFlags::Execute);
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphBeginStateNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginStateNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginStateNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBeginStateNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginStateNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginStateNode::Refresh(const SScriptRefreshParams& params) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginStateNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const
	{
		preCompiler.AddFunctionOutput("Result", nullptr, MakeAny(int32(ELibTransitionResult::Continue)));
		preCompiler.AddFunctionOutput("State", nullptr, MakeAny(int32(0)));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginStateNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const
	{
		if(ILibStateMachine* pLibStateMachine = linker.GetLibStateMachine())
		{
			pLibStateMachine->SetBeginFunction(functionId);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBeginStateNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphEndStateNode::CDocGraphEndStateNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "End", EScriptGraphNodeType::EndState, contextGUID, refGUID, pos)
	{
		CDocGraphNodeBase::AddInput("Success", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::EndSequence | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddInput("Failure", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::EndSequence | EScriptGraphPortFlags::Execute);
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphEndStateNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphEndStateNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphEndStateNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphEndStateNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphEndStateNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphEndStateNode::Refresh(const SScriptRefreshParams& params) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphEndStateNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphEndStateNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphEndStateNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				End(compiler, portIdx == EInput::Success);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphEndStateNode::End(IDocGraphNodeCompiler& compiler, bool bSuccess) const
	{
		compiler.Set(0, MakeAny(int32(bSuccess ? ELibTransitionResult::EndSuccess : ELibTransitionResult::EndFailure)));
		compiler.Set(1, MakeAny(int32(~0)));
		compiler.Return();
	}

	//////////////////////////////////////////////////////////////////////////
	// Comment Node
	CDocGraphCommentNode::CDocGraphCommentNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Comment", EScriptGraphNodeType::Comment, contextGUID, refGUID, pos)
	{
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CDocGraphCommentNode::GetComment() const
	{
		return m_str.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphCommentNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphCommentNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphCommentNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) { }

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphCommentNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphCommentNode::RemoveOutput(size_t outputIdx) { }

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphCommentNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const { }

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphCommentNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const { }

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphCommentNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const { }

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphCommentNode::Serialize(Serialization::IArchive& archive)
	{
		CDocGraphNodeBase::Serialize(archive);
		archive(m_str, "str", "Comment Contents");
	}
}
