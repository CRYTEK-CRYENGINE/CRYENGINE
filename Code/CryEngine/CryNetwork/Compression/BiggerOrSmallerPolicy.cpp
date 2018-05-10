// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "BoolCompress.h"

class CBiggerOrSmallerPolicy
{
public:
	bool Load(XmlNodeRef node, const string& filename)
	{
		m_threshold = 0;
		if (XmlNodeRef params = node->findChild("Params"))
			params->getAttr("threshold", m_threshold);
		m_bigValue = m_threshold + 1;
		if (XmlNodeRef params = node->findChild("Params"))
			params->getAttr("bigValue", m_bigValue);
		return true;
	}

#if USE_MEMENTO_PREDICTORS
	bool ReadMemento(CByteInputStream& in) const
	{
		m_bool.ReadMemento(in);
		return true;
	}

	bool WriteMemento(CByteOutputStream& out) const
	{
		m_bool.WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		m_bool.NoMemento();
	}
#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, float& value, CArithModel* pModel, uint32 age) const
	{
		bool wasBigger = m_bool.ReadValue(in);
		if (wasBigger)
			value = m_bigValue;
		else
			value = m_threshold;
		NetLogPacketDebug("CBiggerOrSmallerPolicy::ReadValue Previously Read (CBoolCompress) %f bigValue %f threshold %f (%f)", value, m_bigValue, m_threshold, in.GetBitSize());
		return true;
	}
	bool WriteValue(CCommOutputStream& out, float value, CArithModel* pModel, uint32 age) const
	{
		bool val = value > m_threshold;
		m_bool.WriteValue(out, val);
		return true;
	}

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("CBiggerOrSmallerPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("CBiggerOrSmallerPolicy: not implemented for generic types");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, float& value, uint32 age) const
	{
		value = m_bool.ReadValue(in) ? m_bigValue : m_threshold;
		NetLogPacketDebug("CBiggerOrSmallerPolicy::ReadValue Previously Read (CBoolCompress) %f bigValue %f threshold %f (%f)", value, m_bigValue, m_threshold, in->GetBitSize());
		return true;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, float value, CArithModel* pModel, uint32 age) const
	{
		m_bool.WriteValue(out, value > m_threshold);
		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("CBiggerOrSmallerPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("CBiggerOrSmallerPolicy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CBiggerOrSmaller");
		pSizer->Add(*this);
	}
#if NET_PROFILE_ENABLE
	int GetBitCount(float value)
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
	float                 m_threshold;
	float                 m_bigValue;
	mutable CBoolCompress m_bool;
};

REGISTER_COMPRESSION_POLICY(CBiggerOrSmallerPolicy, "BiggerOrSmaller");
