// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScriptExtension.h>

namespace Schematyc
{
	DECLARE_SHARED_POINTERS(IScriptExtension)

	class CScriptExtensionMap : public IScriptExtensionMap
	{
	public:
		
		void AddExtension(const IScriptExtensionPtr& pExtension);

		// IScriptExtensionMap
		virtual IScriptExtension* QueryExtension(EScriptExtensionType type) override;
		virtual const IScriptExtension* QueryExtension(EScriptExtensionType type) const override;
		virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
		virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
		virtual void ProcessEvent(const SScriptEvent& event) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		// ~IScriptExtensionMap

	private:

		typedef std::vector<IScriptExtensionPtr> Extensions;

		Extensions m_extensions;
	};
}