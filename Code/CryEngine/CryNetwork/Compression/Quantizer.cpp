// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Quantizer.h"

const double CQuantizer::m_bitsAsDoubles[33] =
{
	1.0,
	2.0,
	4.0,
	8.0,
	16.0,
	32.0,
	64.0,
	128.0,
	256.0,
	512.0,
	1024.0,
	2048.0,
	4096.0,
	8192.0,
	16384.0,
	32768.0,
	65536.0,
	131072.0,
	262144.0,
	524288.0,
	1048576.0,
	2097152.0,
	4194304.0,
	8388608.0,
	16777216.0,
	33554432.0,
	67108864.0,
	134217728.0,
	268435456.0,
	536870912.0,
	1073741824.0,
	2147483648.0,
	4294967296.0,
};

bool CQuantizer::Load(XmlNodeRef node, const string& filename, const string& child, EFloatQuantizationMethod method, int bits)
{
	if (!node)
		return false;

	if (XmlNodeRef params = node->findChild(child))
	{
		float mn, mx;
		bool ok = true;
		ok &= params->getAttr("min", mn);
		ok &= params->getAttr("max", mx);
		if (bits <= 0)
			ok &= params->getAttr("nbits", bits);
		const char* qm = params->getAttr("quantization");
		if (qm[0])
		{
			if (0 == strcmp(qm, "TruncateLeft"))
				method = eFQM_TruncateLeft;
			else if (0 == strcmp(qm, "TruncateCenter"))
				method = eFQM_TruncateCenter;
			else if (0 == strcmp(qm, "RoundLeft"))
				method = eFQM_RoundLeft;
			else if (0 == strcmp(qm, "NeverLower"))
				method = eFQM_NeverLower;
			else if (0 == strcmp(qm, "RoundCenter"))
				method = eFQM_RoundCenter;
			else if (0 == strcmp(qm, "RoundLeftWithMidpoint"))
				method = eFQM_RoundLeftWithMidpoint;
			else
			{
				NetWarning("Invalid quantization method '%s' at %s:%d", qm, filename.c_str(), node->getLine());
				ok = false;
			}
		}
		ok &= mx > mn;
		ok &= bits >= 1;
		ok &= bits <= 32;
		if (ok)
		{
			m_method = method;
			m_fMin = mn;
			m_fRange = mx - mn;
			m_nBits = bits;
		}
		return ok;
	}
	else
	{
		NetWarning("Unable to find float quantization parameters '%s' at %s:%d", child.c_str(), filename.c_str(), node->getLine());
		return false;
	}
}

uint32 CQuantizer::Quantize(float nValue) const
{
	uint32 quantizedValue = 0;

#if DEBUG_QUANTIZATION_CLAMPING
	if (nValue < m_fMin || nValue > m_fMin + m_fRange)
		NetLog("The value %f is clamped to the limits [ %f, %f ] by the Quantizer.", nValue, m_fMin, m_fMin + m_fRange);
#endif

	double fScaled = (nValue - m_fMin) / double(m_fRange);
	if (fScaled < 0.0)
		fScaled = 0.0;
	else if (fScaled > 1.0)
		fScaled = 1.0;

	switch (m_method)
	{
	case eFQM_TruncateLeft:
		QuantizeTL(quantizedValue, fScaled);
		break;
	case eFQM_TruncateCenter:
		QuantizeTC(quantizedValue, fScaled);
		break;
	case eFQM_RoundLeft:
		QuantizeRL(quantizedValue, fScaled);
		break;
	case eFQM_NeverLower:
		QuantizeNL(quantizedValue, fScaled);
		break;
	case eFQM_RoundCenter:
		QuantizeRL(quantizedValue, fScaled);
		break;
	case eFQM_RoundLeftWithMidpoint:
		QuantizeRLM(quantizedValue, fScaled);
		break;
	default:
		NET_ASSERT(!"invalid quantize option");
		return 0;
		break;
	}

	// fixup iff nValue==m_fMax
	quantizedValue -= (quantizedValue == (1u << m_nBits));

	NET_ASSERT(m_nBits < 32);
	NET_ASSERT(quantizedValue < (1u << m_nBits));

	return quantizedValue;
}

float CQuantizer::Dequantize(uint32 nValue) const
{
	float dequantizedValue = 0;

	switch (m_method)
	{
	case eFQM_TruncateLeft:
		DequantizeTL(dequantizedValue, nValue);
		break;
	case eFQM_TruncateCenter:
		DequantizeTC(dequantizedValue, nValue);
		break;
	case eFQM_RoundLeft:
		DequantizeRL(dequantizedValue, nValue);
		break;
	case eFQM_NeverLower:
		DequantizeRL(dequantizedValue, nValue);
		break;
	case eFQM_RoundCenter:
		DequantizeTC(dequantizedValue, nValue);
		break;
	case eFQM_RoundLeftWithMidpoint:
		DequantizeRLM(dequantizedValue, nValue);
		break;
	default:
		NET_ASSERT(!"invalid quantize option");
		return 0;
		break;
	}

	return dequantizedValue;
}
