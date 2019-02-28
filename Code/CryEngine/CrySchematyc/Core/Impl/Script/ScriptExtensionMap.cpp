// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptExtensionMap.h"

#include <CrySerialization/Enum.h>
#include <CrySchematyc/Utils/Assert.h>

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc, EScriptExtensionType, "CrySchematyc Script Extension Type")
SERIALIZATION_ENUM(Schematyc::EScriptExtensionType::Graph, "Graph", "Graph")
SERIALIZATION_ENUM_END()

namespace Schematyc
{
void CScriptExtensionMap::AddExtension(const IScriptExtensionPtr& pExtension)
{
	if (pExtension)
	{
		m_extensions.push_back(pExtension);
	}
}

IScriptExtension* CScriptExtensionMap::QueryExtension(EScriptExtensionType type)
{
	for (Extensions::value_type& pExtension : m_extensions)
	{
		if (pExtension->GetExtensionType() == type)
		{
			return pExtension.get();
		}
	}
	return nullptr;
}

const IScriptExtension* CScriptExtensionMap::QueryExtension(EScriptExtensionType type) const
{
	for (const Extensions::value_type& pExtension : m_extensions)
	{
		if (pExtension->GetExtensionType() == type)
		{
			return pExtension.get();
		}
	}
	return nullptr;
}

void CScriptExtensionMap::EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const
{
	for (const Extensions::value_type& pExtension : m_extensions)
	{
		pExtension->EnumerateDependencies(enumerator, type);
	}
}

void CScriptExtensionMap::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	for (Extensions::value_type& pExtension : m_extensions)
	{
		pExtension->RemapDependencies(guidRemapper);
	}
}

void CScriptExtensionMap::ProcessEvent(const SScriptEvent& event)
{
	for (Extensions::value_type& pExtension : m_extensions)
	{
		pExtension->ProcessEvent(event);
	}
}

void CScriptExtensionMap::Serialize(Serialization::IArchive& archive)
{
	class CExtensionWrapper
	{
	public:

		inline CExtensionWrapper(IScriptExtension& extension)
			: m_extension(extension)
		{}

		void Serialize(Serialization::IArchive& archive)
		{
			m_extension.Serialize(archive);
		}

	private:

		IScriptExtension& m_extension;
	};

	const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<EScriptExtensionType>();
	for (Extensions::value_type& pExtension : m_extensions)
	{
		const char* szName = enumDescription.name(pExtension->GetExtensionType());
		SCHEMATYC_CORE_ASSERT(szName && szName[0]);
		//archive(*pExtension, szName);
		archive(CExtensionWrapper(*pExtension), szName);
	}
}
} // Schematyc
