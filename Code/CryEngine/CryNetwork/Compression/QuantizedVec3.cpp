// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "Quantizer.h"
#include "Protocol/Serialize.h"

class CQuantizedVec3Policy
{
public:
	bool Load(XmlNodeRef node, const string& filename)
	{
		return
		  m_floats[0].Load(node, filename, "XParams") &&
		  m_floats[1].Load(node, filename, "YParams") &&
		  m_floats[2].Load(node, filename, "ZParams");
	}

#if USE_MEMENTO_PREDICTORS
	bool ReadMemento(CByteInputStream& in) const
	{
		return true;
	}

	bool WriteMemento(CByteOutputStream& out) const
	{
		return true;
	}

	void NoMemento() const
	{
	}
#endif

#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& in, Ang3& value, CArithModel* pModel, uint32 age) const
	{
		for (int i = 0; i < 3; i++)
		{
			uint32 q = in.ReadBits(m_floats[i].GetNumBits());
			value[i] = m_floats[i].Dequantize(q);
		}

		NetLogPacketDebug("CQuantizedVec3Policy::ReadValue (%f, %f, %f) (Min %f Max %f NumBits %d, Min %f Max %f NumBits %d, Min %f Max %f NumBits %d) (%f)",
		                  value.x, value.y, value.z,
		                  m_floats[0].GetMinValue(), m_floats[0].GetMaxValue(), m_floats[0].GetNumBits(),
		                  m_floats[1].GetMinValue(), m_floats[1].GetMaxValue(), m_floats[1].GetNumBits(),
		                  m_floats[2].GetMinValue(), m_floats[2].GetMaxValue(), m_floats[2].GetNumBits(),
		                  in.GetBitSize());

		return true;
	}
	bool WriteValue(CCommOutputStream& out, Ang3 value, CArithModel* pModel, uint32 age) const
	{
		for (int i = 0; i < 3; i++)
		{
			uint32 q = m_floats[i].Quantize(value[i]);
			out.WriteBits(q, m_floats[i].GetNumBits());
		}
		return true;
	}
	bool ReadValue(CCommInputStream& in, Vec3& value, CArithModel* pModel, uint32 age) const
	{
		for (int i = 0; i < 3; i++)
		{
			uint32 q = in.ReadBits(m_floats[i].GetNumBits());
			value[i] = m_floats[i].Dequantize(q);
		}

		NetLogPacketDebug("CQuantizedVec3Policy::ReadValue (%f, %f, %f) (Min %f Max %f NumBits %d, Min %f Max %f NumBits %d, Min %f Max %f NumBits %d) (%f)",
		                  value.x, value.y, value.z,
		                  m_floats[0].GetMinValue(), m_floats[0].GetMaxValue(), m_floats[0].GetNumBits(),
		                  m_floats[1].GetMinValue(), m_floats[1].GetMaxValue(), m_floats[1].GetNumBits(),
		                  m_floats[2].GetMinValue(), m_floats[2].GetMaxValue(), m_floats[2].GetNumBits(),
		                  in.GetBitSize());

		return true;
	}
	bool WriteValue(CCommOutputStream& out, Vec3 value, CArithModel* pModel, uint32 age) const
	{
		for (int i = 0; i < 3; i++)
		{
			uint32 q = m_floats[i].Quantize(value[i]);
			out.WriteBits(q, m_floats[i].GetNumBits());
		}
		return true;
	}

	template<class T>
	bool ReadValue(CCommInputStream& in, T& value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("QuantizedVec3Policy: not implemented for generic types");
		return false;
	}
	template<class T>
	bool WriteValue(CCommOutputStream& out, T value, CArithModel* pModel, uint32 age) const
	{
		NetWarning("QuantizedVec3Policy: not implemented for generic types");
		return false;
	}
#else
	bool ReadValue(CNetInputSerializeImpl* in, Ang3& value, uint32 age) const
	{
		value.x = m_floats[0].Dequantize(in->ReadBits(m_floats[0].GetNumBits()));
		value.y = m_floats[1].Dequantize(in->ReadBits(m_floats[1].GetNumBits()));
		value.z = m_floats[2].Dequantize(in->ReadBits(m_floats[2].GetNumBits()));

		NetLogPacketDebug("CQuantizedVec3Policy::ReadValue (%f, %f, %f) (Min %f Max %f NumBits %d, Min %f Max %f NumBits %d, Min %f Max %f NumBits %d) (%f)",
		                  value.x, value.y, value.z,
		                  m_floats[0].GetMinValue(), m_floats[0].GetMaxValue(), m_floats[0].GetNumBits(),
		                  m_floats[1].GetMinValue(), m_floats[1].GetMaxValue(), m_floats[1].GetNumBits(),
		                  m_floats[2].GetMinValue(), m_floats[2].GetMaxValue(), m_floats[2].GetNumBits(),
		                  in->GetBitSize());

		return true;
	}

	bool WriteValue(CNetOutputSerializeImpl* out, Ang3 value, uint32 age) const
	{
		out->WriteBits(m_floats[0].Quantize(value.x), m_floats[0].GetNumBits());
		out->WriteBits(m_floats[1].Quantize(value.y), m_floats[1].GetNumBits());
		out->WriteBits(m_floats[2].Quantize(value.z), m_floats[2].GetNumBits());

		return true;
	}
	bool ReadValue(CNetInputSerializeImpl* in, Vec3& value, uint32 age) const
	{
		value.x = m_floats[0].Dequantize(in->ReadBits(m_floats[0].GetNumBits()));
		value.y = m_floats[1].Dequantize(in->ReadBits(m_floats[1].GetNumBits()));
		value.z = m_floats[2].Dequantize(in->ReadBits(m_floats[2].GetNumBits()));

		NetLogPacketDebug("CQuantizedVec3Policy::ReadValue (%f, %f, %f) (Min %f Max %f NumBits %d, Min %f Max %f NumBits %d, Min %f Max %f NumBits %d) (%f)",
		                  value.x, value.y, value.z,
		                  m_floats[0].GetMinValue(), m_floats[0].GetMaxValue(), m_floats[0].GetNumBits(),
		                  m_floats[1].GetMinValue(), m_floats[1].GetMaxValue(), m_floats[1].GetNumBits(),
		                  m_floats[2].GetMinValue(), m_floats[2].GetMaxValue(), m_floats[2].GetNumBits(),
		                  in->GetBitSize());

		return true;
	}
	bool WriteValue(CNetOutputSerializeImpl* out, Vec3 value, uint32 age) const
	{
		out->WriteBits(m_floats[0].Quantize(value.x), m_floats[0].GetNumBits());
		out->WriteBits(m_floats[1].Quantize(value.y), m_floats[1].GetNumBits());
		out->WriteBits(m_floats[2].Quantize(value.z), m_floats[2].GetNumBits());

		return true;
	}

	template<class T>
	bool ReadValue(CNetInputSerializeImpl* in, T& value, uint32 age) const
	{
		NetWarning("QuantizedVec3Policy: not implemented for generic types");
		return false;
	}

	template<class T>
	bool WriteValue(CNetOutputSerializeImpl* out, T value, uint32 age) const
	{
		NetWarning("QuantizedVec3Policy: not implemented for generic types");
		return false;
	}
#endif

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CQuantizedVec3Policy");
		pSizer->Add(*this);
	}
#if NET_PROFILE_ENABLE
	int GetBitCount(Vec3 value)
	{
		return m_floats[0].GetNumBits() + m_floats[1].GetNumBits() + m_floats[2].GetNumBits();
	}

	template<class T>
	int GetBitCount(T value)
	{
		return 0;
	}
#endif

private:
	CQuantizer m_floats[3];
};

REGISTER_COMPRESSION_POLICY(CQuantizedVec3Policy, "QuantizedVec3");
