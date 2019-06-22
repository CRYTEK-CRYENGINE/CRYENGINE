// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/GraphNodes/ScriptGraphBranchNode.h"

#include <CrySchematyc2/IDomainContext.h>
#include <CrySchematyc2/Script/IScriptGraphNodeCompiler.h>
#include <CrySchematyc2/Script/IScriptGraphNodeCreationMenu.h>

#include "Script/GraphNodes/ScriptGraphNodeFactory.h"

namespace Schematyc2
{
	const SGUID CScriptGraphBranchNode::s_typeGUID = "4F2A4D42-ED2A-40A3-9555-DBB1210A36D8";

	CScriptGraphBranchNode::CScriptGraphBranchNode() 
		: m_bValue(false)
	{
	}

	CScriptGraphBranchNode::CScriptGraphBranchNode(const SGUID& guid)
		: CScriptGraphNodeBase(guid)
		, m_bValue(false)
	{}

	CScriptGraphBranchNode::CScriptGraphBranchNode(const SGUID& guid, const Vec2& pos)
		: CScriptGraphNodeBase(guid, pos)
		, m_bValue(false)
	{}

	SGUID CScriptGraphBranchNode::GetTypeGUID() const
	{
		return s_typeGUID;
	}

	EScriptGraphColor CScriptGraphBranchNode::GetColor() const
	{
		return EScriptGraphColor::Green;
	}

	void CScriptGraphBranchNode::Refresh(const SScriptRefreshParams& params)
	{
		CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
		
		CScriptGraphNodeBase::Refresh(params);

		CScriptGraphNodeBase::SetName("Branch");
		CScriptGraphNodeBase::AddInput(EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute, "In");
		CScriptGraphNodeBase::AddInput(EScriptGraphPortFlags::Data, "Value", GetAggregateTypeId<bool>());

		CScriptGraphNodeBase::AddOutput(EScriptGraphPortFlags::Execute, "True");
		CScriptGraphNodeBase::AddOutput(EScriptGraphPortFlags::Execute, "False");
	}

	void CScriptGraphBranchNode::Serialize(Serialization::IArchive& archive)
	{
		CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

		CScriptGraphNodeBase::Serialize(archive);

		archive(m_bValue, "value", "Value");
	}

	void CScriptGraphBranchNode::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		CScriptGraphNodeBase::RemapGUIDs(guidRemapper);
	}

	void CScriptGraphBranchNode::Compile_New(IScriptGraphNodeCompiler& compiler) const
	{
		compiler.BindCallback(&Execute);

		compiler.BindInput(EInputIdx::Value, m_bValue);
	}

	void CScriptGraphBranchNode::RegisterCreator(CScriptGraphNodeFactory& factory)
	{
		class CCreator : public IScriptGraphNodeCreator
		{
			class CNodeCreationMenuCommand : public IScriptGraphNodeCreationMenuCommand
			{
			public:

				// IScriptGraphNodeCreationMenuCommand
				IScriptGraphNodePtr Execute(const Vec2& pos)
				{
					return std::make_shared<CScriptGraphBranchNode>(gEnv->pSchematyc2->CreateGUID(), pos);
				}
				// ~IScriptGraphNodeCreationMenuCommand
			};

		public:

			// IScriptGraphNodeCreator
			virtual SGUID GetTypeGUID() const override
			{
				return CScriptGraphBranchNode::s_typeGUID;
			}

			virtual IScriptGraphNodePtr CreateNode() override
			{
				return std::make_shared<CScriptGraphBranchNode>(gEnv->pSchematyc2->CreateGUID());
			}

			virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IDomainContext& domainContext, const IScriptGraphExtension& graph) override 
			{
				nodeCreationMenu.AddOption("Branch", "Branch on input Bool value", "", std::make_shared<CNodeCreationMenuCommand>());
			}
			// ~IScriptGraphNodeCreator
		};

		factory.RegisterCreator(std::make_shared<CCreator>());
	}

	SRuntimeResult CScriptGraphBranchNode::Execute(IObject* pObject, const SRuntimeActivationParams& activationParams, CRuntimeNodeData& data)
	{
		const bool* pValue = data.GetInput<bool>(EInputIdx::Value);
		SCHEMATYC2_SYSTEM_ASSERT_FATAL(pValue != nullptr);

		const uint32 output = *pValue ? EOutputIdx::True : EOutputIdx::False;
		return SRuntimeResult(ERuntimeStatus::Continue, output);
	}
}
