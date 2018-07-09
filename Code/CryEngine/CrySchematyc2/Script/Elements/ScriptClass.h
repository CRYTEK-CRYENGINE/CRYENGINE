// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/IFoundation.h>
#include <CrySchematyc2/Script/IScriptFile.h>

#include "Script/ScriptElementBase.h"
#include "Script/ScriptUserDocumentation.h"

namespace Schematyc2
{
	class CScriptClass : public CScriptElementBase<IScriptClass>
	{
	public:

		// #SchematycTODO : Create two separate constructors: one default (before loading) and one for when element is created in editor.
		CScriptClass(IScriptFile& file, const SGUID& guid = SGUID(), const SGUID& scopeGUID = SGUID(), const char* szName = nullptr, const SGUID& foundationGUID = SGUID());

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

		// IScriptClass
		virtual const char* GetAuthor() const override;
		virtual const char* GetDescription() const override;
		virtual SGUID GetFoundationGUID() const override;
		virtual IPropertiesConstPtr GetFoundationProperties() const override;
		// ~IScriptClass

	private:

		void RefreshFoundationProperties();
		void RefreshFoundationComponents();
		void Validate(Serialization::IArchive& archive);

		SGUID                    m_guid;
		SGUID                    m_scopeGUID;
		string                   m_name;
		SScriptUserDocumentation m_userDocumentation;
		SGUID                    m_foundationGUID;
		IPropertiesPtr           m_pFoundationProperties;
	};

	DECLARE_SHARED_POINTERS(CScriptClass)
}
