// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Replace IEnvElementPtr with std::unique_ptr?

#pragma once

#include "CrySchematyc/Env/IEnvElement.h"
#include "CrySchematyc/Utils/Assert.h"
#include "CrySchematyc/Utils/GUID.h"

namespace Schematyc
{

// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(IEnvElement)

struct IEnvElementRegistrar
{
	virtual ~IEnvElementRegistrar() {}

	virtual void Register(const IEnvElementPtr& pElement, const CryGUID& scopeGUID) = 0;
};

class CEnvRegistrationScope
{
public:

	inline CEnvRegistrationScope(IEnvElementRegistrar& elementRegistrar, const CryGUID& scopeGUID)
		: m_elementRegistrar(elementRegistrar)
		, m_scopeGUID(scopeGUID)
	{}

	inline CEnvRegistrationScope Register(const IEnvElementPtr& pElement)
	{
		SCHEMATYC_CORE_ASSERT(pElement);
		if (pElement)
		{
			m_elementRegistrar.Register(pElement, m_scopeGUID);
			return CEnvRegistrationScope(m_elementRegistrar, pElement->GetGUID());
		}
		return *this;
	}

private:

	IEnvElementRegistrar& m_elementRegistrar;
	CryGUID                 m_scopeGUID;
};

struct IEnvRegistrar
{
	virtual ~IEnvRegistrar() {}

	virtual CEnvRegistrationScope RootScope() = 0;
	virtual CEnvRegistrationScope Scope(const CryGUID& scopeGUID) = 0;
};

} // Schematyc
