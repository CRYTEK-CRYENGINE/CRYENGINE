// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphContainerAddNode.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphContainerAddNode::CDocGraphContainerAddNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "", EScriptGraphNodeType::ContainerAdd, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other)); // #SchematycTODO : Remove once we've moved over to unified load/save?
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphContainerAddNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerAddNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerAddNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphContainerAddNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerAddNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerAddNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Refresh(params);
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::Execute);

		const IScriptContainer* pContainer = CDocGraphNodeBase::GetFile().GetContainer(CDocGraphNodeBase::GetRefGUID());
		if(pContainer)
		{
			stack_string name = "Container Add: ";
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
	void CDocGraphContainerAddNode::Serialize(Serialization::IArchive& archive)
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
	void CDocGraphContainerAddNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerAddNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphContainerAddNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphContainerAddNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
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
			compiler.ContainerAdd(CDocGraphNodeBase::GetRefGUID(), *m_pValue);
		}
	}
}
