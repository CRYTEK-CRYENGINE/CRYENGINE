// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ArithModel.h"

class CTimePolicy
{
public:
	bool Load(XmlNodeRef node, const string& filename);

#if USE_MEMENTO_PREDICTORS
	bool ReadMemento(CByteInputStream& in) const;
	bool WriteMemento(CByteOutputStream& out) const;
	void NoMemento() const;
#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, CTimeValue& value, CArithModel* pModel, uint32 age) const;
	bool WriteValue(CCommOutputStream& out, CTimeValue value, CArithModel* pModel, uint32 age) const;

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("EntityIdPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("EntityIdPolicy: not implemented for generic types");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, CTimeValue& value, uint32 age) const;
	bool WriteValue(CNetOutputSerializeImpl* out, CTimeValue value, uint32 age) const;

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("EntityIdPolicy: not implemented for generic types");
		return false;
	}

	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("EntityIdPolicy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const;
#if NET_PROFILE_ENABLE
	int GetBitCount(CTimeValue value);

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif
protected:
	ETimeStream m_stream;
};
