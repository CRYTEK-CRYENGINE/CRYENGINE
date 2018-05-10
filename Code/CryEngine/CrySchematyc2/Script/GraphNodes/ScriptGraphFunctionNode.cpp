// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/GraphNodes/ScriptGraphFunctionNode.h"

#include <CrySerialization/STL.h>
#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/IDomainContext.h>
#include <CrySchematyc2/IObject.h>
#include <CrySchematyc2/LibUtils.h>
#include <CrySchematyc2/Env/IEnvFunctionDescriptor.h>
#include <CrySchematyc2/Script/IScriptGraphNodeCompiler.h>
#include <CrySchematyc2/Script/IScriptGraphNodeCreationMenu.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "DomainContext.h"
#include "Script/GraphNodes/ScriptGraphNodeFactory.h"

SERIALIZATION_ENUM_BEGIN_NESTED2(Schematyc2, CScriptGraphFunctionNode, EFunctionType, "Schematyc Script Graph Node Function Type")
	SERIALIZATION_ENUM(Schematyc2::CScriptGraphFunctionNode::EFunctionType::ComponentPropertiesMember, "ComponentPropertiesMember", "Component Properties Member")
SERIALIZATION_ENUM_END()

namespace Schematyc2
{
	namespace ScriptGraphFunctionNodeUtils
	{
		struct SFunctionInputPtr
		{
			inline SFunctionInputPtr() {}
			
			inline SFunctionInputPtr(const IAnyPtr& _pValue)
				: pValue(_pValue)
			{}

			IAnyPtr pValue;
		};

		typedef std::map<uint32, SFunctionInputPtr> FunctionInputsById;

		inline bool Serialize(Serialization::IArchive& archive, SFunctionInputPtr& value, const char* szName, const char* szLabel)
		{
			if(value.pValue)
			{
				archive(*value.pValue, szName, szLabel);
				return true;
			}
			return false;
		}

		struct SEnvFunctionRuntimeParams
		{
			inline SEnvFunctionRuntimeParams(uint32 _componentIdx, const IEnvFunctionDescriptor* _pEnvFunctionDescriptor = nullptr)
				: componentIdx(_componentIdx)
				, pEnvFunctionDescriptor(_pEnvFunctionDescriptor)
			{}

			void Serialize(Serialization::IArchive& archive) {}

			inline bool operator == (const SEnvFunctionRuntimeParams& rhs) const
			{
				return (componentIdx == rhs.componentIdx) && (pEnvFunctionDescriptor == rhs.pEnvFunctionDescriptor);
			}

			uint32                        componentIdx;
			const IEnvFunctionDescriptor* pEnvFunctionDescriptor;
		};
	}

	using namespace ScriptGraphFunctionNodeUtils;

	const SGUID CScriptGraphFunctionNode::s_typeGUID = "1bcfd811-b8b7-4032-a90c-311dfa4454c6";

	CScriptGraphFunctionNode::SFunctionInput::SFunctionInput()
		: id(s_invalidId)
	{}

	CScriptGraphFunctionNode::SFunctionInput::SFunctionInput(uint32 _id, const IAnyPtr& _pValue)
		: id(_id)
		, pValue(_pValue)
	{}

	CScriptGraphFunctionNode::CScriptGraphFunctionNode() {}

	CScriptGraphFunctionNode::CScriptGraphFunctionNode(const SGUID& guid)
		: CScriptGraphNodeBase(guid)
	{}

	CScriptGraphFunctionNode::CScriptGraphFunctionNode(const SGUID& guid, const Vec2& pos, EFunctionType functionType, const SGUID& contextGUID, const SGUID& refGUID)
		: CScriptGraphNodeBase(guid, pos)
		, m_functionType(functionType)
		, m_contextGUID(contextGUID)
		, m_refGUID(refGUID)
	{
		CreateInputValues();
	}

	SGUID CScriptGraphFunctionNode::GetTypeGUID() const
	{
		return s_typeGUID;
	}

	EScriptGraphColor CScriptGraphFunctionNode::GetColor() const
	{
		return EScriptGraphColor::Red;
	}

	void CScriptGraphFunctionNode::Refresh(const SScriptRefreshParams& params)
	{
		LOADING_TIME_PROFILE_SECTION;

		CScriptGraphNodeBase::Refresh(params);

		if(m_refGUID)
		{
			CScriptGraphNodeBase::AddInput(EScriptGraphPortFlags::MultiLink | EScriptGraphPortFlags::Execute, "In");
			CScriptGraphNodeBase::AddOutput(EScriptGraphPortFlags::Execute, "Out");

			IEnvRegistry&         envRegistry = gEnv->pSchematyc2->GetEnvRegistry();
			const IScriptElement& element = CScriptGraphNodeBase::GetGraph()->GetElement_New();
			CDomainContext        domainContext(SDomainContextScope(&element.GetFile(), element.GetGUID()));

			switch(m_functionType)
			{
			case EFunctionType::ComponentPropertiesMember:
				{
					const IScriptComponentInstance* pComponentInstance = domainContext.GetScriptComponentInstance(m_contextGUID);
					const IEnvFunctionDescriptor*   pEnvFunctionDescriptor = envRegistry.GetFunction(m_refGUID);
					if(pComponentInstance && pEnvFunctionDescriptor)
					{
						stack_string name;
						domainContext.QualifyName(*pComponentInstance, *pEnvFunctionDescriptor, EDomainQualifier::Local, name);
						CScriptGraphNodeBase::SetName(name.c_str());

						for(uint32 inputIdx = 0, inputCount = pEnvFunctionDescriptor->GetInputCount(); inputIdx < inputCount; ++ inputIdx)
						{
							IAnyConstPtr pInputValue = pEnvFunctionDescriptor->GetInputValue(inputIdx);
							if(pInputValue)
							{
								CScriptGraphNodeBase::AddInput(EScriptGraphPortFlags::Data, pEnvFunctionDescriptor->GetInputName(inputIdx), pInputValue->GetTypeInfo().GetTypeId());
							}
						}

						for(uint32 outputIdx = 0, outputCount = pEnvFunctionDescriptor->GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
						{
							IAnyConstPtr pOutputValue = pEnvFunctionDescriptor->GetOutputValue(outputIdx);
							if(pOutputValue)
							{
								CScriptGraphNodeBase::AddOutput(EScriptGraphPortFlags::Data | EScriptGraphPortFlags::MultiLink, pEnvFunctionDescriptor->GetOutputName(outputIdx), pOutputValue->GetTypeInfo().GetTypeId());
							}
						}
					}
					break;
				}
			}
		}
	}

	void CScriptGraphFunctionNode::Serialize(Serialization::IArchive& archive)
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
				CreateInputValues();
				SerializeFunctionInputs(archive);
				break;
			}
		case ESerializationPass::Save:
			{
				SerializeBasicInfo(archive);
				SerializeFunctionInputs(archive);
				break;
			}
		case ESerializationPass::Edit:
			{
				Edit(archive);
				Validate(archive);
				break;
			}
		case ESerializationPass::Validate:
			{
				Validate(archive);
				break;
			}
		}
	}

	void CScriptGraphFunctionNode::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		CScriptGraphNodeBase::RemapGUIDs(guidRemapper);
		m_contextGUID = guidRemapper.Remap(m_contextGUID);
		m_refGUID     = guidRemapper.Remap(m_refGUID);
	}

	void CScriptGraphFunctionNode::Compile_New(IScriptGraphNodeCompiler& compiler) const
	{
		if(m_refGUID)
		{
			switch(m_functionType)
			{
			case EFunctionType::ComponentPropertiesMember:
				{
					const IEnvFunctionDescriptor* pEnvFunctionDescriptor = gEnv->pSchematyc2->GetEnvRegistry().GetFunction(m_refGUID);
					if(pEnvFunctionDescriptor)
					{
						const size_t componentIdx = LibUtils::FindComponentInstanceByGUID(compiler.GetLibClass(), m_contextGUID);
						if(componentIdx != INVALID_INDEX)
						{
							compiler.BindCallback(&Execute);
							compiler.BindAttribute(EAttributeId::Params, SEnvFunctionRuntimeParams(componentIdx, pEnvFunctionDescriptor));

							// #SchematycTODO : Check size of m_functionInputs vs pEnvFunctionDescriptor->GetInputCount()?

							for(uint32 inputIdx = 0, inputCount = pEnvFunctionDescriptor->GetInputCount(); inputIdx < inputCount; ++ inputIdx)
							{
								const IAnyPtr& pInputValue = m_functionInputs[inputIdx].pValue;
								if(pInputValue)
								{
									compiler.BindAnyInput(EInputIdx::FirstParam + inputIdx, *pInputValue);
								}
								else
								{
									// Report error?
								}
							}

							for(uint32 outputIdx = 0, outputCount = pEnvFunctionDescriptor->GetOutputCount(); outputIdx < outputCount; ++ outputIdx)
							{
								IAnyConstPtr pOutputValue = pEnvFunctionDescriptor->GetOutputValue(outputIdx);
								if(pOutputValue)
								{
									compiler.BindAnyOutput(EOutputIdx::FirstParam + outputIdx, *pOutputValue);
								}
								else
								{
									// Report error?
								}
							}
						}
					}
					break;
				}
			}
		}
	}

	void CScriptGraphFunctionNode::RegisterCreator(CScriptGraphNodeFactory& factory)
	{
		class CCreator : public IScriptGraphNodeCreator
		{
		private:

			class CNodeCreationMenuCommand : public IScriptGraphNodeCreationMenuCommand
			{
			public:

				CNodeCreationMenuCommand(EFunctionType functionType, const SGUID& contextGUID, const SGUID& refGUID)
					: m_functionType(functionType)
					, m_contextGUID(contextGUID)
					, m_refGUID(refGUID)
				{}

				// IMenuCommand

				IScriptGraphNodePtr Execute(const Vec2& pos)
				{
					return std::make_shared<CScriptGraphFunctionNode>(gEnv->pSchematyc2->CreateGUID(), pos, m_functionType, m_contextGUID, m_refGUID);
				}
				
				// ~IMenuCommand

			private:

				EFunctionType m_functionType;
				SGUID         m_contextGUID;
				SGUID         m_refGUID;
			};

		public:

			// IScriptGraphNodeCreator
			
			virtual SGUID GetTypeGUID() const override
			{
				return CScriptGraphFunctionNode::s_typeGUID;
			}

			virtual IScriptGraphNodePtr CreateNode() override
			{
				return std::make_shared<CScriptGraphFunctionNode>();
			}

			virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IDomainContext& domainContext, const IScriptGraphExtension& graph) override
			{
				const IScriptElement& graphElement = graph.GetElement_New();
				switch(graphElement.GetElementType())
				{
				case EScriptElementType::ComponentInstance:
					{
						if(graph.GetType() == EScriptGraphType::Property)
						{
							PopulateComponentPropertyGraphNodeCreationMenu(nodeCreationMenu, domainContext, static_cast<const IScriptComponentInstance&>(graphElement));
						}
						break;
					}
				}
			}

			// ~IScriptGraphNodeCreator

		private:

			void PopulateComponentPropertyGraphNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IDomainContext& domainContext, const IScriptComponentInstance& componentInstance)
			{
				// #SchematycTODO : Shouldn't the domain context should be taking care of some of this?
				IEnvRegistry&             envRegistry = gEnv->pSchematyc2->GetEnvRegistry();
				IComponentFactoryConstPtr pComponentFactory = envRegistry.GetComponentFactory(componentInstance.GetComponentGUID());
				if(pComponentFactory)
				{
					const EnvTypeId contextTypeId = pComponentFactory->GetPropertiesTypeInfo().GetTypeId().AsEnvTypeId();
					auto visitEnvFunction = [&nodeCreationMenu, &domainContext, &componentInstance, &contextTypeId] (const IEnvFunctionDescriptor& envFunctionDescriptor) -> EVisitStatus
					{
						if(envFunctionDescriptor.GetContextTypeId() == contextTypeId)
						{
							stack_string label;
							domainContext.QualifyName(componentInstance, envFunctionDescriptor, EDomainQualifier::Global, label);
							label.insert(0, "Function::");
							nodeCreationMenu.AddOption(label.c_str(), "", "", std::make_shared<CNodeCreationMenuCommand>(EFunctionType::ComponentPropertiesMember, componentInstance.GetGUID(), envFunctionDescriptor.GetGUID()));
						}
						return EVisitStatus::Continue;
					};
					envRegistry.VisitFunctions(EnvMemberFunctionVisitor::FromLambdaFunction(visitEnvFunction));
				}
			}
		};

		factory.RegisterCreator(std::make_shared<CCreator>());
	}

	void CScriptGraphFunctionNode::CreateInputValues()
	{
		if(m_refGUID)
		{
			switch(m_functionType)
			{
			case EFunctionType::ComponentPropertiesMember:
				{
					const IEnvFunctionDescriptor* pEnvFunctionDescriptor = gEnv->pSchematyc2->GetEnvRegistry().GetFunction(m_refGUID);
					if(pEnvFunctionDescriptor)
					{
						const uint32 inputCount = pEnvFunctionDescriptor->GetInputCount();
						m_functionInputs.resize(inputCount);
						for(uint32 inputIdx = 0; inputIdx < inputCount; ++ inputIdx)
						{
							IAnyConstPtr pInputValue = pEnvFunctionDescriptor->GetInputValue(inputIdx);
							m_functionInputs[inputIdx] = SFunctionInput(pEnvFunctionDescriptor->GetInputId(inputIdx), pInputValue ? pInputValue->Clone() : IAnyPtr());
						}
						break;
					}
				}
			}
		}
	}

	void CScriptGraphFunctionNode::SerializeBasicInfo(Serialization::IArchive& archive)
	{
		archive(m_functionType, "functionType");
		archive(m_contextGUID, "contextGUID");
		archive(m_refGUID, "refGUID");
	}

	void CScriptGraphFunctionNode::SerializeFunctionInputs(Serialization::IArchive& archive)
	{
		FunctionInputsById functionInputsById;
		for(SFunctionInput& functionInput : m_functionInputs)
		{
			if(functionInput.pValue)
			{
				functionInputsById.insert(FunctionInputsById::value_type(functionInput.id, functionInput.pValue));
			}
		}
		archive(functionInputsById, "inputs");
	}

	void CScriptGraphFunctionNode::Edit(Serialization::IArchive& archive)
	{
		for(uint32 inputIdx = 0, inputCount = m_functionInputs.size(); inputIdx < inputCount; ++ inputIdx)
		{
			const char*    szInputName = CScriptGraphNodeBase::GetInputName(EInputIdx::FirstParam + inputIdx);
			const IAnyPtr& pInputValue = m_functionInputs[inputIdx].pValue;
			if(szInputName && pInputValue)
			{
				archive(*pInputValue, szInputName, szInputName);
			}
		}
	}

	void CScriptGraphFunctionNode::Validate(Serialization::IArchive& archive)
	{
		if(m_refGUID)
		{
			switch(m_functionType)
			{
			case EFunctionType::ComponentPropertiesMember:
				{
					const IEnvFunctionDescriptor* pEnvFunctionDescriptor = gEnv->pSchematyc2->GetEnvRegistry().GetFunction(m_refGUID);
					if(!pEnvFunctionDescriptor)
					{
						archive.error(*this, "Unable to retrieve environment function!");
					}
					break;
				}
			}
		}
	}

	SRuntimeResult CScriptGraphFunctionNode::Execute(IObject* pObject, const SRuntimeActivationParams& activationParams, CRuntimeNodeData& data)
	{
		EnvFunctionInputs inputs = { nullptr };
		for(uint32 inputIdx = 0, inputCount = data.GetInputCount() - EInputIdx::FirstParam; inputIdx < inputCount; ++ inputIdx)
		{
			inputs[inputIdx] = data.GetInput(EInputIdx::FirstParam + inputIdx);
		}

		EnvFunctionOutputs outputs = { nullptr };
		for(uint32 outputIdx = 0, outputCount = data.GetOutputCount() - EOutputIdx::FirstParam; outputIdx < outputCount; ++ outputIdx)
		{
			outputs[outputIdx] = data.GetOutput(EOutputIdx::FirstParam + outputIdx);
		}

		SEnvFunctionRuntimeParams* pParams = data.GetAttribute<SEnvFunctionRuntimeParams>(EAttributeId::Params);
		IPropertiesPtr             pComponentProperties = pObject->GetComponentInstanceProperties(pParams->componentIdx);
		if(pComponentProperties)
		{
			SEnvFunctionResult result = pParams->pEnvFunctionDescriptor->Execute(SEnvFunctionContext(), pComponentProperties->ToVoidPtr(), inputs, outputs);
		}
		else
		{
			// #SchematycTODO : If we fail should we return some kind of error?
		}

		return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
	}
}
