// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/Elements/IScriptModule.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc2
{
	class CScriptModule : public CScriptElementBase<IScriptModule>
	{
	public:

		CScriptModule(IScriptFile& file);
		CScriptModule(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName);

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

	private:

		SGUID  m_guid;
		SGUID  m_scopeGUID;
		string m_name;
	};

	DECLARE_SHARED_POINTERS(CScriptModule)
}
