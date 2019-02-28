// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompressionPolicyTime.h"
#include "ArithModel.h"
#include "AdaptiveFloat.h"
#include "PredictiveFloat.h"
#include "Protocol/Serialize.h"
#include "BoolCompress.h"

template<typename FloatType>
class COrientationPolicyT
{
public:
	COrientationPolicyT()
	{
		m_time32 = 0;
	}

	bool Load(XmlNodeRef node, const string& filename)
	{
		m_name = node->getAttr("name");
		bool ret = m_axis[0].Load(node, filename, "Params") &&
		  m_axis[1].Load(node, filename, "Params") &&
		  m_axis[2].Load(node, filename, "Params");

		return ret;
	}
#if USE_MEMENTO_PREDICTORS
	bool ReadMemento(CByteInputStream& in) const
	{
		m_axis[0].ReadMemento(in);
		m_axis[1].ReadMemento(in);
		m_axis[2].ReadMemento(in);

	#if USE_ARITHSTREAM
		m_halfSphere = in.GetTyped<bool>();
	#else
		m_w.ReadMemento(in);
	#endif

		return true;
	}

	bool WriteMemento(CByteOutputStream& out) const
	{
		m_axis[0].WriteMemento(out);
		m_axis[1].WriteMemento(out);
		m_axis[2].WriteMemento(out);

	#if USE_ARITHSTREAM
		out.PutTyped<bool>() = m_halfSphere;
	#else
		m_w.WriteMemento(out);
	#endif

		return true;
	}

	void NoMemento() const
	{
		m_axis[0].NoMemento();
		m_axis[1].NoMemento();
		m_axis[2].NoMemento();

	#if USE_ARITHSTREAM
		m_halfSphere = false;
	#else
		m_w.NoMemento();
	#endif
	}

	bool Manage(CCompressionManager* pManager)
	{
		bool ret = false;
		for (unsigned k = 0; k < 3; k++)
			ret |= m_axis[k].Manage(pManager, m_name.c_str(), k);

		return ret;
}

	void Init(CCompressionManager* pManager)
	{
		for (int k = 0; k < 3; k++)
			m_axis[k].Init(m_name.c_str(), k, pManager->GetUseDirectory().c_str(), pManager->GetAccDirectory().c_str());
	}

#else //USE_MEMENTO_PREDICTORS
	bool Manage(CCompressionManager* pManager) { return false; }
	void Init(CCompressionManager* pManager) {}
#endif //USE_MEMENTO_PREDICTORS

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, Quat& value, CArithModel* pModel, uint32 age) const
	{
		bool ok = true;
		for (int i = 0; ok && i < 3; i++)
			ok &= m_axis[i].ReadValue(in, value.v[i], age, m_time32);
		if (!ok)
			return false;

		uint8 changedHalfSphere = in.DecodeShift(4);
		if (changedHalfSphere >= 15)
		{
			m_halfSphere = !m_halfSphere;
			in.UpdateShift(4, 15, 1);
		}
		else
			in.UpdateShift(4, 0, 15);

		if (1 - value.v.x * value.v.x - value.v.y * value.v.y - value.v.z * value.v.z > 0)
			value.w = sqrtf(1 - value.v.x * value.v.x - value.v.y * value.v.y - value.v.z * value.v.z);
		else
			value.w = 0;
		if (!m_halfSphere)
			value.w *= -1;
		if (!(fabsf(value.GetLength() - 1.0f) < 0.001f))
			value.Normalize();

		NetLogPacketDebug("TAdaptiveOrientationPolicy::ReadValue Previously Read (CAdaptiveFloat, CAdaptiveFloat, CAdaptiveFloat) (%f, %f, %f, %f) (%f)", value.v.x, value.v.y, value.v.z, value.w, in.GetBitSize());

		return ok;
	}

	bool WriteValue(CCommOutputStream& out, Quat value, CArithModel* pModel, uint32 age) const
	{
		if (fabsf(value.GetLength() - 1.0f) > 0.001f)
			value.Normalize();

		for (int i = 0; i < 3; i++)
			m_axis[i].WriteValue(out, value.v[i], age, m_time32);

		uint8 changedHalfSphere = 0;
		bool newHalfSphere = (value.w >= 0);
		if (newHalfSphere != m_halfSphere)
			changedHalfSphere = 1;
		if (changedHalfSphere)
		{
			out.EncodeShift(4, 15, 1);
			m_halfSphere = newHalfSphere;
		}
		else
			out.EncodeShift(4, 0, 15);

		return true;
	}

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("AdaptiveOrientationPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("AdaptiveOrientationPolicy: not implemented for generic types");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, Quat& value, uint32 age) const
	{
		bool ok;

		ok = m_axis[0].ReadValue(in, value.v.x, age, m_time32);
		ok = ok && m_axis[1].ReadValue(in, value.v.y, age, m_time32);
		ok = ok && m_axis[2].ReadValue(in, value.v.z, age, m_time32);

		float wscale = in->ReadBits(1) ? 1.0f : -1.0f;
		value.w = 1.0f - (value.v.x * value.v.x + value.v.y * value.v.y + value.v.z * value.v.z);
		value.w = (value.w > 0.0f) ? sqrtf(value.w) : 0.0f;
		value.w *= wscale;

		value.Normalize();

		NetLogPacketDebug("TAdaptiveOrientationPolicy::ReadValue Previously Read (CAdaptiveFloat, CAdaptiveFloat, CAdaptiveFloat) (%f, %f, %f, %f) (%f)", value.v.x, value.v.y, value.v.z, value.w, in->GetBitSize());

		return ok;
	}

	bool WriteValue(CNetOutputSerializeImpl* out, Quat value, uint32 age) const
	{
		value.Normalize();

		m_axis[0].WriteValue(out, value.v.x, age, m_time32);
		m_axis[1].WriteValue(out, value.v.y, age, m_time32);
		m_axis[2].WriteValue(out, value.v.z, age, m_time32);
		out->WriteBits(value.w >= 0.0f ? 1 : 0, 1);

		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("AdaptiveOrientationPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("AdaptiveOrientationPolicy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "TAdaptiveOrientationPolicy");
		pSizer->Add(*this);
	}

#if NET_PROFILE_ENABLE
	#define BITCOUNT_ENCODESHIFT 4
	int GetBitCount(Quat value)
	{
		return m_axis[0].GetBitCount() + m_axis[1].GetBitCount() + m_axis[2].GetBitCount() + BITCOUNT_ENCODESHIFT;
	}

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif
	void SetTimeValue(uint32 time)
	{
		m_time32 = time;
	}
	
private:
	string m_name;
	mutable uint32 m_time32;
	FloatType m_axis[3];
#if USE_ARITHSTREAM
	mutable bool   m_halfSphere;
#else
	#if USE_MEMENTO_PREDICTORS
	CBoolCompress m_w;
	#endif
#endif
};

typedef COrientationPolicyT<CAdaptiveFloat> TAdaptiveOrientationPolicy;
typedef COrientationPolicyT<CPredictiveFloat> TPredictiveOrientationPolicy;

REGISTER_COMPRESSION_POLICY_TIME(TAdaptiveOrientationPolicy, "AdaptiveOrientation");
REGISTER_COMPRESSION_POLICY_TIME(TPredictiveOrientationPolicy, "PredictiveOrientation");
