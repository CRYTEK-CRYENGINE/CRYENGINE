// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphGetNode.h"

#include <CrySchematyc/Compiler/CompilerContext.h>
#include <CrySchematyc/Compiler/IGraphNodeCompiler.h>
#include <CrySchematyc/Script/Elements/IScriptVariable.h>
#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>
#include <CrySchematyc/Utils/Any.h>
#include <CrySchematyc/Utils/AnyArray.h>
#include <CrySchematyc/Utils/IGUIDRemapper.h>
#include <CrySchematyc/Utils/StackString.h>

#include "Object.h"
#include "Runtime/RuntimeClass.h"
#include "Script/ScriptView.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h"
#include "SerializationUtils/SerializationContext.h"

#include <CryEntitySystem/IEntityComponent.h>

namespace Schematyc
{
CScriptGraphGetNode::SRuntimeData::SRuntimeData(uint32 _pos)
	: pos(_pos)
{}

CScriptGraphGetNode::SRuntimeData::SRuntimeData(const SRuntimeData& rhs)
	: pos(rhs.pos)
{}

void CScriptGraphGetNode::SRuntimeData::ReflectType(CTypeDesc<CScriptGraphGetNode::SRuntimeData>& desc)
{
	desc.SetGUID("be778830-e855-42d3-a87f-424161017339"_cry_guid);
}

CScriptGraphGetNode::CScriptGraphGetNode() {}

CScriptGraphGetNode::CScriptGraphGetNode(const CryGUID& referenceGUID,uint32 componentMemberId)
	: m_referenceGUID(referenceGUID)
	, m_componentMemberId(componentMemberId)
{}

CryGUID CScriptGraphGetNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphGetNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetStyleId("Core::Data");

	CStackString subject;
	if (!GUID::IsEmpty(m_referenceGUID))
	{
		CScriptView scriptView(CScriptGraphNodeModel::GetNode().GetGraph().GetElement().GetGUID());
		const IScriptElement* pReferenceElement = scriptView.GetScriptElement(m_referenceGUID);
		if (pReferenceElement)
		{
			switch (pReferenceElement->GetType())
			{
			case EScriptElementType::Variable:
				{
					const IScriptVariable& variable = DynamicCast<IScriptVariable>(*pReferenceElement);
					CAnyConstPtr pData = variable.GetData();
					if (pData)
					{
						subject = variable.GetName();

						if (!variable.IsArray())
						{
							layout.AddOutputWithData("Value", variable.GetTypeId().guid, { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink, EScriptGraphPortFlags::Pull }, *pData);
						}
						else
						{
							layout.AddOutputWithData("Value", variable.GetTypeId().guid, { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink, EScriptGraphPortFlags::Array, EScriptGraphPortFlags::Pull }, CAnyArrayPtr());
						}
					}
					break;
				}
			case EScriptElementType::ComponentInstance:
				{
					const IScriptComponentInstance& scriptComponentInstance = DynamicCast<IScriptComponentInstance>(*pReferenceElement);
					if (scriptComponentInstance.GetProperties().GetTypeDesc())
					{
						const CClassMemberDesc* pMember = scriptComponentInstance.GetProperties().GetTypeDesc()->FindMemberById(m_componentMemberId);
						if (pMember)
						{
							CAnyConstPtr pData(pMember->GetTypeDesc(), pMember->GetDefaultValue());
							if (pData)
							{
								subject = scriptComponentInstance.GetName();
								subject += ".";
								subject += pMember->GetLabel();

								//if (!variable.IsArray())
								//{
								layout.AddOutputWithData("Value", pData->GetTypeDesc().GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink, EScriptGraphPortFlags::Pull }, *pData);
								//}
								//else
								{
									//layout.AddOutputWithData("Value", pData->GetTypeDesc().GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink, EScriptGraphPortFlags::Array, EScriptGraphPortFlags::Pull }, CAnyArrayPtr());
								}
							}
						}
					}
					break;
				}
			}
		}
	}
	layout.SetName("Get", subject.c_str());
}

void CScriptGraphGetNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
{
	if (!GUID::IsEmpty(m_referenceGUID))
	{
		CScriptView scriptView(CScriptGraphNodeModel::GetNode().GetGraph().GetElement().GetGUID());
		const IScriptElement* pReferenceElement = scriptView.GetScriptElement(m_referenceGUID);
		if (pReferenceElement)
		{
			switch (pReferenceElement->GetType())
			{
			case EScriptElementType::Variable:
				{
					const IScriptVariable& variable = DynamicCast<IScriptVariable>(*pReferenceElement);
					const CRuntimeClass* pClass = context.interfaces.Query<const CRuntimeClass>();
					if (pClass)
					{
						const uint32 variablePos = pClass->GetVariablePos(variable.GetGUID());
						if (variablePos != InvalidIdx)
						{
							if (!variable.IsArray())
							{
								compiler.BindCallback(&Execute);
							}
							else
							{
								compiler.BindCallback(&ExecuteArray);
							}
							compiler.BindData(SRuntimeData(variablePos));
						}
					}
					break;
				}
			case EScriptElementType::ComponentInstance:
				{
					const IScriptComponentInstance& scriptComponentInstance = DynamicCast<IScriptComponentInstance>(*pReferenceElement);
					const CRuntimeClass* pClass = context.interfaces.Query<const CRuntimeClass>();
					if (pClass && scriptComponentInstance.GetProperties().GetTypeDesc())
					{
						SComponentPropertyRuntimeData runtimeData;
						runtimeData.componentMemberIndex = scriptComponentInstance.GetProperties().GetTypeDesc()->FindMemberIndexById(m_componentMemberId);
						runtimeData.componentIdx = pClass->FindComponentInstance(m_referenceGUID);
						if (runtimeData.componentIdx != InvalidIdx && runtimeData.componentMemberIndex != InvalidIdx)
						{
							compiler.BindCallback(&ExecuteGetComponentProperty);
							compiler.BindData(runtimeData);
						}
					}
					break;
				}
			}
		}
	}
}

void CScriptGraphGetNode::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_referenceGUID, "referenceGUID");
	archive(m_componentMemberId,"memberId");
}

void CScriptGraphGetNode::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_referenceGUID, "referenceGUID");
	archive(m_componentMemberId,"memberId");
}

void CScriptGraphGetNode::Validate(Serialization::IArchive& archive, const ISerializationContext& context)
{
	if (!GUID::IsEmpty(m_referenceGUID))
	{
		CScriptView scriptView(CScriptGraphNodeModel::GetNode().GetGraph().GetElement().GetGUID());
		const IScriptElement* pReferenceElement = scriptView.GetScriptElement(m_referenceGUID);
		if (!pReferenceElement)
		{
			archive.error(*this, "Failed to retrieve reference!");
		}
	}
}

void CScriptGraphGetNode::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	m_referenceGUID = guidRemapper.Remap(m_referenceGUID);
}

void CScriptGraphGetNode::Register(CScriptGraphNodeFactory& factory)
{
	class CCreator : public IScriptGraphNodeCreator
	{
	private:

		class CCreationCommand : public IScriptGraphNodeCreationCommand
		{
		public:

			CCreationCommand(const char* szSubject, const CryGUID& referenceGUID,uint32 componentMemberId=0)
				: m_subject(szSubject)
				, m_referenceGUID(referenceGUID)
				, m_componentMemberId(componentMemberId)
			{}

			// IScriptGraphNodeCreationCommand

			virtual const char* GetBehavior() const override
			{
				return "Get";
			}

			virtual const char* GetSubject() const override
			{
				return m_subject.c_str();
			}

			virtual const char* GetDescription() const override
			{
				if (m_componentMemberId)
					return "Get value of the component property";
				return "Get value of variable";
			}

			virtual const char* GetStyleId() const override
			{
				return "Core::Data";
			}

			virtual IScriptGraphNodePtr Execute(const Vec2& pos) override
			{
				return std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphGetNode>(m_referenceGUID,m_componentMemberId), pos);
			}

			// ~IScriptGraphNodeCreationCommand

		private:

			string m_subject;
			CryGUID  m_referenceGUID;
			uint32   m_componentMemberId;
		};

	public:

		// IScriptGraphNodeCreator

		virtual CryGUID GetTypeGUID() const override
		{
			return CScriptGraphGetNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const CryGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphGetNode>());
		}

		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) override
		{
			auto visitScriptVariable = [&nodeCreationMenu, &scriptView](const IScriptVariable& variable) -> EVisitStatus
			{
				CStackString subject;
				scriptView.QualifyName(variable, EDomainQualifier::Global, subject);
				nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>(subject.c_str(), variable.GetGUID()));

				return EVisitStatus::Continue;
			};
			scriptView.VisitScriptVariables(visitScriptVariable, EDomainScope::Derived);

			// Library variables
			// TODO: Not yet supported.
			/*CScriptView gloablView(gEnv->pSchematyc->GetScriptRegistry().GetRootElement().GetGUID());
			   auto visitLibraries = [&nodeCreationMenu](const IScriptVariable& scriptVariable) -> EVisitStatus
			   {
			   CStackString subject;
			   QualifyScriptElementName(gEnv->pSchematyc->GetScriptRegistry().GetRootElement(), scriptVariable, EDomainQualifier::Global, subject);
			   nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>(subject.c_str(), scriptVariable.GetGUID()));

			   return EVisitStatus::Continue;
			   };
			   gloablView.VisitScriptModuleVariables(visitLibraries);*/
			// ~TODO

			// Populate component properties
			auto visitScriptComponentInstance = [&nodeCreationMenu,&scriptView](const IScriptComponentInstance& scriptComponentInstance) -> EVisitStatus
			{
				CStackString baseName;
				baseName = "Components::";
				baseName += scriptComponentInstance.GetName();
				baseName += "::";

				const CClassDesc* classDesc = scriptComponentInstance.GetProperties().GetTypeDesc();
				if (!classDesc)
					return EVisitStatus::Continue;

				const CClassMemberDescArray &members = classDesc->GetMembers();
				for (const CClassMemberDesc& member : members)
				{
					CStackString name(baseName);
					name += member.GetLabel();
					uint32 memberId = member.GetId();
					nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>(name.c_str(), scriptComponentInstance.GetGUID(),memberId));
				}
				return EVisitStatus::Continue;
			};
			scriptView.VisitScriptComponentInstances(visitScriptComponentInstance, EDomainScope::All);
		}

		// ~IScriptGraphNodeCreator
	};

	factory.RegisterCreator(std::make_shared<CCreator>());
}

SRuntimeResult CScriptGraphGetNode::Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	const SRuntimeData& data = DynamicCast<SRuntimeData>(*context.node.GetData());
	const CAnyPtr pVariable = static_cast<CObject*>(context.pObject)->GetScratchpad().Get(data.pos);
	Any::CopyAssign(*context.node.GetOutputData(EOutputIdx::Value), *pVariable);

	return SRuntimeResult(ERuntimeStatus::Continue);
}

SRuntimeResult CScriptGraphGetNode::ExecuteArray(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	const SRuntimeData& data = DynamicCast<SRuntimeData>(*context.node.GetData());
	CAnyArray* pArray = DynamicCast<CAnyArray>(static_cast<CObject*>(context.pObject)->GetScratchpad().Get(data.pos));
	Any::CopyAssign(*context.node.GetOutputData(EOutputIdx::Value), CAnyArrayPtr(pArray));

	return SRuntimeResult(ERuntimeStatus::Continue);
}

Schematyc::SRuntimeResult CScriptGraphGetNode::ExecuteGetComponentProperty(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	const SComponentPropertyRuntimeData& runtimeData = DynamicCast<SComponentPropertyRuntimeData>(*context.node.GetData());
	
	IEntityComponent* pComponent = static_cast<CObject*>(context.pObject)->GetComponent(runtimeData.componentIdx);
	if (pComponent)
	{
		const CClassMemberDescArray &members = pComponent->GetClassDesc().GetMembers();
		assert(runtimeData.componentMemberIndex < members.size());
		if (runtimeData.componentMemberIndex < members.size())
		{
			const CClassMemberDesc& member = members[runtimeData.componentMemberIndex];
			
			// Pointer to the member of the component
			CAnyConstRef memberAny(member.GetTypeDesc(), (const void*)(reinterpret_cast<uint8*>(pComponent) + member.GetOffset()));

			// Assign component member to the Graph data output
			Any::CopyAssign(*context.node.GetOutputData(EOutputIdx::Value), memberAny);
		}
	}

	return SRuntimeResult(ERuntimeStatus::Continue);
}

const CryGUID CScriptGraphGetNode::ms_typeGUID = "a15c2533-d004-4266-b121-68404aa5e02f"_cry_guid;

} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphGetNode::Register)
