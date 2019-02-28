// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "AdaptiveFloat.h"
#include "BoolCompress.h"

class CAdaptiveUnitVec3Policy
{
public:
	bool Load(XmlNodeRef node, const string& filename)
	{
		return m_x.Load(node, filename, "Params") && m_y.Load(node, filename, "Params");
	}
#if USE_MEMENTO_PREDICTORS
	bool ReadMemento(CByteInputStream& in) const
	{
		m_x.ReadMemento(in);
		m_y.ReadMemento(in);
		m_z.ReadMemento(in);
		return true;
	}

	bool WriteMemento(CByteOutputStream& out) const
	{
		m_x.WriteMemento(out);
		m_y.WriteMemento(out);
		m_z.WriteMemento(out);
		return true;
	}

	void NoMemento() const
	{
		m_x.NoMemento();
		m_y.NoMemento();
		m_z.NoMemento();
	}
#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, Vec3& value, CArithModel* pModel, uint32 age) const
	{
		if (!m_x.ReadValue(in, value.x, age))
			return false;
		if (!m_y.ReadValue(in, value.y, age))
			return false;
		bool sign = m_z.ReadValue(in);
		float ssxy = value.x * value.x + value.y * value.y; // square sum of xy
		if (ssxy <= 1.0f)
		{
			float sssxy = sqrtf(1.0f - ssxy); // square root of square sum of xy
			value.z = sign ? sssxy : -sssxy;
		}
		else
			value.z = 0.0f;
		value.Normalize();
		NetLogPacketDebug("CAdaptiveUnitVec3Policy::ReadValue Previously Read (CAdaptiveFloat, CAdaptiveFloat, CBoolCompress) (%f, %f, %f) (%f)", value.x, value.y, value.z, in.GetBitSize());
		return true;
	}

	bool WriteValue(CCommOutputStream& out, Vec3 value, CArithModel* pModel, uint32 age) const
	{
		value.Normalize();

		m_x.WriteValue(out, value.x, age);
		m_y.WriteValue(out, value.y, age);
		m_z.WriteValue(out, value.z >= 0.0f);

		return true;
	}

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("AdaptiveUnitVec3Policy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("AdaptiveUnitVec3Policy: not implemented for generic types");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, Vec3& value, uint32 age) const
	{
		bool ok;

		ok = m_x.ReadValue(in, value.x, age);
		ok = ok && m_y.ReadValue(in, value.y, age);

		float zscale = m_z.ReadValue(in) ? 1.0f : -1.0f;
		value.z = 1.0f - (value.x * value.x + value.y * value.y);
		value.z = (value.z > 0.0f) ? sqrtf(value.z) : 0.0f;
		value.z *= zscale;

		value.Normalize();
		NetLogPacketDebug("CAdaptiveUnitVec3Policy::ReadValue Previously Read (CAdaptiveFloat, CAdaptiveFloat, CBoolCompress) (%f, %f, %f) (%f)", value.x, value.y, value.z, in->GetBitSize());

		return ok;
	}

	bool WriteValue(CNetOutputSerializeImpl* out, Vec3 value, uint32 age) const
	{
		value.Normalize();

		m_x.WriteValue(out, value.x, age);
		m_y.WriteValue(out, value.y, age);
		m_z.WriteValue(out, value.z >= 0.0f);

		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("AdaptiveUnitVec3Policy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("AdaptiveUnitVec3Policy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CAdaptiveUnitVec3Policy");
		pSizer->Add(*this);
	}

#if NET_PROFILE_ENABLE
	int GetBitCount(Vec3 value)
	{
		return m_x.GetBitCount() + m_y.GetBitCount() + m_z.GetBitCount();
	}

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif

private:
	CAdaptiveFloat m_x;
	CAdaptiveFloat m_y;
	CBoolCompress  m_z;
};

REGISTER_COMPRESSION_POLICY(CAdaptiveUnitVec3Policy, "AdaptiveUnitVec3");
