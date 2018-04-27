// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptSerializers.h"

#include <CrySerialization/IArchiveHost.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include "Script/Script.h"
#include "Script/ScriptElementFactory.h"
#include "Script/Elements/ScriptActionInstance.h"
#include "Script/Elements/ScriptBase.h"
#include "Script/Elements/ScriptClass.h"
#include "Script/Elements/ScriptComponentInstance.h"
#include "Script/Elements/ScriptConstructor.h"
#include "Script/Elements/ScriptEnum.h"
#include "Script/Elements/ScriptFunction.h"
#include "Script/Elements/ScriptInterface.h"
#include "Script/Elements/ScriptInterfaceFunction.h"
#include "Script/Elements/ScriptInterfaceImpl.h"
#include "Script/Elements/ScriptInterfaceTask.h"
#include "Script/Elements/ScriptModule.h"
#include "Script/Elements/ScriptSignal.h"
#include "Script/Elements/ScriptSignalReceiver.h"
#include "Script/Elements/ScriptState.h"
#include "Script/Elements/ScriptStateMachine.h"
#include "Script/Elements/ScriptStruct.h"
#include "Script/Elements/ScriptTimer.h"
#include "Script/Elements/ScriptVariable.h"
#include "SerializationUtils/SerializationContext.h"

namespace Schematyc
{

namespace {

CScriptElementFactory g_scriptElementFactory; // #SchematycTODO : Move to CScriptRegistry? Yes!

enum class EScriptElementOutputSerializerFlags
{
	None    = 0,
	Copying = BIT(0)
};

typedef CEnumFlags<EScriptElementOutputSerializerFlags> ScriptElementOutputSerializerFlags;

struct ScriptElementOutputSerializerParams
{
	inline ScriptElementOutputSerializerParams(const ScriptElementSerializeCallback& _callback, const ScriptElementOutputSerializerFlags& _flags)
		: callback(_callback)
		, flags(_flags)
	{}

	ScriptElementSerializeCallback     callback;
	ScriptElementOutputSerializerFlags flags;
};

class CScriptElementOutputSerializer
{
public:

	inline CScriptElementOutputSerializer()
		: m_pElement(nullptr)
		, m_pParams(nullptr)
	{}

	inline CScriptElementOutputSerializer(IScriptElement& element, const ScriptElementOutputSerializerParams& params)
		: m_pElement(&element)
		, m_pParams(&params)
	{}

	void Serialize(Serialization::IArchive& archive)
	{
		SCHEMATYC_ENV_ASSERT_FATAL(m_pElement && m_pParams);

		{
			CSerializationContext serializationContext(SSerializationContextParams(archive, ESerializationPass::Save));
			m_pElement->Serialize(archive);
		}

		if (m_pParams->callback)
		{
			m_pParams->callback(*m_pElement);
		}

		std::vector<CScriptElementOutputSerializer> children;
		children.reserve(20);
		for (IScriptElement* pChildElement = m_pElement->GetFirstChild(); pChildElement; pChildElement = pChildElement->GetNextSibling())
		{
			if (m_pParams->flags.Check(EScriptElementOutputSerializerFlags::Copying) || !pChildElement->GetScript())
			{
				children.push_back(CScriptElementOutputSerializer(*pChildElement, *m_pParams));
			}
		}
		archive(children, "children");
	}

private:

	IScriptElement*                            m_pElement;
	const ScriptElementOutputSerializerParams* m_pParams;
};

} // Anonymous

CScriptInputElementSerializer::CScriptInputElementSerializer(IScriptElement& element, ESerializationPass serializationPass, const ScriptElementSerializeCallback& callback)
	: m_element(element)
	, m_serializationPass(serializationPass)
{}

void CScriptInputElementSerializer::Serialize(Serialization::IArchive& archive)
{
	CSerializationContext serializationContext(SSerializationContextParams(archive, m_serializationPass));
	m_element.Serialize(archive);
}

SScriptInputElement::SScriptInputElement()
	: sortPriority(0)
{}

void SScriptInputElement::Serialize(Serialization::IArchive& archive)
{
	if (!instance)
	{
		EScriptElementType elementType = EScriptElementType::None;
		archive(elementType, "elementType");

		instance = g_scriptElementFactory.CreateElement(elementType);
		if (instance)
		{
			children.reserve(20);
			archive(children, "children");
			for (SScriptInputElement& child : children)
			{
				if (child.instance)
				{
					instance->AttachChild(*child.instance);
				}
			}
		}
	}
}

bool Serialize(Serialization::IArchive& archive, SScriptInputElement& value, const char* szName, const char*)
{
	archive(value.blackBox, szName);
	Serialization::LoadBlackBox(value, value.blackBox);
	return true;
}

CScriptLoadSerializer::CScriptLoadSerializer(SScriptInputBlock& inputBlock, const ScriptElementSerializeCallback& callback)
	: m_inputBlock(inputBlock)
	, m_callback(callback)
{}

void CScriptLoadSerializer::Serialize(Serialization::IArchive& archive)
{
	uint32 versionNumber = 0;
	archive(versionNumber, "version");
	// #SchematycTODO : Check version number!
	{
		archive(m_inputBlock.guid, "guid");
		archive(m_inputBlock.scopeGUID, "scope");
		archive(m_inputBlock.rootElement, "root");
	}
}

CScriptSaveSerializer::CScriptSaveSerializer(CScript& script, const ScriptElementSerializeCallback& callback)
	: m_script(script)
	, m_callback(callback)
{}

void CScriptSaveSerializer::Serialize(Serialization::IArchive& archive)
{
	uint32 versionNumber = 105;
	archive(versionNumber, "version");

	CryGUID guid = m_script.GetGUID();
	archive(guid, "guid");

	IScriptElement* pRoot = m_script.GetRoot();
	if (pRoot)
	{
		IScriptElement* pScope = pRoot->GetParent();
		if (pScope)
		{
			CryGUID scopeGUID = pScope->GetGUID();
			archive(scopeGUID, "scope");
		}

		archive(CScriptElementOutputSerializer(*pRoot, ScriptElementOutputSerializerParams(m_callback, EScriptElementOutputSerializerFlags::None)), "root");
	}
}

CScriptCopySerializer::CScriptCopySerializer(IScriptElement& root, const ScriptElementSerializeCallback& callback)
	: m_root(root)
	, m_callback(callback)
{}

void CScriptCopySerializer::Serialize(Serialization::IArchive& archive)
{
	archive(CScriptElementOutputSerializer(m_root, ScriptElementOutputSerializerParams(ScriptElementSerializeCallback(), EScriptElementOutputSerializerFlags::Copying)), "root");
}

CScriptPasteSerializer::CScriptPasteSerializer(SScriptInputBlock& inputBlock, const ScriptElementSerializeCallback& callback)
	: m_inputBlock(inputBlock)
	, m_callback(callback)
{}

void CScriptPasteSerializer::Serialize(Serialization::IArchive& archive)
{
	archive(m_inputBlock.rootElement, "root");
}

void UnrollScriptInputElementsRecursive(ScriptInputElementPtrs& output, SScriptInputElement& element)
{
	if (element.instance)
	{
		output.push_back(&element);
	}
	for (SScriptInputElement& childElement : element.children)
	{
		UnrollScriptInputElementsRecursive(output, childElement);
	}
}

bool SortScriptInputElementsByDependency(ScriptInputElementPtrs& elements)
{
	typedef std::unordered_map<CryGUID, SScriptInputElement*> ElementsByGUID;

	ElementsByGUID elementsByGUID;
	const uint32 elementCount = elements.size();
	elementsByGUID.reserve(elementCount);
	for (SScriptInputElement* pElement : elements)
	{
		elementsByGUID.insert(ElementsByGUID::value_type(pElement->instance->GetGUID(), pElement));
	}

	for (SScriptInputElement* pElement : elements)
	{
		auto visitElementDependency = [&elementsByGUID, pElement](const CryGUID& guid)
		{
			ElementsByGUID::const_iterator itDependency = elementsByGUID.find(guid);
			if (itDependency != elementsByGUID.end())
			{
				pElement->dependencies.push_back(itDependency->second);
			}
		};
		pElement->instance->EnumerateDependencies(visitElementDependency, EScriptDependencyType::Load);
	}

	uint32 recursiveDependencyCount = 0;
	for (SScriptInputElement* pElement : elements)
	{
		for (SScriptInputElement* pDependencyElement : pElement->dependencies)
		{
			if (std::find(pDependencyElement->dependencies.begin(), pDependencyElement->dependencies.end(), pElement) != pDependencyElement->dependencies.end())
			{
				++recursiveDependencyCount;
			}
		}
	}
	if (recursiveDependencyCount)
	{
		SCHEMATYC_CORE_CRITICAL_ERROR("%d recursive dependencies detected!", recursiveDependencyCount);
		return false;
	}

	bool bShuffle;
	do
	{
		bShuffle = false;
		for (SScriptInputElement* pElement : elements)
		{
			for (SScriptInputElement* pDependencyElement : pElement->dependencies)
			{
				if (pDependencyElement->sortPriority <= pElement->sortPriority)
				{
					pDependencyElement->sortPriority = pElement->sortPriority + 1;
					bShuffle = true;
				}
			}
		}
	}
	while (bShuffle);

	auto compareElements = [](const SScriptInputElement* lhs, const SScriptInputElement* rhs) -> bool
	{
		return lhs->sortPriority > rhs->sortPriority;
	};
	std::sort(elements.begin(), elements.end(), compareElements);
	return true;
}
} // Schematyc
