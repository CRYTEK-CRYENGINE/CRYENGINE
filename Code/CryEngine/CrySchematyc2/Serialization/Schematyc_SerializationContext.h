// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/Schematyc_IAny.h>
#include <CrySchematyc2/Serialization/Schematyc_ISerializationContext.h>

namespace Schematyc2
{
	class CSerializationContext : public ISerializationContext
	{
	public:

		CSerializationContext(const SSerializationContextParams& params);

		// ISerializationContext
		virtual IScriptFile* GetScriptFile() const override;
		virtual ESerializationPass GetPass() const override;
		virtual void SetValidatorLink(const SValidatorLink& validatorLink) override;
		virtual const SValidatorLink& GetValidatorLink() const override;
		virtual void AddDependency(const void* pDependency, const EnvTypeId& typeId) override;
		// ~ISerializationContext

	protected:

		virtual const void* GetDependency_Protected(const EnvTypeId& typeId) const override;

	private:

		typedef std::unordered_map<EnvTypeId, const void*> DependencyMap;

		Serialization::SContext m_context;
		IScriptFile*            m_pScriptFile;
		ESerializationPass      m_pass;
		SValidatorLink          m_validatorLink;
		DependencyMap           m_dependencies;
	};
}