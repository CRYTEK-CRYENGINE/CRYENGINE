// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Script/Elements/IScriptFunction.h>

#include "Script/ScriptElementBase.h"
#include "Script/ScriptUserDocumentation.h"
#include "Script/ScriptVariableDeclaration.h"

namespace Schematyc2
{
	class CScriptFunction : public CScriptElementBase<IScriptFunction>
	{
	private:

		struct SInput
		{
			void Serialize(Serialization::IArchive& archive);

			SGUID                      guid;
			CScriptVariableDeclaration declaration;
		};

		typedef std::vector<SInput> Inputs;

		struct SOutput
		{
			void Serialize(Serialization::IArchive& archive);

			SGUID                      guid;
			CScriptVariableDeclaration declaration;
		};

		typedef std::vector<SOutput> Outputs;

	public:

		CScriptFunction(IScriptFile& file);
		CScriptFunction(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName);

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

		SGUID                    m_guid;
		SGUID                    m_scopeGUID;
		string                   m_name;
		Inputs                   m_inputs;
		Outputs                  m_outputs;
		SScriptUserDocumentation m_userDocumentation;
	};

	DECLARE_SHARED_POINTERS(CScriptFunction)
}
