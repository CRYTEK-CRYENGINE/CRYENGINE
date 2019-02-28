// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Deprecated/DocGraphNodes/DocGraphSetNode.h"

#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>

#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphSetNode::CDocGraphSetNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "", EScriptGraphNodeType::Set, contextGUID, refGUID, pos)
	{}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphSetNode::GetCustomOutputDefault() const
	{
		return IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphSetNode::AddCustomOutput(const IAny& value)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSetNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphSetNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSetNode::RemoveOutput(size_t outputIdx) {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSetNode::Refresh(const SScriptRefreshParams& params)
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
					CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
					CDocGraphNodeBase::AddOutput("Out", EScriptGraphPortFlags::Execute);

					const IScriptVariable* pVariable = static_cast<const IScriptVariable*>(pElement);
					IAnyConstPtr           pVariableValue = pVariable->GetValue();
					if(pVariableValue)
					{
						stack_string name = "Set";
						name.append(" ");
						name.append(pVariable->GetName());
						CDocGraphNodeBase::SetName(name.c_str());

						CDocGraphNodeBase::AddInput("Value", EScriptGraphPortFlags::None, pVariableValue->GetTypeInfo().GetTypeId());
					}
					break;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSetNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);

		if(!m_pValue) // #SchematycTODO : Move to separate function?
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
						if(pVariableValue)
						{
							m_pValue = pVariableValue->Clone();
						}
						break;
					}
				}
			}
		}

		if(m_pValue)
		{
			archive(*m_pValue, "Value", "Value");
		}

		if(archive.isValidation())
		{
			Validate(archive);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSetNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSetNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSetNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
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
	void CDocGraphSetNode::Validate(Serialization::IArchive& archive)
	{
		if(!CDocGraphNodeBase::GetFile().GetElement(CDocGraphNodeBase::GetRefGUID()))
		{
			archive.error(*this, "Failed to retrieve reference!");
		}
		if(!m_pValue)
		{
			archive.error(*this, "Failed to instantiate value!");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSetNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
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
			compiler.Store(CDocGraphNodeBase::GetRefGUID());
		}
	}
}
