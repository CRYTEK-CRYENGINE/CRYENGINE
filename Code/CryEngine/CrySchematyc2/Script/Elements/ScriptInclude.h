// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

#include "Script/ScriptElementBase.h"
#include "Script/ScriptUserDocumentation.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	class CScriptInclude : public CScriptElementBase<IScriptInclude>
	{
	public:

		// #SchematycTODO : Create two separate constructors: one default (before loading) and one for when element is created in editor.
		CScriptInclude(IScriptFile& file, const SGUID& guid = SGUID(), const SGUID& scopeGUID = SGUID(), const char* szFileName = nullptr, const SGUID& refGUID = SGUID());

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

		// IScriptInclude
		virtual const char* GetFileName() const override;
		virtual SGUID GetRefGUID() const override;
		// ~IScriptInclude

	private:

		SGUID  m_guid;
		SGUID  m_scopeGUID;
		string m_fileName;
		SGUID  m_refGUID;
		// #SchematycTODO : Store pointer to references file?
	};

	DECLARE_SHARED_POINTERS(CScriptInclude)
}
