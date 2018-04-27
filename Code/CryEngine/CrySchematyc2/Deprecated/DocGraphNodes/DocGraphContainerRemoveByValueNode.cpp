// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphContainerRemoveByValueNode.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphContainerRemoveByValueNode::CDocGraphContainerRemoveByValueNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "", EScriptGraphNodeType::ContainerRemoveByValue, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other)); // #SchematycTODO : Remove once we've moved over to unified load/save?
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphContainerRemoveByValueNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerRemoveByValueNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerRemoveByValueNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("True", EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("False", EScriptGraphPortFlags::Execute);

		const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
		if(pContainer)
		{
			stack_string name = "Container Remove By Value: ";
			name.append(pContainer->GetName());
			CDocGraphNodeBase::SetName(name.c_str());

			const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(pContainer->GetTypeGUID());
			if(pTypeDesc)
			{
				CDocGraphNodeBase::AddInput("Value", EScriptGraphPortFlags::None, pTypeDesc->GetTypeInfo().GetTypeId());
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);
		
		if(!m_pValue) // #SchematycTODO : Move to separate function?
		{
			const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
			if(pContainer)
			{
				const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(pContainer->GetTypeGUID());
				if(pTypeDesc)
				{
					m_pValue = pTypeDesc->Create();
				}
			}
		}
		
		if(m_pValue)
		{
			archive(*m_pValue, "Value", "Value");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphContainerRemoveByValueNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		CRY_ASSERT(m_pValue);
		if(m_pValue)
		{
			const size_t stackPos = compiler.FindInputOnStack(*this, EInput::Value);
			if(stackPos != INVALID_INDEX)
			{
				compiler.Copy(stackPos, INVALID_INDEX, *m_pValue);
			}
			else
			{
				compiler.Push(*m_pValue);
			}
			compiler.ContainerRemoveByValue(CDocGraphNodeBase::GetRefGUID(), *m_pValue);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::CompileTrue(IDocGraphNodeCompiler& compiler) const
	{
		compiler.BranchIfZero(*this, EMarker::False);
		compiler.Pop(1);
		compiler.CreateStackFrame(*this, EStackFrame::True);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::CompileFalse(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CollapseStackFrame(*this, EStackFrame::True);
		compiler.Branch(*this, EMarker::End);
		compiler.CreateMarker(*this, EMarker::False);
		compiler.Pop(1);
		compiler.CreateStackFrame(*this, EStackFrame::False);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerRemoveByValueNode::CompileEnd(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CollapseStackFrame(*this, EStackFrame::False);
		compiler.CreateMarker(*this, EMarker::End);
	}
}
