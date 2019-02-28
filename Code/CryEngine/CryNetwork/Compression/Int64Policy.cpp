// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CompressionPolicyTime.h"
#include "ArithModel.h"

//#include "Int64Compressor.h"
#include "PredictiveBase.h"

typedef CPredictiveBase<int64> tBasePolicy;

class CInt64Policy :
	public tBasePolicy
{
public:
	CInt64Policy()
	{
		m_ignoreBits = 0;
	}

	void SetTimeValue(uint32 timeFraction32)
	{
#if USE_MEMENTO_PREDICTORS
		m_timeFraction32 = timeFraction32;
#endif
	}

	bool Manage(CCompressionManager* pManager)
	{
		return tBasePolicy::Manage(pManager, m_name.c_str(), 0);
	}

	void Init(CCompressionManager* pManager)
	{
		tBasePolicy::Init(m_name.c_str(), 0, pManager->GetUseDirectory().c_str(), pManager->GetAccDirectory().c_str());
	}
	
	bool Load( XmlNodeRef node, const string& filename)
	{
		m_name = node->getAttr("name");

		XmlNodeRef params = node->findChild("Params");
		if (params != nullptr)
		{
			tBasePolicy::Load(params, 1.f);
			params->getAttr("ignoreBits", m_ignoreBits);
		}

		return true;
	}
#if USE_MEMENTO_PREDICTORS
	void ReadMemento( CByteInputStream& stm ) const
	{
		m_bHaveMemento = true;

		tBasePolicy::ReadMemento(stm);
	}
	void WriteMemento( CByteOutputStream& stm ) const
	{
		m_bHaveMemento = true;

		tBasePolicy::WriteMemento(stm);
	}
	void NoMemento() const
	{
		m_bHaveMemento = false;

		tBasePolicy::NoMemento();
	}
#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, int64& value, CArithModel * pModel, uint32 age) const
	{
		int64 prediction = tBasePolicy::Predict(m_timeFraction32);

		if (m_bHaveMemento)
		{
			int32 error = 0;

			if (m_errorDistributionRead.ReadValue(error, &in, m_lastTimeZero != 0))
				value = prediction + error;
			else
				value = in.ReadBitsLarge(64 - m_ignoreBits);
		}
		else
			value = in.ReadBitsLarge(64 - m_ignoreBits);

		Update(value, prediction, age, m_timeFraction32);

		value = value << (int64)m_ignoreBits;
		return true;
	}

	bool WriteValue(CCommOutputStream& out, int64 value, CArithModel* pModel, uint32 age) const
	{
		if (m_ignoreBits)
			value = value >> (int64)m_ignoreBits;

		int64 predicted = Predict(m_timeFraction32);

		int64 error = value - predicted;

		m_mementoSequenceId += age;

		bool errorToBig = false;
		if (error > (int64)INT_MAX - 1 || error < (int64)(INT_MIN + 1))
			errorToBig = true;

		if (m_bHaveMemento && !errorToBig)
			Count(error);

		uint32 symSize = 0xffffffff;
		uint32 symTotal = 0xffffffff;

#if ENABLE_ACCURATE_BANDWIDTH_PROFILING	
		float s1 = out.GetBitSize();
#endif

		bool writeBits = true;

		if (m_bHaveMemento)
		{
			if (errorToBig)
			{
				m_errorDistributionWrite.WriteOutOfRange(&out);
				m_errorDistributionWrite.WriteBitOutOfRange(&out);
				writeBits = false;
			}
			else if (m_errorDistributionWrite.WriteValue((int32)error, &out, m_lastTimeZero != 0))
				writeBits = false;

			m_errorDistributionWrite.GetDebug(symSize, symTotal);
		}

		if (writeBits)
			out.WriteBitsLarge(value, 64 - m_ignoreBits);

#if ENABLE_ACCURATE_BANDWIDTH_PROFILING	
		if (m_bHaveMemento)
		{
			float s2 = out.GetBitSize();
			Track((int32)value, (int32)predicted, age, m_timeFraction32, s2 - s1, symSize, symTotal);
		}
#endif

		Update(value, predicted, age, m_timeFraction32);

		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("CInt64Policy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("CInt64Policy: not implemented for generic types");
		return false;
	}
#else
	bool ReadValue( CNetInputSerializeImpl* in, int64& value, uint32 age ) const
	{
		return false;
	}
	bool WriteValue( CNetOutputSerializeImpl* out, int64 value, uint32 age ) const
	{
		return false;
	}

	template <class T>
	bool ReadValue( CNetInputSerializeImpl* in, T& value, uint32 age ) const
	{
		NetWarning("CInt64Policy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CNetOutputSerializeImpl* out, T value, uint32 age ) const
	{
		NetWarning("CInt64Policy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
	}

#if NET_PROFILE_ENABLE
	int GetBitCount(int64 value)
	{
		return 64;
	}

	template <class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif
	
private:
	int m_ignoreBits;
	string m_name;

#if USE_MEMENTO_PREDICTORS
	mutable bool m_bHaveMemento;

	mutable uint32 m_timeFraction32;
#endif
};

REGISTER_COMPRESSION_POLICY_TIME(CInt64Policy, "Int64Policy");