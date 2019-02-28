// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryType/Type.h>

namespace Cry {
namespace Reflection {

class CDescExtension
{
public:
	CDescExtension(const char* szLabel, CryTypeId typeId)
		: m_szLabel(szLabel)
		, m_typeId(typeId)
	{}

	CDescExtension(CDescExtension&& desc)
		: m_szLabel(desc.m_szLabel)
		, m_typeId(desc.m_typeId)
	{}

	CryTypeId   GetTypeId() const { return m_typeId; }
	const char* GetLabel() const  { return m_szLabel; }

private:
	const char*     m_szLabel;
	const CryTypeId m_typeId;
};

struct IExtensibleDesc
{
	virtual ~IExtensibleDesc() {}

	virtual bool                  AddExtension(const CDescExtension* pExtension) const = 0;
	virtual const CDescExtension* FindExtensionByTypeId(CryTypeId typeId) const = 0;

	template<typename EXTENSION_TYPE>
	const EXTENSION_TYPE* FindExtension() const
	{
		static_assert(std::is_base_of<CDescExtension, typename Type::PureType<EXTENSION_TYPE>::Type>::value, "Type doesn't derive from 'Cry::Reflection::CDescExtension'.");
		return static_cast<const EXTENSION_TYPE*>(FindExtensionByTypeId(TypeIdOf<EXTENSION_TYPE>()));
	}
};

} // ~Reflection namespace
} // ~Cry namespace
