// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ICOMPRESSIONPOLICY2_H__
#define __ICOMPRESSIONPOLICY2_H__

#pragma once

#include "ICompressionPolicy.h"

template<class T>
class CCompressionPolicy2 : public ICompressionPolicy
{
public:
#if USE_MEMENTO_PREDICTORS
	typedef typename T::TMemento TMemento;
#endif
	CCompressionPolicy2(uint32 key) : ICompressionPolicy(key) {}
	CCompressionPolicy2(uint32 key, const T& impl) : ICompressionPolicy(key), m_impl(impl) {}

	virtual bool Load(XmlNodeRef node, const string& filename)
	{
		return m_impl.Load(node, filename);
	}

#if USE_ARITHSTREAM
	#define SERIALIZATION_TYPE(T)                                                                                                                                              \
	  virtual bool ReadValue(CCommInputStream & in, T & value, CArithModel * pModel, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const  \
	  {                                                                                                                                                                        \
	    TMemento memento;                                                                                                                                                      \
	    Setup(memento, pCurState);                                                                                                                                             \
	    if (!m_impl.ReadValue(memento, in, pModel, age, value))                                                                                                                \
	      return false;                                                                                                                                                        \
	    return Complete(memento, value, pNewState);                                                                                                                            \
	  }                                                                                                                                                                        \
	  virtual bool WriteValue(CCommOutputStream & out, T value, CArithModel * pModel, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const \
	  {                                                                                                                                                                        \
	    TMemento memento;                                                                                                                                                      \
	    Setup(memento, pCurState);                                                                                                                                             \
	    if (!m_impl.WriteValue(memento, out, pModel, age, value))                                                                                                              \
	      return false;                                                                                                                                                        \
	    return Complete(memento, value, pNewState);                                                                                                                            \
	  }
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE

	virtual bool ReadValue(CCommInputStream& in, SSerializeString& value, CArithModel* pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		TMemento memento;
		Setup(memento, pCurState);
		if (!m_impl.ReadValue(memento, in, pModel, age, value))
			return false;
		return Complete(memento, value, pNewState);
	}
	virtual bool WriteValue(CCommOutputStream& out, const SSerializeString& value, CArithModel* pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		TMemento memento;
		Setup(memento, pCurState);
		if (!m_impl.WriteValue(memento, out, pModel, age, value))
			return false;
		return Complete(memento, value, pNewState);
	}
#else
	// !USE_ARITHSTREAM
	#if USE_MEMENTO_PREDICTORS

		#define SERIALIZATION_TYPE(T)                                                                                                                              \
		  virtual bool ReadValue(CNetInputSerializeImpl * in, T & value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const  \
		  {                                                                                                                                                        \
		    TMemento memento;                                                                                                                                      \
		    Setup(memento, pCurState);                                                                                                                             \
		    if (m_impl.ReadValue(memento, in, age, value))                                                                                                         \
		    {                                                                                                                                                      \
		      return Complete(memento, value, pNewState);                                                                                                          \
		    }                                                                                                                                                      \
		    return false;                                                                                                                                          \
		  }                                                                                                                                                        \
		  virtual bool WriteValue(CNetOutputSerializeImpl * out, T value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const \
		  {                                                                                                                                                        \
		    TMemento memento;                                                                                                                                      \
		    Setup(memento, pCurState);                                                                                                                             \
		    if (m_impl.WriteValue(memento, out, age, value))                                                                                                       \
		    {                                                                                                                                                      \
		      return Complete(memento, value, pNewState);                                                                                                          \
		    }                                                                                                                                                      \
		    return false;                                                                                                                                          \
		  }
		#include <CryNetwork/SerializationTypes.h>
		#undef SERIALIZATION_TYPE

	virtual bool ReadValue(CNetInputSerializeImpl* in, SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		TMemento memento;
		Setup(memento, pCurState);
		if (m_impl.ReadValue(memento, in, age, value))
		{
			return Complete(memento, value, pNewState);
		}
		return false;
	}
	virtual bool WriteValue(CNetOutputSerializeImpl* out, const SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		TMemento memento;
		Setup(memento, pCurState);
		if (m_impl.WriteValue(memento, out, age, value))
		{
			return Complete(memento, value, pNewState);
		}
		return false;
	}

	#else

		#define SERIALIZATION_TYPE(T)                                                                                                                              \
		  virtual bool ReadValue(CNetInputSerializeImpl * in, T & value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const  \
		  {                                                                                                                                                        \
		    return m_impl.ReadValue(in, age, value);                                                                                                               \
		  }                                                                                                                                                        \
		  virtual bool WriteValue(CNetOutputSerializeImpl * out, T value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const \
		  {                                                                                                                                                        \
		    return m_impl.WriteValue(out, age, value);                                                                                                             \
		  }
		#include <CryNetwork/SerializationTypes.h>
		#undef SERIALIZATION_TYPE

	virtual bool ReadValue(CNetInputSerializeImpl* in, SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		return m_impl.ReadValue(in, age, value);
	}
	virtual bool WriteValue(CNetOutputSerializeImpl* out, const SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		return m_impl.WriteValue(out, age, value);
	}

	#endif

#endif

	virtual void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		m_impl.GetMemoryStatistics(pSizer);
	}

#if NET_PROFILE_ENABLE
	#define SERIALIZATION_TYPE(T)         \
	  virtual int GetBitCount(T value)    \
	  {                                   \
	    return m_impl.GetBitCount(value); \
	  }
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE

	virtual int GetBitCount(SSerializeString& value)
	{
		return m_impl.GetBitCount(value);
	}
#endif

private:
#if USE_MEMENTO_PREDICTORS
	ILINE void Setup(TMemento& memento, CByteInputStream* pCurState) const
	{
		if (pCurState)
		{
	#if VERIFY_MEMENTO_BUFFERS
			uint32 tag = pCurState->GetTyped<uint32>();
			NET_ASSERT(tag == 0x12345678);
	#endif
			m_impl.ReadMemento(memento, *pCurState);
	#if VERIFY_MEMENTO_BUFFERS
			tag = pCurState->GetTyped<uint32>();
			NET_ASSERT(tag == 0x87654321);
	#endif
		}
		else
			m_impl.InitMemento(memento);
	}
	template<class U>
	ILINE bool Complete(TMemento& memento, const U& newValue, CByteOutputStream* pNewState) const
	{
		if (pNewState)
		{
	#if VERIFY_MEMENTO_BUFFERS
			pNewState->PutTyped<uint32>() = 0x12345678;
	#endif
			if (!m_impl.UpdateMemento(memento, newValue))
				return false;
			m_impl.WriteMemento(const_cast<const TMemento&>(memento), *pNewState);
	#if VERIFY_MEMENTO_BUFFERS
			pNewState->PutTyped<uint32>() = 0x87654321;
	#endif
		}
		return true;
	}
#endif

	T m_impl;
};

template<class T>
struct CompressionPolicyFactory2 : public CompressionPolicyFactoryBase
{
	CompressionPolicyFactory2(string name)
	{
		CCompressionRegistry::Get()->RegisterPolicy(name, Create);
	}

	static ICompressionPolicyPtr Create(uint32 key)
	{
		return new CCompressionPolicy2<T>(key);
	}
};

#define REGISTER_COMPRESSION_POLICY2(cls, name)                \
  static CompressionPolicyFactory2<cls> cls ## _Factory(name); \
  CompressionPolicyFactoryBase* cls ## _FactoryPtr = &cls ## _Factory;

#endif
