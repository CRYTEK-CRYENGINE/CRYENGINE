// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphGetNode.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphGetNode::CDocGraphGetNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "", EScriptGraphNodeType::Get, contextGUID, refGUID, pos)
	{
		Refresh(SScriptRefreshParams(EScriptRefreshReason::Other));
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphGetNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphGetNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphGetNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Refresh(params);
		
		const IScriptElement* pElement = CDocGraphNodeBase::GetFile().GetElement(CDocGraphNodeBase::GetRefGUID());
		if(pElement)
		{
			switch(pElement->GetElementType())
			{
			case EScriptElementType::Variable:
				{
					const IScriptVariable* pVariable = static_cast<const IScriptVariable*>(pElement);
					IAnyConstPtr           pVariableValue = pVariable->GetValue();
					if(pVariableValue)
					{
						stack_string name = "Get";
						name.append(" ");
						name.append(pVariable->GetName());
						CDocGraphNodeBase::SetName(name.c_str());
						CDocGraphNodeBase::AddOutput("Value", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Pull, pVariableValue->GetTypeInfo().GetTypeId());
					}
					break;
				}
			case EScriptElementType::Property:
				{
					const IScriptProperty* pProperty = static_cast<const IScriptProperty*>(pElement);
					IAnyConstPtr           pPropertyValue = pProperty->GetValue();
					if(pPropertyValue)
					{
						stack_string name = "Get";
						name.append(" ");
						name.append(pProperty->GetName());
						CDocGraphNodeBase::SetName(name.c_str());
						CDocGraphNodeBase::AddOutput("Value", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Pull, pPropertyValue->GetTypeInfo().GetTypeId());
					}
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);

		if(archive.isValidation())
		{
			Validate(archive);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphGetNode::Validate(Serialization::IArchive& archive)
	{
		if(!CDocGraphNodeBase::GetFile().GetElement(CDocGraphNodeBase::GetRefGUID()))
		{
			archive.error(*this, "Failed to retrieve reference!");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphGetNode::CompileOutput(IDocGraphNodeCompiler& compiler) const
	{
		const IScriptElement* pElement = CDocGraphNodeBase::GetFile().GetElement(CDocGraphNodeBase::GetRefGUID());
		if(pElement)
		{
			switch(pElement->GetElementType())
			{
			case EScriptElementType::Variable:
				{
					const IScriptVariable* pVariable = static_cast<const IScriptVariable*>(pElement);
					IAnyConstPtr           pVariableValue = pVariable->GetValue();
					SCHEMATYC2_SYSTEM_ASSERT(pVariableValue);
					if(pVariableValue)
					{
						const size_t stackPos = compiler.GetStackSize();
						compiler.Load(CDocGraphNodeBase::GetRefGUID());
						compiler.BindOutputToStack(*this, stackPos, EOutput::Value, *pVariableValue);
					}
					break;
				}
			case EScriptElementType::Property:
				{
					const IScriptProperty* pProperty = static_cast<const IScriptProperty*>(pElement);
					IAnyConstPtr           pPropertyValue = pProperty->GetValue();
					SCHEMATYC2_SYSTEM_ASSERT(pPropertyValue);
					if(pPropertyValue)
					{
						const size_t stackPos = compiler.GetStackSize();
						compiler.Load(CDocGraphNodeBase::GetRefGUID());
						compiler.BindOutputToStack(*this, stackPos, EOutput::Value, *pPropertyValue);
					}
					break;
				}
			}
		}
	}
}
