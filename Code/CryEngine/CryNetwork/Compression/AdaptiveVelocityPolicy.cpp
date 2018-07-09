// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "AdaptiveVelocity.h"

class CAdaptiveVelocityPolicy
{
public:
	bool Load(XmlNodeRef node, const string& filename)
	{
		return
		  m_floats[0].Load(node, filename, "Params") &&
		  m_floats[1].Load(node, filename, "Params") &&
		  m_floats[2].Load(node, filename, "Params");
	}

#if USE_MEMENTO_PREDICTORS

	bool ReadMemento(CByteInputStream& in) const
	{
		for (int i = 0; i < 3; i++)
			m_floats[i].ReadMemento(in);
		return true;
	}

	bool WriteMemento(CByteOutputStream& out) const
	{
		for (int i = 0; i < 3; i++)
			m_floats[i].WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		for (int i = 0; i < 3; i++)
			m_floats[i].NoMemento();
	}

#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, Vec3& value, CArithModel* pModel, uint32 age) const
	{
		for (int i = 0; i < 3; i++)
			if (!m_floats[i].ReadValue(in, value[i]))
				return false;
		NetLogPacketDebug("CAdaptiveVelocityPolicy::ReadValue Previously Read (CAdaptiveVelocity, CAdaptiveVelocity, CAdaptiveVelocity) (%f, %f, %f) (%f)", value.x, value.y, value.z, in.GetBitSize());
		return true;
	}
	bool WriteValue(CCommOutputStream& out, Vec3 value, CArithModel* pModel, uint32 age) const
	{
		for (int i = 0; i < 3; i++)
			m_floats[i].WriteValue(out, value[i]);
		return true;
	}

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("AdaptiveVelocityPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("AdaptiveVelocityPolicy: not implemented for generic types");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, Vec3& value, uint32 age) const
	{
		for (int i = 0; i < 3; i++)
			if (!m_floats[i].ReadValue(in, value[i]))
				return false;
		NetLogPacketDebug("CAdaptiveVelocityPolicy::ReadValue Previously Read (CAdaptiveVelocity, CAdaptiveVelocity, CAdaptiveVelocity) (%f, %f, %f) (%f)", value.x, value.y, value.z, in->GetBitSize());
		return true;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, Vec3 value, uint32 age) const
	{
		for (int i = 0; i < 3; i++)
			m_floats[i].WriteValue(out, value[i]);
		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("AdaptiveVelocityPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("AdaptiveVelocityPolicy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CAdaptiveVelocityPolicy");
		pSizer->Add(*this);
	}
#if NET_PROFILE_ENABLE
	int GetBitCount(Vec3 value)
	{
		return m_floats[0].GetBitCount() + m_floats[1].GetBitCount() + m_floats[2].GetBitCount();
	}

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif

private:
	CAdaptiveVelocity m_floats[3];
};

//REGISTER_COMPRESSION_POLICY(CAdaptiveVelocityPolicy, "AdaptiveVelocity");
REGISTER_COMPRESSION_POLICY(CAdaptiveVelocityPolicy, "Velocity");
