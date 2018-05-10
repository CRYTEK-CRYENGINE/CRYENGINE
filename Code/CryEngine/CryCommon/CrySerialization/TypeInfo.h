// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

struct STypeInfoInstance
{
	template<class T>
	STypeInfoInstance(T& obj)
		: m_pTypeInfo(&TypeInfo(&obj))
		, m_pObject(&obj)
	{
	}

	STypeInfoInstance(const CTypeInfo* typeInfo, void* object)
		: m_pTypeInfo(typeInfo)
		, m_pObject(object)
	{
	}

	inline void Serialize(Serialization::IArchive& ar);

	const CTypeInfo* m_pTypeInfo;
	void*            m_pObject;
	std::set<string> m_persistentStrings;
};

#include "TypeInfoImpl.h"
