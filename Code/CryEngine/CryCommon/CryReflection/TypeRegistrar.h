// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IReflection.h"
#include "ITypeDesc.h"

namespace Cry {
namespace Reflection {

class CTypeRegistrar
{
public:
	CTypeRegistrar(const CryTypeDesc& typeDesc, const CryGUID& guid, ReflectTypeFunction pReflectFunc, const SSourceFileInfo& srcPos)
		: m_guid(guid)
		, m_pReflectFunc(pReflectFunc)
		, m_typeDesc(typeDesc)
		, m_srcPos(srcPos)
	{
		CTypeRegistrar*& pFirst = GetFirst();
		m_pNextRegistration = pFirst;
		pFirst = this;
	}

	~CTypeRegistrar()
	{
		// TODO: Proper unregister!
	}

	static bool RegisterTypes()
	{
		CTypeRegistrar* pRegistrar = GetFirst();
		while (pRegistrar)
		{
			pRegistrar->Register();
			pRegistrar = pRegistrar->GetNext();
		}
		return true;
	}

	bool      IsRegistered() const { return m_typeIndex.IsValid(); }
	TypeIndex GetTypeIndex() const { return m_typeIndex; }

	CryTypeId GetTypeId() const
	{
		const ITypeDesc* pTypeDesc = GetTypeDesc();
		CRY_ASSERT_MESSAGE(pTypeDesc, "Object is not reflected.");
		return pTypeDesc ? pTypeDesc->GetTypeId() : CryTypeId();
	}

	const ITypeDesc* GetTypeDesc() const
	{
		const ITypeDesc* pTypeDesc = GetReflectionRegistry().FindTypeByIndex(m_typeIndex);
		CRY_ASSERT_MESSAGE(pTypeDesc, "Object is not reflected.");
		return pTypeDesc;
	}

	CTypeRegistrar* GetNext() const { return m_pNextRegistration; }

private:
	void Register()
	{
		ITypeDesc* pTypeDesc = GetReflectionRegistry().Register(m_typeDesc, m_guid, m_pReflectFunc, m_srcPos);
		if (pTypeDesc)
		{
			m_typeIndex = pTypeDesc->GetIndex();
		}
	}

	static CTypeRegistrar*& GetFirst()
	{
		static CTypeRegistrar* pFirst = nullptr;
		return pFirst;
	}

private:
	CTypeRegistrar*     m_pNextRegistration;

	CryGUID             m_guid;
	const CryTypeDesc&  m_typeDesc;
	ReflectTypeFunction m_pReflectFunc;
	SSourceFileInfo     m_srcPos;
	TypeIndex           m_typeIndex;
};

} // ~Reflection namespace
} // ~Cry namespace
