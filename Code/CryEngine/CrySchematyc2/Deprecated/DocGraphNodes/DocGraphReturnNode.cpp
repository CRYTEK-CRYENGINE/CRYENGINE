// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphReturnNode.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/Deprecated/Variant.h>

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphReturnNode::CDocGraphReturnNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Return", EScriptGraphNodeType::Return, contextGUID, refGUID, pos)
	{}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphReturnNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphReturnNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphReturnNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphReturnNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphReturnNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphReturnNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::EndSequence | EScriptGraphPortFlags::Execute);

		const IScriptFile& file = CDocGraphNodeBase::GetFile();
		const IDocGraph&   graph = CDocGraphNodeBase::GetGraph();
		const size_t       graphOutputCount = graph.GetOutputCount();
		for(size_t graphOutputIdx = 0; graphOutputIdx < graphOutputCount; ++ graphOutputIdx)
		{
			IAnyConstPtr pGraphOutputValue = graph.GetOutputValue(graphOutputIdx);
			if(pGraphOutputValue)
			{
				CDocGraphNodeBase::AddInput(graph.GetOutputName(graphOutputIdx), EScriptGraphPortFlags::None, pGraphOutputValue->GetTypeInfo().GetTypeId());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphReturnNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);

		const IDocGraph& graph = CDocGraphNodeBase::GetGraph();
		const size_t     graphOutputCount = graph.GetOutputCount();

		if(graphOutputCount != m_outputValues.size()) // #SchematycTODO : Move to separate function?
		{
			m_outputValues.resize(graphOutputCount);
			for(size_t graphOutputIdx = 0; graphOutputIdx < graphOutputCount; ++ graphOutputIdx)
			{
				IAnyConstPtr pGraphOutputValue = graph.GetOutputValue(graphOutputIdx);
				if(pGraphOutputValue)
				{
					IAnyPtr& pOutputValue = m_outputValues[graphOutputIdx];
					if(!pOutputValue || (pOutputValue->GetTypeInfo().GetTypeId() != pGraphOutputValue->GetTypeInfo().GetTypeId()))
					{
						pOutputValue = pGraphOutputValue->Clone();
					}
				}
			}
		}

		for(size_t graphOutputIdx = 0; graphOutputIdx < graphOutputCount; ++ graphOutputIdx)
		{
			IAnyPtr& pOutputValue = m_outputValues[graphOutputIdx];
			if(pOutputValue)
			{
				const char* szName = graph.GetOutputName(graphOutputIdx);
				archive(*pOutputValue, szName, szName);
			}
		}

		if(archive.isValidation())
		{
			Validate(archive);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphReturnNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphReturnNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphReturnNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				Return(compiler);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphReturnNode::Validate(Serialization::IArchive& archive)
	{
		const IDocGraph& graph = CDocGraphNodeBase::GetGraph();
		const size_t     graphOutputCount = graph.GetOutputCount();
		if(graphOutputCount != m_outputValues.size())
		{
			archive.error(*this, "Number of outputs does not match reference!");
		}
		else
		{
			for(size_t graphOutputIdx = 0; graphOutputIdx < graphOutputCount; ++ graphOutputIdx)
			{
				if(!m_outputValues[graphOutputIdx])
				{
					archive.error(*this, "Failed to instantiate output value: output = %s", graph.GetOutputName(graphOutputIdx));
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphReturnNode::Return(IDocGraphNodeCompiler& compiler) const
	{
		for(size_t outputIdx = 0, outputCount = m_outputValues.size(), pos = 0; outputIdx < outputCount; ++ outputIdx)
		{
			const IAnyPtr& pOutputValue = m_outputValues[outputIdx];
			if(pOutputValue)
			{  
				const size_t stackPos = compiler.FindInputOnStack(*this, EInput::FirstParam + outputIdx);
				if(stackPos != INVALID_INDEX)
				{
					compiler.Copy(stackPos, pos, *pOutputValue);
				}
				else
				{
					compiler.Set(pos, *pOutputValue);
				}

				TVariantVector              variants;
				CVariantVectorOutputArchive archive(variants);
				archive(*pOutputValue, "", "");

				pos += variants.size();
			}
		}

		compiler.Return();
	}
}
