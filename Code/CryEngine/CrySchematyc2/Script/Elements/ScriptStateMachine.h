// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/IScriptFile.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc2
{
	class CScriptStateMachine : public CScriptElementBase<IScriptStateMachine>
	{
	public:

		CScriptStateMachine(IScriptFile& file);
		CScriptStateMachine(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName, EScriptStateMachineLifetime lifetime, const SGUID& contextGUID, const SGUID& partnerGUID);

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

		// IScriptStateMachine
		virtual EScriptStateMachineLifetime GetLifetime() const override;
		virtual SGUID GetContextGUID() const override;
		virtual SGUID GetPartnerGUID() const override;
		// ~IScriptStateMachine

	private:

		void RefreshTransitionGraph();
		void SerializePartner(Serialization::IArchive& archive);

		SGUID                       m_guid;
		SGUID                       m_scopeGUID;
		string                      m_name;
		EScriptStateMachineLifetime m_lifetime;
		SGUID                       m_contextGUID;
		SGUID                       m_partnerGUID;
		SGUID                       m_transitionGraphGUID;
	};

	DECLARE_SHARED_POINTERS(CScriptStateMachine)
}
