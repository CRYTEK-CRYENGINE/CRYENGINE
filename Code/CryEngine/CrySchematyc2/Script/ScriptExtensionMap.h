// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptExtension.h>

namespace Schematyc2
{
	DECLARE_SHARED_POINTERS(IScriptExtension)

	class CScriptExtensionMap : public IScriptExtensionMap
	{
	public:
		
		void AddExtension(const IScriptExtensionPtr& pExtension);

		// IScriptExtensionMap
		virtual IScriptExtension* QueryExtension(EScriptExtensionId id) override;
		virtual const IScriptExtension* QueryExtension(EScriptExtensionId id) const override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) override;
		// ~IScriptExtensionMap

	private:

		typedef std::vector<IScriptExtensionPtr> Extensions;

		Extensions m_extensions;
	};
}
