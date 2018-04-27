// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

#include "Script/ScriptElementBase.h"
#include "Script/ScriptUserDocumentation.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	class CScriptEnumeration : public CScriptElementBase<IScriptEnumeration>
	{
	public:

		// #SchematycTODO : Create two separate constructors: one default (before loading) and one for when element is created in editor.
		CScriptEnumeration(IScriptFile& file, const SGUID& guid = SGUID(), const SGUID& scopeGUID = SGUID(), const char* szName = nullptr);

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

		// IScriptEnumeration
		virtual CAggregateTypeId GetTypeId() const override;
		virtual size_t GetConstantCount() const override;
		virtual size_t FindConstant(const char* szConstant) const override;
		virtual const char* GetConstant(size_t constantIdx) const override;
		// ~IScriptEnumeration

	private:

		typedef std::vector<string> Constants;

		SGUID                    m_guid;
		SGUID                    m_scopeGUID;
		string                   m_name;
		Constants                m_constants;
		SScriptUserDocumentation m_userDocumentation;
	};

	DECLARE_SHARED_POINTERS(CScriptEnumeration)
}
