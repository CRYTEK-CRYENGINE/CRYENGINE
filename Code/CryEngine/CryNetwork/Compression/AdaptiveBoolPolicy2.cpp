// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICompressionPolicy2.h"
#include "ArithModel.h"
#include "BoolCompress2.h"

class CAdaptiveBoolPolicy2
{
public:
#if USE_MEMENTO_PREDICTORS
	typedef CBoolCompress2::TMemento TMemento;
#endif
	bool Load(XmlNodeRef node, const string& filename)
	{
		return true;
	}

#if USE_MEMENTO_PREDICTORS
	bool ReadMemento(TMemento& memento, CByteInputStream& in) const
	{
		m_bool.ReadMemento(memento, in);
		return true;
	}

	bool WriteMemento(TMemento memento, CByteOutputStream& out) const
	{
		m_bool.WriteMemento(memento, out);
		return true;
	}

	void InitMemento(TMemento& memento) const
	{
		m_bool.InitMemento(memento);
	}

	bool UpdateMemento(TMemento& memento, bool value) const
	{
		m_bool.UpdateMemento(memento, value);
		return true;
	}
#endif

#if USE_ARITHSTREAM
	bool ReadValue(TMemento memento, CCommInputStream& in, CArithModel* pModel, uint32 age, bool& value) const
	{
		return m_bool.ReadValue(memento, in, value);
	}
	bool WriteValue(TMemento memento, CCommOutputStream& out, CArithModel* pModel, uint32 age, bool value) const
	{
		return m_bool.WriteValue(memento, out, value);
	}
#else
	#if USE_MEMENTO_PREDICTORS
	bool ReadValue(TMemento memento, CNetInputSerializeImpl* in, uint32 age, bool& value) const
	{
		return m_bool.ReadValue(memento, in, value);
	}
	bool WriteValue(TMemento memento, CNetOutputSerializeImpl* out, uint32 age, bool value) const
	{
		return m_bool.WriteValue(memento, out, value);
	}
	#else
	bool ReadValue(CNetInputSerializeImpl* in, uint32 age, bool& value) const
	{
		return m_bool.ReadValue(in, value);
	}
	bool WriteValue(CNetOutputSerializeImpl* out, uint32 age, bool value) const
	{
		return m_bool.WriteValue(out, value);
	}
	#endif
#endif

#if USE_MEMENTO_PREDICTORS
	template<class T>
	bool UpdateMemento(TMemento& memento, T value) const
	{
		NetWarning("BoolPolicy: not implemented for generic types");
		return false;
	}
#endif

#if USE_ARITHSTREAM
	template<class T>
	bool ReadValue(TMemento memento, CCommInputStream& in, CArithModel* pModel, uint32 age, T& value) const
	{
		NetWarning("BoolPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(TMemento memento, CCommOutputStream& out, CArithModel* pModel, uint32 age, T value) const
	{
		NetWarning("BoolPolicy: not implemented for generic types");
		return false;
	}
#else
	#if USE_MEMENTO_PREDICTORS
	template<class T>
	bool ReadValue(TMemento memento, CNetInputSerializeImpl* in, uint32 age, T& value) const
	{
		NetWarning("BoolPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(TMemento memento, CNetOutputSerializeImpl* out, uint32 age, T value) const
	{
		NetWarning("BoolPolicy: not implemented for generic types");
		return false;
	}
	#else
	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, uint32 age, T& value) const
	{
		NetWarning("BoolPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, uint32 age, T value) const
	{
		NetWarning("BoolPolicy: not implemented for generic types");
		return false;
	}
	#endif
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CAdaptiveBoolPolicy");
		pSizer->Add(*this);
	}

#if NET_PROFILE_ENABLE
	int GetBitCount(bool value)
	{
		return m_bool.GetBitCount();
	}

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif

private:
	CBoolCompress2 m_bool;
};

REGISTER_COMPRESSION_POLICY2(CAdaptiveBoolPolicy2, "AdaptiveBool2");
