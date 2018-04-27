// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc2
{
	class CSerializationContext;

	class CScriptComponentInstance : public CScriptElementBase<IScriptComponentInstance>
	{
	public:

		CScriptComponentInstance(IScriptFile& file);
		CScriptComponentInstance(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName, const SGUID& componentGUID, EScriptComponentInstanceFlags flags);

		// IScriptElement
		virtual EAccessor GetAccessor() const override;
		virtual SGUID GetGUID() const override;
		virtual SGUID GetScopeGUID() const override;
		virtual bool SetName(const char* szName) override;
		virtual const char* GetName() const override;
		virtual void EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const override;
		virtual void Refresh(const SScriptRefreshParams& params) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		virtual void RemapGUIDs(IGUIDRemapper& guidRemapper) override;
		// ~IScriptElement

		// IScriptComponentInstance
		virtual SGUID GetComponentGUID() const override;
		virtual EScriptComponentInstanceFlags GetFlags() const override;
		virtual IPropertiesConstPtr GetProperties() const override;
		// ~IScriptComponentInstance

	private:

		void ApplyComponent();
		void PopulateSerializationContext(Serialization::IArchive& archive);

		SGUID                         m_guid;
		SGUID                         m_scopeGUID;
		string                        m_name;
		SGUID                         m_componentGUID;
		EScriptComponentInstanceFlags m_flags;
		IPropertiesPtr                m_pProperties;
	};

	DECLARE_SHARED_POINTERS(CScriptComponentInstance)
}
