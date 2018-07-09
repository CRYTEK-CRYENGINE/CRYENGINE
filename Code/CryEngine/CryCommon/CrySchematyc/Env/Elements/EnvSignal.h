// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Env/EnvElementBase.h"
#include "CrySchematyc/Env/Elements/IEnvSignal.h"
#include "CrySchematyc/Reflection/TypeDesc.h"

#define SCHEMATYC_MAKE_ENV_SIGNAL(type) Schematyc::EnvSignal::MakeShared<type>(SCHEMATYC_SOURCE_FILE_INFO)

namespace Schematyc
{

class CEnvSignal : public CEnvElementBase<IEnvSignal>
{
public:

	inline CEnvSignal(const CClassDesc& desc, const SSourceFileInfo& sourceFileInfo)
		: CEnvElementBase(sourceFileInfo)
		, m_desc(desc)
	{
		CEnvElementBase::SetGUID(desc.GetGUID());
		CEnvElementBase::SetName(desc.GetLabel());
		CEnvElementBase::SetDescription(desc.GetDescription());
	}

	// IEnvElement

	virtual bool IsValidScope(IEnvElement& scope) const override
	{
		switch (scope.GetType())
		{
		case EEnvElementType::Root:
		case EEnvElementType::Module:
		case EEnvElementType::Component:
		case EEnvElementType::Action:
			{
				return true;
			}
		default:
			{
				return false;
			}
		}
	}

	// ~IEnvElement

	// IEnvSignal

	virtual const CClassDesc& GetDesc() const override
	{
		return m_desc;
	}

	// ~IEnvSignal

private:

	const CClassDesc& m_desc;
};

namespace EnvSignal
{

template<typename TYPE> inline std::shared_ptr<CEnvSignal> MakeShared(const SSourceFileInfo& sourceFileInfo)
{
	static_assert(std::is_default_constructible<TYPE>::value, "Type must be default constructible!");
	static_assert(std::is_class<TYPE>::value, "Type must be struct/class!");
	SCHEMATYC_VERIFY_TYPE_IS_REFLECTED(TYPE);

	return std::make_shared<CEnvSignal>(GetTypeDesc<TYPE>(), sourceFileInfo);
}

} // EnvSignal
} // Schematyc
