// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryType/Type.h>
#include <CryReflection/IDescExtension.h>

#include <CryExtension/CryGUID.h>

#include <vector>

namespace Cry {
namespace Reflection {

// TODO: Probably make 'const CDescExtension*' a shared pointer.
typedef std::vector<const CDescExtension*> ExtensionsByTypeId;
// ~TODO

template<typename INTERFACE_TYPE>
class CExtensibleDesc : public INTERFACE_TYPE
{
public:
	CExtensibleDesc() {}
	~CExtensibleDesc() {}

	// IExtensibleDesc
	bool                  AddExtension(const CDescExtension* pExtension) const;
	const CDescExtension* FindExtensionByTypeId(CryTypeId typeId) const;
	// ~IExtensibleDesc

protected:
	// Note: We always allow to add extensions even though it's a const object.
	mutable ExtensionsByTypeId m_extensions;
};

template<typename INTERFACE_TYPE>
inline bool CExtensibleDesc<INTERFACE_TYPE >::AddExtension(const CDescExtension* pExtension) const
{
	const CryTypeId typeId = pExtension->GetTypeId();
	auto condition = [typeId](const CDescExtension* pExtension) -> bool
	{
		return pExtension->GetTypeId() == typeId;
	};

	const auto result = std::find_if(m_extensions.begin(), m_extensions.end(), condition);
	CRY_ASSERT_MESSAGE(result == m_extensions.end(), "Extension of type '%s' is already registered.", pExtension->GetLabel());

	if (result == m_extensions.end())
	{
		m_extensions.emplace_back(pExtension);
		return true;
	}
	return false;
}

template<typename INTERFACE_TYPE>
inline const CDescExtension* CExtensibleDesc<INTERFACE_TYPE >::FindExtensionByTypeId(CryTypeId typeId) const
{
	auto condition = [typeId](const CDescExtension* pExtension) -> bool
	{
		return pExtension->GetTypeId() == typeId;
	};

	auto result = std::find_if(m_extensions.begin(), m_extensions.end(), condition);
	if (result != m_extensions.end())
	{
		return *result;
	}

	return nullptr;
}

} // ~Reflection namespace
} // ~Cry namespace
