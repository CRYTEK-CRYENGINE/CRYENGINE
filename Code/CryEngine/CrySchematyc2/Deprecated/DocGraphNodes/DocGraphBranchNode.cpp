// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphBranchNode.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphBranchNode::CDocGraphBranchNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Branch", EScriptGraphNodeType::Branch, contextGUID, refGUID, pos)
		, m_bValue(false)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphBranchNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBranchNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphBranchNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddInput("Value", EScriptGraphPortFlags::None, GetAggregateTypeId<bool>());
		CDocGraphNodeBase::AddOutput("True", EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("False", EScriptGraphPortFlags::Execute);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Serialize(archive);
		archive(m_bValue, "value", "Value");
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphBranchNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CreateStackFrame(*this, EStackFrame::Value);
		const size_t stackPos = compiler.FindInputOnStack(*this, EInput::Value);
		if(stackPos != INVALID_INDEX)
		{
			if(stackPos != (compiler.GetStackSize() - 1))
			{
				compiler.Copy(stackPos, INVALID_INDEX, MakeAny(m_bValue));
			}
		}
		else
		{
			compiler.Push(MakeAny(m_bValue));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::CompileTrue(IDocGraphNodeCompiler& compiler) const
	{
		compiler.BranchIfZero(*this, EMarker::BranchFalse);
		compiler.CollapseStackFrame(*this, EStackFrame::Value);
		compiler.CreateStackFrame(*this, EStackFrame::Value);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::CompileFalse(IDocGraphNodeCompiler& compiler) const
	{
		compiler.Branch(*this, EMarker::BranchEnd);
		compiler.CreateMarker(*this, EMarker::BranchFalse);
		compiler.CollapseStackFrame(*this, EStackFrame::Value);
		compiler.CreateStackFrame(*this, EStackFrame::Value);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphBranchNode::CompileEnd(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CreateMarker(*this, EMarker::BranchEnd);
		compiler.CollapseStackFrame(*this, EStackFrame::Value);
	}
}
