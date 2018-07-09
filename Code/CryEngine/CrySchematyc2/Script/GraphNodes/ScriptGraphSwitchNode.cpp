// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/GraphNodes/ScriptGraphSwitchNode.h"

#include <CryString/CryStringUtils.h>
#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/IDomainContext.h>
#include <CrySchematyc2/IObject.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Script/IScriptGraphNodeCompiler.h>
#include <CrySchematyc2/Script/IScriptGraphNodeCreationMenu.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "AggregateTypeIdSerialize.h"
#include "DomainContext.h"
#include "Script/ScriptVariableDeclaration.h"
#include "Script/GraphNodes/ScriptGraphNodeFactory.h"

namespace Schematyc2
{
	const SGUID CScriptGraphSwitchNode::s_typeGUID = "1d081133-e900-4244-add5-f0831d27b16f";

	CScriptGraphSwitchNode::SCase::SCase() {}

	CScriptGraphSwitchNode::SCase::SCase(const IAnyPtr& _pValue)
		: pValue(_pValue)
	{}

	void CScriptGraphSwitchNode::SCase::Serialize(Serialization::IArchive& archive)
	{
		const IAny* pDefaultValue = archive.context<IAny>();
		SCHEMATYC2_SYSTEM_ASSERT(pDefaultValue);
		if(!pValue && pDefaultValue)
		{
			pValue = pDefaultValue->Clone();
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
			archive.error(*this, "Unable to instantiate case value!");
		}
	}

	CScriptGraphSwitchNode::CScriptGraphSwitchNode(const SGUID& guid)
		: CScriptGraphNodeBase(guid)
	{}

	CScriptGraphSwitchNode::CScriptGraphSwitchNode(const SGUID& guid, const Vec2& pos)
		: CScriptGraphNodeBase(guid, pos)
	{}

	SGUID CScriptGraphSwitchNode::GetTypeGUID() const
	{
		return s_typeGUID;
	}

	EScriptGraphColor CScriptGraphSwitchNode::GetColor() const
	{
		return EScriptGraphColor::Orange;
	}

	void CScriptGraphSwitchNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CScriptGraphNodeBase::Refresh(params);

		CScriptGraphNodeBase::SetName("Switch");
		CScriptGraphNodeBase::AddInput(EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute, "In");
		CScriptGraphNodeBase::AddOutput(EScriptGraphPortFlags::Execute | EScriptGraphPortFlags::SpacerBelow, "Default");

		if(m_typeId)
		{
			CScriptGraphNodeBase::AddInput(EScriptGraphPortFlags::Data, "Value", m_typeId);

			uint32 invalidCaseCount = 0;
			for(SCase& _case : m_cases)
			{
				char outputName[128] = "";
				if(_case.pValue)
				{
					ToString(*_case.pValue, outputName);
				}
				if(!outputName[0])
				{
					cry_sprintf(outputName, "INVALID_CASE[%d]", ++ invalidCaseCount);
				}
				CScriptGraphNodeBase::AddOutput(EScriptGraphPortFlags::Removable | EScriptGraphPortFlags::Execute, outputName);
			}
		}
	}

	void CScriptGraphSwitchNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CScriptGraphNodeBase::Serialize(archive);

		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
			{
				SerializeBasicInfo(archive);
				break;
			}
		case ESerializationPass::PostLoad:
			{
				CreateDefaultValue();
				SerializeCases(archive);
				break;
			}
		case ESerializationPass::Save:
			{
				SerializeBasicInfo(archive);
				SerializeCases(archive);
				break;
			}
		case ESerializationPass::Edit:
			{
				Edit(archive);
				SerializeCases(archive);
				Validate(archive);
				break;
			}
		case ESerializationPass::Validate:
			{
				SerializeCases(archive);
				Validate(archive);
				break;
			}
		}
	}

	void CScriptGraphSwitchNode::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		CScriptGraphNodeBase::RemapGUIDs(guidRemapper);

		if(m_typeId.GetDomain() == EDomain::Script) // #SchematycTODO : Create CAggregateTypeId::RemapGUIDs() function?
		{
			m_typeId = CAggregateTypeId::FromScriptTypeGUID(guidRemapper.Remap(m_typeId.AsScriptTypeGUID()));
		}
	}

	void CScriptGraphSwitchNode::Compile_New(IScriptGraphNodeCompiler& compiler) const
	{
		if(m_pDefaultValue)
		{
			compiler.BindCallback(&Execute);

			const uint32 caseCount = m_cases.size();
			compiler.BindAttribute(EAttributeId::CaseCount, caseCount);
			for(uint32 caseIdx = 0; caseIdx < caseCount; ++ caseIdx)
			{
				if(m_cases[caseIdx].pValue)
				{
					compiler.BindAnyAttribute(EAttributeId::FirstCase + caseIdx, *m_cases[caseIdx].pValue);
				}
			}

			compiler.BindAnyInput(EInputIdx::Value, *m_pDefaultValue);
		}
	}

	void CScriptGraphSwitchNode::RegisterCreator(CScriptGraphNodeFactory& factory)
	{
		class CCreator : public IScriptGraphNodeCreator
		{
		private:

			class CNodeCreationMenuCommand : public IScriptGraphNodeCreationMenuCommand
			{
			public:

				// IMenuCommand

				IScriptGraphNodePtr Execute(const Vec2& pos)
				{
					return std::make_shared<CScriptGraphSwitchNode>(gEnv->pSchematyc2->CreateGUID(), pos);
				}

				// ~IMenuCommand
			};

		public:

			// IScriptGraphNodeCreator
			
			virtual SGUID GetTypeGUID() const override
			{
				return CScriptGraphSwitchNode::s_typeGUID;
			}

			virtual IScriptGraphNodePtr CreateNode() override
			{
				return std::make_shared<CScriptGraphSwitchNode>(gEnv->pSchematyc2->CreateGUID());
			}

			virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IDomainContext& domainContext, const IScriptGraphExtension& graph) override
			{
				nodeCreationMenu.AddOption("Switch", "Switch on input value", "", std::make_shared<CNodeCreationMenuCommand>());
			}

			// ~IScriptGraphNodeCreator
		};

		factory.RegisterCreator(std::make_shared<CCreator>());
	}

	void CScriptGraphSwitchNode::CreateDefaultValue()
	{
		if(!m_pDefaultValue || (m_pDefaultValue->GetTypeInfo().GetTypeId() != m_typeId))
		{
			m_pDefaultValue = MakeScriptVariableValueShared(CScriptGraphNodeBase::GetGraph()->GetElement_New().GetFile(), m_typeId);

			m_cases.clear();
		}
	}

	void CScriptGraphSwitchNode::SerializeBasicInfo(Serialization::IArchive& archive)
	{
		archive(m_typeId, "typeId");
	}

	void CScriptGraphSwitchNode::SerializeCases(Serialization::IArchive& archive)
	{
		Serialization::SContext context(archive, static_cast<IAny*>(m_pDefaultValue.get()));
		archive(m_cases, "cases", "Cases");
	}

	void CScriptGraphSwitchNode::Edit(Serialization::IArchive& archive)
	{
		TypeIds                   typeIds;
		Serialization::StringList typeNames;
		typeIds.reserve(50);
		typeNames.reserve(50);

		auto visitEnvType = [&typeIds, &typeNames] (const IEnvTypeDesc& envTypeDesc) -> EVisitStatus
		{
			if((envTypeDesc.GetCategory() == EEnvTypeCategory::Enumeration) || ((envTypeDesc.GetFlags() & EEnvTypeFlags::Switchable) != 0))
			{
				typeIds.push_back(envTypeDesc.GetTypeInfo().GetTypeId());
				typeNames.push_back(envTypeDesc.GetName());
			}
			return EVisitStatus::Continue;
		};
		gEnv->pSchematyc2->GetEnvRegistry().VisitTypeDescs(EnvTypeDescVisitor::FromLambdaFunction(visitEnvType)); // #SchematycTODO : Use domain context?

		const IScriptElement& scriptElement = CScriptGraphNodeBase::GetGraph()->GetElement_New();
		CDomainContext        domainContext(SDomainContextScope(&scriptElement.GetFile(), scriptElement.GetScopeGUID()));
		auto visitScriptEnumeration = [&typeIds, &typeNames, &domainContext] (const IScriptEnumeration& scriptEnumeration) -> EVisitStatus
		{
			typeIds.push_back(scriptEnumeration.GetTypeId());

			stack_string typeName;
			domainContext.QualifyName(scriptEnumeration, EDomainQualifier::Local, typeName);
			typeNames.push_back(typeName.c_str());

			return EVisitStatus::Continue;
		};
		domainContext.VisitScriptEnumerations(ScriptEnumerationConstVisitor::FromLambdaFunction(visitScriptEnumeration), EDomainScope::Derived);

		if(archive.isInput())
		{
			Serialization::StringListValue typeName(typeNames, 0);
			archive(typeName, "typeName", "Type");
			m_typeId = typeIds[typeName.index()];
			CreateDefaultValue();
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
				typeNames.push_back("None");
				typeIdx = static_cast<int>(typeIds.size() - 1);
			}

			Serialization::StringListValue typeName(typeNames, typeIdx);
			archive(typeName, "typeName", "Type");
		}
	}

	void CScriptGraphSwitchNode::Validate(Serialization::IArchive& archive)
	{
		std::vector<stack_string> caseStrings;
		uint32                    duplicateCaseValueCount = 0;
		for(const SCase& _case : m_cases)
		{
			if(_case.pValue)
			{
				char stringBuffer[128] = "";
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

	SRuntimeResult CScriptGraphSwitchNode::Execute(IObject* pObject, const SRuntimeActivationParams& activationParams, CRuntimeNodeData& data)
	{
		const IAny*  pValue = data.GetInput(EInputIdx::Value);
		const uint32 caseCount = *data.GetAttribute<uint32>(EAttributeId::CaseCount);
		for(uint32 caseIdx = 0; caseIdx < caseCount; ++ caseIdx)
		{
			const IAny* pCaseValue = data.GetAttribute(EAttributeId::FirstCase + caseIdx);
			//if(pValue->Equal(*pCaseValue))
			{

			}
		}
		return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Default);
	}
}
