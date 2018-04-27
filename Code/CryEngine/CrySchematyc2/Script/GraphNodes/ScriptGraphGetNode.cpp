// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/GraphNodes/ScriptGraphGetNode.h"

#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/IDomainContext.h>
#include <CrySchematyc2/IObject.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Script/IScriptGraphNodeCompiler.h>
#include <CrySchematyc2/Script/IScriptGraphNodeCreationMenu.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "Script/GraphNodes/ScriptGraphNodeFactory.h"

namespace Schematyc2
{
	struct SGetPropertyRuntimeParams
	{
		inline SGetPropertyRuntimeParams(uint32 _propertyIdx)
			: propertyIdx(_propertyIdx)
		{}

		void Serialize(Serialization::IArchive& archive) {}

		inline bool operator == (const SGetPropertyRuntimeParams& rhs) const
		{
			return propertyIdx == rhs.propertyIdx;
		}

		uint32 propertyIdx;
	};

	const SGUID CScriptGraphGetNode::s_typeGUID = "b7bfa6f4-031a-422e-ac50-57aec8bdd57a";

	CScriptGraphGetNode::CScriptGraphGetNode(const SGUID& guid)
		: CScriptGraphNodeBase(guid)
	{}

	CScriptGraphGetNode::CScriptGraphGetNode(const SGUID& guid, const Vec2& pos, const SGUID& refGUID)
		: CScriptGraphNodeBase(guid, pos)
		, m_refGUID(refGUID)
	{}

	SGUID CScriptGraphGetNode::GetTypeGUID() const
	{
		return s_typeGUID;
	}

	Schematyc2::SGUID CScriptGraphGetNode::GetRefGUID() const
	{
		return m_refGUID;
	}

	EScriptGraphColor CScriptGraphGetNode::GetColor() const
	{
		return EScriptGraphColor::Blue;
	}

	void CScriptGraphGetNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CScriptGraphNodeBase::Refresh(params);
		
		if(m_refGUID)
		{
			IScriptGraphExtension* pGraph = CScriptGraphNodeBase::GetGraph();
			if(pGraph)
			{
				const IScriptElement* pRefElement = pGraph->GetElement_New().GetFile().GetElement(m_refGUID); // #SchematycTODO : Should we be using a domain context to retrieve this?
				if(pRefElement)
				{
					switch(pRefElement->GetElementType())
					{
					case EScriptElementType::Variable:
						{
							const IScriptVariable* pVariable = static_cast<const IScriptVariable*>(pRefElement);
							IAnyConstPtr           pVariableValue = pVariable->GetValue();
							if(pVariableValue)
							{
								stack_string name = "Get";
								name.append(" ");
								name.append(pVariable->GetName());
								CScriptGraphNodeBase::SetName(name.c_str());
								CScriptGraphNodeBase::AddOutput(EScriptGraphPortFlags::Data | EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Pull, "Value", pVariableValue->GetTypeInfo().GetTypeId());
							}
							break;
						}
					case EScriptElementType::Property:
						{
							const IScriptProperty* pProperty = static_cast<const IScriptProperty*>(pRefElement);
							IAnyConstPtr           pPropertyValue = pProperty->GetValue();
							if(pPropertyValue)
							{
								stack_string name = "Get";
								name.append(" ");
								name.append(pProperty->GetName());
								CScriptGraphNodeBase::SetName(name.c_str());
								CScriptGraphNodeBase::AddOutput(EScriptGraphPortFlags::Data | EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Pull, "Value", pPropertyValue->GetTypeInfo().GetTypeId());
							}
							break;
						}
					}
				}
			}
		}
	}

	void CScriptGraphGetNode::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CScriptGraphNodeBase::Serialize(archive);

		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
		case ESerializationPass::Save:
			{
				SerializeBasicInfo(archive);
				break;
			}
		case ESerializationPass::Edit:
		case ESerializationPass::Validate:
			{
				Validate(archive);
				break;
			}
		}
	}

	void CScriptGraphGetNode::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		CScriptGraphNodeBase::RemapGUIDs(guidRemapper);
		m_refGUID = guidRemapper.Remap(m_refGUID);
	}

	void CScriptGraphGetNode::Compile_New(IScriptGraphNodeCompiler& compiler) const
	{
		if(m_refGUID)
		{
			const IScriptGraphExtension* pGraph = CScriptGraphNodeBase::GetGraph();
			if(pGraph)
			{
				const IScriptElement* pRefElement = pGraph->GetElement_New().GetFile().GetElement(m_refGUID); // #SchematycTODO : Should we be using a domain context to retrieve this?
				if(pRefElement)
				{
					switch(pRefElement->GetElementType())
					{
					case EScriptElementType::Property:
						{
							const IScriptProperty* pProperty = static_cast<const IScriptProperty*>(pRefElement);
							IAnyConstPtr           pPropertyValue = pProperty->GetValue();
							if(pPropertyValue)
							{
								const size_t propertyIdx = LibUtils::FindVariableByGUID(compiler.GetLibClass(), m_refGUID);
								if(propertyIdx != INVALID_INDEX)
								{
									compiler.BindCallback(&Execute);
									compiler.BindAttribute(EAttributeId::Params, SGetPropertyRuntimeParams(propertyIdx));
									compiler.BindAnyOutput(EOutputIdx::Value, *pPropertyValue);
								}
							}
							break;
						}
					}
				}
			}
		}
	}

	void CScriptGraphGetNode::RegisterCreator(CScriptGraphNodeFactory& factory)
	{
		class CCreator : public IScriptGraphNodeCreator
		{
		private:

			class CNodeCreationMenuCommand : public IScriptGraphNodeCreationMenuCommand
			{
			public:

				CNodeCreationMenuCommand(const SGUID& refGUID)
					: m_refGUID(refGUID)
				{}

				// IMenuCommand

				IScriptGraphNodePtr Execute(const Vec2& pos)
				{
					return std::make_shared<CScriptGraphGetNode>(gEnv->pSchematyc2->CreateGUID(), pos, m_refGUID);
				}
				
				// ~IMenuCommand

			private:

				SGUID m_refGUID;
			};

		public:

			// IScriptGraphNodeCreator
			
			virtual SGUID GetTypeGUID() const override
			{
				return CScriptGraphGetNode::s_typeGUID;
			}

			virtual IScriptGraphNodePtr CreateNode() override
			{
				return std::make_shared<CScriptGraphGetNode>(gEnv->pSchematyc2->CreateGUID());
			}

			virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IDomainContext& domainContext, const IScriptGraphExtension& graph) override
			{
				auto visitScriptProperty = [&nodeCreationMenu, &domainContext] (const IScriptProperty& property) -> EVisitStatus
				{
					stack_string label;
					domainContext.QualifyName(property, EDomainQualifier::Global, label);
					label.insert(0, "Get::");
					nodeCreationMenu.AddOption(label.c_str(), "Get value of property", "", std::make_shared<CNodeCreationMenuCommand>(property.GetGUID()));
					return EVisitStatus::Continue;
				};
				domainContext.VisitScriptProperties(ScriptPropertyConstVisitor::FromLambdaFunction(visitScriptProperty), EDomainScope::Derived);
			}

			// ~IScriptGraphNodeCreator
		};

		factory.RegisterCreator(std::make_shared<CCreator>());
	}

	void CScriptGraphGetNode::SerializeBasicInfo(Serialization::IArchive& archive)
	{
		archive(m_refGUID, "refGUID");
	}

	void CScriptGraphGetNode::Validate(Serialization::IArchive& archive)
	{
		IScriptGraphExtension* pGraph = CScriptGraphNodeBase::GetGraph();
		if(pGraph)
		{
			if(!pGraph->GetElement_New().GetFile().GetElement(m_refGUID)) // #SchematycTODO : Should we be using a domain context to retrieve this?
			{
				archive.error(*this, "Failed to retrieve reference!");
			}
		}
	}

	SRuntimeResult CScriptGraphGetNode::Execute(IObject* pObject, const SRuntimeActivationParams& activationParams, CRuntimeNodeData& data)
	{
		SGetPropertyRuntimeParams* pParams = data.GetAttribute<SGetPropertyRuntimeParams>(EAttributeId::Params);
		IAnyConstPtr               pPropertyValue = pObject->GetProperty(pParams->propertyIdx);
		if(pPropertyValue)
		{
			*data.GetOutput(EOutputIdx::Value) = *pPropertyValue;
		}
		else
		{
			// #SchematycTODO : If we fail should we return some kind of error?
		}

		return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Value);
	}
}
