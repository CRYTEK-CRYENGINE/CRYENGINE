// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphContainerRemoveByIndexNode.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphContainerRemoveByIndexNode::CDocGraphContainerRemoveByIndexNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "", EScriptGraphNodeType::ContainerRemoveByIndex, contextGUID, refGUID, pos)
		, m_index(0)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other)); // #SchematycTODO : Remove once we've moved over to unified load/save?
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphContainerRemoveByIndexNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerRemoveByIndexNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerRemoveByIndexNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddInput("Index", EScriptGraphPortFlags::None, GetAggregateTypeId<int32>());
		CDocGraphNodeBase::AddOutput("True", EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("False", EScriptGraphPortFlags::Execute);

		const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
		if (pContainer)
		{
			stack_string name = "Container Remove By Index: ";
			name.append(pContainer->GetName());
			CDocGraphNodeBase::SetName(name.c_str());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;
		CDocGraphNodeBase::Serialize(archive);
		archive(m_index, "index", "Index");
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch (sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				CompileInputs(compiler);
				break;
			}
		case EDocGraphSequenceStep::BeginOutput:
			{
				switch (portIdx)
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
	void CDocGraphContainerRemoveByIndexNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CreateStackFrame(*this, EStackFrame::Body);

		const size_t indexStackPos = compiler.FindInputOnStack(*this, EInput::Index);

		if (indexStackPos != INVALID_INDEX)
		{
			compiler.Copy(indexStackPos, INVALID_INDEX, MakeAny(m_index));
		}
		else
		{
			compiler.Push(MakeAny(m_index));
		}

		compiler.ContainerRemoveByIndex(CDocGraphNodeBase::GetRefGUID());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::CompileTrue(IDocGraphNodeCompiler& compiler) const
	{
		compiler.BranchIfZero(*this, EMarker::False);
		//compiler.Pop(1);
		compiler.CreateStackFrame(*this, EStackFrame::True);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::CompileFalse(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CollapseStackFrame(*this, EStackFrame::True);
		compiler.Branch(*this, EMarker::End);
		compiler.CreateMarker(*this, EMarker::False);
		//compiler.Pop(1);
		compiler.CreateStackFrame(*this, EStackFrame::False);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByIndexNode::CompileEnd(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CollapseStackFrame(*this, EStackFrame::False);
		compiler.CreateMarker(*this, EMarker::End);
		compiler.CollapseStackFrame(*this, EStackFrame::Body);
	}
}
