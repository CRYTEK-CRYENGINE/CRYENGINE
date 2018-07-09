// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IReflection.h"
#include "TypeRegistrar.h"

namespace Cry {
namespace Reflection {

template<typename TYPE>
struct CObject : public IObject
{
	virtual ~CObject() {}

	// IObject
	virtual bool             IsReflected() const override final  { return s_registration.IsRegistered(); }
	virtual CryTypeId        GetTypeId() const override final    { return s_registration.GetTypeId(); }
	virtual TypeIndex        GetTypeIndex() const override final { return s_registration.GetTypeIndex(); }
	virtual const ITypeDesc* GetTypeDesc() const override final  { return s_registration.GetTypeDesc(); }

	virtual bool             IsEqualType(const IObject* pObject) const override final
	{
		CRY_ASSERT_MESSAGE(IsReflected() && pObject->IsReflected(), "Object is not reflected.");
		return IsReflected() && pObject->IsReflected() && pObject->GetTypeId() == GetTypeId();
	}

	virtual bool IsEqualObject(const IObject* pObject) const override final
	{
		if (IsEqualType(pObject))
		{
			Type::CEqualOperator isEqual = GetTypeDesc()->GetEqualOperator();
			if (isEqual)
			{
				return isEqual(this, pObject);
			}
			CRY_ASSERT_MESSAGE(isEqual, "Reflected object can't be equal compared.");
		}
		return false;
	}
	// ~IObject

	static const ITypeDesc* Type() { return s_registration.GetTypeDesc(); }

private:
	static CTypeRegistrar s_registration;
};

#define CRY_REFLECTION_REGISTER_OBJECT(type, guid)                                                                                                       \
  namespace Cry {                                                                                                                                        \
  namespace Reflection {                                                                                                                                 \
  template<>                                                                                                                                             \
  CTypeRegistrar CObject<type>::s_registration = CTypeRegistrar(Cry::TypeDescOf<type>(), guid, &type::ReflectType, SSourceFileInfo(__FILE__, __LINE__)); \
  }                                                                                                                                                      \
  }

#define CRY_REFLECTION_REGISTER_TYPE(type, guid, reflectFunc)                                                                                     \
  namespace Cry {                                                                                                                                 \
  namespace Reflection {                                                                                                                          \
  template<>                                                                                                                                      \
  CTypeRegistrar CObject<type>::s_registration = CTypeRegistrar(Cry::TypeDescOf<type>(), guid, reflectFunc, SSourceFileInfo(__FILE__, __LINE__)); \
  }                                                                                                                                               \
  }

} // ~Reflection namespace
} // ~Cry namespace

inline bool operator==(const Cry::Reflection::IObject& lh, CryTypeId rh)
{
	return lh.GetTypeId() == rh;
}

inline bool operator!=(const Cry::Reflection::IObject& lh, CryTypeId rh)
{
	return lh.GetTypeId() != rh;
}

inline bool operator==(const Cry::Reflection::IObject& lh, const CryTypeDesc& rh)
{
	return lh.GetTypeId() == rh.GetTypeId();
}

inline bool operator!=(const Cry::Reflection::IObject& lh, const CryTypeDesc& rh)
{
	return lh.GetTypeId() != rh.GetTypeId();
}
