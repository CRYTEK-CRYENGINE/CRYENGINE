// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc2
{
	class CScriptActionInstance : public CScriptElementBase<IScriptActionInstance>
	{
	public:

		// #SchematycTODO : Create two separate constructors: one default (before loading) and one for when element is created in editor.
		CScriptActionInstance(IScriptFile& file, const SGUID& guid = SGUID(), const SGUID& scopeGUID = SGUID(), const char* szName = nullptr, const SGUID& actionGUID = SGUID(), const SGUID& componentInstanceGUID = SGUID());

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

		// IScriptActionInstance
		virtual SGUID GetActionGUID() const override;
		virtual SGUID GetComponentInstanceGUID() const override;
		virtual IPropertiesConstPtr GetProperties() const override;
		// ~IScriptActionInstance

	private:

		void RefreshAction();
		void SerializeProperties(Serialization::IArchive& archive);

		SGUID          m_guid;
		SGUID          m_scopeGUID;
		string         m_name;
		SGUID          m_actionGUID;
		SGUID          m_componentInstanceGUID;
		IPropertiesPtr m_pProperties;
	};

	DECLARE_SHARED_POINTERS(CScriptActionInstance)
}
