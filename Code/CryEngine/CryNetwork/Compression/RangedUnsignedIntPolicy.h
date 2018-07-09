// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RANGEDUNSIGNEDINTPOLICY_H__
#define __RANGEDUNSIGNEDINTPOLICY_H__

#pragma once

#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "StationaryUnsignedInteger.h"

class CRangedUnsignedIntPolicy
{
public:
	void SetValues(uint32 nMin, uint32 nMax)
	{
		m_integer.SetValues(nMin, nMax);
	}

	bool Load(XmlNodeRef node, const string& filename)
	{
		return m_integer.Load(node, filename, "Range");
	}

#if USE_MEMENTO_PREDICTORS
	bool ReadMemento(CByteInputStream& in) const
	{
		m_integer.ReadMemento(in);
		return true;
	}

	bool WriteMemento(CByteOutputStream& out) const
	{
		m_integer.WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		m_integer.NoMemento();
	}
#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, uint32& value, CArithModel* pModel, uint32 age) const
	{
		value = m_integer.ReadValue(in);
		return true;
	}
	bool WriteValue(CCommOutputStream& out, uint32 value, CArithModel* pModel, uint32 age) const
	{
		m_integer.WriteValue(out, value);
		return true;
	}

	#define DECL_PROXY_UNSIGNED(T)                                                              \
	  bool ReadValue(CCommInputStream & in, T & value, CArithModel * pModel, uint32 age) const  \
	  {                                                                                         \
	    uint32 temp;                                                                            \
	    bool ok = ReadValue(in, temp, pModel, age);                                             \
	    if (ok)                                                                                 \
	    {                                                                                       \
	      value = (T)temp;                                                                      \
	      NET_ASSERT(value == temp);                                                            \
	    }                                                                                       \
	    return ok;                                                                              \
	  }                                                                                         \
	  bool WriteValue(CCommOutputStream & out, T value, CArithModel * pModel, uint32 age) const \
	  {                                                                                         \
	    uint32 temp = (uint32)value;                                                            \
	    NET_ASSERT(temp == value);                                                              \
	    return WriteValue(out, temp, pModel, age);                                              \
	  }

	DECL_PROXY_UNSIGNED(uint8)
	DECL_PROXY_UNSIGNED(uint16)
	DECL_PROXY_UNSIGNED(uint64)
	DECL_PROXY_UNSIGNED(int8)
	DECL_PROXY_UNSIGNED(int16)
	DECL_PROXY_UNSIGNED(int32)
	DECL_PROXY_UNSIGNED(int64)

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("RangedUnsignedIntPolicy: ReadValue not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("RangedUnsignedIntPolicy: WriteValue not implemented for generic types");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, uint32& value, uint32 age) const
	{
		value = m_integer.ReadValue(in);
		return true;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, uint32 value, uint32 age) const
	{
		m_integer.WriteValue(out, value);
		return true;
	}

	#define DECL_PROXY_UNSIGNED(T)                                              \
	  bool ReadValue(CNetInputSerializeImpl * in, T & value, uint32 age) const  \
	  {                                                                         \
	    uint32 temp;                                                            \
	    bool ok = ReadValue(in, temp, age);                                     \
	    if (ok)                                                                 \
	    {                                                                       \
	      value = (T)temp;                                                      \
	      NET_ASSERT(value == temp);                                            \
	    }                                                                       \
	    return ok;                                                              \
	  }                                                                         \
	  bool WriteValue(CNetOutputSerializeImpl * out, T value, uint32 age) const \
	  {                                                                         \
	    uint32 temp = (uint32)value;                                            \
	    NET_ASSERT(temp == value);                                              \
	    return WriteValue(out, temp, age);                                      \
	  }

	DECL_PROXY_UNSIGNED(uint8)
	DECL_PROXY_UNSIGNED(uint16)
	DECL_PROXY_UNSIGNED(uint64)
	DECL_PROXY_UNSIGNED(int8)
	DECL_PROXY_UNSIGNED(int16)
	DECL_PROXY_UNSIGNED(int32)
	DECL_PROXY_UNSIGNED(int64)

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("RangedUnsignedIntPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("RangedUnsignedIntPolicy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "");
		pSizer->Add(*this);
	}

#if NET_PROFILE_ENABLE
	int GetBitCount(uint32 value)
	{
		return m_integer.GetBitCount();
	}

	#define DECL_COUNT_UNSIGNED(T)   \
	  int GetBitCount(T value)       \
	  {                              \
	    uint32 temp = (uint32)value; \
	    return GetBitCount(temp);    \
	  }

	DECL_COUNT_UNSIGNED(uint8)
	DECL_COUNT_UNSIGNED(uint16)
	DECL_COUNT_UNSIGNED(uint64)
	DECL_COUNT_UNSIGNED(int8)
	DECL_COUNT_UNSIGNED(int16)
	DECL_COUNT_UNSIGNED(int32)
	DECL_COUNT_UNSIGNED(int64)

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif

private:
	CStationaryUnsignedInteger m_integer;
};

#endif
