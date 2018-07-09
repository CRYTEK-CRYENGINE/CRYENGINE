// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ICOMPRESSIONPOLICY_H__
#define __ICOMPRESSIONPOLICY_H__

#pragma once

#include "CompressionManager.h"
#include "Streams/ByteStream.h"
#include "Streams/CommStream.h"

class CArithModel;

struct ICompressionPolicy : public CMultiThreadRefCount
{
	const uint32 key;

	ICompressionPolicy(uint32 k) : key(k)
	{
		++g_objcnt.compressionPolicy;
	}

	virtual ~ICompressionPolicy()
	{
		--g_objcnt.compressionPolicy;
	}

	virtual void Init(CCompressionManager* pManager) {}
	virtual bool Load(XmlNodeRef node, const string& filename) = 0;

	virtual void SetTimeValue(uint32 timeFraction32){}

	// when policy needs to process some data from time to time
	// if it return false, this method will never be called again
	// this method is called from different thread than replication and should be thread safe
	virtual bool Manage(CCompressionManager* pManager) { return false; }


#if USE_ARITHSTREAM
	#define SERIALIZATION_TYPE(T)                                                                                                                                                  \
	  virtual bool ReadValue(CCommInputStream & in, T & value, CArithModel * pModel, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const = 0; \
	  virtual bool WriteValue(CCommOutputStream & out, T value, CArithModel * pModel, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const = 0;
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE
	virtual bool ReadValue(CCommInputStream& in, SSerializeString& value, CArithModel* pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const = 0;
	virtual bool WriteValue(CCommOutputStream& out, const SSerializeString& value, CArithModel* pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const = 0;
#else
	#define SERIALIZATION_TYPE(T)                                                                                                                                  \
	  virtual bool ReadValue(CNetInputSerializeImpl * in, T & value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const = 0; \
	  virtual bool WriteValue(CNetOutputSerializeImpl * out, T value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const = 0;
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE
	virtual bool ReadValue(CNetInputSerializeImpl* in, SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const = 0;
	virtual bool WriteValue(CNetOutputSerializeImpl* out, const SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const = 0;
#endif

	virtual void GetMemoryStatistics(ICrySizer* pSizer) const = 0;

#if NET_PROFILE_ENABLE
	#define SERIALIZATION_TYPE(T) \
	  virtual int GetBitCount(T value) = 0;
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE
	virtual int GetBitCount(SSerializeString& value) = 0;
#endif
};

typedef ICompressionPolicy* ICompressionPolicyPtr;

template<class T>
class CCompressionPolicy : public ICompressionPolicy
{
public:
	CCompressionPolicy(uint32 key) : ICompressionPolicy(key), m_impl() {}
	CCompressionPolicy(uint32 key, const T& impl) : ICompressionPolicy(key), m_impl(impl) {}

	virtual bool Load(XmlNodeRef node, const string& filename)
	{
		return m_impl.Load(node, filename);
	}

#if USE_ARITHSTREAM
	#define SERIALIZATION_TYPE(T)                                                                                                                                              \
	  virtual bool ReadValue(CCommInputStream & in, T & value, CArithModel * pModel, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const  \
	  {                                                                                                                                                                        \
	    Setup(pCurState);                                                                                                                                                      \
	    if (!m_impl.ReadValue(in, value, pModel, age))                                                                                                                         \
	      return false;                                                                                                                                                        \
	    Complete(pNewState);                                                                                                                                                   \
	    return true;                                                                                                                                                           \
	  }                                                                                                                                                                        \
	  virtual bool WriteValue(CCommOutputStream & out, T value, CArithModel * pModel, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const \
	  {                                                                                                                                                                        \
	    Setup(pCurState);                                                                                                                                                      \
	    if (!m_impl.WriteValue(out, value, pModel, age))                                                                                                                       \
	      return false;                                                                                                                                                        \
	    Complete(pNewState);                                                                                                                                                   \
	    return true;                                                                                                                                                           \
	  }
	#include <CryNetwork/SerializationTypes.h>
	#undef SERIALIZATION_TYPE

	virtual bool ReadValue(CCommInputStream& in, SSerializeString& value, CArithModel* pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		Setup(pCurState);
		if (!m_impl.ReadValue(in, value, pModel, age))
			return false;
		Complete(pNewState);
		return true;
	}
	virtual bool WriteValue(CCommOutputStream& out, const SSerializeString& value, CArithModel* pModel, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		Setup(pCurState);
		if (!m_impl.WriteValue(out, value, pModel, age))
			return false;
		Complete(pNewState);
		return true;
	}
#else
	// !USE_ARITHSTREAM

	#if USE_MEMENTO_PREDICTORS

		#define SERIALIZATION_TYPE(T)                                                                                                                              \
		  virtual bool ReadValue(CNetInputSerializeImpl * in, T & value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const  \
		  {                                                                                                                                                        \
		    Setup(pCurState);                                                                                                                                      \
		    if (m_impl.ReadValue(in, value, age))                                                                                                                  \
		    {                                                                                                                                                      \
		      Complete(pNewState);                                                                                                                                 \
		      return true;                                                                                                                                         \
		    }                                                                                                                                                      \
		    return false;                                                                                                                                          \
		  }                                                                                                                                                        \
		  virtual bool WriteValue(CNetOutputSerializeImpl * out, T value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const \
		  {                                                                                                                                                        \
		    Setup(pCurState);                                                                                                                                      \
		    if (m_impl.WriteValue(out, value, age))                                                                                                                \
		    {                                                                                                                                                      \
		      Complete(pNewState);                                                                                                                                 \
		      return true;                                                                                                                                         \
		    }                                                                                                                                                      \
		    return false;                                                                                                                                          \
		  }
		#include <CryNetwork/SerializationTypes.h>
		#undef SERIALIZATION_TYPE

	virtual bool ReadValue(CNetInputSerializeImpl* in, SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		Setup(pCurState);
		if (m_impl.ReadValue(in, value, age))
		{
			Complete(pNewState);
			return true;
		}
		return false;
	}
	virtual bool WriteValue(CNetOutputSerializeImpl* out, const SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		Setup(pCurState);
		if (m_impl.WriteValue(out, value, age))
		{
			Complete(pNewState);
			return true;
		}
		return false;
	}
	#else

		#define SERIALIZATION_TYPE(T)                                                                                                                              \
		  virtual bool ReadValue(CNetInputSerializeImpl * in, T & value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const  \
		  {                                                                                                                                                        \
		    return m_impl.ReadValue(in, value, age);                                                                                                               \
		  }                                                                                                                                                        \
		  virtual bool WriteValue(CNetOutputSerializeImpl * out, T value, uint32 age, bool own, CByteInputStream * pCurState, CByteOutputStream * pNewState) const \
		  {                                                                                                                                                        \
		    return m_impl.WriteValue(out, value, age);                                                                                                             \
		  }
		#include <CryNetwork/SerializationTypes.h>
		#undef SERIALIZATION_TYPE

	virtual bool ReadValue(CNetInputSerializeImpl* in, SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		return m_impl.ReadValue(in, value, age);
	}
	virtual bool WriteValue(CNetOutputSerializeImpl* out, const SSerializeString& value, uint32 age, bool own, CByteInputStream* pCurState, CByteOutputStream* pNewState) const
	{
		return m_impl.WriteValue(out, value, age);
	}

	#endif

#endif

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

	virtual void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		m_impl.GetMemoryStatistics(pSizer);
	}

private:

#if USE_MEMENTO_PREDICTORS
	void Setup(CByteInputStream* pCurState) const
	{
		if (pCurState)
		{
	#if VERIFY_MEMENTO_BUFFERS
			uint32 tag = pCurState->GetTyped<uint32>();
			NET_ASSERT(tag == 0x12345678);
	#endif
			m_impl.ReadMemento(*pCurState);
	#if VERIFY_MEMENTO_BUFFERS
			tag = pCurState->GetTyped<uint32>();
			NET_ASSERT(tag == 0x87654321);
	#endif
		}
		else
			m_impl.NoMemento();
	}
	void Complete(CByteOutputStream* pNewState) const
	{
		if (pNewState)
		{
	#if VERIFY_MEMENTO_BUFFERS
			pNewState->PutTyped<uint32>() = 0x12345678;
	#endif
			m_impl.WriteMemento(*pNewState);
	#if VERIFY_MEMENTO_BUFFERS
			pNewState->PutTyped<uint32>() = 0x87654321;
	#endif
		}
	}
#endif

protected:
	T m_impl;
};

struct CompressionPolicyFactoryBase {};

template<class T>
struct CompressionPolicyFactory : public CompressionPolicyFactoryBase
{
	CompressionPolicyFactory(string name)
	{
		CCompressionRegistry::Get()->RegisterPolicy(name, Create);
	}

	static ICompressionPolicyPtr Create(uint32 key)
	{
		return new CCompressionPolicy<T>(key);
	}
};

#define REGISTER_COMPRESSION_POLICY(cls, name)                  \
  extern void RegisterCompressionPolicy_ ## cls()               \
  {                                                             \
    static CompressionPolicyFactory<cls> cls ## _Factory(name); \
  }                                                             \

#endif
