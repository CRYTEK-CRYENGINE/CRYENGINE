// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/ScriptExtensionMap.h"

#include <CrySchematyc2/Services/ILog.h>

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, EScriptExtensionId, "Schematyc Script Extension Id")
	SERIALIZATION_ENUM(Schematyc2::EScriptExtensionId::Graph, "Graph", "Graph")
SERIALIZATION_ENUM_END()

namespace Schematyc2
{
	void CScriptExtensionMap::AddExtension(const IScriptExtensionPtr& pExtension)
	{
		if(pExtension)
		{
			m_extensions.push_back(pExtension);
		}
	}

	IScriptExtension* CScriptExtensionMap::QueryExtension(EScriptExtensionId id)
	{
		for(Extensions::value_type& pExtension : m_extensions)
		{
			if(pExtension->GetId_New() == id)
			{
				return pExtension.get();
			}
		}
		return nullptr;
	}

	const IScriptExtension* CScriptExtensionMap::QueryExtension(EScriptExtensionId id) const
	{
		for(const Extensions::value_type& pExtension : m_extensions)
		{
			if(pExtension->GetId_New() == id)
			{
				return pExtension.get();
			}
		}
		return nullptr;
	}

	void CScriptExtensionMap::Refresh(const SScriptRefreshParams& params)
	{
		for(Extensions::value_type& pExtension : m_extensions)
		{
			pExtension->Refresh_New(params);
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
				m_extension.Serialize_New(archive);
			}

		private:

			IScriptExtension& m_extension;
		};

		const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<EScriptExtensionId>();
		for(Extensions::value_type& pExtension : m_extensions)
		{
			const char* szName = enumDescription.name(pExtension->GetId_New());
			SCHEMATYC2_SYSTEM_ASSERT(szName && szName[0]);
			//archive(*pExtension, szName);
			archive(CExtensionWrapper(*pExtension), szName);
		}
	}

	void CScriptExtensionMap::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		for(Extensions::value_type& pExtension : m_extensions)
		{
			pExtension->RemapGUIDs_New(guidRemapper);
		}
	}
}
