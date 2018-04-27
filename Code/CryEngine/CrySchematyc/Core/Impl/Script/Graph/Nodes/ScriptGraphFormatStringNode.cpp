// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphFormatStringNode.h"

#include <CrySchematyc/Compiler/IGraphNodeCompiler.h>
#include <CrySchematyc/Script/IScriptView.h>
#include <CrySchematyc/Utils/StackString.h>
#include <CrySchematyc/Utils/SharedString.h>

#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h"
#include "SerializationUtils/SerializationContext.h"

SERIALIZATION_ENUM_BEGIN_NESTED2(Schematyc, CScriptGraphFormatStringNode, EElementForm, "CrySchematyc Script Graph Format String Node Element Form")
SERIALIZATION_ENUM(Schematyc::CScriptGraphFormatStringNode::EElementForm::Const, "Const", "Constant")
SERIALIZATION_ENUM(Schematyc::CScriptGraphFormatStringNode::EElementForm::Input, "Input", "Input")
SERIALIZATION_ENUM_END()

namespace Schematyc
{

CScriptGraphFormatStringNode::SElement::SElement()
	: form(EElementForm::Const)
{}

void CScriptGraphFormatStringNode::SElement::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(form, "form");
	switch (form)
	{
	case EElementForm::Const:
		{
			archive(text, "value");
			break;
		}
	case EElementForm::Input:
		{
			archive(text, "name");
			data.SerializeTypeId(archive);
			break;
		}
	}
}

void CScriptGraphFormatStringNode::SElement::PostLoad(Serialization::IArchive& archive, const ISerializationContext& context)
{
	switch (form)
	{
	case EElementForm::Input:
		{
			data.SerializeValue(archive);
			break;
		}
	}
}

void CScriptGraphFormatStringNode::SElement::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(form, "form");
	switch (form)
	{
	case EElementForm::Const:
		{
			archive(text, "value");
			break;
		}
	case EElementForm::Input:
		{
			archive(text, "name");
			data.SerializeTypeId(archive);
			data.SerializeValue(archive);
			break;
		}
	}
}

void CScriptGraphFormatStringNode::SElement::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(form, "form", "^");
	switch (form)
	{
	case EElementForm::Const:
		{
			archive(text, "value", "Value");
			break;
		}
	case EElementForm::Input:
		{
			archive(text, "name", "Name");
			data.SerializeTypeId(archive);
			data.Refresh();
			break;
		}
	}
}

bool CScriptGraphFormatStringNode::SElement::IsValidInput() const
{
	return (form == EElementForm::Input) && !text.empty() && !data.IsEmpty();
}

CScriptGraphFormatStringNode::SRuntimeData::SRuntimeData(const ElementsPtr& _pElements)
	: pElements(_pElements)
{}

CScriptGraphFormatStringNode::SRuntimeData::SRuntimeData(const SRuntimeData& rhs)
	: pElements(rhs.pElements)
{}

void CScriptGraphFormatStringNode::SRuntimeData::ReflectType(CTypeDesc<CScriptGraphFormatStringNode::SRuntimeData>& desc)
{
	desc.SetGUID("3ade58a5-2317-406c-8411-807cb081a6e6"_cry_guid);
}

CScriptGraphFormatStringNode::CScriptGraphFormatStringNode()
	: m_elements(1)
{}

CryGUID CScriptGraphFormatStringNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphFormatStringNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetName("Format String");
	layout.SetStyleId("Core::Utility");

	layout.AddInput("In", CryGUID(), { EScriptGraphPortFlags::Flow, EScriptGraphPortFlags::MultiLink });
	layout.AddOutput("Out", CryGUID(), EScriptGraphPortFlags::Flow);
	layout.AddOutputWithData("Result", GetTypeDesc<CSharedString>().GetGUID(), EScriptGraphPortFlags::Data, CSharedString());

	for (const SElement& element : m_elements)
	{
		if (element.IsValidInput())
		{
			layout.AddInputWithData(element.text.c_str(), element.data.GetTypeId().guid, { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::Persistent, EScriptGraphPortFlags::Editable }, *element.data.GetValue());
		}
	}
}

void CScriptGraphFormatStringNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
{
	compiler.BindCallback(&Execute);
	compiler.BindData(SRuntimeData(std::make_shared<Elements>(m_elements)));
}

void CScriptGraphFormatStringNode::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_elements, "elements");
}

void CScriptGraphFormatStringNode::PostLoad(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_elements, "elements");
}

void CScriptGraphFormatStringNode::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_elements, "elements");
}

void CScriptGraphFormatStringNode::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	string preview;
	for (const SElement& element : m_elements)
	{
		if (element.IsValidInput())
		{
			preview.append("[");
			preview.append(element.data.GetTypeName());
			preview.append("]");
		}
		else if (element.form == EElementForm::Const)
		{
			preview.append(element.text);
		}
	}
	archive(preview, "preview", "^!");

	{
		ScriptVariableData::CScopedSerializationConfig serializationConfig(archive);

		const IScriptElement& scriptElement = CScriptGraphNodeModel::GetNode().GetGraph().GetElement();
		const CryGUID guid = scriptElement.GetGUID();
		serializationConfig.DeclareEnvDataTypes(guid);
		serializationConfig.DeclareScriptEnums(guid);

		archive(m_elements, "elements", "Elements");
	}
}

void CScriptGraphFormatStringNode::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	for (SElement& element : m_elements)
	{
		element.data.RemapDependencies(guidRemapper);
	}
}

void CScriptGraphFormatStringNode::Register(CScriptGraphNodeFactory& factory)
{
	class CCreator : public IScriptGraphNodeCreator
	{
	private:

		class CCreationCommand : public IScriptGraphNodeCreationCommand
		{
		public:

			// IScriptGraphNodeCreationCommand

			virtual const char* GetBehavior() const override
			{
				return "Format String";
			}

			virtual const char* GetSubject() const override
			{
				return nullptr;
			}

			virtual const char* GetDescription() const override
			{
				return "Convert and concatenate multiple inputs to string";
			}

			virtual const char* GetStyleId() const override
			{
				return "Core::Utility";
			}

			virtual IScriptGraphNodePtr Execute(const Vec2& pos) override
			{
				return std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphFormatStringNode>(), pos);
			}

			// ~IScriptGraphNodeCreationCommand
		};

	public:

		// IScriptGraphNodeCreator

		virtual CryGUID GetTypeGUID() const override
		{
			return CScriptGraphFormatStringNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const CryGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphFormatStringNode>());
		}

		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) override
		{
			nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>());
		}

		// ~IScriptGraphNodeCreator
	};

	factory.RegisterCreator(std::make_shared<CCreator>());
}

SRuntimeResult CScriptGraphFormatStringNode::Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	SRuntimeData& data = DynamicCast<SRuntimeData>(*context.node.GetData());
	CSharedString& result = DynamicCast<CSharedString>(*context.node.GetOutputData(EOutputIdx::Result));

	result.clear();

	uint32 inputIdx = EInputIdx::FirstParam;
	for (const SElement& element : * data.pElements)
	{
		if (element.IsValidInput())
		{
			CStackString temp;
			Any::ToString(temp, *context.node.GetInputData(inputIdx));
			result.append(temp.c_str());
			++inputIdx;
		}
		else if (element.form == EElementForm::Const)
		{
			result.append(element.text.c_str());
		}
	}

	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

const CryGUID CScriptGraphFormatStringNode::ms_typeGUID = "7de077fd-97bb-4f98-955c-1a165d0e5efb"_cry_guid;

} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphFormatStringNode::Register)
