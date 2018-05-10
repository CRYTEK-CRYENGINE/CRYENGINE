// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AdaptiveFloat.h"
#include "ArithPrimitives.h"
#include "Protocol/Serialize.h"

/*

   The following is a comment from previous code; the layout of the implementation has changed; the algorithm has not


   AC_FLOAT serialization

   some info about the constructor:
   First 3 params are simple & clear.
   The 4. is the probability height of transmitted values. When a real probability coding is
   used this is crucial for a good compression. Higher is usually better, but too much blows
   the system and thus is capped. The improved algorithm makes only use of the height to
   improve the compression by some %.
   The 5. and 6. param are min and max of the predictor's search range. If max is given, the system
   can't make a bigger step in one call. This limits the maximum range with
   high probability and can lead (from time to time) to a better compression after small to medium jumps.
   But if there is an error value with a big jump, a low max means that the system needs very long
   (to practically forever) to recover from it, which can destroy the compression completely.
   Set maxRange to 0 when you don't know whether you need it.

   Compression Ratios at comparable deviance:
   Low precision values (8bit, 12bit ...) can be compressed to up to 1/5 of their original size.
   For high precision data (20bit, 24bit and up) the limit is usually reached at ~ 1/3 of their size.
 */

CAdaptiveFloat::CAdaptiveFloat()
#if USE_MEMENTO_PREDICTORS
	: m_haveMemento(false)
#endif
{
}

bool CAdaptiveFloat::Load(XmlNodeRef node, const string& filename, const string& child)
{
	bool ok = true;
	if (m_quantizer.Load(node, filename, child))
	{
#if USE_MEMENTO_PREDICTORS
		XmlNodeRef childNode = node->findChild(child);
		if (childNode != 0)
		{
			int timeMode = 0;
			childNode->getAttr("timeMode", timeMode);
			if (timeMode != 0)
				m_predictor.SetMode(CIntegerValuePredictor::ePredictorMode_Time);

			int probHeight = 10;
			childNode->getAttr("probHeight", probHeight);
			if (probHeight < 0)
				ok = false;

			float minRange = 0.1f;
			float maxRange = 1.0f;
			childNode->getAttr("minRange", minRange);
			childNode->getAttr("maxRange", maxRange);
			if (minRange >= maxRange)
				ok = false;

			float startValue = (m_quantizer.GetMinValue() + m_quantizer.GetMaxValue()) * 0.5f;
			childNode->getAttr("start", startValue);
			if (startValue < m_quantizer.GetMinValue() || startValue > m_quantizer.GetMaxValue())
				ok = false;

			int inRange = 85;
			childNode->getAttr("inRange", inRange);
			if (inRange < 1 || inRange > 126)
				return false;

			if (ok)
			{
				m_nQuantizedMinDifference = m_quantizer.Quantize(m_quantizer.GetMinValue() + minRange);
				m_nQuantizedMaxDifference = m_quantizer.Quantize(m_quantizer.GetMinValue() + maxRange);
				m_nQuantizedStartValue = m_quantizer.Quantize(startValue);
				m_nHeight = probHeight;
				m_nInRangePercentage = inRange;
			}
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
void CAdaptiveFloat::ReadMemento(CByteInputStream& stm) const
{
	m_predictor.Deserialize(stm);
	m_haveMemento = true;
}

void CAdaptiveFloat::WriteMemento(CByteOutputStream& stm) const
{
	m_predictor.Serialize(stm);
	m_haveMemento = true;
}

void CAdaptiveFloat::NoMemento() const
{
	m_predictor.Clear(0);
	m_haveMemento = false;
}
#endif

#if USE_ARITHSTREAM
void CAdaptiveFloat::WriteValue(CCommOutputStream& stm, float value, uint32 mementoAge, uint32 timeFraction32) const
{
	//quantize value
	uint32 quantized = m_quantizer.Quantize(value);

	//encode quantized value with SquarePulseProbability
	uint32 left, right;
	left = right = 0;
	int32 predicted = m_predictor.Predict(left, right, m_nHeight, m_nQuantizedMinDifference, m_quantizer.GetMaxQuantizedValue(), m_nQuantizedMaxDifference, mementoAge, timeFraction32);
	m_predictor.Update(quantized, predicted, mementoAge, timeFraction32);

	if (m_haveMemento)
	{
		//write value to the stream (using any probability scheme)
		SquarePulseProbabilityWriteImproved(stm, quantized, left, right, m_nHeight, m_quantizer.GetMaxQuantizedValue(), m_nInRangePercentage, m_quantizer.GetNumBits());
	}
	else
	{
		stm.WriteBits(quantized, m_quantizer.GetNumBits());
	}
}

bool CAdaptiveFloat::ReadValue(CCommInputStream& stm, float& value, uint32 mementoAge, uint32 timeFraction32) const
{
	uint32 left, right;
	left = right = 0;
	int32 prediction = m_predictor.Predict(left, right, m_nHeight, m_nQuantizedMinDifference, m_quantizer.GetMaxQuantizedValue(), m_nQuantizedMaxDifference, mementoAge, timeFraction32);
	uint32 quantized;

	if (m_haveMemento)
	{
		if (!SquarePulseProbabilityReadImproved(quantized, stm, left, right, m_nHeight, m_quantizer.GetMaxQuantizedValue(), m_nInRangePercentage, m_quantizer.GetNumBits()))
		{
			CryLogAlways("[CAdaptiveFloat::ReadValue] read error!\n");

			return false;
		}
	}
	else
	{
		quantized = stm.ReadBits(m_quantizer.GetNumBits());
	}

	m_predictor.Update(quantized, prediction, mementoAge, timeFraction32);
	value = m_quantizer.Dequantize(quantized);

	NetLogPacketDebug("CAdaptiveFloat::ReadValue %f Min %f Max %f NumBits %d (%f)", value, m_quantizer.GetMinValue(), m_quantizer.GetMaxValue(), m_quantizer.GetNumBits(), stm.GetBitSize());
	return true;
}
#else
void CAdaptiveFloat::WriteValue(CNetOutputSerializeImpl* stm, float value, uint32 mementoAge, uint32 timeFraction32) const
{
	uint32 quantized = m_quantizer.Quantize(value);

	stm->WriteBits(quantized, m_quantizer.GetNumBits());
}

bool CAdaptiveFloat::ReadValue(CNetInputSerializeImpl* stm, float& value, uint32 mementoAge, uint32 timeFraction32) const
{
	uint32 quantized = stm->ReadBits(m_quantizer.GetNumBits());

	value = m_quantizer.Dequantize(quantized);
	NetLogPacketDebug("CAdaptiveFloat::ReadValue %f Min %f Max %f NumBits %d (%f)", value, m_quantizer.GetMinValue(), m_quantizer.GetMaxValue(), m_quantizer.GetNumBits(), stm->GetBitSize());

	return true;
}
#endif
#if NET_PROFILE_ENABLE
int CAdaptiveFloat::GetBitCount()
{
	return m_quantizer.GetNumBits();
}
#endif
