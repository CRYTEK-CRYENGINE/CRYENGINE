// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompressionManager.h"
#include "ICompressionPolicy.h"
#include "Quantizer.h"
#include "BoolCompress.h"
#include "Protocol/Serialize.h"

class CJumpyPolicy
{
public:
	bool Load(XmlNodeRef node, const string& filename)
	{
		return m_quantizer.Load(node, filename);
	}

#if USE_MEMENTO_PREDICTORS
	bool ReadMemento(CByteInputStream& in) const
	{
		m_boolCompress.ReadMemento(in);
		m_lastValue = in.GetTyped<uint32>();
		return true;
	}

	bool WriteMemento(CByteOutputStream& out) const
	{
		m_boolCompress.WriteMemento(out);
		out.PutTyped<uint32>() = m_lastValue;
		return true;
	}

	void NoMemento() const
	{
		m_boolCompress.NoMemento();
		m_lastValue = 0;
	}
#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, float& value, CArithModel* pModel, uint32 age) const
	{
		bool changed = m_boolCompress.ReadValue(in);
		if (changed)
		{
			uint32 quantized = in.ReadBits(m_quantizer.GetNumBits());
			m_lastValue = quantized;
		}
		value = m_quantizer.Dequantize(m_lastValue);
		NetLogPacketDebug("CJumpyPolicy::ReadValue Previously Read (CBoolCompress) %f Min %f Max %f NumBits %d  (%f)", value, m_quantizer.GetMinValue(), m_quantizer.GetMaxValue(), m_quantizer.GetNumBits(), in.GetBitSize());

		return true;
	}
	bool WriteValue(CCommOutputStream& out, float value, CArithModel* pModel, uint32 age) const
	{
		uint32 quantized = m_quantizer.Quantize(value);
		bool changed = false;
		if (quantized != m_lastValue)
			changed = true;
		m_boolCompress.WriteValue(out, changed);
		if (changed)
		{
			out.WriteBits(quantized, m_quantizer.GetNumBits());
			m_lastValue = quantized;
		}

		return true;
	}

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("JumpyPolicy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("JumpyPolicy: not implemented for generic types");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, float& value, uint32 age) const
	{
		value = m_quantizer.Dequantize(in->ReadBits(m_quantizer.GetNumBits()));
		NetLogPacketDebug("CJumpyPolicy::ReadValue %f Min %f Max %f NumBits %d (%f)", value, m_quantizer.GetMinValue(), m_quantizer.GetMaxValue(), m_quantizer.GetNumBits(), in->GetBitSize());

		return true;
	}

	bool WriteValue(CNetOutputSerializeImpl* out, float value, uint32 age) const
	{
		out->WriteBits(m_quantizer.Quantize(value), m_quantizer.GetNumBits());

		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("JumpyPolicy: not implemented for generic types");
		return false;
	}

	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("JumpyPolicy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CJumpyPolicy");
		pSizer->Add(*this);
	}
#if NET_PROFILE_ENABLE
	int GetBitCount(float value)
	{
		return
	#if USE_MEMENTO_PREDICTORS
		  m_boolCompress.GetBitCount() +
	#endif
		  m_quantizer.GetNumBits();
	}

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif
private:
	CQuantizer     m_quantizer;
#if USE_MEMENTO_PREDICTORS
	CBoolCompress  m_boolCompress;
	mutable uint32 m_lastValue;
#endif
};

REGISTER_COMPRESSION_POLICY(CJumpyPolicy, "Jumpy");
