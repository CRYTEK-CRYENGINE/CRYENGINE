// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   FloatQuantization
   Replaces the old quantization template for simplicity...
   It's implemented using a switch and functions for all four quantization/
   dequantization methods.
   Since so far the old "combined method" enums are used, the interface is somewhat
   confusing. In fact there are three possibilities for quantization
   and dequantization each ... But not all of them should be mixed and thus
   the user is supposed to call Quantize(..) and Dequantize(..) with the same method.
   Additionally two static helper functions ( [De]QuantizeFloat ) can be called
   when only a single value is (de-)quantized (less performant).

   Quantization methods :

   eFQM_TruncateLeft
   //this quantization helper has a tendency to move values towards zero
   //it always truncates)

   eFQM_TruncateCenter
   //this quantization algorithm evenly spreads values
   //(increases some, decreases others)
   //however, it cannot represent the extrema of the input range
   //(ie input range 0->1, output range does not include 0 or 1)

   eFQM_RoundLeft
   // this quantization method evenly spreads values
   // (increases some, decreases others)
   // it can represent extrema (both ends of the input range are representable)
   // but it is less accurate than eFQM_TruncateCenter for the same
   // bit size

   eFQM_RoundLeftWithMidpoint
   // this quantization method evenly spreads values
   // (increases some, decreases others)
   // it can represent extrema (both ends of the input range are representable) and the midpoint
   // but it is less accurate than eFQM_RoundLeft for the same
   // bit size

   eFQM_NeverLower
   //always rounds up (needed for example for height in position ...)

   eFQM_RoundCenter
   //forbidden to use!
   -------------------------------------------------------------------------
   History:
   - 17/11/2005   10:30 : Created by Jan Müller
   - 01/08/2005           Updated to be used in new compression framework (Craig Tiller)
*************************************************************************/

#ifndef __QUANTIZER__
#define __QUANTIZER__

#pragma once

enum EFloatQuantizationMethod
{
	eFQM_RoundLeftWithMidpoint,
	eFQM_TruncateLeft,
	eFQM_TruncateCenter,
	eFQM_RoundLeft,
	eFQM_NeverLower,
	eFQM_RoundCenter
};

class CQuantizer
{
public:
	CQuantizer()
	{
		m_fMin = 0;
		m_fRange = 0;
		m_nBits = 0;
		m_method = eFQM_RoundLeft;
	}

	bool         Load(XmlNodeRef node, const string& filename, const string& child = "Params", EFloatQuantizationMethod defaultMethod = eFQM_RoundLeft, int bits = -1);

	uint32       Quantize(float nValue) const;
	float        Dequantize(uint32 nValue) const;

	ILINE uint32 GetNumBits() const           { return m_nBits; }
	ILINE uint32 GetMaxQuantizedValue() const { return (1 << m_nBits) - 1; }
	ILINE float  GetMinValue() const          { return m_fMin; }
	ILINE float  GetMaxValue() const          { return m_fMin + m_fRange; }

private:
	ILINE void QuantizeTL(uint32& quantizedValue, double fScaled) const
	{
		quantizedValue = uint32(fScaled * m_bitsAsDoubles[m_nBits]);
	}

	ILINE void QuantizeTC(uint32& quantizedValue, double fScaled) const
	{
		QuantizeTL(quantizedValue, fScaled);
	}

	ILINE void QuantizeRL(uint32& quantizedValue, double fScaled) const
	{
		NET_ASSERT(m_nBits > 0 && m_nBits <= 32);
		quantizedValue = uint32(fScaled * (m_bitsAsDoubles[m_nBits] - 1) + 0.5);
	}

	ILINE void QuantizeRLM(uint32& quantizedValue, double fScaled) const
	{
		NET_ASSERT(m_nBits > 1 && m_nBits <= 32);
		if (fScaled < 0.5)
			quantizedValue = uint32(fScaled * (m_bitsAsDoubles[m_nBits] - 1) + 0.5);
		else
			quantizedValue = uint32((fScaled - 0.5) * (m_bitsAsDoubles[m_nBits] - 1) + m_bitsAsDoubles[m_nBits - 1] + 0.5);
	}

	ILINE void QuantizeNL(uint32& quantizedValue, double fScaled) const
	{
		NET_ASSERT(m_nBits > 0 && m_nBits <= 32);
		quantizedValue = uint32(fScaled * (m_bitsAsDoubles[m_nBits] - 1) + 1.0f);
	}

	ILINE void DequantizeTL(float& dequantizedValue, uint32 nValue) const
	{
		dequantizedValue = (float)nValue * m_fRange / (float)m_bitsAsDoubles[m_nBits] + m_fMin;
	}

	ILINE void DequantizeTC(float& dequantizedValue, uint32 nValue) const
	{
		dequantizedValue = (float)(nValue + 0.5f) * m_fRange / (float)m_bitsAsDoubles[m_nBits] + m_fMin;
	}

	ILINE void DequantizeRL(float& dequantizedValue, uint32 nValue) const
	{
		dequantizedValue = (float)nValue * m_fRange / (float)(m_bitsAsDoubles[m_nBits] - 1) + m_fMin;
	}

	ILINE void DequantizeNL(float& dequantizedValue, uint32 nValue) const
	{
		DequantizeRL(dequantizedValue, nValue);
	}

	ILINE void DequantizeRLM(float& dequantizedValue, uint32 nValue) const
	{
		float fScaled;
		if (nValue < (1u << (m_nBits - 1)))
			fScaled = (float)(nValue / (m_bitsAsDoubles[m_nBits] - 1));
		else
			fScaled = 0.5f + (float)(nValue - (1u << (m_nBits - 1))) / (float)(m_bitsAsDoubles[m_nBits] - 1);
		dequantizedValue = fScaled * m_fRange + m_fMin;
	}

	float                    m_fMin;
	float                    m_fRange;
	uint32                   m_nBits;
	EFloatQuantizationMethod m_method;
	static const double      m_bitsAsDoubles[33];
};

#endif
