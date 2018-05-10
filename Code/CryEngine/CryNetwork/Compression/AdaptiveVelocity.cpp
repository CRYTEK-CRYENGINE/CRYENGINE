// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AdaptiveVelocity.h"
#include "ArithPrimitives.h"
#include "Protocol/Serialize.h"

bool CAdaptiveVelocity::Load(XmlNodeRef node, const string& filename, const string& child)
{
	bool ok = true;
	if (m_quantizer.Load(node, filename, child, eFQM_TruncateLeft))
	{
#if USE_MEMENTO_PREDICTORS
		int height = 1024000;
		node->getAttr("height", height);
		if (height < 0)
			ok = false;

		if (ok)
		{
			m_nHeight = height;
		}
#endif
	}
	else
	{
		ok = false;
	}
	return ok;
}

#if USE_MEMENTO_PREDICTORS
void CAdaptiveVelocity::ReadMemento(CByteInputStream& stm) const
{
	m_boolCompress.ReadMemento(stm);
	m_nLastQuantized = stm.GetTyped<uint32>();
}

void CAdaptiveVelocity::WriteMemento(CByteOutputStream& stm) const
{
	m_boolCompress.WriteMemento(stm);
	stm.PutTyped<uint32>() = m_nLastQuantized;
}

void CAdaptiveVelocity::NoMemento() const
{
	m_boolCompress.NoMemento();
	m_nLastQuantized = 0;
}
#endif

#if USE_ARITHSTREAM
bool CAdaptiveVelocity::ReadValue(CCommInputStream& stm, float& value) const
{
	uint32 left, right;
	uint32 quantized = left = right = m_nLastQuantized;

	bool moved = m_boolCompress.ReadValue(stm);

	if (moved)
	{
		int32 searchArea = m_quantizer.GetMaxQuantizedValue() / int32(50);
		if (searchArea == 0)
			searchArea = 1;
		left = (uint32)((int64(m_nLastQuantized) - searchArea > 0) ? m_nLastQuantized - searchArea : 0);
		right = (uint32)((uint64(m_nLastQuantized) + searchArea < m_quantizer.GetMaxQuantizedValue()) ? m_nLastQuantized + searchArea : m_quantizer.GetMaxQuantizedValue());

		if (!SquarePulseProbabilityReadImproved(quantized, stm, left, right, 1024, m_quantizer.GetMaxQuantizedValue(), 95, m_quantizer.GetNumBits()))
			return false;
		m_nLastQuantized = quantized;
	}

	value = m_quantizer.Dequantize(quantized);
	NetLogPacketDebug("CAdaptiveVelocity::ReadValue Previously Read (CBoolCompress) %f Min %f Max %f NumBits %d (%f)", value, m_quantizer.GetMinValue(), m_quantizer.GetMaxValue(), m_quantizer.GetNumBits(), stm.GetBitSize());
	return true;
}

void CAdaptiveVelocity::WriteValue(CCommOutputStream& stm, float value) const
{
	//quantize value
	uint32 quantized = m_quantizer.Quantize(value);
	uint32 left, right;
	left = right = m_nLastQuantized;

	bool moved = (quantized != m_nLastQuantized) ? true : false;
	m_boolCompress.WriteValue(stm, moved);
	if (moved)
	{
		int32 searchArea = m_quantizer.GetMaxQuantizedValue() / int32(50);
		if (searchArea == 0)
			searchArea = 1;
		left = (uint32)((int64(m_nLastQuantized) - searchArea > 0) ? m_nLastQuantized - searchArea : 0);
		right = (uint32)((uint64(m_nLastQuantized) + searchArea < m_quantizer.GetMaxQuantizedValue()) ? m_nLastQuantized + searchArea : m_quantizer.GetMaxQuantizedValue());
		SquarePulseProbabilityWriteImproved(stm, quantized, left, right, 1024, m_quantizer.GetMaxQuantizedValue(), 95, m_quantizer.GetNumBits());

		m_nLastQuantized = quantized;
	}
}
#else
bool CAdaptiveVelocity::ReadValue(CNetInputSerializeImpl* stm, float& value) const
{
	uint32 quantized = stm->ReadBits(m_quantizer.GetNumBits());

	value = m_quantizer.Dequantize(quantized);
	NetLogPacketDebug("CAdaptiveVelocity::ReadValue %f Min %f Max %f NumBits %d (%f)", value, m_quantizer.GetMinValue(), m_quantizer.GetMaxValue(), m_quantizer.GetNumBits(), stm->GetBitSize());

	return true;
}

void CAdaptiveVelocity::WriteValue(CNetOutputSerializeImpl* stm, float value) const
{
	uint32 quantized = m_quantizer.Quantize(value);

	stm->WriteBits(quantized, m_quantizer.GetNumBits());
}
#endif
#if NET_PROFILE_ENABLE
int CAdaptiveVelocity::GetBitCount()
{
	return
	#if USE_MEMENTO_PREDICTORS
	  m_boolCompress.GetBitCount() +
	#endif
	  m_quantizer.GetNumBits();
}
#endif
