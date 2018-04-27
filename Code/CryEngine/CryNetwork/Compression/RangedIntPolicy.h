// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RANGEDINTPOLICY_H__
#define __RANGEDINTPOLICY_H__

#pragma once

#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "StationaryInteger.h"

class CRangedIntPolicy
{
public:
	void SetValues(int32 nMin, int32 nMax)
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
	bool ReadValue(CCommInputStream& in, int32& value, CArithModel* pModel, uint32 age) const
	{
		value = m_integer.ReadValue(in);
		return true;
	}
	bool WriteValue(CCommOutputStream& out, int32 value, CArithModel* pModel, uint32 age) const
	{
		m_integer.WriteValue(out, value);
		return true;
	}

	#define DECL_PROXY_SIGNED(T)                                                                \
	  bool ReadValue(CCommInputStream & in, T & value, CArithModel * pModel, uint32 age) const  \
	  {                                                                                         \
	    int32 temp;                                                                             \
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
	    int32 temp = (int32)value;                                                              \
	    NET_ASSERT(temp == value);                                                              \
	    return WriteValue(out, temp, pModel, age);                                              \
	  }

	DECL_PROXY_SIGNED(int8)
	DECL_PROXY_SIGNED(int16)
	DECL_PROXY_SIGNED(int64)
	DECL_PROXY_SIGNED(uint8)
	DECL_PROXY_SIGNED(uint16)
	DECL_PROXY_SIGNED(uint32)
	DECL_PROXY_SIGNED(uint64)

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("RangedIntPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("RangedIntPolicy: not implemented for generic types");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, int32& value, uint32 age) const
	{
		value = m_integer.ReadValue(in);
		return true;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, int32 value, uint32 age) const
	{
		m_integer.WriteValue(out, value);
		return true;
	}

	#define DECL_PROXY_SIGNED(T)                                                \
	  bool ReadValue(CNetInputSerializeImpl * in, T & value, uint32 age) const  \
	  {                                                                         \
	    int32 temp;                                                             \
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
	    int32 temp = (int32)value;                                              \
	    NET_ASSERT(temp == value);                                              \
	    return WriteValue(out, temp, age);                                      \
	  }

	DECL_PROXY_SIGNED(int8)
	DECL_PROXY_SIGNED(int16)
	DECL_PROXY_SIGNED(int64)
	DECL_PROXY_SIGNED(uint8)
	DECL_PROXY_SIGNED(uint16)
	DECL_PROXY_SIGNED(uint32)
	DECL_PROXY_SIGNED(uint64)

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("RangedIntPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("RangedIntPolicy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "");
		pSizer->Add(*this);
	}

#if NET_PROFILE_ENABLE
	int GetBitCount(int32 value)
	{
		return m_integer.GetBitCount();
	}

	#define DECL_COUNT_SIGNED(T)   \
	  int GetBitCount(T value)     \
	  {                            \
	    int32 temp = (int32)value; \
	    return GetBitCount(temp);  \
	  }

	DECL_COUNT_SIGNED(int8)
	DECL_COUNT_SIGNED(int16)
	DECL_COUNT_SIGNED(int64)
	DECL_COUNT_SIGNED(uint8)
	DECL_COUNT_SIGNED(uint16)
	DECL_COUNT_SIGNED(uint32)
	DECL_COUNT_SIGNED(uint64)

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif

private:
	CStationaryInteger m_integer;
};

#endif
