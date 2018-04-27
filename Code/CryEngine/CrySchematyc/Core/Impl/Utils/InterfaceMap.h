// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/VectorMap.h>
#include <CrySchematyc/Utils/IInterfaceMap.h>

namespace Schematyc
{
template<typename BASE_INTERFACE> class CInterfaceMap : public IInterfaceMap<BASE_INTERFACE>
{
private:

	typedef VectorMap<CTypeName, BASE_INTERFACE*> Interfaces;

private:

	// IInterfaceMap

	virtual bool Add(const CTypeName& typeName, BASE_INTERFACE* pInterface) override
	{
		auto pair = std::pair<CTypeName, BASE_INTERFACE*>(typeName, pInterface);
		return m_interfaces.insert(pair).second;
	}

	virtual BASE_INTERFACE* Query(const CTypeName& typeName) const override
	{
		auto itInterface = m_interfaces.find(typeName);
		return itInterface != m_interfaces.end() ? itInterface->second : nullptr;
	}

	// ~IInterfaceMap

private:

	Interfaces m_interfaces;
};
} // Schematyc
