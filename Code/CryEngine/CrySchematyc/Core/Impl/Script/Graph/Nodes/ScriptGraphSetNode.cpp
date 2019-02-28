// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphSetNode.h"

#include <CrySchematyc/Compiler/CompilerContext.h>
#include <CrySchematyc/Compiler/IGraphNodeCompiler.h>
#include <CrySchematyc/Script/Elements/IScriptVariable.h>
#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>
#include <CrySchematyc/Utils/Any.h>
#include <CrySchematyc/Utils/IGUIDRemapper.h>
#include <CrySchematyc/Utils/StackString.h>

#include "Object.h"
#include "Runtime/RuntimeClass.h"
#include "Script/ScriptView.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h"
#include "SerializationUtils/SerializationContext.h"

namespace Schematyc
{
CScriptGraphSetNode::SRuntimeData::SRuntimeData(uint32 _pos)
	: pos(_pos)
{}

CScriptGraphSetNode::SRuntimeData::SRuntimeData(const SRuntimeData& rhs)
	: pos(rhs.pos)
{}

void CScriptGraphSetNode::SRuntimeData::ReflectType(CTypeDesc<CScriptGraphSetNode::SRuntimeData>& desc)
{
	desc.SetGUID("1a4b1431-c8fe-46f5-aecd-fc8c11500e99"_cry_guid);
}

CScriptGraphSetNode::CScriptGraphSetNode() {}

CScriptGraphSetNode::CScriptGraphSetNode(const CryGUID& referenceGUID)
	: m_referenceGUID(referenceGUID)
{}

CryGUID CScriptGraphSetNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphSetNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetStyleId("Core::Data");

	CStackString subject;
	if (!GUID::IsEmpty(m_referenceGUID))
	{
		layout.AddInput("In", CryGUID(), { EScriptGraphPortFlags::Flow, EScriptGraphPortFlags::MultiLink });
		layout.AddOutput("Out", CryGUID(), EScriptGraphPortFlags::Flow);

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

						layout.AddInputWithData("Value", variable.GetTypeId().guid, { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink, EScriptGraphPortFlags::Persistent, EScriptGraphPortFlags::Editable }, *pData);
					}
					break;
				}
			}
		}
	}
	layout.SetName("Set", subject.c_str());
}

void CScriptGraphSetNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
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
							compiler.BindCallback(&Execute);
							compiler.BindData(SRuntimeData(variablePos));
						}
					}
					break;
				}
			}
		}
	}
}

void CScriptGraphSetNode::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_referenceGUID, "referenceGUID");
}

void CScriptGraphSetNode::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_referenceGUID, "referenceGUID");
}

void CScriptGraphSetNode::Validate(Serialization::IArchive& archive, const ISerializationContext& context)
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

void CScriptGraphSetNode::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	m_referenceGUID = guidRemapper.Remap(m_referenceGUID);
}

void CScriptGraphSetNode::Register(CScriptGraphNodeFactory& factory)
{
	class CCreator : public IScriptGraphNodeCreator
	{
	private:

		class CCreationCommand : public IScriptGraphNodeCreationCommand
		{
		public:

			CCreationCommand(const char* szSubject, const CryGUID& referenceGUID)
				: m_subject(szSubject)
				, m_referenceGUID(referenceGUID)
			{}

			// IScriptGraphNodeCreationCommand

			virtual const char* GetBehavior() const override
			{
				return "Set";
			}

			virtual const char* GetSubject() const override
			{
				return m_subject.c_str();
			}

			virtual const char* GetDescription() const override
			{
				return "Set value of variable";
			}

			virtual const char* GetStyleId() const override
			{
				return "Core::Data";
			}

			virtual IScriptGraphNodePtr Execute(const Vec2& pos) override
			{
				return std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphSetNode>(m_referenceGUID), pos);
			}

			// ~IScriptGraphNodeCreationCommand

		private:

			string m_subject;
			CryGUID  m_referenceGUID;
		};

	public:

		// IScriptGraphNodeCreator

		virtual CryGUID GetTypeGUID() const override
		{
			return CScriptGraphSetNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const CryGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphSetNode>());
		}

		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) override
		{
			switch (graph.GetType())
			{
			case EScriptGraphType::Construction:
			case EScriptGraphType::Signal:
			case EScriptGraphType::Function:
				{
					auto visitScriptVariable = [&nodeCreationMenu, &scriptView](const IScriptVariable& variable) -> EVisitStatus
					{
						if (!variable.IsArray())
						{
							CStackString subject;
							scriptView.QualifyName(variable, EDomainQualifier::Global, subject);
							nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>(subject.c_str(), variable.GetGUID()));
						}
						return EVisitStatus::Continue;
					};
					scriptView.VisitScriptVariables(visitScriptVariable, EDomainScope::Derived);

					// Library variables
					// TODO: Not yet supported.
					/*CScriptView gloablView(gEnv->pSchematyc->GetScriptRegistry().GetRootElement().GetGUID());
					   auto visitLibraries = [&nodeCreationMenu](const IScriptVariable& scriptVariable) -> EVisitStatus
					   {
					   if (!scriptVariable.IsArray())
					   {
					    CStackString subject;
					    QualifyScriptElementName(gEnv->pSchematyc->GetScriptRegistry().GetRootElement(), scriptVariable, EDomainQualifier::Global, subject);
					    nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>(subject.c_str(), scriptVariable.GetGUID()));
					   }
					   return EVisitStatus::Continue;
					   };
					   gloablView.VisitScriptModuleVariables(visitLibraries);*/
					// ~TODO

					break;
				}
			}
		}

		// ~IScriptGraphNodeCreator
	};

	factory.RegisterCreator(std::make_shared<CCreator>());
}

SRuntimeResult CScriptGraphSetNode::Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	const SRuntimeData& data = DynamicCast<SRuntimeData>(*context.node.GetData());
	Any::CopyAssign(*static_cast<CObject*>(context.pObject)->GetScratchpad().Get(data.pos), *context.node.GetInputData(EInputIdx::Value));

	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

const CryGUID CScriptGraphSetNode::ms_typeGUID = "23145b7a-4ce3-45b8-a34b-1c997ea6448f"_cry_guid;
} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphSetNode::Register)
