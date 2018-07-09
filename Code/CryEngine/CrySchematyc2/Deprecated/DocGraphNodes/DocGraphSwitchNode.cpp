// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Deprecated/DocGraphNodes/DocGraphSwitchNode.h"

#include <CryString/CryStringUtils.h>
#include <CrySchematyc2/ICompiler.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Deprecated/IGlobalFunction.h>
#include <CrySchematyc2/Utils/ToString.h>

#include "AggregateTypeIdSerialize.h"
#include "Deprecated/DocGraphNodes/DocGraphNodeBase.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CDocGraphSwitchNode::CDocGraphSwitchNode(IScriptFile& file, IDocGraph& graph, const SGUID& guid, const SGUID& contextGUID, const SGUID& refGUID, Vec2 pos)
		: CDocGraphNodeBase(file, graph, guid, "Switch", EScriptGraphNodeType::Switch, contextGUID, refGUID, pos)
	{
		if(const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(GetEnvTypeId<int32>()))
		{
			m_typeId = pTypeDesc->GetTypeInfo().GetTypeId();
			m_pValue = pTypeDesc->Create();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CDocGraphSwitchNode::GetCustomOutputDefault() const
	{
		return MakeScriptVariableValueShared(CDocGraphNodeBase::GetFile(), m_typeId);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::EnumerateOptionalOutputs(const ScriptGraphNodeOptionalOutputEnumerator& enumerator) {}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphSwitchNode::AddCustomOutput(const IAny& value)
	{
		char stringBuffer[512] = "";
		ToString(value, stringBuffer);
		if(stringBuffer[0])
		{
			m_cases.push_back(SCase(value.Clone()));
			return CDocGraphNodeBase::AddOutput(stringBuffer, EScriptGraphPortFlags::Removable | EScriptGraphPortFlags::Execute);
		}
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CDocGraphSwitchNode::AddOptionalOutput(const char* szName, EScriptGraphPortFlags flags, const CAggregateTypeId& typeId)
	{
		return INVALID_INDEX;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::RemoveOutput(size_t outputIdx)
	{
		CDocGraphNodeBase::RemoveOutput(outputIdx);
		m_cases.erase(m_cases.begin() + (outputIdx - EOutputIdx::FirstCase));
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Refresh(params);

		if(!ValidateScriptVariableTypeInfo(CDocGraphNodeBase::GetFile(), m_typeId))
		{
			SetType(CAggregateTypeId());
		}
		
		CDocGraphNodeBase::AddInput("In", EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute);
		CDocGraphNodeBase::AddInput("Value", EScriptGraphPortFlags::None, m_pValue ? m_pValue->GetTypeInfo().GetTypeId() : CAggregateTypeId());
		CDocGraphNodeBase::AddOutput("Default", EScriptGraphPortFlags::Execute | EScriptGraphPortFlags::SpacerBelow);
		
		uint32 invalidCaseCount = 0;
		for(SCase& _case : m_cases)
		{
			char outputName[512] = "";
			if(_case.pValue)
			{
				ToString(*_case.pValue, outputName);
			}
			if(!outputName[0])
			{
				cry_sprintf(outputName, "INVALID_CASE[%d]", ++ invalidCaseCount);
			}
			CDocGraphNodeBase::AddOutput(outputName, EScriptGraphPortFlags::Removable | EScriptGraphPortFlags::Execute);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CDocGraphNodeBase::Serialize(archive);

		if(archive.isEdit())
		{
			TypeIds                   typeIds;
			Serialization::StringList typeNames;
			if(const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(GetEnvTypeId<int32>()))
			{
				typeIds.push_back(pTypeDesc->GetTypeInfo().GetTypeId());
				typeNames.push_back(pTypeDesc->GetName());
			}
			if(const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(GetEnvTypeId<uint32>()))
			{
				typeIds.push_back(pTypeDesc->GetTypeInfo().GetTypeId());
				typeNames.push_back(pTypeDesc->GetName());
			}
			if(const IEnvTypeDesc* pTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(GetEnvTypeId<CPoolString>()))
			{
				typeIds.push_back(pTypeDesc->GetTypeInfo().GetTypeId());
				typeNames.push_back(pTypeDesc->GetName());
			}

			STypeVisitor typeVisitor(typeIds, typeNames);
			gEnv->pSchematyc2->GetEnvRegistry().VisitTypeDescs(EnvTypeDescVisitor::FromMemberFunction<STypeVisitor, &STypeVisitor::VisitEnvTypeDesc>(typeVisitor));
			ScriptIncludeRecursionUtils::VisitEnumerations(CDocGraphNodeBase::GetFile(), ScriptIncludeRecursionUtils::EnumerationVisitor::FromMemberFunction<STypeVisitor, &STypeVisitor::VisitScriptEnumeration>(typeVisitor), SGUID(), true);

			if(archive.isInput())
			{
				Serialization::StringListValue typeName(typeNames, 0);
				archive(typeName, "typeName", "Type");
				SetType(typeIds[typeName.index()]);
			}
			else if(archive.isOutput())
			{
				int               typeIdx;
				TypeIds::iterator itTypeId = std::find(typeIds.begin(), typeIds.end(), m_typeId);
				if(itTypeId != typeIds.end())
				{
					typeIdx = static_cast<int>(itTypeId - typeIds.begin());
				}
				else
				{
					typeIds.push_back(m_typeId);
					typeNames.push_back("");
					typeIdx = static_cast<int>(typeIds.size() - 1);
				}

				Serialization::StringListValue typeName(typeNames, typeIdx);
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
				if(!archive(typeId, "typeId"))
				{
					PatchAggregateTypeIdFromDocVariableTypeInfo(archive, typeId, "typeInfo");
				}
				SetType(typeId);
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

		Serialization::SContext context(archive, static_cast<CDocGraphSwitchNode*>(this));
		archive(m_cases, "cases", "Cases");

		if(archive.isInput())
		{
			Refresh(SScriptRefreshParams(EScriptRefreshReason::Other)); // #SchematycTODO : Should we really need to refresh manually or should the system take care of that?
		}

		if(archive.isValidation())
		{
			Validate(archive);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::PreCompileSequence(IDocGraphSequencePreCompiler& preCompiler, size_t outputIdx) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::LinkSequence(IDocGraphSequenceLinker& linker, size_t outputIdx, const LibFunctionId& functionId) const {}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::Compile(IDocGraphNodeCompiler& compiler, EDocGraphSequenceStep sequenceStep, size_t portIdx) const
	{
		switch(sequenceStep)
		{
		case EDocGraphSequenceStep::BeginInput:
			{
				CompileInputs(compiler);
				break;
			}
		case EDocGraphSequenceStep::EndInput:
			{
				CompileEnd(compiler);
				break;
			}
		case EDocGraphSequenceStep::BeginOutput:
			{
				CompileCaseBegin(compiler, portIdx);
				break;
			}
		case EDocGraphSequenceStep::EndOutput:
			{
				CompileCaseEnd(compiler);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphSwitchNode::SCase::SCase() {}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphSwitchNode::SCase::SCase(const IAnyPtr& _pValue)
		: pValue(_pValue)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::SCase::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION

		CDocGraphSwitchNode* pSwitchNode = archive.context<CDocGraphSwitchNode>();
		SCHEMATYC2_SYSTEM_ASSERT(pSwitchNode);
		if(pSwitchNode)
		{
			if(!pValue)
			{
				pValue = MakeScriptVariableValueShared(pSwitchNode->GetFile(), pSwitchNode->m_typeId);
			}
			if(pValue)
			{
				archive(*pValue, "value", "^Value");
				if(archive.isValidation())
				{
					char stringBuffer[512] = "";
					ToString(*pValue, stringBuffer);
					if(!stringBuffer[0])
					{
						archive.error(*this, "Empty case value!");
					}
				}
			}
			else if(archive.isValidation())
			{
				archive.error(*this, "Unable to instantiate value!");
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CDocGraphSwitchNode::STypeVisitor::STypeVisitor(TypeIds& _typeIds, Serialization::StringList& _typeNames)
		: typeIds(_typeIds)
		, typeNames(_typeNames)
	{}

	//////////////////////////////////////////////////////////////////////////
	EVisitStatus CDocGraphSwitchNode::STypeVisitor::VisitEnvTypeDesc(const IEnvTypeDesc& typeDesc)
	{
		if((typeDesc.GetCategory() == EEnvTypeCategory::Enumeration) || ((typeDesc.GetFlags() & EEnvTypeFlags::Switchable) != 0))
		{
			typeIds.push_back(typeDesc.GetTypeInfo().GetTypeId());
			typeNames.push_back(typeDesc.GetName());
		}
		return EVisitStatus::Continue;
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::STypeVisitor::VisitScriptEnumeration(const IScriptFile& enumerationFile, const IScriptEnumeration& enumeration)
	{
		typeIds.push_back(enumeration.GetTypeId());
		typeNames.push_back(enumeration.GetName());
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::Validate(Serialization::IArchive& archive)
	{
		if(!m_pValue)
		{
			archive.error(*this, "Unable to instantiate value!");
		}

		std::vector<stack_string> caseStrings;
		uint32                    duplicateCaseValueCount = 0;
		for(const SCase& _case : m_cases)
		{
			if(_case.pValue)
			{
				char stringBuffer[512] = "";
				ToString(*_case.pValue, stringBuffer);
				if(stringBuffer[0])
				{
					stack_string caseString = stringBuffer;
					if(std::find(caseStrings.begin(), caseStrings.end(), caseString) != caseStrings.end())
					{
						++ duplicateCaseValueCount;
					}
					else
					{
						caseStrings.push_back(caseString);
					}
				}
			}
		}
		if(duplicateCaseValueCount)
		{
			archive.error(*this, "Duplicate case values detected!");
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::SetType(const CAggregateTypeId& typeId)
	{
		if(typeId != m_typeId)
		{
			m_cases.clear();

			m_typeId = typeId;
			m_pValue = MakeScriptVariableValueShared(CDocGraphNodeBase::GetFile(), typeId);

			CDocGraphNodeBase::GetGraph().RemoveLinks(CDocGraphNodeBase::GetGUID());
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::CompileInputs(IDocGraphNodeCompiler& compiler) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(m_pValue);
		if(m_pValue)
		{
			compiler.CreateStackFrame(*this, EStackFrame::Value);

			TVariantVector              variants;
			CVariantVectorOutputArchive archive(variants);
			archive(*m_pValue);
			const size_t typeSize = variants.size();

			const size_t stackPos = compiler.FindInputOnStack(*this, EInputIdx::Value);
			if(stackPos != INVALID_INDEX)
			{
				if(stackPos != (compiler.GetStackSize() - typeSize))
				{
					compiler.Copy(stackPos, INVALID_INDEX, *m_pValue);
				}
			}
			else
			{
				compiler.Push(*m_pValue);
			}

			const size_t rhsPos = compiler.GetStackSize();
			const size_t lhsPos = rhsPos - typeSize;
			for(size_t caseIdx = 0, caseCount = m_cases.size(); caseIdx < caseCount; ++ caseIdx)
			{
				compiler.CreateStackFrame(*this, EStackFrame::Case);

				IAnyConstPtr pCaseValue = m_cases[caseIdx].pValue;
				if(pCaseValue)
				{
					compiler.Push(*pCaseValue);
				}
				else
				{
					SCHEMATYC2_COMPILER_ERROR("Missing case value!");
					compiler.Push(*m_pValue);
				}

				compiler.Compare(lhsPos, rhsPos, typeSize);
				compiler.BranchIfNotZero(*this, EBranchMarker::FirstCase + caseIdx);
				compiler.CollapseStackFrame(*this, EStackFrame::Case);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::CompileCaseBegin(IDocGraphNodeCompiler& compiler, size_t portIdx) const
	{
		if(portIdx > EOutputIdx::Default)
		{
			compiler.CreateMarker(*this, EBranchMarker::FirstCase + (portIdx - EOutputIdx::FirstCase));
		}
		compiler.CollapseStackFrame(*this, EStackFrame::Value);
		compiler.CreateStackFrame(*this, EStackFrame::Value);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::CompileCaseEnd(IDocGraphNodeCompiler& compiler) const
	{
		compiler.Branch(*this, EBranchMarker::End);
	}

	//////////////////////////////////////////////////////////////////////////
	void CDocGraphSwitchNode::CompileEnd(IDocGraphNodeCompiler& compiler) const
	{
		compiler.CreateMarker(*this, EBranchMarker::End);
		compiler.CollapseStackFrame(*this, EStackFrame::Value);
	}
}
