// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc2
{
	struct IAbstractInterface;

	class CScriptAbstractInterfaceImplementation : public CScriptElementBase<IScriptAbstractInterfaceImplementation>
	{
	public:

		// #SchematycTODO : Create two separate constructors: one default (before loading) and one for when element is created in editor.
		CScriptAbstractInterfaceImplementation(IScriptFile& file, const SGUID& guid = SGUID(), const SGUID& scopeGUID = SGUID(), EDomain domain = EDomain::Unknown, const SGUID& refGUID = SGUID());

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

		// IScriptAbstractInterfaceImplementation
		virtual EDomain GetDomain() const override;
		virtual SGUID GetRefGUID() const override;
		// ~IScriptAbstractInterfaceImplementation

	private:

		void RefreshEnvAbstractrInterfaceFunctions(const IAbstractInterface& abstractInterface);
		void RefreshScriptAbstractrInterfaceFunctions(const IScriptFile& abstractrInterfaceFile);
		void RefreshScriptAbstractrInterfaceTasks(const IScriptFile& abstractrInterfaceFile);
		void RefreshScriptAbstractrInterfaceTaskPropertiess(const IScriptFile& abstractrInterfaceFile, const SGUID& taskGUID);
		void Validate(Serialization::IArchive& archive);

	private:

		SGUID   m_guid;
		SGUID   m_scopeGUID;
		string  m_name;
		EDomain m_domain;
		SGUID   m_refGUID;
	};

	DECLARE_SHARED_POINTERS(CScriptAbstractInterfaceImplementation)
}
