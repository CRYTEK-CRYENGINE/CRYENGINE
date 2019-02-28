// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICompressionPolicy.h"

template <class T>
class CCompressionPolicyTime :
	public CCompressionPolicy<T>
{
public:
	CCompressionPolicyTime(uint32 key) : CCompressionPolicy<T>(key)
	{

	}

	virtual void SetTimeValue(uint32 timeFraction32)
	{
		CCompressionPolicy<T>::m_impl.SetTimeValue(timeFraction32);
	}

	virtual void Init(CCompressionManager* pManager)
	{
		CCompressionPolicy<T>::m_impl.Init(pManager);
	}

	virtual bool Manage(CCompressionManager* pManager)
	{
		return CCompressionPolicy<T>::m_impl.Manage(pManager);
	}
};


template <class T>
struct CompressionPolicyTimeFactory :public CompressionPolicyFactoryBase
{
	CompressionPolicyTimeFactory(string name)
	{
		CCompressionRegistry::Get()->RegisterPolicy(name, Create);
	}

	static ICompressionPolicyPtr Create(uint32 key)
	{
		return new CCompressionPolicyTime<T>(key);
	}
};

#define REGISTER_COMPRESSION_POLICY_TIME(cls, name)						\
	extern void RegisterCompressionPolicy_ ## cls()					\
	{																\
		static CompressionPolicyTimeFactory<cls> cls ## _Factory(name);	\
	}																\

